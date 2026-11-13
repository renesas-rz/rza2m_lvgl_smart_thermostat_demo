/**
 * @file thermostat_hal.h
 * @brief Board hooks for the thermostat demo.
 *
 * All functions have weak default implementations in thermostat_hal.c (pure
 * simulation). Override any of them in board code with a strong definition -
 * no change to the thermostat sources is needed.
 *
 * Context: every function except thermostat_hal_set_backlight() is called from
 * the thermostat control task (not the LVGL task, not an ISR).
 */
#ifndef THERMOSTAT_HAL_H
#define THERMOSTAT_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "thermostat_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One-time init (I2C sensor, relay GPIOs ...). Called at control-task start. */
void thermostat_hal_init(void);

/**
 * Read the local temperature/humidity sensor.
 * @return true if a valid reading was produced; false = keep using the simulation.
 */
bool thermostat_hal_read_sensor(tdeci_t * temp_deci_f, uint8_t * rh_percent);

/**
 * Drive equipment outputs. Called every control tick with the requested state.
 * W1 = heat stage 1, Y1 = compressor stage 1, G = fan.
 * In a real product route these to a safety controller (e.g. RA/RX MCU) rather
 * than switching relays directly from the HMI processor.
 */
void thermostat_hal_write_outputs(bool w1, bool y1, bool g);

/** Display backlight, 10..100 %. Called from the LVGL task. */
void thermostat_hal_set_backlight(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_HAL_H */
