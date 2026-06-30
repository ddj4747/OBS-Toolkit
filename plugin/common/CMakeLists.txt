configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.cpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/plugin-support.cpp"
  @ONLY
)

add_library(plugin_common STATIC)

target_sources(
  plugin_common
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/plugin-support.cpp"
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc/plugin-support.h"
)

target_include_directories(plugin_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(plugin_common PUBLIC OBS::libobs)

if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  set_property(TARGET plugin_common PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
