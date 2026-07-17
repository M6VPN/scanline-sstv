# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/src/core/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(
	sstv_core STATIC
	luma_chroma.cpp
	martin_m1.cpp
	mode_registry.cpp
	offline_tx.cpp
	paired_luma_chroma.cpp
	pd_120.cpp
	rgb8_frame.cpp
	robot_36.cpp
	scottie_s1.cpp
	sequential_rgb.cpp
	timing.cpp
	tone.cpp
	tone_renderer.cpp
	vis.cpp
	wav_writer.cpp
)
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
