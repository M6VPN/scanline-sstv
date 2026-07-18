# Dependency policy

System packages are preferred on Linux and BSD. CMake `FetchContent` is limited to small,
pinned, permissively licensed development dependencies. Bundled code needs an update
procedure, licence copy, checksum, and attribution.

| Dependency | Purpose | Licence compatibility | Plan |
| --- | --- | --- | --- |
| Qt 6 Core/Gui/Qml/Quick/QuickControls2 | GUI and platform integration | LGPL/GPL | Required for GUI only |
| miniaudio 0.11.25 | Read-only backend/device discovery in M2A | MIT No Attribution alternative | Pinned source snapshot |
| liquid-dsp | Streaming DSP/modem/FEC primitives | MIT | System or pinned source |
| FFTW3 single precision | FFT/STFT/correlation | GPL-2.0-or-later | System package |
| libvips 8.15+ with `vips-cpp` | M1B raster preparation graph | LGPL-2.1-or-later | Required system package when image support is enabled |
| OpenJPEG 2 | HamDRM JPEG 2000 payloads | BSD-2-Clause | M7 system package |
| notcurses | TUI and terminal images | Apache-2.0/MIT | TUI system package |
| Hamlib | Optional direct rig control | LGPL-2.1-or-later library | Optional system package |
| toml++ | Shared configuration parser | MIT | Pinned header dependency |
| spdlog/fmt | Non-real-time structured diagnostics | MIT | System or pinned |
| CLI11 | Command-line parsing | BSD-3-Clause | Pinned header dependency |
| doctest | Unit-test harness | MIT | Pinned test-only dependency |

flrig and `rigctld` are external processes accessed through documented network protocols;
they are not embedded.

Minimum versions beyond the locked Qt/CMake baseline will be set only when an API is used
and verified in Linux and BSD package repositories. Avoid unnecessary “latest version”
requirements.

FFTW wisdom is an optimisation cache, not a portable correctness input. Tests must pass
without saved wisdom.


## M1B image dependency

`SSTV_BUILD_IMAGE` defaults to `ON`. CMake resolves the system `vips-cpp` pkg-config
target and fails with a clear error when libvips 8.15 or newer is unavailable. Only
`sstv_image` links libvips; core, DSP, analogue protocol, and frontend-independent frame
interfaces do not expose or link `VImage`.

The normal `headless` and `dev` presets build image support. The `minimal` preset sets
`SSTV_BUILD_IMAGE=OFF` for a core-only build:

```sh
cmake --preset minimal
cmake --build --preset minimal
ctest --preset minimal
```

M1B permits only native libvips JPEG and PNG loaders, selected from file content rather
than extension. SVG, PDF, ImageMagick fallback loaders, WebP, TIFF, animated or multipage
images, URLs, and non-regular filesystem inputs are rejected. WebP and TIFF remain
possible later additions only after their page and frame behavior has dedicated tests.

Default input limits are 64 MiB per file, 16,384 pixels per source axis, and 64 million
source pixels. One page or frame is permitted. Output axes are limited to 4,096 pixels.
The byte limit bounds compressed input, the axis and pixel limits stop oversized headers
and decompression-bomb-style workloads before full materialization, the single-page limit
excludes animation and document-like rasters, and the output limit bounds the only pixel
buffer owned outside libvips. Martin M1 uses 320 by 256 within that output ceiling.

## M2A audio dependency

Miniaudio release 0.11.25 is pinned at commit
`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`. The project imports only `miniaudio.h`,
`miniaudio.c`, and the complete upstream dual-licence text. Scanline SSTV uses the MIT
No Attribution alternative. Retrieval metadata, SHA-256 hashes, enabled facilities, and
the update procedure are recorded in `third_party/miniaudio/README.md`.

The build enables ALSA, PulseAudio, JACK, OSS, sndio, audio(4), and null backends only.
Decoder, encoder, codec, resource-manager, node-graph, engine, and signal-generation
facilities are disabled. Exactly one implementation translation unit includes the
vendored `miniaudio.c`. `sstv_audio` links `Threads::Threads` and `${CMAKE_DL_LIBS}`;
`sstv_core` has no miniaudio dependency.

`SSTV_BUILD_AUDIO` defaults to `ON` for headless, development, ASan, and UBSan builds.
The `minimal` preset disables image and audio support. The `audio-disabled` preset keeps
the complete offline image and waveform path while disabling audio discovery:

```sh
cmake --preset audio-disabled
cmake --build --preset audio-disabled
ctest --preset audio-disabled
```

`SSTV_BUILD_RIG` defaults to `ON` when normal audio builds are configured. Setting it to
`OFF` excludes the M2D coordinator and M2E loopback provider while retaining audio
discovery/diagnostics and all offline image and waveform functions. M2E adds no HTTP or
XML library: its narrow client uses bounded C++20 and POSIX socket facilities on Linux
and BSD.

M2A uses context initialization and read-only enumeration only. It does not initialize,
start, capture from, or play to a device. ALSA detail queries are intentionally omitted
because the pinned implementation opens PCM endpoints. Pinned sndio enumeration opens
audio endpoints, so that backend returns a typed safe-enumeration-unsupported result.
PulseAudio and JACK detail queries use server metadata or a non-activated JACK client;
JACK server autostart is disabled. Linux PipeWire is represented truthfully through its
PulseAudio or JACK compatibility service, not as a native miniaudio PipeWire backend.

M2B does not change the pinned files or compile-time facility set. Its private stream
adapter uses miniaudio's low-level context and device APIs with float32 callback buffers,
explicit device IDs, fixed requested periods, PulseAudio autospawn disabled, and JACK
server startup disabled. Decoder, engine, resource-manager, node-graph, and generation
facilities remain disabled. Only the opt-in null-backend integration test initializes a
device automatically; deterministic tests use an injected adapter, and no user-facing
M2B command opens real hardware.
