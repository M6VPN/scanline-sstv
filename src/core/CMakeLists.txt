# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sstv_core STATIC mode_registry.cpp)
add_library(sstv::core ALIAS sstv_core)

target_link_libraries(
  sstv_core
  PUBLIC sstv_project_options
  PRIVATE sstv_project_warnings
)

target_include_directories(
  sstv_core
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(sstv_core PROPERTIES OUTPUT_NAME sstv_core)

install(
  TARGETS sstv_core
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

