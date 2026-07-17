# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/src/audio/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(
	sstv_audio
	audio_discovery.cpp
	audio_diagnostics.cpp
	audio_stream.cpp
	sample_ring.cpp
	miniaudio_adapter.cpp
	miniaudio_stream_adapter.cpp
	miniaudio_implementation.cpp
)
set_source_files_properties(
	miniaudio_implementation.cpp
	PROPERTIES COMPILE_OPTIONS "-w"
)
add_library(sstv::audio ALIAS sstv_audio)

target_include_directories(
	sstv_audio
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
	PRIVATE
		${PROJECT_SOURCE_DIR}/third_party/miniaudio
)
target_link_libraries(
	sstv_audio
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
		sstv_project_options
		sstv_project_warnings
)
target_compile_definitions(
	sstv_audio
	PRIVATE
		MA_ENABLE_ONLY_SPECIFIC_BACKENDS
		MA_ENABLE_ALSA
		MA_ENABLE_PULSEAUDIO
		MA_ENABLE_JACK
		MA_ENABLE_SNDIO
		MA_ENABLE_AUDIO4
		MA_ENABLE_OSS
		MA_ENABLE_NULL
		MA_NO_DECODING
		MA_NO_ENCODING
		MA_NO_WAV
		MA_NO_FLAC
		MA_NO_MP3
		MA_NO_RESOURCE_MANAGER
		MA_NO_NODE_GRAPH
		MA_NO_ENGINE
		MA_NO_GENERATION
)
