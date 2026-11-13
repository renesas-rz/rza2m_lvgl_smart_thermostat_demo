/**
 * @file thermostat_hal.c
 * @brief Weak default (simulation) implementations of the thermostat board hooks.
 */
#include "thermostat_hal.h"

#define TSTAT_WEAK __attribute__((weak))

TSTAT_WEAK void thermostat_hal_init(void)
{
    /* nothing to do in simulation */
}

TSTAT_WEAK bool thermostat_hal_read_sensor(tdeci_t * temp_deci_f, uint8_t * rh_percent)
{
    (void)temp_deci_f;
    (void)rh_percent;
    return false;     /* no physical sensor: model simulation is used */
}

TSTAT_WEAK void thermostat_hal_write_outputs(bool w1, bool y1, bool g)
{
    (void)w1;
    (void)y1;
    (void)g;          /* e.g. drive EVK LEDs here to visualise W1/Y1/G */
}

TSTAT_WEAK void thermostat_hal_set_backlight(uint8_t percent)
{
    (void)percent;    /* RSK panel backlight is not PWM-controlled in this BSP */
}
