# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/src/rig/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sstv_rig STATIC ptt.cpp)
add_library(sstv::rig ALIAS sstv_rig)
target_include_directories(
	sstv_rig PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_link_libraries(
	sstv_rig
	PUBLIC sstv_project_options
	PRIVATE Threads::Threads sstv_project_warnings
)
