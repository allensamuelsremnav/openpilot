add_executable(print_audio_log "print_audio_log.cpp")

target_link_libraries(print_audio_log PRIVATE ${CAPNP_LIBRARIES})
target_include_directories(print_audio_log PRIVATE "${CMAKE_BINARY_DIR}/logger" ${CAPNP_INCLUDE_DIRS})
