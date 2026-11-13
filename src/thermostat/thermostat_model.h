/**
 * @file thermostat_model.h
 * @brief Smart-thermostat state model and control logic (RTOS- and GUI-independent).
 *
 * This module has NO dependency on LVGL or FreeRTOS. It is owned exclusively by the
 * thermostat control task (thermostat_service.c); the LVGL task only ever sees a copy
 * (snapshot) of tstat_model_t.
 *
 * Units: all temperatures are int16_t tenths of a degree Fahrenheit ("deci-F"),
 *        e.g. 705 == 70.5 degF. Conversion to degC is a presentation concern.
 */
#ifndef THERMOSTAT_MODEL_H
#define THERMOSTAT_MODEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- limits / tuning */
#define TSTAT_HEAT_SP_MIN        (450)   /* 45.0 F */
#define TSTAT_HEAT_SP_MAX        (790)   /* 79.0 F */
#define TSTAT_COOL_SP_MIN        (620)   /* 62.0 F */
#define TSTAT_COOL_SP_MAX        (920)   /* 92.0 F */
#define TSTAT_DIAL_MIN           (TSTAT_HEAT_SP_MIN)
#define TSTAT_DIAL_MAX           (TSTAT_COOL_SP_MAX)

#define TSTAT_DEFAULT_DIFF       (5)     /* 0.5 F start differential           */
#define TSTAT_DEFAULT_DEADBAND   (30)    /* 3.0 F min heat/cool separation     */
#define TSTAT_DEFAULT_MIN_ON_S   (300u)  /* compressor/burner minimum run time */
#define TSTAT_DEFAULT_MIN_OFF_S  (300u)  /* anti-short-cycle minimum off time  */

#define TSTAT_SCHED_MAX          (8)
#define TSTAT_SENSOR_COUNT       (3)
#define TSTAT_HISTORY_DAYS       (7)
#define TSTAT_NAME_LEN           (16)

#define TSTAT_SECONDS_PER_DAY    (86400u)
#define TSTAT_SECONDS_PER_WEEK   (7u * TSTAT_SECONDS_PER_DAY)

typedef int16_t tdeci_t;

typedef enum {
    TSTAT_MODE_OFF = 0,
    TSTAT_MODE_HEAT,
    TSTAT_MODE_COOL,
    TSTAT_MODE_AUTO,
    TSTAT_MODE_COUNT
} tstat_mode_t;

typedef enum {
    TSTAT_FAN_AUTO = 0,
    TSTAT_FAN_ON,
    TSTAT_FAN_COUNT
} tstat_fan_t;

typedef enum {
    TSTAT_HVAC_IDLE = 0,
    TSTAT_HVAC_HEATING,
    TSTAT_HVAC_COOLING,
    TSTAT_HVAC_FAN_ONLY,
    TSTAT_HVAC_WAIT_HEAT,     /* call for heat blocked by min-off protection */
    TSTAT_HVAC_WAIT_COOL      /* call for cool blocked by min-off protection */
} tstat_hvac_t;

typedef enum {
    TSTAT_COMFORT_HOME = 0,
    TSTAT_COMFORT_AWAY,
    TSTAT_COMFORT_SLEEP,
    TSTAT_COMFORT_COUNT
} tstat_comfort_t;

typedef enum {
    TSTAT_OUT_NONE = 0,
    TSTAT_OUT_HEAT,
    TSTAT_OUT_COOL
} tstat_output_t;

typedef struct {
    tdeci_t heat;
    tdeci_t cool;
} tstat_setpoints_t;

typedef struct {
    uint16_t start_min;       /* minute of day, 0..1439 */
    uint8_t  comfort;         /* tstat_comfort_t        */
} tstat_sched_entry_t;

typedef struct {
    char    name[TSTAT_NAME_LEN];
    tdeci_t temp;
    uint8_t occupied;
    uint8_t participating;
} tstat_sensor_t;

/* Private simulation state (kept in the struct so the whole model is copyable). */
typedef struct {
    float    room_f[TSTAT_SENSOR_COUNT];
    float    humidity;
    uint32_t rng;
    bool     measured_override;   /* true once the HAL has supplied a real reading */
} tstat_sim_t;

typedef struct {
    /* ---------- configuration (written via commands) */
    tstat_mode_t        mode;
    tstat_fan_t         fan;
    tstat_setpoints_t   comfort[TSTAT_COMFORT_COUNT];
    tstat_sched_entry_t sched[TSTAT_SCHED_MAX];
    uint8_t             sched_count;
    bool                hold;          /* temporary hold until next schedule transition */
    tstat_setpoints_t   hold_sp;
    tdeci_t             differential;
    tdeci_t             deadband;
    uint16_t            min_on_s;
    uint16_t            min_off_s;

    /* ---------- live status */
    uint32_t            clock_s;       /* seconds since Monday 00:00 (simulated RTC) */
    int8_t              sched_index;   /* active schedule entry                      */
    tstat_comfort_t     active_comfort;
    tstat_setpoints_t   active_sp;     /* effective setpoints after hold/schedule    */
    tdeci_t             indoor;        /* control temperature (sensor average)       */
    tdeci_t             outdoor;
    uint8_t             humidity;      /* %RH                                        */
    tstat_hvac_t        hvac;
    tstat_output_t      output;        /* what the equipment is doing                */
    bool                out_w1;        /* heat relay                                 */
    bool                out_y1;        /* compressor relay                           */
    bool                out_g;         /* fan relay                                  */
    uint32_t            output_since_s;/* seconds since last W1/Y1 change            */
    uint16_t            protect_remaining_s;

    /* ---------- runtime statistics */
    uint32_t            today_heat_s;
    uint32_t            today_cool_s;
    uint16_t            hist_heat_min[TSTAT_HISTORY_DAYS]; /* [0] oldest .. [6] yesterday */
    uint16_t            hist_cool_min[TSTAT_HISTORY_DAYS];

    tstat_sensor_t      sensors[TSTAT_SENSOR_COUNT];

    tstat_sim_t         sim;
} tstat_model_t;

/* ---------------------------------------------------------------- API */
void    tstat_model_init(tstat_model_t * m);

/** Advance the model by dt_s simulated seconds (internally sub-stepped at 1 s). */
void    tstat_model_step(tstat_model_t * m, uint32_t dt_s);

/** Inject a real measurement from the sensor HAL (replaces the simulated sensor 0). */
void    tstat_model_inject_measurement(tstat_model_t * m, tdeci_t temp, uint8_t rh);

void    tstat_model_set_mode(tstat_model_t * m, tstat_mode_t mode);
void    tstat_model_set_fan(tstat_model_t * m, tstat_fan_t fan);

/** Set a setpoint as a temporary hold. which: 0 = heat, 1 = cool. Enforces deadband. */
void    tstat_model_set_hold_setpoint(tstat_model_t * m, uint8_t which, tdeci_t value);

/** Shift both setpoints by delta (used in AUTO mode) as a temporary hold. */
void    tstat_model_shift_hold(tstat_model_t * m, tdeci_t delta);

void    tstat_model_resume_schedule(tstat_model_t * m);
void    tstat_model_set_comfort(tstat_model_t * m, tstat_comfort_t c, tdeci_t heat, tdeci_t cool);
void    tstat_model_set_sensor_participation(tstat_model_t * m, uint8_t idx, bool on);

/** Minute-of-day at which the current schedule period ends. */
uint16_t tstat_model_next_transition_min(const tstat_model_t * m);

const char * tstat_comfort_name(tstat_comfort_t c);
const char * tstat_mode_name(tstat_mode_t mode);

/* Unit helpers --------------------------------------------------------------- */
/** deci-F -> deci-C, rounded. */
int16_t tstat_deci_f_to_deci_c(tdeci_t f);
/** deci-C -> deci-F, rounded. */
tdeci_t tstat_deci_c_to_deci_f(int16_t c);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_MODEL_H */
