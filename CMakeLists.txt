# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.24)

project(
	scanline_sstv
	VERSION 0.1.0
	DESCRIPTION "Scanline SSTV transceiver for Linux and BSD"
	LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

option(SSTV_BUILD_GUI "Build the Qt Quick graphical application when Qt is available" ON)
option(SSTV_BUILD_IMAGE "Build libvips raster image preparation support" ON)
option(SSTV_BUILD_AUDIO "Build audio discovery, stream, and diagnostic support" ON)
option(SSTV_ENABLE_AUDIO_HARDWARE_TESTS "Enable explicitly armed audio hardware tests" OFF)
option(SSTV_ARM_AUDIO_HARDWARE_TESTS "Arm the configured audio hardware test run" OFF)
set(SSTV_AUDIO_HARDWARE_BACKEND "" CACHE STRING "Explicit hardware-test backend")
set(SSTV_AUDIO_HARDWARE_PLAYBACK_ID "" CACHE STRING "Explicit hardware-test playback ID")
set(SSTV_AUDIO_HARDWARE_CAPTURE_ID "" CACHE STRING "Explicit hardware-test capture ID")
set(SSTV_AUDIO_HARDWARE_OUTPUT_CHANNEL "" CACHE STRING "Explicit hardware-test output channel")
set(SSTV_AUDIO_HARDWARE_INPUT_CHANNEL "" CACHE STRING "Explicit hardware-test input channel")
set(SSTV_AUDIO_HARDWARE_PLAYBACK_CHANNELS "" CACHE STRING "Hardware-test playback channel count")
set(SSTV_AUDIO_HARDWARE_CAPTURE_CHANNELS "" CACHE STRING "Hardware-test capture channel count")
option(SSTV_ENABLE_WERROR "Treat compiler warnings as errors" OFF)

add_library(sstv_project_options INTERFACE)
target_compile_features(sstv_project_options INTERFACE cxx_std_20)

add_library(sstv_project_warnings INTERFACE)
target_compile_options(
  sstv_project_warnings
  INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wconversion>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wsign-conversion>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Wshadow>
)

if(SSTV_ENABLE_WERROR)
  target_compile_options(
    sstv_project_warnings
    INTERFACE
      $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Werror>
  )
endif()

add_subdirectory(src/core)

if(SSTV_BUILD_AUDIO)
	find_package(Threads REQUIRED)
	add_subdirectory(src/audio)
endif()

if(SSTV_BUILD_IMAGE)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(VIPS_CPP QUIET IMPORTED_TARGET vips-cpp>=8.15)
	if(NOT VIPS_CPP_FOUND)
		message(FATAL_ERROR
			"SSTV_BUILD_IMAGE is ON, but vips-cpp 8.15+ was not found. "
			"Install the system libvips development package or configure "
			"with -DSSTV_BUILD_IMAGE=OFF."
		)
	endif()
	add_subdirectory(src/image)
	add_subdirectory(src/app)
endif()

add_subdirectory(apps/cli)

if(SSTV_BUILD_GUI)
	if(NOT SSTV_BUILD_IMAGE)
		message(FATAL_ERROR
			"SSTV_BUILD_GUI is ON, but the offline editor requires SSTV_BUILD_IMAGE. "
			"Enable image support or configure with -DSSTV_BUILD_GUI=OFF."
		)
	endif()
	find_package(Qt6 6.5 QUIET COMPONENTS Concurrent Core Gui Qml Quick QuickControls2)
	if(Qt6_FOUND)
		add_subdirectory(apps/gui)
	else()
		message(WARNING "Qt 6.5+ was not found; scanline-sstv-gui will not be built")
	endif()
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
