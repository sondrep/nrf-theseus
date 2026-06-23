# ====================================================================================================
# TinyCrypt (crypto primitives used by the BLE stack)
# ====================================================================================================
set(TINYCRYPT_PATH       "${CMAKE_SOURCE_DIR}/../tinycrypt")
target_sources(tinycrypt PRIVATE
    tinycrypt.c
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
# SYSTEM because TinyCrypt headers are third-party, so they stay quiet in our code.
target_include_directories(tinycrypt SYSTEM PUBLIC ${TINYCRYPT_PATH}/lib/include)
target_link_libraries(tinycrypt PRIVATE log rng)

# Upstream TinyCrypt is third-party, so silence the whole target.
# tinycrypt.c is our shim, so opt it back into strict warnings + UBSan.
theseus_mark_third_party(tinycrypt)
theseus_mark_first_party(tinycrypt tinycrypt.c)
