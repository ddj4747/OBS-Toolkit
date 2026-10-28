if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.28)
  project(obs_relay LANGUAGES CXX)
  find_package(Qt6 REQUIRED COMPONENTS Core Network)
  find_package(spdlog REQUIRED)
endif()

file(
  GLOB_RECURSE RELAY_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.m"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
)
file(
  GLOB_RECURSE RELAY_HEADERS
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp"
)
add_executable(relay_server ${RELAY_SOURCES} ${RELAY_HEADERS})
target_include_directories(relay_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(relay_server PRIVATE Qt6::Core Qt6::Network spdlog::spdlog)
target_compile_features(relay_server PRIVATE cxx_std_20)
set_target_properties(relay_server PROPERTIES AUTOMOC ON OUTPUT_NAME obs-relay)

if(WIN32)
  set(_relay_executable_suffix ".exe")
endif()
foreach(_dependency IN ITEMS go-irl mediamtx)
  set(_binary "${CMAKE_SOURCE_DIR}/.deps/${_dependency}${_relay_executable_suffix}")
  if(NOT EXISTS "${_binary}")
    message(FATAL_ERROR "${_binary} is missing. Run configure.py to install relay dependencies.")
  endif()
  add_custom_command(
    TARGET relay_server
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_binary}" "$<TARGET_FILE_DIR:relay_server>"
    VERBATIM
  )
endforeach()

add_custom_target(
  relay_docker_image
  COMMAND docker build --tag obs-relay "${CMAKE_CURRENT_SOURCE_DIR}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)
