# Milestones

## M2J-B1: executable read-only HIL readiness

Complete for hardware-free implementation: Stage 1 performs one explicitly requested
backend refresh after a digest-bound confirmation and exact identity comparison. Stage 3
provider query/unkey behavior is constrained to injected and loopback tests. Physical
Stage 1 and real G90/flrig execution remain pending human confirmation; no physical stage
is marked passed.

#### M2J-B2: real Stage 1 recording and gated flrig Stage 3

Status: **implementation complete; physical execution pending**

Stage 1 now records the exact backend result, native capabilities, persistence, collision,
transport, discovery generation, and explicit not-measured negotiated facts atomically in
the evidence JSON and Markdown. Stage 3 is a real flrig query/unkey-only path compiled
only with all live/HIL build gates, requiring a fresh foreground confirmation and daemon
version matching the evidence session. It never constructs audio and has no key action.

Acceptance requires hardware-free tests plus later human-produced Stage 1 and Stage 3
records. This milestone does not claim physical success, complete M2J, or complete M2.

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

### M2B - Deterministic stream lifecycle and bounded sample transport

Status: **complete**

- Add fixed-capacity dependency-free SPSC float32 sample rings with explicit ownership,
  acquire/release publication, wrap handling, and stopped-only reset.
- Add project-owned playback, capture, and duplex configurations, exact device identity,
  negotiated facts, lifecycle state, typed errors, and immutable statistics.
- Add a bounded `noexcept` callback that zero-fills playback underruns, preserves old
  capture data on overflow, applies explicit channel mapping, and only copies samples or
  updates lock-free atomics.
- Extend the private miniaudio adapter to open exactly selected devices and add injected
  mock and null-backend stream coverage without exposing a user-facing device-open path.

Acceptance:

- Ring tests cover capacities one and non-power-of-two, partial and wrapped operations,
  long ordered sequences, invalid sizes, and two-thread exact-sequence stress.
- Mock tests cover playback/capture/duplex callbacks, prefill, valid and invalid lifecycle
  transitions, cancellations, operation failures, disconnects, final-callback teardown,
  exact identity failures, channel policies, and consistent underrun/overrun statistics.
- The opt-in integration test opens only miniaudio's null backend, observes callbacks,
  verifies ordered ring consumption, capture silence, underrun silence, and bounded
  cleanup. Automated tests never open real hardware.
- Minimal, audio-disabled, headless, dev, ASan, and UBSan presets pass. The focused TSan
  result is recorded separately when the host runtime supports it.
- No SSTV stream reaches a device, no user-facing command opens a device, and no PTT or
  radio-control state exists in the audio boundary.

### M2C - Explicit real-device audio diagnostics

Status: **complete**

- Add a frontend-independent single-owner diagnostics service for bounded input metering,
  armed output calibration, and deterministic local cable loopback.
- Require exact refreshed backend-and-direction identities, explicit channels, bounded
  periods/durations, and no default, name, index, fallback, or automatic reopen.
- Require a fresh per-run CLI flag or GUI confirmation before any output-capable operation
  can construct a discovery provider or stream adapter.
- Add CLI and Wayland-first Qt diagnostics using immutable bounded-rate snapshots and the
  same M2B stream lifecycle.
- Add a disabled-by-default `SSTV_ENABLE_AUDIO_HARDWARE_TESTS` gate. Enabling it alone
  cannot select hardware or arm output.

Acceptance:

- Independent numerical tests cover dBFS boundaries, signal duration/amplitude/envelope,
  finite and non-finite capture, silence, clipping, known delay/gain/polarity, and
  inconclusive correlation.
- Service instrumentation plus CLI and injected Qt tests prove unarmed output and
  loopback perform zero backend operations, refresh clears selection without replacement,
  and GUI progress is independent of audio callback cadence.
- Null-only integration covers bounded input and output lifecycle but is not reported as
  a physical loopback.
- Output is limited to -6 dBFS and 10 seconds, underrun is fatal, capture overflow is
  reported, cancellation and disconnect stop and close without fallback, and no run is
  indefinite.
- Automated verification opens no physical device. No SSTV waveform, WAV playback,
  radio-control, VOX, or PTT path is reachable.

### M2D - Mock-only transmit orchestration and mandatory unkeying

Status: **complete**

- Add a dependency-free `sstv::rig` provider boundary with explicit key, unkey, query,
  readback, observed state, certainty, deadline, recoverability, and typed errors.
- Add a serialized provider supervisor, bounded mandatory-unkey retries, non-copyable
  transmit lease, shared safety record, and independent heartbeat watchdog.
- Add an application-layer coordinator over injected finite sample-source and mock audio
  endpoint interfaces without a real adapter or protocol-waveform implementation.
- Enforce the complete preparing, audio-open, prime, watchdog-arm, key, delay, finite
  sample, drain, tail, unkey, and terminal state sequence.
- Preserve unresolved PTT hazards across sessions and require explicit confirmed cleanup
  before another session can start.

Acceptance:

- Audio is open, negotiated, silently prefilled, and primed before the watchdog is armed;
  keying occurs only after successful watchdog arming.
- No non-silent mock sample is queued before accepted keying and the pre-key delay.
  Cancellation and faults gate further signal samples and cannot cancel mandatory unkey.
- Ambiguous key results, start/source/underrun/disconnect/drain faults, shutdown, and
  cleanup failures all attempt unkey and audio shutdown independently.
- Definite key rejection requires no speculative unkey, while maybe-keyed paths use the
  bounded retry policy and expose indeterminate final state as a hazard, never success.
- Virtual-time mock tests cover exact normal transitions, readback, key ambiguity,
  retry success, permanent hazard, watchdog-arm and expiry paths, pre-key audio failures,
  post-key faults, cancellation, lease destruction, and second-session rejection.
- Minimal and audio-disabled builds retain their previous boundaries. Normal builds and
  sanitizer configurations compile the mock coordinator tests without real devices,
  sockets, serial ports, radios, SSTV waveforms, or hardware PTT.

### M2E - Loopback-only flrig XML-RPC PTT provider

Status: **complete**

- Record flrig 2.0.11 PTT evidence at tag `v2.0.11`, commit
  `e2058cbd5bf6dc4e471d60a077a2ee65289a50a2`, including method signatures,
  implementation response behavior, file hashes, and the documentation disagreement.
- Add a bounded `FlrigPttProvider` over one nonblocking POSIX TCP connection per RPC,
  restricted to explicit literal IPv4 or IPv6 loopback endpoints.
- Accept only the required HTTP/1.1 Content-Length and XML-RPC scalar subset. Reject
  redirects, chunking, compression, upgrades, unsafe XML, unsupported values, malformed
  UTF-8, and responses outside configured limits.
- Require separate `rig.get_ptt` readback after every set operation and map lost,
  malformed, timed-out, or mismatched results conservatively into M2D certainty.
- Add `checkingPtt` before audio acquisition. Keying cannot proceed until preflight or
  bounded cleanup confirms definitely unkeyed state.

Acceptance:

- Configuration rejects hostnames, DNS, non-loopback literals, mapped addresses, zone
  identifiers, malformed paths, zero ports, and invalid resource limits before sockets.
- Deterministic codec and injected-transport tests cover exact requests, fragmentation,
  HTTP/XML failures, unsafe constructs, deadlines, confirmed state, and ambiguous key.
- A real TCP test server binds only ephemeral loopback, records request order, supports
  fragmented responses, and drives the proven coordinator with injected mock audio.
- Preflight failure opens no audio. Every possibly keyed coordinator path retains M2D
  mandatory unkey behavior and unresolved hazards continue to block later sessions.
- Minimal, audio-disabled, normal, GUI, and sanitizer builds retain their boundaries.
  No test contacts real flrig, opens physical audio, renders SSTV, or accesses radio/PTT
  hardware.

### M2F - Loopback-only rigctld TCP PTT provider

Status: **complete**

- Pin Hamlib 4.7.1 commit `d042479a9f8095ba1a8e103a977c3614d7233cb2` as factual
  protocol evidence without copying code, linking libhamlib, or launching rigctld.
- Select only the newline Extended Response Protocol with fixed `+T 1`, `+T 0`, and `+t`
  commands and complete `RPRT`-terminated responses.
- Add a bounded `RigctldPttProvider` restricted to explicit literal IPv4 or IPv6
  loopback endpoints with no conventional-port default, DNS, fallback, or persistence.
- Share M2E's private nonblocking POSIX transport while retaining separate HTTP and
  rigctld line-framing grammars.
- Require readback after every set. Treat values `1`, `2`, and `3` as keyed and only `0`
  as definitely unkeyed; preserve all other outcomes as failures or indeterminate state.

Acceptance:

- Configuration rejects hostnames, URLs, non-loopback literals, mapped addresses, zone
  identifiers, zero ports, excessive timeouts, and invalid parser limits before sockets.
- Codec tests cover exact command and response bytes, all documented PTT values, negative
  `RPRT` codes, fragmentation, overflow, controls, unsupported response modes, and
  unexpected records.
- Mock, flrig, and rigctld providers pass one high-level certainty, deadline, attempt,
  operation-ID, and diagnostic-bound conformance suite.
- An ephemeral TCP server binds only loopback and drives M2D preflight, finite mock audio,
  mandatory readback, and final confirmed unkeying without external processes or hardware.
- M2D and M2E regressions remain unchanged. Minimal, audio-disabled, normal, GUI, and
  sanitizer builds preserve their existing boundaries.

### M2G - Safe rendered-audio integration

Status: **complete**

- Add a finite source over the canonical offline tone-event payload and accepted
  continuous-phase renderer without WAV staging, PCM16 conversion, or callback rendering.
- Add an exact-device playback-only endpoint over M2B `AudioStream`, preserving partial
  queue offsets, bounded backpressure, negotiated format checks, and stream fault mapping.
- Add a one-way callback-boundary signal gate that silences and discards queued signal
  after a fault and can rearm only during a stopped ring reset.
- Start silent audio before watchdog arming and keying, then retain the M2D pre-key delay,
  drain, tail, mandatory unkey, hazard, and exact-provider rules.

Acceptance:

- Martin M1, Scottie S1, Robot 36, and PD120 with and without FSK ID produce exact
  ToneRenderer float samples and frame counts through bounded source pulls.
- Pull sizes from one frame through 4096 frames preserve sample order, phase, boundaries,
  and exhaustion without duplication or omission.
- Mock AudioStream tests cover stale identity, negotiation mismatch, partial/zero queue
  acceptance, lifecycle order, device removal, and callback signal-gate races.
- The deterministic coordinator path uses the production source and endpoint with mock
  audio/PTT, and a bounded integration test opens only miniaudio's null playback backend.
- Existing M2D, loopback flrig, and loopback rigctld fault suites retain mandatory unkey
  behavior. No test opens real hardware or reaches a non-loopback rig endpoint.

The next slice is separately gated, disabled-by-default hardware-in-loop and live-transmit
enablement. It must require explicit device, provider, endpoint, arming, and radio-safety
controls. Direct libhamlib remains optional and out of scope for M2G.

### M2H - Explicitly armed CLI live transmission

Status: **complete**

- Add `SSTV_ENABLE_LIVE_TX`, default OFF in every normal preset, so the CLI command is
  absent unless a separate build explicitly includes it.
- Add one interactive `transmit-image` path over the accepted image preparation,
  tone-event renderer, exact AudioStream endpoint, coordinator, watchdog, and explicit
  flrig or rigctld provider.
- Require exact backend/device/channel selection, literal-loopback PTT configuration,
  explicit pre/post delays, explicit -60 to -6 dBFS constant gain, three runtime arm
  flags, and the exact foreground-TTY confirmation phrase.
- Publish SIGINT, SIGTERM, and SIGHUP cancellation with signal-safe state only; perform
  gating, unkeying, and teardown on the owning control thread.
- Add separately gated manual HIL configuration that ordinary presets, CTest, CI, and
  sanitizer runs cannot execute.

Acceptance:

- Complete preparation and gain/clipping validation occur before confirmation and before
  discovery, audio adapter, provider, or socket acquisition.
- The fresh discovery snapshot must contain exactly one non-colliding requested playback
  identity. No default, name, index, fallback, replacement, or automatic reopen is used.
- The M2D coordinator retains definite-unkeyed preflight, silent audio startup before
  watchdog/key, the callback signal gate, bounded delays/drain, mandatory unkey, hazard
  reporting, and independent audio cleanup.
- Default-disabled and live-enabled tests cover command registration, parser/arm rules,
  loopback policy, confirmation, exact gain samples, clipping, immutable preparation,
  exact device selection, and all pre-existing coordinator/provider fault matrices.
- No automated test opens physical audio, contacts a non-loopback endpoint, or keys a
  radio. Physical HIL is documented but not performed as M2H verification.

### M2I - Wayland-first GUI live transmission

Status: **complete**

- Add a Qt-free `LiveTransmitService` shared by CLI and GUI for preparation snapshots,
  gain/configuration validation, exact resource construction, confirmation, coordinator
  lifetime, cancellation, shutdown, PTT checks, and hazard cleanup.
- Add a live-only Qt model using revisioned immutable snapshots and bounded-rate polling;
  all preparation, discovery, audio, rendering, watchdog, networking, and cleanup work
  remains outside the GUI/render thread.
- Add a conditional Qt Quick panel with exact backend/device/channel and loopback PTT
  controls, constant gain, bounded delays, read-only PTT check, state/progress reporting,
  Stop, close deferral, and unresolved-hazard retry.
- Require three fresh acknowledgements and the exact confirmation phrase in a single-use,
  revision-bound modal review before transmit-time resource construction.
- Add `live-tx-gui-compile`; normal presets and CI retain `SSTV_ENABLE_LIVE_TX=OFF`.

Acceptance:

- Default GUI builds contain no live model, panel, action, shortcut, QML resource, or
  provider factory. The offline editor and audio diagnostics retain their existing tests.
- Live-enabled service/model tests prove preparation and rejected confirmation acquire no
  audio or PTT resources, confirmations are stale-safe and single-use, device refresh
  does not select a replacement, and relevant editor/configuration changes revoke
  readiness.
- Offscreen QML startup proves the live panel exists only in the enabled build and its
  Transmit action begins disabled. Three arms and the exact phrase remain mandatory.
- Stop, window close, process signals, coordinator faults, and destruction retain signal
  gating and mandatory unkey ownership. A hazardous final PTT state blocks another run
  and can only use the unkey/query retry path.
- Automated verification uses injected providers, mock/null audio, ephemeral loopback,
  and offscreen software rendering only. It performs no physical HIL and claims no native
  compositor, backend, radio, deviation, RF, distribution, or BSD compatibility.

### M2J - Opt-in physical HIL evidence and calibration

Status: **not started**

#### M2J-A - Staged HIL evidence harness and calibration framework

Status: **complete**

- Add versioned dependency-light HIL evidence values, deterministic JSON and Markdown,
  explicit unknowns, operator/automatic evidence sources, artifact hashes, and bounded
  atomic local publication.
- Replace the all-at-once manual target with a stage-selected fail-closed planning target.
  No CMake target automatically advances or executes a physical stage.
- Define exact Stage 0 through Stage 7 resource boundaries, prerequisite ordering,
  configuration-digest authorization, fresh single-use stage permits, and unresolved-PTT
  hazard blocking.
- Add a callsign-neutral 320 by 240 Robot 36 reference fixture with frozen image/pixel
  hashes and sample-equivalence proof through the accepted offline renderer.
- Add a schema, template, runbook, emergency-unkey procedure, redaction policy, and
  hardware-free CLI/model tests.

Acceptance:

- Schema, every result state, unknown values, deterministic serialization, hashes,
  resource limits, atomic overwrite policy, path rejection, stage order, permit isolation,
  stale/single-use confirmation, false-success prevention, and hazard blocking pass.
- Stage 0 manifest generation records zero resource acquisitions and CLI parser tests
  reject duplicate, missing, malformed, and unknown options.
- The frozen Robot 36 source event/frame projection remains accepted, and streamed float
  samples equal the existing renderer samples times the explicit gain for every frame.
- Default, live-enabled, GUI, and sanitizer tests remain hardware-free. No physical audio
  device, real daemon, radio, PTT hardware, or key request is used.
- No manual stage is marked passed and no physical evidence is claimed. Physical M2J
  execution remains not started.

- Record manually armed evidence for specific audio backend/device/radio combinations,
  deviation and level calibration, emergency unkey, disconnect, shutdown, native Wayland,
  and XCB/XWayland behavior.
- Retain exact-device, literal-loopback, fresh arming, watchdog, and mandatory-unkey gates.
- Keep optional direct libhamlib behind a later evidence and safety gate.

Overall M2 remains in progress until the separately gated M2J evidence is recorded.

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
