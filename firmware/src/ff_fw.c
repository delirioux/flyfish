/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH on the FM-1: the glue between Flyfish (the player and its screen) and Felucca's firmware (the engines,
 * voices and effects, the panel, the screen driver, USB MIDI, the flash). Included by felucca.c after Felucca's own
 * sources when FELUCCA_FLYFISH is 1; the player (ff_player.c), the resonator and the maths (tuna_reso.c, tuna_libm.c,
 * from Tuna) are their own translation units, linked beside felucca.o (tools/build.py). Shaped as Tuna's tuna_fw.c.
 *
 * Who runs what:
 *   the audio ISR (audio.c audio_block -> ff_audio_pre / ff_audio_post): the player in time, its notes straight
 *     into Felucca's tracks (trk_note_on / off) and out as USB MIDI; after Felucca's mix, the resonator (the
 *     pedal's sympathetic strings) on the master;
 *   the main loop (main.c -> ff_frame, ff_input): the panel into the controls (the audio ISR held off while one
 *     acts), the SOUND card into the tracks' engines (a preset load is the main loop's), the mix, the LEDs, the
 *     settings saved while stopped, the screen.
 * Tracks: 1 the piano's treble, 2 its bass (split at middle C), 3 the keys, 4 the click (ff_sounds.h). */
#include "ff_player.h"
#include "ff_ui.h"
#include "ff_ui.c"
#include "ff_sounds.h"
#include "ff_version.h"
#include "tuna_reso.h"

static ff_player ff_pl;
static ff_ui ff_u;
static tr_reso ff_reso __attribute__((section(".pool")));   /* (12 KB: in the pool, zeroed at boot) */

/* what the tracks were last set to */
static ff_settings ff_applied;
static bool ff_applied_ok;
static int16_t ff_base_dec[3], ff_base_rel[3];
static uint32_t ff_reso_sig = 0xFFFFFFFFu;
static int32_t ff_reso_amt = -1;
static volatile uint8_t ff_last_rh = 0xFF;                   /* the right hand's latest note (the key LEDs) */
static uint8_t ff_tuned_t = 0xFF, ff_tuned_p = 0xFF;            /* the tuning the resonator was given */

/* ---- the audio ISR --------------------------------------------------------------------------------------------- */
static void ff_sound(void *ctx, const ff_event *e)
{
    (void)ctx;
    uint32_t tr = ffs_track_of(e->hand, e->note), h = e->hand & 3u;
    if (e->on) {
        if (tr < 3u)
            ffs_note_on(&trk[tr], &ff_pl.s, e->note, e->vel);  /* (the temperament: the voice's fine pitch) */
        else
            trk_note_on(&trk[tr], e->note, e->vel);
        if (h == FF_HAND_R) ff_last_rh = e->note;
    } else if (tr != 3u) {
        trk_note_off(&trk[tr], e->note);                     /* (a click plays on through its decay) */
    }
    if (h < 3u) {                                            /* MIDI out: the right hand on 1, the left 2, the keys 3 */
        uint32_t st = (e->on ? 0x90u : 0x80u) | h, cin = st >> 4;
        midi_out_event(cin | st << 8 | (uint32_t)e->note << 16 | (uint32_t)(e->on ? e->vel : 0u) << 24);
    }
}

/* MIDI out, the pedals: the sustain (CC64) follows Flyfish's pedal, lifted where the bass changes and put down
 * again 30 ms later (a pianist's legato pedal, so a piano on the other end catches it), half way for HALF; the
 * soft pedal (CC67) follows SOFT. On channels 1 and 2, the hands' */
static uint32_t ff_ped_sig = 0xFFFFFFFFu;
static int32_t ff_ped_down_in = -1;
static uint8_t ff_ped_val = 0, ff_soft_val = 0xFF;
static void ff_midi_cc(uint32_t cc, uint32_t v)
{
    for (uint32_t ch = 0; ch < 2u; ch++)
        midi_out_event(0x0Bu | (0xB0u | ch) << 8 | cc << 16 | v << 24);
}
static void ff_midi_pedals(uint32_t n)
{
    uint8_t want = !ff_pl.playing || ff_pl.s.pedal == FF_PED_DRY ? 0u : ff_pl.s.pedal == FF_PED_HALF ? 64u : 127u;
    if (ff_pl.pedal_sig != ff_ped_sig) {
        ff_ped_sig = ff_pl.pedal_sig;
        if (ff_ped_val) {
            ff_midi_cc(64u, 0u);
            ff_ped_val = 0;
        }
        ff_ped_down_in = want ? (int32_t)(FS * 3u / 100u) : -1;
    }
    if (ff_ped_down_in >= 0 && (ff_ped_down_in -= (int32_t)n) < 0 && want) {
        ff_midi_cc(64u, want);
        ff_ped_val = want;
    }
    if (ff_ped_val && ff_ped_val != want && ff_ped_down_in < 0) {   /* stopped, or the pedal mode changed */
        ff_midi_cc(64u, want);
        ff_ped_val = want;
    }
    uint8_t soft = ff_pl.s.soft ? 127u : 0u;
    if (soft != ff_soft_val) {
        ff_midi_cc(67u, soft);
        ff_soft_val = soft;
    }
}

static void ff_audio_pre(uint32_t n)
{
    ff_process(&ff_pl, (int)n);
    ff_midi_pedals(n);
    if (ff_pl.pedal_sig != ff_reso_sig) {                    /* a new pedal: the strings retune (they glide) */
        int notes[6], k = ff_reso_notes(&ff_pl, notes);
        tr_tune(&ff_reso, notes, k);
        ff_reso_sig = ff_pl.pedal_sig;
    }
    int32_t amt = (int32_t)(ff_reso_amount(&ff_pl) * 100.0f);
    if (amt != ff_reso_amt) {
        tr_set(&ff_reso, (float)amt / 100.0f, 0.55f, 0.45f);
        ff_reso_amt = amt;
    }
}

static void ff_audio_post(int32_t *out, uint32_t n)
{
    if (ff_reso.amount <= 0)
        return;
    int32_t in[CTL];
    uint32_t i;
    for (i = 0; i < n && i < CTL; i++)
        in[i] = (out[2u * i] + out[2u * i + 1u]) >> 1;
    tr_process(&ff_reso, in, out, (int)i);
    for (i = 0; i < 2u * n; i++)
        out[i] = out[i] > 32767 ? 32767 : out[i] < -32768 ? -32768 : out[i];
}

/* ---- the settings: one record in Felucca's first project object, saved only while stopped (the flash is the code's:
 * XIP, writing it while playing would stall the audio) ---- */
#define FF_REC_MAGIC 0x31534646u                             /* "FFS1" */
typedef struct { uint32_t magic; uint16_t size, ver; ff_settings s; } ff_record;
static uint32_t ff_save_ms;
static bool ff_dirty;
static void ff_settings_load(void)
{
#if FELUCCA_FLASH
    ff_record r;
    memset(&r, 0, sizeof r);
    int n = st_load(OBJ_PROJECT0, &r, sizeof r);
    if (n >= (int)(sizeof r - sizeof r.s) && r.magic == FF_REC_MAGIC) {
        ff_settings s;
        ff_defaults(&s);                                     /* (a shorter, older record: the new fields their defaults) */
        uint32_t k = r.size < sizeof s ? r.size : sizeof s;
        memcpy(&s, &r.s, k);
        if (r.ver < 2 && s.space == FF_SPACE_HALL && s.reverb == 30) {   /* 0.1's default space: 0.2's (CATHEDRAL 20) */
            s.space = FF_SPACE_CATHEDRAL;
            s.reverb = 20;
        }
        if (r.ver < 3)                                       /* FORM became ROUNDS (0.3): INF, the default */
            s.rounds = 0;
        if (r.ver < 4 && s.right == 75 && s.left == 100 && s.reverb == 20) {   /* 0.2's mix: 0.3's */
            s.right = 65;
            s.left = 65;
            s.reverb = 25;
        }
        if (r.ver < 5 && s.touch == 70)                     /* 0.3: TOUCH 40, the dips less deep */
            s.touch = 40;
        ff_settings_fix(&s);
        ff_pl.s = s;
    }
#endif
}
/* the progress chart (PRACTICE 3): its own record in the second project object, saved as the settings are */
#define FF_PROG_MAGIC 0x31504646u                            /* "FFP1" */
typedef struct { uint32_t magic; uint8_t best[FF_BARS_N], miss[FF_BARS_N]; } ff_prog_rec;
static void ff_progress_load(void)
{
#if FELUCCA_FLASH
    ff_prog_rec r;
    memset(&r, 0, sizeof r);
    if (st_load(OBJ_PROJECT0 + 1u, &r, sizeof r) == (int)sizeof r && r.magic == FF_PROG_MAGIC) {
        memcpy(ff_pl.best, r.best, sizeof r.best);
        memcpy(ff_pl.miss, r.miss, sizeof r.miss);
    }
#endif
}
static void ff_progress_save(void)
{
#if FELUCCA_FLASH
    ff_prog_rec r;
    r.magic = FF_PROG_MAGIC;
    memcpy(r.best, ff_pl.best, sizeof r.best);
    memcpy(r.miss, ff_pl.miss, sizeof r.miss);
    st_save(OBJ_PROJECT0 + 1u, &r, sizeof r);
#endif
}
static void ff_settings_save(void)
{
#if FELUCCA_FLASH
    ff_record r;
    memset(&r, 0, sizeof r);
    r.magic = FF_REC_MAGIC;
    r.size = (uint16_t)sizeof r.s;
    r.ver = 5;                                               /* (2: CATHEDRAL 20; 3: ROUNDS for FORM; 4: RIGHT LEFT 65, REVERB 25; 5: TOUCH 40) */
    r.s = ff_pl.s;
    st_save(OBJ_PROJECT0, &r, sizeof r);
#endif
}

/* ---- the SOUND card into the tracks (main loop) ----------------------------------------------------------------- */
static void ff_load_sound(void)
{
    const ffs_sound *snd = &FFS_SOUND[ff_pl.s.sound % FF_SOUNDS];
    for (uint32_t k = 0; k < 3u; ++k) {
        undo_depth++;                                        /* (no undo copy of Flyfish's loads) */
        set_engine_of(&trk[k], snd->engine);
        apply_preset_to(&trk[k], snd->preset);
        undo_depth--;
        fm1_irq_off();
        ff_base_dec[k] = trk[k].p[P_DEC];
        ff_base_rel[k] = trk[k].p[P_REL];
        fm1_irq_on();
    }
}

static void ff_apply(void)
{
    const ff_settings *s = &ff_pl.s;
    bool all = !ff_applied_ok;
    if (all || s->sound != ff_applied.sound)
        ff_load_sound();
    if (all || memcmp(s, &ff_applied, sizeof *s)) {
        fm1_irq_off();
        for (uint32_t k = 0; k < 3u; ++k)
            ffs_track(&trk[k], s, (int)k, ff_base_dec[k], ff_base_rel[k]);
        song.g[G_RSIZE] = FFS_SPACE[s->space % FF_SPACE_COUNT][0];
        song.g[G_RDAMP] = FFS_SPACE[s->space % FF_SPACE_COUNT][1];
        song.g[G_RTYPE] = 0;                                 /* (Felucca's ROOM model, sized as Tuna's) */
        song.g[G_TUNE] = ffs_tune(s);                        /* PITCH: A = 440, 435, 430 */
        fm1_irq_on();
    }
    if (s->tuning != ff_tuned_t || s->pitch != ff_tuned_p) {   /* the resonator's strings: the same tuning */
        float ct[12];
        for (int i = 0; i < 12; ++i)
            ct[i] = ff_temper_ct(s, i) + ff_pitch_ct(s);
        fm1_irq_off();
        tr_cents(&ff_reso, ct);
        fm1_irq_on();
        ff_tuned_t = s->tuning;
        ff_tuned_p = s->pitch;
    }
    if (s->palette != settings.palette % NPALETTES) {        /* (the menu's colour, kept as Felucca keeps its own) */
        settings.palette = s->palette;
        settings_save();
    }
    ff_applied = *s;
    ff_applied_ok = true;
}

/* ---- the power-on screen: Flyfish's logo (tools/ff_logo.py: 176 px, 8 colours, rows run-length coded) and the
 * Felucca it runs on; main.c holds it 0.75 s ---- */
#include "ff_logo.inc"
#define FF_SPLASH_MS 750u
static void ff_splash(void)
{
    uint16_t row[2][FF_LOGO_SIZE];                           /* (two: one fills while the other goes out by DMA) */
    uint32_t y, x0 = (240u - FF_LOGO_SIZE) / 2u, y0 = 12u;
    char line[40] = "Flyfish " FF_VERSION " on Felucca ";
    uint32_t k = sizeof "Flyfish " FF_VERSION " on Felucca " - 1u;
    lcd_fill(0, 0, 240, 240, FF_LOGO_BG);
    for (y = 0; y < FF_LOGO_SIZE; y++) {
        const uint8_t *p = FF_LOGO_RLE + FF_LOGO_ROW[y];
        uint16_t *r = row[y & 1u];
        uint32_t x = 0;
        while (x < FF_LOGO_SIZE) {
            uint32_t n = (*p >> 3) + 1u, c = FF_LOGO_PAL[*p & 7u];
            p++;
            while (n-- && x < FF_LOGO_SIZE)
                r[x++] = (uint16_t)c;
        }
        lcd_blit(x0, y0 + y, FF_LOGO_SIZE, 1, r);
    }
    for (const char *v = FELUCCA_VERSION; *v && k < sizeof line - 1u; v++)
        line[k++] = *v;
    line[k] = 0;
    draw_text_line(0, y0 + FF_LOGO_SIZE + 14u, 240, &AF_S, line, 0xA535u, FF_LOGO_BG, 1);   /* (grey, 160 165 170) */
    draw_text_line(0, y0 + FF_LOGO_SIZE + 32u, 240, &AF_S, "for Youri", 0x6B4Du, FF_LOGO_BG, 1);   /* (the film that
                                                                                                * opens with it) */
    lcd_sync();
}

/* ---- start ------------------------------------------------------------------------------------------------------ */
static void ff_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, i == 3u ? ENGI_DRUM : FFS_SAMPLE);  /* the click on track 4, the music on 1..3 */
        apply_preset_to(t, 0u);
        t->engine = t->eng_req;
        track_defaults_steps(t);                             /* (Felucca's sequencer stays empty) */
    }
    undo_depth--;
    trk[3].p[P_LEVEL] = 90;
    song.sel = 0;
    song.master_q12 = 2048;
    ff_player_init(&ff_pl, (float)FS);
    ff_settings_load();
    ff_progress_load();
    ff_pl.s.palette = (uint8_t)(settings.palette % NPALETTES);
    tr_init(&ff_reso, (float)FS);
    ff_set_sound(&ff_pl, ff_sound, 0);
    fu_init(&ff_u, &ff_pl);
    ff_apply();
}

/* ---- the main loop ----------------------------------------------------------------------------------------------- */
static uint32_t ff_notes_prev;
static void ff_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), b, k;
    static const uint8_t KNOB[NE] = {FU_K_SELECT, FU_K_ALGO, FU_K_PRESETS, FU_K1, FU_K2, FU_K3, FU_K4};
    fm1_input_note_edges();                                  /* (the keys: from their state below) */
    for (b = 0; b < NB; b++) {                               /* (Felucca's B_* are Flyfish's FU_B_*, in the same order) */
        uint32_t bit = 1u << panel.btn[b];
        if (pressed & bit) {
            fm1_irq_off();
            fu_button(&ff_u, (int)b, true);
            fm1_irq_on();
        }
        if (released & bit) {
            fm1_irq_off();
            fu_button(&ff_u, (int)b, false);
            fm1_irq_on();
        }
    }
    for (k = 0; k < NE; k++) {
        int32_t s = panel_enc(k);
        if (s) {
            fm1_irq_off();
            fu_knob(&ff_u, KNOB[k], s);
            fm1_irq_on();
        }
    }
    uint32_t notes = fm1_in.notes, ch = notes ^ ff_notes_prev;
    for (k = 0; ch && k < 27u; k++)
        if ((ch >> k) & 1u) {
            fm1_irq_off();
            fu_key(&ff_u, FU_KEY_LO + (int)k, (notes >> k) & 1u);
            fm1_irq_on();
        }
    ff_notes_prev = notes;
}

/* the LEDs: the page's button, PLAY green while playing, REC with the loop on; the keys: held ones, WAIT's notes to
 * find, else the right hand running across them (its note folded into F3 .. G5) */
static void ff_leds(void)
{
    static const uint8_t PAGE_BTN[FU_PAGES] = {
        [FU_PG_HOME] = B_HOME, [FU_PG_SOUND] = B_EDIT, [FU_PG_PRACTICE] = B_SEQ, [FU_PG_PRACTICE2] = B_SEQ,
        [FU_PG_PRACTICE3] = B_SEQ, [FU_PG_VAR] = B_ARP, [FU_PG_MIX] = B_GLO, [FU_PG_MENU] = B_HOME, [FU_PG_PIANO] = B_FX};
    static uint8_t ready;
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[PAGE_BTN[ff_u.page % FU_PAGES]], 1);
    led_put(nl, panel.btn[B_REC], ff_pl.loop);
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    uint32_t lit = 0;
    if (ff_pl.playing && ff_pl.waiting) {
        for (int i = 0; i < ff_pl.nwant; ++i)
            for (k = 0; k < 27u; k++)
                if ((FU_KEY_LO + k) % 12u == ff_pl.want[i] % 12u) lit |= 1u << k;
    } else if (ff_pl.playing && ff_last_rh != 0xFF) {
        int nn = ff_last_rh;
        while (nn > FU_KEY_HI) nn -= 12;
        while (nn < FU_KEY_LO) nn += 12;
        lit |= 1u << (nn - FU_KEY_LO);
    }
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)(((fm1_in.notes >> k) & 1u) || ((lit >> k) & 1u)));
        led_put(nd, 14u + k, 1);
    }
    if (ff_pl.playing)
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++)
        nd[c] = settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO ? (uint8_t)(nd[c] & ~nl[c]) : 0u;
    if (ff_pl.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* a frame: the settings into the tracks, the LEDs, the screen; the settings saved a while after the last change,
 * stopped */
static uint32_t ff_last_ms;
static void ff_frame(void)
{
    uint32_t now = fm1_ms, dt = now - ff_last_ms;
    static uint8_t first = 1;
    ff_last_ms = now;
    if (ff_u.changed) {
        ff_u.changed = false;
        ff_dirty = true;
        ff_save_ms = now;
    }
    ff_apply();
    if (ff_dirty && !ff_pl.playing && now - ff_save_ms > 2000u) {
        ff_settings_save();
        ff_dirty = false;
    }
    if (ff_pl.progress_new && !ff_pl.playing) {              /* (stopped: the flash is the code's while playing) */
        ff_pl.progress_new = 0;
        ff_progress_save();
    }
    song.playing = ff_pl.playing;                            /* (PLAY's green LED, Felucca's meters) */
    ff_leds();
    ff_u.batt = (int)batt_shown();
    fu_tick(&ff_u, dt > 100u ? 100 : (int)dt);
    if (first) {
        fu_draw_all(&ff_u);
        first = 0;
    } else {
        fu_draw(&ff_u);
    }
}
