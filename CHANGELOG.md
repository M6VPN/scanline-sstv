# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases
will use semantic versioning once the public protocol and command-line interfaces
stabilise.

## [Unreleased]

M1a adds the first evidence-backed offline analogue waveform slice. Overall M1 remains in
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

### Changed

- Completed the Scanline SSTV rename in current GUI and CMake identifiers.
- Centralised the Qt QML module URI in the GUI build definition.
- Updated mode capabilities and status text to distinguish offline diagnostic generation
  from live TX and RX.

### Fixed

- Corrected the QML module URI used to load `Main.qml` at GUI startup.

### Security

- Established real-time callback and fail-safe PTT invariants before radio-control code is
  introduced.
