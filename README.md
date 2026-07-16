# Scanline SSTV Transceiver

`scanline-sstv` is the working name for a high-performance analogue and digital SSTV
transceiver for Linux and BSD. It will provide a Wayland-first Qt Quick GUI, a rich TUI,
offline command-line tools, selectable sound-card backends, and safe radio PTT control
through flrig or Hamlib.

## Status

The project is at **M1: verified analogue TX and image preparation** after completing
M0.1 foundation stabilization. The architecture, safety boundaries, milestone plan, core
API seam, diagnostic CLI, smoke tests, and optional Qt GUI shell exist. No M1 signal
generation work is included yet, so the project cannot generate, transmit, receive, or
decode an SSTV signal.

Do not connect this foundation build to a transmitter expecting functional PTT or audio.

## Locked stack

- C++20 with CMake and Ninja.
- Qt Quick 6 for native Wayland and XCB/XWayland GUI operation.
- miniaudio for ALSA, PulseAudio/PipeWire-Pulse, JACK/PipeWire-JACK, OSS, sndio, and
  audio(4).
- liquid-dsp and custom SSTV DSP, with FFTW3f for spectral work.
- libvips for non-destructive image preparation.
- notcurses for terminal graphics.
- flrig XML-RPC, Hamlib `rigctld`, and optional direct libhamlib control.
- HamDRM digital SSTV followed by KG-STV.
- GPL-3.0-or-later.

See [the project plan](docs/PLAN.md) and [milestones](docs/MILESTONES.md).

## Build the foundation

Required:

- CMake 3.24 or newer.
- Ninja.
- A C++20 compiler.

Qt 6.5 or newer with Core, Gui, Qml, Quick, and QuickControls2 remains optional for
headless builds. If Qt is not found, CMake builds the core, CLI, and tests and reports that
the GUI was skipped.

    cmake --preset dev
    cmake --build --preset dev
    ctest --preset dev

For a machine without Qt:

    cmake --preset headless
    cmake --build --preset headless
    ctest --preset headless

Current diagnostic commands:

    ./build/headless/apps/cli/scanline-sstv-cli --version
    ./build/headless/apps/cli/scanline-sstv-cli --list-modes

The mode list remains intentionally empty until verified M1 mode descriptors and vectors
are added.

## Repository map

- `include/sstv/core` — stable, frontend-independent public interfaces.
- `src/core` — shared core implementation.
- `apps/cli` — offline and diagnostic command line.
- `apps/gui` — Qt Quick application.
- `docs` — architecture, milestones, protocol provenance, dependencies, and testing.
- `tests` — deterministic tests that cannot key hardware by default.

## Licence

Copyright holders license this project under GPL-3.0-or-later. See `LICENSE`.
