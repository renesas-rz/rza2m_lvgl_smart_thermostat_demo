/**
 * @file thermostat_app.c
 */
#include <stdio.h>
#include "thermostat_app.h"
#include "thermostat_service.h"
#if (1 == THERMOSTAT_UI_STYLE)
#include "thermostat_ui_elite.h"
#else
#include "thermostat_ui.h"
#endif

void thermostat_app_start(void)
{
    if (!thermostat_service_start()) {
        printf("thermostat: service start failed - UI will show static data\r\n");
    }
#if (1 == THERMOSTAT_UI_STYLE)
    thermostat_ui_elite_create();
#else
    thermostat_ui_create();
#endif
    printf("thermostat: UI style %d created (FreeRTOS %s)\r\n", THERMOSTAT_UI_STYLE, thermostat_service_rtos_version());
}
