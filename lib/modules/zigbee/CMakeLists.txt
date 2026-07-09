set(ZBOSS_PATH  "${CMAKE_SOURCE_DIR}/../ncs-zigbee/lib/zboss")
target_sources(zigbee PUBLIC zigbee.c)

set(CONFIG_FPU ON)
set(CONFIG_FP_HARDABI ON)
set(CONFIG_NRF_802154_RADIO_DRIVER ON)
set(CONFIG_SOC nrf54l15_cpuapp)
include(${NRFXLIB_PATH}/common.cmake)
add_subdirectory(${NRFXLIB_PATH}/nrf_802154 zigbee EXCLUDE_FROM_ALL)

add_library(nrf-802154-platform EXCLUDE_FROM_ALL)
target_compile_definitions(nrf-802154-platform PUBLIC CONFIG_MPSL=1)
target_compile_definitions(nrf-802154-driver PUBLIC CONFIG_MPSL=1 NRF_802154_SWI_PRIORITY=5)
target_compile_options(nrf-802154-driver PRIVATE "-DMAX(a,b)=((a)>(b)?(a):(b))" "-DMIN(a,b)=((a)<(b)?(a):(b))")
target_compile_definitions(nrf-802154-driver-interface INTERFACE CONFIG_MPSL=1)
target_compile_definitions(nrf-802154-sl INTERFACE CONFIG_MPSL=1)

target_sources(nrf-802154-platform PUBLIC nrf_802154_clock.c
                              nrf_802154_hp_timer.c
                              nrf_802154_irq.c
                              nrf_802154_platform_sl_lptimer_grtc.c
                              nrf_802154_platform_sl_lptimer_grtc_hw_task.c
                              nrf_802154_platform_timestamper.c
                              nrf_802154_random.c
                              nrf_802154_temperature.c)
target_link_libraries(nrf-802154-platform PUBLIC rng nrfx nrf-802154-sl nrf-802154-driver-interface freertos_kernel)
target_link_libraries(nrf-802154-platform PUBLIC nrfx)
target_link_libraries(zigbee PUBLIC nrf-802154-driver nrf-802154-sl nrf-802154-driver-interface tinycrypt freertos_kernel)
target_compile_definitions(nrf-802154-platform PUBLIC NRF_802154_HIGH_PRECISION_TIMER_INSTANCE_NO=21)

add_library(zboss INTERFACE)
target_link_libraries(zboss INTERFACE ${ZBOSS_PATH}/lib/cortex-m33/hard-float/libzboss.a  ${ZBOSS_PATH}/lib/cortex-m33/hard-float/libgppb.a)
target_link_libraries(zigbee PUBLIC zboss gppi)
target_link_libraries(zboss INTERFACE rng tinycrypt log)
add_subdirectory(osif)

# ====================================================================================================
# ZBoss: Zigbee protocol
# ====================================================================================================
target_include_directories(zigbee SYSTEM PUBLIC ${ZBOSS_PATH}/include)
target_include_directories(zigbee SYSTEM PUBLIC ${ZBOSS_PATH}/include/osif)
target_include_directories(zigbee SYSTEM PUBLIC ${ZBOSS_PATH}/src/include)

target_compile_definitions(zigbee PUBLIC
 LIBZBOSS_CONFIG_FILE=<${ZBOSS_PATH}/include/osif/libzboss_config.h>)
file(GLOB_RECURSE ZBOSS_SOURCES "${ZBOSS_PATH}/src/*.c")
list(FILTER ZBOSS_SOURCES EXCLUDE REGEX "${ZBOSS_PATH}/src/zcl/zb_zcl_s_metering.c")
target_sources(zboss PUBLIC ${ZBOSS_SOURCES})
#file(GLOB_RECURSE ZBOSS_SOURCES "${ZBOSS_PATH}/src/zcl/*.c")
#target_sources(zboss PUBLIC ${ZBOSS_SOURCES})

target_link_libraries(zboss INTERFACE mpsl_lib)
target_link_libraries(nrf-802154-sl INTERFACE mpsl_lib)
