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
