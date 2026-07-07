add_executable(secure.elf EXCLUDE_FROM_ALL)
target_sources(secure.elf PRIVATE main_s.c spu.c)
target_include_directories(secure.elf PRIVATE
    ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM33/secure)

target_compile_options(secure.elf PRIVATE ${THESEUS_CPU_FLAGS} -mcmse)
target_link_options(secure.elf PRIVATE
    -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3
    -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -T ${LINKER_SCRIPT_S}
    -L ${MDK_DIR}/
    # Don't auto-link default libraries.
    # Our libc is picolibc (lib/modules/log wires stdout to a UART FILE),
    # Default libraries would bloated the image and clashed with picolibc (duplicate fflush, mismatched errno TLS, ...).
    # -nodefaultlibs drops the automatic newlib libc/libgcc, we re-add just what we need below.
    #-ffreestanding
    -nodefaultlibs
    #--specs=nosys.specs

)

target_sources(secure.elf PRIVATE ${MDK_DIR}/gcc_startup_nrf9120.S ${MDK_DIR}/system_nrf91.c)

# NRFX

target_compile_options(nrfx_secure PRIVATE ${THESEUS_CPU_FLAGS}
    -ffunction-sections -fdata-sections
    -fno-common
)
theseus_mark_third_party(nrfx_secure)

target_compile_definitions(nrfx_secure PUBLIC NRF_APPLICATION=1)

# CMSIS is third-party: SYSTEM so its headers never warn inside our strict code.
target_include_directories(nrfx_secure SYSTEM PUBLIC ${CMSIS_PATH}/CMSIS/Core/Include)

# nrfx headers are third-party -> SYSTEM (keeps nrfy_*.h etc. quiet in our code).
target_include_directories(nrfx_secure SYSTEM PUBLIC
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
target_include_directories(nrfx_secure PUBLIC include)

target_sources(nrfx_secure PRIVATE
    ${NRFX_SOURCES}
    ${NRFX_PATH}/helpers/nrfx_flag32_allocator.c
)
target_compile_definitions(nrfx_secure PRIVATE NRFX_CLOCK_ENABLED=1)

target_sources(nrfx_secure PRIVATE nrfx_glue.c)
theseus_mark_first_party(nrfx_secure nrfx_glue.c)

target_link_libraries(secure.elf PRIVATE nrfx_secure c)
