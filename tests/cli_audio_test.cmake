# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/cli_audio_test.cmake
# SPDX-License-Identifier: GPL-3.0-or-later

execute_process(
	COMMAND "${CLI}" list-audio --backend
	RESULT_VARIABLE missing_result
	OUTPUT_VARIABLE missing_output
	ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 2 OR NOT missing_error MATCHES "missing value")
	message(FATAL_ERROR "list-audio missing-value handling failed")
endif()

execute_process(
	COMMAND "${CLI}" list-audio --backend invalid
	RESULT_VARIABLE invalid_result
	OUTPUT_VARIABLE invalid_output
	ERROR_VARIABLE invalid_error
)
if(NOT invalid_result EQUAL 2 OR NOT invalid_error MATCHES "unknown audio backend")
	message(FATAL_ERROR "list-audio unknown-backend handling failed")
endif()

execute_process(
	COMMAND "${CLI}" list-audio --include-null --include-null
	RESULT_VARIABLE duplicate_result
	OUTPUT_VARIABLE duplicate_output
	ERROR_VARIABLE duplicate_error
)
if(NOT duplicate_result EQUAL 2 OR NOT duplicate_error MATCHES "duplicate option")
	message(FATAL_ERROR "list-audio duplicate-option handling failed")
endif()

execute_process(
	COMMAND "${CLI}" list-audio --backend alsa
	RESULT_VARIABLE filter_result
	OUTPUT_VARIABLE filter_output
	ERROR_VARIABLE filter_error
)
if(NOT filter_result EQUAL 0 AND NOT filter_result EQUAL 1)
	message(FATAL_ERROR "list-audio backend filter returned an invalid exit class")
endif()
if(NOT filter_output MATCHES "Backend: ALSA" OR filter_output MATCHES "Backend: JACK"
		OR filter_output MATCHES "Backend: PulseAudio")
	message(FATAL_ERROR "list-audio backend filtering failed")
endif()

execute_process(
	COMMAND "${CLI}" list-audio --backend alsa --include-null
	RESULT_VARIABLE null_result
	OUTPUT_VARIABLE null_output
	ERROR_VARIABLE null_error
)
if(NOT null_result EQUAL 0 AND NOT null_result EQUAL 1)
	message(FATAL_ERROR "list-audio null diagnostic returned an invalid exit class")
endif()
if(NOT null_output MATCHES "Backend: Null diagnostic")
	message(FATAL_ERROR "list-audio did not include the requested null diagnostic")
endif()
