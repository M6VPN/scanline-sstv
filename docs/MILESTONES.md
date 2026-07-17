# Milestones

Each milestone is complete only when its acceptance criteria and relevant tests pass.
Items may be prototyped early, but compatibility claims wait for their milestone gate.

## M0 — Foundation

Status: **complete**

- Lock language, licence, GUI, audio, DSP, image, TUI, rig, and digital-mode decisions.
- Add Codex instructions and architecture documentation.
- Establish CMake targets, headless preset, optional GUI preset, core API seam, diagnostic
  CLI, and smoke test.
- Establish real-time callback and PTT safety invariants.

Acceptance:

- Core and CLI compile with a C++20 compiler.
- The smoke test runs without external libraries.
- CMake/JSON project files pass static validation.
- No foundation code can key a transmitter.

## M0.1 - Foundation stabilization

Status: **complete**

- Repair the Qt QML module startup path and centralise its URI.
- Complete current Scanline SSTV application and build branding.
- Add a noninteractive offscreen GUI startup smoke test.
- Add initial Linux GCC, Clang, and Qt GUI continuous integration.

Acceptance:

- Headless configure, build, and core smoke tests pass.
- The Qt 6.5+ GUI builds and `Main.qml` loads in smoke mode without entering the event
  loop.
- Current user-facing and CMake identifiers use Scanline SSTV branding.
- CI has read-only permissions, immutable action pins, and no hardware or PTT access.
- No signal generation, protocol timing, audio, DSP, or PTT implementation is added.

## M1 — Verified analogue TX and image preparation

Status: **in progress**

### M1a - Martin M1 offline diagnostic encoder

Status: **complete**

- Record attributable Martin M1 and VIS evidence, exact source revisions, licences,
  retrieval dates, locations, and artifact hashes.
- Add normalized rational timing, cumulative sample scheduling, typed tone events, and a
  block renderer with continuous oscillator phase.
- Add an immutable RGB8 frame and deterministic exact-size diagnostic pattern.
- Register Martin M1 for offline test-pattern TX only after its independent compact vector
  passes.
- Stream atomic mono PCM16 WAV output through the CLI with no playback or PTT.

Acceptance:

- Exact timing and cumulative boundaries pass at 8 kHz, 11025 Hz, 44100 Hz, 48 kHz, and
  96 kHz with no repeated-line drift.
- VIS, first-line events, tone mapping, full event count, frame count, and duration match
  the pinned independent vector.
- Rendering is block-size invariant within documented numerical tolerance and preserves
  oscillator phase.
- WAV and CLI tests cover finalization, overwrite refusal, forced replacement, malformed
  input, RIFF overflow, and failed-publish cleanup.
- Headless and Qt development test presets pass without audio, radio, or PTT access.

### M1B - Safe raster image preparation and offline image TX

Status: **complete**

- Add a dedicated `sstv_image` boundary that is the only target linked to system
  `vips-cpp`, with typed results and dependency-free public recipe values.
- Enforce bounded local regular-file input, native JPEG/PNG loader selection, header-first
  limits, single-frame input, EXIF orientation, oriented crop, ICC-to-sRGB conversion,
  premultiplied-alpha Lanczos resizing, contain/cover fit, and explicit background.
- Materialize only immutable exact-mode RGB8 and publish stripped prepared PNG files
  atomically.
- Add `prepare-image` and `encode-image`; both arbitrary-image and diagnostic paths call
  the frozen M1a `encodeMartinM1` implementation and WAV writer.
- Replace mode capability booleans with an extensible typed bitmask. Martin M1 advertises
  offline test-pattern TX and offline image TX, but neither live TX nor receive.

Acceptance:

- JPEG and PNG success paths and all required malformed, special-file, loader, metadata,
  colour, orientation, crop, geometry, resource-limit, publication, and CLI cases pass.
- The exact M1a diagnostic PNG round trip is pixel-identical and produces the same Martin
  M1 event stream as the direct frame path.
- Minimal, headless, Qt 6.11.1, ASan, and UBSan verification passes without playback,
  radio access, or PTT.

### M1C - Scottie S1 offline TX and shared analogue dispatch

Status: **complete**

- Record attributable Scottie S1 evidence, exact inspected revisions, licences,
  locations, retrieval dates, artifact hashes, resolved timings, and implementation
  disagreements.
- Move sequential RGB descriptors into a mode-neutral schedule of fixed tones and RGB
  scans with explicit first-line and pre-image support.
- Use one shared sequential RGB encoder for the frozen Martin M1 path and Scottie S1.
- Add a central registry-backed offline TX dispatch used by both test-pattern and image
  WAV commands.
- Register Scottie S1 for offline test-pattern and prepared-image TX only after its
  independent compact vector passes.

Acceptance:

- Scottie S1 VIS, first-line sequence, sync placement, first, second, and final-line
  boundaries, event count, duration, mapping, and supported-rate frame counts match the
  independent vector.
- Scottie rendering is block-size invariant within the accepted numerical tolerance,
  preserves phase, produces finite bounded PCM input, and accumulates no line drift.
- PNG and JPEG image-to-WAV, preparation options, overwrite, forced replacement, invalid
  mode, wrong dimensions, and failed-publication cleanup pass through shared services.
- The Martin M1 reference artifact, exact event stream, event count, duration, frame
  count, and direct/prepared-image equivalence remain unchanged.
- Minimal, headless, Qt 6.11.1, ASan, and UBSan verification passes without playback,
  radio access, or PTT.

### M1D - Robot 36 offline TX and luma/chroma infrastructure

Status: **complete**

- Record attributable Robot 36 evidence with exact inspected revisions, locations,
  licences, hashes, colour definitions, subsampling rules, timings, and resolved
  implementation disagreements.
- Add immutable dependency-free luma, red-difference, and blue-difference 4:2:0 storage
  with deterministic fixed-point conversion from nonlinear sRGB RGB8.
- Add a separate alternating luma/chroma descriptor and encoder without changing the
  accepted sequential-RGB schedules.
- Register Robot 36 through the central offline dispatch for diagnostic-pattern and
  prepared-image WAV generation only.

Acceptance:

- Conversion, clamping, truncation, 2 by 2 post-conversion averaging, component parity,
  first/final pair handling, and invalid storage match the independent compact vector.
- VIS, even/odd line schedules, identifier tones, event count, duration, boundaries, tone
  mapping, and every supported sample-rate frame count pass exactly with no line drift.
- Rendering remains block-size invariant and continuous-phase; PCM16 RIFF publication,
  overwrite, forced replacement, and failure cleanup pass.
- Test-pattern, PNG, JPEG, contain, cover, crop, background, and exact prepared-image paths
  use the same Robot encoder and central dispatch.
- Martin M1 and Scottie S1 vectors, ordered events, durations, frame counts, and
  direct/prepared-image equivalence remain unchanged.
- Minimal, headless, Qt, ASan, and UBSan verification passes without playback, radio
  access, or PTT.

### M1E - PD120 offline TX and paired-line luma/chroma infrastructure

Status: **complete**

- Record attributable PD120 evidence with original-author documentation, pinned
  interoperability implementations, exact inspected locations, licences, hashes, colour
  definitions, pair rules, timings, and resolved disagreements.
- Add immutable dependency-free full-resolution luma and full-width vertically subsampled
  red-difference and blue-difference storage with deterministic fixed-point conversion
  from nonlinear sRGB RGB8.
- Add a separate paired-line descriptor and encoder without changing sequential-RGB or
  Robot 36 scheduling and colour behaviour.
- Register PD120 through the central offline dispatch for diagnostic-pattern and
  prepared-image WAV generation only.

Acceptance:

- Conversion, clamping, truncation, conversion-before-average, vertical pair averaging,
  full horizontal chroma resolution, first/final pair handling, and invalid storage match
  the independent compact vector.
- VIS, pair schedule, scan order, event count, duration, selected boundaries, tone mapping,
  and every supported sample-rate frame count pass exactly with no pair drift.
- Rendering remains block-size invariant and continuous-phase; PCM16 RIFF publication,
  overwrite, forced replacement, and failure cleanup pass.
- Test-pattern, PNG, JPEG, contain, cover, crop, background, and exact prepared-image paths
  use the same PD120 encoder and central dispatch.
- Martin M1, Scottie S1, and Robot 36 vectors, ordered events, durations, frame counts,
  colour storage, and direct/prepared-image equivalence remain unchanged.
- Minimal, headless, Qt, ASan, and UBSan verification passes without playback, radio
  access, or PTT.

### M1F - Optional analogue FSK ID and defensive WAV inspection

Status: **complete**

- Record attributable FSK ID protocol and implementation evidence, exact inspected
  revisions, locations, licences, hashes, wire framing, alphabet, placement, and resolved
  implementation differences.
- Add a validated optional FSK identifier to central offline-transmission options and
  compose one evidence-backed suffix after every accepted analogue mode without changing
  the base encoders.
- Add a bounded read-only RIFF/WAVE PCM16 inspector with typed errors, checked chunk
  arithmetic, fixed-block sample statistics, and strict local-file policy.
- Expose optional `--fsk-id` on both WAV encoders and `inspect-wav --input FILE` without
  playback, decoding, mode detection, radio access, or PTT.

Acceptance:

- Independent compact FSK vectors prove framing, LSB-first tones, exact duration, event
  boundaries, checksum, placement, every supported-rate frame count, and identifier
  validation.
- Disabled FSK preserves all four accepted event streams, durations, frame counts,
  vectors, and PCM paths; enabled streams preserve the exact base prefix and accepted
  suffix with block-size-invariant continuous-phase rendering.
- The WAV parser accepts valid generated mono PCM16 files and harmless chunk placement,
  calculates exact metadata and deterministic statistics, and rejects malformed,
  unsupported, special-file, symlink, overflow, and resource-limit cases.
- CLI tests cover both FSK-enabled WAV commands, wrong-command and malformed options,
  formatted inspection, overwrite behavior, and exit codes.
- Minimal, headless, Qt, ASan, and UBSan verification passes without playback, radio
  access, or PTT.

### M1G - Wayland-first offline GUI TX editor foundation

Status: **complete**

- Add a Qt-free offline TX editor application service using the existing registry, image
  preparation, central analogue dispatch, FSK validation, WAV writer, and WAV inspector.
- Replace the placeholder QML shell with an asynchronous editor for all four accepted
  offline image modes, contain/cover, crop, background, sample rate, optional FSK ID,
  exact prepared-image preview, metadata, atomic PNG/WAV export, and WAV inspection.
- Publish immutable revisioned preview snapshots and reject stale preparation/export
  completions without exposing mutable image storage across threads.
- Preserve the noninteractive offscreen startup test and explicit no-audio/no-PTT UI.

Acceptance:

- Registry-derived mode choices expose exact dimensions, colour encoding, VIS metadata,
  base and combined duration, and cumulative frame counts at supported sample rates.
- Preparation and encoding stay off the GUI thread; old asynchronous completions cannot
  replace newer mode, image, or recipe state.
- Export is disabled until the current request is ready, refuses overwrite without user
  confirmation, publishes atomically, and inspects the actual exported WAV.
- Qt model tests cover lifecycle states, invalidation, non-local URL rejection, FSK
  feedback, preview revisions, overwrite confirmation, export, and inspection.
- Minimal, headless, dev, ASan, and UBSan presets pass; Qt 6.5+ offscreen GUI tests pass
  without playback, sound-card access, radio access, or PTT.

M1 remains incomplete. Live paths, receive work, and encode/decode round-trip analysis
gates remain later work. The non-live round-trip gate requires the later M3 decoder and is
not weakened by this GUI foundation.

- Build the data-driven mode descriptor schema.
- Add attributed timing/specification tables and golden vectors.

Acceptance:

- Every tone, interval, scanline, VIS bit, image dimension, and total duration matches
  independently generated or captured golden vectors within documented tolerances.
- Round-trip tests pass through the project’s non-live analysis path.
- Unsupported formats fail without producing partial on-air output.

## M2 — Live audio and safe PTT

Status: **in progress**

### M2A - Pinned audio dependency and read-only discovery

Status: **complete**

- Pin miniaudio 0.11.25 at an exact commit with imported-file hashes, complete licence,
  enabled-feature record, and update procedure.
- Add a frontend-independent `sstv_audio` boundary with project-owned backend, device,
  identity, capability, transport, diagnostic, and immutable snapshot values.
- Probe each requested backend through an independent context and preserve partial
  success, zero-device success, unknown devices, and per-backend failures.
- Add read-only `list-audio` diagnostics for Linux and represented BSD backends, with an
  explicit opt-in null backend and safe terminal escaping.
- Keep minimal and audio-disabled builds operational without changing offline image,
  waveform, GUI, or WAV behavior.

Acceptance:

- The discovery provider seam covers backend mapping, compiled status, independent
  attempts, partial failure, zero devices, capture/playback identity, duplicates,
  persistence limits, unknown capabilities, refresh publication, cancellation, resource
  bounds, and concurrent refresh rejection without requiring host audio hardware.
- Production discovery creates contexts and enumerates only. No `ma_device` is
  initialized or started, no callback or stream is created, and no sample is played or
  captured.
- PulseAudio/PipeWire-Pulse, JACK/PipeWire-JACK, and ALSA are reported as distinct Linux
  APIs. Compatibility backends are not called native PipeWire.
- Sndio enumeration fails closed because the pinned implementation opens endpoints;
  missing servers, zero devices, and unavailable hardware remain diagnostics rather than
  false test failures.
- Minimal, headless, dev, audio-disabled, ASan, and UBSan presets pass. Native BSD
  discovery remains a later focused portability check.

M2B is the next intended slice: stream lifecycle, bounded rings, and mock
playback/capture without PTT.

- Integrate miniaudio.
- Enumerate and select Linux/BSD input/output backends and individual devices.
- Add latency/buffer controls, channel selection, level calibration, and loopback test.
- Implement flrig XML-RPC PTT.
- Implement rigctld PTT and optional direct libhamlib.
- Implement the transmit state machine, watchdog, cancellation, and shutdown unkey guard.

Acceptance:

- Live TX uses the exact samples validated by offline WAV tests.
- Mock flrig and rigctld fault-injection tests cover disconnect, timeout, malformed reply,
  cancellation, underrun, normal exit, and forced shutdown.
- USB device removal does not select another output or leave a mock PTT keyed.
- Hardware-in-loop tests remain opt-in.

## M3 — Offline high-performance analogue RX

- Implement conditioning, quadrature tone estimation, VIS detection, sync correlation,
  line-clock PLL, AFC, and pixel reconstruction.
- Decode the M1 transmit modes from WAV/recorded audio.
- Build the generated impairment corpus: AWGN, hum, clipping, offset, drift, dropout,
  filtering, and multipath approximations.
- Add raw decoder metrics and deterministic benchmark commands.

Acceptance:

- Clean encoder-to-decoder vectors are pixel-exact where the protocol permits.
- Impaired-corpus results meet thresholds frozen with the corpus.
- Decoder state and outputs are deterministic for a fixed input and configuration.
- Offline decode is faster than real time on the documented reference machine.

## M4 — Live RX and full GUI workflow

- Connect capture ring buffers to the M3 decoder.
- Add live decoded image, waterfall, spectrum, waveform, level, sync/VIS confidence, AFC,
  and timing displays.
- Add gallery, autosave, raw-audio recording, replay, metadata, and manual correction.
- Finish first-run audio and rig configuration.

Acceptance:

- GUI stalls, resizing, and compositor throttling do not create audio overruns.
- Native Wayland and XCB/XWayland sessions pass the UI test checklist.
- Receive and replay paths produce identical decoder results for the same samples.
- A complete RX/TX analogue QSO workflow is operable without opening a settings file.

## M5 — TUI

- Add `scanline-sstv-tui` using the same application services.
- Render images through Kitty/Sixel/iTerm2 protocols where detected.
- Add true-colour and reduced-colour cell fallbacks.
- Add terminal waterfall, waveform, RX/TX controls, gallery, device, and rig screens.
- Support keyboard-only operation and resize/SSH scenarios.

Acceptance:

- TUI and GUI produce identical offline decode and transmit files.
- At least one pixel-protocol terminal and the Unicode fallback pass screenshots/tests.
- Loss of terminal graphics capability does not lose control or decoder state.

## M6 — Analogue coverage and receiver optimisation

- Add Martin M2, Scottie S2/DX, Robot 72, remaining PD modes, Pasokon, and verified common
  Wraase/monochrome modes.
- Add confidence-ranked no-VIS mode assistance.
- Profile filters, FFT, resampling, image updates, and memory traffic.
- Compare the fixed corpus with current QSSTV and a reproducible MMSSTV environment.

Acceptance:

- Each advertised mode has TX and RX golden vectors.
- Performance/regression report is reproducible.
- No comparison regression is waived without a recorded reason and replacement test.

## M7 — HamDRM digital SSTV

- Implement OFDM acquisition and tracking, framing, QAM, interleaving, CRC/FEC, and
  payload transport required by selected HamDRM profiles.
- Implement image/file payload and JPEG/JPEG 2000 handling.
- Add progressive status, robustness/profile selection, callsign/metadata, and partial
  receive recovery.
- Test against QSSTV and EasyPal-compatible captures/transmissions.

Acceptance:

- Bidirectional interoperability succeeds for every advertised profile.
- Corrupt or hostile payloads cannot escape size/format limits.
- Digital TX uses the same PTT safety state machine.

## M8 — KG-STV

- Implement compatible MSK and 4LFSK waveforms.
- Implement text and image block framing.
- Reconstruct 16-by-16 blocks progressively and combine valid repetitions.
- Implement missing-block/status interactions that can be verified from protocol evidence.

Acceptance:

- Bidirectional interoperability with the reference KG-STV program/captures for every
  advertised modulation.
- Repeated transmissions improve or preserve completion and never replace a valid block
  with a failed block.

## M9 — Portability, packaging, and 1.0

- CI/build verification for Arch Linux, Debian-family Linux, FreeBSD, OpenBSD, and NetBSD.
- Arch package, Flatpak, Debian packaging, and BSD port/pkgsrc submission material.
- Accessibility, localisation readiness, user manual, migration, crash recovery, and
  security review.
- Freeze configuration, CLI, and supported-mode compatibility tables.

Acceptance:

- Clean build/test/install/uninstall on the supported matrix.
- Release artefacts include source, licence/attribution, reproducible test report, and
  checksums.
- No open critical PTT, data-loss, decoder-safety, or interoperability defects.
