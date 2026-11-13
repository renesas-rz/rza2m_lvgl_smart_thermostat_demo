/**
 * @file thermostat_service.h
 * @brief FreeRTOS thermostat control task + thread-safe UI interface.
 *
 * Threading contract
 *  - The control task ("Tstat Ctrl") is the ONLY owner of the live tstat_model_t.
 *  - The LVGL task never touches the live model. It
 *      * sends commands with thermostat_service_send()        (non-blocking queue post)
 *      * reads state with thermostat_service_get_snapshot()   (mutex-protected copy)
 *  - No LVGL API is ever called from the control task, so LVGL needs no extra locking.
 */
#ifndef THERMOSTAT_SERVICE_H
#define THERMOSTAT_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "thermostat_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------ configuration */
#ifndef THERMOSTAT_CTRL_TASK_PRIO
#define THERMOSTAT_CTRL_TASK_PRIO      (7)      /* LVGL manager task runs at 6 */
#endif
#ifndef THERMOSTAT_CTRL_TASK_STACK
#define THERMOSTAT_CTRL_TASK_STACK     (1024u)  /* words                        */
#endif
#ifndef THERMOSTAT_CTRL_PERIOD_MS
#define THERMOSTAT_CTRL_PERIOD_MS      (1000u)  /* control / simulation tick    */
#endif
#ifndef THERMOSTAT_CMD_QUEUE_LEN
#define THERMOSTAT_CMD_QUEUE_LEN       (16u)
#endif
#ifndef THERMOSTAT_DEFAULT_SIM_SPEED
#define THERMOSTAT_DEFAULT_SIM_SPEED   (10u)    /* simulated seconds per tick   */
#endif

typedef enum {
    TSTAT_CMD_SET_MODE = 0,        /* a = tstat_mode_t                         */
    TSTAT_CMD_SET_FAN,             /* a = tstat_fan_t                          */
    TSTAT_CMD_SET_SETPOINT,        /* a = 0 heat / 1 cool, v1 = deci-F         */
    TSTAT_CMD_SHIFT_SETPOINTS,     /* v1 = delta deci-F (AUTO mode)            */
    TSTAT_CMD_RESUME_SCHEDULE,
    TSTAT_CMD_SET_COMFORT,         /* a = comfort, v1 = heat, v2 = cool        */
    TSTAT_CMD_SET_SENSOR_PART,     /* a = sensor index, v1 = 0/1               */
    TSTAT_CMD_SET_SIM_SPEED        /* v1 = simulated seconds per tick          */
} tstat_cmd_id_t;

typedef struct {
    tstat_cmd_id_t id;
    uint8_t        a;
    int16_t        v1;
    int16_t        v2;
} tstat_cmd_t;

/** Snapshot published to the UI. */
typedef struct {
    tstat_model_t model;
    uint16_t      sim_speed;
    uint32_t      seq;              /* increments on every publish */
} tstat_snapshot_t;

/** Create queue, mutex and control task. Safe to call once from any task. */
bool thermostat_service_start(void);

/** Non-blocking command post. Returns false if the queue is full / not started. */
bool thermostat_service_send(const tstat_cmd_t * cmd);

/**
 * Copy the latest snapshot if it is newer than *last_seq.
 * @return true if *out was updated.
 */
bool thermostat_service_get_snapshot(tstat_snapshot_t * out, uint32_t * last_seq);

/** FreeRTOS kernel version string (for the About panel). */
const char * thermostat_service_rtos_version(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_SERVICE_H */
