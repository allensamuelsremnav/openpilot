# Logger library
capnp_generate_cpp(logSources logHeaders log.capnp audiolog.capnp)

if (WIN32)
  add_library(msg_logger STATIC win_msg_logger.cpp ${logSources})
else()
  add_library(msg_logger STATIC lnx_msg_logger.cpp ${logSources})
endif()

add_subdirectory(tests)
