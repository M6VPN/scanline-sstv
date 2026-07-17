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

Current hosted CI runs Linux GCC and Clang with libvips, a separate image-disabled minimal
build, and Qt with offscreen software rendering. Native FreeBSD, OpenBSD, and NetBSD jobs
remain later focused portability work; Linux containers are not treated as BSD coverage.
