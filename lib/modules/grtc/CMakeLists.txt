target_sources(grtc PRIVATE grtc.c)
theseus_mark_first_party(grtc grtc.c)
target_link_libraries(grtc PRIVATE nrfx freertos_kernel)
target_link_libraries(freertos_kernel PRIVATE grtc)
