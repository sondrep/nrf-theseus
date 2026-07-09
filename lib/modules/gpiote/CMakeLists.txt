target_sources(gpiote PRIVATE ${THESEUS_BUILD_TARGET}.c)
theseus_mark_first_party(gpiote ${THESEUS_BUILD_TARGET}.c)

target_link_libraries(gpiote PRIVATE nrfx)
