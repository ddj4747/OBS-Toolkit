# CMake Windows helper functions module

include_guard(GLOBAL)

include(helpers_common)

set(_WINDOWS_HELPERS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# set_target_properties_plugin: Set target properties for use in obs-studio
function(set_target_properties_plugin target)
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs PROPERTIES)
  cmake_parse_arguments(PARSE_ARGV 0 _STPO "${options}" "${oneValueArgs}" "${multiValueArgs}")

  message(DEBUG "Setting additional properties for target ${target}...")

  while(_STPO_PROPERTIES)
    list(POP_FRONT _STPO_PROPERTIES key value)
    set_property(TARGET ${target} PROPERTY ${key} "${value}")
  endwhile()

  string(TIMESTAMP CURRENT_YEAR "%Y")

  set_target_properties(${target} PROPERTIES VERSION 0 SOVERSION ${PLUGIN_VERSION})

  install(TARGETS ${target} RUNTIME DESTINATION "${target}/bin/64bit" LIBRARY DESTINATION "${target}/bin/64bit")

  install(
    FILES "$<TARGET_PDB_FILE:${target}>"
    CONFIGURATIONS RelWithDebInfo Debug Release
    DESTINATION "${target}/bin/64bit"
    OPTIONAL
  )

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${target}>"
      "$<$<CONFIG:Debug,RelWithDebInfo,Release>:$<TARGET_PDB_FILE:${target}>>"
      "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>"
    COMMENT "Copy ${target} to rundir"
    VERBATIM
  )

  target_install_resources(${target})

  get_target_property(target_sources ${target} SOURCES)
  set(target_ui_files ${target_sources})
  list(FILTER target_ui_files INCLUDE REGEX ".+\\.(ui|qrc)")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "UI Files" FILES ${target_ui_files})

  configure_file(
    "${_WINDOWS_HELPERS_DIR}/resources/resource.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc"
  )
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc")
endfunction()

# Helper function to add resources into bundle
function(target_install_resources target)
  message(DEBUG "Installing resources for target ${target}...")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    file(GLOB_RECURSE data_files "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
    foreach(data_file IN LISTS data_files)
      cmake_path(
        RELATIVE_PATH
        data_file
        BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
        OUTPUT_VARIABLE relative_path
      )
      cmake_path(GET relative_path PARENT_PATH relative_path)
      target_sources(${target} PRIVATE "${data_file}")
      source_group("Resources/${relative_path}" FILES "${data_file}")
    endforeach()

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/" DESTINATION "${target}/data" USE_SOURCE_PERMISSIONS)

    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${target}"
      COMMAND
        "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${target}"
      COMMENT "Copy ${target} resources to rundir"
      VERBATIM
    )
  endif()
endfunction()

# Helper function to add a specific resource to a bundle
function(target_add_resource target resource)
  message(DEBUG "Add resource '${resource}' to target ${target}...")

  set(_rel_dir "")
  get_filename_component(_dest_name "${resource}" NAME)

  file(RELATIVE_PATH _rel_to_data "${CMAKE_SOURCE_DIR}/data" "${resource}")
  if(_rel_to_data AND NOT _rel_to_data MATCHES "^\\.\\.")
    get_filename_component(_rel_dir "${_rel_to_data}" DIRECTORY)
    get_filename_component(_dest_name "${_rel_to_data}" NAME)
  endif()

  if(_rel_dir)
    set(_install_dest "${target}/data/${_rel_dir}")
    set(_rundir_dest "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${target}/${_rel_dir}")
  else()
    set(_install_dest "${target}/data")
    set(_rundir_dest "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${target}")
  endif()

  install(FILES "${resource}" DESTINATION "${_install_dest}" COMPONENT Runtime)

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_rundir_dest}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${resource}" "${_rundir_dest}/${_dest_name}"
    COMMENT "Copy ${target} resource ${resource} to rundir"
    VERBATIM
  )

  source_group("Resources/${_rel_dir}" FILES "${resource}")
endfunction()
