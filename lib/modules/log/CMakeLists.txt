add_library(log EXCLUDE_FROM_ALL log.c)

target_link_libraries(log PUBLIC nrfx freertos_config freertos_kernel c)
target_link_options(log PRIVATE
    -mthumb -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -g3
    -Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -lnosys
    -T ${LINKER_SCRIPT}
    -L ${MDK_DIR}/
    --specs=nosys.specs
)
