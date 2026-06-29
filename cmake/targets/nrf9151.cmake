set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/scripts/linker/nrf9151.ld)
set(LINKER_SCRIPT_S ${CMAKE_CURRENT_SOURCE_DIR}/scripts/linker/secure_app/nrf9151_s.ld)
set(LINKER_SCRIPT_NS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/linker/secure_app/nrf9151_ns.ld)
#set(LINKER_SCRIPT ${MDK_DIR}/nrf9160_xxaa.ld)
#set(LINKER_SCRIPT ${MDK_DIR}/nrf9120_xxaa.ld)
set(THESEUS_CPU_FLAGS -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3)



#target_sources(${EXECUTABLE_NAME} PRIVATE ${MDK_DIR}/gcc_startup_nrf9160.S ${MDK_DIR}/system_nrf91.c)
# Vendor startup file: keep it quiet without a target-wide -w
# (which would defeat the per-source first-party warning flags applied to app.elf own sources).
set_source_files_properties(${MDK_DIR}/gcc_startup_nrf9120.S
#set_source_files_properties(${MDK_DIR}/gcc_startup_nrf9160.S
    TARGET_DIRECTORY ${EXECUTABLE_NAME}
    PROPERTIES COMPILE_OPTIONS "-w")


target_compile_definitions(nrfx PUBLIC NRF9120_XXAA=1)
target_sources(${EXECUTABLE_NAME} PUBLIC ${MDK_DIR}/gcc_startup_nrf9120.S ${MDK_DIR}/system_nrf91.c)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE NRF_TRUSTZONE_NONSECURE=1)
#target_compile_definitions(nrfx PUBLIC NRF9160_XXAA=1)

# Drivers excluded from the build (unused here or unsupported on nRF9151).
set(NRFX_EXCLUDE
    adc bellboard clock comp mramc nvmc power qspi rng
    rtc_legacy spi spis tbm tdm twi uart usbd usbreg vevif nfct qdec lpcomp rramc cracen temp grtc kmu
)


set(CONFIG_NRF_MODEM_SOC nrf9120)
#set(CONFIG_NRF_MODEM_SOC nrf9160)
set(CONFIG_NRF_MODEM_FLOAT_TYPE hard-float)
set(CONFIG_NRF_MODEM_VARIANT cellular)
