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

# Tells picolibc's build to compile its ARM TLS helpers (__aeabi_read_tp / _set_tls).
# picolibc's own toolchain files set this, but we use our own, so define it here,
# without it malloc/sbrk would fail to link with "undefined reference to __aeabi_read_tp".
set(_HAVE_PICOLIBC_TLS_API 1)

set(CMAKE_BUILD_TYPE Debug)

set(THESEUS_MODULES bluetooth log rng tinycrypt)

foreach(module IN LISTS THESEUS_MODULES)
add_library(${module} EXCLUDE_FROM_ALL)
endforeach()



# ====================================================================================================
# Toolchain + project
# Toolchain must be selected before project() triggers compiler detection.
# ====================================================================================================
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/toolchain/arm-none-eabi.cmake"
        CACHE FILEPATH "Toolchain file" FORCE)
endif()

# Cache compiler invocations with ccache when it is installed.
# Third-party trees (picolibc, nrfx, NimBLE, FreeRTOS) rarely change,
# so this makes even pristine rebuilds mostly cache hits.
# No-op if ccache isn't on PATH.
# Making build process faster.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_ASM_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(app LANGUAGES C ASM)

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



# ====================================================================================================
# Warning + sanitizer + analyzer policy
#
# Goal: surface every issue in OUR code, stay silent about third-party code.
#
# Third-party (nrfx, NimBLE, FreeRTOS, tinycrypt, picolibc, Nordic MDK)
# is built with -w and pulled in as SYSTEM headers, so its noise never reaches us.
# First-party Theseus code (port/module/hci/glue/log/rng + sample mains) gets:
#
#   * strict warnings       -- unused vars/functions, shadowing, VLAs, fallthrough
#   * GCC static analyzer   -- null-deref / leak / use-after-free / double-free,
#                              at compile time (zero flash), so it's always on
#   * UBSan (trap mode)     -- runtime overflow / shift / bounds / null checks
#
# All on by default for a frictionless `west build && west flash`.
# Warnings stay warnings (no -Werror) so findings show without blocking dev.
# Only UBSan adds code (a few KB): drop it for a minimal release with -DTHESEUS_UBSAN=OFF.
#
# Usage:
#   theseus_mark_first_party(<target> <sources...>)  # strict + analyzer + UBSan
#   theseus_mark_third_party(<target>)               # silence (-w)
# ====================================================================================================
option(THESEUS_UBSAN "UBSan (trap mode) on first-party code" ON)

# Keep compiler diagnostics colored in the terminal, no matter the generator.
set(CMAKE_COLOR_DIAGNOSTICS ON)

set(THESEUS_FIRST_PARTY_C_OPTS
    -Wall
    -Wextra
    -Wshadow
    -Wvla
    -Wimplicit-fallthrough
    -Wno-unused-parameter
    # Static analysis: null-deref / leak / use-after-free / double-free.
    # Compile-time only (zero flash) and GCC-only, so guard it for Clang.
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-fanalyzer>
)
if(THESEUS_UBSAN)
    # 'alignment' is excluded on purpose:
    # the HCI shim does intentional unaligned 16-bit reads that Cortex-M33 handles in hardware,
    # so trapping them would be a false positive that breaks a working BLE link.
    list(APPEND THESEUS_FIRST_PARTY_C_OPTS
        -fsanitize=undefined -fno-sanitize=alignment -fsanitize-trap=undefined)
endif()

# Apply the strict first-party options to specific sources.
# TARGET_DIRECTORY binds the per-source property to the target's directory scope,
# so it works even when the file is added to a target defined in another directory (e.g. app.elf).
function(theseus_mark_first_party tgt)
    set_source_files_properties(${ARGN}
        TARGET_DIRECTORY ${tgt}
        PROPERTIES COMPILE_OPTIONS "${THESEUS_FIRST_PARTY_C_OPTS}")
endfunction()

# Silence a third-party target without affecting the targets that link it.
# Only use this on targets that contain NO first-party sources: a target-wide
# -w wins over a later per-source -Wall (GCC ignores warning flags that follow
# -w), so it would silently disable every first-party warning + the analyzer.
function(theseus_mark_third_party tgt)
    if(TARGET ${tgt})
        target_compile_options(${tgt} PRIVATE -w)
    endif()
endfunction()

# Silence specific third-party SOURCES (per-file -w). Use this for vendor files
# that live in a target which ALSO compiles first-party sources (e.g. app.elf
# mixes the MDK startup/system files with our port/module/main): per-source -w
# keeps the vendor files quiet without clobbering the strict first-party flags.
function(theseus_mark_third_party_sources tgt)
    set_source_files_properties(${ARGN}
        TARGET_DIRECTORY ${tgt}
        PROPERTIES COMPILE_OPTIONS "-w")
endfunction()

# nrfx is third-party, keep only its non-warning code-gen flags here
# (these are PUBLIC because consumers rely on -ffunction-sections/-fdata-sections layout),
# and silence its own warnings.
target_compile_options(nrfx PUBLIC ${THESEUS_CPU_FLAGS}
    -ffunction-sections -fdata-sections
    -fno-common
)
theseus_mark_third_party(nrfx)

# app.elf mixes vendor sources (MDK system_nrf54l.c, startup .S) with first-party sources (port/module/main).
# Silence ONLY the vendor files per-source; the first-party files (in lib/ and samples/) are marked strict
# via theseus_mark_first_party. A target-wide -w here would defeat those per-source -Wall/-fanalyzer flags.
# (The board file silences its own startup .S where that source is added.)
theseus_mark_third_party_sources(${EXECUTABLE_NAME} ${MDK_DIR}/system_nrf54l.c)

add_compile_options(${THESEUS_CPU_FLAGS})

# NOTE: linking is configured once, at the bottom of this file
# (-nodefaultlibs + an explicit picolibc/libgcc/nosys RESCAN group).
# Do not re-introduce a global add_link_options(--specs=nosys.specs ...) here:
# it pulls newlib in next to picolibc and brings back the duplicate-symbol mess.

target_compile_definitions(nrfx PUBLIC NRF_APPLICATION=1)

# CMSIS is third-party: SYSTEM so its headers never warn inside our strict code.
target_include_directories(nrfx SYSTEM PUBLIC ${CMSIS_PATH}/CMSIS/Core/Include)



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

set(FREERTOS_HEAP "4"                     CACHE STRING "" FORCE)  # heap_4.c
set(FREERTOS_PORT "GCC_ARM_CM33_SECURE"   CACHE STRING "" FORCE)  # CM33 port

add_subdirectory(${FREERTOS_KERNEL_PATH} FreeRTOS-Kernel)

# The CM33 secure port ships secure_heap.c,
# which defines its own pvPortMalloc/vPortFree over a small (10 KB) secure-side heap.
# That clashes with heap_4.c's application pvPortMalloc/ucHeap:
# two strong definitions in the kernel,
# and the linker is free to pick either one.
# If it picks the secure heap, our 50 KB heap_4 is bypassed
# -> xTaskCreate runs out of memory and the scheduler never starts.
# We allocate everything from heap_4, so drop secure_heap.c and keep exactly one pvPortMalloc.
get_target_property(_frtos_port_sources freertos_kernel_port SOURCES)
list(FILTER _frtos_port_sources EXCLUDE REGEX "secure_heap\\.c$")
set_property(TARGET freertos_kernel_port PROPERTY SOURCES "${_frtos_port_sources}")

# Application samples (selected via -DSAMPLE_DIRECTORY by `west build`).
add_subdirectory(samples)
# Application library (Theseus console/log etc.).
add_subdirectory(lib)



# ====================================================================================================
# nrfx (Nordic peripheral drivers)
# Compile every driver under drivers/src, then exclude the ones this target does not use or that don't build for nRF54L.
# ====================================================================================================

# nrfx headers are third-party -> SYSTEM (keeps nrfy_*.h etc. quiet in our code).
target_include_directories(nrfx SYSTEM PUBLIC
    ${NRFX_PATH}/drivers
    ${NRFX_PATH}/hal
    ${NRFX_PATH}/bsp/stable/templates
    ${NRFX_PATH}/bsp/stable
    ${NRFX_PATH}/bsp/stable/mdk
    ${NRFX_PATH}/drivers/include
    ${NRFX_PATH}
)

# The repo's own include/ is first-party (nrfx_glue.h, nrfx_log.h, configs):
# keep it as a normal include so warnings in our headers are still reported.
target_include_directories(nrfx PUBLIC include)

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
# Third-party warning silencing
#
# The FreeRTOS kernel is vendor code,
# its CM33 secure port emits cmse-attribute noise we cannot fix from here.
# Silence both kernel targets.
# (nrfx, NimBLE and tinycrypt are silenced next to where they are defined; first-party files inside those libraries are re-marked strict via theseus_mark_first_party.)
# ====================================================================================================
theseus_mark_third_party(freertos_kernel)
theseus_mark_third_party(freertos_kernel_port)



# ====================================================================================================
# Link
# ====================================================================================================
# The $<LINK_GROUP:RESCAN,...> generator expression below is undefined on the 'Generic' (bare-metal) system,
# so teach CMake what it means here: it maps to GNU ld's --start-group / --end-group.
set(CMAKE_C_LINK_GROUP_USING_RESCAN_SUPPORTED TRUE)
set(CMAKE_C_LINK_GROUP_USING_RESCAN "LINKER:--start-group" "LINKER:--end-group")

target_link_options(${EXECUTABLE_NAME} PRIVATE
    -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3
    -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -T ${LINKER_SCRIPT}
    -L ${MDK_DIR}/
    # Don't auto-link default libraries.
    # Our libc is picolibc (lib/modules/log wires stdout to a UART FILE),
    # Default libraries would bloated the image and clashed with picolibc (duplicate fflush, mismatched errno TLS, ...).
    # -nodefaultlibs drops the automatic newlib libc/libgcc, we re-add just what we need below.
    -nodefaultlibs
)

# Re-add the runtime libs we actually need,
# as a rescannable group so cross-references resolve no matter the link order.
# 'c' is picolibc (also pulled in via nrfx), -lgcc and -lnosys close the group.
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    "$<LINK_GROUP:RESCAN,nrfx,c,freertos_kernel,-lgcc,-lnosys>"
)

# NimBLE/mynewt code targets C90 for the kernel.
set_property(TARGET freertos_kernel PROPERTY C_STANDARD 90)
