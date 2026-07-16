# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/cli_encode_test.cmake
# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED CLI OR NOT DEFINED WORK)
	message(FATAL_ERROR "CLI and WORK must be defined")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(output "${WORK}/martin-m1.wav")
set(scottie_output "${WORK}/scottie-s1.wav")

function(run_expect expected)
	execute_process(
		COMMAND "${CLI}" ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr
	)
	if(NOT result EQUAL expected)
		message(FATAL_ERROR
			"Expected exit ${expected}, got ${result}\nstdout: ${stdout}\nstderr: ${stderr}")
	endif()
	set(last_stdout "${stdout}" PARENT_SCOPE)
endfunction()

run_expect(0 encode-test-pattern --mode martin-m1 --output "${output}"
	--sample-rate 8000)
if(NOT EXISTS "${output}")
	message(FATAL_ERROR "Successful encode did not publish the WAV")
endif()
file(SIZE "${output}" output_size)
if(NOT output_size EQUAL 1843246)
	message(FATAL_ERROR "Unexpected 8 kHz WAV size: ${output_size}")
endif()
foreach(expected IN ITEMS
	"Mode: martin-m1"
	"Dimensions: 320x256"
	"Sample rate: 8000 Hz"
	"Frame count: 921601"
	"Duration: 115.200176 seconds")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing success output: ${expected}")
	endif()
endforeach()

run_expect(0 encode-test-pattern --mode scottie-s1 --output "${scottie_output}"
	--sample-rate 8000)
if(NOT EXISTS "${scottie_output}")
	message(FATAL_ERROR "Successful Scottie S1 encode did not publish the WAV")
endif()
file(SIZE "${scottie_output}" scottie_size)
if(NOT scottie_size EQUAL 1768592)
	message(FATAL_ERROR "Unexpected Scottie S1 8 kHz WAV size: ${scottie_size}")
endif()
foreach(expected IN ITEMS
	"Mode: scottie-s1"
	"Dimensions: 320x256"
	"Sample rate: 8000 Hz"
	"Frame count: 884274"
	"Duration: 110.534320 seconds")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing Scottie S1 success output: ${expected}")
	endif()
endforeach()

run_expect(1 encode-test-pattern --mode scottie-s1 --output "${scottie_output}"
	--sample-rate 8000)
run_expect(0 encode-test-pattern --mode scottie-s1 --output "${scottie_output}"
	--sample-rate 8000 --force)

run_expect(0 --list-modes)
foreach(expected IN ITEMS
	"martin-m1\tMartin M1\t320x256\toffline-test-pattern-tx,offline-image-tx"
	"scottie-s1\tScottie S1\t320x256\toffline-test-pattern-tx,offline-image-tx")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing registered mode output: ${expected}")
	endif()
endforeach()

run_expect(0 --help)
foreach(expected IN ITEMS "--mode MODE" "--mode martin-m1" "--mode scottie-s1")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing generic or accepted-mode help: ${expected}")
	endif()
endforeach()

run_expect(1 encode-test-pattern --mode martin-m1 --output "${output}"
	--sample-rate 8000)
run_expect(0 encode-test-pattern --mode martin-m1 --output "${output}"
	--sample-rate 8000 --force)
run_expect(2 encode-test-pattern --mode unknown --output "${WORK}/unknown.wav")
run_expect(2 encode-test-pattern --mode martin-m1 --output "${WORK}/bad.wav"
	--sample-rate nope)
run_expect(2 encode-test-pattern --mode martin-m1 --output)
run_expect(2 encode-test-pattern --mode martin-m1 --output --force)
run_expect(2 encode-test-pattern --mode martin-m1 --output "${WORK}/extra.wav" extra)

set(blocked "${WORK}/blocked")
file(MAKE_DIRECTORY "${blocked}")
file(WRITE "${blocked}/keep" "keep")
run_expect(1 encode-test-pattern --mode martin-m1 --output "${blocked}"
	--sample-rate 8000 --force)
file(GLOB abandoned "${blocked}.tmp.*")
if(abandoned)
	message(FATAL_ERROR "Failed publish abandoned temporary files: ${abandoned}")
endif()

file(REMOVE_RECURSE "${WORK}")
