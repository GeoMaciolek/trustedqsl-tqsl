project(tqsl_tests)

ADD_SUBDIRECTORY(gtest-1.7.0)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(basicTest, basictest.cpp)
target_link_libraries(basicTest, gtest_main)
add_test(basicTest, basicTest)