# Codex repository instructions

These instructions apply to the complete repository.

## Read before changing code

Read these files in order:

1. `docs/PLAN.md` — authoritative product scope and locked technology choices.
2. `docs/ARCHITECTURE.md` — module, thread, audio, DSP, and PTT boundaries.
3. `docs/MILESTONES.md` — current milestone and acceptance criteria.
4. `docs/PROTOCOLS.md` — protocol provenance and interoperability rules.
5. `docs/TESTING.md` — required verification.
6. `CHANGELOG.md` — user-visible changes already recorded.

If implementation and documentation disagree, stop and reconcile them. Do not silently
change a locked decision.

## Locked decisions

- C++20 and CMake.
- Qt Quick 6 GUI, native Wayland first, XCB/XWayland fallback.
- miniaudio audio abstraction.
- liquid-dsp streaming primitives and FFTW3f spectral processing.
- libvips image-processing engine.
- notcurses TUI.
- flrig XML-RPC first, Hamlib `rigctld` second, direct libhamlib optional.
- HamDRM before KG-STV.
- GPL-3.0-or-later.

## Non-negotiable engineering rules

- The audio callback must not allocate, lock, log, perform file/network I/O, invoke rig
  control, or call UI code.
- Audio callbacks only move samples through preallocated bounded buffers and update
  lock-free counters.
- DSP and UI state communicate using explicit snapshots or bounded queues. Never expose a
  mutable image buffer concurrently to the decoder and renderer.
- Keep `sstv_core` independent of Qt, QML, notcurses, and any particular audio server.
- Never key PTT until the output stream is open, primed, and able to deliver audio.
- Every exit, exception, cancellation, underrun, backend disconnect, and signal path must
  initiate unkeying. PTT implementations require watchdog tests.
- Do not add on-air timing constants from memory. Every mode descriptor needs a traceable
  source and golden encode/decode vectors.
- Preserve unknown audio devices and backends; do not assume device zero is safe.
- Tests and offline WAV paths precede live RF transmission for every new encoder.
- No automated test may key real hardware unless an explicit hardware-in-loop option is
  enabled.
- Add SPDX identifiers to source files. Record copied or adapted GPL code and attribution.
- Update `CHANGELOG.md` and the relevant planning document with material changes.

## Normal development loop

Use the narrowest active milestone. Build and test headlessly first:

    cmake --preset headless
    cmake --build --preset headless
    ctest --preset headless

Then test the GUI where Qt is available:

    cmake --preset dev
    cmake --build --preset dev
    ctest --preset dev

Use Clang or GCC sanitizers for parser, image, and offline decoder work. Real-time
performance claims require the reproducible benchmark and impairment corpus described in
`docs/TESTING.md`.

## Scope discipline

The current milestone is M0. M0 supplies architecture, build structure, a core API seam,
a diagnostic CLI, and an optional GUI shell. It does not produce or decode on-air audio.
Complete milestone acceptance criteria in order unless the user explicitly changes
priority.

