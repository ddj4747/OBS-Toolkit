add_library(plugin_backend STATIC)
find_package(miniupnpc REQUIRED)

file(
  GLOB_RECURSE PLUGIN_BACKEND_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.m
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm
)

file(
  GLOB_RECURSE PLUGIN_BACKEND_HEADERS
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp
)

target_sources(plugin_backend PRIVATE ${PLUGIN_BACKEND_SOURCES} ${PLUGIN_BACKEND_HEADERS})
target_link_libraries(
  plugin_backend
  PUBLIC
    Qt6::Core
    Qt6::Network
    OBS::libobs
    plugin_common
    FFmpeg::avcodec
    FFmpeg::avformat
    FFmpeg::avutil
    FFmpeg::swscale
    FFmpeg::swresample
)
target_include_directories(plugin_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

if(APPLE)
  set(_universal_miniupnpc_dir "${CMAKE_SOURCE_DIR}/.deps/conan-macos-universal")
  if(NOT EXISTS "${_universal_miniupnpc_dir}/lib/libminiupnpc.a")
    message(FATAL_ERROR "Universal miniupnpc archive is missing. Run configure.py before configuring the project.")
  endif()
  target_include_directories(plugin_backend PUBLIC "${_universal_miniupnpc_dir}/include")
  target_link_libraries(plugin_backend PUBLIC "${_universal_miniupnpc_dir}/lib/libminiupnpc.a")
else()
  target_link_libraries(plugin_backend PUBLIC miniupnpc::miniupnpc)
endif()

set_target_properties(plugin_backend PROPERTIES AUTOMOC ON)

if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  set_property(TARGET plugin_backend PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
