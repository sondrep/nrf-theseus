target_sources(secure.elf PRIVATE main_s.c)
target_link_libraries(secure.elf PRIVATE nrfx spu c)
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
    -nodefaultlibs
)

target_sources(secure.elf PRIVATE ${MDK_DIR}/gcc_startup_nrf9120.S ${MDK_DIR}/system_nrf91.c)
