IF (WIN32)
  add_executable(win_speaker_test ${Remnav_SOURCE_DIR}/audio/src/win/win_speaker.cpp
                                  win_speaker_test.cpp)
  target_link_libraries(win_speaker_test Winmm Threads::Threads)
  target_include_directories(win_speaker_test PRIVATE ${Remnav_SOURCE_DIR}/audio/src/win)

  add_executable(win_microphone_test ${Remnav_SOURCE_DIR}/audio/src/win/win_microphone.cpp
                                      win_microphone_test.cpp)
  target_link_libraries(win_microphone_test Winmm Threads::Threads)
  target_include_directories(win_microphone_test PRIVATE ${Remnav_SOURCE_DIR}/audio/src/win)

ELSE()
  add_executable(lnx_microphone_test ${Remnav_SOURCE_DIR}/audio/src/lnx/lnx_base_device.cpp 
                                     ${Remnav_SOURCE_DIR}/audio/src/lnx/lnx_microphone.cpp 
                                     lnx_microphone_test.cpp)
  target_link_libraries(lnx_microphone_test asound Threads::Threads)
  target_include_directories(lnx_microphone_test PRIVATE ${Remnav_SOURCE_DIR}/audio/src/lnx)

  add_executable(lnx_speaker_test ${Remnav_SOURCE_DIR}/audio/src/lnx/lnx_base_device.cpp 
                                     ${Remnav_SOURCE_DIR}/audio/src/lnx/lnx_speaker.cpp 
                                     lnx_speaker_test.cpp)
  target_link_libraries(lnx_speaker_test asound Threads::Threads)
  target_include_directories(lnx_speaker_test PRIVATE ${Remnav_SOURCE_DIR}/audio/src/lnx)
ENDIF()