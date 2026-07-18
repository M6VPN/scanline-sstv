# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/cli_hil_test.cmake
# SPDX-License-Identifier: GPL-3.0-or-later

set(output "${CMAKE_CURRENT_BINARY_DIR}/m2j-cli-evidence")
file(REMOVE_RECURSE "${output}")
file(MAKE_DIRECTORY "${output}")
set(arguments
	hil-manifest --output-dir "${output}"
	--utc-start 2026-07-18T12:00:00Z
	--git-commit 0123456789abcdef0123456789abcdef01234567
	--compiler Clang --compiler-version 18.1.8 --preset headless
	--cmake-options SSTV_ENABLE_LIVE_TX=OFF --worktree clean
	--miniaudio-version 0.11.25 --os Linux --kernel test --arch x86_64
	--host-id sha256:redacted --session cli --backend mock
	--playback-id mock:playback:fixture --identity-persistence session-only
	--identity-collision no
	--channel 0 --channels 2 --ptt-provider mock --ptt-address 127.0.0.1
	--ptt-port 12345 --radio-manufacturer required --radio-model required
	--audio-interface required --cabling required --test-arrangement dummy-load
	--radio-mode required --frequency required --power required --vox disabled
	--compressor required --tot required --antenna disconnected --mode robot-36
	--fixture-sha256 2ab0388b27325de68bdb3246cb4eaa043ba85bf27d65f3aebb5d0ba164cbc9d2
	--fsk-id-enabled no
	--duration-ns 36910000000 --frame-count 1771680 --gain-dbfs -60
	--pre-key-ms 250 --post-audio-ms 250)
execute_process(COMMAND "${CLI}" ${arguments}
	RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "HIL manifest command failed: ${stdout}${stderr}")
endif()
if(NOT EXISTS "${output}/m2j-evidence-v1.json"
		OR NOT EXISTS "${output}/m2j-evidence-v1.md")
	message(FATAL_ERROR "HIL manifest output is missing")
endif()
file(READ "${output}/m2j-evidence-v1.json" json)
if(NOT json MATCHES "\"resource_acquisitions\":\"0\"")
	message(FATAL_ERROR "HIL manifest did not record zero resource acquisition")
endif()
execute_process(COMMAND "${CLI}" ${arguments}
	RESULT_VARIABLE overwrite_result OUTPUT_QUIET ERROR_QUIET)
if(overwrite_result EQUAL 0)
	message(FATAL_ERROR "HIL manifest overwrote evidence without --force")
endif()
execute_process(COMMAND "${CLI}" hil-manifest --output-dir "${output}"
	--output-dir "${output}" RESULT_VARIABLE duplicate_result
	OUTPUT_QUIET ERROR_QUIET)
if(duplicate_result EQUAL 0)
	message(FATAL_ERROR "HIL manifest accepted duplicate options")
endif()
execute_process(COMMAND "${CLI}" hil-manifest --output-dir
	RESULT_VARIABLE missing_result OUTPUT_QUIET ERROR_QUIET)
if(missing_result EQUAL 0)
	message(FATAL_ERROR "HIL manifest accepted a missing value")
endif()
execute_process(COMMAND "${CLI}" hil-manifest --unknown value
	RESULT_VARIABLE unknown_result OUTPUT_QUIET ERROR_QUIET)
if(unknown_result EQUAL 0)
	message(FATAL_ERROR "HIL manifest accepted an unknown option")
endif()
file(REMOVE_RECURSE "${output}")
