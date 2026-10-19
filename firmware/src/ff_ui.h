/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH screen and controls: Flyfish's pages on Felucca's drawing (gfx.c: canvas, fonts, palettes, icons), in
 * Tuna's frame: the header, four knob cards, the panel, the footer. Compiled after gfx.c in one unit, as Felucca's
 * ui*.c are (the firmware's unity build, or a host test).
 *
 * The controls
 *   SELECT     tempo (the score: 176)                MASTER     volume (Felucca's)
 *   PRESETS    the sound, on any page (the list shows while turning)
 *   ALGORITHM  a bar to go to (playing: at the next bar line)
 *   KNOB 1..4  the four cards of the page
 *   PLAY       start / stop: from FROM with the loop on, else from the bar it stopped in
 *   REC        the loop FROM .. TO on and off
 *   OCT- OCT+  a bar back / on, at once
 *   keys       music only: play along (FREE), the next note (TAP), the note the music waits for (WAIT)
 *   HOME       the waterfall; held: the menu (colour, style)
 *   EDIT       the sound              GLO   the mix and the space
 *   SEQ        practice, again: practice 2 (MODE RAMP CLICK COUNT)
 *   ARP        variations, again: variations 2 (KEY MODE BASS ROUNDS: 1 .. 16 or INF, the seam at bar 48)
 *   FX         fingers: LEGATO (how long each note of the figures overlaps the next)
 *   SCL ENV LFO SAVE: not used (the settings save themselves while stopped) */
#ifndef FF_UI_H
#define FF_UI_H
#ifndef FF_PALETTE_DEFAULT
#define FF_PALETTE_DEFAULT 3u              /* ICE (ui_palettes.h) */
#endif

#include <stdbool.h>
#include <stdint.h>
#include "ff_player.h"

enum { FU_K_MASTER, FU_K_SELECT, FU_K_PRESETS, FU_K_ALGO, FU_K1, FU_K2, FU_K3, FU_K4, FU_KNOBS };
enum {
    FU_B_FX, FU_B_SCL, FU_B_ENV, FU_B_LFO, FU_B_EDIT, FU_B_GLO,
    FU_B_HOME, FU_B_SAVE, FU_B_ARP, FU_B_SEQ, FU_B_PLAY, FU_B_REC,
    FU_B_OCTDN, FU_B_OCTUP, FU_BUTTONS
};
enum { FU_PG_HOME, FU_PG_SOUND, FU_PG_PRACTICE, FU_PG_PRACTICE2, FU_PG_VAR, FU_PG_VAR2, FU_PG_MIX, FU_PG_MENU, FU_PG_FINGERS, FU_PAGES };
#define FU_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define FU_KEY_HI 79                      /* G5 */
#define FU_DROPS 64
#define FU_RIPPLES 8

typedef struct {
    int16_t x, y, vy;                     /* 1/16 px */
    uint8_t hand, size, life, var;
} fu_drop;

typedef struct { int16_t x; int16_t age; } fu_ripple;   /* age ms, -1 free */

typedef struct {
    ff_player *pl;
    int page;
    int hot, hot_ms;                      /* the knob just turned (its card in the accent); 9: SELECT */
    int browse_ms;                        /* the sound list over the panel (PRESETS) */
    char msg[32];
    int msg_ms;
    int home_ms;
    bool home_down;
    uint32_t keys_down;                   /* bit (note - FU_KEY_LO) */
    fu_drop dr[FU_DROPS];
    fu_ripple rp[FU_RIPPLES];
    uint32_t rnd;
    uint32_t frame;
    int batt;                             /* 0..3 bars, 4 = USB power */
    bool changed;                         /* a setting changed: the glue applies it (sound, mix) and saves it later */
    uint32_t sig_head, sig_card[4], sig_foot;
    bool force;
} ff_ui;

void fu_init(ff_ui *u, ff_player *p);
void fu_knob(ff_ui *u, int knob, int delta);
void fu_button(ff_ui *u, int button, bool down);
void fu_key(ff_ui *u, int note, bool down);
void fu_tick(ff_ui *u, int ms);           /* timers, the droplets; every frame */
void fu_draw(ff_ui *u);                   /* what changed; the panel every frame */
void fu_draw_all(ff_ui *u);
const char *fu_page_name(int page);

#endif
