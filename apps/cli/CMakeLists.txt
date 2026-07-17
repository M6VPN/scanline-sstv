# SPDX-License-Identifier: GPL-3.0-or-later

set(SSTV_CLI_SOURCES main.cpp wav_commands.cpp)
if(SSTV_BUILD_AUDIO)
	list(APPEND SSTV_CLI_SOURCES audio_commands.cpp audio_text.cpp)
else()
	list(APPEND SSTV_CLI_SOURCES audio_commands_stub.cpp)
endif()
if(SSTV_BUILD_IMAGE)
	list(APPEND SSTV_CLI_SOURCES image_commands.cpp)
else()
	list(APPEND SSTV_CLI_SOURCES image_commands_stub.cpp)
endif()

add_executable(scanline-sstv-cli ${SSTV_CLI_SOURCES})
target_link_libraries(
	scanline-sstv-cli
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
)

if(SSTV_BUILD_IMAGE)
	target_link_libraries(scanline-sstv-cli PRIVATE sstv::image)
endif()
if(SSTV_BUILD_AUDIO)
	target_link_libraries(scanline-sstv-cli PRIVATE sstv::audio)
endif()

install(TARGETS scanline-sstv-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
