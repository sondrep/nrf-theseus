target_sources(${EXECUTABLE_NAME} PRIVATE main.c)
theseus_mark_first_party(${EXECUTABLE_NAME} main.c)

target_link_libraries(${EXECUTABLE_NAME} PUBLIC nrfx log zigbee gpiote)
