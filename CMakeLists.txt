cmake_minimum_required(VERSION 3.28...3.30)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/ENV_Loader.cmake")

project(${_name} VERSION ${_version})

load_env_file(.env)

include(compilerconfig)
include(defaults)
include(helpers)

list(APPEND CMAKE_PREFIX_PATH $ENV{QT_DIR}/lib/cmake)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 COMPONENTS Widgets Core)

#add_subdirectory(plugin/backend)
add_subdirectory(plugin/common)
add_subdirectory(plugin/frontend)
add_subdirectory(plugin/main)

target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)

if(DEFINED USER_PLUGIN_OBS_LOCATION)
  message(STATUS "OBS Location passed from Conan: ${USER_PLUGIN_OBS_LOCATION}")

  if(WIN32)
    set(OBS_PROGRAM_PATH ${USER_PLUGIN_OBS_LOCATION}\\bin\\64bit\\obs64.exe)
    set(OBS_PLUGIN_DEST ${USER_PLUGIN_OBS_LOCATION}/obs-plugins/64bit)
    set(OBS_PLUGIN_DATA_DEST ${USER_PLUGIN_OBS_LOCATION}/data/obs-plugins/${CMAKE_PROJECT_NAME})

    add_custom_target(
            run
            COMMAND ${CMAKE_COMMAND} -E remove -f "${OBS_PLUGIN_DEST}/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${OBS_PLUGIN_DEST}/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "${OBS_PLUGIN_DATA_DEST}"
            COMMAND start "" "${OBS_PROGRAM_PATH}"
            COMMENT ${OBS_PROGRAM_PATH}
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
