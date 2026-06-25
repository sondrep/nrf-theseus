set(LINKER_SCRIPT ${MDK_DIR}/nrf54l15_xxaa_application.ld)
set(THESEUS_CPU_FLAGS -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3)

target_sources(${EXECUTABLE_NAME} PRIVATE ${MDK_DIR}/gcc_startup_nrf54l15_application.S ${MDK_DIR}/system_nrf54l.c)
# app.elf mixes vendor sources (MDK system_nrf54l.c, startup .S) with first-party sources (port/module/main).
# Silence ONLY the vendor files per-source; the first-party files (in lib/ and samples/) are marked strict
# via theseus_mark_first_party. A target-wide -w here would defeat those per-source -Wall/-fanalyzer flags.
# (The board file silences its own startup .S where that source is added.)
#theseus_mark_third_party_sources(${EXECUTABLE_NAME} ${MDK_DIR}/system_nrf54l.c)
# Vendor startup file: keep it quiet without a target-wide -w
# (which would defeat the per-source first-party warning flags applied to app.elf own sources).
set_source_files_properties(${MDK_DIR}/gcc_startup_nrf54l15_application.S
    TARGET_DIRECTORY ${EXECUTABLE_NAME}
    PROPERTIES COMPILE_OPTIONS "-w")
target_compile_definitions(nrfx PUBLIC NRF54L15_XXAA=1)
set(MPSL_SOC "nrf54l" CACHE STRING "MPSL library SoC variant")
set(MPSL_FLOAT_TYPE "hard-float" CACHE STRING "MPSL library float type")
set(SDC_SOC ${MPSL_SOC} CACHE STRING "SoftDevice Controller library SoC variant")
set(SDC_FLOAT_TYPE ${MPSL_FLOAT_TYPE} CACHE STRING "SoftDevice Controller library float type")

# Drivers excluded from the build (unused here or unsupported on nRF54L).
set(NRFX_EXCLUDE
    adc bellboard clock comp ipc mramc nvmc power qspi rng
    rtc rtc_legacy spi spis tbm tdm twi uart usbd usbreg vevif
)
