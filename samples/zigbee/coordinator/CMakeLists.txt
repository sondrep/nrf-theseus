target_sources(${EXECUTABLE_NAME} PRIVATE main.c)
theseus_mark_first_party(${EXECUTABLE_NAME} main.c)

target_link_libraries(${EXECUTABLE_NAME} PUBLIC nrfx log zigbee gpiote)

target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ZB_COORDINATOR_ROLE=1)
target_compile_definitions(zigbee PUBLIC CONFIG_ZIGBEE_ROLE_COORDINATOR=1)
