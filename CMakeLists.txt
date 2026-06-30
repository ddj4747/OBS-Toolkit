cmake_minimum_required(VERSION 3.28...3.30)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

file(GLOB_RECURSE PLUGIN_SOURCES CONFIGURE_DEPENDS plugin/src/*.c plugin/src/*.cpp plugin/src/*.m plugin/src/*.mm)
file(GLOB_RECURSE PLUGIN_HEADERS CONFIGURE_DEPENDS plugin/inc/*.h plugin/inc/*.hpp)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE plugin/inc)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(DEFINED USER_PLUGIN_OBS_LOCATION)
  message(STATUS "OBS Location passed from Conan: ${USER_PLUGIN_OBS_LOCATION}")

  if(WIN32)
    set(OBS_PROGRAM_PATH "${USER_PLUGIN_OBS_LOCATION}/bin/64bit/obs64.exe")
    set(OBS_PLUGIN_DEST "${USER_PLUGIN_OBS_LOCATION}/obs-plugins/64bit")

    add_custom_target(
      run
      COMMAND taskkill /F /IM obs64.exe || exit 0
      COMMAND ${CMAKE_COMMAND} -E remove -f "${OBS_PLUGIN_DEST}/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGIN_DEST}/"
      COMMAND start "" "${OBS_PROGRAM_PATH}"
      COMMENT "Deploying plugin and launching OBS..."
      DEPENDS ${CMAKE_PROJECT_NAME}
    )
  elseif(APPLE)
    set(OBS_PROGRAM_PATH "${USER_PLUGIN_OBS_LOCATION}/OBS.app/Contents/MacOS/OBS")
    set(OBS_PLUGIN_DEST "$ENV{HOME}/Library/Application Support/obs-studio/plugins/${CMAKE_PROJECT_NAME}/bin")

    add_custom_target(
      run
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGIN_DEST}"
      COMMAND ${CMAKE_COMMAND} -E remove -f "${OBS_PLUGIN_DEST}/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGIN_DEST}/"
      COMMAND "${OBS_PROGRAM_PATH}"
      COMMENT "Deploying plugin and launching OBS..."
      DEPENDS ${CMAKE_PROJECT_NAME}
    )
  else()
    set(OBS_PROGRAM_PATH "${USER_PLUGIN_OBS_LOCATION}")
    set(OBS_PLUGIN_DEST "$ENV{HOME}/.config/obs-studio/plugins/${CMAKE_PROJECT_NAME}/bin/64bit")

    add_custom_target(
      run
      COMMAND pkill -x obs || true
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OBS_PLUGIN_DEST}"
      COMMAND ${CMAKE_COMMAND} -E remove -f "${OBS_PLUGIN_DEST}/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>"
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGIN_DEST}/"
      COMMAND "${OBS_PROGRAM_PATH}"
      COMMENT "Deploying plugin and launching OBS..."
      DEPENDS ${CMAKE_PROJECT_NAME}
    )
  endif()
endif()
