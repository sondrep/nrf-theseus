set(LINKER_SCRIPT ${MDK_DIR}/nrf54l15_xxaa_application.ld)
set(THESEUS_CPU_FLAGS -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3)

target_sources(${EXECUTABLE_NAME} PRIVATE ${MDK_DIR}/gcc_startup_nrf54l15_application.S)
target_compile_definitions(nrfx PUBLIC NRF54L15_XXAA=1)
set(MPSL_SOC "nrf54l" CACHE STRING "MPSL library SoC variant")
set(MPSL_FLOAT_TYPE "hard-float" CACHE STRING "MPSL library float type")
set(SDC_SOC ${MPSL_SOC} CACHE STRING "SoftDevice Controller library SoC variant")
set(SDC_FLOAT_TYPE ${MPSL_FLOAT_TYPE} CACHE STRING "SoftDevice Controller library float type")
