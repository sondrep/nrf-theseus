# SPDX-License-Identifier: Apache-2.0
#
# Theseus -- top-level build for the nRF54L15 (Cortex-M33) FreeRTOS app.
#
# Driven by `west build` (scripts/west_commands),
# which generates NimBLE's syscfg headers and passes their location in via -DNIMBLE_SYSCFG_INCLUDE.
# NOTE: Building with bare cmake will fail the guard below.

cmake_minimum_required(VERSION 3.15)

# ====================================================================================================
# Paths to the west-managed dependency checkouts (siblings of this repo).
# ====================================================================================================
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../FreeRTOS-Kernel")
set(NRFX_PATH            "${CMAKE_CURRENT_SOURCE_DIR}/../nrfx")
set(NRFXLIB_PATH         "${CMAKE_CURRENT_SOURCE_DIR}/../nrfxlib")
set(CMSIS_PATH           "${CMAKE_CURRENT_SOURCE_DIR}/../cmsis")

set(MDK_DIR        ${CMAKE_SOURCE_DIR}/../nrfx/bsp/stable/mdk/)
set(C_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/include)

# picolibc TLS shim (see libc section below).
set(_HAVE_PICOLIBC_TLS_API 1)

set(CMAKE_BUILD_TYPE Debug)

# ====================================================================================================
# Toolchain + project
# Toolchain must be selected before project() triggers compiler detection.
# ====================================================================================================
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/toolchain/arm-none-eabi.cmake"
        CACHE FILEPATH "Toolchain file" FORCE)
endif()

project(app LANGUAGES C ASM)
enable_language(ASM)

set(EXECUTABLE_NAME ${PROJECT_NAME}.elf)

# The MCU startup/system file is the executable's first source.
add_executable(${EXECUTABLE_NAME} ${MDK_DIR}/system_nrf54l.c)
add_library(nrfx EXCLUDE_FROM_ALL)

# ====================================================================================================
# Build target (board)
# Each supported board has a cmake/targets/<board>.cmake that sets THESEUS_CPU_FLAGS, LINKER_SCRIPT, etc.
# ====================================================================================================
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/targets/${THESEUS_BUILD_TARGET}.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/targets/${THESEUS_BUILD_TARGET}.cmake)
else()
    message(FATAL_ERROR
        "Invalid target: ${THESEUS_BUILD_TARGET} is not a valid build target.")
endif()

target_compile_options(nrfx PUBLIC ${THESEUS_CPU_FLAGS}
    -Wall -Wextra -Wno-unused-parameter
    -ffunction-sections -fdata-sections
    -fno-common
)
add_compile_options(${THESEUS_CPU_FLAGS})
add_link_options(-mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3
    -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -lnosys
    -T ${LINKER_SCRIPT}
    -L ${MDK_DIR}/
    --specs=nosys.specs)

target_compile_definitions(nrfx PUBLIC NRF_APPLICATION=1)
target_include_directories(nrfx PUBLIC ${CMSIS_PATH}/CMSIS/Core/Include)

# Application samples (selected via -DSAMPLE_DIRECTORY by `west build`).
add_subdirectory(samples)

# ====================================================================================================
# C standard library (default: picolibc)
# ====================================================================================================
if(NOT DEFINED THESEUS_LIBC)
    set(THESEUS_LIBC picolibc)
endif()

# picolibc stores errno in thread-local storage by default,
# which requires a thread pointer (__aeabi_read_tp) and TLS setup this bare-metal target does not provide.
# Select a single global errno (consistent with picolibc's __SINGLE_THREAD configuration) so malloc/free/sbrk/signal link without TLS.
set(CONFIG_PICOLIBC_GLOBAL_ERRNO ON)

if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/libc/${THESEUS_LIBC}.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/libc/${THESEUS_LIBC}.cmake)
else()
    message(FATAL_ERROR
        "Invalid libc: ${THESEUS_LIBC} is not a valid C standard library.")
endif()

# ====================================================================================================
# FreeRTOS kernel
# ====================================================================================================
# freertos_config carries the FreeRTOSConfig.h include path to the kernel.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/include")

if(DEFINED FREERTOS_SMP_EXAMPLE AND FREERTOS_SMP_EXAMPLE STREQUAL "1")
    message(STATUS "Build FreeRTOS SMP example")
    add_compile_options(-DconfigNUMBER_OF_CORES=2 -DconfigUSE_PASSIVE_IDLE_HOOK=0)
endif()

set(FREERTOS_HEAP "4"                     CACHE STRING "" FORCE)  # heap_4.c
set(FREERTOS_PORT "GCC_ARM_CM33_SECURE"   CACHE STRING "" FORCE)  # CM33 port

add_subdirectory(${FREERTOS_KERNEL_PATH} FreeRTOS-Kernel)

# Application library (Theseus console/log etc.).
add_subdirectory(lib)

# ====================================================================================================
# nrfx (Nordic peripheral drivers)
# Compile every driver under drivers/src, then exclude the ones this target does not use or that don't build for nRF54L.
# ====================================================================================================

target_include_directories(nrfx PUBLIC
    ${NRFX_PATH}/drivers
    ${NRFX_PATH}/hal
    ${NRFX_PATH}/bsp/stable/templates
    ${NRFX_PATH}/bsp/stable
    ${NRFX_PATH}/bsp/stable/mdk
    ${NRFX_PATH}/drivers/include
    ${NRFX_PATH}
    include
)

file(GLOB_RECURSE NRFX_SOURCES "${NRFX_PATH}/drivers/src/*.c")

# Drivers excluded from the build (unused here or unsupported on nRF54L).
set(NRFX_EXCLUDE
    adc bellboard clock comp ipc mramc nvmc power qspi rng
    rtc rtc_legacy spi spis tbm tdm twi uart usbd usbreg vevif
)
foreach(drv IN LISTS NRFX_EXCLUDE)
    list(FILTER NRFX_SOURCES EXCLUDE REGEX "${NRFX_PATH}/drivers/src/nrfx_${drv}\\.c")
endforeach()

target_sources(nrfx PRIVATE
    ${NRFX_SOURCES}
    ${NRFX_PATH}/helpers/nrfx_flag32_allocator.c
)
target_compile_definitions(nrfx PRIVATE NRFX_CLOCK_ENABLED=1)
target_link_libraries(nrfx PUBLIC c)
target_link_libraries(${EXECUTABLE_NAME} PUBLIC nrfx c)



# ====================================================================================================
# Compile options
#
# Strategy: turn warnings into errors broadly, then opt out per library so any
# compilation approach still builds. Only tested with GNU and Clang.
#
# Compiler-name mapping (FreeRTOS -> CMake CMAKE_C_COMPILER_ID):
#   GCC     -> GNU / Clang
#   IAR     -> IAR
#   Keil    -> ARMCC
#   MSVC    -> MSVC
# ====================================================================================================
target_compile_options(freertos_kernel PRIVATE
    # Colorized diagnostics
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

    # Strict warnings (both GCC and Clang)
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Werror>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wconversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>

    # Clang-only suppressions needed for a clean build.
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-unused-macros>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-padded>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-variable-declarations>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-covered-switch-default>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-cast-align>
)

# ====================================================================================================
# Link
# ====================================================================================================
target_link_options(${EXECUTABLE_NAME} PRIVATE
    -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3
    -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -lnosys
    -T ${LINKER_SCRIPT}
    -L ${MDK_DIR}/
    --specs=nosys.specs
)

# NimBLE/mynewt code targets C90 for the kernel.
set_property(TARGET freertos_kernel PROPERTY C_STANDARD 90)
