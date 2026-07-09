add_subdirectory(${NRFXLIB_PATH}/mpsl MPSL)

target_sources(mpsl_lib PUBLIC mpsl.c)
theseus_mark_first_party(mpsl_lib mpsl.c)

target_link_libraries(mpsl_lib PUBLIC mpsl c board freertos_kernel freertos_config nrfx)
