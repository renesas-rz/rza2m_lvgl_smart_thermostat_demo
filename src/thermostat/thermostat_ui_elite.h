/**
 * @file thermostat_ui_elite.h
 * @brief Premium "minimal" thermostat skin for RZ/A2M (LVGL 9, 800 x 480).
 *
 * Alternative UI for the same thermostat model / FreeRTOS service used by
 * thermostat_ui.c. Must be called from the LVGL task only.
 */
#ifndef THERMOSTAT_UI_ELITE_H
#define THERMOSTAT_UI_ELITE_H

#ifdef __cplusplus
extern "C" {
#endif

void thermostat_ui_elite_create(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_UI_ELITE_H */
