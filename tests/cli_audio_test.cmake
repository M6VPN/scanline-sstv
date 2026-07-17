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

execute_process(
	COMMAND "${CLI}" audio-output-test --backend alsa --playback-id 00 --channel 0
	RESULT_VARIABLE unarmed_output_result
	OUTPUT_VARIABLE unarmed_output
	ERROR_VARIABLE unarmed_error
)
if(NOT unarmed_output_result EQUAL 2
		OR NOT unarmed_error MATCHES "--arm-real-audio is required")
	message(FATAL_ERROR "unarmed output did not fail at the CLI safety gate")
endif()

execute_process(
	COMMAND "${CLI}" audio-loopback --backend alsa --playback-id 00
		--capture-id 00 --output-channel 0 --input-channel 0
	RESULT_VARIABLE unarmed_loopback_result
	OUTPUT_VARIABLE unarmed_loopback_output
	ERROR_VARIABLE unarmed_loopback_error
)
if(NOT unarmed_loopback_result EQUAL 2
		OR NOT unarmed_loopback_error MATCHES "--arm-real-audio is required")
	message(FATAL_ERROR "unarmed loopback did not fail at the CLI safety gate")
endif()

execute_process(
	COMMAND "${CLI}" audio-meter --backend alsa --capture-id 00 --channel 0
		--arm-real-audio
	RESULT_VARIABLE invalid_meter_result
	OUTPUT_VARIABLE invalid_meter_output
	ERROR_VARIABLE invalid_meter_error
)
if(NOT invalid_meter_result EQUAL 2
		OR NOT invalid_meter_error MATCHES "not valid for audio-meter")
	message(FATAL_ERROR "audio meter accepted the output arming flag")
endif()

execute_process(
	COMMAND "${CLI}" audio-output-test --backend alsa --playback-id 00 --channel 0
		--level-dbfs nan --arm-real-audio
	RESULT_VARIABLE nonfinite_level_result
	ERROR_VARIABLE nonfinite_level_error
)
if(NOT nonfinite_level_result EQUAL 2
		OR NOT nonfinite_level_error MATCHES "invalid value for --level-dbfs")
	message(FATAL_ERROR "audio output accepted a non-finite level")
endif()

execute_process(
	COMMAND "${CLI}" audio-meter --backend alsa --capture-id 00 --channel 0
		--playback-channels 2
	RESULT_VARIABLE wrong_direction_result
	ERROR_VARIABLE wrong_direction_error
)
if(NOT wrong_direction_result EQUAL 2
		OR NOT wrong_direction_error MATCHES "playback options are not valid")
	message(FATAL_ERROR "audio meter accepted playback channel-count options")
endif()

execute_process(
	COMMAND "${CLI}" audio-meter --backend alsa --capture-id 00 --channel 0
		--channel 1
	RESULT_VARIABLE duplicate_channel_result
	ERROR_VARIABLE duplicate_channel_error
)
if(NOT duplicate_channel_result EQUAL 2
		OR NOT duplicate_channel_error MATCHES "duplicate channel option")
	message(FATAL_ERROR "audio meter accepted duplicate channel options")
endif()

execute_process(
	COMMAND "${CLI}" audio-loopback --backend alsa --playback-id 00
		--capture-id 00 --output-channel 64 --input-channel 0 --arm-real-audio
	RESULT_VARIABLE channel_bound_result
	ERROR_VARIABLE channel_bound_error
)
if(NOT channel_bound_result EQUAL 2
		OR NOT channel_bound_error MATCHES "invalid value for --output-channel")
	message(FATAL_ERROR "audio loopback accepted an out-of-range channel")
endif()
