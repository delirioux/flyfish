/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH player (ff_player.h): the étude in time. See the header for what each setting does. */
#include "ff_player.h"
#include "tuna_sys.h"
#include <stddef.h>
#include "ff_etude.inc"

#define FF_END FF_STEPS

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *ff_note_name(int pc) { return NOTE_NAMES[((pc % 12) + 12) % 12]; }
static const char *const SOUND_NAMES[FF_SOUNDS] = {"GRAND", "TINE EP", "HARP", "CELESTA", "MUSIC BOX", "KALIMBA",
                                                   "MARIMBA", "SINE KEY"};
const char *ff_sound_name(int i) { return SOUND_NAMES[((i % FF_SOUNDS) + FF_SOUNDS) % FF_SOUNDS]; }

/* ---- settings ---------------------------------------------------------------------------------------------- */
void ff_defaults(ff_settings *s)
{
    memset(s, 0, sizeof *s);
    s->rubato = 35; s->touch = 70; s->accent = 50; s->pedal = FF_PED_SCORE;
    s->sound = 0; s->bright = 60; s->decay = 60; s->width = 70;
    s->speed = 100; s->from = 1; s->to = 2; s->you = FF_YOU_NONE;
    s->mode = FF_MODE_FREE; s->ramp = 0; s->click = 0; s->count = 0;
    s->key = 0; s->scale = FF_SCALE_SCORE; s->bass = FF_BASS_OCTAVE; s->rounds = 0;   /* INF */
    s->right = 65; s->left = 65; s->reverb = 25; s->space = FF_SPACE_CATHEDRAL;
    s->bpm = 176; s->palette = 3; s->style = 0; s->legato = 25;
}

void ff_settings_fix(ff_settings *s)
{
    s->rubato = (uint8_t)clampi(s->rubato, 0, 100); s->touch = (uint8_t)clampi(s->touch, 0, 100);
    s->accent = (uint8_t)clampi(s->accent, 0, 100); s->pedal = (uint8_t)(s->pedal % FF_PED_COUNT);
    s->sound = (uint8_t)(s->sound % FF_SOUNDS); s->bright = (uint8_t)clampi(s->bright, 0, 100);
    s->decay = (uint8_t)clampi(s->decay, 0, 100); s->width = (uint8_t)clampi(s->width, 0, 100);
    s->speed = (uint8_t)clampi(s->speed, 25, 100); s->from = (uint8_t)clampi(s->from, 1, FF_BARS);
    s->to = (uint8_t)clampi(s->to, s->from, FF_BARS); s->you = (uint8_t)(s->you % FF_YOU_COUNT);
    s->mode = (uint8_t)(s->mode % FF_MODE_COUNT); s->ramp = (uint8_t)(s->ramp % 4u);
    s->click = (uint8_t)(s->click % 2u); s->count = (uint8_t)(s->count % 2u);
    s->key = (int8_t)clampi(s->key, -6, 6);
    s->scale = (uint8_t)(s->scale % FF_SCALE_COUNT); s->bass = (uint8_t)(s->bass % FF_BASS_COUNT);
    s->rounds = (uint8_t)(s->rounds > FF_ROUNDS_MAX ? 0 : s->rounds); s->right = (uint8_t)clampi(s->right, 0, 100);
    s->left = (uint8_t)clampi(s->left, 0, 100); s->reverb = (uint8_t)clampi(s->reverb, 0, 100);
    s->space = (uint8_t)(s->space % FF_SPACE_COUNT); s->bpm = (uint8_t)clampi(s->bpm, 40, 240);
    s->palette = (uint8_t)(s->palette % 10u); s->style = (uint8_t)(s->style % 2u);
    s->legato = (uint8_t)clampi(s->legato, 0, 100);
}

/* ---- the notes as they play now ------------------------------------------------------------------------------ */
static inline int n_note(const ff_note *n) { return n->nh & 127; }
static inline int n_hand(const ff_note *n) { return n->nh >> 7; }

static int map_mode(const ff_settings *s, int n)
{
    int pc = ((n % 12) + 12) % 12, to = pc;
    switch (s->scale) {
    case FF_SCALE_MINOR: to = pc == 4 ? 3 : pc == 9 ? 8 : pc == 11 ? 10 : pc; break;
    case FF_SCALE_DORIAN: to = pc == 4 ? 3 : pc == 11 ? 10 : pc; break;
    case FF_SCALE_LYDIAN: to = pc == 5 ? 6 : pc; break;
    default: break;
    }
    return n - pc + to + s->key;
}

/* the notes starting at sixteenth s as they play now: up to max, in out */
static int step_notes(const ff_settings *s, int st, ff_shown *out, int max)
{
    int n = 0, a = FF_FIRST[st], b = FF_FIRST[st + 1];
    int lowest = 128, nheld = 0;
    for (int i = a; i < b; ++i)
        if (n_hand(&FF_NOTES[i]) == 1 && FF_NOTES[i].len >= 8) {
            nheld++;
            if (n_note(&FF_NOTES[i]) < lowest) lowest = n_note(&FF_NOTES[i]);
        }
    for (int i = a; i < b && n < max; ++i) {
        const ff_note *x = &FF_NOTES[i];
        int h = n_hand(x), note = n_note(x);
        if (h == 1 && x->len >= 8) {
            if (s->bass == FF_BASS_NONE) continue;
            if (s->bass == FF_BASS_SINGLE && nheld > 1 && note == lowest) continue;
            if (s->bass == FF_BASS_DRONE) note = note == lowest ? 24 : 36;
        }
        out[n++] = (ff_shown){x->step, x->len, (uint8_t)clampi(map_mode(s, note), 12, 120), (uint8_t)h, 0};
    }
    return n;
}

int ff_notes_in(const ff_player *p, int s0, int s1, ff_shown *out, int max)
{
    int n = 0;
    s0 = clampi(s0, 0, FF_STEPS);
    s1 = clampi(s1, 0, FF_STEPS);
    for (int st = s0; st < s1 && n < max; ++st)
        n += step_notes(&p->s, st, out + n, max - n);
    return n;
}

int ff_score_in(const ff_player *p, int s0, int s1, ff_shown *out, int max)
{
    int n = 0;
    s0 = clampi(s0, 0, FF_STEPS);
    s1 = clampi(s1, 0, FF_STEPS);
    for (int i = FF_FIRST[s0]; i < FF_FIRST[s1] && n < max; ++i) {
        const ff_note *x = &FF_NOTES[i];
        out[n++] = (ff_shown){x->step, x->len, (uint8_t)clampi(n_note(x) + p->s.key, 12, 120), (uint8_t)n_hand(x), 0};
    }
    return n;
}

/* ---- the interpretation: dynamics, accents, rubato (from the score's markings) ------------------------------ */
/* the dynamic level along the bars: f at the start, cresc. at 33, f 35, dimin. 36, cresc. 42, f 45, dim. 48, the
 * coda's hairpins at 69..76, dim. into the last bars */
static const float DYN[][2] = {
    {1, .78f}, {33, .78f}, {35, .92f}, {36, .92f}, {37, .6f}, {42, .62f}, {45, .92f}, {48, .92f}, {49, .66f},
    {68.9f, .76f}, {69.6f, .92f}, {70.6f, .68f}, {71, .68f}, {71.6f, .92f}, {72.6f, .68f}, {73, .68f}, {73.6f, .92f},
    {74.6f, .68f}, {75, .7f}, {75.7f, .95f}, {76.6f, .62f}, {77, .5f}, {77.6f, .6f}, {78.6f, .45f}, {79, .62f}, {80, .62f}};
static float dyn_at(float bar)
{
    for (unsigned i = 1; i < sizeof DYN / sizeof DYN[0]; ++i)
        if (bar <= DYN[i][0]) {
            float b0 = DYN[i - 1][0], v0 = DYN[i - 1][1], b1 = DYN[i][0], v1 = DYN[i][1];
            return v0 + (v1 - v0) * (bar - b0) / (b1 - b0);
        }
    return .7f;
}

static int velocity(const ff_settings *s, const ff_shown *e, bool recap)
{
    int ph = e->step % 32;
    float bar = (float)e->step / 16.0f + 1.0f, v = dyn_at(bar);
    if (recap && bar < 25.0f) {                        /* round again: the recapitulation's level (bar + 48), easing
                                                        * back to the opening's by bar 25 */
        float w = bar < 9.0f ? 1.0f : (25.0f - bar) / 16.0f;
        v = v + (dyn_at(bar + 48.0f) - v) * w;
    }
    if (e->hand == 0) {
        v += ph < 16 ? 0.09f * ((float)ph / 15.0f) : 0.09f * (1.0f - (float)(ph - 16) / 15.0f);   /* each wave */
        if (e->step % 4 == 0)
            v += 0.12f * (float)s->accent / 100.0f;                 /* the accent on each group's first note */
        if (FF_TOP[e->step / 32] == e->step)
            v += 0.1f * (float)s->accent / 100.0f;                  /* the wave's crest */
    } else {
        v += e->len >= 8 ? 0.22f : -0.04f;                          /* the held bass: weight under the waves */
    }
    v = 0.72f + (v - 0.72f) * ((float)s->touch / 60.0f);
    v *= 1.0f + sinf((float)e->step * 12.9898f + (float)e->note) * 0.5f * 0.04f * (float)s->rubato / 100.0f;
    v = clampf(v, 0.15f, 1.0f);
    v *= (float)(e->hand == FF_HAND_L ? s->left : s->right) / 100.0f;   /* RIGHT, LEFT: the hands share a track */
    return clampi((int)(v * 127.0f + 0.5f), 1, 127);
}

/* a sixteenth's length in seconds: the tempo, the speed, the wave breathing (pushing up, a breath at the crest,
 * easing at the wave's end), the last two bars broadening (a little less since 0.3), the fermata */
static float step_dur(const ff_settings *s, int st)
{
    float bpm = (float)s->bpm * (float)s->speed / 100.0f, d = 60.0f / bpm / 4.0f, R = (float)s->rubato / 100.0f;
    int ph = st % 32;
    float shape = 0.0f;
    if (ph < 15) shape = -0.045f * ((float)ph / 14.0f);
    else if (ph <= 18) shape = 0.12f * (1.0f - (float)(ph > 16 ? ph - 16 : 16 - ph) / 3.0f);
    else if (ph >= 28) shape = 0.07f * (float)(ph - 27) / 4.0f;
    d *= 1.0f + R * shape;
    if (st >= 76 * 16 && st < FF_END - 16) {
        float x = (float)(st - 76 * 16) / 32.0f;
        d *= 1.0f + (0.28f + 0.6f * R) * x * x;
    }
    if (st >= FF_END - 16)
        d *= 2.0f;
    return d;
}

static int pedal_index(int st)
{
    int k = -1;
    for (int i = 0; i < FF_PEDAL_N && FF_PEDAL[i] <= st; ++i)
        k = i;
    return k;
}

/* when a note's damper falls: its own length, held on by the pedal to the next change (frames from now). Chopin
 * writes legato over the figures: each finger lets go only after the next key goes down: LEGATO 0 the notes just
 * touch, 100 three sixteenths later (a finger pedal: four notes sounding; 25, the default, three quarters of one).
 * Under the pedal the figures ring on anyway: LEGATO is heard best with PEDAL DRY or HALF. The held bass keeps its
 * own length */
static float release_after(const ff_settings *s, const ff_shown *e, float d)
{
    float own = (float)e->len * d * (e->len < 8 ? 1.0f + 3.0f * (float)s->legato / (100.0f * (float)e->len) : 0.97f);
    /* only the held bass keeps its key down to the pedal change: the figures let go at once and ring on in their
     * release (ff_sounds.h: the pedal's REL), so when Felucca's 8 voices run out it is a released figure that gives
     * way, never the bass (voice.c voice_victim: released voices first) */
    if (s->pedal == FF_PED_DRY || !(e->hand == FF_HAND_L && e->len >= 8))
        return own;
    int grid;
    if (s->pedal == FF_PED_WASH) {
        grid = (e->step / 64 + 1) * 64;
    } else {
        int k = pedal_index(e->step);
        grid = k + 1 < FF_PEDAL_N ? FF_PEDAL[k + 1] : FF_END;   /* (the last pedal lifts at the end) */
    }
    float r = (float)(grid - e->step) * d - 0.015f;
    if (s->pedal == FF_PED_HALF) {
        float h = (float)(e->len > 6 ? e->len : 6) * d;
        if (h < r) r = h;
    }
    return r > own ? r : own;
}

/* ---- sound out ---------------------------------------------------------------------------------------------- */
static void send(ff_player *p, int on, int hand, int note, int vel)
{
    ff_event e = {(uint8_t)on, (uint8_t)hand, (uint8_t)note, (uint8_t)vel};
    if (p->sound)
        p->sound(p->sound_ctx, &e);
}

static void off_now(ff_player *p, int i)
{
    send(p, 0, p->held[i].hand, p->held[i].note, 0);
    p->held[i] = p->held[--p->nheld];
}

static void note_on(ff_player *p, const ff_shown *e, int vel, float hold_s)
{
    for (int i = 0; i < p->nheld; ++i)                 /* a key struck again: its damper first */
        if (p->held[i].note == e->note && p->held[i].hand == e->hand) {
            off_now(p, i);
            break;
        }
    if (p->nheld >= FF_HELD) {                         /* full: the one to end soonest goes now */
        int k = 0;
        for (int i = 1; i < p->nheld; ++i)
            if (p->held[i].off < p->held[k].off) k = i;
        off_now(p, k);
    }
    send(p, 1, e->hand, e->note, vel);
    uint32_t len = (uint32_t)(hold_s * p->sr);
    p->held[p->nheld].note = e->note;
    p->held[p->nheld].hand = e->hand;
    p->held[p->nheld].off = p->clock + (len ? len : 1u);
    p->nheld++;
    ff_seen f = {e->hand, e->note, (uint8_t)vel, e->var};
    uint8_t w = (uint8_t)((p->feed_w + 1u) % FF_FEED);
    if (w != p->feed_r) {
        p->feed[p->feed_w] = f;
        p->feed_w = w;
    }
}

static void say(ff_player *p, const char *m)
{
    int i = 0;
    for (; m[i] && i < (int)sizeof p->msg - 1; ++i)
        p->msg[i] = m[i];
    p->msg[i] = 0;
    p->msg_new = 1;
}

static void click(ff_player *p, bool strong)
{
    send(p, 1, FF_HAND_CLICK, strong ? 76 : 77, strong ? 112 : 80);   /* GM wood blocks, high and low */
    send(p, 0, FF_HAND_CLICK, strong ? 76 : 77, 0);
}

static int you_hand(const ff_player *p) { return p->s.you == FF_YOU_NONE ? -1 : p->s.you - 1; }

/* the pedal: a new one where the bass changes (the resonator retunes) */
static void pedal_follow(ff_player *p, int st)
{
    int k = pedal_index(st);
    if (k != p->pedal_idx) {
        p->pedal_idx = k;
        p->pedal_sig++;
    }
    p->pedal_down = p->s.pedal != FF_PED_DRY;
}

/* sixteenth st: every note but the player's hand */
static void play_step(ff_player *p, int st, int skip_hand)
{
    ff_shown ev[16];
    int n = step_notes(&p->s, st, ev, 16);
    float d = step_dur(&p->s, st);
    if (p->s.click && st % 4 == 0)
        click(p, st % 16 == 0);
    pedal_follow(p, st);
    for (int i = 0; i < n; ++i) {
        if (ev[i].hand == skip_hand)
            continue;
        note_on(p, &ev[i], velocity(&p->s, &ev[i], p->recap), release_after(&p->s, &ev[i], d));
    }
    p->cur = st;
}

static bool has_hand(const ff_player *p, int st, int hand)
{
    ff_shown ev[16];
    int n = step_notes(&p->s, st, ev, 16);
    for (int i = 0; i < n; ++i)
        if (ev[i].hand == hand) return true;
    return false;
}

/* where the sixteenth after a step goes: the jump asked for, the practice loop, the seam (ROUNDS), the end; -1 the
 * end of the last round */
static int wrap(ff_player *p, int st)
{
    if (p->jump && st % 16 == 0) {
        int j = (p->jump - 1) * 16;
        p->jump = 0;
        return j;
    }
    if (p->loop && st >= p->s.to * 16) {
        static const uint8_t RAMP[4] = {0, 2, 5, 10};
        p->loops++;
        if (p->s.ramp && p->s.speed < 100) {
            p->s.speed = (uint8_t)clampi(p->s.speed + RAMP[p->s.ramp], 25, 100);
            char m[24] = "SPEED ";
            int v = p->s.speed, k = 6;
            if (v >= 100) m[k++] = '1';
            if (v >= 10) m[k++] = (char)('0' + v / 10 % 10);
            m[k++] = (char)('0' + v % 10);
            m[k++] = '%';
            m[k] = 0;
            say(p, m);
        }
        return (p->s.from - 1) * 16;
    }
    if (st == FF_SEAM && (p->s.rounds == 0 || p->round < p->s.rounds)) {
        char m[24] = "ROUND ";
        int r = ++p->round, k = 6;
        if (r >= 100) m[k++] = (char)('0' + r / 100 % 10);
        if (r >= 10) m[k++] = (char)('0' + r / 10 % 10);
        m[k++] = (char)('0' + r % 10);
        m[k] = 0;
        say(p, m);
        p->recap = true;
        return 0;
    }
    if (st >= FF_END)
        return -1;
    return st;
}

/* ---- the player ------------------------------------------------------------------------------------------------ */
void ff_player_init(ff_player *p, float sample_rate)
{
    memset(p, 0, sizeof *p);
    ff_defaults(&p->s);
    p->sr = sample_rate;
    p->cur = -1;
    p->pedal_idx = -1;
}

void ff_set_sound(ff_player *p, void (*fn)(void *ctx, const ff_event *e), void *ctx)
{
    p->sound = fn;
    p->sound_ctx = ctx;
}

void ff_all_off(ff_player *p)
{
    while (p->nheld)
        off_now(p, p->nheld - 1);
}

int ff_bar(const ff_player *p)
{
    int st = p->playing && p->cur >= 0 ? p->cur : p->pos;
    return clampi(st / 16 + 1, 1, FF_BARS);
}

float ff_bpm_now(const ff_player *p) { return (float)p->s.bpm * (float)p->s.speed / 100.0f; }

void ff_play(ff_player *p, bool on)
{
    if (on == p->playing)
        return;
    if (on) {
        if (p->loop) p->pos = (p->s.from - 1) * 16;
        if (p->ended || p->pos >= FF_END) p->pos = 0;
        p->pos = p->pos / 16 * 16;
        p->wait = 0.0f;
        p->waiting = false;
        p->loops = 0;
        p->round = 1;
        p->recap = false;
        p->ended = false;
        p->jump = 0;
        p->cur = -1;
        p->pedal_idx = -1;
        p->count_left = p->s.count && p->s.mode != FF_MODE_TAP ? 4 : 0;
        p->count_wait = 0.0f;
        p->playing = true;
        if (p->s.mode == FF_MODE_TAP) say(p, "TAP ANY KEY");
    } else {
        p->playing = false;
        p->waiting = false;
        p->count_left = 0;
        if (p->cur >= 0) p->pos = p->cur / 16 * 16;
        ff_all_off(p);
    }
}

void ff_set_loop(ff_player *p, bool on) { p->loop = on; }

void ff_goto_bar(ff_player *p, int bar)
{
    bar = clampi(bar, 1, FF_BARS);
    if (p->playing) {
        p->jump = bar;
    } else {
        p->pos = (bar - 1) * 16;
        p->cur = -1;
        p->ended = false;
    }
}

void ff_step_bar(ff_player *p, int d)
{
    int bar = clampi(ff_bar(p) + d, 1, FF_BARS);
    p->pos = (bar - 1) * 16;
    p->jump = 0;
    p->ended = false;
    if (p->playing) {
        ff_all_off(p);
        p->wait = 0.0f;
        p->waiting = false;
        p->cur = p->pos;
    } else {
        p->cur = -1;
    }
}

void ff_changed(ff_player *p) { ff_settings_fix(&p->s); }

/* TAP: on to the next sixteenth with the notes the player waits for, playing the way there */
static void tap(ff_player *p)
{
    int you = you_hand(p);
    for (int guard = 0; guard < 128; ++guard) {
        int st = wrap(p, p->pos);
        if (st < 0) {
            p->playing = false;
            p->ended = true;
            p->pos = 0;
            return;
        }
        ff_shown ev[16];
        int n = step_notes(&p->s, st, ev, 16);
        p->pos = st + 1;
        if (!n) continue;
        play_step(p, st, -1);
        bool wanted = you < 0;
        for (int i = 0; i < n && !wanted; ++i)
            if (ev[i].hand == you) wanted = true;
        if (wanted) return;
    }
}

void ff_key(ff_player *p, int note, bool down)
{
    if (down && p->playing && p->s.mode == FF_MODE_TAP) {
        tap(p);
        return;
    }
    if (down && p->playing && p->s.mode == FF_MODE_WAIT && p->waiting) {
        int pc = note % 12, k = 0, hit = 0;
        float d = step_dur(&p->s, p->want_step);
        for (int i = 0; i < p->nwant; ++i) {
            if (p->want[i] % 12 == pc) {
                ff_shown e = {(uint16_t)p->want_step, 1, p->want[i], (uint8_t)you_hand(p), p->want_var[i]};
                ff_shown src[16];
                int n = step_notes(&p->s, p->want_step, src, 16);
                for (int j = 0; j < n; ++j)
                    if (src[j].note == e.note && src[j].hand == e.hand) e = src[j];
                note_on(p, &e, velocity(&p->s, &e, p->recap), release_after(&p->s, &e, d));
                hit++;
            } else {
                p->want[k] = p->want[i];
                p->want_var[k++] = p->want_var[i];
            }
        }
        p->nwant = k;
        if (hit) {
            if (!p->nwant) {
                p->waiting = false;
                p->pos = p->want_step + 1;
                p->wait = d * p->sr;
            }
            return;
        }
        char m[24] = "NOT ";
        const char *nm = ff_note_name(pc);
        int i = 4;
        for (; *nm; ++nm) m[i++] = *nm;
        m[i] = 0;
        say(p, m);
    }
    /* the keys play along (FREE, or between) */
    if (down) {
        send(p, 1, FF_HAND_KEYS, note, 100);
        ff_seen f = {FF_HAND_KEYS, (uint8_t)note, 100, 0};
        uint8_t w = (uint8_t)((p->feed_w + 1u) % FF_FEED);
        if (w != p->feed_r) {
            p->feed[p->feed_w] = f;
            p->feed_w = w;
        }
    } else {
        send(p, 0, FF_HAND_KEYS, note, 0);
    }
}

void ff_process(ff_player *p, int frames)
{
    uint32_t end = p->clock + (uint32_t)frames;
    for (int i = 0; i < p->nheld;)                     /* the dampers that fall in this block */
        if ((int32_t)(p->held[i].off - end) <= 0) off_now(p, i);
        else ++i;
    if (p->playing && p->s.mode != FF_MODE_TAP) {
        if (p->count_left > 0) {
            p->count_wait -= (float)frames;
            while (p->count_wait <= 0.0f && p->count_left > 0) {
                click(p, p->count_left == 4);
                p->count_left--;
                p->count_wait += 60.0f / ff_bpm_now(p) * p->sr;
            }
            if (!p->count_left)
                p->wait = p->count_wait;
        } else if (!p->waiting) {
            p->wait -= (float)frames;
            int you = you_hand(p);
            while (p->wait <= 0.0f && p->playing && !p->waiting) {
                int st = wrap(p, p->pos);
                if (st < 0) {
                    p->playing = false;
                    p->ended = true;
                    p->pos = 0;
                    break;
                }
                p->pos = st;
                if (p->s.mode == FF_MODE_WAIT && you >= 0 && has_hand(p, st, you)) {
                    play_step(p, st, you);
                    ff_shown ev[16];
                    int n = step_notes(&p->s, st, ev, 16);
                    p->nwant = 0;
                    for (int i = 0; i < n && p->nwant < FF_WANT; ++i)
                        if (ev[i].hand == you) {
                            p->want[p->nwant] = ev[i].note;
                            p->want_var[p->nwant++] = ev[i].var;
                        }
                    p->want_step = st;
                    p->waiting = true;
                    break;
                }
                play_step(p, st, you);
                p->wait += step_dur(&p->s, st) * p->sr;
                p->pos = st + 1;
            }
        }
    }
    p->clock = end;
}

bool ff_feed_pop(ff_player *p, ff_seen *n)
{
    if (p->feed_r == p->feed_w)
        return false;
    *n = p->feed[p->feed_r];
    p->feed_r = (uint8_t)((p->feed_r + 1u) % FF_FEED);
    return true;
}

int ff_reso_notes(const ff_player *p, int *notes)
{
    int k = p->pedal_idx < 0 ? 0 : p->pedal_idx, n = 0;
    for (int i = 0; i < 6; ++i)
        if (FF_RESO[k][i] >= 0)
            notes[n++] = clampi(map_mode(&p->s, FF_RESO[k][i]), 24, 108);
    return n;
}

float ff_reso_amount(const ff_player *p)
{
    static const float AMOUNT[FF_PED_COUNT] = {0.30f, 0.18f, 0.0f, 0.45f};
    return p->playing ? AMOUNT[p->s.pedal % FF_PED_COUNT] : AMOUNT[p->s.pedal % FF_PED_COUNT] * 0.5f;
}

void ff_chord_name(const ff_player *p, int bar, char *out, int max)
{
    int k = 0;
    out[0] = 0;
    if (bar < 1 || bar > FF_BARS || FF_CHORD[bar - 1][0] == 255 || p->s.scale != FF_SCALE_SCORE || max < 16)
        return;
    int root = FF_CHORD[bar - 1][0] + p->s.key, q = FF_CHORD[bar - 1][1], bass = FF_CHORD[bar - 1][2] + p->s.key;
    const char *s = ff_note_name(root), *qn = FF_QUALITY[q];
    while (*s && k < max - 1) out[k++] = *s++;
    if (q < 2 && k < max - 1) out[k++] = ' ';           /* C MAJOR, A MINOR; G7, D9 */
    for (s = qn; *s && k < max - 1; ) out[k++] = *s++;
    if ((((bass - root) % 12) + 12) % 12) {
        const char *b = " / ";
        while (*b && k < max - 1) out[k++] = *b++;
        for (s = ff_note_name(bass); *s && k < max - 1;) out[k++] = *s++;
    }
    out[k] = 0;
}

int ff_next_pcs(const ff_player *p, int st, int *pcs, int max)
{
    int you = you_hand(p), n = 0;
    if (you < 0) return 0;
    for (; st < FF_END && n < max; ++st) {
        ff_shown ev[16];
        int k = step_notes(&p->s, st, ev, 16);
        for (int i = 0; i < k; ++i)
            if (ev[i].hand == you) {
                pcs[n++] = ev[i].note % 12;
                break;
            }
    }
    return n;
}
