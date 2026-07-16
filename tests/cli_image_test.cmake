# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/cli_image_test.cmake
# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED CLI OR NOT DEFINED INPUT OR NOT DEFINED JPEG OR NOT DEFINED WORK)
	message(FATAL_ERROR "CLI, INPUT, JPEG, and WORK must be defined")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(png "${WORK}/prepared.png")
set(wav "${WORK}/image.wav")
set(scottie_png "${WORK}/scottie-prepared.png")
set(scottie_wav "${WORK}/scottie-image.wav")
set(scottie_jpeg_wav "${WORK}/scottie-jpeg.wav")

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

run_expect(0 prepare-image --mode martin-m1 --input "${INPUT}" --output "${png}"
	--fit contain --background 102030)
if(NOT EXISTS "${png}")
	message(FATAL_ERROR "prepare-image did not publish its PNG")
endif()
foreach(expected IN ITEMS
	"Source dimensions: 5x3"
	"Applied orientation: 1"
	"Crop: none"
	"Fit: contain"
	"Prepared dimensions: 320x256")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing prepare-image output: ${expected}")
	endif()
endforeach()

run_expect(1 prepare-image --mode martin-m1 --input "${INPUT}" --output "${png}")
run_expect(0 prepare-image --mode martin-m1 --input "${INPUT}" --output "${png}"
	--fit cover --force)
run_expect(0 encode-image --mode martin-m1 --input "${INPUT}" --output "${wav}"
	--crop 0,0,5,3 --sample-rate 8000)
if(NOT EXISTS "${wav}")
	message(FATAL_ERROR "encode-image did not publish its WAV")
endif()
file(SIZE "${wav}" wav_size)
if(NOT wav_size EQUAL 1843246)
	message(FATAL_ERROR "Unexpected 8 kHz image WAV size: ${wav_size}")
endif()
foreach(expected IN ITEMS "Sample rate: 8000 Hz" "Frame count: 921601")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing encode-image output: ${expected}")
	endif()
endforeach()

run_expect(0 prepare-image --mode scottie-s1 --input "${INPUT}"
	--output "${scottie_png}" --fit contain --background 203040)
if(NOT EXISTS "${scottie_png}")
	message(FATAL_ERROR "Scottie S1 prepare-image did not publish its PNG")
endif()
foreach(expected IN ITEMS
	"Source format: PNG"
	"Source dimensions: 5x3"
	"Fit: contain"
	"Prepared dimensions: 320x256")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing Scottie S1 prepare-image output: ${expected}")
	endif()
endforeach()

run_expect(0 encode-image --mode scottie-s1 --input "${INPUT}"
	--output "${scottie_wav}" --fit cover --crop 0,0,5,3 --sample-rate 8000)
if(NOT EXISTS "${scottie_wav}")
	message(FATAL_ERROR "Scottie S1 PNG encode-image did not publish its WAV")
endif()
file(SIZE "${scottie_wav}" scottie_wav_size)
if(NOT scottie_wav_size EQUAL 1768592)
	message(FATAL_ERROR "Unexpected Scottie S1 image WAV size: ${scottie_wav_size}")
endif()
foreach(expected IN ITEMS
	"Source format: PNG"
	"Crop: 0,0,5,3"
	"Fit: cover"
	"Frame count: 884274"
	"Duration: 110.534320 seconds")
	string(FIND "${last_stdout}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Missing Scottie S1 PNG encode output: ${expected}")
	endif()
endforeach()

run_expect(0 encode-image --mode scottie-s1 --input "${JPEG}"
	--output "${scottie_jpeg_wav}" --fit contain --background 102030
	--sample-rate 8000)
if(NOT EXISTS "${scottie_jpeg_wav}")
	message(FATAL_ERROR "Scottie S1 JPEG encode-image did not publish its WAV")
endif()
string(FIND "${last_stdout}" "Source format: JPEG" found)
if(found EQUAL -1)
	message(FATAL_ERROR "Scottie S1 JPEG path did not report its actual loader")
endif()

run_expect(1 encode-image --mode scottie-s1 --input "${INPUT}"
	--output "${scottie_wav}")
run_expect(0 encode-image --mode scottie-s1 --input "${INPUT}"
	--output "${scottie_wav}" --force)

run_expect(1 encode-image --mode martin-m1 --input "${INPUT}" --output "${wav}")
file(WRITE "${wav}" "replacement sentinel")
run_expect(0 encode-image --mode martin-m1 --input "${INPUT}" --output "${wav}" --force)
run_expect(2 prepare-image --mode unknown --input "${INPUT}" --output "${WORK}/x.png")
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output "${WORK}/x.png" --fit stretch)
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output "${WORK}/x.png" --crop 0,0,0,1)
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output "${WORK}/x.png" --background xyzxyz)
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output "${WORK}/x.png" --fit contain --fit cover)
run_expect(2 encode-image --mode martin-m1 --input "${INPUT}" --output "${WORK}/x.wav" --sample-rate nope)
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output)
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output "${WORK}/x.png" extra)
run_expect(2 prepare-image --mode martin-m1 --input "${INPUT}" --output "${INPUT}" --force)

set(failed "${WORK}/missing/output.png")
run_expect(1 prepare-image --mode martin-m1 --input "${INPUT}" --output "${failed}")
file(GLOB abandoned "${failed}.tmp.*")
if(abandoned)
	message(FATAL_ERROR "Failed image publish abandoned temporary files: ${abandoned}")
endif()

set(failed_wav "${WORK}/missing/scottie.wav")
run_expect(1 encode-image --mode scottie-s1 --input "${INPUT}" --output "${failed_wav}")
if(EXISTS "${failed_wav}")
	message(FATAL_ERROR "Failed Scottie S1 WAV publish left a destination")
endif()
file(GLOB abandoned_wav "${failed_wav}.tmp.*")
if(abandoned_wav)
	message(FATAL_ERROR
		"Failed Scottie S1 WAV publish abandoned temporary files: ${abandoned_wav}")
endif()

file(REMOVE_RECURSE "${WORK}")
