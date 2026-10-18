/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH player on the host: the whole étude through ff_process, counted and checked.
 *   tests/ff_tests.sh builds and runs it (exit 0: all passed)
 * Checks: every note of the score sounds once and in order (VARY 0), each note-on has its note-off, the length of
 * a performance, the loop and its ramp, WAIT with the right and wrong keys, TAP, the variations (in the half bar's
 * harmony, the same seed the same notes, VARY 0 the score), the key and the modes, the bass options, the pedal. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ff_player.h"
#include "ff_etude.inc"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SR 44100.0f
#define BLOCK 128

typedef struct { int on, hand, note, vel; long t; } rec_t;
static rec_t rec[20000];
static int nrec;
static long now;
static int sounding[4][128];
static int max_poly;
static void sound(void *ctx, const ff_event *e)
{
    (void)ctx;
    if (nrec < 20000) rec[nrec++] = (rec_t){e->on, e->hand, e->note, e->vel, now};
    if (e->on) sounding[e->hand][e->note]++;
    else if (sounding[e->hand][e->note] > 0) sounding[e->hand][e->note]--;
    int n = 0;
    for (int h = 0; h < 2; ++h) for (int k = 0; k < 128; ++k) n += sounding[h][k] > 0;
    if (n > max_poly) max_poly = n;
}

static ff_player P;
static void reset(void)
{
    ff_player_init(&P, SR);
    ff_set_sound(&P, sound, 0);
    nrec = 0; now = 0; max_poly = 0;
    memset(sounding, 0, sizeof sounding);
}
static void run(float seconds)
{
    long n = (long)(seconds * SR);
    for (long f = 0; f < n; f += BLOCK) { ff_process(&P, BLOCK); now += BLOCK; }
}
static int count_on(int hand) { int n = 0; for (int i = 0; i < nrec; ++i) n += rec[i].on && rec[i].hand == hand; return n; }

int main(void)
{
    /* 1. the score, as written */
    reset();
    ff_play(&P, true);
    float secs = 0;
    while (P.playing && secs < 400) { run(0.5f); secs += 0.5f; }
    run(4);
    int rh = 0, lh = 0;
    for (int i = 0; i < FF_NOTE_N; ++i) { if (FF_NOTES[i].nh >> 7) lh++; else rh++; }
    CHECK(!P.playing && P.ended, "the performance ends (FORM ONCE)");
    CHECK(count_on(0) == rh, "right hand: %d notes, the score has %d", count_on(0), rh);
    CHECK(count_on(1) == lh, "left hand: %d notes, the score has %d", count_on(1), lh);
    {   /* in order, the score's pitches */
        int ok = 1, ri = 0;
        for (int i = 0; i < FF_NOTE_N; ++i) {
            if (FF_NOTES[i].nh >> 7) continue;
            while (ri < nrec && !(rec[ri].on && rec[ri].hand == 0)) ri++;
            if (ri >= nrec || rec[ri].note != (FF_NOTES[i].nh & 127)) { ok = 0; break; }
            ri++;
        }
        CHECK(ok, "the right hand plays the score's notes in order");
    }
    int open = 0;
    for (int h = 0; h < 4; ++h) for (int k = 0; k < 128; ++k) open += sounding[h][k];
    CHECK(open == 0, "every note-on has its note-off (%d left)", open);
    printf("score: %.1f s (the score's tempo without rubato: %.1f s), at most %d notes held by the pedal\n", secs,
           79 * 4 * 60.0 / 176.0, max_poly);
    CHECK(secs > 100 && secs < 135, "a performance of %.1f s", secs);
    {   /* velocities in range, louder at the crests than in the troughs */
        int lo = 127, hi = 0;
        for (int i = 0; i < nrec; ++i) if (rec[i].on && rec[i].hand < 2) { if (rec[i].vel < lo) lo = rec[i].vel; if (rec[i].vel > hi) hi = rec[i].vel; }
        printf("velocities %d..%d\n", lo, hi);
        CHECK(lo >= 20 && hi <= 127 && hi - lo >= 20, "velocities %d..%d", lo, hi);
    }

    /* 2. the loop: bars 11..12 four times, the ramp from 60 % */
    reset();
    P.s.from = 11; P.s.to = 12; P.s.speed = 60; P.s.ramp = 2;
    ff_set_loop(&P, true);
    ff_play(&P, true);
    run(16);
    CHECK(P.loops >= 3, "the loop came round (%d)", P.loops);
    CHECK(P.s.speed > 60, "RAMP raised the speed (%d %%)", P.s.speed);
    CHECK(ff_bar(&P) >= 11 && ff_bar(&P) <= 12, "inside the loop (bar %d)", ff_bar(&P));
    ff_play(&P, false);

    /* 3. WAIT: the right hand left out; the music waits; a wrong key, then the right ones */
    reset();
    P.s.you = FF_YOU_RIGHT; P.s.mode = FF_MODE_WAIT;
    ff_play(&P, true);
    run(1);
    CHECK(P.waiting && P.want_step == 4, "waits at the first right-hand note (step %d)", P.want_step);
    CHECK(count_on(0) == 0 && count_on(1) == 5, "only the left hand sounded: the held octave and three sixteenths (%d %d)", count_on(0), count_on(1));
    ff_key(&P, 62, true); ff_key(&P, 62, false);             /* D: wrong */
    CHECK(P.waiting && !strcmp(P.msg, "NOT D"), "a wrong key: still waiting, \"%s\"", P.msg);
    const int bar1[12] = {64, 60, 67, 72, 76, 72, 79, 84, 88, 84, 91, 96};
    for (int i = 0; i < 12; ++i) {
        CHECK(P.waiting && P.nwant == 1 && P.want[0] == bar1[i], "waits for %d (has %d)", bar1[i], P.want[0]);
        ff_key(&P, 53 + ((bar1[i] % 12) - 53 % 12 + 12) % 12, true);   /* the pitch class, low on the keys */
        ff_key(&P, 53 + ((bar1[i] % 12) - 53 % 12 + 12) % 12, false);
        run(0.2f);
    }
    {
        int ok = 1, k = 0;
        for (int i = 0; i < nrec; ++i) if (rec[i].on && rec[i].hand == 0) { if (k < 12 && rec[i].note != bar1[k]) ok = 0; k++; }
        CHECK(ok && k >= 12, "the right hand sounded the score's octaves for the keys found (%d)", k);
    }
    ff_play(&P, false);

    /* 4. TAP: each key the next note */
    reset();
    P.s.mode = FF_MODE_TAP;
    ff_play(&P, true);
    run(1);
    CHECK(count_on(0) + count_on(1) == 0, "TAP: nothing before a tap");
    for (int i = 0; i < 6; ++i) { ff_key(&P, 60, true); ff_key(&P, 60, false); run(0.1f); }
    CHECK(P.pos == 6, "six taps, six sixteenths (%d)", P.pos);

    /* 5. the variations */
    reset();
    ff_shown a[600], b[600], c[600];
    int na = ff_notes_in(&P, 0, 16 * 16, a, 600);
    P.s.vary = 100; P.s.figure = FF_FIG_WAVE; P.s.seed = 5;
    int nb = ff_notes_in(&P, 0, 16 * 16, b, 600);
    int nc = ff_notes_in(&P, 0, 16 * 16, c, 600);
    CHECK(nb == nc && !memcmp(b, c, sizeof(ff_shown) * (size_t)nb), "the same seed, the same notes");
    {
        int varied = 0, out_of_harmony = 0;
        for (int i = 0; i < nb; ++i) {
            if (!b[i].var) continue;
            varied++;
            int hb = b[i].step / 8, ok = 0;
            for (int k = FF_FIRST[hb * 8]; k < FF_FIRST[hb * 8 + 8]; ++k) ok |= (FF_NOTES[k].nh & 127) % 12 == b[i].note % 12;
            out_of_harmony += !ok;
        }
        CHECK(varied > 100 && out_of_harmony == 0, "VARY 100: %d new notes, %d outside their half bar's harmony", varied, out_of_harmony);
    }
    {
        int same = 1;
        P.s.vary = 0;
        int nd = ff_notes_in(&P, 0, 16 * 16, c, 600);
        same = nd == na && !memcmp(a, c, sizeof(ff_shown) * (size_t)na);
        CHECK(same, "VARY 0: the score");
    }
    for (int f = 0; f < FF_FIG_COUNT; ++f) {
        P.s.vary = 100; P.s.figure = (uint8_t)f; P.s.spread = 4;
        int n = ff_notes_in(&P, 0, FF_STEPS, c, 600);
        int bad = 0;
        for (int i = 0; i < n; ++i) bad += c[i].note < 12 || c[i].note > 120;
        CHECK(n > 0 && !bad, "figure %d: %d notes, %d out of range", f, n, bad);
    }
    /* the key and the mode */
    P.s.vary = 0; P.s.figure = FF_FIG_WAVE; P.s.key = 2; P.s.scale = FF_SCALE_MINOR;
    ff_notes_in(&P, 0, 16, c, 600);
    {
        int r = 0;
        while (c[r].hand != 0) r++;
        CHECK(c[0].note == 36 + 2 && c[r].note == 63 + 2, "D minor: bar 1 from D2, its right hand from F4 (%d %d)", c[0].note, c[r].note);
    }
    {
        char nm[24];
        P.s.scale = FF_SCALE_SCORE;
        ff_chord_name(&P, 1, nm, sizeof nm);
        CHECK(!strcmp(nm, "D MAJOR"), "bar 1 in KEY +2: \"%s\"", nm);
        P.s.key = 0;
        ff_chord_name(&P, 4, nm, sizeof nm);
        printf("bar 4: %s\n", nm);
    }
    /* the bass */
    P.s.key = 0; P.s.scale = 0; P.s.bass = FF_BASS_SINGLE;
    int ns = ff_notes_in(&P, 0, 1, c, 600);
    CHECK(ns == 1 && c[0].note == 48, "BASS SINGLE: one held note (%d, %d)", ns, c[0].note);
    P.s.bass = FF_BASS_NONE;
    ns = ff_notes_in(&P, 0, 1, c, 600);
    CHECK(ns == 0, "BASS NONE: no held bass (%d)", ns);

    /* 6. the pedal: only the held bass keeps its key down to the pedal change (bar 3 here, where F1 comes); the
     * figures let go at once (they ring on in the track's release) */
    reset();
    ff_play(&P, true);
    run(2.2f);                                                 /* bar 2: C2 still held (C3 was struck again on the
                                                                * next sixteenth, as a pianist's key), the figures not */
    int held_bass = sounding[1][36], held_figs = 0;
    for (int k = 0; k < 128; ++k) held_figs += sounding[0][k];
    CHECK(held_bass == 1, "SCORE: the bass C2 held into bar 2 (%d)", held_bass);
    CHECK(held_figs <= 1, "SCORE: the figures let go at once (%d held)", held_figs);
    run(1.5f);
    CHECK(sounding[1][36] == 0, "SCORE: the bass let go at the pedal change");
    ff_play(&P, false);
    reset();
    P.s.pedal = FF_PED_WASH;
    ff_play(&P, true); run(3.6f);
    CHECK(sounding[1][36] == 1, "WASH: the bass held through the pedal change of bar 3");
    ff_play(&P, false);
    {
        int notes[6];
        int n = ff_reso_notes(&P, notes);
        CHECK(n >= 3, "the resonator has strings (%d)", n);
    }

    printf("ff_player: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
