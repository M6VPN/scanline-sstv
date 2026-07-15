# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(scanline-sstv-core-smoke core_smoke.cpp)
target_link_libraries(
  scanline-sstv-core-smoke
  PRIVATE
    sstv::core
    sstv_project_options
    sstv_project_warnings
)

add_test(NAME scanline-sstv-core-smoke COMMAND scanline-sstv-core-smoke)

