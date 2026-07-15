# Scanline SSTV Transceiver base plan

Status: **Approved architecture; M0.1 stabilization pending Qt 6.5+ verification**

This is the authoritative base plan for Codex and human contributors. Detailed component
rules are in `ARCHITECTURE.md` and completion gates are in `MILESTONES.md`.

## 1. Product objective

Build a reliable, high-performance SSTV transceiver for Linux, FreeBSD, OpenBSD, and
NetBSD with:

- Analogue SSTV transmission and reception compatible with widely used MMSSTV/QSSTV
  modes.
- HamDRM digital SSTV interoperability, followed by KG-STV.
- Progressive, low-latency image display while a signal is received.
- Selectable USB and built-in audio devices across PipeWire, JACK, PulseAudio, ALSA, and
  native BSD audio systems.
- flrig XML-RPC and Hamlib-based PTT/radio control.
- A Wayland-first graphical interface with XCB/XWayland fallback.
- A TUI capable of actual terminal images where supported and a portable cell renderer
  elsewhere.
- Non-destructive preparation of arbitrary source images for the selected transmission
  mode.
- Offline WAV encode/decode and deterministic diagnostic tools.

The application must remain useful without CAT control: manual PTT and VOX workflows will
be available, but automatic PTT must always be fail-safe.

## 2. Locked decisions

| Layer | Decision |
| --- | --- |
| Language | C++20 |
| Build | CMake 3.24+, Ninja preferred |
| Licence | GPL-3.0-or-later |
| GUI | Qt Quick/QML 6.5+ with C++ scene-graph items |
| Window systems | Native Wayland first; Qt XCB under X11/XWayland fallback |
| TUI | notcurses with pixel protocol and Unicode-cell fallbacks |
| Audio | miniaudio low-level API and explicit backend/device enumeration |
| Streaming DSP | liquid-dsp plus project-owned SSTV algorithms |
| Spectral DSP | FFTW3f plans created outside real-time paths |
| Images | libvips processing graph; Qt interactive composition |
| Rig control | flrig XML-RPC, then rigctld, then optional direct libhamlib |
| Digital SSTV | HamDRM first, KG-STV second |
| Configuration | Versioned TOML in XDG paths; platform-native equivalents where needed |

Qt, notcurses, audio-server, and rig-control types must not leak into `sstv_core`.

## 3. Deliverables

The project will produce:

- `scanline-sstv-gui` — full desktop transceiver.
- `scanline-sstv-tui` — terminal transceiver sharing the same application services.
- `scanline-sstv-cli` — image preparation, WAV generation, WAV/recording decode, diagnostics, and
  benchmarks.
- `libsstv_core` — frontend-independent mode, image, DSP, and orchestration APIs.
- A versioned test corpus containing only redistributable or generated recordings.
- Packaging for Arch Linux first, followed by Debian-family Linux, Flatpak, FreeBSD ports,
  OpenBSD ports, and NetBSD pkgsrc.

## 4. Functional scope

### 4.1 Receive

- Capture mono audio as float32, normally at 48 kHz.
- Explicit input backend/device/channel selection with stable saved preferences.
- USB-device prioritisation only for first-run suggestions; never silently replace an
  explicit user choice.
- Live level meter, waveform, spectrum, waterfall, sync confidence, VIS confidence, AFC
  offset, line timing, and decoder status.
- Automatic VIS detection plus manual mode override.
- Progressive image updates, manual and automatic slant correction, AFC, optional AGC,
  and configurable filtering.
- Autosave, metadata sidecar, gallery, duplicate protection, and raw-audio capture.
- Replay a recording through exactly the same decoder path used for live audio.

### 4.2 Transmit

- Load common raster, vector, camera, and clipboard inputs through approved loaders.
- Honour EXIF orientation and colour profiles.
- Non-destructive crop, rotate, flip, scale, fit/fill, gamma, contrast, saturation,
  sharpen, denoise, and safe-level operations.
- Text and image layers for callsign, locator, frequency, rig, antenna, timestamp, and
  reusable templates.
- Mode-aware preview, pixel dimensions, duration, expected audio bandwidth, and FSK ID.
- Generate and inspect WAV audio before any new mode is allowed to use live PTT.
- Explicit output backend/device/channel, level calibration, pre-key delay, post-audio
  tail, and cancellation.
- No limiter or normaliser may silently alter tone relationships during a transmission.

### 4.3 Rig control

- flrig XML-RPC is the normal default because it can own the serial radio connection and
  serve multiple applications.
- `rigctld` is the normal Hamlib route.
- Direct libhamlib is optional for users who deliberately give this application exclusive
  control of the device.
- PTT provider selection, connection test, readback where supported, configurable
  pre/post delays, retry policy, and an unkey watchdog.
- Frequency and mode display are useful but not prerequisites for PTT.

### 4.4 Interfaces

The GUI and TUI expose the same receive, transmit, gallery, device, and rig service
models. Reduced terminal resolution may simplify plots, but it must not create a separate
decoder or configuration format.

The GUI uses C++ scene-graph items for:

- The progressively decoded image.
- A circular waterfall texture updated in bounded batches.
- Min/max-decimated waveform geometry.
- Spectrum and confidence overlays.

The TUI selects Kitty, Sixel, or iTerm2-style pixel output through notcurses when
available, then falls back to true-colour half-block/quadrant/cell rendering.

## 5. Analogue mode target

The initial families are:

- Martin M1/M2.
- Scottie S1/S2/DX.
- Robot 36/72.
- PD 50/90/120/160/180/240/290.
- Pasokon P3/P5/P7.
- Common Wraase and monochrome modes after protocol verification.

Mode support is data-driven, but modes with different colour sequencing or synchronisation
may supply specialised codec strategies. A descriptor is not considered supported until
both TX and RX golden vectors pass.

## 6. Digital mode target

### HamDRM

Target interoperability with QSSTV and EasyPal-compatible HamDRM image/file transfers.
Implement the DRM physical and framing layers behind a digital-codec interface. Expected
building blocks include OFDM synchronisation, FAC/SDC/MSC handling, interleaving, QAM,
CRC/FEC, Reed-Solomon processing, and JPEG/JPEG 2000 payloads. Exact combinations come
from captured interoperability vectors and attributed GPL-compatible references.

### KG-STV

After HamDRM acceptance, implement KG-STV-compatible block transfer with MSK and 4LFSK,
progressive 16-by-16 block reconstruction, repeated-block combining, text, and missing
block reporting where supported by the protocol.

No new proprietary digital mode is in the 1.0 critical path.

## 7. Performance and reliability

- The audio callback performs bounded sample copies only.
- DSP must run continuously at 48 kHz without deadline misses on the eventual documented
  minimum reference machine.
- GUI rendering is rate-limited independently of DSP; a slow compositor cannot stall
  decoding.
- Waterfall rendering targets a stable 30 frames per second where the display can sustain
  it, while image lines remain prioritised.
- Offline decoding must support faster-than-real-time corpus regression.
- Receiver comparisons use the same recordings against QSSTV and, where reproducible,
  MMSSTV. Mode detection, completed-image rate, line lock, pixel error, and SSIM are
  recorded.
- Every PTT failure path begins unkeying immediately and continues bounded retries without
  blocking the audio callback or UI.
- Device disconnects preserve the selected device identity and report a recoverable
  fault; the program must not transmit through an arbitrary replacement device.

Numerical thresholds will be frozen with the M3 reference corpus rather than invented at
M0.

## 8. Data and privacy

- Default user data follows XDG configuration, data, cache, and state directories.
- Received pictures and raw audio are never uploaded automatically.
- Metadata sidecars distinguish heard callsigns from user-entered station identity.
- Image loader allowlists, dimension limits, and decompression-bomb checks protect
  untrusted received/imported files.
- Logs exclude image contents and serialise no CAT credentials or remote-control secrets.

## 9. Out of scope for 1.0

- Video or fast-scan television.
- SDR I/Q demodulation; 1.0 consumes radio-demodulated audio.
- Cloud galleries or automatic social publishing.
- Remote unauthenticated PTT.
- A bespoke kernel real-time requirement.
- Compatibility claims unsupported by repeatable vectors.

## 10. Definition of 1.0

Version 1.0 requires completion of M0 through M9, documented supported-mode vectors,
analogue and HamDRM interoperability testing, KG-STV implementation according to its
milestone scope, fail-safe PTT tests, usable GUI and TUI workflows, and repeatable Linux
plus BSD builds.
