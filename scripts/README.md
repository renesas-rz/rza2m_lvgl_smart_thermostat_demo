# RZ/A2M J-Link Flash Utility (`jlink_flash.ps1`)

This PowerShell script automates programming the **RZ/A2M Thermostat Demo** firmware (and its QSPI DDR bootloader) into external QSPI flash memory using **SEGGER J-Link Commander**.

It handles converting the bootloader `.elf` into an Intel `.hex` image, generating the J-Link command script dynamically, and executing the flash sequence over SWD.

---

## Prerequisites & Tool Locations

Before running the script, ensure the following tools are installed on your Windows machine. The script uses automatic discovery, environment variables, or CLI parameters to locate them:

### 1. SEGGER J-Link Software (`JLink.exe`)
Download Page: [SEGGER J-Link Downloads](https://www.segger.com/downloads/jlink/)
The script searches for `JLink.exe` in the following order:
1. **`-JLinkDir` parameter** (if provided via command line).
2. **`$env:JLINK_DIR` environment variable**.
3. **Default Install Path:** The highest-versioned folder matching `C:\Program Files\SEGGER\JLink*` (e.g., `C:\Program Files\SEGGER\JLink_V794e\JLink.exe`).

### 2. Arm GNU Toolchain (`arm-none-eabi-objcopy.exe`)
Download Page: [Arm GNU Toolchain Downloads v12.2 rel1](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm/-/tree/releases/12.2.rel1?ref_type=heads) arm-gnu-toolchain-12.2.rel1-mingw-w64-i686-arm-none-eabi.exe
Required to convert the bootloader `.elf` file into an Intel `.hex` file so J-Link only writes to the QSPI flash address window (`0x20000000`). The script searches in this order:
1. **`-ToolchainBin` parameter** (path to the `bin` folder).
2. **`$env:ARM_TOOLCHAIN_DIR\bin`** (via environment variable).
3. **Default Install Path:** `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\12.2 rel1\bin\arm-none-eabi-objcopy.exe`.

---

## Expected Folder Layout

The script determines the project root dynamically using `$PSScriptRoot` (assuming the script lives inside a `scripts/` subfolder). Your workspace should match the following directory structure:

```text
<Project_Root>/
├── bootloader/
│   └── rza2_qspi_flash_ddr_bootloader.elf   # [Input] Pre-built bootloader ELF (0x20000000)
│
├── build/
│   └── <Preset>/                           # e.g., HardwareDebug, HardwareRelease, QSPIFlashDebug...
│       ├── rza2m_thermostat_demo.hex       # [Input] Compiled application HEX (0x20010000)
│       ├── rza2_qspi_flash_ddr_bootloader.hex # [Generated] Converted bootloader HEX
│       └── jlink_flash.jlink               # [Generated] Auto-generated J-Link command script
│
└── scripts/
    └── jlink_flash.ps1                     # This flashing script
```

### File Breakdown
* **Bootloader ELF (`bootloader/rza2_qspi_flash_ddr_bootloader.elf`):** Must exist prior to running the script unless the `-NoBootloader` flag is passed.
* **Application HEX (`build/<Preset>/rza2m_thermostat_demo.hex`):** Must be built beforehand using your CMake preset.
* **Generated Artifacts (`build/<Preset>/`):** Both the converted bootloader `.hex` and the temporary `jlink_flash.jlink` command script are automatically placed inside the active build preset folder during execution.

---

## Usage

Open a Windows Command terminal in the `<Project_Root>` directory and run the script, specifying the target `-Preset`. NOTE: Due to PC permissions do not attempt to run script in powershell. 

### Basic Examples

**Flash both the bootloader and application (Release build):**
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\jlink_flash.ps1 -Preset HardwareRelease
```

**Flash only the application (skip bootloader):**
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\jlink_flash.ps1 -Preset HardwareRelease -NoBootloader
```

**Preview commands without connecting to hardware (Dry Run):**
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\jlink_flash.ps1 -Preset HardwareRelease -DryRun
```

**Specify custom tool paths:**
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\jlink_flash.ps1 `
    -Preset HardwareRelease `
    -JLinkDir "D:\Tools\SEGGER\JLink" `
    -ToolchainBin "D:\Tools\ArmGNU\13.2\bin"
```

---

## Parameters Reference

| Parameter | Type | Required | Default | Description |
| :--- | :--- | :---: | :--- | :--- |
| **`-Preset`** | `String` | **Yes** | — | CMake preset / build directory name. Valid options: `HardwareDebug`, `HardwareRelease`, `QSPIFlashDebug`, `QSPIFlashRelease`, `HyperRAMDebug`, `HyperRAMRelease`, `HyperRAMClut8Debug`, `HyperRAMClut8Release`. |
| **`-Device`** | `String` | No | `R7S921053VCBG` | Target Renesas RZ/A2M device identifier for J-Link. |
| **`-Speed`** | `Int` | No | `4000` | SWD interface clock speed in kHz. |
| **`-NoBootloader`** | `Switch` | No | `$false` | Skips converting and flashing `rza2_qspi_flash_ddr_bootloader.elf`. |
| **`-NoRun`** | `Switch` | No | `$false` | Resets the target after flashing, leaving the CPU halted instead of executing (`g`). |
| **`-DryRun`** | `Switch` | No | `$false` | Generates `jlink_flash.jlink` and prints the CLI command without invoking `JLink.exe`. |
| **`-JLinkDir`** | `String` | No | Auto-detected | Directory containing `JLink.exe`. Overrides `$env:JLINK_DIR`. |
| **`-ToolchainBin`** | `String` | No | Auto-detected | Directory containing `arm-none-eabi-objcopy.exe`. Overrides `$env:ARM_TOOLCHAIN_DIR`. |

---

## How It Works Under the Hood

1. **Validation:** Verifies that `JLink.exe`, `arm-none-eabi-objcopy.exe`, and the target `.hex`/`.elf` files exist.
2. **Conversion:** Runs `arm-none-eabi-objcopy -O ihex` on the bootloader `.elf` to strip non-flash sections and output an Intel Hex file.
3. **Command Generation:** Writes `build/<Preset>/jlink_flash.jlink` with the following sequence:
   ```text
   r
   h
   loadfile "<root>\build\<Preset>\rza2_qspi_flash_ddr_bootloader.hex"
   loadfile "<root>\build\<Preset>\rza2m_thermostat_demo.hex"
   r
   g
   exit
   ```
4. **Execution & Error Checking:** Invokes `JLink.exe` in headless mode (`-NoGui 1 -ExitOnError 1`) over SWD. Because J-Link Commander sometimes returns exit code `0` even on connection errors, the script parses `stdout` for failure strings (`ERROR`, `Cannot connect`, `Failed to`) to ensure reliable CI/CD and terminal error reporting.