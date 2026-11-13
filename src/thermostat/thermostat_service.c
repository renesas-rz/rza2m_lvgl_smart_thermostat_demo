/**
 * @file thermostat_service.c
 * @brief FreeRTOS control task that owns the thermostat model.
 *
 * Loop:
 *   wait on command queue until the next control deadline
 *     - command received  -> apply, publish snapshot immediately (snappy UI)
 *     - deadline reached  -> read HAL sensor, step model, write HAL outputs, publish
 */
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "thermostat_service.h"
#include "thermostat_hal.h"

static tstat_model_t     s_model;          /* owned by control task only */
static tstat_snapshot_t  s_snapshot;       /* guarded by s_snap_mutex    */
static SemaphoreHandle_t s_snap_mutex;
static QueueHandle_t     s_cmd_queue;
static TaskHandle_t      s_task;
static uint16_t          s_sim_speed = (uint16_t)THERMOSTAT_DEFAULT_SIM_SPEED;
static uint32_t          s_seq;

static void publish(void)
{
    if (pdTRUE == xSemaphoreTake(s_snap_mutex, portMAX_DELAY)) {
        s_seq++;
        memcpy(&s_snapshot.model, &s_model, sizeof(s_model));
        s_snapshot.sim_speed = s_sim_speed;
        s_snapshot.seq       = s_seq;
        (void)xSemaphoreGive(s_snap_mutex);
    }
}

static void apply_command(const tstat_cmd_t * c)
{
    switch (c->id) {
        case TSTAT_CMD_SET_MODE:
            tstat_model_set_mode(&s_model, (tstat_mode_t)c->a);
            break;
        case TSTAT_CMD_SET_FAN:
            tstat_model_set_fan(&s_model, (tstat_fan_t)c->a);
            break;
        case TSTAT_CMD_SET_SETPOINT:
            tstat_model_set_hold_setpoint(&s_model, c->a, (tdeci_t)c->v1);
            break;
        case TSTAT_CMD_SHIFT_SETPOINTS:
            tstat_model_shift_hold(&s_model, (tdeci_t)c->v1);
            break;
        case TSTAT_CMD_RESUME_SCHEDULE:
            tstat_model_resume_schedule(&s_model);
            break;
        case TSTAT_CMD_SET_COMFORT:
            tstat_model_set_comfort(&s_model, (tstat_comfort_t)c->a, (tdeci_t)c->v1, (tdeci_t)c->v2);
            break;
        case TSTAT_CMD_SET_SENSOR_PART:
            tstat_model_set_sensor_participation(&s_model, c->a, (0 != c->v1));
            break;
        case TSTAT_CMD_SET_SIM_SPEED:
            if ((c->v1 >= 1) && (c->v1 <= 600)) { s_sim_speed = (uint16_t)c->v1; }
            break;
        default:
            break;
    }
}

static void control_tick(void)
{
    tdeci_t t;
    uint8_t rh;

    if (thermostat_hal_read_sensor(&t, &rh)) {
        tstat_model_inject_measurement(&s_model, t, rh);
    }
    tstat_model_step(&s_model, s_sim_speed);
    thermostat_hal_write_outputs(s_model.out_w1, s_model.out_y1, s_model.out_g);
}

static void thermostat_ctrl_task(void * arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(THERMOSTAT_CTRL_PERIOD_MS);
    TickType_t next = xTaskGetTickCount() + period;
    tstat_cmd_t cmd;

    thermostat_hal_init();
    publish();

    for (;;) {
        TickType_t now  = xTaskGetTickCount();
        TickType_t wait = ((int32_t)(next - now) > 0) ? (next - now) : 0;

        if (pdTRUE == xQueueReceive(s_cmd_queue, &cmd, wait)) {
            apply_command(&cmd);
            /* drain any burst (e.g. fast +/- taps) before publishing */
            while (pdTRUE == xQueueReceive(s_cmd_queue, &cmd, 0)) { apply_command(&cmd); }
            publish();
            continue;
        }

        now = xTaskGetTickCount();
        if ((int32_t)(now - next) >= 0) {
            control_tick();
            publish();
            next += period;
            if ((int32_t)(now - next) > (int32_t)period) { next = now + period; } /* resync after stall */
        }
    }
}

bool thermostat_service_start(void)
{
    if (NULL != s_task) { return true; }

    tstat_model_init(&s_model);

    s_snap_mutex = xSemaphoreCreateMutex();
    s_cmd_queue  = xQueueCreate(THERMOSTAT_CMD_QUEUE_LEN, sizeof(tstat_cmd_t));
    if ((NULL == s_snap_mutex) || (NULL == s_cmd_queue)) {
        printf("thermostat: queue/mutex allocation failed\r\n");
        return false;
    }

    /* publish an initial snapshot so the UI has data before the task runs */
    publish();

    if (pdPASS != xTaskCreate(thermostat_ctrl_task, "Tstat Ctrl",
                              THERMOSTAT_CTRL_TASK_STACK, NULL,
                              THERMOSTAT_CTRL_TASK_PRIO, &s_task)) {
        printf("thermostat: control task creation failed\r\n");
        s_task = NULL;
        return false;
    }
    return true;
}

bool thermostat_service_send(const tstat_cmd_t * cmd)
{
    if (NULL == s_cmd_queue) { return false; }
    return (pdTRUE == xQueueSend(s_cmd_queue, cmd, 0));
}

bool thermostat_service_get_snapshot(tstat_snapshot_t * out, uint32_t * last_seq)
{
    bool updated = false;
    if (NULL == s_snap_mutex) { return false; }
    if (pdTRUE == xSemaphoreTake(s_snap_mutex, pdMS_TO_TICKS(5))) {
        if (s_snapshot.seq != *last_seq) {
            memcpy(out, &s_snapshot, sizeof(*out));
            *last_seq = s_snapshot.seq;
            updated = true;
        }
        (void)xSemaphoreGive(s_snap_mutex);
    }
    return updated;
}

const char * thermostat_service_rtos_version(void)
{
    return tskKERNEL_VERSION_NUMBER;
}
