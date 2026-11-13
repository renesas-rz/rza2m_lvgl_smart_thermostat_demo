/**
 * @file thermostat_ui_elite.c
 * @brief Premium minimal thermostat HMI for RZ/A2M (LVGL 9, 800 x 480, RGB565).
 *
 * Screens
 *   Home    : weather | time, large indoor temperature, schedule / equipment
 *             status, "Heat to / Cool to" setpoint with chevrons,
 *             humidity | indoor air quality
 *   Mode    : Heat / Cool / Auto / Off cards
 *   Fan     : Auto / On cards
 *   Menu    : Room Priority, Ventilation, Idle Screen, Display & Units,
 *             Equipment Status
 *   Idle    : full-screen idle view after inactivity, 3 styles
 *             (Dashboard, Analog clock, Text) x 7 accent colors
 *
 * Same threading contract as thermostat_ui.c: the UI reads snapshots and posts
 * commands; it never touches the live model. Theme or style changes rebuild the
 * object tree via lv_async_call() (never from inside the deleted object's event).
 *
 * UI-only demo state (not in the control model): ventilation mode/timer,
 * "Active Rooms" priority mode, idle-screen style, accent color, theme.
 * Indoor air quality is estimated from humidity until a VOC sensor is wired in.
 *
 * Rev 2 (readability):
 *   - Home equipment status 16 px, colour-coded (heating orange, cooling blue).
 *   - Auto mode shows stacked "Heat 68" / "Cool 75" setpoints.
 *   - Dashboard idle-screen captions 20 px.
 *
 * Rev 3 (7-inch 800 x 480 panel, ~133 ppi):
 *   - All text uses Roboto fonts generated for this demo (fonts/tstat_fonts.h,
 *     4 bpp, stored in flash), sized to the reference proportions:
 *     home temperature 200 px (digits ~142 px = ~27 mm), setpoint 88 px,
 *     standby temperature / time 150 px, nav and labels 24-32 px.
 *     LV_SYMBOL_* icons fall back to the built-in Montserrat fonts.
 *   - Standby dashboard: temperature and time are each centred in their own
 *     half of the screen, symmetric about the centre divider.
 *   - Every page re-spaced for the larger type.
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "lvgl.h"
#if LV_USE_PERF_MONITOR
#include "lvgl_private.h"
#endif

#include "thermostat_ui_elite.h"
#include "thermostat_service.h"
#include "thermostat_hal.h"

/* ============================================================== fonts */
#include "fonts/tstat_fonts.h"

/* numerals (digits . - : % + degree), tight line height */
#define F_HERO      (&tstat_num_200)    /* home indoor temperature       */
#define F_BIG       (&tstat_num_150)    /* standby dashboard temp / time */
#define F_SP        (&tstat_num_88)     /* setpoint                      */
/* full ASCII + degree; LV_SYMBOL_* glyphs fall back to Montserrat        */
#define F_64        (&tstat_roboto_64)
#define F_48        (&tstat_roboto_48)
#define F_40        (&tstat_roboto_40)
#define F_32        (&tstat_roboto_32)
#define F_28        (&tstat_roboto_28)
#define F_24        (&tstat_roboto_24)
#define F_20        (&tstat_roboto_20)
#define F_16        (&tstat_roboto_16)
/* icon-only labels */
#define F_ICON40    (&lv_font_montserrat_40)
#define F_ICON32    (&lv_font_montserrat_32)
#define F_ICON24    (&lv_font_montserrat_24)

#define DEG         "\xC2\xB0"
#define SCR_W       (800)
#define SCR_H       (480)
#define NAV_W       (200)
#define CX_OFS      (25)     /* home content centre: between nav rail and setpoint column */
#define RC_CX       (718)    /* setpoint column centre x                                  */
#define REFRESH_MS  (200u)
#define PENDING_MS  (1500u)

/* ============================================================== theme */
typedef struct {
    lv_color_t bg, fg, dim, card, line, sel, accent;
    lv_color_t heat, cool;      /* status colours, contrast-tuned per theme */
} theme_t;

#define ACCENT_COUNT (7)
static const uint32_t s_accents[ACCENT_COUNT] = {
    0xE2725B, 0xA06CD5, 0x5CB85C, 0x8A8F98, 0x4FB3BF, 0xE0A93B, 0x4A78C2
};

typedef enum { IDLE_DASH = 0, IDLE_CLOCK, IDLE_TEXT, IDLE_STYLE_COUNT } idle_style_t;
static const char * const s_idle_names[IDLE_STYLE_COUNT] = { "Dashboard", "Clock", "Text" };

typedef enum {
    PG_HOME = 0, PG_MODE, PG_FAN, PG_MENU,                     /* with nav rail */
    PG_PRIORITY, PG_VENT, PG_IDLEPICK, PG_DISPLAY, PG_STATUS,  /* full screen + Back */
    PG_COUNT
} page_t;
#define PG_NAV_LAST (PG_MENU)

/* persistent settings (survive a rebuild) */
typedef struct {
    bool     dark;
    uint8_t  accent;
    uint8_t  idle_style;
    bool     celsius;
    uint16_t idle_timeout_s;      /* 0 = never */
    uint8_t  brightness;
    uint8_t  vent_auto;           /* 1 = Auto, 0 = Off               */
    uint32_t vent_timer_end_s;    /* model clock_s when timer ends   */
    bool     priority_active;     /* "Active Rooms" mode             */
    page_t   page;
} cfg_t;

static cfg_t s_cfg = { true, 4u, IDLE_TEXT, false, 30u, 80u, 1u, 0u, false, PG_HOME };

/* idle view instance (overlay and chooser preview) */
typedef struct {
    lv_obj_t * root;
    lv_obj_t * time;
    lv_obj_t * temp;
    lv_obj_t * line3;
    lv_obj_t * tile_val[4];
    lv_obj_t * hand_h;
    lv_obj_t * hand_m;
    lv_point_precise_t pts_h[2];
    lv_point_precise_t pts_m[2];
    int32_t    cx, cy, len_h, len_m;
    uint8_t    style;
    bool       big;
} idle_view_t;

typedef struct {
    theme_t    T;
    lv_obj_t * scr;
    lv_obj_t * nav;
    lv_obj_t * nav_item[4];
    lv_obj_t * page[PG_COUNT];
    lv_obj_t * badge;

    /* home */
    lv_obj_t * h_out, * h_time, * h_temp, * h_sched, * h_equip;
    lv_obj_t * h_hum, * h_iaq;
    lv_obj_t * h_up, * h_down, * h_sp_cap, * h_sp_val;
    lv_obj_t * h_sp_heat, * h_sp_cool;          /* Auto mode: stacked pair */

    /* mode / fan */
    lv_obj_t * mode_card[TSTAT_MODE_COUNT];
    lv_obj_t * fan_card[TSTAT_FAN_COUNT];
    lv_obj_t * mode_note;

    /* priority */
    lv_obj_t * pr_seg[2];
    lv_obj_t * pr_hint;
    lv_obj_t * pr_card[TSTAT_SENSOR_COUNT];
    lv_obj_t * pr_temp[TSTAT_SENSOR_COUNT];
    lv_obj_t * pr_occ[TSTAT_SENSOR_COUNT];
    bool       pr_sel[TSTAT_SENSOR_COUNT];
    uint8_t    pr_tab;

    /* ventilation */
    lv_obj_t * v_seg[3];
    lv_obj_t * v_tab[3];
    lv_obj_t * v_status;
    lv_obj_t * v_card[2];
    lv_obj_t * v_timer_btn[3];
    lv_obj_t * v_timer_lbl;
    uint8_t    v_sel;            /* pending Auto/Off before Save */

    /* idle chooser */
    lv_obj_t * ip_holder;
    lv_obj_t * ip_left, * ip_right;
    lv_obj_t * ip_dot[ACCENT_COUNT];
    lv_obj_t * ip_style_dot[IDLE_STYLE_COUNT];
    uint8_t    ip_style, ip_accent;

    /* display & units */
    lv_obj_t * d_theme[2], * d_unit[2], * d_to[4], * d_speed[3];
    lv_obj_t * d_bright_lbl;

    /* status */
    lv_obj_t * st_val[8];
    lv_obj_t * st_perf;

    /* idle */
    lv_obj_t *  overlay;
    bool        overlay_preview;
    idle_view_t iv_overlay;
    idle_view_t iv_preview;
    bool        idle_on;

    /* state */
    bool       pend_valid;
    tdeci_t    pend_val;
    uint32_t   pend_tick;
} ui_t;

static ui_t             s_ui;
static tstat_snapshot_t s_snap;
static uint32_t         s_seq;
static bool             s_have_snap;
static lv_timer_t *     s_timer;

static const uint16_t s_speeds[3]   = { 1u, 10u, 60u };
static const uint16_t s_timeouts[4] = { 15u, 30u, 60u, 0u };
static const uint16_t s_vent_min[3] = { 20u, 40u, 60u };

/* ============================================================== utils */

static void build_all(void);

static int32_t div_round(int32_t n, int32_t d)
{
    return (n >= 0) ? ((n + d / 2) / d) : -((-n + d / 2) / d);
}

static void theme_init(void)
{
    theme_t * t = &s_ui.T;
    if (s_cfg.dark) {
        t->bg = lv_color_hex(0x000000); t->fg = lv_color_hex(0xFFFFFF); t->dim = lv_color_hex(0x8C8C8C);
        t->card = lv_color_hex(0x1C1C1E); t->line = lv_color_hex(0x3A3A3C); t->sel = lv_color_hex(0x48484A);
        t->heat = lv_color_hex(0xFF8A3D); t->cool = lv_color_hex(0x5AAEFF);
    } else {
        t->bg = lv_color_hex(0xFFFFFF); t->fg = lv_color_hex(0x111111); t->dim = lv_color_hex(0x6E6E73);
        t->card = lv_color_hex(0xEFEFF2); t->line = lv_color_hex(0xC7C7CC); t->sel = lv_color_hex(0xFFFFFF);
        t->heat = lv_color_hex(0xC2410C); t->cool = lv_color_hex(0x1D5FBF);   /* >= 4.5:1 on white */
    }
    t->accent = lv_color_hex(s_accents[s_cfg.accent % ACCENT_COUNT]);
}

static void set_text(lv_obj_t * l, const char * txt)
{
    const char * cur = lv_label_get_text(l);
    if ((NULL == cur) || (0 != strcmp(cur, txt))) { lv_label_set_text(l, txt); }
}

/* LVGL invalidates (redraws) a widget on every style/flag/size/position setter, even when the new value equals the
 * old one. The periodic refresh therefore only writes a value that actually changed; an unconditional
 * ui_set_arc_color() on the 340 px dial alone redrew about 30% of the screen once per second. */
static void set_hidden(lv_obj_t * obj, bool hidden)
{
    if (hidden == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) { return; }
    if (hidden) { lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); }
    else        { lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN); }
}

static void set_clickable(lv_obj_t * obj, bool clickable)
{
    if (clickable == lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) { return; }
    if (clickable) { lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE); }
    else           { lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE); }
}

static void ui_set_arc_color(lv_obj_t * o, lv_color_t c, lv_part_t part)
{
    if (!lv_color_eq(lv_obj_get_style_arc_color(o, part), c)) { lv_obj_set_style_arc_color(o, c, part); }
}

static void ui_set_text_color(lv_obj_t * o, lv_color_t c, lv_part_t part)
{
    if (!lv_color_eq(lv_obj_get_style_text_color(o, part), c)) { lv_obj_set_style_text_color(o, c, part); }
}

static void ui_set_bg_color(lv_obj_t * o, lv_color_t c, lv_part_t part)
{
    if (!lv_color_eq(lv_obj_get_style_bg_color(o, part), c)) { lv_obj_set_style_bg_color(o, c, part); }
}

static void ui_set_border_color(lv_obj_t * o, lv_color_t c, lv_part_t part)
{
    if (!lv_color_eq(lv_obj_get_style_border_color(o, part), c)) { lv_obj_set_style_border_color(o, c, part); }
}

static void ui_set_border_width(lv_obj_t * o, int32_t w, lv_part_t part)
{
    if (lv_obj_get_style_border_width(o, part) != w) { lv_obj_set_style_border_width(o, w, part); }
}

static void ui_set_opa(lv_obj_t * o, lv_opa_t opa, lv_part_t part)
{
    if (lv_obj_get_style_opa(o, part) != opa) { lv_obj_set_style_opa(o, opa, part); }
}

static void ui_set_x(lv_obj_t * o, int32_t x)
{
    if (lv_obj_get_style_x(o, LV_PART_MAIN) != x) { lv_obj_set_x(o, x); }
}

static void ui_set_y(lv_obj_t * o, int32_t y)
{
    if (lv_obj_get_style_y(o, LV_PART_MAIN) != y) { lv_obj_set_y(o, y); }
}

static void ui_set_height(lv_obj_t * o, int32_t h)
{
    if (lv_obj_get_style_height(o, LV_PART_MAIN) != h) { lv_obj_set_height(o, h); }
}

/** whole-degree display value (hero font has no '.') */
static int32_t disp_whole(tdeci_t f)
{
    return s_cfg.celsius ? div_round(tstat_deci_f_to_deci_c(f), 10) : div_round(f, 10);
}

static void fmt_deg(char * b, size_t n, tdeci_t f)
{
    lv_snprintf(b, n, "%d" DEG, (int)disp_whole(f));
}

/** setpoint text: F integer, C in 0.5 steps */
static void fmt_sp(char * b, size_t n, tdeci_t f)
{
    if (s_cfg.celsius) {
        int32_t c = div_round(tstat_deci_f_to_deci_c(f), 5) * 5;
        if ((c % 10) != 0) { lv_snprintf(b, n, "%d.5", (int)(c / 10)); }
        else               { lv_snprintf(b, n, "%d", (int)(c / 10)); }
    } else {
        lv_snprintf(b, n, "%d", (int)div_round(f, 10));
    }
}

static tdeci_t step_sp(tdeci_t f, int dir)
{
    if (s_cfg.celsius) {
        int32_t c = div_round(tstat_deci_f_to_deci_c(f), 5) * 5;
        return tstat_deci_c_to_deci_f((int16_t)(c + dir * 5));
    }
    return (tdeci_t)(div_round(f, 10) * 10 + dir * 10);
}

static void clock_parts(uint32_t clock_s, unsigned * h12, unsigned * mi, bool * pm, unsigned * h24)
{
    uint32_t sod = clock_s % TSTAT_SECONDS_PER_DAY;
    unsigned h = (unsigned)(sod / 3600u);
    *h24 = h;
    *mi  = (unsigned)((sod / 60u) % 60u);
    *pm  = (h >= 12u);
    h %= 12u;
    *h12 = (0u == h) ? 12u : h;
}

static const char * weather_text(const tstat_model_t * m)
{
    unsigned h12, mi, h24; bool pm;
    clock_parts(m->clock_s, &h12, &mi, &pm, &h24);
    if ((h24 < 7u) || (h24 >= 19u)) { return "Clear"; }
    return (m->outdoor >= 550) ? "Sunny" : "Partly Sunny";
}

static const char * iaq_text(const tstat_model_t * m)
{
    return ((m->humidity >= 30u) && (m->humidity <= 55u)) ? "Good" : "Fair";
}

static void send_cmd(tstat_cmd_id_t id, uint8_t a, int16_t v1, int16_t v2)
{
    tstat_cmd_t c = { id, a, v1, v2 };
    (void)thermostat_service_send(&c);
}

/* ============================================================ widgets */

static lv_obj_t * box(lv_obj_t * p, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t * label(lv_obj_t * p, const lv_font_t * f, lv_color_t c, const char * t)
{
    lv_obj_t * l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    ui_set_text_color(l, c, 0);
    lv_label_set_text(l, t);
    return l;
}

/** Outlined card (unselected 1 px line, selected 3 px fg) */
static lv_obj_t * card(lv_obj_t * p, int32_t x, int32_t y, int32_t w, int32_t h, lv_event_cb_t cb, void * ud)
{
    lv_obj_t * c = box(p, x, y, w, h);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    ui_set_bg_color(c, s_ui.T.bg, 0);
    ui_set_bg_color(c, s_ui.T.card, LV_STATE_PRESSED);
    ui_set_border_width(c, 1, 0);
    ui_set_border_color(c, s_ui.T.line, 0);
    lv_obj_set_style_pad_all(c, 14, 0);
    if (NULL != cb) {
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, ud);
    }
    return c;
}

static void card_select(lv_obj_t * c, bool sel)
{
    ui_set_border_width(c, sel ? 3 : 1, 0);
    ui_set_border_color(c, sel ? s_ui.T.fg : s_ui.T.line, 0);
}

/** Pill button. solid: filled fg with bg text; else outlined. */
static lv_obj_t * pill(lv_obj_t * p, int32_t x, int32_t y, int32_t w, int32_t h, const char * t, bool solid,
                       const lv_font_t * f, lv_event_cb_t cb, void * ud)
{
    lv_obj_t * b = box(p, x, y, w, h);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(b, solid ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    ui_set_bg_color(b, s_ui.T.fg, 0);
    ui_set_border_width(b, solid ? 0 : 2, 0);
    ui_set_border_color(b, s_ui.T.fg, 0);
    ui_set_opa(b, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_t * l = label(b, f, solid ? s_ui.T.bg : s_ui.T.fg, t);
    lv_obj_center(l);
    if (NULL != cb) { lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud); }
    return b;
}

/** Segmented control; returns container, fills btns[] */
static lv_obj_t * segmented(lv_obj_t * p, int32_t cx, int32_t y, const char * const * names, int n,
                            int32_t seg_w, lv_obj_t ** btns, lv_event_cb_t cb)
{
    int32_t w = n * seg_w + 8;
    lv_obj_t * c = box(p, cx - w / 2, y, w, 54);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    ui_set_bg_color(c, s_ui.T.card, 0);
    for (int i = 0; i < n; i++) {
        lv_obj_t * b = box(c, 4 + i * seg_w, 4, seg_w, 46);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        ui_set_bg_color(b, s_ui.T.sel, 0);
        lv_obj_t * l = label(b, F_24, s_ui.T.fg, names[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        btns[i] = b;
    }
    return c;
}

static void segmented_set(lv_obj_t ** btns, int n, int sel)
{
    for (int i = 0; i < n; i++) {
        lv_obj_set_style_bg_opa(btns[i], (i == sel) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_t * l = lv_obj_get_child(btns[i], 0);
        ui_set_text_color(l, (i == sel) ? s_ui.T.fg : s_ui.T.dim, 0);
    }
}

/* -------- simple vector icons (static points shared by all instances) */
static const lv_point_precise_t s_snow_a[2] = { { 16, 1 }, { 16, 31 } };
static const lv_point_precise_t s_snow_b[2] = { {  3, 9 }, { 29, 23 } };
static const lv_point_precise_t s_snow_c[2] = { {  3, 23 }, { 29, 9 } };

static lv_obj_t * icon_line(lv_obj_t * p, const lv_point_precise_t * pts, lv_color_t c, int32_t w)
{
    lv_obj_t * l = lv_line_create(p);
    lv_line_set_points(l, pts, 2);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, c, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_set_pos(l, 0, 0);
    return l;
}

static lv_obj_t * icon_snowflake(lv_obj_t * p, int32_t x, int32_t y, lv_color_t c)
{
    lv_obj_t * o = box(p, x, y, 32, 32);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    icon_line(o, s_snow_a, c, 3);
    icon_line(o, s_snow_b, c, 3);
    icon_line(o, s_snow_c, c, 3);
    return o;
}

static lv_obj_t * icon_sun(lv_obj_t * p, int32_t d)
{
    lv_obj_t * o = box(p, 0, 0, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    ui_set_bg_color(o, lv_color_hex(0xF5A623), 0);
    return o;
}

/** content-sized flex row (items centred vertically unless align_top) */
static lv_obj_t * flex_row(lv_obj_t * p)
{
    lv_obj_t * r = lv_obj_create(p);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 14, 0);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    return r;
}

static lv_obj_t * vbar(lv_obj_t * p)
{
    lv_obj_t * b = box(p, 0, 0, 2, 32);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    ui_set_bg_color(b, s_ui.T.line, 0);
    return b;
}

/* ========================================================== navigation */

static void refresh_all(void);

static void show_page(page_t pg)
{
    s_cfg.page = pg;
    for (int i = 0; i < PG_COUNT; i++) { set_hidden(s_ui.page[i], i != (int)pg); }
    set_hidden(s_ui.nav, pg > PG_NAV_LAST);
    for (int i = 0; i < 4; i++) {
        bool sel = (i == (int)pg);
        lv_obj_t * it = s_ui.nav_item[i];
        uint32_t n = lv_obj_get_child_count(it);
        for (uint32_t k = 0; k < n; k++) {
            lv_obj_t * ch = lv_obj_get_child(it, (int32_t)k);
            if (lv_obj_check_type(ch, &lv_label_class)) {
                ui_set_text_color(ch, sel ? s_ui.T.fg : s_ui.T.dim, 0);
            } else {
                uint32_t nl = lv_obj_get_child_count(ch);   /* vector icon */
                for (uint32_t q = 0; q < nl; q++) {
                    lv_obj_set_style_line_color(lv_obj_get_child(ch, (int32_t)q), sel ? s_ui.T.fg : s_ui.T.dim, 0);
                }
            }
        }
        lv_obj_set_style_bg_opa(it, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    if (s_have_snap) { refresh_all(); }
}

static void nav_cb(lv_event_t * e)   { show_page((page_t)(uintptr_t)lv_event_get_user_data(e)); }
static void back_cb(lv_event_t * e)  { (void)e; show_page(PG_MENU); }

static lv_obj_t * subpage_header(lv_obj_t * pg, const char * title)
{
    lv_obj_t * b = box(pg, 12, 10, 150, 56);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    ui_set_opa(b, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_t * l = label(b, F_28, s_ui.T.fg, LV_SYMBOL_LEFT "  Back");
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(b, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * t = label(pg, F_32, s_ui.T.fg, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 19);
    return t;
}

static lv_obj_t * nav_item(lv_obj_t * nav, int idx, const char * sym, const char * text)
{
    lv_obj_t * it = box(nav, 16, 92 + idx * 86, NAV_W - 28, 72);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(it, 14, 0);
    ui_set_bg_color(it, s_ui.T.card, 0);
    lv_obj_set_style_bg_opa(it, LV_OPA_TRANSP, 0);
    if (NULL != sym) {
        lv_obj_t * ic = label(it, F_ICON32, s_ui.T.dim, sym);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 14, 0);
    } else {
        (void)icon_snowflake(it, 14, 20, s_ui.T.dim);
    }
    lv_obj_t * tl = label(it, F_28, s_ui.T.dim, text);
    lv_obj_align(tl, LV_ALIGN_LEFT_MID, 64, 0);
    lv_obj_add_event_cb(it, nav_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    return it;
}

/* ================================================================ home */

static tdeci_t home_target(const tstat_model_t * m)
{
    if (s_ui.pend_valid) { return s_ui.pend_val; }
    return (TSTAT_MODE_COOL == m->mode) ? m->active_sp.cool : m->active_sp.heat;
}

static void chevron_cb(lv_event_t * e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    const tstat_model_t * m = &s_snap.model;
    if (TSTAT_MODE_OFF == m->mode) { return; }

    if (TSTAT_MODE_AUTO == m->mode) {
        tdeci_t h = step_sp(m->active_sp.heat, dir);
        send_cmd(TSTAT_CMD_SHIFT_SETPOINTS, 0u, (int16_t)(h - m->active_sp.heat), 0);
        return;
    }
    tdeci_t v = step_sp(home_target(m), dir);
    if (TSTAT_MODE_COOL == m->mode) {
        if ((v < TSTAT_COOL_SP_MIN) || (v > TSTAT_COOL_SP_MAX)) { return; }
        send_cmd(TSTAT_CMD_SET_SETPOINT, 1u, v, 0);
    } else {
        if ((v < TSTAT_HEAT_SP_MIN) || (v > TSTAT_HEAT_SP_MAX)) { return; }
        send_cmd(TSTAT_CMD_SET_SETPOINT, 0u, v, 0);
    }
    s_ui.pend_valid = true;
    s_ui.pend_val   = v;
    s_ui.pend_tick  = lv_tick_get();
    refresh_all();
}

static void sched_tap_cb(lv_event_t * e)
{
    (void)e;
    if (s_snap.model.hold) {
        s_ui.pend_valid = false;
        send_cmd(TSTAT_CMD_RESUME_SCHEDULE, 0u, 0, 0);
    }
}

static lv_obj_t * chevron(lv_obj_t * p, int32_t x, int32_t y, const char * sym, int dir)
{
    lv_obj_t * b = box(p, x, y, 130, 76);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, 14, 0);
    ui_set_bg_color(b, s_ui.T.card, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_t * l = label(b, F_ICON40, s_ui.T.fg, sym);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, chevron_cb, LV_EVENT_CLICKED, (void *)(intptr_t)dir);
    lv_obj_add_event_cb(b, chevron_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)dir);
    return b;
}

static lv_obj_t * sp_label(lv_obj_t * pg, const lv_font_t * f, lv_color_t c, int32_t x, int32_t y, int32_t w)
{
    lv_obj_t * l = label(pg, f, c, "");
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void build_home(lv_obj_t * pg)
{
    const theme_t * T = &s_ui.T;

    /* top: sun  45° | 1:32  (centre y ~43) */
    lv_obj_t * top = flex_row(pg);
    lv_obj_align(top, LV_ALIGN_TOP_MID, CX_OFS, 24);
    icon_sun(top, 26);
    s_ui.h_out = label(top, F_32, T->fg, "--" DEG);
    vbar(top);
    s_ui.h_time = label(top, F_32, T->fg, "--:--");

    /* hero: 200 px indoor temperature (centre y ~216) */
    s_ui.h_temp = label(pg, F_HERO, T->fg, "--");
    lv_obj_align(s_ui.h_temp, LV_ALIGN_CENTER, CX_OFS, -24);

    s_ui.h_sched = label(pg, F_24, T->fg, "");
    lv_obj_align(s_ui.h_sched, LV_ALIGN_CENTER, CX_OFS, 84);
    lv_obj_add_flag(s_ui.h_sched, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_ui.h_sched, 20);
    lv_obj_add_event_cb(s_ui.h_sched, sched_tap_cb, LV_EVENT_CLICKED, NULL);

    /* equipment status, colour set per state in refresh_home() */
    s_ui.h_equip = label(pg, F_24, T->dim, "");
    lv_obj_align(s_ui.h_equip, LV_ALIGN_CENTER, CX_OFS, 116);

    /* bottom: tint 46% | house Good (centre y ~417) */
    lv_obj_t * bot = flex_row(pg);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, CX_OFS, -44);
    label(bot, F_ICON32, T->dim, LV_SYMBOL_TINT);
    s_ui.h_hum = label(bot, F_32, T->fg, "--%");
    vbar(bot);
    label(bot, F_ICON32, T->dim, LV_SYMBOL_HOME);
    s_ui.h_iaq = label(bot, F_32, T->fg, "--");

    /* brand */
    lv_obj_t * br = label(pg, F_16, T->dim, "RENESAS  RZ/A2M");
    lv_obj_set_style_text_letter_space(br, 3, 0);
    lv_obj_align(br, LV_ALIGN_BOTTOM_MID, CX_OFS, -8);

    /* right setpoint column, centred on RC_CX */
    s_ui.h_up      = chevron(pg, RC_CX - 65, 96, LV_SYMBOL_UP, 1);
    s_ui.h_sp_cap  = sp_label(pg, F_28, T->fg, RC_CX - 80, 184, 160);
    s_ui.h_sp_val  = sp_label(pg, F_SP, T->fg, RC_CX - 80, 220, 160);
    /* Auto mode: two stacked, colour-coded setpoints */
    s_ui.h_sp_heat = sp_label(pg, F_40, T->heat, RC_CX - 80, 186, 160);
    s_ui.h_sp_cool = sp_label(pg, F_40, T->cool, RC_CX - 80, 240, 160);
    s_ui.h_down    = chevron(pg, RC_CX - 65, 300, LV_SYMBOL_DOWN, -1);
}

static void refresh_home(const tstat_model_t * m)
{
    char b[48], t1[16];
    unsigned h12, mi, h24; bool pm;

    clock_parts(m->clock_s, &h12, &mi, &pm, &h24);
    fmt_deg(b, sizeof(b), m->outdoor);             set_text(s_ui.h_out, b);
    lv_snprintf(b, sizeof(b), "%u:%02u", h12, mi); set_text(s_ui.h_time, b);
    lv_snprintf(b, sizeof(b), "%d", (int)disp_whole(m->indoor)); set_text(s_ui.h_temp, b);

    uint16_t nx = tstat_model_next_transition_min(m);
    unsigned nh = (unsigned)(nx / 60u) % 12u; if (0u == nh) { nh = 12u; }
    if (m->hold) {
        lv_snprintf(b, sizeof(b), "Hold until %u:%02u %s   " LV_SYMBOL_REFRESH " Resume",
                    nh, (unsigned)(nx % 60u), (nx >= 720u) ? "PM" : "AM");
    } else if (TSTAT_MODE_OFF == m->mode) {
        lv_snprintf(b, sizeof(b), "System Off");
    } else {
        lv_snprintf(b, sizeof(b), "Following Schedule (%s)", tstat_comfort_name(m->active_comfort));
    }
    set_text(s_ui.h_sched, b);

    const char * eq;
    lv_color_t   ec = s_ui.T.dim;
    switch (m->hvac) {
        case TSTAT_HVAC_HEATING:   eq = "Heating";               ec = s_ui.T.heat; break;
        case TSTAT_HVAC_COOLING:   eq = "Cooling";               ec = s_ui.T.cool; break;
        case TSTAT_HVAC_WAIT_HEAT:
        case TSTAT_HVAC_WAIT_COOL: eq = "Waiting for Equipment"; ec = s_ui.T.dim;  break;
        case TSTAT_HVAC_FAN_ONLY:  eq = "Fan On";                ec = s_ui.T.fg;   break;
        default:                   eq = "";                                         break;
    }
    set_text(s_ui.h_equip, eq);
    ui_set_text_color(s_ui.h_equip, ec, 0);

    lv_snprintf(b, sizeof(b), "%u%%", (unsigned)m->humidity); set_text(s_ui.h_hum, b);
    set_text(s_ui.h_iaq, iaq_text(m));

    const bool off  = (TSTAT_MODE_OFF == m->mode);
    const bool autm = (TSTAT_MODE_AUTO == m->mode);
    set_hidden(s_ui.h_up, off);
    set_hidden(s_ui.h_down, off);
    set_hidden(s_ui.h_sp_cap, autm);
    set_hidden(s_ui.h_sp_val, autm);
    set_hidden(s_ui.h_sp_heat, !autm);
    set_hidden(s_ui.h_sp_cool, !autm);

    if (off) {
        set_text(s_ui.h_sp_cap, "System");
        set_text(s_ui.h_sp_val, "Off");
        lv_obj_set_style_text_font(s_ui.h_sp_val, F_48, 0);
    } else if (autm) {
        /* 0.5 C steps make the text wider: drop to 32 px so it stays on one line */
        const lv_font_t * af = s_cfg.celsius ? F_32 : F_40;
        lv_obj_set_style_text_font(s_ui.h_sp_heat, af, 0);
        lv_obj_set_style_text_font(s_ui.h_sp_cool, af, 0);
        fmt_sp(t1, sizeof(t1), m->active_sp.heat);
        lv_snprintf(b, sizeof(b), "Heat %s", t1); set_text(s_ui.h_sp_heat, b);
        fmt_sp(t1, sizeof(t1), m->active_sp.cool);
        lv_snprintf(b, sizeof(b), "Cool %s", t1); set_text(s_ui.h_sp_cool, b);
    } else {
        set_text(s_ui.h_sp_cap, (TSTAT_MODE_COOL == m->mode) ? "Cool to" : "Heat to");
        fmt_sp(t1, sizeof(t1), home_target(m));
        /* "20.5" at 88 px is wider than the column: use 64 px for half degrees */
        lv_obj_set_style_text_font(s_ui.h_sp_val, (NULL != strchr(t1, '.')) ? F_64 : F_SP, 0);
        set_text(s_ui.h_sp_val, t1);
    }
}

/* =========================================================== mode / fan */

static void mode_cb(lv_event_t * e)
{
    s_ui.pend_valid = false;
    send_cmd(TSTAT_CMD_SET_MODE, (uint8_t)(uintptr_t)lv_event_get_user_data(e), 0, 0);
}

static void fan_cb(lv_event_t * e)
{
    send_cmd(TSTAT_CMD_SET_FAN, (uint8_t)(uintptr_t)lv_event_get_user_data(e), 0, 0);
}

static lv_obj_t * option_card(lv_obj_t * pg, int32_t x, int32_t y, int32_t w, int32_t h,
                              const char * icon, const char * title, const char * desc,
                              lv_event_cb_t cb, void * ud)
{
    lv_obj_t * c = card(pg, x, y, w, h, cb, ud);
    lv_obj_t * ic = label(c, F_ICON32, s_ui.T.fg, icon);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t * t = label(c, F_28, s_ui.T.fg, title);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_t * d = label(c, F_20, s_ui.T.dim, desc);
    lv_obj_set_width(d, w - 28);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_align(d, LV_ALIGN_TOP_LEFT, 0, 80);
    return c;
}

static void build_mode(lv_obj_t * pg)
{
    static const char * const icons[TSTAT_MODE_COUNT] = { LV_SYMBOL_POWER, LV_SYMBOL_CHARGE, "", LV_SYMBOL_SHUFFLE };
    static const char * const desc[TSTAT_MODE_COUNT]  = {
        "Heating and cooling are turned off", "Heats your home to the set temperature",
        "Cools your home to the set temperature", "Switches between heat and cool as needed" };
    static const tstat_mode_t order[TSTAT_MODE_COUNT] = { TSTAT_MODE_HEAT, TSTAT_MODE_COOL, TSTAT_MODE_AUTO, TSTAT_MODE_OFF };

    lv_obj_t * t = label(pg, F_32, s_ui.T.fg, "System Mode");
    lv_obj_set_pos(t, NAV_W + 12, 20);

    const int32_t w = 284, h = 164;
    for (int i = 0; i < TSTAT_MODE_COUNT; i++) {
        tstat_mode_t md = order[i];
        int32_t x = NAV_W + 12 + (i % 2) * (w + 12);
        int32_t y = 72 + (i / 2) * (h + 12);
        lv_obj_t * c = option_card(pg, x, y, w, h, icons[md], tstat_mode_name(md), desc[md],
                                   mode_cb, (void *)(uintptr_t)md);
        if (TSTAT_MODE_COOL == md) { (void)icon_snowflake(c, 0, 0, s_ui.T.fg); }
        s_ui.mode_card[md] = c;
    }
    s_ui.mode_note = label(pg, F_20, s_ui.T.dim, "");
    lv_obj_set_pos(s_ui.mode_note, NAV_W + 12, 438);
}

static void refresh_mode(const tstat_model_t * m)
{
    char b[96];
    for (int i = 0; i < TSTAT_MODE_COUNT; i++) { card_select(s_ui.mode_card[i], (int)m->mode == i); }
    lv_snprintf(b, sizeof(b), "Compressor protection: %u min off time between cycles",
                (unsigned)(m->min_off_s / 60u));
    set_text(s_ui.mode_note, b);
}

static void build_fan(lv_obj_t * pg)
{
    lv_obj_t * t = label(pg, F_32, s_ui.T.fg, "Fan Mode");
    lv_obj_set_pos(t, NAV_W + 12, 20);
    s_ui.fan_card[TSTAT_FAN_AUTO] = option_card(pg, NAV_W + 12, 72, 284, 190, LV_SYMBOL_REFRESH, "Auto",
        "Fan runs only while heating or cooling", fan_cb, (void *)(uintptr_t)TSTAT_FAN_AUTO);
    s_ui.fan_card[TSTAT_FAN_ON] = option_card(pg, NAV_W + 308, 72, 284, 190, LV_SYMBOL_LOOP, "On",
        "Fan runs continuously to circulate air", fan_cb, (void *)(uintptr_t)TSTAT_FAN_ON);
}

static void refresh_fan(const tstat_model_t * m)
{
    for (int i = 0; i < TSTAT_FAN_COUNT; i++) { card_select(s_ui.fan_card[i], (int)m->fan == i); }
}

/* ================================================================ menu */

static void menu_cb(lv_event_t * e)
{
    page_t pg = (page_t)(uintptr_t)lv_event_get_user_data(e);
    if (PG_IDLEPICK == pg) {
        s_ui.ip_style  = s_cfg.idle_style;
        s_ui.ip_accent = s_cfg.accent;
    }
    if (PG_VENT == pg) { s_ui.v_sel = s_cfg.vent_auto; }
    if (PG_PRIORITY == pg) {
        for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) { s_ui.pr_sel[i] = (0u != s_snap.model.sensors[i].participating); }
        s_ui.pr_tab = s_cfg.priority_active ? 1u : 0u;
    }
    show_page(pg);
}

static void build_menu(lv_obj_t * pg)
{
    static const char * const names[] = { "Priority", "Ventilation", "Idle Screen", "Display & Units", "Equipment Status" };
    static const char * const subs[]  = { "Choose rooms to average for comfort", "Fresh-air ventilation mode and timer",
                                          "Style and color of the idle display", "Theme, units, timeout, demo speed",
                                          "Outputs, protection timers, system info" };
    static const page_t dst[] = { PG_PRIORITY, PG_VENT, PG_IDLEPICK, PG_DISPLAY, PG_STATUS };

    lv_obj_t * t = label(pg, F_32, s_ui.T.fg, "Menu");
    lv_obj_set_pos(t, NAV_W + 12, 20);
    for (int i = 0; i < 5; i++) {
        lv_obj_t * r = box(pg, NAV_W + 12, 66 + i * 80, SCR_W - NAV_W - 32, 76);
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(r, 10, 0);
        ui_set_bg_color(r, s_ui.T.card, 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
        ui_set_border_width(r, 1, 0);
        ui_set_border_color(r, s_ui.T.line, 0);
        lv_obj_t * n = label(r, F_28, s_ui.T.fg, names[i]);
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 14, 6);
        lv_obj_t * s = label(r, F_20, s_ui.T.dim, subs[i]);
        lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 14, -8);
        lv_obj_t * c = label(r, F_ICON24, s_ui.T.dim, LV_SYMBOL_RIGHT);
        lv_obj_align(c, LV_ALIGN_RIGHT_MID, -14, 0);
        lv_obj_add_event_cb(r, menu_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)dst[i]);
    }
}

/* ============================================================ priority */

static void pr_tab_cb(lv_event_t * e)
{
    s_ui.pr_tab = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    refresh_all();
}

static void pr_room_cb(lv_event_t * e)
{
    if (1u == s_ui.pr_tab) { return; }      /* read-only in Active Rooms */
    int i = (int)(uintptr_t)lv_event_get_user_data(e);
    s_ui.pr_sel[i] = !s_ui.pr_sel[i];
    refresh_all();
}

static void pr_save_cb(lv_event_t * e)
{
    (void)e;
    s_cfg.priority_active = (1u == s_ui.pr_tab);
    if (!s_cfg.priority_active) {
        for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) {
            send_cmd(TSTAT_CMD_SET_SENSOR_PART, (uint8_t)i, s_ui.pr_sel[i] ? 1 : 0, 0);
        }
    }
    show_page(PG_MENU);
}

static void build_priority(lv_obj_t * pg)
{
    static const char * const tabs[2] = { "Selected Rooms", "Active Rooms" };
    subpage_header(pg, "Priority");
    segmented(pg, SCR_W / 2, 76, tabs, 2, 210, s_ui.pr_seg, pr_tab_cb);
    s_ui.pr_hint = label(pg, F_20, s_ui.T.dim, "");
    lv_obj_align(s_ui.pr_hint, LV_ALIGN_TOP_MID, 0, 142);

    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        lv_obj_t * c = card(pg, 30 + i * 250, 178, 236, 180, pr_room_cb, (void *)(uintptr_t)i);
        lv_obj_t * n = label(c, F_28, s_ui.T.fg, (0 == i) ? "Living Room" : (1 == i) ? "Bedroom" : "Kitchen");
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        s_ui.pr_temp[i] = label(c, F_32, s_ui.T.fg, "--");
        lv_obj_align(s_ui.pr_temp[i], LV_ALIGN_LEFT_MID, 0, 6);
        s_ui.pr_occ[i] = label(c, F_20, s_ui.T.dim, "");
        lv_obj_align(s_ui.pr_occ[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
        s_ui.pr_card[i] = c;
    }
    (void)pill(pg, SCR_W / 2 - 110, 396, 220, 60, "Save", true, F_28, pr_save_cb, NULL);
}

static void refresh_priority(const tstat_model_t * m)
{
    char b[64];
    segmented_set(s_ui.pr_seg, 2, s_ui.pr_tab);
    int n = 0;
    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        bool sel = (0u == s_ui.pr_tab) ? s_ui.pr_sel[i] : (0u != m->sensors[i].occupied);
        card_select(s_ui.pr_card[i], sel);
        ui_set_opa(s_ui.pr_card[i], sel ? LV_OPA_COVER : LV_OPA_60, 0);
        if (sel) { n++; }
        lv_snprintf(b, sizeof(b), "%d" DEG "  " LV_SYMBOL_TINT " %u%%", (int)disp_whole(m->sensors[i].temp), (unsigned)m->humidity);
        set_text(s_ui.pr_temp[i], b);
        set_text(s_ui.pr_occ[i], m->sensors[i].occupied ? "Activity sensed" : "No activity");
    }
    if (0u == s_ui.pr_tab) {
        lv_snprintf(b, sizeof(b), "%d selected - tap rooms to average for comfort", n);
    } else {
        lv_snprintf(b, sizeof(b), "Rooms with sensed activity are prioritized automatically");
    }
    set_text(s_ui.pr_hint, b);
}

/** Active Rooms mode: follow occupancy (falls back to all rooms if none active) */
static void priority_auto_apply(const tstat_model_t * m)
{
    if (!s_cfg.priority_active) { return; }
    int occ = 0;
    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) { if (m->sensors[i].occupied) { occ++; } }
    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        uint8_t want = (0 == occ) ? 1u : m->sensors[i].occupied;
        if (want != m->sensors[i].participating) {
            send_cmd(TSTAT_CMD_SET_SENSOR_PART, (uint8_t)i, (int16_t)want, 0);
        }
    }
}

/* ========================================================= ventilation */

static void v_tab_cb(lv_event_t * e)
{
    int t = (int)(uintptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 3; i++) { set_hidden(s_ui.v_tab[i], i != t); }
    segmented_set(s_ui.v_seg, 3, t);
}

static void v_card_cb(lv_event_t * e)
{
    s_ui.v_sel = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    refresh_all();
}

static void v_timer_cb(lv_event_t * e)
{
    uint16_t mins = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    s_cfg.vent_timer_end_s = s_snap.model.clock_s + (uint32_t)mins * 60u;
    refresh_all();
}

static void v_save_cb(lv_event_t * e)
{
    (void)e;
    s_cfg.vent_auto = s_ui.v_sel;
    show_page(PG_MENU);
}

static uint32_t vent_timer_left_s(const tstat_model_t * m)
{
    if (0u == s_cfg.vent_timer_end_s) { return 0u; }
    if (m->clock_s >= s_cfg.vent_timer_end_s) { s_cfg.vent_timer_end_s = 0u; return 0u; }
    return s_cfg.vent_timer_end_s - m->clock_s;
}

static void build_vent(lv_obj_t * pg)
{
    static const char * const tabs[3] = { "Mode", "Timer", "Options" };
    char b[16];
    subpage_header(pg, "Ventilation");
    segmented(pg, SCR_W / 2, 76, tabs, 3, 160, s_ui.v_seg, v_tab_cb);
    s_ui.v_status = label(pg, F_24, s_ui.T.fg, "");
    lv_obj_align(s_ui.v_status, LV_ALIGN_TOP_MID, 0, 140);

    for (int i = 0; i < 3; i++) { s_ui.v_tab[i] = box(pg, 0, 178, SCR_W, 206); }

    s_ui.v_card[1] = option_card(s_ui.v_tab[0], SCR_W / 2 - 320, 0, 310, 200, LV_SYMBOL_HOME, "Auto",
        "Ventilation runs as programmed by the installer", v_card_cb, (void *)(uintptr_t)1u);
    s_ui.v_card[0] = option_card(s_ui.v_tab[0], SCR_W / 2 + 10, 0, 310, 200, LV_SYMBOL_CLOSE, "Off",
        "Ventilation remains off unless turned on using the timer", v_card_cb, (void *)(uintptr_t)0u);

    lv_obj_t * tl = label(s_ui.v_tab[1], F_24, s_ui.T.fg, "Run ventilation now for:");
    lv_obj_align(tl, LV_ALIGN_TOP_MID, 0, 14);
    for (int i = 0; i < 3; i++) {
        lv_snprintf(b, sizeof(b), "%u min", (unsigned)s_vent_min[i]);
        s_ui.v_timer_btn[i] = pill(s_ui.v_tab[1], SCR_W / 2 - 280 + i * 190, 64, 180, 60, b, false, F_28,
                                   v_timer_cb, (void *)(uintptr_t)s_vent_min[i]);
    }
    s_ui.v_timer_lbl = label(s_ui.v_tab[1], F_24, s_ui.T.dim, "");
    lv_obj_align(s_ui.v_timer_lbl, LV_ALIGN_TOP_MID, 0, 146);

    lv_obj_t * ol = label(s_ui.v_tab[2], F_24, s_ui.T.dim,
        "Ventilation equipment, run time per hour and lockouts\nare configured by your installer.");
    lv_obj_set_style_text_align(ol, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ol, LV_ALIGN_TOP_MID, 0, 40);

    set_hidden(s_ui.v_tab[1], true);
    set_hidden(s_ui.v_tab[2], true);
    segmented_set(s_ui.v_seg, 3, 0);
    (void)pill(pg, SCR_W / 2 - 110, 400, 220, 60, "Save", true, F_28, v_save_cb, NULL);
}

static void refresh_vent(const tstat_model_t * m)
{
    char b[64];
    uint32_t left = vent_timer_left_s(m);
    card_select(s_ui.v_card[1], 1u == s_ui.v_sel);
    card_select(s_ui.v_card[0], 0u == s_ui.v_sel);
    if (left > 0u) {
        lv_snprintf(b, sizeof(b), "Status: Ventilating (timer %u min left)", (unsigned)((left + 59u) / 60u));
    } else if (s_cfg.vent_auto && m->out_g) {
        lv_snprintf(b, sizeof(b), "Status: Ventilating");
    } else if (s_cfg.vent_auto) {
        lv_snprintf(b, sizeof(b), "Status: Auto - idle");
    } else {
        lv_snprintf(b, sizeof(b), "Status: Off");
    }
    set_text(s_ui.v_status, b);
    if (left > 0u) { lv_snprintf(b, sizeof(b), "Timer running: %u min remaining", (unsigned)((left + 59u) / 60u)); }
    else           { b[0] = '\0'; }
    set_text(s_ui.v_timer_lbl, b);
}

/* ========================================================== idle views */

static void iv_build(idle_view_t * v, lv_obj_t * parent, int32_t w, int32_t h, uint8_t style, bool big)
{
    const theme_t * T = &s_ui.T;
    memset(v, 0, sizeof(*v));
    v->style = style;
    v->big   = big;
    v->root  = box(parent, 0, 0, w, h);
    lv_obj_set_style_bg_opa(v->root, LV_OPA_COVER, 0);
    ui_set_bg_color(v->root, s_cfg.dark ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(v->root, big ? 0 : 12, 0);
    lv_obj_set_style_clip_corner(v->root, !big, 0);
    lv_obj_remove_flag(v->root, LV_OBJ_FLAG_CLICKABLE);

    if (IDLE_DASH == style) {
        /* Each value is centred in its own half (0..w/2 and w/2..w), so both are
         * symmetric about the centre divider. "12:59" at 150 px is 372 px wide,
         * which fits the 400 px half. */
        const int32_t hy = big ? 56 : 8, hh = big ? 220 : 72;
        lv_obj_t * lh = box(v->root, 0, hy, w / 2, hh);
        lv_obj_t * rh = box(v->root, w / 2, hy, w / 2, hh);
        v->temp = label(lh, big ? F_BIG : F_48, T->fg, "--");
        lv_obj_center(v->temp);
        v->time = label(rh, big ? F_BIG : F_48, T->fg, "--:--");
        lv_obj_center(v->time);
        lv_obj_t * sep = box(v->root, w / 2 - 1, big ? 76 : 16, 2, big ? 180 : 56);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        ui_set_bg_color(sep, T->line, 0);

        static const char * const cap[4] = { "Set to", "Fan", "Outside", "Humidity" };
        const int32_t tw = w / 4;
        for (int i = 0; i < 4; i++) {
            lv_obj_t * c = label(v->root, big ? F_28 : F_16, T->accent, cap[i]);
            lv_obj_set_width(c, tw);
            lv_obj_set_style_text_align(c, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(c, i * tw, big ? 322 : 90);
            v->tile_val[i] = label(v->root, big ? F_40 : F_20, T->fg, "--");
            lv_obj_set_width(v->tile_val[i], tw);
            lv_obj_set_style_text_align(v->tile_val[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(v->tile_val[i], i * tw, big ? 362 : 112);
        }
    } else if (IDLE_CLOCK == style) {
        int32_t d = big ? 380 : 120;
        lv_obj_t * face = box(v->root, (w - d) / 2, (h - d) / 2, d, d);
        lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
        ui_set_border_width(face, big ? 3 : 2, 0);
        ui_set_border_color(face, T->line, 0);
        for (int k = 0; k < 12; k++) {          /* hour dots */
            int32_t r  = d / 2 - (big ? 20 : 8);
            int32_t dx = (r * lv_trigo_sin((int16_t)(k * 30))) / 32768;
            int32_t dy = (r * lv_trigo_cos((int16_t)(k * 30))) / 32768;
            int32_t ds = (0 == (k % 3)) ? (big ? 12 : 5) : (big ? 6 : 3);
            lv_obj_t * dot = box(face, d / 2 + dx - ds / 2, d / 2 - dy - ds / 2, ds, ds);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            ui_set_bg_color(dot, T->dim, 0);
        }
        v->cx = d / 2; v->cy = d / 2;
        v->len_h = d * 26 / 100; v->len_m = d * 38 / 100;
        v->hand_h = lv_line_create(face);
        v->hand_m = lv_line_create(face);
        lv_obj_set_style_line_width(v->hand_h, big ? 12 : 4, 0);
        lv_obj_set_style_line_width(v->hand_m, big ? 7 : 3, 0);
        lv_obj_set_style_line_color(v->hand_h, T->fg, 0);
        lv_obj_set_style_line_color(v->hand_m, T->accent, 0);
        lv_obj_set_style_line_rounded(v->hand_h, true, 0);
        lv_obj_set_style_line_rounded(v->hand_m, true, 0);
        lv_obj_set_pos(v->hand_h, 0, 0);
        lv_obj_set_pos(v->hand_m, 0, 0);
        v->temp  = label(v->root, big ? F_48 : F_20, T->fg, "");
        lv_obj_align(v->temp, LV_ALIGN_LEFT_MID, big ? 40 : 10, 0);
        v->line3 = label(v->root, big ? F_48 : F_20, T->fg, "");
        lv_obj_align(v->line3, LV_ALIGN_RIGHT_MID, big ? -50 : -10, 0);
    } else {
        lv_obj_t * col = lv_obj_create(v->root);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(col, big ? 12 : 4, 0);
        lv_obj_align(col, LV_ALIGN_LEFT_MID, big ? 80 : 24, 0);
        v->time  = label(col, big ? F_64 : F_28, T->fg, "--:--");
        v->temp  = label(col, big ? F_64 : F_28, T->accent, "--");
        v->line3 = label(col, big ? F_48 : F_24, T->fg, "");
    }
}

static void iv_update(idle_view_t * v, const tstat_model_t * m)
{
    char b[48], t[16];
    unsigned h12, mi, h24; bool pm;
    if (NULL == v->root) { return; }
    clock_parts(m->clock_s, &h12, &mi, &pm, &h24);

    if (IDLE_DASH == v->style) {
        lv_snprintf(b, sizeof(b), "%d" DEG, (int)disp_whole(m->indoor)); set_text(v->temp, b);
        lv_snprintf(b, sizeof(b), "%u:%02u", h12, mi);                set_text(v->time, b);
        if (TSTAT_MODE_OFF == m->mode) { lv_snprintf(b, sizeof(b), "Off"); }
        else { fmt_sp(t, sizeof(t), (TSTAT_MODE_COOL == m->mode) ? m->active_sp.cool : m->active_sp.heat); lv_snprintf(b, sizeof(b), "%s", t); }
        set_text(v->tile_val[0], b);
        set_text(v->tile_val[1], (TSTAT_FAN_ON == m->fan) ? "On" : "Auto");
        fmt_deg(b, sizeof(b), m->outdoor);                        set_text(v->tile_val[2], b);
        lv_snprintf(b, sizeof(b), "%u%%", (unsigned)m->humidity); set_text(v->tile_val[3], b);
    } else if (IDLE_CLOCK == v->style) {
        int32_t am = (int32_t)mi * 6;
        int32_t ah = (int32_t)(h24 % 12u) * 30 + (int32_t)mi / 2;
        v->pts_m[0].x = v->cx; v->pts_m[0].y = v->cy;
        v->pts_m[1].x = v->cx + (v->len_m * lv_trigo_sin((int16_t)am)) / 32768;
        v->pts_m[1].y = v->cy - (v->len_m * lv_trigo_cos((int16_t)am)) / 32768;
        v->pts_h[0].x = v->cx; v->pts_h[0].y = v->cy;
        v->pts_h[1].x = v->cx + (v->len_h * lv_trigo_sin((int16_t)ah)) / 32768;
        v->pts_h[1].y = v->cy - (v->len_h * lv_trigo_cos((int16_t)ah)) / 32768;
        lv_line_set_points(v->hand_m, v->pts_m, 2);
        lv_line_set_points(v->hand_h, v->pts_h, 2);
        lv_snprintf(b, sizeof(b), LV_SYMBOL_HOME " %d" DEG, (int)disp_whole(m->indoor)); set_text(v->temp, b);
        fmt_deg(t, sizeof(t), m->outdoor);
        set_text(v->line3, t);
    } else {
        lv_snprintf(b, sizeof(b), "%u:%02u %s", h12, mi, pm ? "PM" : "AM");          set_text(v->time, b);
        lv_snprintf(b, sizeof(b), "%d" DEG " Inside", (int)disp_whole(m->indoor));   set_text(v->temp, b);
        fmt_deg(t, sizeof(t), m->outdoor);
        lv_snprintf(b, sizeof(b), "%s %s", t, weather_text(m));                       set_text(v->line3, b);
    }
}

static void overlay_hide(void)
{
    if (!s_ui.idle_on) { return; }
    s_ui.idle_on = false;
    set_hidden(s_ui.overlay, true);
    thermostat_hal_set_backlight(s_cfg.brightness);
}

static void overlay_build(void)
{
    if (NULL != s_ui.overlay) { lv_obj_delete(s_ui.overlay); }
    s_ui.overlay = box(lv_layer_top(), 0, 0, SCR_W, SCR_H);
    lv_obj_add_flag(s_ui.overlay, LV_OBJ_FLAG_CLICKABLE);
    iv_build(&s_ui.iv_overlay, s_ui.overlay, SCR_W, SCR_H, s_cfg.idle_style, true);
    set_hidden(s_ui.overlay, true);
    s_ui.idle_on = false;
}

static void overlay_click_cb(lv_event_t * e);

static void overlay_restore(void * p)
{
    (void)p;
    overlay_build();                         /* back to the saved style */
    lv_obj_add_event_cb(s_ui.overlay, overlay_click_cb, LV_EVENT_CLICKED, NULL);
}

static void overlay_click_cb(lv_event_t * e)
{
    (void)e;
    overlay_hide();
    lv_display_trigger_activity(NULL);
    if (s_ui.overlay_preview) {
        s_ui.overlay_preview = false;
        lv_async_call(overlay_restore, NULL); /* never delete the object inside its own event */
    }
}

static void overlay_show(void)
{
    if (s_ui.idle_on) { return; }
    s_ui.idle_on = true;
    if (s_have_snap) { iv_update(&s_ui.iv_overlay, &s_snap.model); }
    set_hidden(s_ui.overlay, false);
    lv_obj_move_foreground(s_ui.overlay);
    thermostat_hal_set_backlight((uint8_t)((s_cfg.brightness > 40u) ? 40u : s_cfg.brightness));
}

/* ======================================================= idle chooser */

static void ip_rebuild_preview(void)
{
    if (NULL != s_ui.iv_preview.root) { lv_obj_delete(s_ui.iv_preview.root); }
    lv_color_t keep = s_ui.T.accent;
    s_ui.T.accent = lv_color_hex(s_accents[s_ui.ip_accent]);
    iv_build(&s_ui.iv_preview, s_ui.ip_holder, 400, 150, s_ui.ip_style, false);
    s_ui.T.accent = keep;
    set_text(lv_obj_get_child(s_ui.ip_left, 0),  s_idle_names[(s_ui.ip_style + IDLE_STYLE_COUNT - 1u) % IDLE_STYLE_COUNT]);
    set_text(lv_obj_get_child(s_ui.ip_right, 0), s_idle_names[(s_ui.ip_style + 1u) % IDLE_STYLE_COUNT]);
    for (int i = 0; i < ACCENT_COUNT; i++) {
        ui_set_border_width(s_ui.ip_dot[i], (i == (int)s_ui.ip_accent) ? 3 : 0, 0);
    }
    for (int i = 0; i < IDLE_STYLE_COUNT; i++) {
        ui_set_bg_color(s_ui.ip_style_dot[i], (i == (int)s_ui.ip_style) ? s_ui.T.fg : s_ui.T.line, 0);
    }
    if (s_have_snap) { iv_update(&s_ui.iv_preview, &s_snap.model); }
}

static void ip_nav_cb(lv_event_t * e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    s_ui.ip_style = (uint8_t)((s_ui.ip_style + IDLE_STYLE_COUNT + dir) % IDLE_STYLE_COUNT);
    ip_rebuild_preview();
}

static void ip_dot_cb(lv_event_t * e)
{
    s_ui.ip_accent = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ip_rebuild_preview();
}

static void async_rebuild(void * p) { (void)p; build_all(); }

static void ip_save_cb(lv_event_t * e)
{
    (void)e;
    s_cfg.idle_style = s_ui.ip_style;
    s_cfg.accent     = s_ui.ip_accent;
    s_cfg.page       = PG_MENU;
    lv_async_call(async_rebuild, NULL);       /* accent is used across the UI */
}

static void ip_preview_cb(lv_event_t * e)
{
    (void)e;
    uint8_t ks = s_cfg.idle_style, ka = s_cfg.accent;
    s_cfg.idle_style = s_ui.ip_style;
    s_cfg.accent     = s_ui.ip_accent;
    lv_color_t keep = s_ui.T.accent;
    s_ui.T.accent = lv_color_hex(s_accents[s_ui.ip_accent]);
    overlay_build();
    lv_obj_add_event_cb(s_ui.overlay, overlay_click_cb, LV_EVENT_CLICKED, NULL);
    s_ui.T.accent = keep;
    s_cfg.idle_style = ks;
    s_cfg.accent     = ka;
    s_ui.overlay_preview = true;
    overlay_show();
}

static void build_idlepick(lv_obj_t * pg)
{
    subpage_header(pg, "Choose Idle Screen");
    const int32_t py = 84;

    s_ui.ip_left  = box(pg, 24, py + 20, 160, 110);
    s_ui.ip_right = box(pg, SCR_W - 184, py + 20, 160, 110);
    lv_obj_t * sides[2] = { s_ui.ip_left, s_ui.ip_right };
    for (int i = 0; i < 2; i++) {
        lv_obj_t * s = sides[i];
        lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(s, 12, 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        ui_set_bg_color(s, s_ui.T.card, 0);
        lv_obj_t * l = label(s, F_20, s_ui.T.dim, "");
        lv_obj_center(l);
        lv_obj_add_event_cb(s, ip_nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)((0 == i) ? -1 : 1));
    }
    s_ui.ip_holder = box(pg, SCR_W / 2 - 200, py, 400, 150);
    ui_set_border_width(s_ui.ip_holder, 2, 0);
    ui_set_border_color(s_ui.ip_holder, s_ui.T.line, 0);
    lv_obj_set_style_radius(s_ui.ip_holder, 12, 0);

    for (int i = 0; i < IDLE_STYLE_COUNT; i++) {
        lv_obj_t * d = box(pg, SCR_W / 2 - 37 + i * 30, py + 164, 14, 14);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        s_ui.ip_style_dot[i] = d;
    }
    lv_obj_t * cl = label(pg, F_20, s_ui.T.dim, "Accent color");
    lv_obj_align(cl, LV_ALIGN_TOP_MID, 0, py + 188);
    for (int i = 0; i < ACCENT_COUNT; i++) {
        lv_obj_t * d = box(pg, SCR_W / 2 - (ACCENT_COUNT * 62) / 2 + i * 62 + 6, py + 218, 50, 50);
        lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        ui_set_bg_color(d, lv_color_hex(s_accents[i]), 0);
        ui_set_border_color(d, s_ui.T.fg, 0);
        lv_obj_add_event_cb(d, ip_dot_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_ui.ip_dot[i] = d;
    }
    (void)pill(pg, SCR_W / 2 - 230, 404, 210, 60, "Preview", false, F_28, ip_preview_cb, NULL);
    (void)pill(pg, SCR_W / 2 + 20,  404, 210, 60, "Save",    true,  F_28, ip_save_cb,    NULL);
}

/* ===================================================== display & units */

static void d_theme_cb(lv_event_t * e)
{
    bool dark = (0u == (uintptr_t)lv_event_get_user_data(e));
    if (dark == s_cfg.dark) { return; }
    s_cfg.dark = dark;
    s_cfg.page = PG_DISPLAY;
    lv_async_call(async_rebuild, NULL);
}

static void d_unit_cb(lv_event_t * e)
{
    s_cfg.celsius = (0u != (uintptr_t)lv_event_get_user_data(e));
    s_ui.pend_valid = false;
    refresh_all();
}

static void d_to_cb(lv_event_t * e)
{
    s_cfg.idle_timeout_s = s_timeouts[(uintptr_t)lv_event_get_user_data(e)];
    refresh_all();
}

static void d_speed_cb(lv_event_t * e)
{
    send_cmd(TSTAT_CMD_SET_SIM_SPEED, 0u, (int16_t)s_speeds[(uintptr_t)lv_event_get_user_data(e)], 0);
}

static void d_bright_cb(lv_event_t * e)
{
    char b[8];
    s_cfg.brightness = (uint8_t)lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    lv_snprintf(b, sizeof(b), "%u%%", (unsigned)s_cfg.brightness);
    set_text(s_ui.d_bright_lbl, b);
    thermostat_hal_set_backlight(s_cfg.brightness);
}

static lv_obj_t * row(lv_obj_t * pg, int32_t y, const char * title)
{
    lv_obj_t * r = box(pg, 50, y, SCR_W - 100, 64);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    ui_set_border_width(r, 1, 0);
    ui_set_border_color(r, s_ui.T.line, 0);
    lv_obj_t * l = label(r, F_24, s_ui.T.fg, title);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    return r;
}

static void row_buttons(lv_obj_t * r, const char * const * names, int n, lv_obj_t ** out, lv_event_cb_t cb)
{
    for (int i = 0; i < n; i++) {
        lv_obj_t * b = pill(r, 0, 0, 104, 50, names[i], false, F_24, cb, (void *)(uintptr_t)i);
        lv_obj_align(b, LV_ALIGN_RIGHT_MID, -(n - 1 - i) * 112, 0);
        out[i] = b;
    }
}

static void pill_select(lv_obj_t * b, bool sel)
{
    lv_obj_set_style_bg_opa(b, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    ui_set_text_color(lv_obj_get_child(b, 0), sel ? s_ui.T.bg : s_ui.T.fg, 0);
}

static void build_display(lv_obj_t * pg)
{
    static const char * const th[2] = { "Dark", "Light" };
    static const char * const un[2] = { DEG "F", DEG "C" };
    static const char * const to[4] = { "15 s", "30 s", "60 s", "Never" };
    static const char * const sp[3] = { "1x", "10x", "60x" };
    char b[8];
    subpage_header(pg, "Display & Units");
    row_buttons(row(pg, 76,  "Theme"),              th, 2, s_ui.d_theme, d_theme_cb);
    row_buttons(row(pg, 142, "Temperature units"),  un, 2, s_ui.d_unit,  d_unit_cb);
    row_buttons(row(pg, 208, "Idle screen after"),  to, 4, s_ui.d_to,    d_to_cb);
    row_buttons(row(pg, 274, "Demo speed"),         sp, 3, s_ui.d_speed, d_speed_cb);
    lv_obj_t * r = row(pg, 340, "Brightness");
    lv_obj_t * s = lv_slider_create(r);
    lv_obj_set_width(s, 300);
    lv_slider_set_range(s, 10, 100);
    lv_slider_set_value(s, s_cfg.brightness, LV_ANIM_OFF);
    lv_obj_align(s, LV_ALIGN_RIGHT_MID, -90, 0);
    ui_set_bg_color(s, s_ui.T.line, LV_PART_MAIN);
    ui_set_bg_color(s, s_ui.T.accent, LV_PART_INDICATOR);
    ui_set_bg_color(s, s_ui.T.fg, LV_PART_KNOB);
    lv_obj_add_event_cb(s, d_bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_ui.d_bright_lbl = label(r, F_24, s_ui.T.dim, "");
    lv_obj_align(s_ui.d_bright_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_snprintf(b, sizeof(b), "%u%%", (unsigned)s_cfg.brightness);
    lv_label_set_text(s_ui.d_bright_lbl, b);
}

static void refresh_display(uint16_t speed)
{
    pill_select(s_ui.d_theme[0], s_cfg.dark);
    pill_select(s_ui.d_theme[1], !s_cfg.dark);
    pill_select(s_ui.d_unit[0], !s_cfg.celsius);
    pill_select(s_ui.d_unit[1], s_cfg.celsius);
    for (int i = 0; i < 4; i++) { pill_select(s_ui.d_to[i], s_cfg.idle_timeout_s == s_timeouts[i]); }
    for (int i = 0; i < 3; i++) { pill_select(s_ui.d_speed[i], speed == s_speeds[i]); }
}

/* ================================================== equipment status */

#if LV_USE_PERF_MONITOR
static void perf_cb(lv_observer_t * o, lv_subject_t * subj)
{
    char b[40];
    const lv_sysmon_perf_info_t * p = lv_subject_get_pointer(subj);
    lv_snprintf(b, sizeof(b), "%d FPS  |  CPU %d%%", (int)p->calculated.fps, (int)p->calculated.cpu);
    set_text(lv_observer_get_target_obj(o), b);
}
#endif

static void build_status(lv_obj_t * pg)
{
    static const char * const names[8] = { "System mode", "Equipment", "Outputs (W1 / Y1 / G)", "Compressor protection",
                                            "Schedule", "Indoor / Outdoor", "Ventilation", "Platform" };
    char b[96];
    subpage_header(pg, "Equipment Status");
    for (int i = 0; i < 8; i++) {
        lv_obj_t * r = box(pg, 50, 72 + i * 46, SCR_W - 100, 44);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
        ui_set_border_width(r, 1, 0);
        ui_set_border_color(r, s_ui.T.line, 0);
        lv_obj_t * n = label(r, F_20, s_ui.T.dim, names[i]);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
        s_ui.st_val[i] = label(r, F_20, s_ui.T.fg, "");
        lv_obj_align(s_ui.st_val[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }
    lv_snprintf(b, sizeof(b), "RZ/A2M  |  FreeRTOS %s  |  LVGL %d.%d.%d", thermostat_service_rtos_version(),
                LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_label_set_text(s_ui.st_val[7], b);
    s_ui.st_perf = label(pg, F_16, s_ui.T.accent, "");
    lv_obj_align(s_ui.st_perf, LV_ALIGN_BOTTOM_MID, 0, -8);
#if LV_USE_PERF_MONITOR
    lv_subject_add_observer_obj(&lv_display_get_default()->perf_sysmon_backend.subject, perf_cb, s_ui.st_perf, NULL);
#else
    lv_label_set_text(s_ui.st_perf, "Enable LV_USE_PERF_MONITOR for FPS / CPU");
#endif
}

static void refresh_status(const tstat_model_t * m)
{
    char b[64], t1[16], t2[16];
    static const char * const hv[] = { "Idle", "Heating", "Cooling", "Fan only",
                                       "Waiting for Equipment (heat)", "Waiting for Equipment (cool)" };
    set_text(s_ui.st_val[0], tstat_mode_name(m->mode));
    set_text(s_ui.st_val[1], hv[m->hvac]);
    lv_snprintf(b, sizeof(b), "%s  /  %s  /  %s", m->out_w1 ? "ON" : "off", m->out_y1 ? "ON" : "off", m->out_g ? "ON" : "off");
    set_text(s_ui.st_val[2], b);
    if (m->protect_remaining_s) {
        lv_snprintf(b, sizeof(b), "Active - %u:%02u remaining", (unsigned)(m->protect_remaining_s / 60u), (unsigned)(m->protect_remaining_s % 60u));
    } else {
        lv_snprintf(b, sizeof(b), "Ready (min run %u min / min off %u min)", (unsigned)(m->min_on_s / 60u), (unsigned)(m->min_off_s / 60u));
    }
    set_text(s_ui.st_val[3], b);
    uint16_t nx = tstat_model_next_transition_min(m);
    lv_snprintf(b, sizeof(b), "%s%s, next change %02u:%02u", tstat_comfort_name(m->active_comfort), m->hold ? " (hold)" : "",
                (unsigned)(nx / 60u), (unsigned)(nx % 60u));
    set_text(s_ui.st_val[4], b);
    fmt_deg(t1, sizeof(t1), m->indoor); fmt_deg(t2, sizeof(t2), m->outdoor);
    lv_snprintf(b, sizeof(b), "%s  /  %s  |  %u%% RH  |  IAQ %s", t1, t2, (unsigned)m->humidity, iaq_text(m));
    set_text(s_ui.st_val[5], b);
    set_text(s_ui.st_val[6], (vent_timer_left_s(m) > 0u) ? "Timer running" : (s_cfg.vent_auto ? "Auto" : "Off"));
}

/* ====================================================== refresh + timer */

static void refresh_all(void)
{
    const tstat_model_t * m = &s_snap.model;
    char b[16];
    if (s_ui.pend_valid && (lv_tick_elaps(s_ui.pend_tick) > PENDING_MS)) { s_ui.pend_valid = false; }

    if (s_snap.sim_speed > 1u) { lv_snprintf(b, sizeof(b), "DEMO %ux", (unsigned)s_snap.sim_speed); }
    else                       { b[0] = '\0'; }
    set_text(s_ui.badge, b);

    switch (s_cfg.page) {
        case PG_HOME:     refresh_home(m);                    break;
        case PG_MODE:     refresh_mode(m);                    break;
        case PG_FAN:      refresh_fan(m);                     break;
        case PG_PRIORITY: refresh_priority(m);                break;
        case PG_VENT:     refresh_vent(m);                    break;
        case PG_IDLEPICK: iv_update(&s_ui.iv_preview, m);     break;
        case PG_DISPLAY:  refresh_display(s_snap.sim_speed);  break;
        case PG_STATUS:   refresh_status(m);                  break;
        default: break;
    }
    if (s_ui.idle_on) { iv_update(&s_ui.iv_overlay, m); }
}

static void timer_cb(lv_timer_t * t)
{
    (void)t;
    if (thermostat_service_get_snapshot(&s_snap, &s_seq)) {
        s_have_snap = true;
        priority_auto_apply(&s_snap.model);
        refresh_all();
    }
    /* idle screen */
    if ((s_cfg.idle_timeout_s > 0u) && !s_ui.idle_on && (PG_IDLEPICK != s_cfg.page) &&
        (lv_display_get_inactive_time(NULL) > (uint32_t)s_cfg.idle_timeout_s * 1000u)) {
        if (PG_HOME != s_cfg.page) { show_page(PG_HOME); }
        overlay_show();
    }
}

/* ============================================================ build */

static void build_all(void)
{
    page_t keep_page = s_cfg.page;
    if (NULL != s_ui.overlay) { lv_obj_delete(s_ui.overlay); }
    memset(&s_ui, 0, sizeof(s_ui));
    theme_init();

    s_ui.scr = lv_screen_active();
    lv_obj_clean(s_ui.scr);
    lv_obj_remove_style_all(s_ui.scr);
    lv_obj_set_style_bg_opa(s_ui.scr, LV_OPA_COVER, 0);
    ui_set_bg_color(s_ui.scr, s_ui.T.bg, 0);
    lv_obj_remove_flag(s_ui.scr, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < PG_COUNT; i++) { s_ui.page[i] = box(s_ui.scr, 0, 0, SCR_W, SCR_H); }

    /* nav rail (drawn above pages) */
    s_ui.nav = box(s_ui.scr, 0, 0, NAV_W, SCR_H);
    s_ui.nav_item[0] = nav_item(s_ui.nav, 0, LV_SYMBOL_HOME,    "Home");
    s_ui.nav_item[1] = nav_item(s_ui.nav, 1, NULL,              "Mode");
    s_ui.nav_item[2] = nav_item(s_ui.nav, 2, LV_SYMBOL_REFRESH, "Fan");
    s_ui.nav_item[3] = nav_item(s_ui.nav, 3, LV_SYMBOL_BARS,    "Menu");

    s_ui.badge = label(s_ui.scr, F_16, s_ui.T.accent, "");
    lv_obj_align(s_ui.badge, LV_ALIGN_TOP_RIGHT, -14, 8);

    build_home(s_ui.page[PG_HOME]);
    build_mode(s_ui.page[PG_MODE]);
    build_fan(s_ui.page[PG_FAN]);
    build_menu(s_ui.page[PG_MENU]);
    build_priority(s_ui.page[PG_PRIORITY]);
    build_vent(s_ui.page[PG_VENT]);
    build_idlepick(s_ui.page[PG_IDLEPICK]);
    build_display(s_ui.page[PG_DISPLAY]);
    build_status(s_ui.page[PG_STATUS]);

    overlay_build();
    lv_obj_add_event_cb(s_ui.overlay, overlay_click_cb, LV_EVENT_CLICKED, NULL);

    s_ui.ip_style  = s_cfg.idle_style;
    s_ui.ip_accent = s_cfg.accent;
    s_ui.v_sel     = s_cfg.vent_auto;
    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) { s_ui.pr_sel[i] = true; }
    s_ui.pr_tab = s_cfg.priority_active ? 1u : 0u;
    ip_rebuild_preview();

    show_page(keep_page);
}

void thermostat_ui_elite_create(void)
{
    build_all();
    thermostat_hal_set_backlight(s_cfg.brightness);
    if (NULL == s_timer) { s_timer = lv_timer_create(timer_cb, REFRESH_MS, NULL); }
    timer_cb(NULL);
}
