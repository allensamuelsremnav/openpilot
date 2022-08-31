add_executable(lnx_udp_microphone_client lnx_udp_microphone_client.cpp 
                                         ../audio_logger.cpp
                                         lnx_udp_client.cpp
                                         lnx_microphone.cpp
                                         lnx_base_device.cpp)

target_link_libraries(lnx_udp_microphone_client asound Threads::Threads msg_logger ${CAPNP_LIBRARIES})
target_include_directories(lnx_udp_microphone_client PRIVATE "${CMAKE_BINARY_DIR}/logger/include" ${CAPNP_INCLUDE_DIRS})
target_include_directories(lnx_udp_microphone_client PRIVATE ${Remnav_SOURCE_DIR}/audio/src)

include_directories(lnx_udp_speaker_client ../src)
add_executable(lnx_udp_speaker_client lnx_udp_speaker_client.cpp 
                                           lnx_udp_server.cpp
                                           lnx_speaker.cpp
                                           lnx_base_device.cpp)

target_include_directories(lnx_udp_speaker_client PRIVATE ${Remnav_SOURCE_DIR}/audio/src)
target_link_libraries(lnx_udp_speaker_client asound Threads::Threads)
