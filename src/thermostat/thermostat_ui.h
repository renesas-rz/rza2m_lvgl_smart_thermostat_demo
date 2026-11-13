/**
 * @file thermostat_ui.h
 * @brief ecobee-style thermostat HMI built with LVGL 9 (800 x 480 landscape).
 *
 * Must be called from the LVGL task only (after lv_init() and display/indev setup).
 */
#ifndef THERMOSTAT_UI_H
#define THERMOSTAT_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/** Build all pages on the active screen and start the 100 ms refresh timer. */
void thermostat_ui_create(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_UI_H */
