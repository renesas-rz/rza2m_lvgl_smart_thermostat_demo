# Toolchain file: GNU Arm Embedded 12.2 rel1, bare-metal Cortex-A9 (RZ/A2M)
#
# Override the install location with  -DARM_TOOLCHAIN_DIR=<dir>  or the ARM_TOOLCHAIN_DIR env var.
# <dir> is the folder that contains bin/arm-none-eabi-gcc(.exe).

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT ARM_TOOLCHAIN_DIR)
    if(DEFINED ENV{ARM_TOOLCHAIN_DIR})
        set(ARM_TOOLCHAIN_DIR "$ENV{ARM_TOOLCHAIN_DIR}")
    else()
        set(ARM_TOOLCHAIN_DIR "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/12.2 rel1")
    endif()
endif()

set(_tc_bin "${ARM_TOOLCHAIN_DIR}/bin")
set(_exe "")
if(CMAKE_HOST_WIN32)
    set(_exe ".exe")
endif()

set(CMAKE_C_COMPILER   "${_tc_bin}/arm-none-eabi-gcc${_exe}")
set(CMAKE_CXX_COMPILER "${_tc_bin}/arm-none-eabi-g++${_exe}")
set(CMAKE_ASM_COMPILER "${_tc_bin}/arm-none-eabi-gcc${_exe}")
set(CMAKE_OBJCOPY      "${_tc_bin}/arm-none-eabi-objcopy${_exe}" CACHE FILEPATH "")
set(CMAKE_SIZE         "${_tc_bin}/arm-none-eabi-size${_exe}"    CACHE FILEPATH "")

# No executables can be linked during compiler detection (needs the board linker script)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
