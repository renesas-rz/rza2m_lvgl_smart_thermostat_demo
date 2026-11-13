/**
 * @file thermostat_model.c
 * @brief Smart-thermostat state model, control state machine and plant simulation.
 *
 * Control behaviour (per 1 s step):
 *  - Effective setpoints = temporary hold (if active) else schedule comfort setting.
 *  - HEAT : call starts at T <= heat - diff, ends at T >= heat.
 *  - COOL : call starts at T >= cool + diff, ends at T <= cool.
 *  - AUTO : both of the above, mutually exclusive, deadband enforced on edit.
 *  - Equipment protection: a started stage runs at least min_on_s; after stopping,
 *    equipment stays off at least min_off_s (anti-short-cycle). Heat<->cool
 *    changeover always passes through OFF.
 *  - Fan (G): ON when fan mode is ON or any stage is running.
 *
 * NOTE: This is demonstration-grade logic. In a product, relay sequencing and
 * protection timers belong in a dedicated safety-qualified controller.
 */
#include <string.h>
#include "thermostat_model.h"


/* ------------------------------------------------------------- plant constants */
#define SIM_LOSS_PER_S        (1.0f / 10800.0f)   /* envelope time constant ~3 h   */
#define SIM_HEAT_F_PER_S      (0.0055f)           /* ~3.3 F per 10 min of heating  */
#define SIM_COOL_F_PER_S      (0.0045f)           /* ~2.7 F per 10 min of cooling  */
#define SIM_INTERNAL_F_PER_S  (0.0004f)
#define SIM_MIX_PER_S         (1.0f / 1800.0f)    /* room-to-room mixing           */
/* Outdoor temperature (deci-F) at each hour 00..24: 50 F mean, +/-9 F swing,
 * coldest ~05:00, warmest ~17:00. Table avoids a libm dependency. */
static const int16_t s_outdoor_hourly[25] = { 477, 455, 436, 422, 413, 410, 413, 422, 436, 455, 477, 500, 523, 545, 564, 578, 587, 590, 587, 578, 564, 545, 523, 500, 477 };

static const float s_room_gain_bias[TSTAT_SENSOR_COUNT] = { 0.0f, -0.00025f, 0.00035f };

static tdeci_t clamp_t(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) { return (tdeci_t)lo; }
    if (v > hi) { return (tdeci_t)hi; }
    return (tdeci_t)v;
}

static int32_t round_f(float v)
{
    return (int32_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}

static uint32_t rng_next(tstat_model_t * m)
{
    /* xorshift32 - deterministic, no libc dependency */
    uint32_t x = m->sim.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    m->sim.rng = x;
    return x;
}

static uint16_t minute_of_day(const tstat_model_t * m)
{
    return (uint16_t)((m->clock_s % TSTAT_SECONDS_PER_DAY) / 60u);
}

static int8_t schedule_index_for(const tstat_model_t * m, uint16_t minute)
{
    int8_t idx = (int8_t)(m->sched_count - 1);    /* wraps from previous day */
    for (uint8_t i = 0; i < m->sched_count; i++) {
        if (m->sched[i].start_min <= minute) { idx = (int8_t)i; }
    }
    return idx;
}

static void enforce_deadband(tstat_model_t * m, tstat_setpoints_t * sp, uint8_t changed)
{
    sp->heat = clamp_t(sp->heat, TSTAT_HEAT_SP_MIN, TSTAT_HEAT_SP_MAX);
    sp->cool = clamp_t(sp->cool, TSTAT_COOL_SP_MIN, TSTAT_COOL_SP_MAX);
    if ((int32_t)sp->cool - (int32_t)sp->heat < m->deadband) {
        if (0u == changed) {
            sp->cool = clamp_t((int32_t)sp->heat + m->deadband, TSTAT_COOL_SP_MIN, TSTAT_COOL_SP_MAX);
            sp->heat = clamp_t((int32_t)sp->cool - m->deadband, TSTAT_HEAT_SP_MIN, TSTAT_HEAT_SP_MAX);
        } else {
            sp->heat = clamp_t((int32_t)sp->cool - m->deadband, TSTAT_HEAT_SP_MIN, TSTAT_HEAT_SP_MAX);
            sp->cool = clamp_t((int32_t)sp->heat + m->deadband, TSTAT_COOL_SP_MIN, TSTAT_COOL_SP_MAX);
        }
    }
}

static void update_effective_setpoints(tstat_model_t * m)
{
    int8_t idx = schedule_index_for(m, minute_of_day(m));
    if (idx != m->sched_index) {
        m->sched_index = idx;
        m->hold = false;                     /* "hold until next activity" */
    }
    m->active_comfort = (tstat_comfort_t)m->sched[idx].comfort;
    m->active_sp = m->hold ? m->hold_sp : m->comfort[m->active_comfort];
}

static void update_indoor_average(tstat_model_t * m)
{
    int32_t sum = 0;
    int32_t n   = 0;
    for (uint8_t i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        if (m->sensors[i].participating) { sum += m->sensors[i].temp; n++; }
    }
    if (0 == n) { sum = m->sensors[0].temp; n = 1; }
    m->indoor = (tdeci_t)((sum + (n / 2)) / n);
}

static void simulate_plant_1s(tstat_model_t * m)
{
    const uint32_t sod  = m->clock_s % TSTAT_SECONDS_PER_DAY;
    const uint32_t hour = sod / 3600u;
    const int32_t  a    = s_outdoor_hourly[hour];
    const int32_t  b    = s_outdoor_hourly[hour + 1u];
    const int32_t  out  = a + ((b - a) * (int32_t)(sod % 3600u)) / 3600;
    const float    t_out = (float)out / 10.0f;
    m->outdoor = (tdeci_t)out;

    float mean = 0.0f;
    for (uint8_t i = 0; i < TSTAT_SENSOR_COUNT; i++) { mean += m->sim.room_f[i]; }
    mean /= (float)TSTAT_SENSOR_COUNT;

    for (uint8_t i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        float t  = m->sim.room_f[i];
        float dt = SIM_LOSS_PER_S * (t_out - t) + SIM_INTERNAL_F_PER_S + s_room_gain_bias[i]
                 + SIM_MIX_PER_S * (mean - t);
        if (m->out_w1) { dt += SIM_HEAT_F_PER_S; }
        if (m->out_y1) { dt -= SIM_COOL_F_PER_S; }
        if ((0u == i) && m->sim.measured_override) { continue; } /* real sensor owns room 0 */
        m->sim.room_f[i] = t + dt;
    }

    /* humidity: cooling dehumidifies, otherwise relaxes toward 46 %RH */
    if (!m->sim.measured_override) {
        if (m->out_y1) { m->sim.humidity -= 0.0025f; }
        else           { m->sim.humidity += (46.0f - m->sim.humidity) / 7200.0f; }
        if (m->sim.humidity < 25.0f) { m->sim.humidity = 25.0f; }
        m->humidity = (uint8_t)round_f(m->sim.humidity);
    }

    for (uint8_t i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        m->sensors[i].temp = (tdeci_t)round_f(m->sim.room_f[i] * 10.0f);
    }

    /* occupancy: re-evaluated once per simulated minute */
    if (0u == (m->clock_s % 60u)) {
        for (uint8_t i = 0; i < TSTAT_SENSOR_COUNT; i++) {
            uint32_t p;
            switch (m->active_comfort) {
                case TSTAT_COMFORT_HOME:  p = (i == 0u) ? 70u : 45u; break;
                case TSTAT_COMFORT_SLEEP: p = (i == 1u) ? 90u : 5u;  break;
                default:                  p = 2u;                    break;
            }
            /* re-sample roughly every 10 simulated minutes to avoid flicker */
            if ((rng_next(m) % 10u) == 0u) {
                m->sensors[i].occupied = (uint8_t)(((rng_next(m) % 100u) < p) ? 1u : 0u);
            }
        }
    }
}

static void control_1s(tstat_model_t * m)
{
    const tdeci_t t    = m->indoor;
    const tdeci_t hsp  = m->active_sp.heat;
    const tdeci_t csp  = m->active_sp.cool;
    const tdeci_t diff = m->differential;
    tstat_output_t want = TSTAT_OUT_NONE;

    const bool heat_allowed = (TSTAT_MODE_HEAT == m->mode) || (TSTAT_MODE_AUTO == m->mode);
    const bool cool_allowed = (TSTAT_MODE_COOL == m->mode) || (TSTAT_MODE_AUTO == m->mode);

    /* demand with hysteresis, based on what is currently running */
    if (heat_allowed) {
        if (TSTAT_OUT_HEAT == m->output) { if (t < hsp)        { want = TSTAT_OUT_HEAT; } }
        else                             { if (t <= hsp - diff) { want = TSTAT_OUT_HEAT; } }
    }
    if ((TSTAT_OUT_NONE == want) && cool_allowed) {
        if (TSTAT_OUT_COOL == m->output) { if (t > csp)        { want = TSTAT_OUT_COOL; } }
        else                             { if (t >= csp + diff) { want = TSTAT_OUT_COOL; } }
    }

    m->output_since_s++;
    m->protect_remaining_s = 0u;

    if (want != m->output) {
        if (TSTAT_OUT_NONE != m->output) {
            /* running -> must satisfy minimum on-time (unless system switched OFF) */
            if ((m->output_since_s >= m->min_on_s) || (TSTAT_MODE_OFF == m->mode)) {
                m->output = TSTAT_OUT_NONE;          /* always stop first */
                m->output_since_s = 0u;
            } else {
                m->protect_remaining_s = (uint16_t)(m->min_on_s - m->output_since_s);
            }
        } else {
            /* idle -> must satisfy minimum off-time */
            if (m->output_since_s >= m->min_off_s) {
                m->output = want;
                m->output_since_s = 0u;
            } else {
                m->protect_remaining_s = (uint16_t)(m->min_off_s - m->output_since_s);
            }
        }
    }

    m->out_w1 = (TSTAT_OUT_HEAT == m->output);
    m->out_y1 = (TSTAT_OUT_COOL == m->output);
    m->out_g  = (TSTAT_FAN_ON == m->fan) || m->out_w1 || m->out_y1;

    if (m->out_w1)                                          { m->hvac = TSTAT_HVAC_HEATING; }
    else if (m->out_y1)                                     { m->hvac = TSTAT_HVAC_COOLING; }
    else if ((TSTAT_OUT_HEAT == want) && m->protect_remaining_s) { m->hvac = TSTAT_HVAC_WAIT_HEAT; }
    else if ((TSTAT_OUT_COOL == want) && m->protect_remaining_s) { m->hvac = TSTAT_HVAC_WAIT_COOL; }
    else if (m->out_g)                                      { m->hvac = TSTAT_HVAC_FAN_ONLY; }
    else                                                    { m->hvac = TSTAT_HVAC_IDLE; }

    if (m->out_w1) { m->today_heat_s++; }
    if (m->out_y1) { m->today_cool_s++; }
}

static void day_rollover(tstat_model_t * m)
{
    for (uint8_t i = 0; i < (TSTAT_HISTORY_DAYS - 1u); i++) {
        m->hist_heat_min[i] = m->hist_heat_min[i + 1u];
        m->hist_cool_min[i] = m->hist_cool_min[i + 1u];
    }
    m->hist_heat_min[TSTAT_HISTORY_DAYS - 1u] = (uint16_t)(m->today_heat_s / 60u);
    m->hist_cool_min[TSTAT_HISTORY_DAYS - 1u] = (uint16_t)(m->today_cool_s / 60u);
    m->today_heat_s = 0u;
    m->today_cool_s = 0u;
}

/* =================================================================== public */

void tstat_model_init(tstat_model_t * m)
{
    static const tstat_sched_entry_t def_sched[] = {
        {  6u * 60u + 30u, TSTAT_COMFORT_HOME  },
        {  8u * 60u + 30u, TSTAT_COMFORT_AWAY  },
        { 17u * 60u + 30u, TSTAT_COMFORT_HOME  },
        { 22u * 60u + 30u, TSTAT_COMFORT_SLEEP },
    };
    /* demo history (minutes of runtime per day), oldest first */
    static const uint16_t def_heat[TSTAT_HISTORY_DAYS] = { 182u, 165u, 201u, 140u, 118u, 156u, 171u };
    static const uint16_t def_cool[TSTAT_HISTORY_DAYS] = {   0u,  12u,   0u,   0u,  25u,   8u,   0u };
    static const char * const names[TSTAT_SENSOR_COUNT] = { "Living Room", "Bedroom", "Kitchen" };
    static const float start_temp[TSTAT_SENSOR_COUNT]   = { 64.6f, 63.2f, 65.4f };

    memset(m, 0, sizeof(*m));

    m->mode         = TSTAT_MODE_HEAT;
    m->fan          = TSTAT_FAN_AUTO;
    m->differential = TSTAT_DEFAULT_DIFF;
    m->deadband     = TSTAT_DEFAULT_DEADBAND;
    m->min_on_s     = (uint16_t)TSTAT_DEFAULT_MIN_ON_S;
    m->min_off_s    = (uint16_t)TSTAT_DEFAULT_MIN_OFF_S;

    m->comfort[TSTAT_COMFORT_HOME]  = (tstat_setpoints_t){ 700, 760 };
    m->comfort[TSTAT_COMFORT_AWAY]  = (tstat_setpoints_t){ 620, 800 };
    m->comfort[TSTAT_COMFORT_SLEEP] = (tstat_setpoints_t){ 660, 780 };

    m->sched_count = (uint8_t)(sizeof(def_sched) / sizeof(def_sched[0]));
    memcpy(m->sched, def_sched, sizeof(def_sched));

    memcpy(m->hist_heat_min, def_heat, sizeof(def_heat));
    memcpy(m->hist_cool_min, def_cool, sizeof(def_cool));

    for (uint8_t i = 0; i < TSTAT_SENSOR_COUNT; i++) {
        strncpy(m->sensors[i].name, names[i], TSTAT_NAME_LEN - 1u);
        m->sensors[i].participating = 1u;
        m->sim.room_f[i] = start_temp[i];
        m->sensors[i].temp = (tdeci_t)round_f(start_temp[i] * 10.0f);
    }
    m->sensors[0].occupied = 0u;

    m->sim.humidity = 44.0f;
    m->humidity     = 44u;
    m->sim.rng      = 0x2545F491u;

    /* Monday 17:20 - schedule switches Away -> Home at 17:30, which makes a
     * nice live demo (heating starts within ~1 real minute at 10x speed). */
    m->clock_s        = 17u * 3600u + 20u * 60u;
    m->sched_index    = -1;
    m->output_since_s = m->min_off_s;     /* allow an immediate start */

    update_effective_setpoints(m);
    update_indoor_average(m);
    simulate_plant_1s(m);
}

void tstat_model_step(tstat_model_t * m, uint32_t dt_s)
{
    while (dt_s-- > 0u) {
        m->clock_s++;
        if (m->clock_s >= TSTAT_SECONDS_PER_WEEK) { m->clock_s = 0u; }
        if (0u == (m->clock_s % TSTAT_SECONDS_PER_DAY)) { day_rollover(m); }

        update_effective_setpoints(m);
        simulate_plant_1s(m);
        update_indoor_average(m);
        control_1s(m);
    }
}

void tstat_model_inject_measurement(tstat_model_t * m, tdeci_t temp, uint8_t rh)
{
    m->sim.measured_override = true;
    m->sim.room_f[0]   = (float)temp / 10.0f;
    m->sensors[0].temp = temp;
    m->humidity        = rh;
    m->sim.humidity    = (float)rh;
    update_indoor_average(m);
}

void tstat_model_set_mode(tstat_model_t * m, tstat_mode_t mode)
{
    if (mode < TSTAT_MODE_COUNT) { m->mode = mode; }
}

void tstat_model_set_fan(tstat_model_t * m, tstat_fan_t fan)
{
    if (fan < TSTAT_FAN_COUNT) { m->fan = fan; }
}

void tstat_model_set_hold_setpoint(tstat_model_t * m, uint8_t which, tdeci_t value)
{
    if (!m->hold) { m->hold_sp = m->comfort[m->active_comfort]; m->hold = true; }
    if (0u == which) { m->hold_sp.heat = value; } else { m->hold_sp.cool = value; }
    enforce_deadband(m, &m->hold_sp, which);
    m->active_sp = m->hold_sp;
}

void tstat_model_shift_hold(tstat_model_t * m, tdeci_t delta)
{
    if (!m->hold) { m->hold_sp = m->comfort[m->active_comfort]; m->hold = true; }
    int32_t h = (int32_t)m->hold_sp.heat + delta;
    int32_t c = (int32_t)m->hold_sp.cool + delta;
    /* keep the pair inside both ranges */
    if (h < TSTAT_HEAT_SP_MIN) { c += TSTAT_HEAT_SP_MIN - h; h = TSTAT_HEAT_SP_MIN; }
    if (c > TSTAT_COOL_SP_MAX) { h -= c - TSTAT_COOL_SP_MAX; c = TSTAT_COOL_SP_MAX; }
    m->hold_sp.heat = (tdeci_t)h;
    m->hold_sp.cool = (tdeci_t)c;
    enforce_deadband(m, &m->hold_sp, 0u);
    m->active_sp = m->hold_sp;
}

void tstat_model_resume_schedule(tstat_model_t * m)
{
    m->hold = false;
    m->active_sp = m->comfort[m->active_comfort];
}

void tstat_model_set_comfort(tstat_model_t * m, tstat_comfort_t c, tdeci_t heat, tdeci_t cool)
{
    if (c >= TSTAT_COMFORT_COUNT) { return; }
    tstat_setpoints_t sp = { heat, cool };
    uint8_t changed = (uint8_t)((heat != m->comfort[c].heat) ? 0u : 1u);
    enforce_deadband(m, &sp, changed);
    m->comfort[c] = sp;
    if (!m->hold) { m->active_sp = m->comfort[m->active_comfort]; }
}

void tstat_model_set_sensor_participation(tstat_model_t * m, uint8_t idx, bool on)
{
    if (idx >= TSTAT_SENSOR_COUNT) { return; }
    m->sensors[idx].participating = on ? 1u : 0u;
    update_indoor_average(m);
}

uint16_t tstat_model_next_transition_min(const tstat_model_t * m)
{
    int8_t next = (int8_t)(m->sched_index + 1);
    if (next >= (int8_t)m->sched_count) { next = 0; }
    return m->sched[next].start_min;
}

const char * tstat_comfort_name(tstat_comfort_t c)
{
    static const char * const n[TSTAT_COMFORT_COUNT] = { "Home", "Away", "Sleep" };
    return (c < TSTAT_COMFORT_COUNT) ? n[c] : "?";
}

const char * tstat_mode_name(tstat_mode_t mode)
{
    static const char * const n[TSTAT_MODE_COUNT] = { "Off", "Heat", "Cool", "Auto" };
    return (mode < TSTAT_MODE_COUNT) ? n[mode] : "?";
}

int16_t tstat_deci_f_to_deci_c(tdeci_t f)
{
    int32_t num = ((int32_t)f - 320) * 5;
    return (int16_t)((num >= 0) ? ((num + 4) / 9) : ((num - 4) / 9));
}

tdeci_t tstat_deci_c_to_deci_f(int16_t c)
{
    int32_t num = (int32_t)c * 9;
    int32_t q   = (num >= 0) ? ((num + 2) / 5) : ((num - 2) / 5);
    return (tdeci_t)(q + 320);
}
