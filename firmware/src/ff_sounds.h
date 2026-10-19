/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH's sounds on Felucca's engines, and the cards that set a track: shared by the firmware (ff_fw.c) and
 * the host render (tests/ff_felucca.c), so both play the same. Needs core.h (track_t, P_*, the song) first.
 *
 * Tracks: 1 the piano's treble (C4 and up), 2 its bass (below C4), 3 the FM-1's keys (playing along), 4 the click
 * (the DRUM engine's wood blocks). The piano is split by pitch, not by hand: a note always plays on the same track,
 * so a key one hand strikes while the other's rings is struck again (Felucca restarts the voice of the same note)
 * instead of sounding twice, and the two halves sit left and right as a piano does from the bench: the bass left,
 * the treble right, the arpeggios sweeping across. WIDTH sets how far apart; RIGHT and LEFT are the hands' velocities
 * (ff_player.c). The samples are mono (stereo ones would take twice the flash); the reverb is stereo. */
#ifndef FF_SOUNDS_H
#define FF_SOUNDS_H

#include "ff_player.h"

enum { FFS_ANALOG = 0, FFS_PHASE = 2, FFS_SAMPLE = 4, FFS_PHYS = 9, FFS_FM6 = 12 };
typedef struct { uint8_t engine, preset; } ffs_sound;   /* (the names: ff_player.c ff_sound_name, in this order) */
static const ffs_sound FFS_SOUND[FF_SOUNDS] = {
    {FFS_SAMPLE, 0},    /* GRAND: the piano set: a Steinway D (bitKlavier Grand, CC BY 4.0) */
    {FFS_FM6, 0},       /* TINE EP */
    {FFS_PHYS, 8},      /* HARP */
    {FFS_PHASE, 4},     /* CELESTA: PHASE's BELL */
    {FFS_FM6, 1},       /* MUSIC BOX: FM6's BELL */
    {FFS_PHYS, 4},      /* KALIMBA */
    {FFS_FM6, 5},       /* MARIMBA */
    {FFS_ANALOG, 5},    /* SINE KEY */
    {FFS_SAMPLE, 0},    /* PLEYEL: the same Steinway, voiced as an 1840s Pleyel (ffs_track): darker, a quicker decay,
                         * a lighter touch. A voicing, not a recording of a Pleyel */
};
#define FFS_PLEYEL 8

/* the reverb spaces: SIZE and DAMP on Felucca's ROOM model, as Tuna's */
static const uint8_t FFS_SPACE[FF_SPACE_COUNT][2] = {{71, 48}, {86, 45}, {97, 30}};

/* the track a note plays on: the treble 0, the bass 1 (split at middle C), the keys 2, the click 3 */
#define FFS_SPLIT 60
enum { FFS_TREBLE, FFS_BASS, FFS_KEYS, FFS_CLICK };
static inline uint32_t ffs_track_of(int hand, int note)
{
    if (hand == FF_HAND_KEYS || hand == FF_HAND_CLICK)
        return (uint32_t)hand;
    return note < FFS_SPLIT ? FFS_BASS : FFS_TREBLE;
}

static inline int16_t ffs_clamp(int v) { return (int16_t)(v < 0 ? 0 : v > 127 ? 127 : v); }

/* a music track's settings over its preset (k: FFS_TREBLE, FFS_BASS, FFS_KEYS); base_dec / base_rel are the
 * preset's own DEC and REL */
static inline void ffs_track(track_t *t, const ff_settings *s, int k, int16_t base_dec, int16_t base_rel)
{
    int lvl = k == FFS_KEYS ? 85 : 100;             /* (the hands' own levels: their velocities, ff_player.c) */
    t->p[P_VOICE] = V_POLY;
    t->p[P_LEVEL] = ffs_clamp(lvl * 104 / 100);         /* (louder since 0.3: the loudest bars into the limiter) */
    t->p[P_REV] = ffs_clamp(s->reverb * 127 / 100);
    /* WIDTH: the bass left, the treble right (PAN is -64 .. 63, 0 the centre; WIDTH 100: 40 each side) */
    t->p[P_PAN] = (int16_t)((k == FFS_TREBLE ? 1 : k == FFS_BASS ? -1 : 0) * s->width * 40 / 100);
    t->p[P_SPRD] = 0;
    /* DECAY: the preset's own decay and release, scaled 0.4x .. 2x around 60 */
    int f = s->decay < 60 ? 40 + s->decay : 100 + (s->decay - 60) * 100 / 40;   /* percent */
    if (s->sound == FFS_PLEYEL)
        f = f * 62 / 100;                        /* PLEYEL: lighter strings, a quicker decay */
    t->p[P_DEC] = ffs_clamp(base_dec * f / 100);
    t->p[P_REL] = ffs_clamp(base_rel * f / 100);
    /* the pedal: the figures let go of their keys at once (ff_player.c release_after) and ring on in a long release,
     * as under a pedal; HALF shorter, DRY the preset's own */
    static const uint8_t PED_REL[FF_PED_COUNT] = {104, 88, 0, 118};
    if (PED_REL[s->pedal % FF_PED_COUNT] && k != FFS_KEYS)
        t->p[P_REL] = ffs_clamp(PED_REL[s->pedal % FF_PED_COUNT] * f / 100);
    /* the dynamics: the matrix's last slot, VEL -> AMP (the SAMPLE engine has no velocity of its own; the FM and
     * physical engines have theirs, so less there) */
    t->p[P_M4SRC] = 3;                           /* MS_VEL */
    t->p[P_M4DST] = 4;                           /* MD_AMP */
    t->p[P_M4AMT] = (int16_t)(s->sound == FFS_PLEYEL ? 50 : t->engine == FFS_SAMPLE || t->eng_req == FFS_SAMPLE ? 64 : 36);
    /* BRIGHT: the INSERT's CRUSH at 16 bits and the full rate is clean: its low-pass alone (100: off). PLEYEL and
     * SOFT (una corda) darker */
    int lpf = 56 + s->bright * 70 / 100 - (s->sound == FFS_PLEYEL ? 16 : 0) - (s->soft ? 12 : 0);
    if (s->bright >= 96 && lpf >= 123) {
        t->p[P_ITYPE] = 0;
    } else {
        t->p[P_ITYPE] = 5;                       /* IT_CRUSH */
        t->p[P_IA] = 127;                        /* 16 bits */
        t->p[P_IB] = 127;                        /* the full rate */
        t->p[P_IC] = ffs_clamp(lpf);             /* LPF */
        t->p[P_IMIX] = 127;
    }
}

/* the tuning: a note's temperament as its voices' fine pitch (1/4096 of the phase increment: 1 cent = 2.367); the
 * concert pitch is Felucca's TUNE (song.g[G_TUNE], cents) */
static inline int32_t ffs_fine(const ff_settings *s, int note)
{
    float c = ff_temper_ct(s, note % 12) * 2.367f;
    return (int32_t)(c < 0 ? c - 0.5f : c + 0.5f);
}
static inline void ffs_note_on(track_t *t, const ff_settings *s, uint32_t note, uint32_t vel)
{
    trk_note_on(t, note, vel);
    int32_t fine = ffs_fine(s, (int)note);
    if (fine)
        for (uint32_t i = 0; i < NVOICE; i++)       /* (the voice just started on this note: POLY, no unison) */
            if (t->v[i].gate && t->v[i].note == note)
                t->v[i].fine = fine;
}
static inline int16_t ffs_tune(const ff_settings *s)
{
    float c = ff_pitch_ct(s);
    return (int16_t)(c < 0 ? c - 0.5f : c + 0.5f);
}

#endif
