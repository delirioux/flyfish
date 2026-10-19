/* SPDX-License-Identifier: GPL-3.0-only */
/* FLYFISH screen and controls (ff_ui.h): Flyfish's pages in Tuna's frame, drawn with Felucca's gfx.c. Included after
 * gfx.c (the firmware's felucca.c unit through ff_fw.c, or tests/ff_ui_test.c). */
#include "ff_ui.h"
#include "ff_version.h"
#include "ui_icons.h"

#define FU_HEAD 24
#define FU_CARD_Y 28
#define FU_CARD_W 57
#define FU_CARD_H 44
#define FU_CARD_X(c) (3 + 59 * (int32_t)(c))
#define FU_PANEL_Y 76
#define FU_PANEL_H 122
#define FU_FOOT_Y 202
#define FU_FOOT_H 38
#define FU_MENU_HOLD_MS 600
#define FU_LO 24                                   /* the panel's pitch range: C1 .. E7 */
#define FU_HI 100

static int fu_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int fu_wrap(int v, int n) { return n > 0 ? ((v % n) + n) % n : 0; }
static void fu_cpy(char *d, const char *s, unsigned n)
{
    unsigned i = 0;
    for (; s[i] && i + 1 < n; ++i) d[i] = s[i];
    d[i] = 0;
}
static void fu_cat(char *d, const char *s, unsigned n)
{
    unsigned i = 0;
    while (d[i] && i + 1 < n) ++i;
    for (unsigned k = 0; s[k] && i + 1 < n; ++k) d[i++] = s[k];
    d[i] = 0;
}
static void fu_fmt_int(char *d, int v)
{
    char t[12];
    int n = 0, neg = v < 0;
    unsigned u = (unsigned)(neg ? -v : v);
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    int k = 0;
    if (neg) d[k++] = '-';
    while (n) d[k++] = t[--n];
    d[k] = 0;
}
static uint32_t fu_hash(uint32_t h, const char *s)
{
    for (; *s; ++s) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

/* ---- icons (the Fukiai cells of ui_icons.h, as Tuna draws them) ------------------------------------------- */
static int32_t fu_icon_cell(uint32_t *size, uint32_t id)
{
    const uint8_t *idx = *size == 24u ? AI24_IDX : *size == 16u ? AI16_IDX : AI12_IDX;
    if (id >= ICON_COUNT)
        return -1;
    if (idx[id] == 0xFFu) {
        if (*size == 12u || AI12_IDX[id] == 0xFFu)
            return -1;
        *size = 12u;
        idx = AI12_IDX;
    }
    return idx[id];
}
static int32_t fu_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    uint32_t sz = size;
    int32_t k = fu_icon_cell(&sz, id), y0, y1;
    if (k < 0)
        return 0;
    if (sz == 24u) {
        uint32_t v = AI24_INK[k];
        y0 = (int32_t)(v >> 8 & 255u);
        y1 = (int32_t)(v >> 24 & 255u);
    } else {
        uint32_t v = sz == 16u ? AI16_INK[k] : AI12_INK[k];
        y0 = (int32_t)(v >> 4 & 15u);
        y1 = (int32_t)(v >> 12 & 15u) + 1;
    }
    int32_t y = cy - HALF_UP(y1 - y0) - y0;
    cv_alpha_hc(x, y, sz, sz, sz == 24u ? AI24_DATA + AI24_OFF[k] : sz == 16u ? AI16_DATA + AI16_OFF[k] :
                AI12_DATA + AI12_OFF[k], AI_HC, ramp(fg, bg));
    GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)sz, y + cv_oy + (int32_t)sz, "icon", 4u);
    return (int32_t)sz;
}

/* ---- the cards ------------------------------------------------------------------------------------------- */
enum { FC_NUM, FC_LIST, FC_PCT, FC_BAR, FC_OCT, FC_SEED, FC_KEY, FC_SND, FC_PAL, FC_ROUNDS, FC_OFF };
typedef struct {
    const char *label;
    uint8_t off;                                   /* offsetof(ff_settings, the field) */
    uint8_t kind;
    int16_t lo, hi;
    uint8_t step;
    const char *const *names;
} fu_cdef;
#define FS_(f) ((uint8_t)__builtin_offsetof(ff_settings, f))
static const char *const FN_PEDAL[] = {"SCORE", "HALF", "DRY", "WASH"};
static const char *const FN_YOU[] = {"NONE", "RIGHT", "LEFT"};
static const char *const FN_MODE[] = {"FREE", "TAP", "WAIT"};
static const char *const FN_RAMP[] = {"OFF", "+2%", "+5%", "+10%"};
static const char *const FN_ONOFF[] = {"OFF", "ON"};
static const char *const FN_COUNT[] = {"OFF", "1 BAR"};
static const char *const FN_FIGURE[] = {"RISE", "WAVE", "ALBERTI", "BROKEN", "TREMOLO", "SPARKLE"};
static const char *const FN_SCALE[] = {"SCORE", "MINOR", "DORIAN", "LYDIAN"};
static const char *const FN_BASS[] = {"OCTAVE", "SINGLE", "DRONE", "NONE"};
static const char *const FN_SPACE[] = {"ROOM", "HALL", "CHURCH"};           /* (the card: CATHEDRAL does not fit, as Tuna) */
static const char *const FN_SPACE_LONG[] = {"ROOM", "HALL", "CATHEDRAL"};   /* (the MIX panel) */
static const char *const FN_STYLE[] = {"FLAT", "LINE"};
#define C_NUM(l, f) {l, FS_(f), FC_NUM, 0, 100, 5, 0}
#define C_LIST(l, f, n) {l, FS_(f), FC_LIST, 0, (int16_t)(sizeof n / sizeof n[0]) - 1, 1, n}
#define C_OFF {"", 0, FC_OFF, 0, 0, 0, 0}
static const fu_cdef FU_CARDS[FU_PAGES][4] = {
    [FU_PG_HOME] = {C_NUM("RUBATO", rubato), C_NUM("TOUCH", touch), C_NUM("ACCENT", accent), C_LIST("PEDAL", pedal, FN_PEDAL)},
    [FU_PG_SOUND] = {{"SOUND", FS_(sound), FC_SND, 0, FF_SOUNDS - 1, 1, 0}, C_NUM("BRIGHT", bright), C_NUM("DECAY", decay),
                     C_NUM("WIDTH", width)},
    [FU_PG_PRACTICE] = {{"SPEED", FS_(speed), FC_PCT, 25, 100, 5, 0}, {"FROM", FS_(from), FC_BAR, 1, 79, 1, 0},
                        {"TO", FS_(to), FC_BAR, 1, 79, 1, 0}, C_LIST("YOU", you, FN_YOU)},
    [FU_PG_PRACTICE2] = {C_LIST("MODE", mode, FN_MODE), C_LIST("RAMP", ramp, FN_RAMP), C_LIST("CLICK", click, FN_ONOFF),
                         C_LIST("COUNT", count, FN_COUNT)},
    [FU_PG_VAR] = {C_NUM("VARY", vary), C_LIST("FIGURE", figure, FN_FIGURE), {"SPREAD", FS_(spread), FC_OCT, 1, 4, 1, 0},
                   {"SEED", FS_(seed), FC_SEED, 0, 255, 1, 0}},
    [FU_PG_VAR2] = {{"KEY", FS_(key), FC_KEY, -6, 6, 1, 0}, C_LIST("MODE", scale, FN_SCALE), C_LIST("BASS", bass, FN_BASS),
                    {"ROUNDS", FS_(rounds), FC_ROUNDS, 0, FF_ROUNDS_MAX, 1, 0}},
    [FU_PG_MIX] = {C_NUM("RIGHT", right), C_NUM("LEFT", left), C_NUM("REVERB", reverb), C_LIST("SPACE", space, FN_SPACE)},
    [FU_PG_FINGERS] = {C_NUM("LEGATO", legato), C_OFF, C_OFF, C_OFF},
    [FU_PG_MENU] = {{"COLOR", FS_(palette), FC_PAL, 0, 9, 1, 0}, C_LIST("STYLE", style, FN_STYLE), C_OFF, C_OFF},
};
static const char *const FU_PAGE_NAMES[FU_PAGES] = {"HOME", "SOUND", "PRACTICE", "PRACTICE 2", "VARIATIONS",
                                                    "VARIATIONS 2", "MIX", "MENU", "FINGERS"};
const char *fu_page_name(int page) { return page >= 0 && page < FU_PAGES ? FU_PAGE_NAMES[page] : ""; }

static int fu_get(const ff_ui *u, const fu_cdef *c)
{
    const uint8_t *b = (const uint8_t *)&u->pl->s + c->off;
    return c->kind == FC_KEY ? (int)*(const int8_t *)b : (int)*b;
}
static void fu_set(ff_ui *u, const fu_cdef *c, int v)
{
    uint8_t *b = (uint8_t *)&u->pl->s + c->off;
    if (c->kind == FC_KEY) *(int8_t *)b = (int8_t)v;
    else *b = (uint8_t)v;
}
static void fu_card_value(const ff_ui *u, const fu_cdef *c, char *t, unsigned n)
{
    int v = fu_get(u, c);
    t[0] = 0;
    switch (c->kind) {
    case FC_LIST: fu_cpy(t, c->names[fu_clampi(v, c->lo, c->hi)], n); break;
    case FC_PCT: fu_fmt_int(t, v); fu_cat(t, "%", n); break;
    case FC_OCT: fu_fmt_int(t, v); fu_cat(t, " OCT", n); break;
    case FC_KEY: fu_cpy(t, ff_note_name(v), n); break;       /* the étude's C moved: its new tonic */
    case FC_SND: fu_cpy(t, ff_sound_name(v), n); break;
    case FC_PAL: fu_cpy(t, UI_PALETTES[fu_wrap(v, (int)NPALETTES)].name, n); break;
    case FC_ROUNDS: if (v) fu_fmt_int(t, v); else fu_cpy(t, "INF", n); break;   /* (0 after 16: INF) */
    case FC_OFF: break;
    default: fu_fmt_int(t, v); break;
    }
}

static void fu_draw_card(ff_ui *u, int i, const fu_cdef *c)
{
    bool hot = u->hot == i && u->hot_ms > 0;
    uint16_t lc = hot ? T_ACCENT : T_MID, vc = hot ? T_ACCENT : T_THEME;
    char val[24];
    fu_card_value(u, c, val, sizeof val);
    int ratio = -1;
    if (c->kind != FC_OFF && (c->kind != FC_LIST || c->off == FS_(mode) || c->off == FS_(ramp)) && c->hi > c->lo)
        ratio = c->kind == FC_ROUNDS && !fu_get(u, c) ? 1000 : (fu_get(u, c) - c->lo) * 1000 / (c->hi - c->lo);
    uint32_t sig = fu_hash(2166136261u + (uint32_t)i * 7u + ux.gen * 131u + (uint32_t)hot * 3u + (uint32_t)u->page * 977u,
                           c->label);
    sig = fu_hash(sig, val) + (uint32_t)(ratio + 1) * 31u;
    if (!u->force && sig == u->sig_card[i])
        return;
    u->sig_card[i] = sig;
    cv_begin(FU_CARD_W, FU_CARD_H, T_BG);
    cv_rrect(0, 0, FU_CARD_W, FU_CARD_H, 4, T_SURF, T_BG);
    if (c->kind == FC_OFF) {
        cv_rect(FU_CARD_W / 2 - 6, FU_CARD_H / 2, 12, 1, T_DIM);
    } else {
        cv_text_on(5, 3, &AF_S, c->label, lc, T_SURF);
        const aafont_t *vf = text_w(&AF_M, val) <= FU_CARD_W - 8 ? &AF_M : &AF_S;
        cv_text_fit(5, vf == &AF_M ? 17 : 19, vf, val, vc, T_SURF, FU_CARD_W - 8);
        if (ratio >= 0) {
            int32_t w = FU_CARD_W - 10, x0 = 5, y = 37, r = fu_clampi(ratio, 0, 1000) * w / 1000;
            cv_rect(x0, y, w, 3, T_LINE);
            if (c->kind == FC_KEY) {
                int32_t mid = x0 + w / 2;
                if (r >= w / 2) cv_rect(mid, y, r - w / 2 + 1, 3, vc);
                else cv_rect(x0 + r, y, mid - x0 - r + 1, 3, vc);
                cv_rect(mid, y - 2, 1, 7, hot ? T_ACCENT : T_MID);   /* the score's key */
            } else {
                cv_rect(x0, y, r > 0 ? r : 1, 3, vc);
            }
        }
    }
    cv_blit((uint32_t)FU_CARD_X(i), FU_CARD_Y);
}

/* ---- the header -------------------------------------------------------------------------------------------- */
static void fu_head_msg(const ff_ui *u, char *t, unsigned n)
{
    const ff_player *p = u->pl;
    t[0] = 0;
    if (u->msg_ms > 0) {
        fu_cpy(t, u->msg, n);
    } else if (p->playing && p->waiting && p->nwant) {
        fu_cpy(t, "WAIT: ", n);
        for (int i = 0; i < p->nwant; ++i) {
            if (i) fu_cat(t, " ", n);
            fu_cat(t, ff_note_name(p->want[i]), n);
        }
    } else if (p->playing && p->count_left > 0) {
        fu_cpy(t, "COUNT IN", n);
    } else if (p->loop) {
        char a[8];
        fu_cpy(t, "LOOP ", n);
        fu_fmt_int(a, p->s.from);
        fu_cat(t, a, n);
        fu_cat(t, "-", n);
        fu_fmt_int(a, p->s.to);
        fu_cat(t, a, n);
    }
}

static void fu_draw_head(ff_ui *u)
{
    const ff_player *p = u->pl;
    char b[16], m[40];
    int bpm = (int)(ff_bpm_now(p) + 0.5f);
    bool hot = u->hot == 9 && u->hot_ms > 0;
    fu_head_msg(u, m, sizeof m);
    uint32_t sig = fu_hash((uint32_t)p->playing * 3u + (uint32_t)p->loop * 7u + (uint32_t)bpm * 101u + ux.gen * 13u +
                           (uint32_t)u->batt * 7777u + (uint32_t)hot * 23u, m);
    if (!u->force && sig == u->sig_head)
        return;
    u->sig_head = sig;
    cv_begin(240, FU_HEAD, T_BG);
    fu_icon_mid(8, FU_HEAD / 2, 16, p->playing ? ICON_X_PLAY : ICON_X_STOP, p->playing ? T_THEME : T_MID, T_BG);
    if (p->loop)
        fu_icon_mid(28, FU_HEAD / 2, 16, ICON_X_REPEAT, T_REC, T_BG);
    fu_icon_mid(54, FU_HEAD / 2, 16, ICON_TEMPO, T_MID, T_BG);
    fu_fmt_int(b, bpm);
    cv_text_on(72, CAP_IN(M, FU_HEAD), &AF_M, b, hot ? T_ACCENT : T_THEME, T_BG);
    if (m[0])
        cv_free_text(106, CAP_IN(S, FU_HEAD), &AF_S, m, T_TEXT, T_BG, 186 - 106);
    {
        static const uint8_t BAT[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
        int lv = fu_clampi(u->batt, 0, 4);
        fu_icon_mid(214, FU_HEAD / 2, 24, BAT[lv], lv == 1 ? T_ACCENT : lv == 0 ? T_MID : T_THEME, T_BG);
    }
    cv_blit(0, 0);
}

/* ---- the footer: the piece as 16 dashes, WATERFALL, the bar (on the variation pages: the variation) ---------- */
static void fu_draw_foot(ff_ui *u)
{
    const ff_player *p = u->pl;
    int bar = ff_bar(p), filled = (bar - 1) * 16 / 79;
    char small[16], a[8];
    if (u->page == FU_PG_VAR || u->page == FU_PG_VAR2) {
        if (p->s.vary) {
            fu_cpy(small, "VAR ", sizeof small);
            fu_fmt_int(a, p->s.seed);
            fu_cat(small, a, sizeof small);
        } else {
            fu_cpy(small, "SCORE", sizeof small);
        }
    } else {
        fu_cpy(small, "BAR ", sizeof small);
        fu_fmt_int(a, bar);
        fu_cat(small, a, sizeof small);
    }
    uint32_t sig = fu_hash(fu_hash((uint32_t)filled * 7u + (uint32_t)p->playing * 3u + ux.gen * 11u + (uint32_t)u->page * 13u,
                                   small), fu_page_name(u->page));
    if (!u->force && sig == u->sig_foot)
        return;
    u->sig_foot = sig;
    cv_begin(240, FU_FOOT_H, T_BG);
    for (int i = 0; i < 16; ++i)
        cv_rect(10 + i * 14, 6, 10, 2, i < filled ? T_THEME : i == filled && p->playing ? T_ACCENT : T_LINE);
    cv_free_text(10, 17, &AF_M, "WATERFALL", T_THEME, T_BG, 130);
    int32_t nx = 10 + text_w(&AF_M, "WATERFALL");
    int32_t yb = 17 + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H;
    int32_t px = 230 - text_w(&AF_S, fu_page_name(u->page)) - 8;
    if (px - nx - 6 > 24)
        cv_free_text(nx + 6, yb, &AF_S, small, T_MID, T_BG, px - nx - 6);
    cv_text_r(230, yb, &AF_S, fu_page_name(u->page), T_MID, T_BG);
    cv_blit(0, FU_FOOT_Y);
}

/* ---- the panels -------------------------------------------------------------------------------------------- */
static void fu_panel_begin(void)
{
    cv_begin(240, FU_PANEL_H, T_BG);
    cv_rrect(3, 0, 234, FU_PANEL_H, 4, T_SURF, T_BG);
}
static int32_t fu_x_of(int note) { return 8 + (fu_clampi(note, FU_LO, FU_HI) - FU_LO) * 222 / (FU_HI - FU_LO); }

static bool fu_drops_alive(const ff_ui *u)
{
    for (int i = 0; i < FU_DROPS; ++i) if (u->dr[i].life) return true;
    for (int i = 0; i < FU_RIPPLES; ++i) if (u->rp[i].age >= 0) return true;
    return false;
}

/* HOME: each note a droplet falling from where it sounds (across: its pitch), the bass a ripple on the pool */
#define FU_LEDGE 22
#define FU_POOL (FU_PANEL_H - 12)
static void fu_panel_waterfall(ff_ui *u)
{
    const ff_player *p = u->pl;
    cv_rect(8, FU_LEDGE - 2, 224, 1, T_LINE);                /* the ledge the notes fall from */
    for (int i = 0; i < FU_RIPPLES; ++i) {
        const fu_ripple *r = &u->rp[i];
        if (r->age < 0) continue;
        int32_t rad = 4 + r->age / 40;
        uint16_t c = ux_mix(T_SURF, T_TEXT, fu_clampi(80 - r->age / 30, 0, 80));
        cv_rect(r->x - rad, FU_POOL + 4, rad * 2, 1, c);
        cv_rect(r->x - rad / 2, FU_POOL + 6, rad, 1, ux_mix(T_SURF, c, 55));
    }
    for (int i = 0; i < FU_DROPS; ++i) {
        const fu_drop *d = &u->dr[i];
        if (!d->life) continue;
        uint16_t base = d->hand == FF_HAND_KEYS || d->var ? T_ACCENT : d->hand == FF_HAND_R ? T_THEME : T_TEXT;
        uint16_t c = d->life < 64 ? ux_mix(T_SURF, base, d->life * 100 / 64) : base;
        int32_t x = d->x >> 4, y = d->y >> 4, s = d->size;
        if (d->vy > 0 && d->life >= 64) {                     /* a trail while falling, longer the faster */
            int32_t tl = 2 + d->vy / 5;
            cv_rect(x + s / 2, y - tl, 1, tl / 2, ux_mix(T_SURF, c, 25));
            cv_rect(x + s / 2, y - tl / 2, 1, tl / 2, ux_mix(T_SURF, c, 55));
        }
        cv_rrect(x, y, s, s, s > 3 ? 2 : 1, d->life > 250 ? T_ACCENT : c, T_SURF);
        if (d->vy == 0 && d->life >= 90) {                    /* landed: a splash */
            cv_rect(x - 3, FU_POOL + 1, 2, 1, ux_mix(T_SURF, c, 60));
            cv_rect(x + s + 1, FU_POOL + 1, 2, 1, ux_mix(T_SURF, c, 60));
        }
    }
    cv_rect(8, FU_POOL + 8, 224, 1, T_LINE);
    char ch[24];
    if (p->playing) {
        ff_chord_name(p, ff_bar(p), ch, sizeof ch);
        if (ch[0])
            cv_text_on(10, 4, &AF_S, ch, T_TEXT, T_SURF);
    }
    cv_text_r(230, 4, &AF_S, "OP. 10 NO. 1", T_MID, T_SURF);
}

static void fu_panel_stopped(ff_ui *u)
{
    const ff_player *p = u->pl;
    char line[40], a[8];
    GFX_HOOK_ALIGN(3, 0, 237, FU_PANEL_H, AL_H, "home: press play");
    int32_t w = kc_w(KC_PLAY) + 5 + text_w(&AF_S, "TO START");
    int32_t x = 3 + HALF_UP(234 - w), y = FU_PANEL_H / 2 - KC_H / 2 - 8;
    cv_keycap(x, y, KC_PLAY, T_KEY, T_INK, T_SURF);
    cv_text_on(x + kc_w(KC_PLAY) + 5, y + CAP_IN(S, KC_H), &AF_S, "TO START", T_MID, T_SURF);
    fu_cpy(line, "FROM BAR ", sizeof line);
    fu_fmt_int(a, p->loop ? p->s.from : ff_bar(p));
    fu_cat(line, a, sizeof line);
    fu_cat(line, "   ALLEGRO ", sizeof line);
    fu_fmt_int(a, p->s.bpm);
    fu_cat(line, a, sizeof line);
    int32_t lw = text_w(&AF_S, line);
    cv_text_on(3 + HALF_UP(234 - lw), FU_PANEL_H / 2 + 14, &AF_S, line, T_DIM, T_SURF);
}

/* a list: SOUND (the PRESETS knob too) and MENU */
static void fu_panel_list(const char *title, const char *const *items, int n, int sel, bool count)
{
    char t[16], a[8];
    cv_text_on(10, 4, &AF_S, title, T_THEME, T_SURF);
    if (count) {
        fu_fmt_int(t, sel + 1);
        fu_cat(t, "/", sizeof t);
        fu_fmt_int(a, n);
        fu_cat(t, a, sizeof t);
        cv_text_r(230, 4, &AF_S, t, T_MID, T_SURF);
    }
    const int rows = 5, rh = 20, y0 = 20;
    int top = fu_clampi(sel - rows / 2, 0, n > rows ? n - rows : 0);
    for (int r = 0; r < rows && top + r < n; ++r) {
        int it = top + r;
        int32_t y = y0 + r * rh;
        bool on = it == sel;
        uint16_t bg = on ? T_SEL : T_SURF, fg = on ? T_INK : T_TEXT;
        if (on) {
            cv_rrect(6, y, 228, rh - 2, 3, T_SEL, T_SURF);
            fu_icon_mid(10, y + (rh - 2) / 2, 16, ICON_X_PLAY, T_INK, bg);
        }
        cv_free_text(30, y + CAP_IN(M, rh - 2), &AF_M, items[it], fg, bg, 190);
    }
}

/* the roll: four bars across, pitch up; the right hand THEME, the left TEXT, a variation's notes ACCENT over the
 * score's faint; the hand the player plays hollow; the loop shaded; the playhead ACCENT */
#define FU_ROLL_MAX 200
static void fu_panel_roll(ff_ui *u, bool var)
{
    const ff_player *p = u->pl;
    static ff_shown ev[FU_ROLL_MAX];
    int bar = ff_bar(p), b0;
    if (p->loop && p->s.to - p->s.from < 4) b0 = p->s.from;
    else b0 = bar - 1;
    b0 = fu_clampi(b0, 1, 76);
    const int nb = 4, steps = nb * 16, x0 = 10, w = 220, top = 4, h = FU_PANEL_H - 8;
    for (int b = 0; b < nb; ++b) {
        int32_t bx = x0 + b * w / nb;
        bool in = p->loop && b0 + b >= p->s.from && b0 + b <= p->s.to;
        uint16_t bg = in ? T_TINT : T_SURF;
        char t[8];
        if (in) cv_rect(bx, top, w / nb, h, T_TINT);
        cv_rect(bx, top, 1, h, T_LINE);
        fu_fmt_int(t, b0 + b);
        cv_text_on(bx + 3, top + 2, &AF_S, t, in ? T_THEME : T_DIM, bg);
    }
    int32_t pt = top + 16, ph = h - 18, s0 = (b0 - 1) * 16;
#define FU_Y(n) (pt + ph - 2 - (fu_clampi((n), FU_LO, FU_HI) - FU_LO) * (ph - 3) / (FU_HI - FU_LO))
    if (var) {
        int n = ff_score_in(p, s0, s0 + steps, ev, FU_ROLL_MAX);
        for (int i = 0; i < n; ++i)
            if (ev[i].hand == 0)
                cv_rect(x0 + (ev[i].step - s0) * w / steps, FU_Y(ev[i].note) - 1, 2, 2, ux_mix(T_SURF, T_THEME, 28));
    }
    int you = p->s.you == FF_YOU_NONE ? -1 : p->s.you - 1;
    int n = ff_notes_in(p, s0, s0 + steps, ev, FU_ROLL_MAX);
    for (int i = 0; i < n; ++i) {
        int32_t x = x0 + (ev[i].step - s0) * w / steps, y = FU_Y(ev[i].note);
        int32_t xw = fu_clampi(ev[i].len, 1, 16) * w / steps - 1;
        if (xw < 2) xw = 2;
        if (x + xw > x0 + w) xw = x0 + w - x;
        uint16_t c = ev[i].var ? T_ACCENT : ev[i].hand == 0 ? (var ? ux_mix(T_SURF, T_THEME, 75) : T_THEME) : T_TEXT;
        if (ev[i].hand == you)
            cv_frame(x, y - 1, xw, 3, c);
        else
            cv_rect(x, y - 1, xw, 2, c);
    }
#undef FU_Y
    if (p->playing && p->cur >= s0 && p->cur < s0 + steps)
        cv_rect(x0 + (p->cur - s0) * w / steps, top + 14, 1, h - 14, T_ACCENT);
    if (u->page == FU_PG_VAR2 && (p->s.key || p->s.scale)) {
        char t[24];
        t[0] = 0;
        if (p->s.key) { fu_cpy(t, "KEY ", sizeof t); fu_cat(t, ff_note_name(p->s.key), sizeof t); }
        if (p->s.scale) { if (t[0]) fu_cat(t, "  ", sizeof t); fu_cat(t, FN_SCALE[p->s.scale], sizeof t); }
        int32_t ty = top + h - 15, tw = text_w(&AF_S, t);
        cv_rect(230 - tw - 3, ty - 1, tw + 4, 14, T_SURF);   /* (over the notes: readable) */
        cv_text_r(230, ty, &AF_S, t, T_ACCENT, T_SURF);
    }
}

/* the FM-1's keys, F3 .. G5: lit, the notes to press (or held); tinted, the ones coming */
static void fu_keys(uint16_t lit, uint16_t soon, int32_t ky)
{
    static const uint8_t WHITE[16] = {53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79};
    const int32_t kx0 = 8, kw = 14, kh = 46;
    for (int i = 0; i < 16; ++i) {
        int pc = WHITE[i] % 12;
        uint16_t f = (lit >> pc & 1u) ? T_ACCENT : (soon >> pc & 1u) ? ux_mix(T_RAISE, T_THEME, 45) : T_RAISE;
        cv_rrect(kx0 + i * kw + 1, ky, kw - 2, kh, 2, f, T_SURF);
    }
    for (int i = 0; i < 15; ++i) {
        if (WHITE[i + 1] - WHITE[i] != 2) continue;
        int pc = (WHITE[i] + 1) % 12;
        uint16_t f = (lit >> pc & 1u) ? T_ACCENT : (soon >> pc & 1u) ? ux_mix(T_BG, T_THEME, 45) : T_BG;
        cv_rrect(kx0 + i * kw + kw - 4, ky, 8, 28, 2, f, T_RAISE);
    }
    cv_text_on(10, ky + kh + 4, &AF_S, "F3", T_DIM, T_SURF);
    cv_text_r(230, ky + kh + 4, &AF_S, "G5", T_DIM, T_SURF);
}

static void fu_panel_practice2(ff_ui *u)
{
    const ff_player *p = u->pl;
    int you = p->s.you == FF_YOU_NONE ? -1 : p->s.you - 1;
    const char *who = you == 0 ? "YOUR RIGHT HAND" : you == 1 ? "YOUR LEFT HAND" : "BOTH HANDS PLAYING";
    char t[16], a[8];
    cv_text_on(10, 4, &AF_S, who, T_MID, T_SURF);
    fu_cpy(t, "BAR ", sizeof t);
    fu_fmt_int(a, ff_bar(p));
    fu_cat(t, a, sizeof t);
    cv_text_r(230, 4, &AF_S, t, T_MID, T_SURF);
    uint16_t lit = 0, soon = 0;
    if (p->s.mode == FF_MODE_WAIT && you >= 0) {
        int pcs[3], n;
        for (int i = 0; p->waiting && i < p->nwant; ++i) lit |= (uint16_t)(1u << (p->want[i] % 12));
        n = ff_next_pcs(p, p->waiting ? p->want_step + 1 : (p->cur >= 0 ? p->cur + 1 : p->pos), pcs, 3);
        for (int i = 0; i < n; ++i) soon |= (uint16_t)(1u << pcs[i]);
        int32_t x = 10;
        x = cv_text_on(x, 24, &AF_S, "PRESS", T_MID, T_SURF) + 6;
        if (p->waiting && p->nwant)
            x = cv_text_on(x, 21, &AF_M, ff_note_name(p->want[0]), T_ACCENT, T_SURF) + 8;
        for (int i = 0; i < n; ++i)
            x = cv_text_on(x, 24, &AF_S, ff_note_name(pcs[i]), T_DIM, T_SURF) + 7;
        cv_text_r(230, 24, &AF_S, "ANY OCTAVE", T_DIM, T_SURF);
    } else {
        const char *m = p->s.mode == FF_MODE_TAP ? (p->playing ? "TAP ANY KEY" : "PLAY, THEN TAP")
                        : you >= 0 ? "PLAY YOUR HAND" : p->s.mode == FF_MODE_WAIT ? "SET YOU FIRST" : "PLAY ALONG";
        int32_t mw = text_w(&AF_M, m);
        cv_text_on(3 + HALF_UP(234 - mw), 22, &AF_M, m, p->s.mode == FF_MODE_TAP && p->playing ? T_ACCENT : T_TEXT, T_SURF);
        for (int k = 0; k <= FU_KEY_HI - FU_KEY_LO; ++k)
            if (u->keys_down >> k & 1u) lit |= (uint16_t)(1u << ((FU_KEY_LO + k) % 12));
    }
    fu_keys(lit, soon, 50);
}

static void fu_panel_space(ff_ui *u)
{
    const ff_player *p = u->pl;
    char t[16], a[8];
    cv_text_on(10, 4, &AF_M, FN_SPACE_LONG[p->s.space % FF_SPACE_COUNT], T_TEXT, T_SURF);
    fu_cpy(t, "SEND ", sizeof t);
    fu_fmt_int(a, p->s.reverb);
    fu_cat(t, a, sizeof t);
    cv_text_r(230, 4 + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H, &AF_S, t, T_MID, T_SURF);
    static const int8_t N[FF_SPACE_COUNT] = {30, 52, 64};
    int n = N[p->s.space % FF_SPACE_COUNT], base = FU_PANEL_H - 14;
    for (int i = 0; i < n; ++i) {
        int32_t x = 14 + i * 212 / n, bw = 212 / n - 2 > 1 ? 212 / n - 2 : 1;
        int a100 = 100;                               /* e^(-i / (n / 3.2)), in percent, without libm */
        for (int k = 0; k < i; ++k) a100 = a100 * (1000 - 3200 / n) / 1000;
        int hh = i == 0 ? 86 : a100 * 80 / 100 * (60 + 40 * ((i * 37) % 11) / 10) / 100 * (30 + 70 * p->s.reverb / 100) / 100;
        cv_rect(x, base - hh, bw, hh, i == 0 ? T_THEME : ux_mix(T_SURF, T_THEME, 25 + a100 * 70 / 100));
    }
    cv_rect(8, base + 1, 224, 1, T_LINE);
}

static void fu_panel_menu(ff_ui *u)
{
    static char rows[3][32];
    const char *items[3] = {rows[0], rows[1], rows[2]};
    fu_cpy(rows[0], "COLOR  ", sizeof rows[0]);
    fu_cat(rows[0], UI_PALETTES[u->pl->s.palette % NPALETTES].name, sizeof rows[0]);
    fu_cpy(rows[1], "STYLE  ", sizeof rows[1]);
    fu_cat(rows[1], FN_STYLE[u->pl->s.style % 2u], sizeof rows[1]);
    fu_cpy(rows[2], "FLYFISH " FF_VERSION_STR, sizeof rows[2]);
    fu_panel_list("MENU", items, 3, 0, false);
}

static void fu_draw_panel(ff_ui *u)
{
    fu_panel_begin();
    if (u->browse_ms > 0) {
        static const char *names[FF_SOUNDS];
        for (int i = 0; i < FF_SOUNDS; ++i) names[i] = ff_sound_name(i);
        fu_panel_list("SOUND", names, FF_SOUNDS, u->pl->s.sound, true);
    } else {
        switch (u->page) {
        case FU_PG_HOME: case FU_PG_FINGERS: if (u->pl->playing || fu_drops_alive(u)) fu_panel_waterfall(u); else fu_panel_stopped(u); break;
        case FU_PG_SOUND: {
            static const char *names[FF_SOUNDS];
            for (int i = 0; i < FF_SOUNDS; ++i) names[i] = ff_sound_name(i);
            fu_panel_list("SOUND", names, FF_SOUNDS, u->pl->s.sound, true);
            break;
        }
        case FU_PG_PRACTICE: fu_panel_roll(u, false); break;
        case FU_PG_PRACTICE2: fu_panel_practice2(u); break;
        case FU_PG_VAR: case FU_PG_VAR2: fu_panel_roll(u, true); break;
        case FU_PG_MIX: fu_panel_space(u); break;
        case FU_PG_MENU: fu_panel_menu(u); break;
        }
    }
    cv_blit(0, FU_PANEL_Y);
}

static void fu_background(void)
{
    cv_begin(240, 40, T_BG);
    for (int y = 0; y < 240; y += 40)
        cv_blit(0, (uint32_t)y);
    if (ux.style == ST_LINE) {                    /* LINE: no cards, 1 px rules between the areas */
        cv_begin(240, 1, T_RULE);
        cv_blit(0, FU_HEAD + 1);
        cv_blit(0, FU_CARD_Y + FU_CARD_H + 1);
        cv_blit(0, FU_FOOT_Y - 3);
        cv_begin(1, FU_CARD_H, T_RULE);
        for (int c = 1; c < 4; ++c)
            cv_blit((uint32_t)(FU_CARD_X(c) - 2), FU_CARD_Y);
    }
}

void fu_draw(ff_ui *u)
{
    if (u->force)
        fu_background();
    fu_draw_head(u);
    for (int i = 0; i < 4; ++i)
        fu_draw_card(u, i, &FU_CARDS[u->page][i]);
    fu_draw_panel(u);
    fu_draw_foot(u);
    u->force = false;
    u->frame++;
}

void fu_draw_all(ff_ui *u)
{
    u->force = true;
    fu_draw(u);
}

/* ---- input --------------------------------------------------------------------------------------------------- */
static void fu_msg(ff_ui *u, const char *m)
{
    fu_cpy(u->msg, m, sizeof u->msg);
    u->msg_ms = 1500;
}

static void fu_goto_page(ff_ui *u, int pg)
{
    if (pg != u->page) {
        u->page = pg;
        u->force = true;
    }
    u->browse_ms = 0;
}

static void fu_apply_style(ff_ui *u)
{
    ux.style = (uint8_t)(u->pl->s.style ? ST_LINE : ST_FLAT);
    palette_set((uint32_t)u->pl->s.palette % NPALETTES);
    u->force = true;
}

void fu_init(ff_ui *u, ff_player *p)
{
    memset(u, 0, sizeof *u);
    u->pl = p;
    u->page = FU_PG_HOME;
    u->hot = -1;
    u->rnd = 0x66666u;
    u->batt = 3;
    for (int i = 0; i < FU_RIPPLES; ++i) u->rp[i].age = -1;
    fu_apply_style(u);
}

void fu_knob(ff_ui *u, int k, int d)
{
    ff_player *p = u->pl;
    char m[24], a[8];
    switch (k) {
    case FU_K_SELECT:
        p->s.bpm = (uint8_t)fu_clampi(p->s.bpm + d, 40, 240);
        u->hot = 9;
        u->hot_ms = 700;
        u->changed = true;
        return;
    case FU_K_PRESETS:
        p->s.sound = (uint8_t)fu_wrap(p->s.sound + d, FF_SOUNDS);
        u->browse_ms = 1500;
        u->changed = true;
        return;
    case FU_K_ALGO: {
        int b = fu_clampi((p->jump ? p->jump : ff_bar(p)) + d, 1, 79);
        ff_goto_bar(p, b);
        fu_cpy(m, p->playing ? "TO BAR " : "BAR ", sizeof m);
        fu_fmt_int(a, b);
        fu_cat(m, a, sizeof m);
        fu_msg(u, m);
        return;
    }
    case FU_K1: case FU_K2: case FU_K3: case FU_K4: {
        const fu_cdef *c = &FU_CARDS[u->page][k - FU_K1];
        if (c->kind == FC_OFF) return;
        int v = fu_get(u, c) + d * c->step;
        if (c->kind == FC_ROUNDS)                     /* 1 .. 16, then INF (0); turning down from 1: INF */
            v = fu_wrap(v, FF_ROUNDS_MAX + 1);
        else if (c->kind == FC_SEED || c->kind == FC_SND || c->kind == FC_PAL || c->off == FS_(figure))
            v = c->lo + fu_wrap(v - c->lo, c->hi - c->lo + 1);
        v = fu_clampi(v, c->lo, c->hi);
        fu_set(u, c, v);
        if (c->off == FS_(from) && p->s.to < p->s.from) p->s.to = p->s.from;
        if (c->off == FS_(to) && p->s.from > p->s.to) p->s.from = p->s.to;
        if (c->off == FS_(mode)) p->waiting = false;
        if (c->off == FS_(palette) || c->off == FS_(style)) fu_apply_style(u);
        ff_changed(p);
        u->hot = k - FU_K1;
        u->hot_ms = 700;
        u->changed = true;
        return;
    }
    default:
        return;
    }
}

void fu_button(ff_ui *u, int b, bool down)
{
    ff_player *p = u->pl;
    if (b == FU_B_HOME) {
        if (down) {
            u->home_down = true;
            u->home_ms = 0;
        } else if (u->home_down) {
            u->home_down = false;
            if (u->home_ms < FU_MENU_HOLD_MS) fu_goto_page(u, FU_PG_HOME);
        }
        return;
    }
    if (!down)
        return;
    switch (b) {
    case FU_B_PLAY: ff_play(p, !p->playing); break;
    case FU_B_REC: {
        char m[24], a[8];
        ff_set_loop(p, !p->loop);
        if (p->loop) {
            fu_cpy(m, "LOOP ", sizeof m);
            fu_fmt_int(a, p->s.from); fu_cat(m, a, sizeof m); fu_cat(m, "-", sizeof m);
            fu_fmt_int(a, p->s.to); fu_cat(m, a, sizeof m);
        } else {
            fu_cpy(m, "LOOP OFF", sizeof m);
        }
        fu_msg(u, m);
        break;
    }
    case FU_B_OCTDN: case FU_B_OCTUP: {
        char m[16], a[8];
        ff_step_bar(p, b == FU_B_OCTUP ? 1 : -1);
        fu_cpy(m, "BAR ", sizeof m);
        fu_fmt_int(a, ff_bar(p));
        fu_cat(m, a, sizeof m);
        fu_msg(u, m);
        break;
    }
    case FU_B_EDIT: fu_goto_page(u, FU_PG_SOUND); break;
    case FU_B_GLO: fu_goto_page(u, FU_PG_MIX); break;
    case FU_B_FX: fu_goto_page(u, FU_PG_FINGERS); break;
    case FU_B_SEQ: fu_goto_page(u, u->page == FU_PG_PRACTICE ? FU_PG_PRACTICE2 : FU_PG_PRACTICE); break;
    case FU_B_ARP: fu_goto_page(u, u->page == FU_PG_VAR ? FU_PG_VAR2 : FU_PG_VAR); break;
    case FU_B_SAVE: fu_msg(u, p->playing ? "STOP TO SAVE" : "SAVED"); u->changed = true; break;
    default: fu_msg(u, "NOT USED"); break;
    }
}

void fu_key(ff_ui *u, int note, bool down)
{
    if (note < FU_KEY_LO || note > FU_KEY_HI)
        return;
    uint32_t bit = 1u << (note - FU_KEY_LO);
    if (down) u->keys_down |= bit;
    else u->keys_down &= ~bit;
    ff_key(u->pl, note, down);
}

/* ---- time --------------------------------------------------------------------------------------------------- */
static void fu_spawn(ff_ui *u, const ff_seen *n)
{
    int32_t x = fu_x_of(n->note);
    if (n->hand == FF_HAND_L && n->note < 48) {          /* a held bass: a ripple on the pool */
        int k = 0;
        for (int i = 0; i < FU_RIPPLES; ++i) {
            if (u->rp[i].age < 0) { k = i; break; }
            if (u->rp[i].age > u->rp[k].age) k = i;
        }
        u->rp[k].x = (int16_t)x;
        u->rp[k].age = 0;
        return;
    }
    int best = 0, life = 256;
    for (int i = 0; i < FU_DROPS; ++i) {             /* a free one, or the faintest */
        if (!u->dr[i].life) { best = i; break; }
        if (u->dr[i].life < life) { life = u->dr[i].life; best = i; }
    }
    fu_drop *d = &u->dr[best];
    u->rnd = u->rnd * 1664525u + 1013904223u;
    d->x = (int16_t)((x + (int32_t)(u->rnd >> 30) - 1) << 4);
    d->y = (int16_t)(FU_LEDGE << 4);
    d->vy = (int16_t)(1 + n->vel / 40);
    d->hand = n->hand;
    d->var = n->var;
    d->size = (uint8_t)(n->hand == FF_HAND_L ? 5 : n->vel > 100 ? 5 : 4);
    d->life = 255;
}

void fu_tick(ff_ui *u, int ms)
{
    ff_player *p = u->pl;
    ff_seen n;
    while (ff_feed_pop(p, &n))
        fu_spawn(u, &n);
    if (p->msg_new) {
        p->msg_new = 0;
        fu_msg(u, p->msg);
    }
    for (int i = 0; i < FU_DROPS; ++i) {
        fu_drop *d = &u->dr[i];
        if (!d->life) continue;
        int floor = (FU_POOL - d->size) << 4;
        if (d->y < floor) {
            d->y = (int16_t)(d->y + d->vy * ms / 16);
            d->vy = (int16_t)(d->vy + (ms + 24) / 48);  /* gravity */
            if (d->life > 200) d->life--;             /* (its first frames, above 250: the note just struck, ACCENT) */
            if (d->y >= floor) {
                d->y = (int16_t)floor;
                d->vy = 0;
                if (d->life > 120) d->life = 120;     /* landed: it fades */
            }
        } else {
            int f = ms / 6 > 0 ? ms / 6 : 1;
            d->life = (uint8_t)(d->life > f ? d->life - f : 0);
        }
    }
    for (int i = 0; i < FU_RIPPLES; ++i)
        if (u->rp[i].age >= 0 && (u->rp[i].age = (int16_t)(u->rp[i].age + ms)) > 2400)
            u->rp[i].age = -1;
    if (u->hot_ms > 0) u->hot_ms -= ms;
    if (u->browse_ms > 0 && (u->browse_ms -= ms) <= 0) {
        u->browse_ms = 0;
        u->force = true;
    }
    if (u->msg_ms > 0) u->msg_ms -= ms;
    if (u->home_down) {
        u->home_ms += ms;
        if (u->home_ms >= FU_MENU_HOLD_MS && u->page != FU_PG_MENU) {
            fu_goto_page(u, FU_PG_MENU);
            u->home_down = false;
        }
    }
}
