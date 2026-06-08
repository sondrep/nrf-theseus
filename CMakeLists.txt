cmake_minimum_required(VERSION 3.15)


set(FREERTOS_KERNEL_PATH "../FreeRTOS-Kernel")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Get linker script from nrfx
set(MDK_DIR ${CMAKE_SOURCE_DIR}/../nrfx/bsp/stable/mdk/)

project(hello_world LANGUAGES C ASM)

set(EXECUTABLE_NAME ${PROJECT_NAME}.elf)
set(CMAKE_SYSTEM_PROCESSOR arm)


add_compile_options(-mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3)
#add_link_options(-T ${LINKER_SCRIPT})
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
set(FREERTOS_PORT "GCC_ARM_CM33_NONSECURE" CACHE STRING "" FORCE)

set(C_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/include)


# Default libc is picolibc
if (NOT DEFINED THESEUS_LIBC)
    set(THESEUS_LIBC picolibc)
endif()

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
add_executable(${EXECUTABLE_NAME} ${MDK_DIR}/system_nrf54l.c)

add_compile_options(${THESEUS_CPU_FLAGS}
        -Wall -Wextra -Wno-unused-parameter
        -ffunction-sections -fdata-sections
        -fno-common
    )

include(${CMAKE_SOURCE_DIR}/cmake/targets/${THESEUS_BUILD_TARGET}.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/libc/${THESEUS_LIBC}.cmake)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NRF_APPLICATION=1)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../cmsis/CMSIS/Core/Include)
add_subdirectory(samples)

target_link_options(${EXECUTABLE_NAME} PRIVATE -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3 -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map -lnosys -T ${LINKER_SCRIPT} -L ${MDK_DIR}/ --specs=nosys.specs)
target_link_libraries(${EXECUTABLE_NAME} PUBLIC freertos_kernel freertos_config c)

set_property(TARGET freertos_kernel PROPERTY C_STANDARD 90)
