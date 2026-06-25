
add_library(board INTERFACE)

target_include_directories(board INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/${THESEUS_BUILD_TARGET}
)
