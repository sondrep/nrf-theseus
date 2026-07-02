target_sources(spu PUBLIC spu.c)
theseus_mark_first_party(spu spu.c)
target_link_libraries(spu PRIVATE nrfx_secure)
target_link_libraries(spu PRIVATE freertos_kernel)
