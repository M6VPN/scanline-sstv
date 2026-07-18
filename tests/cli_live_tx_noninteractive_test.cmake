# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/cli_live_tx_noninteractive_test.cmake
# SPDX-License-Identifier: GPL-3.0-or-later

execute_process(
	COMMAND "${CLI}" transmit-image
		--mode martin-m1 --input "${INPUT}"
		--backend alsa --playback-id hex:0102
		--output-channel 0 --playback-channels 2
		--ptt-provider flrig --ptt-address 127.0.0.1
		--ptt-port 1 --flrig-path /RPC2
		--pre-key-ms 250 --post-audio-ms 250 --gain-dbfs -30
		--arm-real-audio --arm-automatic-ptt --arm-live-tx
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
)
if(NOT result EQUAL 2)
	message(FATAL_ERROR "noninteractive live TX returned ${result}: ${output}${error}")
endif()
if(NOT error MATCHES "not interactively confirmed")
	message(FATAL_ERROR "noninteractive live TX did not fail at confirmation: ${error}")
endif()
