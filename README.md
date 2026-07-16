# Scanline SSTV Transceiver

`scanline-sstv` is the working name for a high-performance analogue and digital SSTV
transceiver for Linux and BSD. It will provide a Wayland-first Qt Quick GUI, a rich TUI,
offline command-line tools, selectable sound-card backends, and safe radio PTT control
through flrig or Hamlib.

## Status

The project is at **M1: verified analogue TX and image preparation**. M1a provides the
evidence-backed offline Martin M1 waveform path. M1B adds bounded native JPEG/PNG
preparation for arbitrary source dimensions, exact-mode immutable RGB8 output, prepared
PNG export, and offline Martin M1 image-to-WAV generation. It does not play audio,
transmit, receive, decode, control a radio, or key PTT.

Martin M1 advertises only `offline-test-pattern-tx` and `offline-image-tx`. Overall M1 is
not complete.

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
- libvips 8.15 or newer with the `vips-cpp` pkg-config package.

Qt 6.5 or newer with Core, Gui, Qml, Quick, and QuickControls2 remains optional for
headless builds. libvips is required by the normal `dev` and `headless` presets. If Qt is
not found, CMake builds the core, CLI, image module, and tests and reports that
the GUI was skipped.

    cmake --preset dev
    cmake --build --preset dev
    ctest --preset dev

For a machine without Qt:

    cmake --preset headless
    cmake --build --preset headless
    ctest --preset headless

For a core-only build without Qt or libvips:
    cmake --preset minimal

    cmake --build --preset minimal
    ctest --preset minimal

Current offline commands:

    ./build/headless/apps/cli/scanline-sstv-cli --version
    ./build/headless/apps/cli/scanline-sstv-cli --list-modes
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode martin-m1 --output martin-m1.wav
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode martin-m1 --output martin-m1-44100.wav --sample-rate 44100
    ./build/headless/apps/cli/scanline-sstv-cli prepare-image \
        --mode martin-m1 --input source.png --output prepared.png \
        --fit contain --background 000000
    ./build/headless/apps/cli/scanline-sstv-cli encode-image \
        --mode martin-m1 --input source.jpg --output martin-m1-image.wav \
        --fit cover --sample-rate 48000

The image and WAV commands refuse to overwrite an existing output unless `--force` is
supplied.
Preparation and generation are offline only and never start playback or PTT.

## Repository map

- `include/sstv/core` - stable, frontend-independent public interfaces.
- `src/core` - shared core implementation.
- `include/sstv/image` and `src/image` - bounded libvips raster preparation.
- `apps/cli` - offline and diagnostic command line.
- `apps/gui` - Qt Quick application.
- `docs` - architecture, milestones, protocol provenance, dependencies, and testing.
- `tests` - deterministic tests that cannot key hardware by default.

## Licence

Copyright holders license this project under GPL-3.0-or-later. See `LICENSE`.
