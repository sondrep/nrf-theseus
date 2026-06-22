foreach(module IN LISTS THESEUS_MODULES)
add_subdirectory(${module})
endforeach()
