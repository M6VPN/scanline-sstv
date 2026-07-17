# Scanline SSTV Transceiver

`scanline-sstv` is the working name for a high-performance analogue and digital SSTV
transceiver for Linux and BSD. It will provide a Wayland-first Qt Quick GUI, a rich TUI,
offline command-line tools, selectable sound-card backends, and safe radio PTT control
through flrig or Hamlib.

## Status

M1 analogue TX work remains provisionally incomplete, and M2B audio transport is
complete. M1a provides the
evidence-backed offline Martin M1 waveform path. M1B adds bounded native JPEG/PNG
preparation for arbitrary source dimensions, exact-mode immutable RGB8 output, prepared
PNG export, and offline Martin M1 image-to-WAV generation. M1C adds evidence-backed
Scottie S1 test-pattern and prepared-image WAV generation through a shared sequential RGB
encoder and central offline TX dispatch. M1D adds evidence-backed Robot 36 offline
test-pattern and prepared-image WAV generation with deterministic luma and 2 by 2
subsampled red/blue colour-difference conversion. M1E adds evidence-backed PD120 offline
test-pattern and prepared-image WAV generation with paired luma rows and full-width,
vertically averaged red/blue colour-difference components. M1F adds evidence-backed
optional analogue FSK ID suffixes and a defensive read-only PCM16 RIFF/WAVE inspector.
M1G replaces the GUI placeholder with an asynchronous offline TX editor for exact
prepared-image preview, recipe control, optional FSK ID, atomic PNG/WAV export, and WAV
inspection. It does not play audio, transmit over a sound card, receive, decode SSTV,
control a radio, or key PTT.

M2A adds pinned miniaudio 0.11.25 and read-only discovery for distinct ALSA,
PulseAudio/PipeWire-Pulse, JACK/PipeWire-JACK, OSS, sndio, and audio(4) backends. It can
list playback and capture endpoints but cannot initialize a device, create a stream,
play, record, automatically select hardware, or access PTT. M1 remains incomplete because
its non-live encode/decode round-trip gate depends on the later M3 decoder; that gate has
not been removed or weakened.

M2B adds fixed-capacity mono float32 SPSC rings, explicit playback/capture/duplex stream
configuration, exact-device lifecycle control, channel mapping, and immutable stream
statistics. Automated lifecycle tests use an injected adapter or miniaudio's null backend.
There is no CLI or GUI action that opens a device, no SSTV live-audio path, and no PTT or
radio-control integration.

Martin M1, Scottie S1, Robot 36, and PD120 advertise `offline-test-pattern-tx`,
`offline-image-tx`, and `optional-fsk-id`. Overall M1 is not complete.

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
- POSIX threads and dynamic-loading support used by the pinned miniaudio discovery and
  stream adapters on supported Linux/BSD systems.

Qt 6.5 or newer with Concurrent, Core, Gui, Qml, Quick, QuickControls2, and Test remains
optional for headless builds. libvips is required by the normal `dev` and `headless`
presets. If Qt is
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

For the complete offline image/TX path without audio discovery:

    cmake --preset audio-disabled
    cmake --build --preset audio-disabled
    ctest --preset audio-disabled

Current offline commands:

    ./build/headless/apps/cli/scanline-sstv-cli --version
    ./build/headless/apps/cli/scanline-sstv-cli --list-modes
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode martin-m1 --output martin-m1.wav
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode martin-m1 --output martin-m1-44100.wav --sample-rate 44100
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode scottie-s1 --output scottie-s1.wav
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode robot-36 --output robot-36.wav
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode pd-120 --output pd-120.wav
    ./build/headless/apps/cli/scanline-sstv-cli encode-test-pattern \
        --mode martin-m1 --output martin-m1-id.wav --fsk-id M6VPN
    ./build/headless/apps/cli/scanline-sstv-cli prepare-image \
        --mode martin-m1 --input source.png --output prepared.png \
        --fit contain --background 000000
    ./build/headless/apps/cli/scanline-sstv-cli prepare-image \
        --mode scottie-s1 --input source.png --output scottie-s1-prepared.png \
        --fit contain --background 000000
    ./build/headless/apps/cli/scanline-sstv-cli prepare-image \
        --mode robot-36 --input source.png --output robot-36-prepared.png \
        --fit contain --background 000000
    ./build/headless/apps/cli/scanline-sstv-cli prepare-image \
        --mode pd-120 --input source.png --output pd-120-prepared.png \
        --fit contain --background 000000
    ./build/headless/apps/cli/scanline-sstv-cli encode-image \
        --mode martin-m1 --input source.jpg --output martin-m1-image.wav \
        --fit cover --sample-rate 48000
    ./build/headless/apps/cli/scanline-sstv-cli encode-image \
        --mode scottie-s1 --input source.jpg --output scottie-s1-image.wav \
        --fit cover --sample-rate 48000
    ./build/headless/apps/cli/scanline-sstv-cli encode-image \
        --mode robot-36 --input source.jpg --output robot-36-image.wav \
        --fit cover --sample-rate 48000
    ./build/headless/apps/cli/scanline-sstv-cli encode-image \
        --mode pd-120 --input source.jpg --output pd-120-image.wav \
        --fit cover --sample-rate 48000
    ./build/headless/apps/cli/scanline-sstv-cli encode-image \
        --mode scottie-s1 --input source.png --output scottie-s1-id.wav \
        --fsk-id M6VPN
    ./build/headless/apps/cli/scanline-sstv-cli inspect-wav \
        --input scottie-s1-id.wav
    ./build/headless/apps/cli/scanline-sstv-cli list-audio
    ./build/headless/apps/cli/scanline-sstv-cli list-audio --backend alsa
    ./build/headless/apps/cli/scanline-sstv-cli list-audio --backend pulseaudio
    ./build/headless/apps/cli/scanline-sstv-cli list-audio --backend jack
    ./build/headless/apps/cli/scanline-sstv-cli list-audio --include-null

The image and WAV commands refuse to overwrite an existing output unless `--force` is
supplied. FSK identifiers contain one to nine evidence-compatible characters; lowercase
ASCII letters are normalized to uppercase and invalid input is rejected without
truncation. `inspect-wav` accepts bounded nonsymlink regular local mono PCM16 files at
project-supported rates and reports container metadata and sample statistics. It is not
an SSTV decoder or mode detector. Preparation, generation, and inspection are offline
only and never start playback or PTT.

`list-audio` probes each requested backend independently. Success means at least one real
backend was enumerated, even if it reported zero devices. Per-backend failures remain in
the output. Null is opt-in and never counts as hardware success. Device identities include
backend and direction; PulseAudio API names may be persistent, while other current IDs
are conservatively session-only. Transport remains `unknown` unless authoritative
metadata exists, and display-name text is never used to infer USB. Pinned sndio
enumeration opens endpoints, so M2A reports it as safe-enumeration-unsupported instead of
opening the device.

M2B's public audio API is under `include/sstv/audio`. `FloatSpscRing` provides bounded
mono sample transport, while `AudioStream` owns exact-device open, prime, start, stop,
close, callback, negotiated-format, and statistics state. These APIs are not connected to
the CLI or GUI in this milestone.

The Qt application provides the same offline image workflow without duplicating protocol
or image logic:

    ./build/dev/apps/gui/scanline-sstv-gui

Choose a local JPEG or PNG, select the mode and preparation recipe, then export the exact
prepared PNG or an offline PCM16 WAV. The editor inspects an exported WAV through the
same defensive service used by the CLI. Existing destinations require explicit
confirmation. No GUI action opens an audio device or accesses radio control.

## Repository map

- `include/sstv/core` - stable, frontend-independent public interfaces.
- `src/core` - shared core implementation.
- `include/sstv/image` and `src/image` - bounded libvips raster preparation.
- `include/sstv/app` and `src/app` - frontend-independent offline editor workflow.
- `include/sstv/audio` and `src/audio` - discovery, bounded rings, and stream lifecycle.
- `include/sstv/audio` and `src/audio` - frontend-independent read-only audio discovery.
- `apps/cli` - offline and diagnostic command line.
- `apps/gui` - Qt Quick application.
- `docs` - architecture, milestones, protocol provenance, dependencies, and testing.
- `tests` - deterministic tests that cannot key hardware by default.

## Licence

Copyright holders license this project under GPL-3.0-or-later. See `LICENSE`.
