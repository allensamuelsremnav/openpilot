enable_testing()

if (MSVC)
  include(FetchContent)
  Fetchcontent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
  )

  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
else()
  find_package(GTest REQUIRED)
  include_directories(${GTEST_INCLUDE_DIRS})
endif()


add_executable(test_ts_fifo test_ts_fifo.cpp)
add_executable(test_arg_parser test_arg_parser.cpp)

if (MSVC)
  
  target_link_libraries(
    test_ts_fifo
    gtest_main
  )

  target_link_libraries(
    test_arg_parser
    gtest_main
  )
else()
  add_compile_options(-fPIC)
  target_link_libraries(test_ts_fifo Threads::Threads ${GTEST_BOTH_LIBRARIES})
  target_link_libraries(test_arg_parser Threads::Threads ${GTEST_BOTH_LIBRARIES})
endif()