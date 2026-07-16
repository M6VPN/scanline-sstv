# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/src/image/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sstv_image STATIC image.cpp)
add_library(sstv::image ALIAS sstv_image)

target_link_libraries(
	sstv_image
	PUBLIC
		sstv::core
		sstv_project_options
	PRIVATE
		PkgConfig::VIPS_CPP
		sstv_project_warnings
)

target_include_directories(
	sstv_image
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(sstv_image PROPERTIES OUTPUT_NAME sstv_image)
install(TARGETS sstv_image ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
