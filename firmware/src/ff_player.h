/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH player: Chopin's Étude Op. 10 No. 1 as a performance. It walks the notes (ff_etude.inc) sixteenth by
 * sixteenth and hands each note-on and note-off to the sound (Felucca's tracks on the FM-1, a renderer on the
 * host), shaped by what the player set:
 *
 *   PERFORM     RUBATO (the pulse breathing with each two-bar wave), TOUCH (how far the dynamics move), ACCENT
 *               (Chopin's accents on each group's first note, each wave's crest), PEDAL (SCORE: a new pedal where
 *               the bass changes, as the score's Ped. marks; HALF; DRY; WASH: four bars a pedal)
 *   PRACTICE    SPEED, a loop FROM..TO (REC), YOU (the hand left out for the player), MODE (FREE: the keys play
 *               along; TAP: each key press plays the next note, the player is the rhythm; WAIT: the music waits
 *               for the right note, any octave), RAMP (each loop a little faster), CLICK, COUNT (a bar's count-in)
 *   VARIATIONS  VARY (0: the score note for note), FIGURE, SPREAD, SEED; KEY, MODE (minor, Dorian, Lydian), BASS,
 *               FORM (ONCE, REPEAT, EVOLVE: a new variation every round)
 *
 * Dynamics, rubato and pedalling are Flyfish's own reading of the score's markings (f, cresc., dim., the
 * hairpins, the accents, Ped.): none of it comes from a recording. The variations take each half bar's own
 * harmony and give the right hand a new figure on its rhythm; the same seed gives the same variation (the
 * random numbers are mulberry32, as the browser simulator's, so both play the same notes).
 *
 * No libm, no doubles: floats on the FPU, and the five functions of tuna_libm.c at most. */
#ifndef FF_PLAYER_H
#define FF_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

enum { FF_HAND_R, FF_HAND_L, FF_HAND_KEYS, FF_HAND_CLICK };
enum { FF_PED_SCORE, FF_PED_HALF, FF_PED_DRY, FF_PED_WASH, FF_PED_COUNT };
enum { FF_YOU_NONE, FF_YOU_RIGHT, FF_YOU_LEFT, FF_YOU_COUNT };
enum { FF_MODE_FREE, FF_MODE_TAP, FF_MODE_WAIT, FF_MODE_COUNT };
enum { FF_FIG_RISE, FF_FIG_WAVE, FF_FIG_ALBERTI, FF_FIG_BROKEN, FF_FIG_TREMOLO, FF_FIG_SPARKLE, FF_FIG_COUNT };
enum { FF_SCALE_SCORE, FF_SCALE_MINOR, FF_SCALE_DORIAN, FF_SCALE_LYDIAN, FF_SCALE_COUNT };
enum { FF_BASS_OCTAVE, FF_BASS_SINGLE, FF_BASS_DRONE, FF_BASS_NONE, FF_BASS_COUNT };
enum { FF_FORM_ONCE, FF_FORM_REPEAT, FF_FORM_EVOLVE, FF_FORM_COUNT };
enum { FF_SPACE_ROOM, FF_SPACE_HALL, FF_SPACE_CATHEDRAL, FF_SPACE_COUNT };
#define FF_SOUNDS 8

/* what the player set: the cards (and the knobs above them). Saved as it is (ff_fw.c), so append only */
typedef struct {
    uint8_t rubato, touch, accent, pedal;          /* HOME */
    uint8_t sound, bright, decay, width;           /* SOUND */
    uint8_t speed, from, to, you;                  /* PRACTICE: speed 25..100 %, bars 1..79 */
    uint8_t mode, ramp, click, count;              /* PRACTICE 2 */
    uint8_t vary, figure, spread, seed;            /* VARIATIONS */
    int8_t key;                                    /* VARIATIONS 2: -6..6 */
    uint8_t scale, bass, form;
    uint8_t right, left, reverb, space;            /* MIX */
    uint8_t bpm;                                   /* SELECT: 40..240 */
    uint8_t palette, style;                        /* MENU */
} ff_settings;

typedef struct {
    uint8_t on, hand, note, vel;                   /* vel 1..127 (on) */
} ff_event;

/* what the screen hears: each note-on (the droplets), the variation's own marked */
typedef struct {
    uint8_t hand, note, vel, var;
} ff_seen;

#define FF_HELD 48                                 /* notes sounding, waiting for their note-off */
#define FF_FEED 32
#define FF_WANT 4

typedef struct {
    ff_settings s;
    float sr;                                      /* the rate ff_process counts in */
    bool playing, loop;
    int pos;                                       /* the next sixteenth to play */
    int cur;                                       /* the sixteenth sounding (-1: none yet) */
    float wait;                                    /* frames until the next sixteenth */
    uint32_t clock;                                /* frames since start */
    int jump;                                      /* a bar asked for while playing (1..79; 0 none): at the next bar line */
    int loops;
    bool ended;                                    /* FORM ONCE: the last bar played out */
    /* WAIT: the notes of the player's hand at step want_step, still to find */
    bool waiting;
    int want_step;
    uint8_t want[FF_WANT];                         /* MIDI notes (as played: transposed) */
    uint8_t want_var[FF_WANT];
    int nwant;
    /* the count-in */
    int count_left;
    float count_wait;
    /* the notes sounding: their note-off when the clock reaches off */
    struct { uint8_t note, hand; uint32_t off; } held[FF_HELD];
    int nheld;
    /* the pedal for the resonator: FF_PEDAL index sounding, -1 up */
    int pedal_idx;
    bool pedal_down;
    uint32_t pedal_sig;                            /* bumped at every change: the host retunes the strings */
    ff_seen feed[FF_FEED];
    uint8_t feed_w, feed_r;
    char msg[24];                                  /* a word for the screen ("NOT F", "SPEED 70%"); the UI clears it */
    uint8_t msg_new;
    void (*sound)(void *ctx, const ff_event *e);
    void *sound_ctx;
} ff_player;

void ff_defaults(ff_settings *s);
void ff_settings_fix(ff_settings *s);              /* every field into its range (a loaded record) */
void ff_player_init(ff_player *p, float sample_rate);
void ff_set_sound(ff_player *p, void (*fn)(void *ctx, const ff_event *e), void *ctx);

void ff_play(ff_player *p, bool on);               /* from the loop's FROM with LOOP on, else the bar it stopped in */
void ff_set_loop(ff_player *p, bool on);
void ff_goto_bar(ff_player *p, int bar);           /* stopped: at once; playing: at the next bar line */
void ff_step_bar(ff_player *p, int d);             /* OCT-/OCT+: one bar back or on, at once */
void ff_changed(ff_player *p);                     /* a setting changed (the variations, the key): the notes follow */
void ff_key(ff_player *p, int note, bool down);    /* the FM-1's keys: FREE plays, TAP advances, WAIT checks */
void ff_process(ff_player *p, int frames);         /* the audio: notes in time; nothing while stopped */
void ff_all_off(ff_player *p);

int ff_bar(const ff_player *p);                    /* 1..79: the bar sounding (stopped: the one PLAY starts) */
float ff_bpm_now(const ff_player *p);              /* the tempo with SPEED */
bool ff_feed_pop(ff_player *p, ff_seen *n);
/* the notes as they play now (the variation, the key, the mode, the bass): for the screen's roll. Fills up to max
 * notes starting at sixteenth s0, before s1; returns how many. var: 1 a variation's note */
typedef struct { uint16_t step; uint8_t len, note, hand, var; } ff_shown;
int ff_notes_in(const ff_player *p, int s0, int s1, ff_shown *out, int max);
/* the score's own notes (no variation; the key followed), for the roll's faint copy under a variation */
int ff_score_in(const ff_player *p, int s0, int s1, ff_shown *out, int max);
/* the strings the resonator rings for the pedal down now (MIDI, -1 off); amount 0..1 for the pedal mode */
int ff_reso_notes(const ff_player *p, int *notes);
float ff_reso_amount(const ff_player *p);
/* the bar's chord as the screen names it ("" when its notes make none cleanly); the key and mode followed */
void ff_chord_name(const ff_player *p, int bar, char *out, int max);
/* the next notes the player's hand plays from sixteenth s (WAIT's preview): pitch classes, up to max */
int ff_next_pcs(const ff_player *p, int s, int *pcs, int max);
const char *ff_note_name(int pc);
const char *ff_sound_name(int i);                 /* the SOUND card's names (ff_sounds.h: what they are) */

#endif
