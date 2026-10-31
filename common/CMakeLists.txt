file(
  GLOB_RECURSE COMMON_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.m"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
)
file(
  GLOB_RECURSE COMMON_HEADERS
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp"
)

add_library(toolkit_common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
target_include_directories(toolkit_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_link_libraries(toolkit_common PUBLIC openssl::openssl)
target_compile_features(toolkit_common PUBLIC cxx_std_20)
set_target_properties(toolkit_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
