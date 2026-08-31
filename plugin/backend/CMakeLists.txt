add_library(plugin_backend STATIC)
find_package(miniupnpc REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

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
  PRIVATE Qt6::Core OBS::libobs plugin_common miniupnpc::miniupnpc ${FFMPEG_LIBRARIES}
)
target_include_directories(plugin_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${FFMPEG_INCLUDE_DIRS})

set_target_properties(plugin_backend PROPERTIES AUTOMOC ON)

if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  set_property(TARGET plugin_backend PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
