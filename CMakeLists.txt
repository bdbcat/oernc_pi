##---------------------------------------------------------------------------
## Author:      bdbcat aka. dsr (Dave Register)
## Copyright:
## License:     wxWidgets License
##---------------------------------------------------------------------------


# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2)
# This should be 2.8.0 to have FindGTK2 module
IF (COMMAND cmake_policy)
  CMAKE_POLICY(SET CMP0003 OLD)
  CMAKE_POLICY(SET CMP0005 OLD)
  CMAKE_POLICY(SET CMP0011 OLD)
ENDIF (COMMAND cmake_policy)

PROJECT(oernc_pi)

SET(PLUGIN_SOURCE_DIR .)

SET(PACKAGE_NAME oernc_pi)
SET(VERBOSE_NAME oeRNCCharts)
SET(TITLE_NAME oeRNCCharts)
SET(CPACK_PACKAGE_CONTACT "Dave Register")

MESSAGE (STATUS "*** Building ${PACKAGE_NAME} ***")

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "1")
SET(VERSION_PATCH "0")
SET(PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )
SET(OCPN_MIN_VERSION "ov50")

#SET(CMAKE_BUILD_TYPE Release)

set (CMAKE_CXX_STANDARD 11)
message(STATUS "Setting C++11 standard via cmake standard mechanism")

IF( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE Release CACHE STRING
       "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
       FORCE )
ENDIF( NOT CMAKE_BUILD_TYPE )

MESSAGE (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

SET(CMAKE_VERBOSE_MAKEFILE ON)

IF (PREFIX)
 SET(CMAKE_INSTALL_PREFIX ${PREFIX})
ENDIF (PREFIX)

SET(PREFIX_BIN bin)
SET(PREFIX_INCLUDE include)
SET(PREFIX_DATA share)
SET(PREFIX_PKGDATA ${PREFIX_DATA}/${PACKAGE_NAME})

IF (NOT WIN32)
  # default
  SET (ARCH "i386")
  IF (UNIX AND NOT APPLE)
    IF (PREFIX)
        SET(CMAKE_INSTALL_PREFIX ${PREFIX})
    ELSE (PREFIX )
        SET(CMAKE_INSTALL_PREFIX "/usr")
    ENDIF (PREFIX)

    MESSAGE (STATUS "*** Will install to ${CMAKE_INSTALL_PREFIX}  ***")

    IF (EXISTS /etc/debian_version)
      SET (PACKAGE_FORMAT "DEB")
      SET (PACKAGE_DEPS "libc6, libwxgtk3.0-0, libglu1-mesa (>= 7.0.0) curl")
      SET (LIB_INSTALL_DIR "lib")
      IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "amd64")
      ELSE (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "i386")
      ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    ENDIF (EXISTS /etc/debian_version)
    IF (EXISTS /etc/redhat-release)
      SET (PACKAGE_FORMAT "RPM")
      SET (PACKAGE_DEPS  "opencpn")
      IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "x86_64")
        SET (LIB_INSTALL_DIR "lib64")
      ELSE (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "i386")
        SET (LIB_INSTALL_DIR "lib")
      ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    ENDIF (EXISTS /etc/redhat-release)
  ENDIF (UNIX AND NOT APPLE)
endif(NOT WIN32)

IF (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
  SET (ARCH "armhf")
  ADD_DEFINITIONS( -DARMHF )
ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

MESSAGE (STATUS "*** Build Architecture is ${ARCH}")


#  To build for android, use this cmake invocation..
#$cmake -DUSE_GARMINHOST=OFF -D_wx_selected_config=androideabi-qt
#-DCMAKE_TOOLCHAIN_FILE=../buildandroid/build_android.cmake
#-DwxQt_Build=build_android_55_intl -DwxQt_Base=/home/dsr/Projects/wxqt/wxWidgets
#-DQt_Base=/home/dsr/Projects/qt5bare -DQt_Build=build_release/qtbase ..

IF(DEFINED _wx_selected_config)

#  May be overridden
IF(NOT DEFINED Qt_Build)
  SET(Qt_Build "android_armv7")
ENDIF(NOT DEFINED Qt_Build)

MESSAGE (STATUS "selected config ${_wx_selected_config}")

IF(_wx_selected_config MATCHES "androideabi-qt")
  MESSAGE (STATUS "Building for wxQt-Android")
  MESSAGE (STATUS "Qt_Base/Build: " ${Qt_Base} "/" ${Qt_Build})
  MESSAGE (STATUS "wxQt_Base/Build: " ${wxQt_Base} "/" ${wxQt_Build})
  SET(QT_ANDROID "ON")
  SET(QT_LINUX "OFF")
  SET(QT "ON")
ENDIF(_wx_selected_config MATCHES "androideabi-qt")

ENDIF(DEFINED _wx_selected_config)


INCLUDE("cmake/PluginConfigure.cmake")

ADD_DEFINITIONS(-DMAKING_PLUGIN)


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/wxJSON)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/cpl)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/dsa)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/lodepng)


IF(NOT WIN32)
 ADD_DEFINITIONS( "-Wall -g -fexceptions -O2 -Wno-unused -fno-strict-aliasing")

 IF(CMAKE_BUILD_TYPE MATCHES "Debug")
    ADD_DEFINITIONS( " -O0")
 ENDIF(CMAKE_BUILD_TYPE MATCHES "Debug")

 IF(NOT APPLE)
  SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
  ADD_DEFINITIONS( "-rdynamic" )
 ELSE(NOT APPLE)
  SET(CMAKE_SHARED_LINKER_FLAGS "-Wl -undefined dynamic_lookup")
  ADD_DEFINITIONS( "-Wno-overloaded-virtual" )
 ENDIF(NOT APPLE)

ENDIF(NOT WIN32)


# Add some definitions to satisfy MS
IF(WIN32)
    ADD_DEFINITIONS(-D__MSVC__)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_DEPRECATE)
ENDIF(WIN32)

# Let cmake find additional modules private
LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})


ADD_DEFINITIONS(-DTIXML_USE_STL)


# For convenience we define the sources as a variable. You can add
# header files and cpp/c files and CMake will sort them out


SET(SRC_OERNC
            src/oernc_pi.h
            src/oernc_pi.cpp
            src/chart.cpp
            src/chart.h
            src/oernc_inStream.h
            src/oernc_inStream.cpp
            src/ellipse.c
            src/base64.h
            src/base64.c
            src/lodepng/lodepng.cpp
            src/lodepng/lodepng.h
            src/ochartShop.cpp
            src/ochartShop.h
            src/fpr.cpp
     )

IF(QT_ANDROID)
    SET( SRC_OERNC ${SRC_OERNC} src/androidSupport.cpp )
ENDIF(QT_ANDROID)


IF(NOT QT_ANDROID)
    OPTION(OCPN_USE_CURL "Use Curl libraries" ON)
    ADD_DEFINITIONS(-D__OCPN_USE_CURL__)
ENDIF(NOT QT_ANDROID)

IF(WIN32)
    OPTION(OCPN_USE_EXTERN_CURL "Use external libcurl" OFF)
ELSE(WIN32)
    SET(OCPN_USE_EXTERN_CURL ON)
ENDIF(WIN32)

IF(NOT QT_ANDROID)

    IF(OCPN_USE_EXTERN_CURL)
        FIND_PACKAGE(CURL REQUIRED)
        IF(CURL_FOUND)
            MESSAGE (STATUS "Using SYSTEM curl library...${CURL_LIBRARIES}...${CURL_INCLUDE_DIR}")
        ENDIF(CURL_FOUND)
        INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
    ELSE(OCPN_USE_EXTERN_CURL)
        INCLUDE("Curl.cmake")
        MESSAGE (STATUS "Using local curl library...")
    ENDIF(OCPN_USE_EXTERN_CURL)


    IF(WIN32)
        SET(SRC_WXCURL
        src/wxcurl/base.cpp
        src/wxcurl/dav.cpp
        src/wxcurl/davtool.cpp
        #src/wxcurl/dialog.cpp
        src/wxcurl/ftp.cpp
        src/wxcurl/ftpparse.cpp
        src/wxcurl/ftptool.cpp
        src/wxcurl/http.cpp
        src/wxcurl/panel.cpp
        src/wxcurl/telnet.cpp
        src/wxcurl/thread.cpp
        src/wxcurl/utils.cpp
        )
    ENDIF(WIN32)

#    ADD_LIBRARY(WXCURL ${SRC_WXCURL})
    INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/wxcurl)
#    SET(EXTRA_LIBS ${EXTRA_LIBS} WXCURL)
#    SET(EXTRA_LIBS ${EXTRA_LIBS} ${CURL_LIBRARIES})

    MESSAGE (STATUS "wxCurl configured...")

ENDIF(NOT QT_ANDROID)

IF(UNIX AND NOT APPLE)
 INCLUDE("cmake/FindTinyXML.cmake")
 FIND_PACKAGE(TinyXML QUIET)
ENDIF(UNIX AND NOT APPLE)

IF(TINYXML_FOUND)
 message (STATUS "${PACKAGE_NAME} building with system tinyxml")
 INCLUDE_DIRECTORIES(${TINYXML_INCLUDE_DIR})
ELSE(TINYXML_FOUND)
 message (STATUS "${PACKAGE_NAME} building with embedded tinyxml")
 INCLUDE_DIRECTORIES(src/tinyxml/)

 SET(SRC_LTINYXML
         src/tinyxml/tinyxml.cpp
         src/tinyxml/tinyxmlerror.cpp
         src/tinyxml/tinyxmlparser.cpp
    )

ENDIF(TINYXML_FOUND)
ADD_DEFINITIONS(-DTIXML_USE_STL)


ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_OERNC} ${SRC_WXCURL} ${SRC_LTINYXML})

IF (QT_ANDROID )
    TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${OCPN_Core_LIBRARIES} )
    TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} "-lgnustl_shared")
    TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} "-lz" )
ENDIF(QT_ANDROID)

IF(WIN32)
  SET(OPENCPN_IMPORT_LIB "C:/Projects/opencpn/build/${CMAKE_CFG_INTDIR}/opencpn.lib")
  TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${OPENCPN_IMPORT_LIB} )
  TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${CURL_LIBRARIES})

ENDIF(WIN32)

IF(NOT APPLE)
TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${wxWidgets_LIBRARIES} )
ENDIF(NOT APPLE)



# Install any special data files or binary helpers here....
# The PlugIn library itself and any "data" directory (with contents) gets installed in generic PluginInstall.cmake later

IF(UNIX AND NOT APPLE)
SET(PARENT opencpn)
SET(PREFIX_PLUGINS ${LIB_INSTALL_DIR}/${PARENT})
SET(PREFIX_PARENTBIN bin)
SET(PREFIX_PARENTDATA ${PREFIX_DATA}/${PARENT})


IF (ARCH MATCHES "amd64" OR ARCH MATCHES "x86_64")
INSTALL(FILES "buildlinux64/ofcserverd/ofcserverd"
       PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
       DESTINATION ${PREFIX_PARENTBIN})
ELSE (ARCH MATCHES "amd64" OR ARCH MATCHES "x86_64")

    IF (ARCH MATCHES "arm*" )
        INSTALL(FILES "buildlinuxarm/ofcserverd/ofcserverd"
          PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          DESTINATION ${PREFIX_PARENTBIN})
    ELSE (ARCH MATCHES "arm*" )
        INSTALL(FILES "buildlinux/ofcserverd/ofcserverd"
          PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          DESTINATION ${PREFIX_PARENTBIN})
    ENDIF (ARCH MATCHES "arm*" )

ENDIF (ARCH MATCHES "amd64" OR ARCH MATCHES "x86_64")

ENDIF(UNIX AND NOT APPLE)

IF(WIN32)

   INSTALL(FILES "buildwin/ofcserverd/ofcserverd.exe"
      PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ WORLD_EXECUTE
      DESTINATION "plugins\\\\${PACKAGE_NAME}")

    INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME DESTINATION "plugins")
    SET(INSTALL_DIRECTORY "plugins\\\\${PACKAGE_NAME}")

ENDIF(WIN32)

IF(APPLE)
SET(CMAKE_INSTALL_PREFIX "/tmp")

INSTALL(FILES "buildosx/ofcserverd/ofcserverd"
       PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ WORLD_EXECUTE
       DESTINATION "ofc_pi/ofc_pi")

#INSTALL(FILES "src/rrc_eula_ChartSetsForOpenCPN.txt"
#      PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
#      DESTINATION "ofc_pi/ofc_pi" )

  # We also need to copy the helper utility to the binary build directory so that the PACKAGES scripts will find it.
  
configure_file(${PROJECT_SOURCE_DIR}/buildosx/ofcserverd/ofcserverd
            ${CMAKE_CURRENT_BINARY_DIR}/ofcserverd COPYONLY)

#configure_file(${PROJECT_SOURCE_DIR}/data/TD-CHS-RM-ATL05.jpg
#            ${CMAKE_CURRENT_BINARY_DIR}/TD-CHS-RM-ATL05.jpg COPYONLY)

#configure_file(${PROJECT_SOURCE_DIR}/data/TD-EXC-BAH.jpg
#            ${CMAKE_CURRENT_BINARY_DIR}/TD-EXC-BAH.jpg COPYONLY)

#configure_file(${PROJECT_SOURCE_DIR}/data/TD-DK-S12.jpg
#            ${CMAKE_CURRENT_BINARY_DIR}/TD-DK-S12.jpg COPYONLY)

#configure_file(${PROJECT_SOURCE_DIR}/data/TD-DK-S21.jpg
#            ${CMAKE_CURRENT_BINARY_DIR}/TD-DK-S21.jpg COPYONLY)

#configure_file(${PROJECT_SOURCE_DIR}/data/TD-NV-KK-2015.jpg
#            ${CMAKE_CURRENT_BINARY_DIR}/TD-NV-KK-2015.jpg COPYONLY)

#configure_file(${PROJECT_SOURCE_DIR}/data/TD-ST-HG.jpg
#            ${CMAKE_CURRENT_BINARY_DIR}/TD-ST-HG.jpg COPYONLY)


ENDIF(APPLE)



SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "dsr")

INCLUDE("cmake/PluginLocalization.cmake")

IF(NOT QT_ANDROID)
INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginPackage.cmake")
ENDIF(NOT QT_ANDROID)


