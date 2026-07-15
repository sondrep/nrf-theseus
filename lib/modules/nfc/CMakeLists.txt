#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# NFC headers from nrfxlib. PUBLIC so main.c can use them too.
target_include_directories(nfc SYSTEM PUBLIC ${NRFXLIB_PATH}/nfc/include)

# Module sources.
target_sources(nfc PRIVATE
    # Platform glue wiring the nrfxlib NFC lib to our HAL/OS (IRQs, timing).
    platform.c
    # Builds a raw NDEF message from its records.
    ndef/msg.c
    # Builds a single NDEF record (header + payload).
    ndef/record.c
    # Builds an NDEF "Text" record (language code + UTF-8 text).
    ndef/text_rec.c
    # Wraps an NDEF message into a Type 4 Tag file (prepends the 2-byte NLEN length field).
    t4t/ndef_file.c
)

# For our own code, we use strict warnings.
# For the copied Nordic files, we use silenced warnings.
theseus_mark_first_party(nfc platform.c)
theseus_mark_third_party_sources(nfc
    ndef/msg.c
    ndef/record.c
    ndef/text_rec.c
    t4t/ndef_file.c
)

# Enable the nrfx drivers platform.c compiles against.
target_compile_definitions(nfc PRIVATE
    NRFX_CLOCK_ENABLED=1
    NRFX_NFCT_ENABLED=1
    CONFIG_NFC_T2T_NRFXLIB=1
    CONFIG_NFC_T4T_NRFXLIB=1
)

# ---------------------------------------------------------------------------
# Nordic's precompiled NFC libraries (nrfxlib).
#
# These are closed-source, prebuilt static archives (.a)
# there is no source to compile, so we IMPORT them.
# (point a target at the existing file on disk)
#
# nrfxlib ships separate hard-float and soft-float builds.
# Select the one that matches the target's ABI via THESEUS_FPU
# (set in cmake/targets/<board>.cmake)
# The hard-float variant must be paired with -mfloat-abi=hard,
# NOTE: mixing ABIs fails to link!
#   libnfc_t2t.a -> Type 2 Tag emulation (read-only NDEF tag)
#   libnfc_t4t.a -> Type 4 Tag emulation (read/write NDEF tag)
# A static lib only pulls in referenced members,
# so the unused tag type costs nothing until a sample calls its API.
# ---------------------------------------------------------------------------
if(THESEUS_FPU)
    set(NFC_FLOAT_VARIANT hard-float)
else()
    set(NFC_FLOAT_VARIANT soft-float)
endif()

set(NFC_LIB_DIR ${NRFXLIB_PATH}/nfc/lib/cortex-m33/${NFC_FLOAT_VARIANT})

if(NOT EXISTS ${NFC_LIB_DIR}/libnfc_t2t.a)
    message(FATAL_ERROR "NFC prebuilt libs not found in ${NFC_LIB_DIR} "
                        "(check THESEUS_FPU / architecture).")
endif()

add_library(nfc_t2t STATIC IMPORTED)
set_target_properties(nfc_t2t PROPERTIES IMPORTED_LOCATION ${NFC_LIB_DIR}/libnfc_t2t.a)

add_library(nfc_t4t STATIC IMPORTED)
set_target_properties(nfc_t4t PROPERTIES IMPORTED_LOCATION ${NFC_LIB_DIR}/libnfc_t4t.a)

target_link_libraries(nfc PUBLIC nrfx c log nfc_t2t nfc_t4t)

# The prebuilt NFC libs call back into our platform.c (nfc_platform_*),
# so nfc and nfc_t2t/nfc_t4t are mutually dependent.
# Declaring the reverse edge lets CMake repeat the archives on the link line and resolve the back-references.
set_property(TARGET nfc_t2t PROPERTY INTERFACE_LINK_LIBRARIES nfc)
set_property(TARGET nfc_t4t PROPERTY INTERFACE_LINK_LIBRARIES nfc)
