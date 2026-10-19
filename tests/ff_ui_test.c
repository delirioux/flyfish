/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH's screen and controls on the host: firmware/src/ff_ui.c drawn by Felucca's gfx.c into a 240 x 240 frame,
 * the player running, the controls driven as a player would, every page in every palette written out, and the
 * layout lint (from Tuna's tests/tuna_ui_test.c): every text inside its canvas, not cut, not ellipsised unless it
 * is a name, no two texts of one canvas overlapping.
 *   tests/ff_ui_test.sh [OUTDIR]     (exit 0: all passed; PNGs and a GIF of HOME playing under OUTDIR) */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            fails++;                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

/* ---- the layout lint: the ink boxes of one canvas, checked when it is blitted -------------------------- */
typedef struct { int x0, y0, x1, y1, flags; char s[24]; } box;
static box boxes[256];
static int nbox;
static const char *lint_where = "";
static int lint_findings;
static void hook_text(int x0, int y0, int x1, int y1, const char *s, int flags)
{
    if (nbox < 256) {
        box *b = &boxes[nbox++];
        b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1; b->flags = flags;
        strncpy(b->s, s, sizeof b->s - 1);
        b->s[sizeof b->s - 1] = 0;
    }
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hook_text((x0), (y0), (x1), (y1), (s), (flags))
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() (nbox = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)

static uint16_t fb[240 * 240];
static int blit_count;
static void lint_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    for (int i = 0; i < nbox; ++i) {
        box *b = &boxes[i];
        if (b->x1 <= b->x0)
            continue;
        bool name = (b->flags & 8) != 0;            /* free text (a name, a message) may be ellipsised */
        if (b->x0 < 0 || b->y0 < 0 || b->x1 > (int)w || b->y1 > (int)h) {
            printf("LINT %s: \"%s\" leaves its canvas (%d,%d..%d,%d of %ux%u at %u,%u)\n", lint_where, b->s, b->x0,
                   b->y0, b->x1, b->y1, w, h, x, y);
            lint_findings++;
        }
        if ((int)x + b->x1 > 240 || (int)y + b->y1 > 240) {
            printf("LINT %s: \"%s\" off the screen\n", lint_where, b->s);
            lint_findings++;
        }
        if ((b->flags & 1) && !name) {
            printf("LINT %s: \"%s\" ellipsised\n", lint_where, b->s);
            lint_findings++;
        }
        for (int j = i + 1; j < nbox; ++j) {
            box *c = &boxes[j];
            if (c->x1 <= c->x0)
                continue;
            if (b->x0 < c->x1 && c->x0 < b->x1 && b->y0 < c->y1 && c->y0 < b->y1) {
                printf("LINT %s: \"%s\" overlaps \"%s\" (%d,%d..%d,%d / %d,%d..%d,%d)\n", lint_where, b->s, c->s, b->x0,
                       b->y0, b->x1, b->y1, c->x0, c->y0, c->x1, c->y1);
                lint_findings++;
            }
        }
    }
    nbox = 0;
    nbox = 0;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    lint_blit(x, y, w, h);
    blit_count++;
    for (uint32_t j = 0; j < h && y + j < 240; j++)
        for (uint32_t i = 0; i < w && x + i < 240; i++)
            fb[(y + j) * 240 + x + i] = p[j * w + i];
}
#include "gfx.c"
#include "ff_ui.c"

static const char *outdir;
static int frames_written;
static void write_ppm(const char *name)
{
    if (!outdir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/ppm/%s.ppm", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t px[3] = {(uint8_t)((c >> 11) << 3 | (c >> 13)), (uint8_t)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                         (uint8_t)((c & 31) << 3 | ((c >> 2) & 7))};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    frames_written++;
}

static int notes_on, notes_off;
static void sound(void *ctx, const ff_event *e) { (void)ctx; if (e->on) notes_on++; else notes_off++; }

static ff_player pl;
static ff_ui ui;
static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 16) {
        ff_process(&pl, 44100 * 16 / 1000);
        fu_tick(&ui, 16);
        fu_draw(&ui);
    }
}
static void press(int b) { fu_button(&ui, b, true); fu_button(&ui, b, false); }
static void shot(const char *name)
{
    lint_where = name;
    fu_draw_all(&ui);
    write_ppm(name);
}
static void all_palettes(const char *page)
{
    int keep = pl.s.palette, keep_st = pl.s.style;
    for (int p = 0; p < (int)NPALETTES; ++p)
        for (int st = 0; st < 2; ++st) {
            char name[64];
            pl.s.palette = (uint8_t)p;
            pl.s.style = (uint8_t)st;
            fu_apply_style(&ui);
            snprintf(name, sizeof name, "%s%s_%s", st ? "LINE_" : "", UI_PALETTES[p].name, page);
            shot(name);
        }
    pl.s.palette = (uint8_t)keep;
    pl.s.style = (uint8_t)keep_st;
    fu_apply_style(&ui);
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    ff_player_init(&pl, 44100.0f);
    ff_set_sound(&pl, sound, 0);
    fu_init(&ui, &pl);
    CHECK(pl.s.palette == 3 && !strcmp(UI_PALETTES[3].name, "ICE"), "ICE by default");
    all_palettes("home_stopped");
    press(FU_B_PLAY);
    CHECK(pl.playing, "PLAY plays");
    run_ms(1700);
    shot("home_playing");
    all_palettes("home");
    /* the HOME animation: bars 1..4, every 16th frame-ish */
    {
        ff_player_init(&pl, 44100.0f); ff_set_sound(&pl, sound, 0); fu_init(&ui, &pl);
        press(FU_B_PLAY);
        for (int f = 0; f < 150; ++f) {
            char nm[32];
            run_ms(48);
            snprintf(nm, sizeof nm, "anim_%03d", f);
            lint_where = nm;
            write_ppm(nm);
        }
    }
    /* SOUND, the PRESETS list */
    press(FU_B_EDIT);
    CHECK(ui.page == FU_PG_SOUND, "EDIT: SOUND");
    fu_knob(&ui, FU_K1, 2);
    CHECK(pl.s.sound == 2, "K1 turns the sound (%d)", pl.s.sound);
    shot("sound");
    all_palettes("sound");
    fu_knob(&ui, FU_K1, -2);
    /* PRACTICE: the loop 11-12 at 60 % */
    press(FU_B_SEQ);
    CHECK(ui.page == FU_PG_PRACTICE, "SEQ: PRACTICE");
    fu_knob(&ui, FU_K1, -8);
    fu_knob(&ui, FU_K2, 10);
    fu_knob(&ui, FU_K3, 1);
    CHECK(pl.s.speed == 60 && pl.s.from == 11 && pl.s.to == 12, "speed %d from %d to %d", pl.s.speed, pl.s.from, pl.s.to);
    press(FU_B_REC);
    CHECK(pl.loop, "REC: the loop");
    press(FU_B_PLAY); press(FU_B_PLAY);
    run_ms(2500);
    CHECK(ff_bar(&pl) >= 11 && ff_bar(&pl) <= 12, "in the loop (%d)", ff_bar(&pl));
    shot("practice");
    all_palettes("practice");
    /* PRACTICE 2: WAIT for the right hand */
    fu_knob(&ui, FU_K4, 1);
    press(FU_B_SEQ);
    CHECK(ui.page == FU_PG_PRACTICE2, "SEQ again: PRACTICE 2");
    fu_knob(&ui, FU_K1, 2);
    CHECK(pl.s.mode == FF_MODE_WAIT, "MODE WAIT");
    press(FU_B_PLAY); press(FU_B_PLAY);
    run_ms(1200);
    CHECK(pl.waiting, "waiting for the right hand");
    shot("practice2_wait");
    all_palettes("practice2_wait");
    if (pl.waiting) {
        int want = pl.want[0];
        fu_key(&ui, 53 + ((want % 12) - 5 + 12) % 12, true);
        fu_key(&ui, 53 + ((want % 12) - 5 + 12) % 12, false);
        run_ms(100);
        CHECK(!pl.waiting || pl.want[0] != want || pl.want_step != 0, "the right key moves on");
    }
    fu_knob(&ui, FU_K1, -1);
    shot("practice2_tap");
    fu_knob(&ui, FU_K1, -1);
    shot("practice2_free");
    press(FU_B_REC);
    /* PRACTICE 3: the rhythms, the chords, the progress */
    press(FU_B_SEQ);
    CHECK(ui.page == FU_PG_PRACTICE3, "SEQ a third time: PRACTICE 3");
    fu_knob(&ui, FU_K1, 1);
    CHECK(pl.s.rhythm == FF_RHY_LONG_SHORT, "RHYTHM LONG-SHORT (%d)", pl.s.rhythm);
    fu_knob(&ui, FU_K2, 1);
    CHECK(pl.s.chords == 1, "CHORDS on");
    for (int b = 0; b < 79; ++b) { pl.best[b] = (uint8_t)(b < 30 ? 100 : b < 50 ? 60 + b % 5 * 5 : 0); pl.miss[b] = (uint8_t)(b % 7 == 3 ? b % 11 : 0); }
    shot("practice3_best");
    all_palettes("practice3_best");
    fu_knob(&ui, FU_K3, 1);
    shot("practice3_misses");
    fu_knob(&ui, FU_K4, 1);
    CHECK(pl.best[0] == 100, "ERASE once: armed, nothing erased");
    shot("practice3_erase");
    fu_knob(&ui, FU_K4, 1);
    CHECK(pl.best[0] == 0 && pl.miss[3] == 0, "ERASE twice: erased");
    fu_knob(&ui, FU_K1, -1); fu_knob(&ui, FU_K2, -1); fu_knob(&ui, FU_K3, -1);
    press(FU_B_SEQ);
    CHECK(ui.page == FU_PG_PRACTICE, "SEQ: back to PRACTICE");
    fu_knob(&ui, FU_K4, -1);
    fu_knob(&ui, FU_K1, 8);
    /* PIANO: a STYLE, SOFT, a temperament */
    press(FU_B_FX);
    CHECK(ui.page == FU_PG_PIANO, "FX: PIANO");
    fu_knob(&ui, FU_K1, 2);
    CHECK(pl.s.interp == FF_STYLE_ROMANTIC && pl.s.bpm == 160 && pl.s.rubato == 70, "STYLE ROMANTIC: its tempo and rubato");
    fu_knob(&ui, FU_K3, 1);
    fu_knob(&ui, FU_K4, 1);
    CHECK(pl.s.soft && pl.s.tuning == FF_TUNE_WERCKMEISTER, "SOFT, WERCKMEISTER");
    shot("piano");
    all_palettes("piano");
    fu_knob(&ui, FU_K2, 1);
    CHECK(pl.s.interp == FF_STYLE_OWN, "LEGATO by hand: the STYLE is OWN");
    fu_knob(&ui, FU_K2, -1); fu_knob(&ui, FU_K3, -1); fu_knob(&ui, FU_K4, -1);
    ff_style_apply(&pl.s, FF_STYLE_OWN);
    pl.s.bpm = 176; pl.s.rubato = 35; pl.s.legato = 25; pl.s.touch = 40; pl.s.accent = 50; pl.s.pedal = FF_PED_SCORE;
    /* VARIATIONS */
    press(FU_B_ARP);
    CHECK(ui.page == FU_PG_VAR, "ARP: VARIATIONS");
    run_ms(1000);
    fu_knob(&ui, FU_K1, 2);
    fu_knob(&ui, FU_K2, 1);
    shot("variations");
    all_palettes("variations");
    fu_knob(&ui, FU_K1, -2);
    fu_knob(&ui, FU_K2, -1);
    /* MIX */
    press(FU_B_GLO);
    shot("mix");
    all_palettes("mix");
    /* MENU: HOME held */
    fu_button(&ui, FU_B_HOME, true);
    run_ms(700);
    fu_button(&ui, FU_B_HOME, false);
    CHECK(ui.page == FU_PG_MENU, "HOME held: MENU");
    shot("menu");
    all_palettes("menu");
    fu_knob(&ui, FU_K3, 1);
    CHECK(pl.s.view == FF_VIEW_SCORE, "VIEW SCORE");
    fu_knob(&ui, FU_K4, 2);
    CHECK(pl.s.pitch == FF_PITCH_430, "PITCH 430");
    shot("menu_score");
    press(FU_B_HOME);
    if (!pl.playing) press(FU_B_PLAY);
    CHECK(pl.playing, "playing for the score");
    run_ms(900);
    shot("home_score");
    all_palettes("home_score");
    ff_goto_bar(&pl, 34);
    run_ms(2600);
    shot("home_score_34");
    press(FU_B_PLAY);
    fu_button(&ui, FU_B_HOME, true); run_ms(700); fu_button(&ui, FU_B_HOME, false);
    fu_knob(&ui, FU_K3, -1);
    fu_knob(&ui, FU_K4, -2);
    press(FU_B_HOME);
    CHECK(ui.page == FU_PG_HOME, "HOME: home");
    /* the PRESETS list over the page; ALGORITHM to a bar; OCT */
    fu_knob(&ui, FU_K_PRESETS, 1);
    shot("browse");
    run_ms(1600);
    press(FU_B_PLAY);
    fu_knob(&ui, FU_K_ALGO, 5);
    shot("goto");
    press(FU_B_OCTUP);
    CHECK(ff_bar(&pl) >= 2, "OCT+ (%d)", ff_bar(&pl));
    press(FU_B_SCL);
    shot("not_used");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("ff_ui: %d checks, %d failed, %d frames, %d notes on\n", checks, fails, frames_written, notes_on);
    return fails ? 1 : 0;
}
