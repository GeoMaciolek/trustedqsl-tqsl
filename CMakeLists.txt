cmake_minimum_required (VERSION 2.8 FATAL_ERROR)

project(TrustedQSL)

if ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  ADD_COMPILE_OPTIONS("-fPIC")
endif( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )

if(WIN32)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  # just windows for now... unix knows how to find libraries better
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmakemodules/")
find_package(OptionalAddSubdirectory REQUIRED)

option(USE_STATIC_MSVCRT "Use a static Visual C++ Runtime when building with MSVC")

if(MSVC AND USE_STATIC_MSVCRT)
foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
   if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
   endif(${flag_var} MATCHES "/MD")
endforeach(flag_var)
endif()

add_subdirectory(src) #tqsllib
add_subdirectory(apps)
macro_optional_add_subdirectory(tests)
