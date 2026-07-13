# nrf_modem only supports nRF91 SoCs, so only build it when the sample actually needs it 
# (checked via theseus_module_needed() in CMakeLists.txt)
theseus_module_needed(nrf_modem_lib THESEUS_NEED_NRF_MODEM)

if(THESEUS_NEED_NRF_MODEM)

target_sources(nrf_modem_lib PUBLIC modem.c shmem.c)
theseus_mark_first_party(nrf_modem_lib modem.c shmem.c)

set(CONFIG_NRF_MODEM_LINK_BINARY ON)
set(CONFIG_NRF_MODEM_LINK_BINARY_CELLULAR ON)
set(CONFIG_NRF_MODEM_LOG ON)
target_compile_definitions(nrfx PRIVATE NRFX_RTC_ENABLED=1)
target_compile_definitions(nrfx PRIVATE NRFX_RTC1_ENABLED=1)
add_subdirectory(${NRFXLIB_PATH}/nrf_modem nrf_Modem EXCLUDE_FROM_ALL)

# nrfxlib own CMakeLists.txt has a bug: 
# it sets nrf_modem_imported IMPORTED_LOCATION to the literal string "lib_path" instead of ${lib_path}.
# Recompute the same path here and fix it up
if(CONFIG_NRF_MODEM_LOG)
    set(_modem_lib_file libmodem_log.a)
else()
    set(_modem_lib_file libmodem.a)
endif()
set(_modem_lib_path
    "${NRFXLIB_PATH}/nrf_modem/lib/${CONFIG_NRF_MODEM_VARIANT}/${CONFIG_NRF_MODEM_SOC}/${CONFIG_NRF_MODEM_FLOAT_TYPE}/${_modem_lib_file}")
set_target_properties(nrf_modem_imported PROPERTIES IMPORTED_LOCATION "${_modem_lib_path}")

target_sources(nrf_modem INTERFACE glue.c)
target_link_libraries(nrf_modem_lib PUBLIC nrfx freertos_config freertos_kernel c board nrf_modem log)
target_include_directories(nrf_modem_lib PUBLIC ${NRFXLIB_PATH}/nrf_modem/include)

# Build secure.elf alongside the modem sample (needed for the TrustZone
# secure/non-secure split -- depend on the executable target directly, not
# a flashing target, so `west build` never touches hardware).
add_dependencies(nrf_modem_lib secure.elf)

else()

# Empty stub so the unused library target still has a source to build.
target_sources(nrf_modem_lib PRIVATE stub.c)

endif()
