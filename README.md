# Scanline SSTV Transceiver

`scanline-sstv` is the working name for a high-performance analogue and digital SSTV
transceiver for Linux and BSD. It will provide a Wayland-first Qt Quick GUI, a rich TUI,
offline command-line tools, selectable sound-card backends, and safe radio PTT control
through flrig or Hamlib.

## Status

M1 analogue TX work remains provisionally incomplete. M2I is complete, and M2J-A adds a
hardware-free staged evidence harness for later supervised physical HIL. M1a provides the
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
M2B itself has no user-facing open action, SSTV live-audio path, PTT, or radio-control
integration.

M2C adds explicitly selected real-device diagnostics: bounded input metering, a freshly
armed low-level 1 kHz interface-calibration signal, and deterministic local cable
loopback. It never plays SSTV or WAV content. Output defaults to -30 dBFS, is limited to
-6 dBFS, and cannot run longer than 10 seconds. M1 and M2 remain incomplete.

M2D adds dependency-free PTT certainty values, a serialized injected provider boundary,
mandatory unkey retries, an RAII transmit lease, an independent heartbeat watchdog, and
a mock-only application coordinator. Audio must be open, silently prefilled, and primed
before the watchdog can arm or keying can begin. Only generated test values can reach its
injected mock endpoint, and no CLI or GUI Transmit action exists. M2D provides no real
audio adapter, PTT provider, socket, serial access, radio path, or SSTV playback.

M2E adds a strict flrig XML-RPC provider pinned to flrig 2.0.11. It accepts only an
explicit literal loopback address and ephemeral or explicit port, performs mandatory
`rig.get_ptt` readback after set operations, and adds a definitely-unkeyed coordinator
preflight before audio opens. It has no CLI or GUI configuration, no real-flrig or
real-radio enablement, and no SSTV playback path.

M2F adds a strict Hamlib rigctld provider pinned to Hamlib 4.7.1 protocol evidence. It
uses only fixed Extended Response Protocol PTT commands against explicit literal
loopback endpoints and requires `get_ptt` readback after every set. The flrig and rigctld
providers share private bounded POSIX socket mechanics while retaining separate wire
parsers. No rigctld process, libhamlib, serial device, real radio, CLI/GUI transmit action,
or SSTV playback path is enabled.

M2G connects the canonical offline tone-event renderer to the M2D coordinator through an
exact-device M2B AudioStream adapter. Automated end-to-end tests use injected audio,
miniaudio's null backend, mock PTT, and the existing loopback-only rig-provider suites.
The callback has a one-way fault gate that silences and discards queued signal from the
next callback boundary. M2G itself enabled no real device or radio endpoint and added no
CLI or GUI transmit action.

M2H adds the first user-invocable live path as a separately compiled `transmit-image`
command. Normal builds omit it. A live-enabled build requires an exact device and channel,
explicit -60 to -6 dBFS software gain, an explicit literal-loopback flrig or rigctld
endpoint, three fresh arm flags, and a foreground interactive confirmation. The command
has no defaults or fallback and reuses the M2D safety coordinator. There is no GUI/TUI
Transmit action, remote rig control, direct libhamlib, WAV playback, batch mode, or
unattended bypass. Mode capability flags remain protocol metadata and do not claim
verified physical hardware compatibility.

M2I adds a conditional Qt Quick live-image workflow over the same Qt-free application
service now used by the CLI. It requires an exact prepared revision, refreshed exact
device identity, explicit channels and -60 to -6 dBFS gain, explicit literal-loopback
flrig or rigctld configuration, bounded delays, three fresh acknowledgements, and the
exact single-use phrase. Stop, window close, and process signals retain coordinator
unkey cleanup. Normal builds contain no live panel. Automated tests use injected or null
audio and loopback-only providers; M2 remains incomplete pending M2J physical HIL.

M2J-A adds versioned JSON/Markdown evidence, explicit result and evidence-source states,
stage/resource isolation, single-use configuration-bound permits, atomic local output,
and a frozen Robot 36 reference fixture. The old all-at-once manual target is removed.
No physical stage was run, no physical evidence is included, and overall M2 remains in
progress. See [the M2J runbook](docs/hil/M2J_RUNBOOK.md).

M2J-B1 adds the digest-bound `hil-stage discovery` command and keeps PTT readiness
restricted to injected or loopback providers. It does not select a device or run a
physical stage.

M2J-B2 adds atomic native-fact recording for the manually confirmed Stage 1 discovery
and a real-flrig Stage 3 query/unkey-only path behind all live/HIL build gates. It does
not open audio, key PTT, or contact the installed flrig during automated verification.

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
    ./build/headless/apps/cli/scanline-sstv-cli audio-meter \
        --backend alsa --capture-id ID --channel 0 --duration 5
    ./build/headless/apps/cli/scanline-sstv-cli audio-output-test \
        --backend alsa --playback-id ID --channel 0 --level-dbfs -30 \
        --duration 2 --arm-real-audio
    ./build/headless/apps/cli/scanline-sstv-cli audio-loopback \
        --backend alsa --playback-id ID --capture-id ID \
        --output-channel 0 --input-channel 0 --level-dbfs -30 \
        --arm-real-audio

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

M2C diagnostics refresh the chosen backend immediately before opening and require the
exact IDs printed by `list-audio`; names and defaults are never selectors. Output and
loopback require `--arm-real-audio` for every invocation. The flag is not stored. Before
arming, disconnect radio transmit audio where practical, disable VOX, verify no external
PTT is asserted, reduce monitor/headphone volume, and use a local cable or dummy audio
load. Input metering captures transient sample values for bounded in-memory statistics;
it writes no recording but still exposes local microphone/interface audio to this process.

Loopback reports correlation, apparent round-trip latency, gain, polarity, clipping, and
stream discontinuities. Its deterministic marker is an interface diagnostic, not SSTV,
and results are not laboratory-grade frequency-response measurements. Silence, ambiguous
peaks, missing samples, underruns, and drops are reported rather than automatically
changing gain, periods, or channels.

M2D's public PTT API is under `include/sstv/rig`; transmit orchestration is under
`include/sstv/app`. A provider result distinguishes definite keyed, definite unkeyed, and
indeterminate state. Once keying may have begun, cancellation, failure, shutdown, and
destruction all retain mandatory unkey ownership. Failed confirmation remains a visible
hazard and blocks another session. The watchdog uses injected monotonic time and a
serialized provider, and it remains armed until definite unkey or a recorded hazard.
M2E connects the same supervisor to a loopback-only flrig provider for tests. It remains
disconnected from miniaudio, offline SSTV encoders, CLI commands, and GUI controls.
M2F adds the second explicit provider through the same supervisor and coordinator, using
only a test server bound to an ephemeral loopback port. It does not add automatic provider
selection or a conventional rigctld endpoint.
M2G adds the production finite tone-event source and exact AudioStream endpoint adapter to
that application boundary. They are reachable only through tests in this slice and do not
create a user-facing or real-radio transmit path.

Hardware-in-loop testing is disabled by default. `SSTV_ENABLE_AUDIO_HARDWARE_TESTS=ON`
does nothing audible unless `SSTV_ARM_AUDIO_HARDWARE_TESTS=ON` and explicit backend,
playback/capture IDs, channel indices, and channel counts are also configured. CI never
sets these values. See [the testing guide](docs/TESTING.md) for the manual checklist.

Stage 0 manifest generation is hardware-free and available in image-capable builds:

    scanline-sstv-cli hil-manifest --output-dir HIL_DIRECTORY [explicit metadata]

It writes versioned JSON and Markdown atomically to an existing nonsymlink directory and
records zero resource acquisitions. Required fields and placeholder-only command shape
are documented in [the M2J runbook](docs/hil/M2J_RUNBOOK.md). Do not store real arm values
in a reusable command.

Compile the live CLI path without running hardware:

    cmake --preset live-tx-compile
    cmake --build --preset live-tx-compile
    ctest --preset live-tx-compile --output-on-failure

Compile the live CLI and Qt panel for hardware-free offscreen tests:

    CMAKE_PREFIX_PATH=/opt/Qt/6.11.1/gcc_64 cmake --preset live-tx-gui-compile
    cmake --build --preset live-tx-gui-compile
    ctest --preset live-tx-gui-compile --output-on-failure

The command syntax in that build is:

    ./build/live-tx-compile/apps/cli/scanline-sstv-cli transmit-image \
        --mode martin-m1 --input source.png \
        --backend alsa --playback-id ID --output-channel 0 \
        --playback-channels 2 --ptt-provider flrig \
        --ptt-address 127.0.0.1 --ptt-port PORT --flrig-path /RPC2 \
        --pre-key-ms 250 --post-audio-ms 250 --gain-dbfs -30 \
        --arm-real-audio --arm-automatic-ptt --arm-live-tx

This example is syntax only. Software gain is not radio deviation calibration. The command
prepares the full immutable transmission before prompting and makes no audio or PTT
acquisition unless the exact phrase is entered on a foreground TTY.

The normal Qt application provides the offline image workflow without live controls:

    ./build/dev/apps/gui/scanline-sstv-gui

Choose a local JPEG or PNG, select the mode and preparation recipe, then export the exact
prepared PNG or an offline PCM16 WAV. The editor inspects an exported WAV through the
same defensive service used by the CLI. Existing destinations require explicit
confirmation. The separate Audio Diagnostics panel uses the same bounded service and
fresh arm warning. With `SSTV_ENABLE_LIVE_TX=OFF`, no GUI action accesses radio control
or PTT. A live-enabled build adds the separately gated panel, but compiling it does not
arm audio or PTT and no unattended or noninteractive bypass exists.

## Repository map

- `include/sstv/core` - stable, frontend-independent public interfaces.
- `src/core` - shared core implementation.
- `include/sstv/image` and `src/image` - bounded libvips raster preparation.
- `include/sstv/app` and `src/app` - offline editor and safe transmit orchestration adapters.
- `include/sstv/audio` and `src/audio` - discovery, bounded rings, streams, and diagnostics.
- `include/sstv/rig` and `src/rig` - PTT safety plus bounded loopback flrig and rigctld providers.
- `apps/cli` - offline and diagnostic command line.
- `apps/gui` - Qt Quick application.
- `docs` - architecture, milestones, protocol provenance, dependencies, and testing.
- `tests` - deterministic tests that cannot key hardware by default.

## Licence

Copyright holders license this project under GPL-3.0-or-later. See `LICENSE`.
