# Prints fault info (PC, LR, CFSR, etc.) over UART on crash instead of just hanging silently.
option(THESEUS_FAULT_HANDLERS "Install lib/ports/fault.c HardFault/BusFault/etc. handlers" ON)

target_sources(${EXECUTABLE_NAME} PRIVATE ${THESEUS_BUILD_TARGET}.c)
theseus_mark_first_party(${EXECUTABLE_NAME} ${THESEUS_BUILD_TARGET}.c)

if(THESEUS_FAULT_HANDLERS)
    target_sources(${EXECUTABLE_NAME} PRIVATE fault.c)
    theseus_mark_first_party(${EXECUTABLE_NAME} fault.c)
endif()
