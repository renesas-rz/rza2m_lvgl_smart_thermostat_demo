<#
.SYNOPSIS
    Programs a build of the thermostat demo into the RZ/A2M QSPI flash with SEGGER J-Link Commander.

.DESCRIPTION
    Downloads, in this order:
      1. bootloader/rza2_qspi_flash_ddr_bootloader.elf  (0x20000000 - 0x200006E4, sets up the memory clocks)
      2. build/<Preset>/rza2m_thermostat_demo.hex       (application, XIP from 0x20010000)
    then resets the target and lets it run. Both images are converted/read as Intel hex so that J-Link
    Commander only has to deal with load addresses that are in the QSPI flash window.

.PARAMETER Preset
    CMake preset / build directory name: HardwareDebug, HardwareRelease, QSPIFlashDebug, ...

.PARAMETER NoBootloader
    Skip the bootloader download (only reprogram the application).

.PARAMETER NoRun
    Leave the CPU halted after programming instead of resetting and running.

.PARAMETER DryRun
    Write the J-Link command file and print the J-Link command line, but do not touch the target.

.PARAMETER JLinkDir
    J-Link software folder. Default: $env:JLINK_DIR, otherwise the last "C:\Program Files\SEGGER\JLink*" folder.

.PARAMETER ToolchainBin
    Arm GNU Toolchain bin folder (for arm-none-eabi-objcopy). Default: $env:ARM_TOOLCHAIN_DIR\bin or the
    12.2 rel1 default install.

.EXAMPLE
    powershell -File scripts\jlink_flash.ps1 -Preset HardwareRelease
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('HardwareDebug', 'HardwareRelease', 'HardwareDebug32', 'HardwareRelease32',
                 'HyperRAMDebug', 'HyperRAMRelease', 'HyperRAMClut8Debug', 'HyperRAMClut8Release')]
    [string]$Preset,
    [string]$Device = 'R7S921053VCBG',
    [int]$Speed = 4000,
    [switch]$NoBootloader,
    [switch]$NoRun,
    [switch]$DryRun,
    [string]$JLinkDir,
    [string]$ToolchainBin
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# ---- locate J-Link Commander -----------------------------------------------------------------
if (-not $JLinkDir) { $JLinkDir = $env:JLINK_DIR }
if (-not $JLinkDir) {
    $JLinkDir = Get-ChildItem 'C:\Program Files\SEGGER' -Directory -Filter 'JLink*' -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName 'JLink.exe') } |
        Sort-Object Name | Select-Object -Last 1 -ExpandProperty FullName
}
$jlink = if ($JLinkDir) { Join-Path $JLinkDir 'JLink.exe' } else { $null }
if (-not $jlink -or -not (Test-Path $jlink)) {
    throw "JLink.exe not found. Install the SEGGER J-Link software or set JLINK_DIR / -JLinkDir."
}

# ---- locate objcopy ----------------------------------------------------------------------------
if (-not $ToolchainBin) {
    $tc = if ($env:ARM_TOOLCHAIN_DIR) { $env:ARM_TOOLCHAIN_DIR } `
          else { 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\12.2 rel1' }
    $ToolchainBin = Join-Path $tc 'bin'
}
$objcopy = Join-Path $ToolchainBin 'arm-none-eabi-objcopy.exe'
if (-not (Test-Path $objcopy)) { throw "arm-none-eabi-objcopy.exe not found in '$ToolchainBin'." }

# ---- images --------------------------------------------------------------------------------------
$buildDir = Join-Path $root "build\$Preset"
$appHex   = Join-Path $buildDir 'rza2m_thermostat_demo.hex'
if (-not (Test-Path $appHex)) { throw "$appHex not found. Build the '$Preset' preset first." }

$blElf = Join-Path $root 'bootloader\rza2_qspi_flash_ddr_bootloader.elf'
$blHex = Join-Path $buildDir 'rza2_qspi_flash_ddr_bootloader.hex'
if (-not $NoBootloader) {
    if (-not (Test-Path $blElf)) { throw "$blElf not found (use -NoBootloader to skip it)." }
    & $objcopy -O ihex $blElf $blHex
    if ($LASTEXITCODE -ne 0) { throw 'objcopy of the bootloader failed.' }
}

# ---- J-Link command file -------------------------------------------------------------------------
$cmds = @('r', 'h')            # reset + halt before touching the flash
if (-not $NoBootloader) { $cmds += "loadfile `"$blHex`"" }
$cmds += "loadfile `"$appHex`""
if ($NoRun) { $cmds += 'r' } else { $cmds += 'r'; $cmds += 'g' }
$cmds += 'exit'

$cmdFile = Join-Path $buildDir 'jlink_flash.jlink'
Set-Content -Path $cmdFile -Value $cmds -Encoding ASCII

Write-Host "J-Link   : $jlink"
Write-Host "Device   : $Device (SWD, $Speed kHz)"
Write-Host "Preset   : $Preset"
if (-not $NoBootloader) { Write-Host "Download : $blHex" }
Write-Host "Download : $appHex"
Write-Host ''

$jlinkArgs = @('-NoGui', '1', '-ExitOnError', '1', '-device', $Device, '-if', 'SWD', '-speed', $Speed,
               '-autoconnect', '1', '-CommandFile', $cmdFile)
if ($DryRun) {
    Write-Host "Dry run, not executing: `"$jlink`" $($jlinkArgs -join ' ')"
    Write-Host "Command file ($cmdFile):"
    $cmds | ForEach-Object { Write-Host "    $_" }
    exit 0
}

$out = (& $jlink @jlinkArgs 2>&1) | ForEach-Object { "$_" }
$code = $LASTEXITCODE
$out | ForEach-Object { Write-Host $_ }

# J-Link Commander reports failures as "ERROR: ..." / "Cannot connect to target." (case-sensitive match:
# its banner line "...will now exit on Error" is not a failure)
$bad = $out | Where-Object { $_ -cmatch '^\**ERROR|Cannot connect|Could not connect|Failed to' }
if ($code -ne 0 -or $bad) {
    Write-Host ''
    Write-Host "J-Link programming failed (exit code $code)." -ForegroundColor Red
    exit 1
}
Write-Host ''
Write-Host "Programmed '$Preset'." -ForegroundColor Green
