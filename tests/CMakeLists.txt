# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/CMakeLists.txt
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

if(NOT SSTV_ENABLE_LIVE_TX)
	add_test(
		NAME scanline-sstv-cli-live-tx-disabled
		COMMAND ${CMAKE_COMMAND}
			-DCLI=$<TARGET_FILE:scanline-sstv-cli>
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cli_live_tx_disabled_test.cmake
	)
endif()

if(SSTV_ENABLE_LIVE_TX)
	add_executable(scanline-sstv-m2i-live-service-tests
		m2i_live_service_tests.cpp)
	target_link_libraries(scanline-sstv-m2i-live-service-tests
		PRIVATE sstv::app sstv_project_options sstv_project_warnings)
	add_test(NAME scanline-sstv-m2i-live-service-tests
		COMMAND scanline-sstv-m2i-live-service-tests)
	set_tests_properties(scanline-sstv-m2i-live-service-tests PROPERTIES
		LABELS "live-tx-mock;audio-concurrency" TIMEOUT 30)
	add_executable(scanline-sstv-m2h-live-transmit-tests
		m2h_live_transmit_tests.cpp
		${PROJECT_SOURCE_DIR}/apps/cli/live_tx_commands.cpp)
	target_include_directories(scanline-sstv-m2h-live-transmit-tests
		PRIVATE ${PROJECT_SOURCE_DIR}/apps/cli)
	target_compile_definitions(scanline-sstv-m2h-live-transmit-tests
		PRIVATE SSTV_IMAGE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/image")
	target_link_libraries(scanline-sstv-m2h-live-transmit-tests
		PRIVATE sstv::app sstv::rig sstv_project_options sstv_project_warnings)
	add_test(NAME scanline-sstv-m2h-live-transmit-tests
		COMMAND scanline-sstv-m2h-live-transmit-tests)
	set_tests_properties(scanline-sstv-m2h-live-transmit-tests PROPERTIES
		LABELS "live-tx-mock;audio-concurrency" TIMEOUT 30)
	add_executable(scanline-sstv-m2h-signal-tests m2h_signal_tests.cpp)
	target_include_directories(scanline-sstv-m2h-signal-tests
		PRIVATE ${PROJECT_SOURCE_DIR}/apps/cli)
	target_link_libraries(scanline-sstv-m2h-signal-tests
		PRIVATE sstv::app sstv_project_options sstv_project_warnings)
	add_test(NAME scanline-sstv-m2h-signal-tests
		COMMAND scanline-sstv-m2h-signal-tests)
	set_tests_properties(scanline-sstv-m2h-signal-tests PROPERTIES
		LABELS "live-tx-mock;audio-concurrency" TIMEOUT 5)
	add_test(NAME scanline-sstv-cli-live-tx-noninteractive
		COMMAND ${CMAKE_COMMAND}
			-DCLI=$<TARGET_FILE:scanline-sstv-cli>
			-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/image/marker.png
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cli_live_tx_noninteractive_test.cmake)
	set_tests_properties(scanline-sstv-cli-live-tx-noninteractive PROPERTIES
		LABELS "live-tx-mock" TIMEOUT 10)
endif()

if(SSTV_BUILD_AUDIO AND SSTV_BUILD_RIG)
	add_executable(scanline-sstv-m2g-rendered-transmit-tests
		m2g_rendered_transmit_tests.cpp)
	target_link_libraries(
		scanline-sstv-m2g-rendered-transmit-tests
		PRIVATE sstv::app sstv_project_options sstv_project_warnings
	)
	add_test(NAME scanline-sstv-m2g-rendered-transmit-tests
		COMMAND scanline-sstv-m2g-rendered-transmit-tests)
	set_tests_properties(
		scanline-sstv-m2g-rendered-transmit-tests
		PROPERTIES LABELS "audio-concurrency" TIMEOUT 60
	)
	add_executable(scanline-sstv-m2g-null-transmit-test
		m2g_null_transmit_test.cpp)
	target_link_libraries(
		scanline-sstv-m2g-null-transmit-test
		PRIVATE sstv::app sstv_project_options sstv_project_warnings
	)
	add_test(NAME scanline-sstv-m2g-null-transmit-test
		COMMAND scanline-sstv-m2g-null-transmit-test)
	set_tests_properties(
		scanline-sstv-m2g-null-transmit-test
		PROPERTIES LABELS "audio-null" TIMEOUT 8
	)

	add_executable(scanline-sstv-m2e-flrig-tests m2e_flrig_tests.cpp)
	target_include_directories(
		scanline-sstv-m2e-flrig-tests
		PRIVATE ${PROJECT_SOURCE_DIR}/src/rig
	)
	target_link_libraries(
		scanline-sstv-m2e-flrig-tests
		PRIVATE sstv::app sstv_project_options sstv_project_warnings
	)
	add_test(NAME scanline-sstv-m2e-flrig-tests COMMAND scanline-sstv-m2e-flrig-tests)
	set_tests_properties(
		scanline-sstv-m2e-flrig-tests
		PROPERTIES LABELS "rig-loopback;audio-concurrency" TIMEOUT 20
	)

	add_executable(scanline-sstv-m2f-rigctld-tests m2f_rigctld_tests.cpp)
	target_include_directories(
		scanline-sstv-m2f-rigctld-tests
		PRIVATE ${PROJECT_SOURCE_DIR}/src/rig
	)
	target_link_libraries(
		scanline-sstv-m2f-rigctld-tests
		PRIVATE sstv::app sstv_project_options sstv_project_warnings
	)
	add_test(NAME scanline-sstv-m2f-rigctld-tests COMMAND scanline-sstv-m2f-rigctld-tests)
	set_tests_properties(
		scanline-sstv-m2f-rigctld-tests
		PROPERTIES LABELS "rig-loopback;audio-concurrency" TIMEOUT 20
	)

	add_executable(scanline-sstv-m2d-transmit-tests m2d_transmit_tests.cpp)
	target_link_libraries(
		scanline-sstv-m2d-transmit-tests
		PRIVATE
			sstv::app
			sstv_project_options
			sstv_project_warnings
	)
	add_test(
		NAME scanline-sstv-m2d-transmit-tests
		COMMAND scanline-sstv-m2d-transmit-tests
	)
	set_tests_properties(
		scanline-sstv-m2d-transmit-tests
		PROPERTIES LABELS "audio-concurrency" TIMEOUT 20
	)

endif()

if(SSTV_BUILD_AUDIO)

	add_executable(
		scanline-sstv-audio-tests
		audio_tests.cpp
		${PROJECT_SOURCE_DIR}/apps/cli/audio_text.cpp
	)
	target_include_directories(
		scanline-sstv-audio-tests
		PRIVATE
			${PROJECT_SOURCE_DIR}/src/audio
			${PROJECT_SOURCE_DIR}/third_party/miniaudio
			${PROJECT_SOURCE_DIR}/apps/cli
	)
	target_link_libraries(
		scanline-sstv-audio-tests
		PRIVATE
			sstv::audio
			sstv_project_options
			sstv_project_warnings
	)
	add_test(NAME scanline-sstv-audio-tests COMMAND scanline-sstv-audio-tests)
	set_tests_properties(scanline-sstv-audio-tests PROPERTIES TIMEOUT 10)

	add_executable(scanline-sstv-audio-diagnostics-tests audio_diagnostics_tests.cpp)
	target_link_libraries(
		scanline-sstv-audio-diagnostics-tests
		PRIVATE
			sstv::audio
			sstv_project_options
			sstv_project_warnings
	)
	add_test(
		NAME scanline-sstv-audio-diagnostics-tests
		COMMAND scanline-sstv-audio-diagnostics-tests
	)
	set_tests_properties(
		scanline-sstv-audio-diagnostics-tests
		PROPERTIES LABELS "audio-concurrency" TIMEOUT 20
	)

	add_executable(scanline-sstv-audio-stream-tests audio_stream_tests.cpp)
	target_link_libraries(
		scanline-sstv-audio-stream-tests
		PRIVATE
			sstv::audio
			sstv_project_options
			sstv_project_warnings
	)
	add_test(NAME scanline-sstv-audio-stream-tests COMMAND scanline-sstv-audio-stream-tests)
	set_tests_properties(
		scanline-sstv-audio-stream-tests
		PROPERTIES
			LABELS "audio-concurrency"
			TIMEOUT 20
	)

	add_executable(scanline-sstv-audio-null-stream-test audio_null_stream_test.cpp)
	target_link_libraries(
		scanline-sstv-audio-null-stream-test
		PRIVATE
			sstv::audio
			sstv_project_options
			sstv_project_warnings
	)
	add_test(
		NAME scanline-sstv-audio-null-stream-test
		COMMAND scanline-sstv-audio-null-stream-test
	)
	set_tests_properties(
		scanline-sstv-audio-null-stream-test
		PROPERTIES
			LABELS "audio-null"
			TIMEOUT 5
	)

	add_executable(scanline-sstv-audio-host-smoke audio_host_smoke.cpp)
	target_link_libraries(
		scanline-sstv-audio-host-smoke
		PRIVATE
			sstv::audio
			sstv_project_options
			sstv_project_warnings
	)
	add_test(NAME scanline-sstv-audio-host-smoke COMMAND scanline-sstv-audio-host-smoke)
	set_tests_properties(scanline-sstv-audio-host-smoke PROPERTIES TIMEOUT 15)

	add_test(
		NAME scanline-sstv-cli-audio-test
		COMMAND
			${CMAKE_COMMAND}
			-DCLI=$<TARGET_FILE:scanline-sstv-cli>
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cli_audio_test.cmake
	)
	set_tests_properties(scanline-sstv-cli-audio-test PROPERTIES TIMEOUT 10)

	if(SSTV_ENABLE_AUDIO_HARDWARE_TESTS AND SSTV_ARM_AUDIO_HARDWARE_TESTS)
		if(SSTV_AUDIO_HARDWARE_BACKEND STREQUAL ""
				OR SSTV_AUDIO_HARDWARE_PLAYBACK_ID STREQUAL ""
				OR SSTV_AUDIO_HARDWARE_CAPTURE_ID STREQUAL ""
				OR SSTV_AUDIO_HARDWARE_OUTPUT_CHANNEL STREQUAL ""
				OR SSTV_AUDIO_HARDWARE_INPUT_CHANNEL STREQUAL ""
				OR SSTV_AUDIO_HARDWARE_PLAYBACK_CHANNELS STREQUAL ""
				OR SSTV_AUDIO_HARDWARE_CAPTURE_CHANNELS STREQUAL "")
			message(FATAL_ERROR
				"Armed audio hardware tests require explicit backend, playback ID, "
				"capture ID, output channel, and input channel cache values."
			)
		endif()
		add_test(
			NAME scanline-sstv-audio-hardware-loopback
			COMMAND
				$<TARGET_FILE:scanline-sstv-cli> audio-loopback
				--backend ${SSTV_AUDIO_HARDWARE_BACKEND}
				--playback-id ${SSTV_AUDIO_HARDWARE_PLAYBACK_ID}
				--capture-id ${SSTV_AUDIO_HARDWARE_CAPTURE_ID}
				--output-channel ${SSTV_AUDIO_HARDWARE_OUTPUT_CHANNEL}
				--input-channel ${SSTV_AUDIO_HARDWARE_INPUT_CHANNEL}
				--playback-channels ${SSTV_AUDIO_HARDWARE_PLAYBACK_CHANNELS}
				--capture-channels ${SSTV_AUDIO_HARDWARE_CAPTURE_CHANNELS}
				--level-dbfs -60 --arm-real-audio
		)
		set_tests_properties(
			scanline-sstv-audio-hardware-loopback
			PROPERTIES LABELS "audio-hardware" TIMEOUT 10
		)
	endif()
endif()

add_executable(scanline-sstv-m1a-tests m1a_tests.cpp)
target_link_libraries(
	scanline-sstv-m1a-tests
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
)

add_test(NAME scanline-sstv-m1a-tests COMMAND scanline-sstv-m1a-tests)
set_tests_properties(scanline-sstv-m1a-tests PROPERTIES TIMEOUT 20)

add_executable(scanline-sstv-m1c-tests m1c_tests.cpp)
target_link_libraries(
	scanline-sstv-m1c-tests
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
)

add_test(NAME scanline-sstv-m1c-tests COMMAND scanline-sstv-m1c-tests)
set_tests_properties(scanline-sstv-m1c-tests PROPERTIES TIMEOUT 30)

add_executable(scanline-sstv-m1d-tests m1d_tests.cpp)
target_link_libraries(
	scanline-sstv-m1d-tests
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
)

add_test(NAME scanline-sstv-m1d-tests COMMAND scanline-sstv-m1d-tests)
set_tests_properties(scanline-sstv-m1d-tests PROPERTIES TIMEOUT 30)

add_executable(scanline-sstv-m1e-tests m1e_tests.cpp)
target_link_libraries(
	scanline-sstv-m1e-tests
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
)

add_test(NAME scanline-sstv-m1e-tests COMMAND scanline-sstv-m1e-tests)
set_tests_properties(scanline-sstv-m1e-tests PROPERTIES TIMEOUT 45)

add_executable(scanline-sstv-m1f-tests m1f_tests.cpp)
target_link_libraries(
	scanline-sstv-m1f-tests
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
)

add_test(NAME scanline-sstv-m1f-tests COMMAND scanline-sstv-m1f-tests)
set_tests_properties(scanline-sstv-m1f-tests PROPERTIES TIMEOUT 90)

add_test(
	NAME scanline-sstv-cli-encode-test
	COMMAND
		${CMAKE_COMMAND}
		-DCLI=$<TARGET_FILE:scanline-sstv-cli>
		-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-encode-test
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cli_encode_test.cmake
)

if(SSTV_BUILD_IMAGE)
	add_executable(scanline-sstv-m1g-app-tests m1g_app_tests.cpp)
	target_link_libraries(
		scanline-sstv-m1g-app-tests
		PRIVATE
			sstv::app
			sstv_project_options
			sstv_project_warnings
	)
	target_compile_definitions(
		scanline-sstv-m1g-app-tests
		PRIVATE SSTV_IMAGE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/image"
	)
	add_test(NAME scanline-sstv-m1g-app-tests COMMAND scanline-sstv-m1g-app-tests)
	set_tests_properties(scanline-sstv-m1g-app-tests PROPERTIES TIMEOUT 120)

	add_executable(scanline-sstv-image-tests image_tests.cpp)
	target_link_libraries(
		scanline-sstv-image-tests
		PRIVATE
			sstv::image
			sstv_project_options
			sstv_project_warnings
	)
	target_compile_definitions(
		scanline-sstv-image-tests
		PRIVATE SSTV_IMAGE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/image"
	)
	add_test(NAME scanline-sstv-image-tests COMMAND scanline-sstv-image-tests)
	set_tests_properties(scanline-sstv-image-tests PROPERTIES TIMEOUT 30)
	add_test(
		NAME scanline-sstv-cli-image-test
		COMMAND
			${CMAKE_COMMAND}
			-DCLI=$<TARGET_FILE:scanline-sstv-cli>
			-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/image/marker.png
			-DJPEG=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/image/marker.jpg
			-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-image-test
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cli_image_test.cmake
	)
	set_tests_properties(scanline-sstv-cli-image-test PROPERTIES TIMEOUT 90)
endif()
set_tests_properties(scanline-sstv-cli-encode-test PROPERTIES TIMEOUT 30)
