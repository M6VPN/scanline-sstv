# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases
will use semantic versioning once the public protocol and command-line interfaces
stabilise.

## [Unreleased]

M1a adds the first evidence-backed offline analogue waveform slice. M1B adds safe offline
raster preparation and Martin M1 image-to-WAV generation. M1C adds evidence-backed
Scottie S1 offline test-pattern and image-to-WAV generation through shared analogue
services. M1D adds evidence-backed Robot 36 offline test-pattern and image-to-WAV
generation with deterministic luma and colour-difference subsampling. M1E adds
evidence-backed PD120 offline test-pattern and image-to-WAV generation with paired luma
rows and full-width vertically averaged colour-difference components. Overall M1 remains
in progress. M1F adds optional evidence-backed analogue FSK ID suffixes and defensive
offline PCM16 RIFF/WAVE inspection. M1G adds the Wayland-first offline GUI TX editor and
mode-aware prepared-image preview. M2A adds pinned miniaudio and read-only backend/device
discovery. M2B adds deterministic bounded sample transport and mock/null stream lifecycle
without an SSTV live-audio or PTT path. M2C adds explicitly armed real-device metering,
calibration, and local loopback diagnostics without SSTV playback or PTT. M2D adds
mock-only transmit orchestration, explicit PTT certainty, mandatory unkey cleanup, an
RAII lease, and an independent watchdog without a real audio, radio, or PTT path. M1
remains incomplete pending the M3 round-trip dependency, and M2 remains incomplete.

### Added

- Locked C++20, Qt Quick 6, miniaudio, liquid-dsp, FFTW3f, libvips, notcurses, flrig,
  Hamlib, HamDRM, and KG-STV architecture.
- Authoritative project plan, component architecture, milestones, dependency policy,
  protocol provenance rules, and verification strategy.
- CMake foundation with headless and GUI-capable presets.
- Frontend-independent mode registry API.
- Diagnostic `scanline-sstv-cli` with version and mode-list commands.
- Optional Qt Quick GUI foundation.
- Frontend-independent offline TX editor service with immutable prepared snapshots,
  bounded retained events, exact frame projections, atomic export, and WAV inspection.
- Asynchronous Qt editor model with stale-request rejection and a revisioned thread-safe
  prepared-image provider.
- Offline GUI controls for all four accepted modes, image recipe, sample rate, optional
  FSK ID, prepared PNG/WAV export, overwrite confirmation, and PCM16 WAV inspection.
- Qt model and extended offscreen GUI smoke tests covering editor states, registry modes,
  preview invalidation, local-URL policy, export safety, and the no-audio/no-PTT boundary.
- Core smoke test.
- Noninteractive Qt Quick startup smoke test using offscreen software rendering.
- Linux GCC and Clang headless CI plus a Qt 6.5+ GUI smoke-test job.
- GPL-3.0-or-later project licensing.
- Martin M1 and VIS protocol evidence with pinned source revisions, artifact hashes,
  resolved timing values, and a documented QSSTV timing divergence.
- Exact rational durations, cumulative sample-boundary scheduling, typed tone events, and
  a continuous-phase pull renderer.
- Immutable RGB8 frames and a deterministic 320 by 256 diagnostic pattern.
- Evidence-gated Martin M1 offline test-pattern registration and compact independent
  golden vectors.
- Atomic streaming mono PCM16 WAV output with explicit conversion and overwrite policy.
- `encode-test-pattern` CLI support for Martin M1 at validated sample rates.
- M1a timing, waveform, VIS, descriptor, WAV, vector, and CLI tests.
- Dedicated `sstv_image` target using system `vips-cpp`, typed image results, reusable
  value recipes, immutable RGB8 output, and centralized nonzero resource limits.
- Content-selected native JPEG/PNG loading with regular-file checks, APNG rejection,
  header-first limits, EXIF orientation, oriented crop, ICC validation and sRGB conversion,
  grayscale/16-bit conversion, premultiplied alpha, Lanczos contain/cover fit, and explicit
  background compositing.
- Atomic stripped prepared-PNG publication plus `prepare-image` and `encode-image` CLI
  commands with shared recipe parsing and overwrite policy.
- Project-generated compact raster fixtures and M1B unit, integration, CLI, malformed-input,
  resource-limit, atomic-cleanup, exact diagnostic round-trip, and sanitizer tests.
- Minimal core-only CMake preset with image support disabled.
- Scottie S1 protocol evidence with pinned source revisions, artifact hashes, resolved
  VIS, first-line sequence, sync placement, exact timings, tone mapping, duration, and
  documented PySSTV and QSSTV divergences.
- Mode-neutral sequential RGB fixed-tone and channel-scan schedules with explicit
  pre-image and first-line behavior, plus one shared Martin M1 and Scottie S1 encoder.
- Registry-backed typed offline TX dispatch used by test-pattern and image WAV commands.
- Scottie S1 offline test-pattern, native JPEG/PNG image, atomic WAV, compact vector,
  timing, renderer, dispatch, CLI, image integration, and sanitizer tests.
- Robot 36 protocol and colour evidence with pinned sources, inspected locations, artifact
  hashes, exact VIS and line schedule, and resolved handbook, QSSTV, and PySSTV
  disagreements.
- Immutable dependency-free full-resolution luma and 2 by 2 subsampled red-difference and
  blue-difference storage with checked dimensions, plane sizes, and access.
- Deterministic fixed-point nonlinear RGB conversion, post-conversion chroma averaging,
  explicit even/odd component selection, and a separate alternating-component descriptor.
- Robot 36 offline test-pattern, native JPEG/PNG image, atomic WAV, independent compact
  vector, colour, subsampling, timing, renderer, dispatch, CLI, image integration, and
  sanitizer tests.
- PD120 protocol and colour evidence with pinned sources, inspected locations, artifact
  hashes, exact VIS and paired-line schedule, and resolved timing, terminology, colour,
  averaging, and active-height disagreements.
- Immutable dependency-free paired-line storage with full-resolution luma and full-width
  vertically subsampled red-difference and blue-difference planes, checked dimensions,
  plane sizes, arithmetic, and access.
- Deterministic fixed-point nonlinear RGB conversion, conversion-before-average chroma,
  explicit first/second luma and colour-difference ordering, and a separate paired-line
  descriptor.
- PD120 offline test-pattern, native JPEG/PNG image, atomic WAV, independent compact
  vector, colour, paired-line, timing, renderer, dispatch, CLI, image integration, and
  sanitizer tests.
- Analogue FSK ID evidence with pinned implementation revisions, artifact hashes, resolved
  framing differences, and a compact independent event/boundary vector.
- Validated immutable FSK identifiers and mode-neutral suffix composition through typed
  offline-transmission options, with exact combined durations and continuous phase.
- Optional `--fsk-id` support for test-pattern and prepared-image WAV commands.
- Defensive `inspect-wav` command and frontend-independent streaming RIFF/WAVE PCM16
  parser with typed errors, checked chunk arithmetic, bounded resources, and sample
  statistics.
- Clang ASan and UBSan CMake presets covering the core, CLI, parser, and image integration.
- Pinned miniaudio 0.11.25 at commit
  `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`, with exact imported-file hashes, complete
  upstream licence, enabled-facility record, and update procedure.
- Frontend-independent `sstv_audio` discovery values and service with one context per
  backend, immutable snapshot publication, partial success, cancellation, collision
  handling, and injected-provider tests.
- Read-only `list-audio` CLI filtering for ALSA, PulseAudio, JACK, OSS, sndio, and audio(4),
  explicit null diagnostics, safe device-name escaping, and typed exit behavior.
- Deterministic audio discovery, identity, refresh, teardown, CLI, and real-host smoke
  tests that never initialize or start a device.
- Audio-disabled CMake preset and CI coverage retaining the full offline image/TX path.
- Fixed-capacity dependency-free mono float32 SPSC ring with acquire/release publication,
  partial operations, wrap handling, stopped-only reset, and two-thread stress coverage.
- Project-owned playback, capture, and duplex stream configuration, exact selected-device
  identity validation, negotiated facts, lifecycle errors, and immutable statistics.
- Real-time-safe bounded callback handling for explicit channel mapping, playback
  underrun silence, preserve-oldest capture overflow, fault flags, and counters.
- Injected deterministic stream adapter tests and a separately labelled miniaudio
  null-backend lifecycle test that never opens real hardware.
- Focused ThreadSanitizer preset for the hardware-free audio concurrency suite.
- Qt-free single-owner audio diagnostics service with exact-device refresh, bounded
  capture metering, low-level output calibration, deterministic loopback correlation,
  cancellation, timeout, disconnect, and immutable progress snapshots.
- `audio-meter`, `audio-output-test`, and `audio-loopback` CLI diagnostics with explicit
  backend/device/channel selection and a per-run `--arm-real-audio` output gate.
- Accessible Qt Audio Diagnostics panel with exact device selectors, channel and period
  controls, meter, fresh output warning, negotiated results, and emergency Stop.
- Hardware-free signal/statistics/correlation fixtures, injected arming tests, bounded
  null-backend meter/output integration, and a disabled-by-default audio hardware gate.
- Dependency-free PTT action, observed-state, certainty, readback, deadline, error,
  provider, serialized supervisor, safety-record, watchdog, and lease APIs.
- Mock-only application transmit coordinator with finite generated sample and audio
  endpoint seams, validated timing/retry bounds, immutable snapshots, and exact traces.
- Virtual-time fault-matrix tests covering audio readiness, watchdog gating, key
  ambiguity, readback, mandatory retries, unresolved hazards, signal gating, cancellation,
  running faults, cleanup failures, watchdog expiry, and lease destruction.

### Changed

- Completed the Scanline SSTV rename in current GUI and CMake identifiers.
- Centralised the Qt QML module URI in the GUI build definition.
- Updated mode capabilities and status text to distinguish offline diagnostic generation
  from live TX and RX.
- Replaced fixed mode capability booleans with a typed bitmask and added truthful Martin M1
  offline image TX capability without advertising live TX or receive.
- Updated Linux GCC, Clang, and Qt CI jobs to install libvips and run M1B tests; added a
  separate image-disabled minimal job.
- Martin M1 arbitrary-image TX reuses the frozen M1a encoder, timing, VIS, scheduler,
  renderer, and PCM16 writer without protocol changes.
- Registered Scottie S1 with offline test-pattern and image TX capabilities only; CLI
  mode parsing and help now cover both accepted offline modes.
- Refactored Martin M1 onto the shared sequential RGB encoder while preserving its exact
  ordered event stream, event count, duration, frame counts, compact vector, and prepared
  image equivalence.
- Registered Robot 36 with offline test-pattern and image TX capabilities only and routed
  both commands through the existing central offline TX service and frozen WAV writer.
- Registered PD120 with offline test-pattern and image TX capabilities only and routed
  both commands through the existing central offline TX service and frozen WAV writer.
- Added truthful optional offline FSK ID capability metadata to all four accepted analogue
  modes without adding live TX or receive capability.
- Enabled audio discovery and hardware-free stream/diagnostic tests in normal headless,
  development, and sanitizer builds while keeping `sstv_core` and audio-disabled offline
  behavior independent of miniaudio.

### Fixed

- Corrected the QML module URI used to load `Main.qml` at GUI startup.

### Security

- Established real-time callback and fail-safe PTT invariants before radio-control code is
  introduced.
- Enforced audio-open/prime and watchdog-arm gates before keying, signal gating until
  accepted key plus pre-key delay, and mandatory unkey cleanup for every maybe-keyed path.
- Image inputs fail closed for special files, URLs, disallowed loaders, animation,
  contradictory metadata, corrupt ICC profiles, unprofiled CMYK, and configured resource
  limits before any output is published.
- WAV inspection rejects URLs, symlinks, special files, unsupported formats, malformed or
  contradictory chunks, excessive resources, and overflow attempts while streaming
  sample analysis in fixed-size blocks.
