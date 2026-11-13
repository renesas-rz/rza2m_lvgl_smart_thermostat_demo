/**
 * @file thermostat_app.h
 * @brief Entry point for the RZ/A2M FreeRTOS + LVGL smart-thermostat demo.
 */
#ifndef THERMOSTAT_APP_H
#define THERMOSTAT_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/** Application selector used by rm_lvgl_port.c: 1 = thermostat, 0 = original oven demo */
#ifndef RZA2M_APP_THERMOSTAT
#define RZA2M_APP_THERMOSTAT   (1)
#endif

/**
 * UI skin selector (same model, control task and HAL for both):
 *   0 = original dial UI          (thermostat_ui.c)
 *   1 = premium minimal UI        (thermostat_ui_elite.c)  - default
 */
#ifndef THERMOSTAT_UI_STYLE
#define THERMOSTAT_UI_STYLE    (1)
#endif

void thermostat_app_start(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_APP_H */
