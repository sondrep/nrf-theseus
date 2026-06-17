set(LINKER_SCRIPT ${MDK_DIR}/nrf54l15_xxaa_application.ld)
set(THESEUS_CPU_FLAGS -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3)

target_sources(${EXECUTABLE_NAME} PRIVATE ${MDK_DIR}/gcc_startup_nrf54l15_application.S)
target_compile_definitions(nrfx PUBLIC NRF54L15_XXAA=1)
