# Dependency policy

System packages are preferred on Linux and BSD. CMake `FetchContent` is limited to small,
pinned, permissively licensed development dependencies. Bundled code needs an update
procedure, licence copy, checksum, and attribution.

| Dependency | Purpose | Licence compatibility | Plan |
| --- | --- | --- | --- |
| Qt 6 Core/Gui/Qml/Quick/QuickControls2 | GUI and platform integration | LGPL/GPL | Required for GUI only |
| miniaudio | Cross-platform capture/playback | MIT-0/public-domain choice | Pin source snapshot |
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
