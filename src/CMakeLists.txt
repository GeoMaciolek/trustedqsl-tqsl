project (tqsllib)

set(OSX_ARCH i386 CACHE STRING "Architectures to build for OSX")

string(REPLACE " " ";" CMAKE_OSX_ARCHITECTURES ${OSX_ARCH})

if(WIN32)
  add_definitions(-DXML_STATIC)
endif()

find_package(EXPAT REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
# Prefer LMDB, fall back to BDB if that's not available
FIND_PACKAGE(LMDB)
if(NOT LMDB_FOUND)
	FIND_PACKAGE(BDB REQUIRED)
else()
	add_definitions(-DUSE_LMDB)
endif()

set(TQSLLIB_VERSION_MAJOR 2)
set(TQSLLIB_VERSION_MINOR 5)
set(TQSLLIB_VERSION "${TQSLLIB_VERSION_MAJOR}.${TQSLLIB_VERSION_MINOR}")

add_definitions(-DTQSLLIB_VERSION_MAJOR=${TQSLLIB_VERSION_MAJOR})
add_definitions(-DTQSLLIB_VERSION_MINOR=${TQSLLIB_VERSION_MINOR})

option(TQSLLIB_STATIC "Build static tqsllib" OFF)
if (${TQSLLIB_STATIC})
set(TQSLLIBTYPE STATIC)
else()
set(TQSLLIBTYPE SHARED)
endif()

if(WIN32)
set(TQSLLIB_RCFILE "tqsllib.rc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${TQSLLIB_RCFILE}.in" "${CMAKE_CURRENT_SOURCE_DIR}/${TQSLLIB_RCFILE}")
set(TQSLLIBNAME "tqsllib2") #for compatibility with existing software
else()
set(TQSLLIBNAME "tqsllib")
endif()

set(TQSLLIB_SRC tqsllib.cpp openssl_cert.cpp adif.cpp xml.cpp location.cpp tqslconvert.cpp cabrillo.cpp)
set(TQSLLIB_HDR xml.h openssl_cert.h tqslexc.h tqsllib.h tqslerrno.h cabrillo.h adif.h tqslconvert.h)

if(APPLE)
find_library(CARBON_LIBRARY Carbon)
find_path(CARBON_INCLUDE_DIR Carbon.h)
endif()
if(WIN32)
#def file for correct 2.x exports
set(TQSLLIB_SRC ${TQSLLIB_SRC} "tqsllib2.def")
endif()

add_library(${TQSLLIBNAME} ${TQSLLIBTYPE} ${TQSLLIB_SRC} ${TQSLLIB_HDR} ${TQSLLIB_RCFILE})
target_link_libraries(${TQSLLIBNAME} ${OPENSSL_LIBRARIES} ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${BDB_LIBRARIES} ${LMDB_LIBRARIES} ${CARBON_LIBRARY})
include_directories(${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} ${EXPAT_INCLUDE_DIR} ${BDB_INCLUDE_DIR} ${CARBON_INCLUDE_DIR})

if(NOT APPLE AND NOT WIN32)
set_source_files_properties(location.cpp PROPERTIES COMPILE_DEFINITIONS CONFDIR="${CMAKE_INSTALL_PREFIX}/share/TrustedQSL/")
set(HEADERS_TO_INSTALL tqsllib.h tqslerrno.h cabrillo.h adif.h tqslconvert.h)
install(TARGETS tqsllib DESTINATION lib${LIB_SUFFIX})
install(FILES config.xml DESTINATION share/TrustedQSL)
install(FILES ${HEADERS_TO_INSTALL} DESTINATION include)
endif()

option(BUILD_ALL_EXTRAPROGS "Build all extra programs")

if(BUILD_ALL_EXTRAPROGS)
set(BUILD_GENCRQ ON)
set(BUILD_LOADCERT ON)
set(BUILD_STATIONLOC ON)
set(BUILD_CONVERTER ON)

if(WIN32 AND (BUILD_GENCRQ OR BUILD_LOADCERT OR BUILD_STATIONLOC OR BUILD_CONVERTER))
message("I can't build the tqsllib programs on Windows yet. Disabling them...")
set(BUILD_GENCRQ OFF)
set(BUILD_LOADCERT OFF)
set(BUILD_STATIONLOC OFF)
set(BUILD_CONVERTER OFF)
endif()

else()
option(BUILD_GENCRQ "Build gen_crq")
option(BUILD_LOADCERT "Build load_cert")
option(BUILD_STATIONLOC "Build station_loc")
option(BUILD_CONVERTER "Build converter")

endif(BUILD_ALL_EXTRAPROGS)

if(BUILD_GENCRQ)
add_executable(gen_crq gen_crq.cpp)
target_link_libraries(gen_crq ${TQSLLIBNAME})
endif(BUILD_GENCRQ)

if(BUILD_LOADCERT)
add_executable(load_cert load_cert.cpp)
target_link_libraries(load_cert ${TQSLLIBNAME})
endif(BUILD_LOADCERT)

if(BUILD_STATIONLOC)
add_executable(station_loc station_loc.cpp)
target_link_libraries(station_loc ${TQSLLIBNAME})
endif(BUILD_STATIONLOC)

if(BUILD_CONVERTER)
add_executable(converter converter.cpp)
target_link_libraries(converter ${TQSLLIBNAME})
endif(BUILD_CONVERTER)
