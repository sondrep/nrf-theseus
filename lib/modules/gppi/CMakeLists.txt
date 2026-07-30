target_sources(gppi PUBLIC ${THESEUS_BUILD_TARGET}.c)
theseus_mark_first_party(gppi ${THESEUS_BUILD_TARGET}.c)
target_link_libraries(gppi PRIVATE nrfx)
