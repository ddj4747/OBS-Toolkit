if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.28)
  project(obs_relay LANGUAGES CXX)
  find_package(Qt6 REQUIRED COMPONENTS Core Network)
  find_package(spdlog REQUIRED)
  find_package(httplib REQUIRED)
  find_package(OpenSSL REQUIRED)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

set(RELAY_API_PORT 8080 CACHE STRING "Relay HTTPS API port")
set(RELAY_SRTLA_PORT 5000 CACHE STRING "Relay SRTLA port")
set(RELAY_SRT_PORT 8890 CACHE STRING "Relay SRT port")
set(RELAY_RTSP_PORT 8554 CACHE STRING "Relay RTSP port")
set(RELAY_RTP_PORT 8000 CACHE STRING "Relay RTP port")
set(RELAY_RTCP_PORT 8001 CACHE STRING "Relay RTCP port")
set(RELAY_RTMP_PORT 1935 CACHE STRING "Relay RTMP port")
set(RELAY_WEBRTC_PORT 8889 CACHE STRING "Relay WebRTC port")
set(RELAY_MOQ_PORT 8892 CACHE STRING "Relay MoQ port")
set(RELAY_MOQ_QUIC_PORT 8893 CACHE STRING "Relay MoQ QUIC port")

set(
  RELAY_PORT_OPTIONS
  API
  SRTLA
  SRT
  RTSP
  RTP
  RTCP
  RTMP
  WEBRTC
  MOQ
  MOQ_QUIC
)
foreach(_name IN LISTS RELAY_PORT_OPTIONS)
  set(_variable "RELAY_${_name}_PORT")
  if(NOT "${${_variable}}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${_variable} must be an integer between 1 and 65535")
  elseif("${${_variable}}" GREATER 65535)
    message(FATAL_ERROR "${_variable} must be an integer between 1 and 65535")
  endif()
endforeach()

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
foreach(_name IN LISTS RELAY_PORT_OPTIONS)
  target_compile_definitions(relay_server PRIVATE "RELAY_${_name}_PORT=${RELAY_${_name}_PORT}")
endforeach()
target_include_directories(relay_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(
  relay_server
  PRIVATE Qt6::Core Qt6::Network spdlog::spdlog httplib::httplib toolkit_common
)
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
  COMMAND docker build --tag obs-relay -f "${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile" "${CMAKE_CURRENT_SOURCE_DIR}/.."
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)
