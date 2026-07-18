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
option(SSTV_BUILD_RIG "Build mock-safe rig coordination and loopback rig providers" ON)
option(SSTV_ENABLE_LIVE_TX "Build the explicitly armed CLI live-transmit path" OFF)
option(SSTV_ENABLE_TX_HARDWARE_TESTS "Build the manual live-TX hardware test harness" OFF)
option(SSTV_ARM_TX_HARDWARE_TESTS "Arm the manual live-TX hardware test harness" OFF)
option(SSTV_ENABLE_AUDIO_HARDWARE_TESTS "Enable explicitly armed audio hardware tests" OFF)
option(SSTV_ARM_AUDIO_HARDWARE_TESTS "Arm the configured audio hardware test run" OFF)
set(SSTV_AUDIO_HARDWARE_BACKEND "" CACHE STRING "Explicit hardware-test backend")
set(SSTV_AUDIO_HARDWARE_PLAYBACK_ID "" CACHE STRING "Explicit hardware-test playback ID")
set(SSTV_AUDIO_HARDWARE_CAPTURE_ID "" CACHE STRING "Explicit hardware-test capture ID")
set(SSTV_AUDIO_HARDWARE_OUTPUT_CHANNEL "" CACHE STRING "Explicit hardware-test output channel")
set(SSTV_AUDIO_HARDWARE_INPUT_CHANNEL "" CACHE STRING "Explicit hardware-test input channel")
set(SSTV_AUDIO_HARDWARE_PLAYBACK_CHANNELS "" CACHE STRING "Hardware-test playback channel count")
set(SSTV_AUDIO_HARDWARE_CAPTURE_CHANNELS "" CACHE STRING "Hardware-test capture channel count")
set(SSTV_TX_HARDWARE_MODE "" CACHE STRING "Manual live-TX mode")
set(SSTV_TX_HARDWARE_INPUT "" CACHE FILEPATH "Manual live-TX source image")
set(SSTV_TX_HARDWARE_BACKEND "" CACHE STRING "Manual live-TX audio backend")
set(SSTV_TX_HARDWARE_PLAYBACK_ID "" CACHE STRING "Manual live-TX playback identity")
set(SSTV_TX_HARDWARE_OUTPUT_CHANNEL "" CACHE STRING "Manual live-TX output channel")
set(SSTV_TX_HARDWARE_PLAYBACK_CHANNELS "" CACHE STRING "Manual live-TX playback channels")
set(SSTV_TX_HARDWARE_GAIN_DBFS "" CACHE STRING "Manual live-TX software gain")
set(SSTV_TX_HARDWARE_PTT_PROVIDER "" CACHE STRING "Manual live-TX PTT provider")
set(SSTV_TX_HARDWARE_PTT_ADDRESS "" CACHE STRING "Manual live-TX literal-loopback address")
set(SSTV_TX_HARDWARE_PTT_PORT "" CACHE STRING "Manual live-TX PTT port")
set(SSTV_TX_HARDWARE_FLRIG_PATH "" CACHE STRING "Manual live-TX flrig path")
option(SSTV_ENABLE_WERROR "Treat compiler warnings as errors" OFF)

if(SSTV_ENABLE_LIVE_TX AND (NOT SSTV_BUILD_IMAGE OR NOT SSTV_BUILD_AUDIO OR NOT SSTV_BUILD_RIG))
	message(FATAL_ERROR
		"SSTV_ENABLE_LIVE_TX requires SSTV_BUILD_IMAGE, SSTV_BUILD_AUDIO, and SSTV_BUILD_RIG."
	)
endif()
if((SSTV_ENABLE_TX_HARDWARE_TESTS OR SSTV_ARM_TX_HARDWARE_TESTS)
		AND NOT SSTV_ENABLE_LIVE_TX)
	message(FATAL_ERROR
		"Live-TX hardware-test gates require SSTV_ENABLE_LIVE_TX=ON."
	)
endif()

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
	if(SSTV_BUILD_RIG)
		add_subdirectory(src/rig)
	endif()
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
endif()

if(SSTV_BUILD_IMAGE OR SSTV_BUILD_AUDIO)
	add_subdirectory(src/app)
endif()

add_subdirectory(apps/cli)

if(SSTV_ENABLE_TX_HARDWARE_TESTS AND SSTV_ARM_TX_HARDWARE_TESTS)
	if(SSTV_TX_HARDWARE_MODE STREQUAL ""
			OR SSTV_TX_HARDWARE_INPUT STREQUAL ""
			OR SSTV_TX_HARDWARE_BACKEND STREQUAL ""
			OR SSTV_TX_HARDWARE_PLAYBACK_ID STREQUAL ""
			OR SSTV_TX_HARDWARE_OUTPUT_CHANNEL STREQUAL ""
			OR SSTV_TX_HARDWARE_PLAYBACK_CHANNELS STREQUAL ""
			OR SSTV_TX_HARDWARE_GAIN_DBFS STREQUAL ""
			OR SSTV_TX_HARDWARE_PTT_PROVIDER STREQUAL ""
			OR SSTV_TX_HARDWARE_PTT_ADDRESS STREQUAL ""
			OR SSTV_TX_HARDWARE_PTT_PORT STREQUAL "")
		message(FATAL_ERROR
			"Armed live-TX hardware testing requires explicit image, mode, audio, "
			"channel, gain, provider, and loopback endpoint cache values."
		)
	endif()
	set(SSTV_TX_HARDWARE_FLRIG_ARGUMENTS)
	if(SSTV_TX_HARDWARE_PTT_PROVIDER STREQUAL "flrig")
		if(SSTV_TX_HARDWARE_FLRIG_PATH STREQUAL "")
			message(FATAL_ERROR "Armed flrig live-TX testing requires an explicit path.")
		endif()
		list(APPEND SSTV_TX_HARDWARE_FLRIG_ARGUMENTS
			--flrig-path "${SSTV_TX_HARDWARE_FLRIG_PATH}")
	endif()
	add_custom_target(scanline-sstv-live-tx-hardware-manual
		COMMAND $<TARGET_FILE:scanline-sstv-cli> transmit-image
			--mode "${SSTV_TX_HARDWARE_MODE}"
			--input "${SSTV_TX_HARDWARE_INPUT}"
			--backend "${SSTV_TX_HARDWARE_BACKEND}"
			--playback-id "${SSTV_TX_HARDWARE_PLAYBACK_ID}"
			--output-channel "${SSTV_TX_HARDWARE_OUTPUT_CHANNEL}"
			--playback-channels "${SSTV_TX_HARDWARE_PLAYBACK_CHANNELS}"
			--ptt-provider "${SSTV_TX_HARDWARE_PTT_PROVIDER}"
			--ptt-address "${SSTV_TX_HARDWARE_PTT_ADDRESS}"
			--ptt-port "${SSTV_TX_HARDWARE_PTT_PORT}"
			${SSTV_TX_HARDWARE_FLRIG_ARGUMENTS}
			--pre-key-ms 250 --post-audio-ms 250
			--gain-dbfs "${SSTV_TX_HARDWARE_GAIN_DBFS}"
			--arm-real-audio --arm-automatic-ptt --arm-live-tx
		DEPENDS scanline-sstv-cli
		USES_TERMINAL
		COMMENT "Manual armed live-TX hardware test; requires interactive confirmation"
	)
endif()

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
