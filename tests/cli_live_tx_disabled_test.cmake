# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/cli_live_tx_disabled_test.cmake
# SPDX-License-Identifier: GPL-3.0-or-later

execute_process(
	COMMAND "${CLI}" transmit-image
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
)
if(NOT result EQUAL 2)
	message(FATAL_ERROR "disabled transmit-image returned ${result}: ${output}${error}")
endif()
if(NOT error MATCHES "unknown argument: transmit-image")
	message(FATAL_ERROR "disabled CLI unexpectedly registered transmit-image: ${error}")
endif()
