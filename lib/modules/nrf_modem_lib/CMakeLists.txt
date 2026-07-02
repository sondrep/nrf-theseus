target_sources(nrf_modem_lib PUBLIC modem.c)
theseus_mark_first_party(nrf_modem_lib modem.c)

set(CONFIG_NRF_MODEM_LINK_BINARY ON)
set(CONFIG_NRF_MODEM_LINK_BINARY_CELLULAR ON)
set(CONFIG_NRF_MODEM_LOG ON)
target_compile_definitions(nrfx_nonsecure PRIVATE NRFX_RTC_ENABLED=1)
target_compile_definitions(nrfx_nonsecure PRIVATE NRFX_RTC1_ENABLED=1)
add_subdirectory(${NRFXLIB_PATH}/nrf_modem nrf_Modem)

target_sources(nrf_modem INTERFACE glue.c)
target_link_libraries(nrf_modem_lib PUBLIC nrfx_nonsecure freertos_config freertos_kernel c board nrf_modem log)
target_include_directories(nrf_modem_lib PUBLIC {NRFXLIB_PATH}/nrf_modem/include)
