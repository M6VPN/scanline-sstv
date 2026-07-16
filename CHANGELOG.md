# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases
will use semantic versioning once the public protocol and command-line interfaces
stabilise.

## [Unreleased]

M1a adds the first evidence-backed offline analogue waveform slice. M1B adds safe offline
raster preparation and Martin M1 image-to-WAV generation. Overall M1 remains in
progress.

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

### Fixed

- Corrected the QML module URI used to load `Main.qml` at GUI startup.

### Security

- Established real-time callback and fail-safe PTT invariants before radio-control code is
  introduced.
- Image inputs fail closed for special files, URLs, disallowed loaders, animation,
  contradictory metadata, corrupt ICC profiles, unprofiled CMYK, and configured resource
  limits before any output is published.
