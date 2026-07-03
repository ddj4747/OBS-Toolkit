configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.cpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/plugin-support.cpp"
  @ONLY
)

add_library(plugin_common STATIC)

file(
  GLOB_RECURSE PLUGIN_COMMON_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.m
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm
)

file(
  GLOB_RECURSE PLUGIN_COMMON_HEADERS
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp
)

target_sources(
  plugin_common
  PRIVATE ${PLUGIN_COMMON_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/plugin-support.cpp" ${PLUGIN_COMMON_HEADERS}
)

target_include_directories(plugin_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(plugin_common PUBLIC OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets)

if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  set_property(TARGET plugin_common PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
