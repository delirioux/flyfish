/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH's sounds on Felucca's engines, and the cards that set a track: shared by the firmware (ff_fw.c) and
 * the host render (tests/ff_felucca.c), so both play the same. Needs core.h (track_t, P_*, the song) first.
 *
 * Tracks: 1 the right hand, 2 the left, 3 the FM-1's keys (playing along), 4 the click (the DRUM engine's wood
 * blocks). The three music tracks take the SOUND card's engine and factory preset. */
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
};

/* the reverb spaces: SIZE and DAMP on Felucca's ROOM model, as Tuna's */
static const uint8_t FFS_SPACE[FF_SPACE_COUNT][2] = {{71, 48}, {86, 45}, {97, 30}};

static inline int16_t ffs_clamp(int v) { return (int16_t)(v < 0 ? 0 : v > 127 ? 127 : v); }

/* a music track's settings over its preset: base_dec / base_rel are the preset's own DEC and REL */
static inline void ffs_track(track_t *t, const ff_settings *s, int hand, int16_t base_dec, int16_t base_rel)
{
    int lvl = hand == FF_HAND_R ? s->right : hand == FF_HAND_L ? s->left : 85;
    t->p[P_VOICE] = V_POLY;
    t->p[P_LEVEL] = ffs_clamp(lvl * 96 / 100);          /* (the loudest bars just under the master limiter) */
    t->p[P_REV] = ffs_clamp(s->reverb * 127 / 100);
    t->p[P_PAN] = ffs_clamp(64 + (hand == FF_HAND_R ? 1 : hand == FF_HAND_L ? -1 : 0) * s->width * 28 / 100);
    /* DECAY: the preset's own decay and release, scaled 0.4x .. 2x around 60 */
    int f = s->decay < 60 ? 40 + s->decay : 100 + (s->decay - 60) * 100 / 40;   /* percent */
    t->p[P_DEC] = ffs_clamp(base_dec * f / 100);
    t->p[P_REL] = ffs_clamp(base_rel * f / 100);
    /* the pedal: the figures let go of their keys at once (ff_player.c release_after) and ring on in a long release,
     * as under a pedal; HALF shorter, DRY the preset's own */
    static const uint8_t PED_REL[FF_PED_COUNT] = {104, 88, 0, 118};
    if (PED_REL[s->pedal % FF_PED_COUNT] && hand != FF_HAND_KEYS)
        t->p[P_REL] = ffs_clamp(PED_REL[s->pedal % FF_PED_COUNT] * f / 100);
    /* the dynamics: the matrix's last slot, VEL -> AMP (the SAMPLE engine has no velocity of its own; the FM and
     * physical engines have theirs, so less there) */
    t->p[P_M4SRC] = 3;                           /* MS_VEL */
    t->p[P_M4DST] = 4;                           /* MD_AMP */
    t->p[P_M4AMT] = (int16_t)(t->engine == FFS_SAMPLE || t->eng_req == FFS_SAMPLE ? 64 : 36);
    /* BRIGHT: the INSERT's CRUSH at 16 bits and the full rate is clean: its low-pass alone (100: off) */
    if (s->bright >= 96) {
        t->p[P_ITYPE] = 0;
    } else {
        t->p[P_ITYPE] = 5;                       /* IT_CRUSH */
        t->p[P_IA] = 127;                        /* 16 bits */
        t->p[P_IB] = 127;                        /* the full rate */
        t->p[P_IC] = ffs_clamp(56 + s->bright * 70 / 100 - (hand == FF_HAND_L ? 12 : 0));   /* LPF (the left hand
                                                         * darker: its bass rounder and deeper) */
        t->p[P_IMIX] = 127;
    }
}

#endif
