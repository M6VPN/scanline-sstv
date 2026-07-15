# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(scanline-sstv-cli main.cpp)
target_link_libraries(
  scanline-sstv-cli
  PRIVATE
    sstv::core
    sstv_project_options
    sstv_project_warnings
)

install(TARGETS scanline-sstv-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

