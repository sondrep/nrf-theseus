# ====================================================================================================
# NimBLE (Bluetooth host/controller)
# Sources are globbed
# tests, apps, mesh, audio and the per-driver transport files are excluded, and only the single transport.c shim is kept.
# Generated syscfg headers come in via NIMBLE_SYSCFG_INCLUDE (set by west).
# ====================================================================================================
target_sources(bluetooth PRIVATE hci.c)

set(NIMBLE_PATH          "${CMAKE_SOURCE_DIR}/../mynewt-nimble")
set(MYNEWT_CORE_PATH     "${CMAKE_SOURCE_DIR}/../mynewt-core")

if(NOT DEFINED NIMBLE_SYSCFG_INCLUDE)
    message(FATAL_ERROR
       "NIMBLE_SYSCFG_INCLUDE not set - build with 'west build', not bare cmake.")
endif()

file(GLOB_RECURSE NIMBLE_SOURCES
    "${NIMBLE_PATH}/nimble/**/*.c"
    "${NIMBLE_PATH}/porting/npl/freertos/**/*.c"
    "${NIMBLE_PATH}/porting/nimble/src/os_mbuf.c"
    "${NIMBLE_PATH}/porting/nimble/src/endian.c"
    "${NIMBLE_PATH}/porting/nimble/src/os_mempool.c"
    "${NIMBLE_PATH}/porting/nimble/src/os_msys.c"
    "${NIMBLE_PATH}/porting/nimble/src/mem.c"
    "${NIMBLE_PATH}/porting/nimble/src/nimble_port.c"
)

# Drop tests/drivers/optional subsystems, then add back the one transport shim.
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/nimble/controller/.*/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/nimble/drivers/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/nimble/host/audio/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/nimble/host/mesh/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/nimble/host/test/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/nimble/transport/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/apps/.+\\.c")
list(FILTER NIMBLE_SOURCES EXCLUDE REGEX "${NIMBLE_PATH}/babblesim/.+\\.c")
list(APPEND NIMBLE_SOURCES "${NIMBLE_PATH}/nimble/transport/src/transport.c")

# NimBLE + mynewt-core are third-party:
# mark their includes SYSTEM so the headers stay quiet even when our strict hci.c (and the sample mains) pull them in.
target_include_directories(bluetooth SYSTEM PUBLIC
    ${NIMBLE_SYSCFG_INCLUDE}                                   # generated syscfg.h (from west)
    ${NIMBLE_PATH}/porting/nimble/include
    ${NIMBLE_PATH}/porting/npl/freertos/include
    ${NIMBLE_PATH}/include
    ${NIMBLE_PATH}/nimble/include
    ${NIMBLE_PATH}/nimble/transport/include
    ${NIMBLE_PATH}/nimble/transport/common/hci_h4/include
    ${NIMBLE_PATH}/nimble/controller/include
    ${NIMBLE_PATH}/nimble/drivers/nrf5x/include
    ${NIMBLE_PATH}/nimble/host/include
    ${NIMBLE_PATH}/nimble/host/util/include
    ${NIMBLE_PATH}/nimble/host/store/config/include
    ${NIMBLE_PATH}/nimble/host/store/ram/include
    # GATT services
    ${NIMBLE_PATH}/nimble/host/services/ans/include
    ${NIMBLE_PATH}/nimble/host/services/bas/include
    ${NIMBLE_PATH}/nimble/host/services/bleuart/include
    ${NIMBLE_PATH}/nimble/host/services/dis/include
    ${NIMBLE_PATH}/nimble/host/services/gap/include
    ${NIMBLE_PATH}/nimble/host/services/gatt/include
    ${NIMBLE_PATH}/nimble/host/services/ias/include
    ${NIMBLE_PATH}/nimble/host/services/ipss/include
    ${NIMBLE_PATH}/nimble/host/services/lls/include
    ${NIMBLE_PATH}/nimble/host/services/tps/include
    # mynewt-core console stub (NimBLE logs through it)
    ${MYNEWT_CORE_PATH}/sys/console/stub/include
)

# NimBLE's NPL pulls in the SoC header for IRQ/critical-section primitives.
target_compile_definitions(bluetooth PRIVATE
    NIMBLE_NPL_OS_EXTRA_INCLUDE="nrf.h")

target_sources(bluetooth PRIVATE ${NIMBLE_SOURCES})

# ====================================================================================================
# Nordic binary libraries (SoftDevice Controller + MPSL)
# ====================================================================================================
get_target_property(compile_defs bluetooth COMPILE_DEFINITIONS)

set(THESEUS_SDC_CENTRAL OFF)
set(THESEUS_SDC_PERIPHERAL OFF)
foreach( cd ${compile_defs} )
    if(${cd} STREQUAL "MYNEWT_VAL_BLE_ROLE_CENTRAL=1")
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ROLE_PERIPHERAL=1")
        set(THESEUS_SDC_PERIPHERAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ROLE_BROADCASTER=1")
        set(THESEUS_SDC_PERIPHERAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ROLE_OBSERVER=1")
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_EXT_ADV=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_PERIODIC_ADV=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_PERIODIC_ADV_SYNC_TRANSFER=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_PERIODIC_ADV_SYNC_BIGINFO_REPORTS=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ISO=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ISO_BROADCAST_SOURCE=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ISO_BROADCAST_SINK=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_ISO_TEST=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_CHANNEL_SOUNDING=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_PHY_CODED=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    elseif(${cd} STREQUAL "MYNEWT_VAL_BLE_WHITELIST=1")
        set(THESEUS_SDC_PERIPHERAL ON)
        set(THESEUS_SDC_CENTRAL ON)
    endif()
    if(THESEUS_SDC_CENTRAL AND THESEUS_SDC_PERIPHERAL)
        break()
    endif()
endforeach()

if(THESEUS_SDC_CENTRAL AND THESEUS_SDC_PERIPHERAL)
set(SDC_VARIANT multirole)
elseif(THESEUS_SDC_PERIPHERAL)
set(SDC_VARIANT peripheral)
elseif(THESEUS_SDC_CENTRAL)
set(SDC_VARIANT central)
endif()

add_subdirectory(${NRFXLIB_PATH}/softdevice_controller SoftDevice)
add_subdirectory(${NRFXLIB_PATH}/mpsl MPSL)

target_link_libraries(bluetooth PRIVATE
    tinycrypt
    mpsl
    softdevice_controller
    nrfx_nonsecure freertos_config freertos_kernel c rng log
)

# NimBLE is third-party, so silence the whole target.
# hci.c is our HCI transport shim, so opt it back into strict warnings + UBSan.
theseus_mark_third_party(bluetooth)
theseus_mark_first_party(bluetooth hci.c)
