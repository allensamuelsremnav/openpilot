cmake_minimum_required(VERSION 3.5)
set (CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/CompilerOptions.cmake")

project(Remnav)
set(CMAKE_CXX_STANDARD 14)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (MSVC)
  set(MY_CAPNPROTO_INSTALL "C:/Users/snjvs/sandbox/capnproto-c++-win32-0.10.2/capnproto-c++-0.10.2/out/install/x64-Debug")
  list(APPEND CMAKE_PREFIX_PATH ${MY_CAPNPROTO_INSTALL})
  include_directories(${MY_CAPNPROTO_INSTALL}/include)
endif()

find_package(CapnProto CONFIG REQUIRED)

include_directories(logger)
add_subdirectory(logger)
add_subdirectory(audio)
