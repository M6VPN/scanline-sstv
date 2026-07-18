# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/apps/cli/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

set(SSTV_CLI_SOURCES main.cpp wav_commands.cpp)
if(SSTV_BUILD_AUDIO)
	list(APPEND SSTV_CLI_SOURCES
		audio_commands.cpp
		audio_diagnostic_commands.cpp
		audio_text.cpp
	)
else()
	list(APPEND SSTV_CLI_SOURCES audio_commands_stub.cpp)
endif()
if(SSTV_BUILD_IMAGE)
	list(APPEND SSTV_CLI_SOURCES image_commands.cpp)
else()
	list(APPEND SSTV_CLI_SOURCES image_commands_stub.cpp)
endif()
if(SSTV_ENABLE_LIVE_TX)
	list(APPEND SSTV_CLI_SOURCES live_tx_commands.cpp)
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
if(SSTV_ENABLE_LIVE_TX)
	target_compile_definitions(scanline-sstv-cli PRIVATE SSTV_ENABLE_LIVE_TX=1)
	target_link_libraries(scanline-sstv-cli PRIVATE sstv::app sstv::rig)
endif()

install(TARGETS scanline-sstv-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
