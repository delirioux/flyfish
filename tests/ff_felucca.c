/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH through Felucca's engines on the host: the player's notes on the real DSP (the same sources as the
 * firmware, through hostsim.c), the resonator on the mix as the firmware runs it, to a WAV. What the FM-1 will
 * play, short of its hardware; and the voice budget and the CPU against Felucca's reference mix.
 *
 *   tests/ff_felucca.sh OUTDIR [SECONDS]
 *   env SOUND=0..7 (the SOUND card), PEDAL=0..3, VARY=0..100 FIGURE=0..5 SEED, KEY, SCALE, BARS=from,to (a loop),
 *       NAME (the file: OUTDIR/flyfish_NAME.wav) */
#define LIM_T 24000                            /* the master as Flyfish has it (fx.c) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "ff_player.h"
#include "ff_sounds.h"
#include "tuna_reso.h"

static int env_int(const char *n, int def) { const char *v = getenv(n); return v ? atoi(v) : def; }

static ff_player P;
static tr_reso reso;
static uint32_t reso_sig = 0xFFFFFFFFu;
static long n_on, n_off;
static int16_t base_dec[3], base_rel[3];

static void sound(void *ctx, const ff_event *e)
{
    (void)ctx;
    uint32_t tr = ffs_track_of(e->hand, e->note);
    if (e->on) { trk_note_on(&trk[tr], e->note, e->vel); n_on++; }
    else if (tr != 3u) { trk_note_off(&trk[tr], e->note); n_off++; }
}

static void setup(void)
{
    host_tracks_init();
    song.g[G_RTYPE] = 0;
    const ffs_sound *snd = &FFS_SOUND[P.s.sound];
    for (uint32_t k = 0; k < 3u; ++k) {
        host_preset(&trk[k], snd->engine, snd->preset);
        base_dec[k] = trk[k].p[P_DEC];
        base_rel[k] = trk[k].p[P_REL];
        ffs_track(&trk[k], &P.s, (int)k, base_dec[k], base_rel[k]);
    }
    host_drums(&trk[3]);
    trk[3].p[P_LEVEL] = 90;
    song.g[G_RSIZE] = FFS_SPACE[P.s.space][0];
    song.g[G_RDAMP] = FFS_SPACE[P.s.space][1];
    song.g[G_BPM] = P.s.bpm;
    tr_init(&reso, (float)FS);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int seconds = argc > 2 ? atoi(argv[2]) : 0;
    char path[512];
    const char *name = getenv("NAME") ? getenv("NAME") : "etude";
    snprintf(path, sizeof path, "%s/flyfish_%s.wav", dir, name);
    ff_player_init(&P, (float)FS);
    P.s.sound = (uint8_t)env_int("SOUND", 0);
    P.s.pedal = (uint8_t)env_int("PEDAL", 0);
    P.s.vary = (uint8_t)env_int("VARY", 0);
    P.s.figure = (uint8_t)env_int("FIGURE", 1);
    P.s.seed = (uint8_t)env_int("SEED", 17);
    P.s.key = (int8_t)env_int("KEY", 0);
    P.s.scale = (uint8_t)env_int("SCALE", 0);
    P.s.rubato = (uint8_t)env_int("RUBATO", 35);
    P.s.bright = (uint8_t)env_int("BRIGHT", 60);
    P.s.decay = (uint8_t)env_int("DECAY", 60);
    P.s.reverb = (uint8_t)env_int("REVERB", 25);
    P.s.space = (uint8_t)env_int("SPACE", 2);
    P.s.rounds = (uint8_t)env_int("ROUNDS", 1);
    P.s.legato = (uint8_t)env_int("LEGATO", 25);       /* (the firmware's default is INF: a render ends) */
    if (getenv("BARS")) {
        int a = 1, b = 2;
        sscanf(getenv("BARS"), "%d,%d", &a, &b);
        P.s.from = (uint8_t)a; P.s.to = (uint8_t)b;
        ff_set_loop(&P, true);
    }
    ff_settings_fix(&P.s);
    ff_set_sound(&P, sound, 0);
    setup();
    FILE *w = fopen(path, "wb");
    if (!w) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    wav_hdr(w, 0);
    ff_play(&P, true);
    long f = 0, limit = seconds > 0 ? (long)seconds * FS : 400L * FS, tail = -1;
    uint32_t busy_max = 0, clips = 0, kills0 = voice_kills;
    int32_t peak = 0;
    uint64_t mix_ns = 0;
    while (1) {
        int32_t o[2 * CTL];
        int r0 = P.round;
        ff_process(&P, CTL);
        if (P.round != r0) fprintf(stderr, "round %d at %.2f s\n", P.round, (double)f / FS);
        if (P.pedal_sig != reso_sig) {                    /* the pedal changed: the strings retune */
            int notes[6];
            int n = ff_reso_notes(&P, notes);
            tr_tune(&reso, notes, n);
            reso_sig = P.pedal_sig;
        }
        tr_set(&reso, ff_reso_amount(&P), 0.55f, 0.45f);
        uint64_t t0 = now_ns();
        mix_block(o, CTL);
        if (reso.amount > 0) {
            int32_t in[CTL];
            for (int i = 0; i < CTL; ++i) in[i] = (o[2 * i] + o[2 * i + 1]) >> 1;
            tr_process(&reso, in, o, CTL);
        }
        mix_ns += now_ns() - t0;
        uint32_t b = busy_now();
        if (b > busy_max) busy_max = b;
        for (int i = 0; i < 2 * CTL; ++i) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > peak) peak = v;
            if (v >= 32700) clips++;
        }
        for (int i = 0; i < CTL; ++i) wav_put(w, o[2 * i], o[2 * i + 1]);
        f += CTL;
        if (tail < 0 && (!P.playing || f >= limit)) { if (P.playing) ff_play(&P, false); tail = 4L * FS; }
        if (tail >= 0 && (tail -= CTL) <= 0) break;
    }
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, (uint32_t)f);
    fclose(w);
    printf("%s: %.1f s, %ld notes on, %ld off, voices at most %u, %u stolen, peak %d (%.1f dBFS), %u clipped, mix %.0f ns/sample\n",
           path, (double)f / FS, n_on, n_off, busy_max, voice_kills - kills0, peak, 20.0 * log10((double)peak / 32768.0),
           clips, (double)mix_ns / (double)f);
    return 0;
}
