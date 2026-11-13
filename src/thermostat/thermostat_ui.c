/**
 * @file thermostat_ui.c
 * @brief ecobee-style smart-thermostat HMI for RZ/A2M (LVGL 9, 800 x 480, RGB565).
 *
 * Pages: Home (dial) | Mode | Schedule | Sensors | Energy | Settings
 *
 * Design rules used for the RZ/A2M software renderer (DIRECT render mode):
 *  - one screen, pages are shown/hidden (no screen-load animations, no full redraws)
 *  - no shadows, gradients, blur or opacity layers
 *  - labels are only re-written when their text actually changes
 *  - all model access is through snapshots (see thermostat_service.h)
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "lvgl.h"
#if LV_USE_PERF_MONITOR
#include "lvgl_private.h"
#endif

#include "thermostat_ui.h"
#include "thermostat_service.h"
#include "thermostat_hal.h"

/* ====================================================================== fonts */
/* Reuse the 100 px Roboto digits already linked by the oven project
 * (glyphs: 0-9, space, degree, ':', 'h', 'm'). Set to 0 to use Montserrat 48 only. */
#ifndef THERMO_USE_ROBOTO_100
#define THERMO_USE_ROBOTO_100   (1)
#endif

#if THERMO_USE_ROBOTO_100
LV_FONT_DECLARE(Roboto_Medium_100_data)
#define FONT_HERO       (&Roboto_Medium_100_data)
#else
#define FONT_HERO       (&lv_font_montserrat_48)
#endif
#define FONT_HERO_SMALL (&lv_font_montserrat_40)
#define FONT_XL         (&lv_font_montserrat_48)
#define FONT_L          (&lv_font_montserrat_28)
#define FONT_M          (&lv_font_montserrat_20)
#define FONT_S          (&lv_font_montserrat_16)
#define FONT_XS         (&lv_font_montserrat_14)
#define FONT_XXS        (&lv_font_montserrat_12)
#define FONT_ICON       (&lv_font_montserrat_24)

/* ===================================================================== colors */
#define C_BG            (0x0B0F14)
#define C_BAR           (0x0F141B)
#define C_CARD          (0x161C24)
#define C_CARD_HI       (0x1F2731)
#define C_BTN           (0x252E3A)
#define C_TEXT          (0xF2F5F8)
#define C_TEXT_DIM      (0x8A94A3)
#define C_TEXT_MUTED    (0x5B6573)
#define C_HEAT          (0xFF6A2B)
#define C_COOL          (0x2E9BFF)
#define C_AUTO          (0xA77BFF)
#define C_OFF           (0x5B6573)
#define C_OK            (0x37C978)
#define C_HOME          (0x37C978)
#define C_AWAY          (0x5B8DB8)
#define C_SLEEP         (0x7C6CF0)
#define C_RENESAS       (0x2A54F5)

#define DEG             "\xC2\xB0"

/* ===================================================================== layout */
#define SCR_W           (800)
#define SCR_H           (480)
#define BAR_H           (44)
#define NAV_W           (84)
#define CONT_W          (SCR_W - NAV_W)
#define CONT_H          (SCR_H - BAR_H)

#define ARC_SIZE        (340)
#define ARC_X           (24)
#define ARC_Y           (30)
#define ARC_W           (18)

#define REFRESH_MS      (100u)
#define PENDING_MS      (1500u)

typedef enum {
    PAGE_HOME = 0, PAGE_MODE, PAGE_SCHED, PAGE_SENSORS, PAGE_ENERGY, PAGE_SETTINGS, PAGE_COUNT
} page_t;

#define CHART_COLS      (TSTAT_HISTORY_DAYS)
#define SEG_MAX         (TSTAT_SCHED_MAX + 1)

typedef struct {
    /* frame */
    lv_obj_t * page[PAGE_COUNT];
    lv_obj_t * nav_btn[PAGE_COUNT];
    lv_obj_t * lbl_clock;
    lv_obj_t * lbl_bar_outdoor;
    lv_obj_t * lbl_demo;
    page_t     cur_page;

    /* home */
    lv_obj_t * arc;
    lv_obj_t * lbl_state;
    lv_obj_t * lbl_hero;
    lv_obj_t * lbl_hero_frac;
    lv_obj_t * lbl_inside;
    lv_obj_t * lbl_second_sp;
    lv_obj_t * btn_minus;
    lv_obj_t * btn_plus;
    lv_obj_t * val_mode;
    lv_obj_t * sub_mode;
    lv_obj_t * val_fan;
    lv_obj_t * sub_fan;
    lv_obj_t * val_hum;
    lv_obj_t * val_out;
    lv_obj_t * lbl_comfort;
    lv_obj_t * lbl_comfort_sub;
    lv_obj_t * lbl_equip;
    lv_obj_t * btn_resume;
    lv_obj_t * comfort_dot;

    /* mode */
    lv_obj_t * mode_tile[TSTAT_MODE_COUNT];
    lv_obj_t * fan_btn[TSTAT_FAN_COUNT];
    lv_obj_t * lbl_protect;

    /* schedule */
    lv_obj_t * timeline;
    lv_obj_t * seg[SEG_MAX];
    lv_obj_t * now_marker;
    lv_obj_t * lbl_sched_now;
    lv_obj_t * lbl_cf_heat[TSTAT_COMFORT_COUNT];
    lv_obj_t * lbl_cf_cool[TSTAT_COMFORT_COUNT];
    bool       timeline_built;

    /* sensors */
    lv_obj_t * sens_temp[TSTAT_SENSOR_COUNT];
    lv_obj_t * sens_occ_dot[TSTAT_SENSOR_COUNT];
    lv_obj_t * sens_occ_lbl[TSTAT_SENSOR_COUNT];
    lv_obj_t * sens_sw[TSTAT_SENSOR_COUNT];
    lv_obj_t * lbl_sens_avg;

    /* energy */
    lv_obj_t * bar_heat[CHART_COLS];
    lv_obj_t * bar_cool[CHART_COLS];
    lv_obj_t * bar_val[CHART_COLS];
    lv_obj_t * bar_day[CHART_COLS];
    lv_obj_t * lbl_today;
    lv_obj_t * lbl_week;

    /* settings */
    lv_obj_t * unit_btn[2];
    lv_obj_t * slider_bright;
    lv_obj_t * lbl_bright;
    lv_obj_t * speed_btn[3];
    lv_obj_t * out_dot[3];
    lv_obj_t * lbl_perf;

    /* state */
    tstat_snapshot_t snap;
    uint32_t   seq;
    bool       have_snap;
    bool       celsius;
    bool       force;
    bool       pend_valid;
    tdeci_t    pend_val;
    uint32_t   pend_tick;
} ui_t;

static ui_t s_ui;

static lv_style_t st_card;
static lv_style_t st_btn;
static lv_style_t st_btn_pr;
static lv_style_t st_plain;

static const uint16_t s_speeds[3] = { 1u, 10u, 60u };
static const char * const s_days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

/* ================================================================= utilities */

static int32_t div_round(int32_t n, int32_t d)
{
    return (n >= 0) ? ((n + d / 2) / d) : -((-n + d / 2) / d);
}

static lv_color_t comfort_color(tstat_comfort_t c)
{
    switch (c) {
        case TSTAT_COMFORT_HOME:  return lv_color_hex(C_HOME);
        case TSTAT_COMFORT_AWAY:  return lv_color_hex(C_AWAY);
        default:                  return lv_color_hex(C_SLEEP);
    }
}

static lv_color_t mode_color(tstat_mode_t m)
{
    switch (m) {
        case TSTAT_MODE_HEAT: return lv_color_hex(C_HEAT);
        case TSTAT_MODE_COOL: return lv_color_hex(C_COOL);
        case TSTAT_MODE_AUTO: return lv_color_hex(C_AUTO);
        default:              return lv_color_hex(C_OFF);
    }
}

static void set_text(lv_obj_t * lbl, const char * txt)
{
    const char * cur = lv_label_get_text(lbl);
    if ((NULL == cur) || (0 != strcmp(cur, txt))) {
        lv_label_set_text(lbl, txt);
    }
}

static void set_hidden(lv_obj_t * obj, bool hidden)
{
    if (hidden) { lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); }
    else        { lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN); }
}

/** Format a temperature. F: integer ("68°"). C: one decimal ("20.5°"). */
static void fmt_temp(char * buf, size_t n, tdeci_t f, bool celsius)
{
    if (celsius) {
        int32_t c = tstat_deci_f_to_deci_c(f);
        int32_t a = (c < 0) ? -c : c;
        lv_snprintf(buf, n, "%s%d.%d" DEG, (c < 0) ? "-" : "", (int)(a / 10), (int)(a % 10));
    } else {
        lv_snprintf(buf, n, "%d" DEG, (int)div_round(f, 10));
    }
}

/** Snap a deci-F value to the display step (1 F or 0.5 C). */
static tdeci_t snap_step(tdeci_t f, bool celsius)
{
    if (celsius) {
        int32_t c = tstat_deci_f_to_deci_c(f);
        c = div_round(c, 5) * 5;
        return tstat_deci_c_to_deci_f((int16_t)c);
    }
    return (tdeci_t)(div_round(f, 10) * 10);
}

/** Step a deci-F setpoint by +/-1 display unit. */
static tdeci_t step_setpoint(tdeci_t f, int dir, bool celsius)
{
    if (celsius) {
        int32_t c = div_round(tstat_deci_f_to_deci_c(f), 5) * 5;
        return tstat_deci_c_to_deci_f((int16_t)(c + dir * 5));
    }
    return (tdeci_t)(div_round(f, 10) * 10 + dir * 10);
}

static void fmt_duration(char * buf, size_t n, uint32_t minutes)
{
    if (minutes >= 60u) { lv_snprintf(buf, n, "%uh %02um", (unsigned)(minutes / 60u), (unsigned)(minutes % 60u)); }
    else                { lv_snprintf(buf, n, "%um", (unsigned)minutes); }
}

static void send_cmd(tstat_cmd_id_t id, uint8_t a, int16_t v1, int16_t v2)
{
    tstat_cmd_t c = { id, a, v1, v2 };
    (void)thermostat_service_send(&c);
}

/* ============================================================ widget helpers */

static void styles_init(void)
{
    lv_style_init(&st_card);
    lv_style_set_bg_color(&st_card, lv_color_hex(C_CARD));
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_radius(&st_card, 16);
    lv_style_set_border_width(&st_card, 0);
    lv_style_set_pad_all(&st_card, 14);
    lv_style_set_text_color(&st_card, lv_color_hex(C_TEXT));

    lv_style_init(&st_btn);
    lv_style_set_bg_color(&st_btn, lv_color_hex(C_BTN));
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_radius(&st_btn, 12);
    lv_style_set_border_width(&st_btn, 0);
    lv_style_set_text_color(&st_btn, lv_color_hex(C_TEXT));

    lv_style_init(&st_btn_pr);
    lv_style_set_bg_color(&st_btn_pr, lv_color_hex(C_CARD_HI));

    lv_style_init(&st_plain);
    lv_style_set_bg_opa(&st_plain, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_plain, 0);
    lv_style_set_pad_all(&st_plain, 0);
    lv_style_set_radius(&st_plain, 0);
}

static lv_obj_t * mk_box(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, &st_plain, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t * mk_card(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, &st_card, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t * mk_label(lv_obj_t * parent, const lv_font_t * font, uint32_t color, const char * txt)
{
    lv_obj_t * l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

static lv_obj_t * mk_button(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h,
                            const lv_font_t * font, const char * txt,
                            lv_event_cb_t cb, void * user)
{
    lv_obj_t * b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &st_btn, 0);
    lv_obj_add_style(b, &st_btn_pr, LV_STATE_PRESSED);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if (NULL != txt) {
        lv_obj_t * l = mk_label(b, font, C_TEXT, txt);
        lv_obj_center(l);
    }
    if (NULL != cb) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    }
    return b;
}

static lv_obj_t * mk_dot(lv_obj_t * parent, int32_t x, int32_t y, int32_t d, uint32_t color)
{
    lv_obj_t * o = mk_box(parent, x, y, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static void set_selected(lv_obj_t * btn, bool sel, lv_color_t sel_color)
{
    lv_obj_set_style_bg_color(btn, sel ? sel_color : lv_color_hex(C_BTN), 0);
}

static lv_obj_t * mk_page_title(lv_obj_t * page, const char * title, const char * sub)
{
    lv_obj_t * t = mk_label(page, FONT_L, C_TEXT, title);
    lv_obj_set_pos(t, 24, 14);
    if (NULL != sub) {
        lv_obj_t * s = mk_label(page, FONT_XS, C_TEXT_DIM, sub);
        lv_obj_align_to(s, t, LV_ALIGN_OUT_RIGHT_BOTTOM, 14, -4);
    }
    return t;
}

/* ================================================================ navigation */

static void show_page(page_t p)
{
    for (int i = 0; i < PAGE_COUNT; i++) {
        set_hidden(s_ui.page[i], (i != (int)p));
        lv_obj_set_style_bg_opa(s_ui.nav_btn[i], (i == (int)p) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_t * icon = lv_obj_get_child(s_ui.nav_btn[i], 0);
        lv_obj_t * text = lv_obj_get_child(s_ui.nav_btn[i], 1);
        uint32_t col = (i == (int)p) ? C_TEXT : C_TEXT_DIM;
        lv_obj_set_style_text_color(icon, lv_color_hex(col), 0);
        lv_obj_set_style_text_color(text, lv_color_hex(col), 0);
    }
    s_ui.cur_page = p;
    s_ui.force = true;
}

static void nav_cb(lv_event_t * e)
{
    show_page((page_t)(uintptr_t)lv_event_get_user_data(e));
}

/* ============================================================ home handlers */

static tdeci_t home_current_target(void)
{
    const tstat_model_t * m = &s_ui.snap.model;
    if (s_ui.pend_valid) { return s_ui.pend_val; }
    return (TSTAT_MODE_COOL == m->mode) ? m->active_sp.cool : m->active_sp.heat;
}

static void home_commit_target(tdeci_t v)
{
    const tstat_model_t * m = &s_ui.snap.model;
    tdeci_t base = (TSTAT_MODE_COOL == m->mode) ? m->active_sp.cool : m->active_sp.heat;

    switch (m->mode) {
        case TSTAT_MODE_HEAT: send_cmd(TSTAT_CMD_SET_SETPOINT, 0u, v, 0); break;
        case TSTAT_MODE_COOL: send_cmd(TSTAT_CMD_SET_SETPOINT, 1u, v, 0); break;
        case TSTAT_MODE_AUTO: send_cmd(TSTAT_CMD_SHIFT_SETPOINTS, 0u, (int16_t)(v - base), 0); break;
        default: return;
    }
    /* optimistic display until the next snapshot confirms it */
    s_ui.pend_valid = true;
    s_ui.pend_val   = v;
    s_ui.pend_tick  = lv_tick_get();
    s_ui.force      = true;
}

static void hero_show(tdeci_t f)
{
    char buf[16];
    if (s_ui.celsius) {
        int32_t c = div_round(tstat_deci_f_to_deci_c(f), 5) * 5;
        lv_snprintf(buf, sizeof(buf), "%d", (int)(c / 10));
        set_text(s_ui.lbl_hero, buf);
        set_text(s_ui.lbl_hero_frac, ((c % 10) != 0) ? ".5" DEG : DEG);
    } else {
        lv_snprintf(buf, sizeof(buf), "%d", (int)div_round(f, 10));
        set_text(s_ui.lbl_hero, buf);
        set_text(s_ui.lbl_hero_frac, DEG);
    }
}

static void arc_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    tdeci_t v = snap_step((tdeci_t)lv_arc_get_value(s_ui.arc), s_ui.celsius);

    if (LV_EVENT_VALUE_CHANGED == code) {
        hero_show(v);                     /* live preview while dragging */
    } else if (LV_EVENT_RELEASED == code) {
        lv_arc_set_value(s_ui.arc, v);
        home_commit_target(v);
    }
}

static void plus_minus_cb(lv_event_t * e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    const tstat_model_t * m = &s_ui.snap.model;
    tdeci_t v = step_setpoint(home_current_target(), dir, s_ui.celsius);

    if (TSTAT_MODE_COOL == m->mode) {
        if ((v < TSTAT_COOL_SP_MIN) || (v > TSTAT_COOL_SP_MAX)) { return; }
    } else {
        if ((v < TSTAT_HEAT_SP_MIN) || (v > TSTAT_HEAT_SP_MAX)) { return; }
    }
    home_commit_target(v);
}

static void fan_card_cb(lv_event_t * e)
{
    (void)e;
    tstat_fan_t f = (TSTAT_FAN_AUTO == s_ui.snap.model.fan) ? TSTAT_FAN_ON : TSTAT_FAN_AUTO;
    send_cmd(TSTAT_CMD_SET_FAN, (uint8_t)f, 0, 0);
}

static void mode_card_cb(lv_event_t * e)
{
    (void)e;
    show_page(PAGE_MODE);
}

static void resume_cb(lv_event_t * e)
{
    (void)e;
    s_ui.pend_valid = false;
    send_cmd(TSTAT_CMD_RESUME_SCHEDULE, 0u, 0, 0);
}

static lv_obj_t * mk_info_card(lv_obj_t * parent, int32_t x, int32_t y, const char * title,
                               lv_obj_t ** val, lv_obj_t ** sub, lv_event_cb_t cb)
{
    lv_obj_t * c = mk_card(parent, x, y, 142, 118);
    lv_obj_t * t = mk_label(c, FONT_XS, C_TEXT_DIM, title);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
    *val = mk_label(c, FONT_L, C_TEXT, "--");
    lv_obj_align(*val, LV_ALIGN_LEFT_MID, 0, 4);
    if (NULL != sub) {
        *sub = mk_label(c, FONT_XXS, C_TEXT_DIM, "");
        lv_obj_align(*sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
    if (NULL != cb) {
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_style(c, &st_btn_pr, LV_STATE_PRESSED);
        lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, NULL);
    }
    return c;
}

static void build_home(lv_obj_t * pg)
{
    /* ---- dial */
    lv_obj_t * arc = lv_arc_create(pg);
    s_ui.arc = arc;
    lv_obj_set_pos(arc, ARC_X, ARC_Y);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_range(arc, TSTAT_HEAT_SP_MIN, TSTAT_HEAT_SP_MAX);
    lv_arc_set_value(arc, 700);
    lv_obj_set_style_arc_width(arc, ARC_W, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ARC_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(C_CARD_HI), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(C_HEAT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 7, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_border_width(arc, 0, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(arc, arc_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(arc, arc_cb, LV_EVENT_RELEASED, NULL);

    const int32_t cx = ARC_X + ARC_SIZE / 2;
    const int32_t cy = ARC_Y + ARC_SIZE / 2;

    s_ui.lbl_state = mk_label(pg, FONT_S, C_TEXT_DIM, "HEAT TO");
    lv_obj_set_style_text_letter_space(s_ui.lbl_state, 2, 0);
    lv_obj_align(s_ui.lbl_state, LV_ALIGN_TOP_MID, cx - CONT_W / 2, cy - 92);

    /* hero row: big integer + small ".5°" / "°" */
    lv_obj_t * row = lv_obj_create(pg);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, &st_plain, 0);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    s_ui.lbl_hero      = mk_label(row, FONT_HERO, C_TEXT, "70");
    s_ui.lbl_hero_frac = mk_label(row, FONT_HERO_SMALL, C_TEXT, DEG);
    lv_obj_set_style_pad_top(s_ui.lbl_hero_frac, 10, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, cx - CONT_W / 2, cy - 70);

    s_ui.lbl_second_sp = mk_label(pg, FONT_S, C_COOL, "");
    lv_obj_align(s_ui.lbl_second_sp, LV_ALIGN_TOP_MID, cx - CONT_W / 2, cy + 52);

    s_ui.lbl_inside = mk_label(pg, FONT_M, C_TEXT_DIM, "Inside --");
    lv_obj_align(s_ui.lbl_inside, LV_ALIGN_TOP_MID, cx - CONT_W / 2, cy + 78);

    s_ui.btn_minus = mk_button(pg, cx - 96, cy + 108, 64, 64, FONT_ICON, LV_SYMBOL_MINUS,
                               plus_minus_cb, (void *)(intptr_t)-1);
    s_ui.btn_plus  = mk_button(pg, cx + 32, cy + 108, 64, 64, FONT_ICON, LV_SYMBOL_PLUS,
                               plus_minus_cb, (void *)(intptr_t)1);
    lv_obj_set_style_radius(s_ui.btn_minus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(s_ui.btn_plus, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(s_ui.btn_minus, plus_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)-1);
    lv_obj_add_event_cb(s_ui.btn_plus,  plus_minus_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)1);

    /* ---- right column */
    const int32_t rx = 410;
    mk_info_card(pg, rx,       20,  "SYSTEM",   &s_ui.val_mode, &s_ui.sub_mode, mode_card_cb);
    mk_info_card(pg, rx + 154, 20,  "FAN",      &s_ui.val_fan,  &s_ui.sub_fan,  fan_card_cb);
    mk_info_card(pg, rx,       150, "HUMIDITY", &s_ui.val_hum,  NULL,           NULL);
    mk_info_card(pg, rx + 154, 150, "OUTSIDE",  &s_ui.val_out,  NULL,           NULL);

    lv_obj_t * cf = mk_card(pg, rx, 280, 296, 136);
    s_ui.comfort_dot = mk_dot(cf, 0, 8, 14, C_HOME);
    s_ui.lbl_comfort = mk_label(cf, FONT_L, C_TEXT, "Home");
    lv_obj_set_pos(s_ui.lbl_comfort, 24, 0);
    s_ui.lbl_comfort_sub = mk_label(cf, FONT_XS, C_TEXT_DIM, "");
    lv_obj_set_pos(s_ui.lbl_comfort_sub, 0, 40);
    s_ui.lbl_equip = mk_label(cf, FONT_XS, C_TEXT_DIM, "");
    lv_obj_align(s_ui.lbl_equip, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    s_ui.btn_resume = mk_button(cf, 0, 0, 104, 44, FONT_XS, "Resume", resume_cb, NULL);
    lv_obj_align(s_ui.btn_resume, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_ui.btn_resume, lv_color_hex(C_RENESAS), 0);
}

static void refresh_home(const tstat_model_t * m)
{
    char buf[64];
    char t1[16];
    char t2[16];
    lv_color_t mc = mode_color(m->mode);

    /* dial range / colour per mode */
    int32_t lo = (TSTAT_MODE_COOL == m->mode) ? TSTAT_COOL_SP_MIN : TSTAT_HEAT_SP_MIN;
    int32_t hi = (TSTAT_MODE_COOL == m->mode) ? TSTAT_COOL_SP_MAX : TSTAT_HEAT_SP_MAX;
    if ((lv_arc_get_min_value(s_ui.arc) != lo) || (lv_arc_get_max_value(s_ui.arc) != hi)) {
        lv_arc_set_range(s_ui.arc, lo, hi);
    }

    lv_color_t ind = mc;
    if (TSTAT_MODE_AUTO == m->mode) {
        if (TSTAT_HVAC_COOLING == m->hvac)      { ind = lv_color_hex(C_COOL); }
        else if (TSTAT_HVAC_HEATING == m->hvac) { ind = lv_color_hex(C_HEAT); }
    }
    lv_obj_set_style_arc_color(s_ui.arc, ind, LV_PART_INDICATOR);

    const bool off = (TSTAT_MODE_OFF == m->mode);
    set_hidden(s_ui.btn_minus, off);
    set_hidden(s_ui.btn_plus, off);
    if (off) { lv_obj_remove_flag(s_ui.arc, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_opa(s_ui.arc, LV_OPA_40, 0); }
    else     { lv_obj_add_flag(s_ui.arc, LV_OBJ_FLAG_CLICKABLE);    lv_obj_set_style_opa(s_ui.arc, LV_OPA_COVER, 0); }

    tdeci_t target = home_current_target();
    if (!lv_obj_has_state(s_ui.arc, LV_STATE_PRESSED)) {
        if (lv_arc_get_value(s_ui.arc) != target) { lv_arc_set_value(s_ui.arc, target); }
        if (off) {
            set_text(s_ui.lbl_hero, "");
            set_text(s_ui.lbl_hero_frac, "");
        } else {
            hero_show(target);
        }
    }

    /* state caption */
    const char * cap;
    switch (m->hvac) {
        case TSTAT_HVAC_HEATING:   cap = "HEATING TO"; break;
        case TSTAT_HVAC_COOLING:   cap = "COOLING TO"; break;
        case TSTAT_HVAC_WAIT_HEAT: cap = "HEAT PENDING"; break;
        case TSTAT_HVAC_WAIT_COOL: cap = "COOL PENDING"; break;
        default:
            cap = (TSTAT_MODE_OFF == m->mode) ? "SYSTEM OFF" :
                  (TSTAT_MODE_COOL == m->mode) ? "COOL TO" :
                  (TSTAT_MODE_AUTO == m->mode) ? "HEAT TO" : "HEAT TO";
            break;
    }
    set_text(s_ui.lbl_state, cap);
    lv_obj_set_style_text_color(s_ui.lbl_state,
        (TSTAT_HVAC_HEATING == m->hvac) ? lv_color_hex(C_HEAT) :
        (TSTAT_HVAC_COOLING == m->hvac) ? lv_color_hex(C_COOL) : lv_color_hex(C_TEXT_DIM), 0);

    if (TSTAT_MODE_AUTO == m->mode) {
        fmt_temp(t1, sizeof(t1), m->active_sp.cool, s_ui.celsius);
        lv_snprintf(buf, sizeof(buf), "Cool to %s", t1);
        set_text(s_ui.lbl_second_sp, buf);
    } else {
        set_text(s_ui.lbl_second_sp, "");
    }

    fmt_temp(t1, sizeof(t1), m->indoor, s_ui.celsius);
    lv_snprintf(buf, sizeof(buf), "Inside %s", t1);
    set_text(s_ui.lbl_inside, buf);

    /* cards */
    set_text(s_ui.val_mode, tstat_mode_name(m->mode));
    lv_obj_set_style_text_color(s_ui.val_mode, mc, 0);
    set_text(s_ui.sub_mode, "Tap to change");

    set_text(s_ui.val_fan, (TSTAT_FAN_ON == m->fan) ? "On" : "Auto");
    set_text(s_ui.sub_fan, m->out_g ? "Running - tap to toggle" : "Idle - tap to toggle");

    lv_snprintf(buf, sizeof(buf), "%u%%", (unsigned)m->humidity);
    set_text(s_ui.val_hum, buf);

    fmt_temp(t1, sizeof(t1), m->outdoor, s_ui.celsius);
    set_text(s_ui.val_out, t1);

    /* comfort / hold */
    uint16_t nxt = tstat_model_next_transition_min(m);
    if (m->hold) {
        set_text(s_ui.lbl_comfort, "Hold");
        lv_obj_set_style_bg_color(s_ui.comfort_dot, lv_color_hex(C_RENESAS), 0);
        lv_snprintf(buf, sizeof(buf), "Until %02u:%02u, then schedule", (unsigned)(nxt / 60u), (unsigned)(nxt % 60u));
    } else {
        set_text(s_ui.lbl_comfort, tstat_comfort_name(m->active_comfort));
        lv_obj_set_style_bg_color(s_ui.comfort_dot, comfort_color(m->active_comfort), 0);
        if (TSTAT_MODE_AUTO == m->mode) {
            fmt_temp(t1, sizeof(t1), m->active_sp.heat, s_ui.celsius);
            fmt_temp(t2, sizeof(t2), m->active_sp.cool, s_ui.celsius);
            lv_snprintf(buf, sizeof(buf), "%s - %s until %02u:%02u", t1, t2, (unsigned)(nxt / 60u), (unsigned)(nxt % 60u));
        } else {
            lv_snprintf(buf, sizeof(buf), "Schedule until %02u:%02u", (unsigned)(nxt / 60u), (unsigned)(nxt % 60u));
        }
    }
    set_text(s_ui.lbl_comfort_sub, buf);
    set_hidden(s_ui.btn_resume, !m->hold);

    if (m->protect_remaining_s > 0u) {
        lv_snprintf(buf, sizeof(buf), "Equip. protect %u:%02u",
                     (unsigned)(m->protect_remaining_s / 60u), (unsigned)(m->protect_remaining_s % 60u));
    } else {
        lv_snprintf(buf, sizeof(buf), "W1 %s  Y1 %s  G %s",
                     m->out_w1 ? "ON" : "off", m->out_y1 ? "ON" : "off", m->out_g ? "ON" : "off");
    }
    set_text(s_ui.lbl_equip, buf);
}

/* ================================================================ mode page */

static void mode_tile_cb(lv_event_t * e)
{
    s_ui.pend_valid = false;
    send_cmd(TSTAT_CMD_SET_MODE, (uint8_t)(uintptr_t)lv_event_get_user_data(e), 0, 0);
}

static void fan_btn_cb(lv_event_t * e)
{
    send_cmd(TSTAT_CMD_SET_FAN, (uint8_t)(uintptr_t)lv_event_get_user_data(e), 0, 0);
}

static void build_mode(lv_obj_t * pg)
{
    static const char * const desc[TSTAT_MODE_COUNT] = {
        "System off", "Heating only", "Cooling only", "Heat & cool"
    };
    static const tstat_mode_t order[TSTAT_MODE_COUNT] = {
        TSTAT_MODE_HEAT, TSTAT_MODE_COOL, TSTAT_MODE_AUTO, TSTAT_MODE_OFF
    };

    mk_page_title(pg, "System Mode", NULL);

    for (int i = 0; i < TSTAT_MODE_COUNT; i++) {
        tstat_mode_t md = order[i];
        lv_obj_t * t = mk_button(pg, 24 + i * 168, 70, 154, 150, NULL, NULL,
                                 mode_tile_cb, (void *)(uintptr_t)md);
        lv_obj_t * n = mk_label(t, FONT_L, C_TEXT, tstat_mode_name(md));
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 16, 16);
        lv_obj_t * d = mk_label(t, FONT_XS, C_TEXT, desc[md]);
        lv_obj_set_style_text_opa(d, LV_OPA_80, 0);
        lv_obj_align(d, LV_ALIGN_BOTTOM_LEFT, 16, -16);
        lv_obj_t * bar = mk_box(t, 16, 60, 40, 6);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 3, 0);
        lv_obj_set_style_bg_color(bar, mode_color(md), 0);
        s_ui.mode_tile[md] = t;
    }

    lv_obj_t * ft = mk_label(pg, FONT_M, C_TEXT, "Fan");
    lv_obj_set_pos(ft, 24, 246);
    s_ui.fan_btn[TSTAT_FAN_AUTO] = mk_button(pg, 24,  280, 160, 56, FONT_S, "Auto", fan_btn_cb, (void *)(uintptr_t)TSTAT_FAN_AUTO);
    s_ui.fan_btn[TSTAT_FAN_ON]   = mk_button(pg, 196, 280, 160, 56, FONT_S, "On",   fan_btn_cb, (void *)(uintptr_t)TSTAT_FAN_ON);
    lv_obj_t * fd = mk_label(pg, FONT_XS, C_TEXT_DIM,
                             "Auto: fan runs only with heating or cooling.\nOn: continuous circulation.");
    lv_obj_set_pos(fd, 380, 288);

    s_ui.lbl_protect = mk_label(pg, FONT_XS, C_TEXT_MUTED, "");
    lv_obj_set_pos(s_ui.lbl_protect, 24, 372);
}

static void refresh_mode(const tstat_model_t * m)
{
    char buf[96];
    for (int i = 0; i < TSTAT_MODE_COUNT; i++) {
        set_selected(s_ui.mode_tile[i], (int)m->mode == i, lv_color_hex(C_CARD_HI));
        lv_obj_set_style_border_width(s_ui.mode_tile[i], ((int)m->mode == i) ? 3 : 0, 0);
        lv_obj_set_style_border_color(s_ui.mode_tile[i], mode_color((tstat_mode_t)i), 0);
    }
    for (int i = 0; i < TSTAT_FAN_COUNT; i++) {
        set_selected(s_ui.fan_btn[i], (int)m->fan == i, lv_color_hex(C_RENESAS));
    }
    lv_snprintf(buf, sizeof(buf),
                "Equipment protection\nminimum run %u:%02u  |  minimum off %u:%02u  |  differential %d.%d" DEG "F  |  auto deadband %d" DEG "F",
                (unsigned)(m->min_on_s / 60u), (unsigned)(m->min_on_s % 60u),
                (unsigned)(m->min_off_s / 60u), (unsigned)(m->min_off_s % 60u),
                (int)(m->differential / 10), (int)(m->differential % 10), (int)(m->deadband / 10));
    set_text(s_ui.lbl_protect, buf);
}

/* ============================================================ schedule page */

#define TL_X    (24)
#define TL_Y    (70)
#define TL_W    (660)
#define TL_H    (44)

static void comfort_btn_cb(lv_event_t * e)
{
    uintptr_t code = (uintptr_t)lv_event_get_user_data(e);
    tstat_comfort_t c = (tstat_comfort_t)(code >> 2);
    uint8_t which = (uint8_t)((code >> 1) & 1u);
    int dir = (code & 1u) ? 1 : -1;
    tstat_setpoints_t sp = s_ui.snap.model.comfort[c];

    if (0u == which) { sp.heat = step_setpoint(sp.heat, dir, s_ui.celsius); }
    else             { sp.cool = step_setpoint(sp.cool, dir, s_ui.celsius); }
    send_cmd(TSTAT_CMD_SET_COMFORT, (uint8_t)c, sp.heat, sp.cool);
}

static void build_timeline(const tstat_model_t * m)
{
    int n = 0;
    for (int i = 0; i < SEG_MAX; i++) { set_hidden(s_ui.seg[i], true); }

    for (uint8_t i = 0; (i < m->sched_count) && (n < SEG_MAX); i++) {
        uint16_t start = m->sched[i].start_min;
        uint16_t end   = (i + 1u < m->sched_count) ? m->sched[i + 1u].start_min : 1440u;
        lv_obj_t * s = s_ui.seg[n++];
        lv_obj_set_pos(s, (int32_t)start * TL_W / 1440, 0);
        lv_obj_set_size(s, ((int32_t)end - start) * TL_W / 1440, TL_H);
        lv_obj_set_style_bg_color(s, comfort_color((tstat_comfort_t)m->sched[i].comfort), 0);
        lv_label_set_text(lv_obj_get_child(s, 0), tstat_comfort_name((tstat_comfort_t)m->sched[i].comfort));
        set_hidden(s, false);
    }
    /* the period that wraps from the previous day (00:00 -> first entry) */
    if ((m->sched_count > 0u) && (m->sched[0].start_min > 0u) && (n < SEG_MAX)) {
        uint8_t last = (uint8_t)(m->sched_count - 1u);
        lv_obj_t * s = s_ui.seg[n++];
        lv_obj_set_pos(s, 0, 0);
        lv_obj_set_size(s, (int32_t)m->sched[0].start_min * TL_W / 1440, TL_H);
        lv_obj_set_style_bg_color(s, comfort_color((tstat_comfort_t)m->sched[last].comfort), 0);
        lv_label_set_text(lv_obj_get_child(s, 0), tstat_comfort_name((tstat_comfort_t)m->sched[last].comfort));
        set_hidden(s, false);
    }
    s_ui.timeline_built = true;
}

static void build_sched(lv_obj_t * pg)
{
    static const char * const hours[] = { "00", "03", "06", "09", "12", "15", "18", "21", "24" };

    mk_page_title(pg, "Schedule", "Every day  |  edits apply to comfort settings");

    s_ui.timeline = mk_box(pg, TL_X, TL_Y, TL_W, TL_H);
    lv_obj_set_style_radius(s_ui.timeline, 10, 0);
    lv_obj_set_style_clip_corner(s_ui.timeline, true, 0);
    lv_obj_set_style_bg_opa(s_ui.timeline, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_ui.timeline, lv_color_hex(C_CARD), 0);
    for (int i = 0; i < SEG_MAX; i++) {
        lv_obj_t * s = mk_box(s_ui.timeline, 0, 0, 10, TL_H);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s, 1, 0);
        lv_obj_set_style_border_color(s, lv_color_hex(C_BG), 0);
        lv_obj_t * l = mk_label(s, FONT_XS, C_BG, "");
        lv_obj_center(l);
        lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
        s_ui.seg[i] = s;
    }

    for (int i = 0; i < 9; i++) {
        lv_obj_t * l = mk_label(pg, FONT_XXS, C_TEXT_DIM, hours[i]);
        lv_obj_set_pos(l, TL_X + (i * TL_W) / 8 - ((i == 8) ? 14 : (i == 0 ? 0 : 7)), TL_Y + TL_H + 6);
    }

    s_ui.now_marker = mk_box(pg, TL_X, TL_Y - 6, 4, TL_H + 12);
    lv_obj_set_style_bg_opa(s_ui.now_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_ui.now_marker, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_radius(s_ui.now_marker, 2, 0);

    s_ui.lbl_sched_now = mk_label(pg, FONT_S, C_TEXT, "");
    lv_obj_set_pos(s_ui.lbl_sched_now, TL_X, TL_Y + TL_H + 30);

    for (int c = 0; c < TSTAT_COMFORT_COUNT; c++) {
        lv_obj_t * card = mk_card(pg, TL_X + c * 225, 186, 210, 228);
        lv_obj_t * dot = mk_dot(card, 0, 8, 14, 0);
        lv_obj_set_style_bg_color(dot, comfort_color((tstat_comfort_t)c), 0);
        lv_obj_t * n = mk_label(card, FONT_M, C_TEXT, tstat_comfort_name((tstat_comfort_t)c));
        lv_obj_set_pos(n, 24, 2);

        for (int w = 0; w < 2; w++) {
            int32_t y = 46 + w * 86;
            lv_obj_t * cap = mk_label(card, FONT_XS, C_TEXT_DIM, (0 == w) ? "HEAT" : "COOL");
            lv_obj_set_pos(cap, 0, y);
            lv_obj_t * v = mk_label(card, FONT_L, (0 == w) ? C_HEAT : C_COOL, "--");
            lv_obj_set_pos(v, 0, y + 22);
            if (0 == w) { s_ui.lbl_cf_heat[c] = v; } else { s_ui.lbl_cf_cool[c] = v; }

            uintptr_t base = ((uintptr_t)c << 2) | ((uintptr_t)w << 1);
            mk_button(card, 82,  y + 14, 44, 44, FONT_S, LV_SYMBOL_MINUS, comfort_btn_cb, (void *)(base | 0u));
            mk_button(card, 134, y + 14, 44, 44, FONT_S, LV_SYMBOL_PLUS,  comfort_btn_cb, (void *)(base | 1u));
        }
    }
}

static void refresh_sched(const tstat_model_t * m)
{
    char buf[96];
    char t[16];
    uint16_t mod = (uint16_t)((m->clock_s % TSTAT_SECONDS_PER_DAY) / 60u);

    if (!s_ui.timeline_built) { build_timeline(m); }
    lv_obj_set_x(s_ui.now_marker, TL_X + ((int32_t)mod * TL_W) / 1440 - 2);

    uint16_t nxt = tstat_model_next_transition_min(m);
    int8_t   ni  = (int8_t)((m->sched_index + 1) % (int8_t)m->sched_count);
    lv_snprintf(buf, sizeof(buf), "Now: %s%s   |   Next: %s at %02u:%02u",
                tstat_comfort_name(m->active_comfort), m->hold ? " (hold active)" : "",
                tstat_comfort_name((tstat_comfort_t)m->sched[ni].comfort),
                (unsigned)(nxt / 60u), (unsigned)(nxt % 60u));
    set_text(s_ui.lbl_sched_now, buf);

    for (int c = 0; c < TSTAT_COMFORT_COUNT; c++) {
        fmt_temp(t, sizeof(t), m->comfort[c].heat, s_ui.celsius);
        set_text(s_ui.lbl_cf_heat[c], t);
        fmt_temp(t, sizeof(t), m->comfort[c].cool, s_ui.celsius);
        set_text(s_ui.lbl_cf_cool[c], t);
    }
}

/* ============================================================= sensors page */

static void sensor_sw_cb(lv_event_t * e)
{
    lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    send_cmd(TSTAT_CMD_SET_SENSOR_PART, idx, lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0, 0);
}

static void build_sensors(lv_obj_t * pg)
{
    mk_page_title(pg, "Room Sensors", "Comfort is controlled to the average of selected rooms");

    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        lv_obj_t * c = mk_card(pg, 24 + i * 225, 70, 210, 268);
        lv_obj_t * n = mk_label(c, FONT_M, C_TEXT, "");
        lv_obj_set_pos(n, 0, 0);
        lv_label_set_text(n, (0 == i) ? "Living Room" : (1 == i) ? "Bedroom" : "Kitchen");
        lv_obj_t * k = mk_label(c, FONT_XXS, C_TEXT_MUTED, (0 == i) ? "THERMOSTAT" : "REMOTE SENSOR");
        lv_obj_set_pos(k, 0, 28);

        s_ui.sens_temp[i] = mk_label(c, FONT_XL, C_TEXT, "--");
        lv_obj_set_pos(s_ui.sens_temp[i], 0, 62);

        s_ui.sens_occ_dot[i] = mk_dot(c, 0, 142, 12, C_TEXT_MUTED);
        s_ui.sens_occ_lbl[i] = mk_label(c, FONT_XS, C_TEXT_DIM, "");
        lv_obj_set_pos(s_ui.sens_occ_lbl[i], 20, 138);

        lv_obj_t * u = mk_label(c, FONT_XS, C_TEXT_DIM, "Use for comfort");
        lv_obj_align(u, LV_ALIGN_BOTTOM_LEFT, 0, -10);
        lv_obj_t * sw = lv_switch_create(c);
        lv_obj_set_size(sw, 56, 30);
        lv_obj_align(sw, LV_ALIGN_BOTTOM_RIGHT, 0, -4);
        lv_obj_set_style_bg_color(sw, lv_color_hex(C_BTN), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(C_OK), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, sensor_sw_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
        s_ui.sens_sw[i] = sw;
    }

    s_ui.lbl_sens_avg = mk_label(pg, FONT_S, C_TEXT, "");
    lv_obj_set_pos(s_ui.lbl_sens_avg, 24, 356);
    lv_obj_t * note = mk_label(pg, FONT_XXS, C_TEXT_MUTED,
                               "If no room is selected the thermostat's own sensor is used.");
    lv_obj_set_pos(note, 24, 384);
}

static void refresh_sensors(const tstat_model_t * m)
{
    char t[16];
    char buf[64];
    for (int i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        const tstat_sensor_t * s = &m->sensors[i];
        fmt_temp(t, sizeof(t), s->temp, s_ui.celsius);
        set_text(s_ui.sens_temp[i], t);
        lv_obj_set_style_text_color(s_ui.sens_temp[i],
                                    lv_color_hex(s->participating ? C_TEXT : C_TEXT_MUTED), 0);
        lv_obj_set_style_bg_color(s_ui.sens_occ_dot[i], lv_color_hex(s->occupied ? C_OK : C_TEXT_MUTED), 0);
        set_text(s_ui.sens_occ_lbl[i], s->occupied ? "Occupied" : "Unoccupied");
        bool checked = lv_obj_has_state(s_ui.sens_sw[i], LV_STATE_CHECKED);
        if (checked != (0u != s->participating)) {
            if (s->participating) { lv_obj_add_state(s_ui.sens_sw[i], LV_STATE_CHECKED); }
            else                  { lv_obj_remove_state(s_ui.sens_sw[i], LV_STATE_CHECKED); }
        }
    }
    fmt_temp(t, sizeof(t), m->indoor, s_ui.celsius);
    lv_snprintf(buf, sizeof(buf), "Control temperature: %s", t);
    set_text(s_ui.lbl_sens_avg, buf);
}

/* ============================================================== energy page */

#define CH_X        (24)
#define CH_Y        (64)
#define CH_W        (660)
#define CH_H        (262)
#define CH_BASE     (214)       /* baseline y inside chart card */
#define CH_MAXH     (170)
#define CH_BAR_W    (36)

static void build_energy(lv_obj_t * pg)
{
    mk_page_title(pg, "Energy", "HVAC runtime - last 7 days");

    lv_obj_t * ch = mk_card(pg, CH_X, CH_Y, CH_W, CH_H);
    lv_obj_set_style_pad_all(ch, 0, 0);

    lv_obj_t * base = mk_box(ch, 16, CH_BASE, CH_W - 32, 2);
    lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(base, lv_color_hex(C_CARD_HI), 0);

    const int32_t col_w = (CH_W - 32) / CHART_COLS;
    for (int i = 0; i < CHART_COLS; i++) {
        int32_t x = 16 + i * col_w + (col_w - CH_BAR_W) / 2;
        s_ui.bar_heat[i] = mk_box(ch, x, CH_BASE, CH_BAR_W, 0);
        lv_obj_set_style_bg_opa(s_ui.bar_heat[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_ui.bar_heat[i], lv_color_hex(C_HEAT), 0);
        s_ui.bar_cool[i] = mk_box(ch, x, CH_BASE, CH_BAR_W, 0);
        lv_obj_set_style_bg_opa(s_ui.bar_cool[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_ui.bar_cool[i], lv_color_hex(C_COOL), 0);

        s_ui.bar_val[i] = mk_label(ch, FONT_XXS, C_TEXT_DIM, "");
        lv_obj_set_width(s_ui.bar_val[i], col_w);
        lv_obj_set_style_text_align(s_ui.bar_val[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_ui.bar_val[i], 16 + i * col_w, CH_BASE - 20);

        s_ui.bar_day[i] = mk_label(ch, FONT_XS, C_TEXT_DIM, "");
        lv_obj_set_width(s_ui.bar_day[i], col_w);
        lv_obj_set_style_text_align(s_ui.bar_day[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_ui.bar_day[i], 16 + i * col_w, CH_BASE + 12);
    }

    lv_obj_t * lh = mk_dot(pg, CH_X, CH_Y + CH_H + 18, 12, C_HEAT);
    (void)lh;
    lv_obj_t * lht = mk_label(pg, FONT_XS, C_TEXT_DIM, "Heat");
    lv_obj_set_pos(lht, CH_X + 18, CH_Y + CH_H + 14);
    mk_dot(pg, CH_X + 70, CH_Y + CH_H + 18, 12, C_COOL);
    lv_obj_t * lct = mk_label(pg, FONT_XS, C_TEXT_DIM, "Cool");
    lv_obj_set_pos(lct, CH_X + 88, CH_Y + CH_H + 14);

    s_ui.lbl_today = mk_label(pg, FONT_S, C_TEXT, "");
    lv_obj_set_pos(s_ui.lbl_today, CH_X + 160, CH_Y + CH_H + 12);
    s_ui.lbl_week = mk_label(pg, FONT_XS, C_TEXT_DIM, "");
    lv_obj_set_pos(s_ui.lbl_week, CH_X, CH_Y + CH_H + 46);
}

static void refresh_energy(const tstat_model_t * m)
{
    char buf[96];
    char a[16];
    char b[16];
    uint32_t heat[CHART_COLS];
    uint32_t cool[CHART_COLS];
    uint32_t maxv = 60u;
    uint32_t wk_h = 0u;
    uint32_t wk_c = 0u;
    uint32_t today_dow = m->clock_s / TSTAT_SECONDS_PER_DAY;   /* 0 = Mon */

    /* columns: 6 previous days + today */
    for (int i = 0; i < CHART_COLS - 1; i++) {
        heat[i] = m->hist_heat_min[i + 1];
        cool[i] = m->hist_cool_min[i + 1];
    }
    heat[CHART_COLS - 1] = m->today_heat_s / 60u;
    cool[CHART_COLS - 1] = m->today_cool_s / 60u;

    for (int i = 0; i < CHART_COLS; i++) {
        if (heat[i] + cool[i] > maxv) { maxv = heat[i] + cool[i]; }
        wk_h += heat[i];
        wk_c += cool[i];
    }
    maxv = ((maxv + 59u) / 60u) * 60u;

    for (int i = 0; i < CHART_COLS; i++) {
        int32_t hh = (int32_t)(heat[i] * CH_MAXH / maxv);
        int32_t ch = (int32_t)(cool[i] * CH_MAXH / maxv);
        lv_obj_set_height(s_ui.bar_heat[i], hh);
        lv_obj_set_y(s_ui.bar_heat[i], CH_BASE - hh);
        lv_obj_set_height(s_ui.bar_cool[i], ch);
        lv_obj_set_y(s_ui.bar_cool[i], CH_BASE - hh - ch);

        fmt_duration(a, sizeof(a), heat[i] + cool[i]);
        set_text(s_ui.bar_val[i], a);
        lv_obj_set_y(s_ui.bar_val[i], CH_BASE - hh - ch - 20);

        uint32_t dow = (today_dow + 7u - (uint32_t)(CHART_COLS - 1 - i)) % 7u;
        set_text(s_ui.bar_day[i], (i == CHART_COLS - 1) ? "Today" : s_days[dow]);
    }

    fmt_duration(a, sizeof(a), heat[CHART_COLS - 1]);
    fmt_duration(b, sizeof(b), cool[CHART_COLS - 1]);
    lv_snprintf(buf, sizeof(buf), "Today:  heat %s   |   cool %s", a, b);
    set_text(s_ui.lbl_today, buf);

    fmt_duration(a, sizeof(a), wk_h);
    fmt_duration(b, sizeof(b), wk_c);
    lv_snprintf(buf, sizeof(buf), "7-day total:  heat %s   |   cool %s      (history seeded with demo data)", a, b);
    set_text(s_ui.lbl_week, buf);
}

/* ============================================================ settings page */

static void unit_cb(lv_event_t * e)
{
    s_ui.celsius = (0u != (uintptr_t)lv_event_get_user_data(e));
    s_ui.pend_valid = false;
    s_ui.force = true;
}

static void bright_cb(lv_event_t * e)
{
    char buf[8];
    int32_t v = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    lv_snprintf(buf, sizeof(buf), "%d%%", (int)v);
    set_text(s_ui.lbl_bright, buf);
    thermostat_hal_set_backlight((uint8_t)v);
}

static void speed_cb(lv_event_t * e)
{
    send_cmd(TSTAT_CMD_SET_SIM_SPEED, 0u, (int16_t)(uintptr_t)lv_event_get_user_data(e), 0);
}

#if LV_USE_PERF_MONITOR
static void perf_observer_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    char buf[48];
    const lv_sysmon_perf_info_t * perf = lv_subject_get_pointer(subject);
    lv_snprintf(buf, sizeof(buf), "%d FPS  |  CPU %d%%", (int)perf->calculated.fps, (int)perf->calculated.cpu);
    set_text(lv_observer_get_target_obj(observer), buf);
}
#endif

static lv_obj_t * mk_setting_row(lv_obj_t * pg, int32_t y, const char * title)
{
    lv_obj_t * r = mk_card(pg, 24, y, 660, 62);
    lv_obj_set_style_pad_ver(r, 0, 0);
    lv_obj_t * t = mk_label(r, FONT_S, C_TEXT, title);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
    return r;
}

static void build_settings(lv_obj_t * pg)
{
    char buf[96];
    static const char * const outs[3] = { "W1 heat", "Y1 cool", "G fan" };

    mk_page_title(pg, "Settings", NULL);

    lv_obj_t * r = mk_setting_row(pg, 62, "Temperature units");
    s_ui.unit_btn[0] = mk_button(r, 0, 0, 72, 42, FONT_S, DEG "F", unit_cb, (void *)(uintptr_t)0u);
    s_ui.unit_btn[1] = mk_button(r, 0, 0, 72, 42, FONT_S, DEG "C", unit_cb, (void *)(uintptr_t)1u);
    lv_obj_align(s_ui.unit_btn[0], LV_ALIGN_RIGHT_MID, -82, 0);
    lv_obj_align(s_ui.unit_btn[1], LV_ALIGN_RIGHT_MID, 0, 0);

    r = mk_setting_row(pg, 132, "Display brightness");
    s_ui.slider_bright = lv_slider_create(r);
    lv_obj_set_width(s_ui.slider_bright, 300);
    lv_slider_set_range(s_ui.slider_bright, 10, 100);
    lv_slider_set_value(s_ui.slider_bright, 80, LV_ANIM_OFF);
    lv_obj_align(s_ui.slider_bright, LV_ALIGN_RIGHT_MID, -70, 0);
    lv_obj_set_style_bg_color(s_ui.slider_bright, lv_color_hex(C_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.slider_bright, lv_color_hex(C_RENESAS), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_ui.slider_bright, lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_add_event_cb(s_ui.slider_bright, bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_ui.lbl_bright = mk_label(r, FONT_S, C_TEXT_DIM, "80%");
    lv_obj_align(s_ui.lbl_bright, LV_ALIGN_RIGHT_MID, 0, 0);

    r = mk_setting_row(pg, 202, "Demo speed (simulated time)");
    for (int i = 0; i < 3; i++) {
        lv_snprintf(buf, sizeof(buf), "%ux", (unsigned)s_speeds[i]);
        s_ui.speed_btn[i] = mk_button(r, 0, 0, 72, 42, FONT_S, buf, speed_cb, (void *)(uintptr_t)s_speeds[i]);
        lv_obj_align(s_ui.speed_btn[i], LV_ALIGN_RIGHT_MID, -(2 - i) * 82, 0);
    }

    r = mk_setting_row(pg, 272, "Equipment outputs");
    for (int i = 0; i < 3; i++) {
        s_ui.out_dot[i] = mk_dot(r, 0, 0, 14, C_TEXT_MUTED);
        lv_obj_align(s_ui.out_dot[i], LV_ALIGN_RIGHT_MID, -(2 - i) * 110 - 84, 0);
        lv_obj_t * l = mk_label(r, FONT_XS, C_TEXT_DIM, outs[i]);
        lv_obj_align_to(l, s_ui.out_dot[i], LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    }

    r = mk_setting_row(pg, 342, "System");
    lv_snprintf(buf, sizeof(buf), "RZ/A2M  |  FreeRTOS %s  |  LVGL %d.%d.%d  |  800x480 RGB565",
                thermostat_service_rtos_version(),
                LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_obj_t * sys = mk_label(r, FONT_XS, C_TEXT_DIM, buf);
    lv_obj_align(sys, LV_ALIGN_RIGHT_MID, 0, -10);
    s_ui.lbl_perf = mk_label(r, FONT_XS, C_OK, "");
    lv_obj_align(s_ui.lbl_perf, LV_ALIGN_RIGHT_MID, 0, 12);

#if LV_USE_PERF_MONITOR
    lv_display_t * disp = lv_display_get_default();
    lv_subject_add_observer_obj(&disp->perf_sysmon_backend.subject, perf_observer_cb, s_ui.lbl_perf, NULL);
#else
    lv_label_set_text(s_ui.lbl_perf, "Enable LV_USE_PERF_MONITOR for FPS / CPU");
#endif
}

static void refresh_settings(const tstat_model_t * m, uint16_t speed)
{
    set_selected(s_ui.unit_btn[0], !s_ui.celsius, lv_color_hex(C_RENESAS));
    set_selected(s_ui.unit_btn[1],  s_ui.celsius, lv_color_hex(C_RENESAS));
    for (int i = 0; i < 3; i++) {
        set_selected(s_ui.speed_btn[i], speed == s_speeds[i], lv_color_hex(C_RENESAS));
    }
    lv_obj_set_style_bg_color(s_ui.out_dot[0], lv_color_hex(m->out_w1 ? C_HEAT : C_TEXT_MUTED), 0);
    lv_obj_set_style_bg_color(s_ui.out_dot[1], lv_color_hex(m->out_y1 ? C_COOL : C_TEXT_MUTED), 0);
    lv_obj_set_style_bg_color(s_ui.out_dot[2], lv_color_hex(m->out_g  ? C_OK   : C_TEXT_MUTED), 0);
}

/* ============================================================ frame / timer */

static void refresh_status_bar(const tstat_model_t * m, uint16_t speed)
{
    char buf[32];
    char t[16];
    uint32_t sod = m->clock_s % TSTAT_SECONDS_PER_DAY;
    lv_snprintf(buf, sizeof(buf), "%s  %02u:%02u", s_days[(m->clock_s / TSTAT_SECONDS_PER_DAY) % 7u],
                (unsigned)(sod / 3600u), (unsigned)((sod / 60u) % 60u));
    set_text(s_ui.lbl_clock, buf);

    fmt_temp(t, sizeof(t), m->outdoor, s_ui.celsius);
    lv_snprintf(buf, sizeof(buf), "Outside %s", t);
    set_text(s_ui.lbl_bar_outdoor, buf);

    if (speed > 1u) { lv_snprintf(buf, sizeof(buf), "DEMO %ux", (unsigned)speed); }
    else            { buf[0] = '\0'; }
    set_text(s_ui.lbl_demo, buf);
}

static void refresh_all(void)
{
    const tstat_model_t * m = &s_ui.snap.model;

    if (s_ui.pend_valid && (lv_tick_elaps(s_ui.pend_tick) > PENDING_MS)) { s_ui.pend_valid = false; }

    refresh_status_bar(m, s_ui.snap.sim_speed);
    /* only the visible page is refreshed - hidden pages are refreshed on show */
    switch (s_ui.cur_page) {
        case PAGE_HOME:     refresh_home(m);                          break;
        case PAGE_MODE:     refresh_mode(m);                          break;
        case PAGE_SCHED:    refresh_sched(m);                         break;
        case PAGE_SENSORS:  refresh_sensors(m);                       break;
        case PAGE_ENERGY:   refresh_energy(m);                        break;
        case PAGE_SETTINGS: refresh_settings(m, s_ui.snap.sim_speed); break;
        default: break;
    }
}

static void ui_timer_cb(lv_timer_t * t)
{
    (void)t;
    bool updated = thermostat_service_get_snapshot(&s_ui.snap, &s_ui.seq);
    if (updated) { s_ui.have_snap = true; }
    if (s_ui.have_snap && (updated || s_ui.force)) {
        s_ui.force = false;
        refresh_all();
    }
}

static void build_frame(lv_obj_t * scr)
{
    static const char * const icons[PAGE_COUNT] = {
        LV_SYMBOL_HOME, LV_SYMBOL_POWER, LV_SYMBOL_LIST, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_CHARGE, LV_SYMBOL_SETTINGS
    };
    static const char * const names[PAGE_COUNT] = {
        "Home", "Mode", "Schedule", "Sensors", "Energy", "Settings"
    };

    /* status bar */
    lv_obj_t * bar = mk_box(scr, 0, 0, SCR_W, BAR_H);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_BAR), 0);

    lv_obj_t * brand = mk_label(bar, FONT_S, C_TEXT, "RENESAS");
    lv_obj_set_style_text_color(brand, lv_color_hex(0x6F8BFF), 0);
    lv_obj_set_style_text_letter_space(brand, 2, 0);
    lv_obj_align(brand, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_t * prod = mk_label(bar, FONT_XS, C_TEXT_DIM, "RZ/A2M  Smart Thermostat");
    lv_obj_align_to(prod, brand, LV_ALIGN_OUT_RIGHT_MID, 12, 1);

    s_ui.lbl_clock = mk_label(bar, FONT_M, C_TEXT, "--:--");
    lv_obj_align(s_ui.lbl_clock, LV_ALIGN_CENTER, 40, 0);

    lv_obj_t * icons_r = mk_label(bar, FONT_S, C_TEXT_DIM, LV_SYMBOL_WIFI "  " LV_SYMBOL_BELL);
    lv_obj_align(icons_r, LV_ALIGN_RIGHT_MID, -16, 0);
    s_ui.lbl_bar_outdoor = mk_label(bar, FONT_XS, C_TEXT_DIM, "");
    lv_obj_align(s_ui.lbl_bar_outdoor, LV_ALIGN_RIGHT_MID, -76, 0);
    s_ui.lbl_demo = mk_label(bar, FONT_XXS, C_AUTO, "");
    lv_obj_align(s_ui.lbl_demo, LV_ALIGN_RIGHT_MID, -190, 0);

    /* nav rail */
    lv_obj_t * nav = mk_box(scr, 0, BAR_H, NAV_W, CONT_H);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(C_BAR), 0);
    for (int i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t * b = lv_button_create(nav);
        lv_obj_remove_style_all(b);
        lv_obj_set_pos(b, 6, 4 + i * 72);
        lv_obj_set_size(b, NAV_W - 12, 66);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(C_CARD_HI), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t * ic = mk_label(b, FONT_ICON, C_TEXT_DIM, icons[i]);
        lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 8);
        lv_obj_t * tx = mk_label(b, FONT_XXS, C_TEXT_DIM, names[i]);
        lv_obj_align(tx, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_ui.nav_btn[i] = b;
    }

    /* page containers */
    for (int i = 0; i < PAGE_COUNT; i++) {
        s_ui.page[i] = mk_box(scr, NAV_W, BAR_H, CONT_W, CONT_H);
    }
}

/* =================================================================== public */

void thermostat_ui_create(void)
{
    memset(&s_ui, 0, sizeof(s_ui));
    styles_init();

    lv_obj_t * scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(C_TEXT), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_frame(scr);
    build_home(s_ui.page[PAGE_HOME]);
    build_mode(s_ui.page[PAGE_MODE]);
    build_sched(s_ui.page[PAGE_SCHED]);
    build_sensors(s_ui.page[PAGE_SENSORS]);
    build_energy(s_ui.page[PAGE_ENERGY]);
    build_settings(s_ui.page[PAGE_SETTINGS]);

    show_page(PAGE_HOME);

    (void)lv_timer_create(ui_timer_cb, REFRESH_MS, NULL);
    ui_timer_cb(NULL);
}
