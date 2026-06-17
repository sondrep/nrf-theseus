add_library(bluetooth EXCLUDE_FROM_ALL)

target_sources(bluetooth PRIVATE hci.c)

# ====================================================================================================
# NimBLE (Bluetooth host/controller)
# Sources are globbed
# tests, apps, mesh, audio and the per-driver transport files are excluded, and only the single transport.c shim is kept.
# Generated syscfg headers come in via NIMBLE_SYSCFG_INCLUDE (set by west).
# ====================================================================================================
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

target_include_directories(bluetooth PUBLIC
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
# TinyCrypt (crypto primitives used by the BLE stack)
# ====================================================================================================
add_library(tinycrypt
    ${TINYCRYPT_PATH}/lib/source/aes_decrypt.c
    ${TINYCRYPT_PATH}/lib/source/aes_encrypt.c
    ${TINYCRYPT_PATH}/lib/source/cbc_mode.c
    ${TINYCRYPT_PATH}/lib/source/ccm_mode.c
    ${TINYCRYPT_PATH}/lib/source/cmac_mode.c
    ${TINYCRYPT_PATH}/lib/source/ctr_mode.c
    ${TINYCRYPT_PATH}/lib/source/ctr_prng.c
    ${TINYCRYPT_PATH}/lib/source/ecc_dh.c
    ${TINYCRYPT_PATH}/lib/source/ecc_dsa.c
    ${TINYCRYPT_PATH}/lib/source/ecc.c
    ${TINYCRYPT_PATH}/lib/source/hmac_prng.c
    ${TINYCRYPT_PATH}/lib/source/hmac.c
    ${TINYCRYPT_PATH}/lib/source/sha256.c
    ${TINYCRYPT_PATH}/lib/source/utils.c
)
# PUBLIC so the main executable inherits the include path when it links.
target_include_directories(tinycrypt PUBLIC ${TINYCRYPT_PATH}/lib/include)

# ====================================================================================================
# Nordic binary libraries (SoftDevice Controller + MPSL)
# ====================================================================================================
target_include_directories(bluetooth PRIVATE
    ${NRFXLIB_PATH}/softdevice_controller/include
    ${NRFXLIB_PATH}/mpsl/include
)

target_link_libraries(bluetooth PRIVATE
    tinycrypt
    ${NRFXLIB_PATH}/softdevice_controller/lib/nrf54l/hard-float/libsoftdevice_controller_multirole.a
    ${NRFXLIB_PATH}/mpsl/lib/nrf54l/hard-float/libmpsl.a
    ${NRFXLIB_PATH}/mpsl/fem/common/lib/nrf54l/hard-float/libmpsl_fem_common.a
)
