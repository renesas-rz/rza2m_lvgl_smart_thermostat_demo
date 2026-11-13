# RZ/A2M Smart Thermostat (CMake)

FreeRTOS + LVGL 9 thermostat HMI for the RZ/A2M evaluation board (800x480 RSK panel, FT5216 touch).

Assembled from:
- `rza2m_oven_demo_gcc` -> BSP (`generate/`), FreeRTOS port, LVGL 9 library (`src/lvgl/src`), LVGL display/touch port (`src/renesas`), `main.c`
- `rza2m_thermostat_demo_gcc` -> `src/thermostat/` and the patched `rm_lvgl_port.c`

## Build

Needs CMake >= 3.21, Ninja and the Arm GNU Toolchain 12.2 rel1 (e2 studio ships CMake and Ninja).

    cmake --preset HardwareRelease
    cmake --build --preset HardwareRelease

Outputs in `build/<preset>/`: `rza2m_thermostat_demo.elf`, `.hex`, `.bin`, `.map`. Six presets cover three memory maps:

| Preset (configure + build) | Memory map | Frame buffers | HyperRAM use |
|---|---|---|---|
| `HardwareDebug` / `HardwareRelease` | Hardware | internal RAM (`0x82180000`, uncached mirror) | resources tagged `RZA2M_RESOURCE_SECTION` go to HyperRAM (none today) |
| `QSPIFlashDebug` / `QSPIFlashRelease` | QSPIFlash | internal RAM | **zero**: those resources are linked into QSPI flash and read in place; the HyperRAM region has length 0, so any use is a link error |
| `HyperRAMDebug` / `HyperRAMRelease` | HyperRAM | both VDC frame buffers (1,536,000 bytes) in HyperRAM at `0x40000000` | frame buffers; the MMU maps `0x40000000` L1 write-back cacheable in this map (`-DRZA2M_HYPERRAM_CACHE=OFF` for non-cacheable) |

Debug = `-O0 -g3`, Release = `-O2`. Each preset builds into `build/<preset>/`. The post-build step prints a memory report
(bytes in QSPI / HyperRAM / internal RAM). The map is the cache variable `RZA2M_MEMORY_MAP` (`Hardware|QSPIFlash|HyperRAM`).

Toolchain location: `cmake/arm-none-eabi-gcc.cmake` (default `C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/12.2 rel1`;
override with `-DARM_TOOLCHAIN_DIR=...` or the `ARM_TOOLCHAIN_DIR` environment variable).

## Notes

- The Renesas linker script (`generate/linker_script.ld.in`) picks sections by object name (`*/sections.o`, `*/r_cpg/*.o`, ...).
  `cmake/object_ext_override.cmake` forces the `.o` extension; do not remove it or code lands in the wrong memory.
- Not carried over from the oven project: oven UI and images, LVGL demos/examples/tests, USB HID touch path.
  `rm_lvgl_port.c` always starts the thermostat (the `DEMO_MODE` / `RZA2M_APP_THERMOSTAT` switch is gone).
- `src/thermostat/fonts/Roboto_Medium_100_data.c` is the 100 px digit font copied from the oven UI.
- Flash with the J-Link settings of the original e2 studio project; the QSPI bootloader ELF is in `bootloader/`.
- Host unit test of the control model: `test_host/README.txt`. Original overlay docs: `docs/THERMOSTAT_README.md`.
- **All code runs in place (XIP) from QSPI flash at 0x20010000.** `generate/linker_script.ld.in` was modified from the
  Smart Configurator template: the hardware-setup / CPG / clock-setup, cache driver and interrupt-controller code,
  the IRQ/FIQ handler and the vector mirror table are no longer copied to internal RAM; only their data is.
  `resetprg.c` therefore switches VBAR to the flash copy of the vector table. The original script is kept as
  `generate/linker_script.ram_code.ld.orig`. Check `build/<preset>/*.map`: every executable section must have an address of 0x2001xxxx.
  Regenerating `generate/` with the Smart Configurator would overwrite this change.
- `generate/linker_script.ld.in` is a template: CMake runs it through `gcc -E -P -x c` to produce `build/<preset>/linker_script.ld`
  (`RZA2M_MEMMAP_HARDWARE|QSPIFLASH|HYPERRAM` select the map). Frame-buffer / resource placement is chosen in C with
  `RZA2M_FRAMEBUFFER_SECTION` and `RZA2M_RESOURCE_SECTION` from `src/renesas/application/inc/rza2m_memory_map.h`.
  Anything linked into QSPI flash through `RZA2M_RESOURCE_SECTION` must be `const`.
- **HyperRAM map needs an initialised HyperBus controller.** The BSP only has an empty weak `R_HYPERBUS_Setup()` hook.
  `src/renesas/application/porting/hyperbus_setup.c` implements it (clock select, module clock, HyperBus pin function,
  HYPER.MCR0/1 and MTR0/1), using the sequence recovered from the Guiliani SDK's `rza2m_hyperflash_boot2_loader_gcc.elf`.
  It is active in the HyperRAM map only (`RZA2M_HYPERBUS_INIT`). `hyperram_selftest()` then checks HyperRAM at start-up
  (`RZA2M_HYPERRAM_SELFTEST`) and prints the result on the serial console. Not yet verified on hardware.
  The framebuffers are rendered into directly (LVGL DIRECT mode), so expect lower frame rates than internal RAM.
- Built but **not run on hardware**.

## Flashing and debugging with SEGGER J-Link (VS Code)

Requires the SEGGER J-Link software (device `R7S921053VCBG`, SWD, 4 MHz) and, for debugging, the
Cortex-Debug extension (recommended in `.vscode/extensions.json`).

- **Flash only**: Terminal > Run Task > `Flash <preset> (J-Link)`. It builds the preset, then runs
  `scripts/jlink_flash.ps1`, which programs the bootloader (`bootloader/rza2_qspi_flash_ddr_bootloader.elf`,
  0x20000000) and the application (0x20010000, XIP) through J-Link Commander, then resets and runs the target.
  The script also works from a shell: `powershell -File scripts\jlink_flash.ps1 -Preset HardwareRelease`
  (`-NoBootloader`, `-NoRun`, `-DryRun`, `-JLinkDir`, `-ToolchainBin` are optional).
- **Debug**: Run and Debug > `Debug <preset> (J-Link)`. It builds, halts and resets the target, downloads both
  ELFs to the QSPI flash through the J-Link GDB server, resets, and runs to `main`.
  `Attach (J-Link, no download)` connects to a running target using the symbols of a chosen build.
- The J-Link GDB server and the Arm GDB paths are in `.vscode/settings.json`
  (`cortex-debug.JLinkGDBServerPath.windows`, `cortex-debug.armToolchainPath.windows`); the flash script uses the
  last `C:\Program Files\SEGGER\JLink*` folder unless `JLINK_DIR` is set.

## HyperRAM usage by configuration

HyperRAM region in the linker script: `0x40000000`, 14 MB (`0x00E00000`); the real device size is board specific.

| Preset | VDC frame buffer format | Stride | Per buffer | 2 buffers in HyperRAM | % of the 14 MB region | VDC read (avg / peak) | Extra internal RAM |
|---|---|---|---|---|---|---|---|
| `Hardware*` | RGB565, internal RAM | 1600 B | 768,000 B | 0 | 0% | - | frame buffers 1,536,512 B (uncached mirror) |
| `QSPIFlash*` | RGB565, internal RAM | 1600 B | 768,000 B | 0 | 0% | - | as Hardware |
| `HyperRAM*` | RGB565 | 1664 B (128 B aligned) | 798,720 B | 1,597,440 B (`0x40000000`-`0x40186000`) | 10.9% | 46 / 66 MB/s | render buffer 798,720 B (uncached mirror, cached view) |
| `HyperRAMClut8*` | CLUT8 (8 bpp, 256-colour palette) | 896 B (128 B aligned) | 430,080 B | 860,160 B (`0x40000000`-`0x400D2000`) | 5.9% | 23 / 33 MB/s | render buffer 798,720 B + 64 KB RGB565 lookup table + 1 KB palette |

HyperRAM is only ever written through the uncached alias `0xB0000000` in the HyperRAM presets (changed areas copied from the internal-RAM render
buffer, converted to palette indices in the CLUT8 presets). Start-up self test and bench touch the first 1 KB (non-destructive) / 256 KB (read only).
