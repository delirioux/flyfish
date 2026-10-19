/* SPDX-License-Identifier: GPL-3.0-only */
/* TUNA resonator: a bank of sympathetic strings on the mix, a send effect in the manner of Rings
 * (Emilie Gillet's resonator; Felucca's PHYS engine has its SYMP model). The strings are tuned to the music:
 * the chord sounding now (CHORD), the key's tonic and fifth (KEY) or the chord's root in octaves (ROOT), and
 * retune when the harmony moves, gliding so nothing clicks. What the parts play excites them; they ring on.
 *
 * Fixed point, as Felucca's DSP: delay lines at half the audio rate (a 512-sample line: 43 Hz and up,
 * lower notes fold up an octave; 32-bit, the samples with 8 more bits below, so a long decay dies away
 * smoothly instead of sticking at the last bit: 12 KB), linear interpolation, a one-pole low-pass in the loop (BRIGHT), the loop
 * gain from each string's RT60 (DECAY), a DC blocker on the input, the output spread across the stereo field
 * (the strings alternate left and right). Integers only per sample; floats only when retuned or set. */
#ifndef TUNA_RESO_H
#define TUNA_RESO_H

#include <stdint.h>

#define TR_STRINGS 6
#define TR_LINE 512

enum { TR_TUNE_CHORD, TR_TUNE_KEY, TR_TUNE_ROOT, TR_TUNE_COUNT };

typedef struct {
    int32_t line[TR_STRINGS][TR_LINE];         /* samples << 8 */
    int32_t d[TR_STRINGS], d_to[TR_STRINGS];   /* the delay now and where it glides, Q16 samples (half rate) */
    int32_t g[TR_STRINGS];                     /* loop gain, Q15 */
    int32_t ig[TR_STRINGS];                    /* input gain, Q15: (1 - g) x 4, so a string at its own pitch rings
                                                * about as loud however long it rings */
    int32_t lp[TR_STRINGS];                    /* the loop filter's state */
    int8_t note[TR_STRINGS];                   /* MIDI notes, -1: off */
    uint32_t wp;
    int32_t dcx, dcy;                          /* the input's DC blocker */
    int32_t half;                              /* the half-rate input summed over the pair */
    int32_t yl, yr, pl, pr;                    /* the last two outputs (linear back to the full rate) */
    uint32_t phase;
    float sr;
    int32_t amount;                            /* Q15: the strings into the mix */
    int32_t bright;                            /* Q15: the loop filter's coefficient */
    float decay_s;                             /* RT60 of a string at 220 Hz, seconds */
    float ct[12];                              /* (Flyfish) each pitch class's offset in cents: the temperament, the pitch */
} tr_reso;

void tr_init(tr_reso *r, float sample_rate);
/* amount, decay, brightness: 0..1 each */
void tr_set(tr_reso *r, float amount, float decay, float bright);
/* the strings' notes (MIDI; n up to TR_STRINGS, the rest off); unchanged notes keep ringing */
void tr_tune(tr_reso *r, const int *notes, int n);
/* (Flyfish) the strings' tuning: cents for C .. B (a temperament, the concert pitch); the strings sounding retune */
void tr_cents(tr_reso *r, const float *ct12);
/* the notes a mode wants: from the key's tonic (pitch class), the chord's root and tones (semitones above the
 * tonic, as tt_chord) */
int tr_notes_for(int mode, int tonic, int chord_root, const int8_t *tones, int ntones, int *notes);
/* in: the send (mono, about +-32767); adds the strings to out (interleaved stereo); n frames */
void tr_process(tr_reso *r, const int32_t *in, int32_t *out, int n);

#endif
