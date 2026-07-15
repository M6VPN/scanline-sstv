# Changelog

All notable changes to this project will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and releases
will use semantic versioning once the public protocol and command-line interfaces
stabilise.

## [Unreleased]

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
- GPL-3.0-or-later project licensing.

### Security

- Established real-time callback and fail-safe PTT invariants before radio-control code is
  introduced.

