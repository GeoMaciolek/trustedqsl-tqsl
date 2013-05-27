project(tqsl)

set(OSX_ARCH i386 CACHE STRING "Architectures to build for OSX")

string(REPLACE " " ";" CMAKE_OSX_ARCHITECTURES ${OSX_ARCH})

if(WIN32)
set(wxWidgets_USE_STATIC ON)
add_definitions(-DCURL_STATICLIB)
endif()

find_package(CURL REQUIRED)

find_package(wxWidgets COMPONENTS core base adv html REQUIRED)

find_package(ZLIB REQUIRED)

include(${wxWidgets_USE_FILE})

if(APPLE)
find_library(CARBON_LIBRARY Carbon)
find_path(CARBON_INCLUDE_DIR Carbon.h)
endif()

include_directories(${tqsllib_SOURCE_DIR} ${ZLIB_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${CARBON_INCLUDE_DIR})

#get version from file

file(READ "tqslversion.ver" TQSLVERSION)
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" TQSL_VERSION_MAJOR ${TQSLVERSION})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2" TQSL_VERSION_MINOR ${TQSLVERSION})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3" TQSL_VERSION_POINT ${TQSLVERSION})


#get build # with git

find_program(GIT_PROG git "C:\\Program Files (x86)\\Git\\bin" "C:\\Program Files\\Git\\bin")

if(GIT_PROG) #we can use git
execute_process(COMMAND ${GIT_PROG} describe RESULT_VARIABLE GITERROR ERROR_QUIET OUTPUT_VARIABLE HEAD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT GITERROR) #git ran alright
		set(BUILD ${HEAD_COMMIT})
	endif()
endif()

if(BUILD) 
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h")
else() #hope this is a source distribution, which will always include a correct tqslbuild
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h")
		#but otherwise someone downloaded a snapshot without a buildfile
		set(BUILD "unknown")
		configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h")
	endif()
endif()

if(WIN32)
set(TQSLLIB_NAME tqsllib2)
set(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set(TQSL_RCFILE "tqsl.rc")
set(TQSLCERT_RCFILE "tqslcert.rc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${TQSLCERT_RCFILE}.in" "${CMAKE_CURRENT_SOURCE_DIR}/${TQSLCERT_RCFILE}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${TQSL_RCFILE}.in" "${CMAKE_CURRENT_SOURCE_DIR}/${TQSL_RCFILE}")
else()
set(TQSLLIB_NAME tqsllib)
endif()

if(APPLE)
set(OSX_ICONFILE "tqslicons.icns")
set(MACOSX_BUNDLE_ICON_FILE ${OSX_ICONFILE})
SET_SOURCE_FILES_PROPERTIES(${OSX_ICONFILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set(TQSL_RCFILE ${OSX_ICONFILE})
set(TQSLCERT_RCFILE ${OSX_ICONFILE})
endif()

if(NOT WIN32) #rpath setup
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()


set(TQSLCERT_SRC tqslcert.cpp crqwiz.cpp dxcc.cpp certtree.cpp tqslcert_prefs.cpp getpassword.cpp extwizard.cpp loadcertwiz.cpp wxutil.cpp)
set(TQSLCERT_HDR certtree.h tqslcertctrls.h util.h dxcc.h crqwiz.h getpassword.h extwizard.h tqslcert_prefs.h getpassword.h loadcertwiz.h wxutil.h tqslbuild.h)
set(TQSLCERT_XPM cert.xpm nocert.xpm broken-cert.xpm folder.xpm key.xpm)

add_executable(tqslcert WIN32 MACOSX_BUNDLE ${TQSLCERT_SRC} ${TQSLCERT_HDR} ${TQSLCERT_XPM} ${TQSLCERT_RCFILE})

set(TQSL_SRC tqsl.cpp extwizard.cpp tqslwiz.cpp dxcc.cpp stationdial.cpp qsodatadialog.cpp tqslvalidator.cpp tqsl_prefs.cpp wxutil.cpp)
set(TQSL_HDR tqslcert.h tqslapp.h tqslwiz.h qsodatadialog.h tqslexcept.h tqslpaths.h stationdial.h dxcc.h tqsl_prefs.h extwizard.h certtree.h tqslvalidator.h tqslbuild.h)
set(TQSL_XPM left.xpm right.xpm bottom.xpm top.xpm key.xpm)

add_executable(tqsl WIN32 MACOSX_BUNDLE ${TQSL_SRC} ${TQSL_HDR} ${TQSL_XPM} ${TQSL_RCFILE})

target_link_libraries(tqslcert ${TQSLLIB_NAME} ${wxWidgets_LIBRARIES} ${CARBON_LIBRARY})
target_link_libraries(tqsl ${TQSLLIB_NAME}
  ${wxWidgets_LIBRARIES} ${ZLIB_LIBRARIES} ${CURL_LIBRARIES} ${CARBON_LIBRARY})

if(NOT APPLE AND NOT WIN32)
add_definitions("-DCONFDIR=\"${CMAKE_INSTALL_PREFIX}/share/TrustedQSL/\"")
install(TARGETS tqsl DESTINATION bin)
install(TARGETS tqslcert DESTINATION bin)
install(DIRECTORY help DESTINATION share/TrustedQSL)
install(FILES trustedqsl.5 DESTINATION share/man/man5)
endif()
