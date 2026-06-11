cmake_minimum_required(VERSION 3.15)

# Set path variables
set(FREERTOS_KERNEL_PATH "../FreeRTOS-Kernel")
set(MDK_DIR ${CMAKE_SOURCE_DIR}/../nrfx/bsp/stable/mdk/)
set(C_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_BUILD_TYPE Debug)

# Set the toolchain file before project() is called
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/toolchain/arm-none-eabi.cmake"
        CACHE FILEPATH "Toolchain file" FORCE)
endif()

project(hello_world LANGUAGES C ASM)

set(EXECUTABLE_NAME ${PROJECT_NAME}.elf)

enable_language(ASM)
add_executable(${EXECUTABLE_NAME} ${MDK_DIR}/system_nrf54l.c)

# Error check for valid build target. Must add /cmake/targets/TARGET.cmake file for valid target.
if (EXISTS ${CMAKE_SOURCE_DIR}/cmake/targets/${THESEUS_BUILD_TARGET}.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/targets/${THESEUS_BUILD_TARGET}.cmake)
else()
    message(FATAL_ERROR "Invalid target: ${THESEUS_BUILD_TARGET} is not a valid build target.")
endif()

target_compile_options(${EXECUTABLE_NAME} PUBLIC ${THESEUS_CPU_FLAGS}
        -Wall -Wextra -Wno-unused-parameter
        -ffunction-sections -fdata-sections
        -fno-common
    )
add_compile_options(${THESEUS_CPU_FLAGS})

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NRF_APPLICATION=1)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../cmsis/CMSIS/Core/Include)
add_subdirectory(samples)

# Default libc is picolibc
if (NOT DEFINED THESEUS_LIBC)
    set(THESEUS_LIBC picolibc)
endif()

# Error check for valid libc library.
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/libc/${THESEUS_LIBC}.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/libc/${THESEUS_LIBC}.cmake)
else()
    message(FATAL_ERROR "Invalid libc: ${THESEUS_LIBC} is not a valid C standard library.")
endif()

# Add the freertos_config for FreeRTOS-Kernel
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config
    INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/include"
)

# Add library
add_subdirectory(lib)

# Include nrfx
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx/drivers/include)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx/drivers)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx/hal)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx/bsp/stable/templates)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx/bsp/stable)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../nrfx/bsp/stable/mdk)
target_sources(${EXECUTABLE_NAME} PRIVATE ../nrfx/drivers/src/nrfx_gpiote.c ../nrfx/drivers/src/nrfx_grtc.c)
target_sources(${EXECUTABLE_NAME} PRIVATE ../nrfx/helpers/nrfx_flag32_allocator.c)
target_sources(${EXECUTABLE_NAME} PRIVATE ../nrfx/drivers/src/nrfx_grtc.c)

target_include_directories(${EXECUTABLE_NAME} PRIVATE ../cmsis/CMSIS/Core/Include)

if (DEFINED FREERTOS_SMP_EXAMPLE AND FREERTOS_SMP_EXAMPLE STREQUAL "1")
    message(STATUS "Build FreeRTOS SMP example")
    # Adding the following configurations to build SMP template port
    add_compile_options( -DconfigNUMBER_OF_CORES=2 -DconfigUSE_PASSIVE_IDLE_HOOK=0 )
endif()

# Select the heap port.  values between 1-4 will pick a heap.
set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

# Select the native compile PORT
set(FREERTOS_PORT "GCC_ARM_CM33_SECURE" CACHE STRING "" FORCE)

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


target_link_options(${EXECUTABLE_NAME} PRIVATE -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3 -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map -lnosys -T ${LINKER_SCRIPT} -L ${MDK_DIR}/ --specs=nosys.specs)
target_link_libraries(${EXECUTABLE_NAME} PUBLIC freertos_kernel freertos_config c)

set_property(TARGET freertos_kernel PROPERTY C_STANDARD 90)
