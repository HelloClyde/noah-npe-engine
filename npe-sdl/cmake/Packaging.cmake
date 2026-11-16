# A production installation has the same layout as the build directory:
# the executable and its native font assets share one base directory.
if(APPLE)
  install(TARGETS npe-sdl BUNDLE DESTINATION .)
else()
  install(TARGETS npe-sdl RUNTIME DESTINATION .)
endif()
install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/gbk12-native.bin"
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/gbk16-native.bin"
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/FONT-PROVENANCE.txt"
  DESTINATION assets)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/PORTABLE.md"
  DESTINATION . RENAME README.md)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL2-LICENSE.txt"
  DESTINATION licenses)

option(NPE_BUNDLE_RUNTIME "Install non-system Windows DLL dependencies" ${WIN32})
set(NPE_RUNTIME_DIRECTORIES "" CACHE STRING
  "Additional directories for Windows runtime DLL dependency lookup")
if(WIN32 AND NPE_BUNDLE_RUNTIME)
  get_filename_component(npe_compiler_directory "${CMAKE_C_COMPILER}" DIRECTORY)
  set(npe_runtime_directories "${npe_compiler_directory};${NPE_RUNTIME_DIRECTORIES}")
  if(TARGET SDL2::SDL2)
    get_target_property(npe_sdl_target_type SDL2::SDL2 TYPE)
    if(NOT npe_sdl_target_type STREQUAL "INTERFACE_LIBRARY")
      list(APPEND npe_runtime_directories "$<TARGET_FILE_DIR:SDL2::SDL2>")
    endif()
  else()
    # pkg-config commonly returns <prefix>/lib, while DLLs live in bin.
    foreach(npe_sdl_directory IN LISTS SDL2_LIBRARY_DIRS)
      list(APPEND npe_runtime_directories "${npe_sdl_directory}"
        "${npe_sdl_directory}/../bin")
    endforeach()
  endif()
  file(READ "${CMAKE_CURRENT_LIST_DIR}/InstallRuntime.cmake.in" npe_runtime_code)
  string(CONFIGURE "${npe_runtime_code}" npe_runtime_code @ONLY)
  install(CODE "${npe_runtime_code}")
endif()

string(TOLOWER "${CMAKE_SYSTEM_NAME}" npe_package_system)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" npe_package_architecture)
if(npe_package_architecture MATCHES "^(amd64|x86_64)$")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(npe_package_architecture x64)
  else()
    set(npe_package_architecture x86)
  endif()
endif()
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME noah-npe-engine)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "noah-npe-engine-${PROJECT_VERSION}-${npe_package_system}-${npe_package_architecture}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Standalone C11 and SDL2 NPE runtime")
include(CPack)
