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
offline PCM16 RIFF/WAVE inspection.

### Added

- Locked C++20, Qt Quick 6, miniaudio, liquid-dsp, FFTW3f, libvips, notcurses, flrig,
  Hamlib, HamDRM, and KG-STV architecture.
- Authoritative project plan, component architecture, milestones, dependency policy,
  protocol provenance rules, and verification strategy.
- CMake foundation with headless and GUI-capable presets.
- Frontend-independent mode registry API.
- Diagnostic `scanline-sstv-cli` with version and mode-list commands.
- Optional Qt Quick GUI foundation.
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

### Fixed

- Corrected the QML module URI used to load `Main.qml` at GUI startup.

### Security

- Established real-time callback and fail-safe PTT invariants before radio-control code is
  introduced.
- Image inputs fail closed for special files, URLs, disallowed loaders, animation,
  contradictory metadata, corrupt ICC profiles, unprofiled CMYK, and configured resource
  limits before any output is published.
- WAV inspection rejects URLs, symlinks, special files, unsupported formats, malformed or
  contradictory chunks, excessive resources, and overflow attempts while streaming
  sample analysis in fixed-size blocks.
