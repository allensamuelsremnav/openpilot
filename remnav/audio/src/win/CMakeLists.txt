
add_executable(win_udp_microphone_client win_udp_microphone_client.cpp
                                         ../audio_logger.cpp
                                         win_udp_client.cpp
                                         win_microphone.cpp)


target_link_libraries(win_udp_microphone_client Winmm Ws2_32 Threads::Threads msg_logger CapnProto::capnp)
target_compile_options(win_udp_microphone_client PRIVATE "/W4")
target_include_directories(win_udp_microphone_client PRIVATE "${CMAKE_BINARY_DIR}/logger/include")

add_executable(win_udp_speaker_client win_udp_speaker_client.cpp 
                                           win_udp_server.cpp
                                           win_speaker.cpp)
target_link_libraries(win_udp_speaker_client Winmm Ws2_32 Threads::Threads)
target_compile_options(win_udp_speaker_client PRIVATE "/W4")

