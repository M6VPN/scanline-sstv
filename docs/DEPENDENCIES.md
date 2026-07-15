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
| libvips | Image load and processing graph | LGPL-2.1-or-later | System package |
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

