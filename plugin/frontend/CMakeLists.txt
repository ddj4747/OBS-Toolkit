add_library(plugin_frontend STATIC)

file(
  GLOB_RECURSE PLUGIN_FRONTEND_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.m
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm
)

file(
  GLOB_RECURSE PLUGIN_FRONTEND_HEADERS
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp
)

target_sources(plugin_frontend PRIVATE ${PLUGIN_FRONTEND_SOURCES} ${PLUGIN_FRONTEND_HEADERS})
target_link_libraries(
  plugin_frontend
  PRIVATE Qt6::Core Qt6::Widgets OBS::obs-frontend-api OBS::libobs plugin_common
)
target_include_directories(plugin_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

set_target_properties(plugin_frontend PROPERTIES AUTOMOC ON)

if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  set_property(TARGET plugin_frontend PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
