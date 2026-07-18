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

### M2C audio diagnostics

`scanline-sstv-audio-diagnostics-tests` is hardware-free. It independently checks the
-60 to -6 dBFS conversion range, default -30 dBFS calibration signal, continuous phase,
10 ms fades, exact duration, deterministic loopback sequence, incremental peak/RMS/DC and
clipping statistics, finite -120 dBFS silence representation, and fail-closed NaN/infinity
handling. Frozen correlation cases cover a 137-frame delay, -6.02 dB gain, polarity
inversion, silence, and insufficient capture. Instrumentation proves an unarmed output or
loopback request performs no discovery or adapter creation.

An explicitly pumped injected stream adapter covers selected-channel capture, exact
bounded meter frames, overrun/drop accounting, selected-channel calibration output,
producer-starvation underrun, known-delay loopback, device removal, cancellation during
discovery, stream cleanup, and concurrent-start rejection without sleeps or hardware.

CLI tests cover missing, duplicate, malformed, wrong-command, channel-bound, and unarmed
options. Usage failures return 2, runtime failures return 1, and inconclusive loopback
returns 3. The Qt model test injects discovery, verifies refresh publishes exact identities
without selecting one, rejects a second queued refresh immediately, and proves unarmed
output creates no stream adapter. The model uses request revisions to discard stale queued
snapshots. The offscreen
smoke test requires the audio diagnostics panel, PTT-unavailable notice, and absence of an
enabled Transmit action.

The null-backend integration exercises a bounded silent input meter and armed low-level
output calibration. Null capture is not treated as a physical loopback. Normal CI never
constructs a physical-device adapter. The hardware gate defaults off:

```sh
cmake -S . -B build/audio-hardware -G Ninja \
    -DSSTV_ENABLE_AUDIO_HARDWARE_TESTS=ON \
    -DSSTV_ARM_AUDIO_HARDWARE_TESTS=ON \
    -DSSTV_AUDIO_HARDWARE_BACKEND=alsa \
    -DSSTV_AUDIO_HARDWARE_PLAYBACK_ID=ID \
    -DSSTV_AUDIO_HARDWARE_CAPTURE_ID=ID \
    -DSSTV_AUDIO_HARDWARE_OUTPUT_CHANNEL=0 \
    -DSSTV_AUDIO_HARDWARE_INPUT_CHANNEL=0 \
    -DSSTV_AUDIO_HARDWARE_PLAYBACK_CHANNELS=2 \
    -DSSTV_AUDIO_HARDWARE_CAPTURE_CHANNELS=2
```

That option only makes separately authored hardware tests eligible. A run must still
receive explicit backend, device identities, channels, and a per-run arm flag. Before a
manual cable-loopback check: disconnect radio transmit audio where practical, disable
VOX, verify external PTT is inactive, reduce monitor/headphone volume, connect only the
chosen output to the chosen input or dummy audio load, start at -60 dBFS, verify Stop,
then verify device-removal cleanup. Hardware tests are never run in CI.

### M2D mock transmit safety

`scanline-sstv-m2d-transmit-tests` uses only injected mocks. Its source contains a short
finite sequence of generated float values, not an analogue encoder, `ToneRenderer`, VIS,
FSK ID, image, or WAV data. The mock endpoint records open, silent prefill, prime, start,
queue, gate, stop, and close operations but cannot construct miniaudio or select a device.
The scriptable PTT provider contains no network, serial, GPIO, process, or radio access.

All safety timing uses an injected monotonic clock and scheduler. Tests advance virtual
time explicitly; no state assertion depends on `sleep` or wall-clock deadlines. The
normal trace is checked exactly from preparing through audio opening/priming, watchdog
arming, keying, pre-key delay, finite sample release, drain, tail, unkey, completion, and
idle. Fault cases verify the exact PTT action sequence, signal gate, unkey attempts, final
certainty, audio stop/close attempts, outcome, and retained hazard.

Coverage includes invalid policy before acquisition, audio open/prefill/prime failure,
watchdog-arm failure, definite-unkeyed key rejection, readback confirmation, ambiguous
key timeout, readback mismatch, bounded unkey retry success, permanent unkey failure,
running underrun, bounded drain timeout, coordinator-stall watchdog expiry,
disconnect/removal, source failure, start and cleanup failures, cancellation before key,
every active coordinator phase, shutdown during opening, watchdog expiry, lease
destruction, and second-session rejection while a hazard remains.
The `audio-concurrency` label includes M2D so the focused TSan preset exercises provider
serialization, shared safety snapshots, watchdog teardown, and coordinator cancellation.

M2D has no CLI or GUI transmit action. The tests must not open a real or null miniaudio
device, render an SSTV waveform, access a socket or serial port, or key hardware. A
provider that ignores its deadline is outside the in-process guarantee; `SIGKILL`, power
loss, and kernel failure also remain unrecoverable by RAII or the watchdog.

### M2E flrig loopback safety

`scanline-sstv-m2e-flrig-tests` first exercises HTTP/XML-RPC request generation and
parsing through deterministic in-memory fixtures and an injected transport. Coverage
includes address/path rejection before transport use, exact key/unkey/query bodies,
Content-Length framing, response fragmentation, redirects, transfer encodings, unsafe
XML constructs, invalid PTT scalars, expired deadlines, confirmed readback, and lost key
responses.

The integration server binds only `127.0.0.1` on an ephemeral port and records every RPC.
It drives `FlrigPttProvider` and the M2D coordinator with a finite generated source and
mock audio endpoint. The coordinator test begins with keyed readback, confirms preflight
unkey before audio acquisition, then verifies key and final unkey readbacks. The
`rig-loopback` label has a bounded timeout. No test contacts an installed flrig process,
opens physical audio, renders or plays SSTV, resolves DNS, or accesses serial/radio/PTT
hardware.

### M2F loopback-only rigctld provider

`scanline-sstv-m2f-rigctld-tests` exercises exact Extended Response Protocol commands,
bounded line framing, every documented PTT state, negative `RPRT` results, checked integer
parsing, fragmented input, unsupported response modes, controls, line/byte limits, and
injected transport failures. Configuration tests prove that invalid endpoints and limits
fail before the POSIX transport is constructed.

One provider-neutral conformance runner applies the same query, confirmed key, confirmed
unkey, ambiguous-key, request-attempt, operation-ID, timestamp, and diagnostic-bound
assertions to mock, flrig, and rigctld providers. Existing M2D and M2E tests remain
separate regression gates.

The M2F TCP server binds only an ephemeral `127.0.0.1` or `::1` endpoint, supports
byte-fragmented responses, records exact command order, and drives the existing M2D
coordinator with mock audio. The `rig-loopback` and `audio-concurrency` labels keep the
suite in normal sanitizer and focused TSan runs. No external rigctld or flrig process is
spawned, and no Hamlib library, serial device, physical audio device, radio, hardware PTT,
or SSTV playback path is reachable.

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

Current hosted CI runs Linux GCC and Clang with libvips, read-only audio discovery, and
hardware-free M2B/M2C audio tests, a
separate image/audio-disabled minimal build, an audio-disabled offline-image build, and Qt
with offscreen software rendering. Native FreeBSD, OpenBSD, and NetBSD jobs remain later
focused portability work; Linux containers are not treated as BSD coverage.
