# SPDX-License-Identifier: GPL-3.0-or-later

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(scanline-sstv-gui main.cpp)

qt_add_qml_module(
  scanline-sstv-gui
  URI org.m6vpn.ScanlineSSTV
  VERSION 1.0
  QML_FILES Main.qml
)

target_link_libraries(
  scanline-sstv-gui
  PRIVATE
    sstv::core
    sstv_project_options
    sstv_project_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS scanline-sstv-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

