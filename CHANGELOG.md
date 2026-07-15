# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases
will use semantic versioning once the public protocol and command-line interfaces
stabilise.

## [Unreleased]

M0.1 stabilises the foundation before analogue signal work begins.

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

### Changed

- Completed the Scanline SSTV rename in current GUI and CMake identifiers.
- Centralised the Qt QML module URI in the GUI build definition.

### Fixed

- Corrected the QML module URI used to load `Main.qml` at GUI startup.

### Security

- Established real-time callback and fail-safe PTT invariants before radio-control code is
  introduced.
