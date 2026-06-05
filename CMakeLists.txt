cmake_minimum_required(VERSION 3.15)
project(hello_world)

set(FREERTOS_KERNEL_PATH "../FreeRTOS-Kernel")

set(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_C_COMPILER arm-none-eabi-gcc)
SET(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
SET(CMAKE_AR arm-none-eabi-ar)
SET(CMAKE_OBJCOPY arm-none-eabi-objcopy)
SET(CMAKE_OBJDUMP arm-none-eabi-objdump)
SET(SIZE arm-none-eabi-size)
SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

SET(CMAKE_COMPILE_FILES_DIR ${CMAKE_SOURCE_DIR}/scripts)
SET(COMMON_LINKER_SCRIPT ${CMAKE_COMPILE_FILES_DIR}/nrf_common.ld)
SET(LINKER_SCRIPT ${CMAKE_COMPILE_FILES_DIR}/nrf52840_xxaa.ld)

add_link_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-T ${LINKER_SCRIPT})
#add_compile_options(-march=armv6)

# Add the freertos_config for FreeRTOS-Kernel
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config
    INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/include"
)

if (DEFINED FREERTOS_SMP_EXAMPLE AND FREERTOS_SMP_EXAMPLE STREQUAL "1")
    message(STATUS "Build FreeRTOS SMP example")
    # Adding the following configurations to build SMP template port
    add_compile_options( -DconfigNUMBER_OF_CORES=2 -DconfigUSE_PASSIVE_IDLE_HOOK=0 )
endif()

# Select the heap port.  values between 1-4 will pick a heap.
set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

# Select the native compile PORT
set(FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "" FORCE)

set(C_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/include)

# Adding the FreeRTOS-Kernel subdirectory
add_subdirectory(${FREERTOS_KERNEL_PATH} FreeRTOS-Kernel)

########################################################################
# Overall Compile Options
# Note the compile option strategy is to error on everything and then
# Per library opt-out of things that are warnings/errors.
# This ensures that no matter what strategy for compilation you take, the
# builds will still occur.
#
# Only tested with GNU and Clang.
# Other options are https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html#variable:CMAKE_%3CLANG%3E_COMPILER_ID
# Naming of compilers translation map:
#
#   FreeRTOS    | CMake
#   -------------------
#   CCS         | ?TBD?
#   GCC         | GNU, Clang, *Clang Others?
#   IAR         | IAR
#   Keil        | ARMCC
#   MSVC        | MSVC # Note only for MinGW?
#   Renesas     | ?TBD?

target_compile_options(freertos_kernel PRIVATE
    ### Gnu/Clang C Options
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Werror>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wconversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>

    # Suppressions required to build clean with clang.
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-unused-macros>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-padded>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-variable-declarations>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-covered-switch-default>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-cast-align> )

enable_language(ASM)
add_executable(${PROJECT_NAME}.elf ../nrfx/bsp/stable/mdk/gcc_startup_nrf52840.S ${COMMON_LINKER_SCRIPT} ${LINKER_SCRIPT})
add_subdirectory(samples)

target_link_libraries(${PROJECT_NAME}.elf freertos_kernel freertos_config)

set_property(TARGET freertos_kernel PROPERTY C_STANDARD 90)
