add_library(${CMAKE_PROJECT_NAME} MODULE)

file(
  GLOB_RECURSE PLUGIN_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.m
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm
)

file(
  GLOB_RECURSE PLUGIN_HEADERS
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE Qt6::Core Qt6::Widgets OBS::obs-frontend-api OBS::libobs plugin_common plugin_frontend
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
