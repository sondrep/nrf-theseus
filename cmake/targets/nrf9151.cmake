set(LINKER_SCRIPT ${MDK_DIR}/nrf9160_xxaa.ld)
set(THESEUS_CPU_FLAGS -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3)

target_sources(${EXECUTABLE_NAME} PRIVATE ${MDK_DIR}/gcc_startup_nrf9160.S ${MDK_DIR}/system_nrf91.c)
# Vendor startup file: keep it quiet without a target-wide -w
# (which would defeat the per-source first-party warning flags applied to app.elf own sources).
set_source_files_properties(${MDK_DIR}/gcc_startup_nrf9160.S
    TARGET_DIRECTORY ${EXECUTABLE_NAME}
    PROPERTIES COMPILE_OPTIONS "-w")
target_compile_definitions(nrfx PUBLIC NRF9160_XXAA=1)

# Drivers excluded from the build (unused here or unsupported on nRF9151).
set(NRFX_EXCLUDE
    adc bellboard clock comp ipc mramc nvmc power qspi rng
    rtc_legacy spi spis tbm tdm twi uart usbd usbreg vevif nfct qdec lpcomp rramc cracen temp grtc kmu
)
