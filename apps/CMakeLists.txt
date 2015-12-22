project(tqsl)

set(OSX_ARCH i386 CACHE STRING "Architectures to build for OSX")

string(REPLACE " " ";" CMAKE_OSX_ARCHITECTURES ${OSX_ARCH})

if(WIN32)
	set(wxWidgets_USE_STATIC ON)
	add_definitions(-DCURL_STATICLIB)
	add_definitions(-DXML_STATIC)
endif()

find_package(CURL REQUIRED)
find_package(EXPAT REQUIRED)

find_package(wxWidgets COMPONENTS core base adv html REQUIRED)

find_package(ZLIB REQUIRED)

include(${wxWidgets_USE_FILE})

if(APPLE)
	find_library(CARBON_LIBRARY Carbon)
	find_path(CARBON_INCLUDE_DIR Carbon.h)
endif()

include_directories(${tqsllib_SOURCE_DIR} ${ZLIB_INCLUDE_DIR} ${EXPAT_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${CARBON_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${BDB_INCLUDE_DIR})

#get version from file

file(STRINGS "tqslversion.ver" TQSLVERSION)
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" TQSL_VERSION_MAJOR ${TQSLVERSION})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2" TQSL_VERSION_MINOR ${TQSLVERSION})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3" TQSL_VERSION_UPDATE ${TQSLVERSION})

# Handle case where version number does not have an update i.e. 2.0
if (TQSL_VERSION_MAJOR STREQUAL ${TQSLVERSION}) 
	string(REGEX REPLACE "([0-9]+)\\.([0-9]+)" "\\1" TQSL_VERSION_MAJOR ${TQSLVERSION})
	string(REGEX REPLACE "([0-9]+)\\.([0-9]+)" "\\2" TQSL_VERSION_MINOR ${TQSLVERSION})
	set(TQSL_VERSION_UPDATE "0")
endif()

#get build # with git

find_program(GIT_PROG git "C:\\Program Files (x86)\\Git\\bin" "C:\\Program Files\\Git\\bin")

if(GIT_PROG) #we can use git
	execute_process(COMMAND ${GIT_PROG} describe RESULT_VARIABLE GITERROR ERROR_QUIET OUTPUT_VARIABLE HEAD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT GITERROR) #git ran alright
		set(BUILD ${HEAD_COMMIT})
	else()		# Git error - hope this is a source distro
		if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h)
			file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h BUILDSTR REGEX "\\[.*\\]")
			string(REGEX REPLACE ".*\\[(.*)\\].*" "\\1" BUILD ${BUILDSTR})
		endif()
	endif()
else()	# No git, again grab from source
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h)
		file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h BUILDSTR REGEX "\\[.*\\]")
		string(REGEX REPLACE ".*\\[(.*)\\].*" "\\1" BUILD ${BUILDSTR})
	endif()
endif()

# If BUILD is set (we have a version) and BUILDSTR is not (we got that build from
# git), then write a tqslbuild.h. Else leave it alone.
if(BUILD AND NOT BUILDSTR)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h")
else() #hope this is a source distribution, which will always include a correct tqslbuild
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h")
		#but otherwise someone downloaded a snapshot without a buildfile
		set(BUILD "unknown")
		configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/tqslbuild.h")
	endif()
endif()

# In case the versions couldn't be deduced, set them
if(NOT ${TQSL_VERSION_MAJOR})
	set(TQSL_VERSION_MAJOR 2)
endif()

if(NOT TQSL_VERSION_MINOR)
	set(TQSL_VERSION_MINOR 1)
endif()

if (NOT TQSL_VERSION_UPDATE)
	set(TQSL_VERSION_UPDATE 0)
endif()

IF(TQSL_VERSION_UPDATE AND NOT TQSL_VERSION_UPDATE EQUAL 0) 
	set(TQSLVERSION "${TQSL_VERSION_MAJOR}.${TQSL_VERSION_MINOR}.${TQSL_VERSION_UPDATE}")
else()
	set(TQSLVERSION "${TQSL_VERSION_MAJOR}.${TQSL_VERSION_MINOR}")
endif()

if(${BUILD} MATCHES "-(alpha|beta|test|rc[0-9])")
	message("${BUILD} is a test build")
	add_definitions(-DTQSL_TEST_BUILD)
endif()

if(WIN32)
	set(TQSLLIB_NAME tqsllib2)
	set(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
	set(TQSL_RCFILE "tqsl.rc")
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${TQSL_RCFILE}.in" "${CMAKE_CURRENT_SOURCE_DIR}/${TQSL_RCFILE}")
else()
	set(TQSLLIB_NAME tqsllib)
endif()

if(APPLE)
	set(OSX_ICONFILE "tqslicons.icns")
	set(MACOSX_BUNDLE_INFO_STRING "tqsl")
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.arrl.tqsl")
	set(MACOSX_BUNDLE_LONG_VERSION_STRING "TQSL V ${TQSLVERSION} build ${BUILD}")
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${TQSLVERSION}")
	set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2001-2015 American Radio Relay League")

	set(MACOSX_BUNDLE_ICON_FILE ${OSX_ICONFILE})
	SET_SOURCE_FILES_PROPERTIES(${OSX_ICONFILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	set(TQSL_RCFILE ${OSX_ICONFILE})
endif()

if(NOT WIN32) #rpath setup
	SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 
	SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()


set(TQSL_SRC tqsl.cpp extwizard.cpp tqslwiz.cpp dxcc.cpp stationdial.cpp
qsodatadialog.cpp tqslvalidator.cpp tqsl_prefs.cpp wxutil.cpp tqslhelp.cpp
crqwiz.cpp certtree.cpp getpassword.cpp loadcertwiz.cpp loctree.cpp)
set(TQSL_HDR tqslapp.h tqslwiz.h qsodatadialog.h tqslexcept.h
tqslpaths.h stationdial.h dxcc.h tqsl_prefs.h extwizard.h certtree.h
tqslvalidator.h tqslbuild.h tqslhelp.h certtree.h tqslctrls.h util.h
getpassword.h extwizard.h loadcertwiz.h wxutil.h loctree.h tqsltrace.h)
set(TQSL_XPM left.xpm right.xpm bottom.xpm top.xpm key.xpm cert.xpm nocert.xpm
broken-cert.xpm folder.xpm replaced.xpm expired.xpm home.xpm delete.xpm edit.xpm
loc_add_dis.xpm properties.xpm download_dis.xpm loc_add.xpm upload_dis.xpm
download.xpm file_edit_dis.xpm upload.xpm delete_dis.xpm edit_dis.xpm
file_edit.xpm properties_dis.xpm save.xpm import.xpm lotw.xpm)
add_executable(tqsl WIN32 MACOSX_BUNDLE ${TQSL_SRC} ${TQSL_HDR} ${TQSL_XPM} ${TQSL_RCFILE})

target_link_libraries(tqsl ${TQSLLIB_NAME}
  ${wxWidgets_LIBRARIES} ${ZLIB_LIBRARIES} ${CURL_LIBRARIES} ${CARBON_LIBRARY} ${EXPAT_LIBRARIES})

if(NOT APPLE AND NOT WIN32)
	add_definitions("-DCONFDIR=\"${CMAKE_INSTALL_PREFIX}/share/TrustedQSL/\"")
	install(TARGETS tqsl DESTINATION bin)
	install(DIRECTORY help DESTINATION share/TrustedQSL)
	if (CMAKE_INSTALL_PREFIX STREQUAL "/usr")
		install(FILES tqsl.5 DESTINATION share/man/man5)
	else()
		install(FILES tqsl.5 DESTINATION man/man5)
	endif()
	install(FILES icons/key48.png DESTINATION share/pixmaps RENAME TrustedQSL.png)
	install(FILES lang/de/tqslapp.mo DESTINATION share/locale/de/LC_MESSAGES)
	install(FILES lang/de/wxstd.mo DESTINATION share/locale/de/LC_MESSAGES)
	install(FILES lang/es/tqslapp.mo DESTINATION share/locale/es/LC_MESSAGES)
	install(FILES lang/es/wxstd.mo DESTINATION share/locale/es/LC_MESSAGES)
	install(FILES lang/fr/tqslapp.mo DESTINATION share/locale/fr/LC_MESSAGES)
	install(FILES lang/fr/wxstd.mo DESTINATION share/locale/fr/LC_MESSAGES)
	install(FILES lang/it/tqslapp.mo DESTINATION share/locale/it/LC_MESSAGES)
	install(FILES lang/it/wxstd.mo DESTINATION share/locale/it/LC_MESSAGES)
	install(FILES lang/ja/tqslapp.mo DESTINATION share/locale/ja/LC_MESSAGES)
	install(FILES lang/ja/wxstd.mo DESTINATION share/locale/ja/LC_MESSAGES)
	install(FILES lang/pt/tqslapp.mo DESTINATION share/locale/pt/LC_MESSAGES)
	install(FILES lang/pt/wxstd.mo DESTINATION share/locale/pt/LC_MESSAGES)
endif()
