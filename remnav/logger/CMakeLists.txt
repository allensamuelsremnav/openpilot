# Logger library
capnp_generate_cpp(logSources logHeaders include/log.capnp include/audiolog.capnp)

if (WIN32)
  add_library(msg_logger STATIC src/win_msg_logger.cpp ${logSources})
else()
  add_library(msg_logger STATIC src/lnx_msg_logger.cpp ${logSources})
endif()

add_subdirectory(tests)
