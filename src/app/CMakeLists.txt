# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/src/app/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sstv_app STATIC offline_tx_editor.cpp)
add_library(sstv::app ALIAS sstv_app)
target_link_libraries(
	sstv_app
	PUBLIC sstv::image sstv::core sstv_project_options
	PRIVATE sstv_project_warnings
)
target_include_directories(
	sstv_app PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
