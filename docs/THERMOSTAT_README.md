# RZ/A2M Smart Thermostat Demo (FreeRTOS + LVGL 9)

This overlay converts `rza2m_oven_demo_gcc` (FreeRTOS + LVGL 9, 800 x 480 RSK panel) into an
ecobee-style smart thermostat HMI. The board, display, touch (FT5216), VDC, FreeRTOS and LVGL port
layers are **unchanged**. The oven demo is still in the project and can be selected again with one macro.

## 1. Install

1. Back up the project (or commit to git).
2. Copy the `src/` folder from this overlay over the project root, keeping the folder structure:
   - `src/thermostat/` — **new** (10 files)
   - `src/renesas/application/rm_lvgl_port.c` — **modified** (3 small hunks, see `patches/rm_lvgl_port.c.patch`)
3. In e² studio, select **Project › Refresh**, then **Build**. Folders under `src/` are compiled
   automatically, and `${ProjDirPath}/src` is already on the include path, so you don't need to change
   any build settings.
4. Flash and run. The serial console shows `LVGL Smart Thermostat demo ...` and `thermostat: UI created`.

To go back to the oven demo, add `RZA2M_APP_THERMOSTAT=0` to the compiler defines, or edit `src/thermostat/thermostat_app.h`.

## 2. Architecture

```
 main.c  ──►  "lvgl Manager" task (prio 6)            "Tstat Ctrl" task (prio 7)  NEW
              task_lvgl_main_task()                    thermostat_service.c
                lv_init(), board_init()                  owns tstat_model_t
                thermostat_app_start() ───creates──►     1 s control tick:
                  thermostat_ui_create()                   HAL read sensor
                loop: lv_timer_handler()                   model step (schedule, control,
                       │                                    anti-short-cycle, simulation)
                       │ 100 ms lv_timer                    HAL write W1/Y1/G
                       ├─ get_snapshot() ◄──mutex copy──  publish snapshot
                       └─ send_cmd()  ──FreeRTOS queue──► apply command, publish immediately
```

* **No LVGL calls outside the LVGL task.** The control task never touches LVGL, and the UI never
  touches the live model. Data moves only through a queue (UI → control) and a mutex-protected
  snapshot (control → UI). No extra LVGL locking is needed.
* `thermostat_model.c` is pure C (no FreeRTOS, no LVGL, no libm), so it can be unit-tested on a PC
  (`test_host/`).

| File | Role |
|---|---|
| `thermostat_app.c/.h` | Entry point `thermostat_app_start()` and the `RZA2M_APP_THERMOSTAT` selector |
| `thermostat_service.c/.h` | FreeRTOS control task, command queue, snapshot mutex |
| `thermostat_model.c/.h` | State, schedule, comfort settings, hold, control and protection logic, room/plant simulation |
| `thermostat_ui.c/.h` | LVGL UI: 6 pages, 100 ms snapshot refresh |
| `thermostat_hal.c/.h` | **Weak** board hooks: sensor, W1/Y1/G outputs, backlight |

## 3. UI pages (800 x 480)

| Page | Content |
|---|---|
| **Home** | 340 px setpoint dial (drag, or +/- with long-press repeat), 100 px setpoint digits, inside temperature, System / Fan / Humidity / Outside cards, comfort or hold banner with **Resume**, live W1/Y1/G or equipment-protection countdown |
| **Mode** | Heat / Cool / Auto / Off tiles, Fan Auto/On, protection parameters |
| **Schedule** | 24 h timeline (Home / Away / Sleep) with a live "now" marker, current and next period, editable heat/cool setpoints per comfort setting |
| **Sensors** | Living Room (thermostat), Bedroom, Kitchen: temperature, occupancy, "use for comfort" switch, control average |
| **Energy** | 7-day stacked heat/cool runtime bars (6 previous days plus today), daily and weekly totals |
| **Settings** | °F/°C, brightness (calls the HAL), demo speed 1x/10x/60x, live relay indicators, RZ/A2M / FreeRTOS / LVGL versions, FPS/CPU (if `LV_USE_PERF_MONITOR`) |

## 4. Control behaviour

* **HEAT:** heating starts at T ≤ SP − 0.5 °F and stops at T ≥ SP.
* **COOL:** cooling starts at T ≥ SP + 0.5 °F and stops at T ≤ SP.
* **AUTO:** both, with a 3 °F minimum deadband enforced on every edit. On Home, +/- shifts both setpoints together.
* **Minimum run 5 min / minimum off 5 min (anti-short-cycle).** Changeover between heat and cool always passes through OFF. Mode OFF stops the equipment immediately.
* **Setpoint changes create a hold** that lasts until the next schedule period (like ecobee's "until next activity"), or until **Resume** is pressed.
* Default schedule: Home 06:30, Away 08:30, Home 17:30, Sleep 22:30. The simulated clock starts on **Monday at 17:20 at 10x speed**, so the Away → Home change and the start of heating happen about one real minute after boot.

Host test result (3 simulated days): the shortest run and shortest off time were both exactly 300 s, holds were released at schedule changes, deadband was enforced, and °F/°C conversion round-trips within ±0.1 °F.

## 5. Moving from simulation to hardware

Override the weak hooks in your board code (no edits to `src/thermostat/` are needed):

```c
bool thermostat_hal_read_sensor(tdeci_t *t, uint8_t *rh) { /* e.g. I2C SHT4x on riic3 */ return true; }
void thermostat_hal_write_outputs(bool w1, bool y1, bool g) { /* GPIO, LEDs, or message to RA/RX safety MCU */ }
void thermostat_hal_set_backlight(uint8_t pct) { /* PWM */ }
```

Once `read_sensor` returns true, sensor 0 and humidity come from hardware, and the other rooms stay simulated.
For a production design, keep relay sequencing and the protection timers in a separate safety-qualified
controller. The RZ/A2M should stay the HMI and connectivity host.

## 6. Resources and configuration

| Item | Value |
|---|---|
| New FreeRTOS task | `Tstat Ctrl`, priority 7 (of 22), 1024-word (4 KB) stack, created from the FreeRTOS heap (`configTOTAL_HEAP_SIZE` = 128 KB) |
| Queue / mutex | 16 × 12-byte commands, plus 1 mutex |
| Snapshot | 240 bytes (static), copied at most every 100 ms |
| Fonts | Montserrat 12/14/16/20/24/28/40/48 (already enabled in `lv_conf.h`) plus the existing `Roboto_Medium_100_data` (digits and °) |
| New images | None |
| Overridable macros | `THERMOSTAT_CTRL_TASK_PRIO`, `THERMOSTAT_CTRL_TASK_STACK`, `THERMOSTAT_CTRL_PERIOD_MS`, `THERMOSTAT_DEFAULT_SIM_SPEED`, `THERMO_USE_ROBOTO_100`, `RZA2M_APP_THERMOSTAT` |

Rendering is tuned for the current SW renderer in DIRECT mode (`LV_USE_DRAW_DAVE2D = 0`). It uses one
screen with show/hide pages, no shadows, gradients, blur or opacity layers, and labels are only rewritten
when their text changes.

### Optional: reclaim flash
The oven sources are still compiled (their images and fonts stay in flash). After you accept the
thermostat, you can exclude `src/renesas-smart-oven/oven_ui/images` from the build. Keep
`fonts/Roboto_Medium_100_data.c`, or set `THERMO_USE_ROBOTO_100=0`. If you exclude the oven sources,
also remove the `#else` oven branch and the `oven_ui.h` include from `rm_lvgl_port.c`.

## 7. Verification status

* `thermostat_service.c` / `thermostat_hal.c` / `thermostat_app.c` were syntax-checked against **this
  project's** FreeRTOS kernel headers, `ARM_CA9_RZA2M` portmacro and `FreeRTOSConfig.h`.
* `thermostat_ui.c` was compiled warning-free (`-Wall -Wextra -Wformat=2 -Wshadow`) against LVGL 9 API
  signatures. The LVGL library sources were not in the shared archive, so it has **not** been built
  against the project's exact LVGL release.
* The control model was unit-tested on a host (`test_host/`).
* **Not yet done:** an ARM GCC build in e² studio and an on-target run. Please build, and send me the
  first compiler errors if any appear.
