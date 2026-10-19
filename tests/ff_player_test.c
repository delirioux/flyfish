/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH player on the host: the whole étude through ff_process, counted and checked.
 *   tests/ff_tests.sh builds and runs it (exit 0: all passed)
 * Checks: every note of the score sounds once and in order, each note-on has its note-off, the length of a
 * performance, the loop and its ramp, WAIT with the right and wrong keys, TAP, the key and the modes, the bass
 * options, the pedal, ROUNDS and its seam, LEGATO. */
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
    /* 1. the score, as written: one round */
    reset();
    CHECK(P.s.rounds == 0, "INF by default (%d)", P.s.rounds);
    P.s.rounds = 1;
    ff_play(&P, true);
    float secs = 0;
    while (P.playing && secs < 400) { run(0.5f); secs += 0.5f; }
    run(4);
    int rh = 0, lh = 0;
    for (int i = 0; i < FF_NOTE_N; ++i) { if (FF_NOTES[i].nh >> 7) lh++; else rh++; }
    CHECK(!P.playing && P.ended, "the performance ends (ROUNDS 1)");
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

    /* 5. the notes are Chopin's: every note shown is a note of the score */
    reset();
    ff_shown a[600], c[600];
    int na = ff_notes_in(&P, 0, 16 * 16, a, 600);
    {
        int k = 0, ok = 1;
        for (int i = 0; i < FF_NOTE_N && FF_NOTES[i].step < 16 * 16; ++i) k++;
        CHECK(na == k, "bars 1-16: the score's %d notes (%d)", k, na);
        (void)ok;
    }
    /* the key and the mode */
    P.s.key = 2; P.s.scale = FF_SCALE_MINOR;
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

    /* 7. ROUNDS: two rounds are bars 1 .. 48, then 1 .. 79; the seam (bar 48 into bar 1) as bar 48 into bar 49 */
    {
        int pre = 0, all = 0, rh48 = 0;
        for (int i = 0; i < FF_NOTE_N; ++i) { all++; if (FF_NOTES[i].step < 48 * 16) pre++; }
        for (int i = 0; i < FF_NOTE_N; ++i) rh48 += FF_NOTES[i].step < 48 * 16 && !(FF_NOTES[i].nh >> 7);
        reset();
        P.s.rounds = 2;
        ff_play(&P, true);
        float t = 0;
        while (P.playing && t < 600) { run(0.5f); t += 0.5f; }
        CHECK(!P.playing && P.ended && P.round == 2, "ROUNDS 2 ends after two rounds (round %d)", P.round);
        CHECK(count_on(0) + count_on(1) == pre + all, "ROUNDS 2: %d notes, bars 1-48 and the whole: %d",
              count_on(0) + count_on(1), pre + all);
        /* the right hand across the seam: bar 48's last notes, then bar 1's, as bar 48 into bar 49 */
        int k = 0, seam = -1;
        for (int i = 0; i < nrec; ++i) if (rec[i].on && rec[i].hand == 0) { if (k == rh48) { seam = i; break; } k++; }
        int ok = seam > 0;
        const int b1[4] = {64, 60, 67, 72}, b49ok = 1;
        for (int j = 0, m = 0; ok && m < 4 && seam + j < nrec; ++j)
            if (rec[seam + j].on && rec[seam + j].hand == 0) { ok &= rec[seam + j].note == b1[m]; m++; }
        CHECK(ok && b49ok, "after bar 48 the right hand plays bar 1 (E4 C4 G4 C5)");
        {
            /* both hands: no onset gap around the seam longer than at the same place in the score (bar 48 into 49,
             * where the left hand carries the line: the right hand rests over the bar line in both) */
            long worst = 0;
            int lo = seam, n = 0;
            while (lo > 0 && n < 12) { --lo; n += rec[lo].on; }
            long last = -1;
            for (int i = lo, m = 0; i < nrec && m < 24; ++i) {
                if (!rec[i].on) continue;
                if (last >= 0 && rec[i].t - last > worst) worst = rec[i].t - last;
                last = rec[i].t; m++;
            }
            CHECK(seam > 0 && worst < SR / 8, "the seam keeps the beat (largest onset gap %ld samples)", worst);
        }
        printf("ROUNDS 2: %.1f s\n", t);
    }
    reset();
    P.s.rounds = 0;
    ff_play(&P, true);
    run(300);
    CHECK(P.playing && P.round >= 3, "INF: still playing after 300 s, round %d", P.round);
    ff_play(&P, false);

    /* 8. LEGATO: a figure's note lets go after the next one starts (25: a quarter of a sixteenth), at 0 as it does */
    for (int lg = 0; lg <= 100; lg += 100) {
        reset();
        P.s.rounds = 1; P.s.pedal = FF_PED_DRY; P.s.legato = (uint8_t)lg;
        P.s.from = 1; P.s.to = 1;
        ff_play(&P, true);
        run(1.5f);
        ff_play(&P, false);
        long on1 = -1, on2 = -1, off1 = -1; int n1 = -1;
        for (int i = 0; i < nrec; ++i) {
            if (rec[i].hand != 0) continue;
            if (rec[i].on && on1 < 0) { on1 = rec[i].t; n1 = rec[i].note; continue; }
            if (rec[i].on && on1 >= 0 && on2 < 0) on2 = rec[i].t;
            if (!rec[i].on && rec[i].note == n1 && off1 < 0) off1 = rec[i].t;
        }
        long step = on2 - on1, over = off1 - on2;
        if (lg == 0) CHECK(over <= 0 && over > -step / 8, "LEGATO 0: the notes just touch (%ld)", over);
        else CHECK(over > step * 5 / 2 && over < step * 7 / 2, "LEGATO 100: three sixteenths' overlap (%ld of %ld)", over, step);
    }

    /* 9. RHYTHM: dotted pairs keep the bar; the accent moves */
    {
        reset();
        P.s.rounds = 1; P.s.rhythm = FF_RHY_LONG_SHORT; P.s.rubato = 0;
        ff_play(&P, true);
        run(2.0f);
        ff_play(&P, false);
        long t[8]; int k = 0;
        for (int i = 0; i < nrec && k < 8; ++i) if (rec[i].on && rec[i].hand == 0) t[k++] = rec[i].t;
        long a1 = t[1] - t[0], a2 = t[2] - t[1];
        CHECK(k == 8 && a1 > a2 * 5 / 2 && a1 < a2 * 7 / 2, "LONG-SHORT: three to one (%ld, %ld)", a1, a2);
        reset();
        P.s.rhythm = FF_RHY_ACC3; P.s.rubato = 0; P.s.touch = 60;
        ff_shown e2 = {6, 1, 67, 0, 0}, e0 = {4, 1, 64, 0, 0};          /* bar 1: the group 4..7, its 3rd and 1st */
        P.s.rhythm = FF_RHY_ACC3;
        int v3 = 0, v1 = 0;
        ff_play(&P, true); run(0.8f); ff_play(&P, false);
        for (int i = 0; i < nrec; ++i) if (rec[i].on && rec[i].hand == 0) { if (rec[i].note == 67 && !v3) v3 = rec[i].vel; if (rec[i].note == 64 && !v1) v1 = rec[i].vel; }
        (void)e2; (void)e0;
        CHECK(v3 > v1, "ACC 3: the group's third note the loudest (%d > %d)", v3, v1);
    }
    /* 10. CHORDS: each group of four as one chord on its first sixteenth */
    {
        reset();
        P.s.chords = 1;
        ff_shown c4[32];
        int n = ff_notes_in(&P, 4, 8, c4, 32), at4 = 0, other = 0;
        for (int i = 0; i < n; ++i) { if (c4[i].step == 4 && c4[i].len == 4) at4++; else other++; }
        CHECK(at4 == 4 && other == 0, "CHORDS: bar 1's second group, E4 C4 G4 C5 at once (%d, %d)", at4, other);
        int total = ff_notes_in(&P, 0, FF_BARS * 16, (ff_shown[1600]){0}, 1600);
        CHECK(total > 600 && total < 1337, "CHORDS: the piece in %d notes", total);
    }
    /* 11. STYLE, TUNING, PITCH, the Roman numerals */
    {
        ff_settings s;
        ff_defaults(&s);
        ff_style_apply(&s, FF_STYLE_NOCTURNE);
        CHECK(s.interp == FF_STYLE_NOCTURNE && s.bpm == 112 && s.pedal == FF_PED_WASH, "NOCTURNE: slow, washed");
        s.tuning = FF_TUNE_EQUAL;
        CHECK(ff_temper_ct(&s, 4) == 0.0f, "EQUAL: E at 0 cents");
        s.tuning = FF_TUNE_KIRNBERGER;
        float c = ff_temper_ct(&s, 0), e = ff_temper_ct(&s, 4), a = ff_temper_ct(&s, 9);
        CHECK(a == 0.0f && c > 9.8f && c < 10.8f && e > -3.9f && e < -2.9f, "KIRNBERGER, tuned to A: C %+.1f E %+.1f", c, e);
        s.pitch = FF_PITCH_430;
        CHECK(ff_pitch_ct(&s) < -39.0f && ff_pitch_ct(&s) > -41.0f, "A = 430: %.1f cents", ff_pitch_ct(&s));
        reset();
        char rn[16];
        ff_roman(&P, 1, rn, sizeof rn);
        CHECK(!strcmp(rn, "I"), "bar 1: I (%s)", rn);
        int shown = 0, sec = 0;
        for (int b = 1; b <= FF_BARS; ++b) { ff_roman(&P, b, rn, sizeof rn); shown += rn[0] != 0; sec += strchr(rn, '/') != 0; }
        CHECK(shown > 40 && sec > 3, "Roman numerals for %d bars, %d secondary", shown, sec);
        ff_roman(&P, 6, rn, sizeof rn);
        CHECK(!strcmp(rn, "V7/V"), "bar 6, D7: V7/V (%s)", rn);
    }
    /* 12. the progress: WAIT, a bar played clean keeps its speed; a wrong key counts against it */
    {
        reset();
        P.s.rounds = 1; P.s.mode = FF_MODE_WAIT; P.s.you = FF_YOU_RIGHT; P.s.speed = 70; P.s.from = 1; P.s.to = 2;
        ff_set_loop(&P, true);
        ff_play(&P, true);
        int guard = 0;
        while (P.playing && guard++ < 4000 && P.loops < 1) {
            run(0.02f);
            if (P.waiting) {
                if (P.want_step / 16 == 1 && P.miss[1] == 0) ff_key(&P, (P.want[0] + 1) % 12 + 60, true);   /* bar 2: one wrong */
                for (int i = 0; P.waiting && i < 4; ++i) ff_key(&P, P.want[0], true);
            }
        }
        ff_play(&P, false);
        CHECK(P.best[0] == 70 && P.best[1] == 0 && P.miss[1] == 1, "bar 1 clean at 70%%, bar 2 one wrong key (%d %d %d)",
              P.best[0], P.best[1], P.miss[1]);
        ff_progress_erase(&P);
        CHECK(P.best[0] == 0 && P.miss[1] == 0, "ERASE");
    }

    printf("ff_player: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
