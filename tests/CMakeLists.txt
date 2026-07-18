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

if(SSTV_BUILD_AUDIO)
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
