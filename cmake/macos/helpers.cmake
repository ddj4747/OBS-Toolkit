# CMake macOS helper functions module

include_guard(GLOBAL)

include(helpers_common)

set(_MACOS_HELPERS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# set_target_properties_obs: Set target properties for use in obs-studio
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
  set_target_properties(
    ${target}
    PROPERTIES
      BUNDLE TRUE
      BUNDLE_EXTENSION plugin
      XCODE_ATTRIBUTE_PRODUCT_NAME ${target}
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${MACOS_BUNDLEID}
      XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION ${PLUGIN_BUILD_NUMBER}
      XCODE_ATTRIBUTE_MARKETING_VERSION ${PLUGIN_VERSION}
      XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
      XCODE_ATTRIBUTE_INFOPLIST_FILE ""
      XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName ${target}
      XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright "(c) ${CURRENT_YEAR} ${PLUGIN_AUTHOR}"
      XCODE_ATTRIBUTE_INSTALL_PATH "$(USER_LIBRARY_DIR)/Application Support/obs-studio/plugins"
  )

  if(EXISTS "${_MACOS_HELPERS_DIR}/entitlements.plist")
    set_target_properties(
      ${target}
      PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${_MACOS_HELPERS_DIR}/entitlements.plist"
    )
  endif()

  target_install_resources(${target})

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_directory "$<TARGET_BUNDLE_DIR:${target}>"
      "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/$<TARGET_BUNDLE_DIR_NAME:${target}>"
    COMMENT "Copy ${target} to rundir"
    VERBATIM
  )

  get_target_property(target_sources ${target} SOURCES)
  set(target_ui_files ${target_sources})
  list(FILTER target_ui_files INCLUDE REGEX ".+\\.(ui|qrc)")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "UI Files" FILES ${target_ui_files})

  install(TARGETS ${target} LIBRARY DESTINATION .)
  install(FILES "$<TARGET_BUNDLE_DIR:${target}>.dsym" CONFIGURATIONS Release DESTINATION . OPTIONAL)

  configure_file("${_MACOS_HELPERS_DIR}/resources/distribution.in" "${CMAKE_CURRENT_BINARY_DIR}/distribution" @ONLY)
  configure_file(
    "${_MACOS_HELPERS_DIR}/resources/create-package.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/create-package.cmake"
    @ONLY
  )
  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/create-package.cmake")
endfunction()

# target_install_resources: Helper function to add resources into bundle
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
      set_property(SOURCE "${data_file}" PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${relative_path}")
      source_group("Resources/${relative_path}" FILES "${data_file}")
    endforeach()
  endif()
endfunction()

# Point single-architecture Conan static libraries at the universal archives created by configure.py.
function(use_macos_universal_conan_libraries)
  set(universal_lib_dir "${CMAKE_SOURCE_DIR}/.deps/conan-macos-universal/lib")
  if(NOT EXISTS "${universal_lib_dir}/libssl.a" OR NOT EXISTS "${universal_lib_dir}/libcrypto.a")
    message(
      FATAL_ERROR
      "Universal OpenSSL archives are missing in ${universal_lib_dir}. "
      "Run configure.py before configuring the project."
    )
  endif()

  set(
    location_properties
    IMPORTED_LOCATION
    IMPORTED_LOCATION_RELEASE
    IMPORTED_LOCATION_RELWITHDEBINFO
    IMPORTED_LOCATION_MINSIZEREL
  )
  set(retargeted_ssl FALSE)
  set(retargeted_crypto FALSE)

  get_property(imported_targets DIRECTORY PROPERTY IMPORTED_TARGETS)
  foreach(imported_target IN LISTS imported_targets)
    get_target_property(aliased "${imported_target}" ALIASED_TARGET)
    if(aliased)
      continue()
    endif()

    get_target_property(target_type "${imported_target}" TYPE)
    if(NOT target_type MATCHES "^(STATIC_LIBRARY|UNKNOWN_LIBRARY)$")
      continue()
    endif()

    foreach(location_property IN LISTS location_properties)
      get_target_property(location "${imported_target}" "${location_property}")
      if(NOT location OR location MATCHES "-NOTFOUND$")
        continue()
      endif()

      cmake_path(GET location FILENAME filename)
      set(universal_library "${universal_lib_dir}/${filename}")
      if(NOT EXISTS "${universal_library}")
        continue()
      endif()

      set_target_properties("${imported_target}" PROPERTIES "${location_property}" "${universal_library}")
      message(STATUS "Using universal ${filename} for ${imported_target} (${location_property})")
      if(filename STREQUAL "libssl.a")
        set(retargeted_ssl TRUE)
      elseif(filename STREQUAL "libcrypto.a")
        set(retargeted_crypto TRUE)
      endif()
    endforeach()
  endforeach()

  if(NOT retargeted_ssl OR NOT retargeted_crypto)
    message(
      FATAL_ERROR
      "OpenSSL import targets still point at single-architecture libraries. "
      "Universal libssl.a and libcrypto.a were not applied."
    )
  endif()
endfunction()

# target_add_resource: Helper function to add a specific resource to a bundle
function(target_add_resource target resource)
  message(DEBUG "Add resource ${resource} to target ${target}...")

  set(_rel_dir "")
  file(RELATIVE_PATH _rel_to_data "${CMAKE_SOURCE_DIR}/data" "${resource}")
  if(_rel_to_data AND NOT _rel_to_data MATCHES "^\\.\\.")
    get_filename_component(_rel_dir "${_rel_to_data}" DIRECTORY)
  endif()

  target_sources(${target} PRIVATE "${resource}")
  if(_rel_dir)
    set_property(SOURCE "${resource}" PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${_rel_dir}")
    source_group("Resources/${_rel_dir}" FILES "${resource}")
  else()
    set_property(SOURCE "${resource}" PROPERTY MACOSX_PACKAGE_LOCATION Resources)
    source_group("Resources" FILES "${resource}")
  endif()
endfunction()
