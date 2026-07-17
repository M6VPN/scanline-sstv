# Testing and performance plan

## 1. Test layers

### Unit

- Phase/timing accumulators, tone mapping, VIS parity, CRC/FEC, colour conversion.
- Ring-buffer wrap, overrun, underrun, cancellation, and clock conversion.
- Image recipes and configuration migration.
- XML-RPC and rigctld parsing with malformed and fragmented replies.

### Golden vector

Each on-air mode includes:

- A small diagnostic colour image.
- Expected scanline/tone event sequence.
- Deterministically generated WAV hash or tolerance-based sample comparison.
- Expected decoded pixels and metadata.
- Provenance and generator version.

Lossy/reference-program WAVs use feature/timing tolerances rather than brittle byte hashes.

### M1a offline Martin M1

The M1a suite runs without sound-card or radio access and covers:

- Rational normalization, invalid values, checked overflow, common sample rates, and a
  long cumulative schedule with no drift.
- Oscillator phase continuity and equivalent output for one-frame, odd, normal, and
  whole-sequence render blocks.
- Every valid seven-bit VIS code, LSB-first construction, even parity, complete framing,
  and the independently frozen Martin M1 VIS vector.
- Martin M1 dimensions, GBR ordering, exact timings, tone mapping, first-line boundaries,
  full event count, frame counts, and exact nominal duration.
- Immutable diagnostic-pattern corners, gradients, and line markers.
- Finite bounded float samples, documented PCM16 conversion, RIFF fields and sizes,
  overwrite policy, RIFF overflow, and atomic cleanup after failure.
- CLI success, overwrite refusal, `--force`, missing values, malformed rates, unknown
  modes, and extra arguments.

The compact vector under `tests/vectors/analogue/martin-m1` is generated with pinned
PySSTV source and contains no large WAV. Portable sample tests use numerical tolerances;
integer schedules and RIFF fields are exact.

### M1B safe raster preparation

The M1B tests use small project-generated fixtures with frozen SHA-256 hashes and unique
temporary directories. They cover:

- Exact RGB PNG round trip and equality between direct diagnostic-frame and prepared-image
  Martin M1 event streams.
- Landscape, portrait, square, tiny, and odd dimensions; contain bars; centered cover;
  crop bounds and overflow; and all eight EXIF orientations.
- Grayscale expansion, deterministic 16-bit to 8-bit conversion, premultiplied-alpha
  resizing and compositing, valid embedded sRGB profiles, corrupt profiles, and rejected
  unprofiled CMYK.
- Content-based JPEG/PNG loader selection, extension spoofing, truncated and malformed
  input, valid oversized headers, APNG, SVG, PDF, URLs, directories, FIFOs, and configured
  byte, dimension, pixel, page, and output limits.
- Deterministic repeated output, stripped atomic PNG publication, overwrite refusal,
  forced replacement, temporary-file cleanup, CLI parsing, exit codes, and offline WAV
  metadata. JPEG pixel checks use a documented tolerance of 100 for the compact 5 by 3
  high-quality fixture; lossless fixtures use exact or narrowly bounded checks.

Run `minimal` to verify the core-only boundary, `headless` for all non-Qt M1B tests, and
`dev` for the same suite plus the offscreen QML smoke test. Image tests also run under
ASan and UBSan where the compiler supports them. None of these tests plays audio, accesses
a radio, or keys PTT.

### M1C Scottie S1 offline TX

The M1C suite keeps the accepted M1a and M1B assertions and adds:

- An independently generated Scottie S1 VIS, event, timing, first-line, second-line,
  final-line, duration, mapping, and common-rate frame-count vector.
- Explicit proof that Scottie sync occurs between blue and red and that the first line has
  no additional Robot 1200C-style leading sync.
- Long repeated-line scheduling without drift, block sizes of one, odd, normal, and one
  large block, continuous phase, finite bounded samples, PCM conversion, and WAV metadata.
- Registry and central dispatch consistency for every mode advertising offline TX, plus
  typed rejection of unknown modes, missing capabilities, and wrong dimensions.
- Scottie test-pattern, PNG, and JPEG WAV paths; contain, cover, crop, background,
  overwrite, force, malformed mode, and atomic-failure cleanup cases.
- Pixel-identical exact-size PNG preparation followed by event-identical Scottie direct
  and prepared-frame encoding.

The Martin M1 reference JSON is not regenerated. A pre-refactor full-stream hash covers
every diagnostic event duration, frequency bit pattern, amplitude, and ordering in
addition to the accepted event-count, boundary, duration, frame-count, and prepared-PNG
assertions. ASan and UBSan run the encoder, dispatch, CLI, and image integration tests.
No test plays generated audio or accesses radio or PTT hardware.

### M1D Robot 36 offline TX

The M1D suite retains all M1a through M1C assertions and adds:

- Independent limited-range nonlinear RGB conversion vectors for black, white, neutral
  greys, primaries, boundary values, exact truncation, clamping, and deterministic output.
- Horizontal and vertical 2 by 2 post-conversion chroma averages, halfway truncation,
  alternating/checker patterns, component distinction, final line-pair bounds, invalid
  dimensions, invalid plane storage, and checked access.
- Robot VIS, dimensions, colour metadata, exact even/odd schedules, 1500/2300 Hz component
  identifiers, first, second, penultimate, and final line boundaries, full event count,
  duration, mapping, common-rate frame counts, and long no-drift scheduling.
- Block-size-invariant continuous-phase rendering, finite bounded samples, PCM16 RIFF
  metadata, overwrite refusal, forced replacement, and failed-publication cleanup.
- Central dispatch consistency and typed capability/dimension failures; test-pattern,
  native PNG/JPEG, contain, cover, crop, background, and exact-size prepared-PNG paths.
- Pixel-identical 320 by 240 diagnostic PNG preparation followed by event-identical Robot
  direct and prepared-frame encoding.

The Martin M1 and Scottie S1 reference JSON files are not regenerated. Their accepted
event counts, complete durations, frame counts, full existing test suites, fixture hashes,
and direct/prepared-image equality remain regression gates. ASan and UBSan cover the
colour conversion, subsampling, schedule, dispatch, CLI, and image integration paths.
No test plays generated audio or accesses radio or PTT hardware.

### M1E PD120 offline TX

The M1E suite retains all M1a through M1D assertions and adds:

- Independent PD120 conversion vectors for black, white, neutral greys, primaries,
  boundary values, exact fixed-point truncation, clamping, and deterministic output.
- Full-width vertically averaged red-difference and blue-difference planes, with fixtures
  distinguishing conversion-before-average, vertical averaging from row reuse, full
  horizontal resolution from Robot-style subsampling, component order, and final-pair
  bounds.
- PD120 VIS, dimensions, colour metadata, exact paired-line schedule, first, second,
  middle, and final pair boundaries, selected component mappings, complete event count,
  exact duration, all supported frame counts, and long no-drift scheduling.
- Block-size-invariant continuous-phase rendering, finite bounded samples, PCM16 RIFF
  metadata, overwrite refusal, forced replacement, and failed-publication cleanup.
- Registry and dispatch consistency, typed capability and dimension failures, diagnostic
  test-pattern WAV generation, native PNG/JPEG image WAV generation, and contain, cover,
  crop, background, and exact-size prepared-image paths.
- Pixel-identical 640 by 496 diagnostic PNG preparation followed by event-identical PD120
  direct and prepared-frame encoding.

The Martin M1, Scottie S1, and Robot 36 reference JSON files are not regenerated. Their
accepted event streams, durations, sample boundaries, frame counts, fixture hashes, and
direct/prepared-image equality remain regression gates. Robot conversion and 4:2:0 plane
bytes remain exact. ASan and UBSan cover PD120 conversion, paired-line preparation,
scheduling, dispatch, CLI, and image integration. No test plays generated audio or
accesses radio or PTT hardware.

### M1F optional FSK ID and WAV inspection

The M1F suite retains all M1a through M1E assertions and adds:

- An independent FSK vector for `M6VPN-1` covering its normalized alphabet, six-bit
  LSB-first codes, leader/start sequence, checksum, trailer, exact event boundaries,
  duration, event count, and frame count at every supported sample rate.
- Empty, all-space, oversized, control, NUL, non-ASCII, unsupported-character, and
  boundary-length identifier validation; lowercase ASCII normalization is explicit.
- Exact disabled-FSK regression for all four mode streams and exact base-prefix plus suffix
  comparison when enabled, including block-size-invariant continuous-phase rendering
  across the composition boundary.
- Test-pattern and image CLI paths, duplicate and missing options, wrong-command rejection,
  overwrite refusal, forced replacement, normalized status output, and combined metadata.
- Generated RIFF fixtures for valid writer output from every mode, unknown and odd-sized
  chunks, independently calculated sample statistics, truncated and contradictory sizes,
  duplicate/missing critical chunks, bad padding/alignment/rates/formats, overflow attempts,
  URLs, directories, symlinks, FIFOs, and resource limits.

The inspector rejects symlinks rather than resolving them. Defaults are 256 MiB per input,
16 MiB per unknown chunk, and 4096 chunks. These bounds exceed the largest current
192 kHz PCM16 transmission while bounding traversal and streamed analysis. Sample analysis
uses 8192-byte blocks. CLI duration, DC mean, and RMS are locale-independent fixed-point
decimal values in raw PCM16 sample units with six digits after the decimal point. Standard
classic-locale stream formatting rounds the computed `long double` value to the nearest
six-decimal representation; integer metadata and counts are printed exactly.

Run the sanitizer presets with:

    cmake --preset asan
    cmake --build --preset asan
    ctest --preset asan --output-on-failure

    cmake --preset ubsan
    cmake --build --preset ubsan
    ctest --preset ubsan --output-on-failure

Both presets use Clang, retain image integration, and disable the GUI. No M1F test plays
audio, performs SSTV receive decoding, accesses radio hardware, or keys PTT.

### M1G offline GUI TX editor

The frontend-independent M1G app-service suite verifies registry-derived choices and
dimensions for all four accepted modes, exact cumulative frame projection at supported
sample rates, stale-generation rejection, immutable preparation, optional FSK metadata,
atomic PNG/WAV publication, overwrite refusal and confirmed replacement, and inspection
of the actual exported WAV.

The Qt model suite runs with `QT_QPA_PLATFORM=offscreen` and
`QT_QUICK_BACKEND=software`. It verifies initial, loading, ready, exporting, completed,
and error behavior; mode/recipe invalidation; stale completion rejection; local-URL
policy; FSK validation; revisioned preview publication; export enablement and overwrite
confirmation; and WAV inspection. The GUI smoke test also requires four registry modes,
the editor workspace, the visible offline safety notice, and no enabled action named
`Transmit`.

Native Wayland and XCB launches are runtime checks separate from offscreen acceptance.
Offscreen success does not claim native compositor coverage. No M1G target links an audio
or radio-control implementation, and no test plays generated WAV files.

### M2A read-only audio discovery

M2A unit tests inject a deterministic provider. They verify backend enum and name
mappings, compiled and uncompiled results, exactly one attempt per requested backend,
multiple successes, partial failure, zero-device success, capture/playback direction,
defaults, duplicate names, exact duplicate IDs, identity collisions, known and unknown
capabilities, persistence claims, transport classification, immutable generation
publication, failed-refresh preservation, cancellation, resource limits, and concurrent
refresh rejection. Instrumentation keeps device initialization, device start, playback,
capture, callback, and PTT operation counts at zero.

Adapter tests serialize only initialized PulseAudio, ALSA, JACK, and null ID fields.
They never hash raw `ma_device_id` union storage. CLI tests cover malformed filters,
missing and duplicate options, filtering, null opt-in, exit classes, and safe output
formatting. Device names are untrusted; controls and invalid UTF-8 bytes must appear as
`\\xNN` escapes.

The separately named host smoke test may initialize one context per compiled backend and
enumerate devices. It accepts zero devices and total backend unavailability, opens no
device, and makes no hardware-presence assertion. Linux manual checks record whether the
actual host exposes ALSA, PulseAudio or PipeWire-Pulse, and JACK or PipeWire-JACK. Linux
results do not establish FreeBSD, OpenBSD, or NetBSD compatibility.

`SSTV_BUILD_AUDIO=OFF` excludes `sstv_audio` and leaves the established offline
image-to-WAV path available. The `minimal` preset disables both image and audio modules;
the `audio-disabled` preset retains image support. ASan and UBSan exercise provider,
refresh, error, cancellation, identity, and teardown paths.

On the current Linux host, the installed libjack client blocks for more than 40 seconds
when miniaudio probes a missing JACK server under ASan. PulseAudio and ALSA context
discovery and teardown complete under ASan, and deterministic provider tests cover JACK
success and failure without libjack. The sanitizer host smoke therefore probes
PulseAudio and ALSA; non-sanitized manual verification probes JACK with server autostart
disabled. No sanitizer suppression is used.

### M2B audio transport

`scanline-sstv-audio-stream-tests` uses only an injected synchronous adapter. Ring tests
cover exact capacity, non-power-of-two wrap, partial operations, long sequences, invalid
capacity, and a two-thread producer/consumer stress run. Callback tests cover selected
and duplicated playback mapping, silent non-selected channels, selected interleaved
capture, arbitrary and zero block sizes, underrun zero fill, preserve-oldest capture
overflow, and immutable counter snapshots. Lifecycle tests cover exact identity and
generation validation, prefill, transitions, cancellation, injected operation failures,
disconnect publication, and a final callback during stop.

`scanline-sstv-audio-null-stream-test` is labelled `audio-null`, has a five-second
timeout, and initializes only miniaudio's null backend. It neither enumerates nor opens
real hardware. The test checks playback FIFO consumption, silence after underrun, silent
null capture, statistics, and teardown. M2A's host discovery smoke remains enumeration
only.

The focused ThreadSanitizer run is:

```sh
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan --output-on-failure
```

The `tsan` test preset selects only the `audio-concurrency` label. A host runtime failure
must be reported rather than suppressed or treated as a passing concurrency test.

### Impairment corpus

Generated variants use recorded seeds and parameters:

- AWGN across a useful SNR range.
- 50/60 Hz hum and harmonics.
- Gain error, clipping, DC offset, and quantisation.
- Audio frequency offset and slow drift.
- Sample clock error and time-varying drift.
- Dropouts, duplicated blocks, and buffer discontinuities.
- Voice-radio band-pass, ripple, and group-delay approximations.
- Single/delayed-path mixtures.

Original unmodified audio remains available so a pipeline regression can be separated from
a corpus-generation change.

### Integration

- WAV encode to decoder.
- Recording replay to live decoder service.
- Mock miniaudio device lifecycle.
- Mock flrig and rigctld PTT state/fault matrix.
- GUI/TUI snapshot models without requiring radio hardware.

### Hardware in loop

Disabled by default and labelled clearly. Initial fixtures use a dummy load, loopback
audio, and a mock or physically observed PTT line. A test must never assume an antenna is
safe.

## 2. Receiver metrics

- VIS/mode detection result and confidence.
- Completed image rate.
- Time to first locked line.
- Sync loss/reacquisition count.
- Estimated frequency offset and sample-clock error.
- Pixel MAE and SSIM where a source image is known.
- Correctly positioned scanline percentage.
- Real-time factor, CPU time, peak memory, ring overruns, and UI drops.

Corpus thresholds are versioned. Updating a threshold requires a result comparison and
reason in the changelog or benchmark report.

## 3. Performance method

- Measure release builds after a warm-up.
- Record compiler, flags, CPU, OS, audio rate/block size, FFT backend, and thread count.
- Separate DSP throughput from GUI frame rate and image-file loading.
- Do not run FFTW planning in the timed streaming section.
- Use fixed inputs and report median plus dispersion, not only the best run.
- Profile before adding manual SIMD; 48 kHz audio does not justify opaque code without a
  measured gain.

## 4. Comparison

Use identical audio files and known settings with the current QSSTV release. MMSSTV
comparison requires a recorded Windows/Wine version and configuration. Save outputs and
metrics, but redistribute third-party binaries or images only when their licences permit.

The goal is not to reproduce another decoder’s artefacts. A difference is acceptable when
the source image/timing evidence shows this decoder is more accurate.

## 5. CI matrix

Planned required jobs:

- Linux GCC debug, warnings, and sanitizers.
- Linux Clang release and performance smoke.
- Qt GUI compile plus headless QML launch test.
- FreeBSD Clang.
- OpenBSD Clang.
- NetBSD Clang/GCC as supported by pkgsrc.

All CI uses mock PTT. Integration jobs bind test servers to loopback only.

Current hosted CI runs Linux GCC and Clang with libvips and read-only audio discovery, a
separate image/audio-disabled minimal build, an audio-disabled offline-image build, and Qt
with offscreen software rendering. Native FreeBSD, OpenBSD, and NetBSD jobs remain later
focused portability work; Linux containers are not treated as BSD coverage.
