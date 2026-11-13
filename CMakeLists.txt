cmake_minimum_required(VERSION 3.21)

# RZ/A2M (Cortex-A9) + FreeRTOS + LVGL 9 : Smart Thermostat demo.
# Build:   cmake --preset HardwareRelease && cmake --build --preset HardwareRelease
# Presets: {Hardware,QSPIFlash,HyperRAM} x {Debug,Release}  (see CMakePresets.json)
# (the toolchain file is set by the presets; see cmake/arm-none-eabi-gcc.cmake)

# Forces the .o object extension (the Generic platform on a Windows host would otherwise use .obj)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/object_ext_override.cmake")

project(rza2m_thermostat_demo LANGUAGES C ASM)

# The Renesas linker script selects input sections by object file name (e.g. "*/sections.o",
# "*/r_cpg/*.o"), so objects must be called <name>.o and not <name>.c.obj.
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE   1)
set(CMAKE_ASM_OUTPUT_EXTENSION_REPLACE 1)

# Optimisation / debug flags are set explicitly below (e2 studio uses -O2 / -O0 -g3, no -DNDEBUG)
foreach(cfg DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
    set(CMAKE_C_FLAGS_${cfg}   "")
    set(CMAKE_ASM_FLAGS_${cfg} "")
endforeach()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(APP_DEMO_MODE 1 CACHE STRING "DEMO_MODE macro (kept for compatibility with the e2 studio project)")

# --------------------------------------------------------------------------------------------------
# Common CPU / code-generation flags (taken from the e2 studio project)
# --------------------------------------------------------------------------------------------------
set(CPU_FLAGS
    -mcpu=cortex-a9 -marm -mlittle-endian -mfloat-abi=hard -mfpu=neon -mno-unaligned-access)

add_compile_options(${CPU_FLAGS} -ffunction-sections -fdata-sections)
add_link_options(${CPU_FLAGS})

# GNU assembler files in this BSP use the .asm extension and the C preprocessor
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp")
set_property(GLOBAL PROPERTY ASM_SOURCE_EXTENSIONS "s;S;asm")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(APP_BUILD_MODE 0)
    add_compile_options(-O0 -g3)
else()
    set(APP_BUILD_MODE 1)
    add_compile_options(-O2)
endif()

# --------------------------------------------------------------------------------------------------
# Memory map (see src/renesas/application/inc/rza2m_memory_map.h and generate/linker_script.ld.in)
#   Hardware  : frame buffers in internal RAM, HyperRAM available for resources (default)
#   QSPIFlash : HyperRAM utilisation is zero, resources are read in place from QSPI flash
#   HyperRAM  : VDC frame buffers in HyperRAM (0x40000000, MMU maps it non-cacheable)
# --------------------------------------------------------------------------------------------------
set(RZA2M_MEMORY_MAP "Hardware" CACHE STRING "Memory map: Hardware | QSPIFlash | HyperRAM")
set_property(CACHE RZA2M_MEMORY_MAP PROPERTY STRINGS Hardware QSPIFlash HyperRAM)
string(TOUPPER "${RZA2M_MEMORY_MAP}" RZA2M_MEMORY_MAP_UPPER)
if(NOT RZA2M_MEMORY_MAP_UPPER MATCHES "^(HARDWARE|QSPIFLASH|HYPERRAM)$")
    message(FATAL_ERROR "RZA2M_MEMORY_MAP must be Hardware, QSPIFlash or HyperRAM (got '${RZA2M_MEMORY_MAP}')")
endif()
set(MEMMAP_DEFINE RZA2M_MEMMAP_${RZA2M_MEMORY_MAP_UPPER})

# HyperRAM map only: map the HyperRAM L1-cacheable (ON, default) or non-cacheable (OFF)
# VDC frame-buffer read burst: 128-byte bursts (line stride padded to 128 bytes) amortise the HyperBus transaction
# overhead; 32-byte bursts are the BSP default. AUTO = 128-byte in the HyperRAM map only.
set(RZA2M_VDC_BURST128 "AUTO" CACHE STRING "VDC frame buffer burst: AUTO | ON | OFF")
set_property(CACHE RZA2M_VDC_BURST128 PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${RZA2M_VDC_BURST128}" VDC_BURST128_UPPER)
if(VDC_BURST128_UPPER STREQUAL "ON" OR (VDC_BURST128_UPPER STREQUAL "AUTO" AND RZA2M_MEMORY_MAP_UPPER STREQUAL "HYPERRAM"))
    set(VDC_BURST128_VALUE 1)
else()
    set(VDC_BURST128_VALUE 0)
endif()

# HyperRAM map only: frame buffers as 8 bit per pixel (VDC CLUT8 palette) - halves the HyperRAM traffic of CPU and VDC
option(RZA2M_FB_CLUT8 "HyperRAM map: 8 bpp CLUT8 frame buffers with a 256-colour palette (see clut8.c)" OFF)
if(RZA2M_FB_CLUT8)
    set(FB_CLUT8_VALUE 1)
else()
    set(FB_CLUT8_VALUE 0)
endif()

option(RZA2M_HYPERRAM_CACHE "HyperRAM map: map HyperRAM L1 write-back cacheable (frame buffer is cleaned on flush)" ON)
if(RZA2M_HYPERRAM_CACHE)
    set(HYPERRAM_CACHE_VALUE 1)
else()
    set(HYPERRAM_CACHE_VALUE 0)
endif()

add_compile_definitions(BUILD_MODE=${APP_BUILD_MODE} DEMO_MODE=${APP_DEMO_MODE}
                        ${MEMMAP_DEFINE} RZA2M_HYPERRAM_CACHE=${HYPERRAM_CACHE_VALUE}
                        RZA2M_VDC_BURST128=${VDC_BURST128_VALUE}
                        RZA2M_FB_CLUT8=${FB_CLUT8_VALUE}
                        RZA2M_MEMORY_MAP_NAME="${RZA2M_MEMORY_MAP}")

set(PROJ ${CMAKE_CURRENT_SOURCE_DIR})

# --------------------------------------------------------------------------------------------------
# Include paths (same set as the e2 studio project, minus the oven UI)
# --------------------------------------------------------------------------------------------------
set(INCLUDE_DIRS
    src/FreeRTOS/include
    src/FreeRTOS/portable/GCC/ARM_CA9_RZA2M
    src/lvgl
    src/lvgl/src
    src
    src/config_files
    src/renesas/application/inc
    src/renesas/application/Image
    generate/compiler/inc
    generate/configuration
    generate/system/inc
    generate/system/iodefine
    generate/os_abstraction/inc
    generate/drivers/r_cache/inc
    generate/drivers/r_cpg/inc
    generate/drivers/r_gpio/inc
    generate/drivers/r_intc/inc
    generate/drivers/r_mmu/inc
    generate/drivers/r_stb/inc
    generate/sc_drivers/r_cbuffer/inc
    generate/sc_drivers/r_ceu/inc
    generate/sc_drivers/r_jcu/inc
    generate/sc_drivers/r_mipi/inc
    generate/sc_drivers/r_ostm/inc
    generate/sc_drivers/r_riic/inc
    generate/sc_drivers/r_rvapi/inc
    generate/sc_drivers/r_scifa/inc
    generate/sc_drivers/r_vdc/inc)
list(TRANSFORM INCLUDE_DIRS PREPEND "${PROJ}/")
include_directories(${INCLUDE_DIRS})

# --------------------------------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------------------------------
function(collect_sources out)
    set(result "")
    foreach(dir IN LISTS ARGN)
        file(GLOB_RECURSE found CONFIGURE_DEPENDS
             "${PROJ}/${dir}/*.c" "${PROJ}/${dir}/*.asm" "${PROJ}/${dir}/*.S")
        list(APPEND result ${found})
    endforeach()
    set(${out} ${result} PARENT_SCOPE)
endfunction()

# Smart Configurator generated BSP (only the modules the e2 studio project builds)
collect_sources(BSP_SRCS
    generate/compiler/asm
    generate/compiler/init
    generate/configuration
    generate/drivers
    generate/os_abstraction/src
    generate/system
    generate/sc_drivers/r_cbuffer
    generate/sc_drivers/r_ceu
    generate/sc_drivers/r_jcu
    generate/sc_drivers/r_mipi
    generate/sc_drivers/r_ostm
    generate/sc_drivers/r_riic
    generate/sc_drivers/r_rvapi
    generate/sc_drivers/r_scifa
    generate/sc_drivers/r_vdc)

# FreeRTOS kernel + RZ/A2M Cortex-A9 port (the heap comes from the OS abstraction layer)
file(GLOB FREERTOS_KERNEL_SRCS "${PROJ}/src/FreeRTOS/*.c")
file(GLOB FREERTOS_PORT_SRCS
     "${PROJ}/src/FreeRTOS/portable/GCC/ARM_CA9_RZA2M/*.c"
     "${PROJ}/src/FreeRTOS/portable/GCC/ARM_CA9_RZA2M/*.S")
set(FREERTOS_SRCS ${FREERTOS_KERNEL_SRCS} ${FREERTOS_PORT_SRCS})

# LVGL library only (no demos / examples / tests)
file(GLOB_RECURSE LVGL_SRCS CONFIGURE_DEPENDS "${PROJ}/src/lvgl/src/*.c")
# Assembly accelerators are disabled in lv_conf_user.h (LV_USE_DRAW_SW_ASM = NONE)

# Application: LVGL port, touch, user_prog, thermostat
collect_sources(APP_SRCS
    src/renesas
    src/user_prog
    src/thermostat)
# USB HID touch/mouse path (LCD_PANEL_DVI) is not built by the e2 studio project either
list(FILTER APP_SRCS EXCLUDE REGEX "/lcd_controller/usb/|/tp_task_usb[.]c$")

# --------------------------------------------------------------------------------------------------
# Object libraries (per-group warning level), linked into one ELF
# --------------------------------------------------------------------------------------------------
add_library(bsp      OBJECT ${BSP_SRCS})
add_library(freertos OBJECT ${FREERTOS_SRCS})
add_library(lvgl     OBJECT ${LVGL_SRCS})
add_library(app      OBJECT ${APP_SRCS})

# Third-party / generated code: keep the log readable
foreach(t bsp freertos lvgl)
    target_compile_options(${t} PRIVATE $<$<COMPILE_LANGUAGE:C>:-w>)
endforeach()
target_compile_options(app PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wshadow -Wno-unused-parameter>)

set(TARGET_NAME rza2m_thermostat_demo)
add_executable(${TARGET_NAME}
    $<TARGET_OBJECTS:bsp>
    $<TARGET_OBJECTS:freertos>
    $<TARGET_OBJECTS:lvgl>
    $<TARGET_OBJECTS:app>)
set_target_properties(${TARGET_NAME} PROPERTIES
    SUFFIX ".elf"
    LINKER_LANGUAGE C)

# The linker script is a template: run it through the C preprocessor to select the memory map
set(LINKER_SCRIPT_IN "${PROJ}/generate/linker_script.ld.in")
set(LINKER_SCRIPT    "${CMAKE_BINARY_DIR}/linker_script.ld")
add_custom_command(
    OUTPUT  "${LINKER_SCRIPT}"
    COMMAND "${CMAKE_C_COMPILER}" -E -P -x c -undef -nostdinc -D${MEMMAP_DEFINE}
            "${LINKER_SCRIPT_IN}" -o "${LINKER_SCRIPT}"
    DEPENDS "${LINKER_SCRIPT_IN}"
    COMMENT "Preprocessing linker script (${RZA2M_MEMORY_MAP} memory map)"
    VERBATIM)
add_custom_target(linker_script DEPENDS "${LINKER_SCRIPT}")
add_dependencies(${TARGET_NAME} linker_script)
set_target_properties(${TARGET_NAME} PROPERTIES LINK_DEPENDS "${LINKER_SCRIPT}")

target_link_options(${TARGET_NAME} PRIVATE
    -T "${LINKER_SCRIPT}"
    -nostartfiles
    -Wl,--gc-sections
    -Wl,-eStart
    -Wl,-Map,${CMAKE_BINARY_DIR}/${TARGET_NAME}.map)
target_link_libraries(${TARGET_NAME} PRIVATE m)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${TARGET_NAME}>
    COMMAND ${CMAKE_COMMAND} -DSIZE_TOOL=${CMAKE_SIZE} -DELF=$<TARGET_FILE:${TARGET_NAME}> -P ${PROJ}/cmake/memory_report.cmake
    COMMENT "Generating hex/bin and printing size")
