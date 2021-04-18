cmake_minimum_required(VERSION 2.8.12)

if(CMAKE_HOST_SYSTEM_NAME MATCHES "CYGWIN|Linux")
  set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
  set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
  set(PKG_CONFIG_EXECUTABLE x86_64-w64-mingw32-pkg-config)
endif()

project(bdasrc)

set(CMAKE_CXX_STANDARD 11)

if(MSVC)
  if(CMAKE_CL_64)
    set(GST_ROOT $ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64})
  else()
    set(GST_ROOT $ENV{GSTREAMER_1_0_ROOT_MSVC_X86})
  endif()
  set(BDA_LIBRARIES
    gstbase-1.0.lib
    gstreamer-1.0.lib
    glib-2.0.lib
    gobject-2.0.lib
    quartz.lib
    strmiids.lib
  )
  set(BDA_INCLUDES
    include
    ${GST_ROOT}/include/glib-2.0
    ${GST_ROOT}/include/gstreamer-1.0
    ${GST_ROOT}/lib/gstreamer-1.0/include
    ${GST_ROOT}/lib/glib-2.0/include
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)
  pkg_search_module(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)
  set(BDA_LIBRARIES
    ksguid ole32 oleaut32 quartz strmiids uuid
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
  )
  set(BDA_INCLUDES
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_BASE_INCLUDE_DIRS}
  )
  add_definitions(-Wall -Wextra)
endif()

link_directories(
  ${GST_ROOT}/lib
)

add_definitions(
  "-DVERSION=\"0.0.1\""
  "-DGST_LICENSE=\"LGPL\""
  "-DGST_PACKAGE_NAME=\"GStreamer BDA Plugin\""
  "-DGST_PACKAGE_ORIGIN=\"https://github.com/raipe/gst-bda\""
  "-DPACKAGE=\"gstreamer\""
)

set(BDA_SRC
  gstbdagrabber.h
  gstbdagrabber.cpp
  gstbdasrc.h
  gstbdasrc.cpp
  gstbdautil.h
  gstbdautil.cpp
  gstbdatypes.h
)

add_library(${PROJECT_NAME} SHARED
  ${BDA_SRC}
)

target_include_directories(${PROJECT_NAME}
  PRIVATE ${BDA_INCLUDES}
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE ${BDA_LIBRARIES}
)
