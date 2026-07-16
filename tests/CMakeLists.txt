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

add_test(
	NAME scanline-sstv-cli-encode-test
	COMMAND
		${CMAKE_COMMAND}
		-DCLI=$<TARGET_FILE:scanline-sstv-cli>
		-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-encode-test
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cli_encode_test.cmake
)

if(SSTV_BUILD_IMAGE)
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
	set_tests_properties(scanline-sstv-cli-image-test PROPERTIES TIMEOUT 45)
endif()
set_tests_properties(scanline-sstv-cli-encode-test PROPERTIES TIMEOUT 30)
