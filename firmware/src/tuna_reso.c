/* SPDX-License-Identifier: GPL-3.0-only */
/* TUNA resonator (see tuna_reso.h). */
#include "tuna_reso.h"
#include "tuna_sys.h"

#define TR_MASK (TR_LINE - 1)

/* v * q >> 15, rounded toward zero: a decay that reaches 0 (a floor would leave -1 for ever, a DC in the loop) */
static int32_t mulq15(int32_t v, int32_t q)
{
    int64_t p = (int64_t)v * q;
    return (int32_t)((p + ((p >> 63) & 32767)) >> 15);     /* (branchless: a negative product rounds up) */
}

/* a string's loop gain for its period (half-rate samples): its RT60 shorter as it goes higher */
static int32_t loop_gain(const tr_reso *r, float period)
{
    float fs2 = r->sr * 0.5f, hz = fs2 / period;
    float rt = r->decay_s * sqrtf(220.0f / hz);
    if (rt < 0.05f)
        rt = 0.05f;
    float gain = exp2f(-9.9658f * period / (fs2 * rt));   /* 60 dB (log2 1000 = 9.97) over rt seconds */
    if (gain > 0.9995f)
        gain = 0.9995f;
    return (int32_t)(gain * 32768.0f);
}

static int32_t input_gain(int32_t g)
{
    int32_t ig = (32768 - g) * 4;
    return ig > 32767 ? 32767 : ig;
}

static float period_of(const tr_reso *r, int note)
{
    float hz = 440.0f * exp2f((float)(note - 69) / 12.0f);
    float p = r->sr * 0.5f / hz;
    while (p > (float)(TR_LINE - 4))
        p *= 0.5f;                                 /* too low for the line: an octave up */
    return p;
}

void tr_init(tr_reso *r, float sample_rate)
{
    memset(r, 0, sizeof *r);
    r->sr = sample_rate;
    for (int k = 0; k < TR_STRINGS; ++k)
        r->note[k] = -1;
    tr_set(r, 0.0f, 0.5f, 0.5f);
}

void tr_set(tr_reso *r, float amount, float decay, float bright)
{
    amount = amount < 0.0f ? 0.0f : amount > 1.0f ? 1.0f : amount;
    decay = decay < 0.0f ? 0.0f : decay > 1.0f ? 1.0f : decay;
    bright = bright < 0.0f ? 0.0f : bright > 1.0f ? 1.0f : bright;
    r->amount = (int32_t)(amount * 32767.0f);
    r->decay_s = 0.3f * exp2f(5.0f * decay);       /* 0.3 s .. 9.6 s */
    r->bright = (int32_t)((0.15f + 0.8f * bright) * 32767.0f);
    for (int k = 0; k < TR_STRINGS; ++k)
        if (r->note[k] >= 0) {
            r->g[k] = loop_gain(r, (float)r->d_to[k] / 65536.0f);
            r->ig[k] = input_gain(r->g[k]);
        }
}

void tr_tune(tr_reso *r, const int *notes, int n)
{
    for (int k = 0; k < TR_STRINGS; ++k) {
        int note = k < n ? notes[k] : -1;
        if (note == r->note[k])
            continue;                              /* (unchanged: it rings on) */
        if (note < 0) {
            r->note[k] = -1;
            r->g[k] = 0;                           /* off: it dies away within a period */
            r->ig[k] = 0;
            continue;
        }
        float p = period_of(r, note);
        r->d_to[k] = (int32_t)(p * 65536.0f);
        if (r->note[k] < 0)
            r->d[k] = r->d_to[k];                  /* a string that was off starts in tune */
        r->note[k] = (int8_t)note;
        r->g[k] = loop_gain(r, p);
        r->ig[k] = input_gain(r->g[k]);
    }
}

int tr_notes_for(int mode, int tonic, int chord_root, const int8_t *tones, int ntones, int *notes)
{
    int n = 0;
    int base = 48 + (tonic % 12);                  /* the strings sit around C3 .. C5 */
    switch (mode) {
    case TR_TUNE_KEY:
        notes[n++] = base - 12;
        notes[n++] = base - 5;
        notes[n++] = base;
        notes[n++] = base + 7;
        notes[n++] = base + 12;
        notes[n++] = base + 19;
        break;
    case TR_TUNE_ROOT: {
        int root = base + chord_root % 12;
        if (root > base + 6) root -= 12;
        for (int o = 0; o < 3; ++o) {
            notes[n++] = root - 12 + 12 * o;
            notes[n++] = root - 5 + 12 * o;            /* and its fifth below each */
        }
        break;
    }
    default: {                                     /* CHORD: its tones, then the root and fifth an octave up */
        int root = base + chord_root % 12;
        if (root > base + 6) root -= 12;
        for (int i = 0; i < ntones && n < 4; ++i)
            notes[n++] = root + tones[i] % 12 + (tones[i] >= 12 ? 12 : 0);
        notes[n++] = root + 12;
        if (n < TR_STRINGS)
            notes[n++] = root + 19;
        break;
    }
    }
    return n;
}

void tr_process(tr_reso *r, const int32_t *in, int32_t *out, int n)
{
    if (r->amount <= 0) {
        r->phase = 0;
        return;
    }
    for (int i = 0; i < n; ++i) {
        /* the input: DC blocked, summed over a pair of samples */
        int32_t x = in[i] << 8;                    /* (8 bits below the sample, as in the lines) */
        int32_t y = x - r->dcx + mulq15(r->dcy, 32604);   /* 0.995 */
        r->dcx = x;
        r->dcy = y;
        r->half += y >> 8;
        if (r->phase ^= 1u) {
            /* the first of a pair: halfway between the last two string outputs */
            out[2 * i] += (r->pl + r->yl) >> 1;
            out[2 * i + 1] += (r->pr + r->yr) >> 1;
            continue;
        }
        int32_t xin = r->half << 6;                /* (two samples: half their mean, << 8 into the lines) */
        r->half = 0;
        r->wp = (r->wp + 1u) & TR_MASK;
        int32_t sl = 0, sr = 0;
        for (int k = 0; k < TR_STRINGS; ++k) {
            if (!r->g[k]) {
                if (r->lp[k] > -256 && r->lp[k] < 256) {   /* off and quiet (under a bit): done */
                    r->lp[k] = 0;
                    continue;
                }
            }
            if (r->d[k] != r->d_to[k])
                r->d[k] += (r->d_to[k] - r->d[k]) >> 9 ? (r->d_to[k] - r->d[k]) >> 9 : r->d_to[k] - r->d[k];   /* a retune glides (about 20 ms) */
            int32_t d = r->d[k], di = d >> 16, fr = d & 0xFFFF;
            const int32_t *ln = r->line[k];
            int32_t a = ln[(r->wp - (uint32_t)di) & TR_MASK], b = ln[(r->wp - (uint32_t)di - 1u) & TR_MASK];
            int32_t s = a + (int32_t)(((int64_t)(b - a) * fr) >> 16);
            r->lp[k] += mulq15(s - r->lp[k], r->bright);
            int32_t v = mulq15(r->lp[k], r->g[k]) + mulq15(xin, r->ig[k]);
            r->line[k][r->wp] = v > (32767 << 8) ? (32767 << 8) : v < -(32768 << 8) ? -(32768 << 8) : v;
            if (k & 1) sr += r->lp[k]; else sl += r->lp[k];
        }
        r->pl = r->yl;
        r->pr = r->yr;
        r->yl = (int32_t)(((int64_t)sl * r->amount) >> 23);   /* (<< 8 off) */
        r->yr = (int32_t)(((int64_t)sr * r->amount) >> 23);
        out[2 * i] += r->yl;
        out[2 * i + 1] += r->yr;
    }
}
