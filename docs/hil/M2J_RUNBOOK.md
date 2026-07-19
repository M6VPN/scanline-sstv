# M2J physical HIL runbook

M2J-A supplies a hardware-free evidence and authorization framework. It does not contain
physical evidence. Codex and CI must not execute physical-audio or keyed stages.

## Required equipment

- Exact audio interface and verified channel cabling.
- Dummy load or another documented safe RF arrangement.
- RF power meter and, where available, an oscilloscope, audio meter, monitor receiver, or
  SDR.
- A hardware PTT release method tested before keyed work.
- A second observer ready to release PTT for high-risk fault checks.

## Stage order

Stages are separate invocations and never advance automatically.

| Stage | Purpose | Permitted resources |
| --- | --- | --- |
| 0 Manifest | Record build, fixture, requested configuration, and plan | None |
| 1 Discovery | Refresh one exact backend and record capabilities | Discovery only |
| 2 Audio calibration | M2C meter, calibration, or local cable loopback | Exact audio only; no PTT or SSTV |
| 3 PTT unkey | Query and, if needed, request confirmed unkey | PTT query/unkey only; no audio or key |
| 4 Keyed silence | Verify bounded key, Stop, unkey, and cleanup | Silent audio and PTT key/unkey |
| 5 Full SSTV | Send the frozen Robot 36 fixture into a dummy load | Exact live service and accepted SSTV source |
| 6 Controlled fault | One separately selected fault check | Only resources required by that check |
| 7 GUI compositor | Record native Wayland and XCB/XWayland behavior separately | GUI; physical resources only in a separately armed run |

Stage 2 can emit physical audio but never SSTV and never keys PTT. Stages 4 through 6 can
key PTT. Stage 5 can emit SSTV. Stages 4 through 7 are manual and must never be registered
with CTest.

## Stage 0

M2J-B1/B2 adds executable read-only stages. Stage 1 is invoked with
`scanline-sstv-cli hil-stage --stage discovery --evidence-dir DIR --backend BACKEND`
and explicit `--playback-id`, manifest `--digest`, and a fresh
`AUTHORIZE M2J discovery DIGEST` confirmation typed at the foreground prompt. It refreshes
one backend, compares only the exact native identity, records enumeration facts separately
from negotiated facts, and never initializes or opens an audio endpoint. Stage 3 is a
separately build-gated flrig query/unkey-only command; it requires the Stage 1 result,
daemon version, literal loopback endpoint, and a fresh
`AUTHORIZE M2J ptt-unkey DIGEST` prompt. It never sends a key request.

For the current G90 session the operator command is:

    scanline-sstv-cli hil-stage --stage discovery \
      --evidence-dir hil-evidence/g90-b2 --backend pulseaudio \
      --playback-id pulseaudio:playback:616c73615f6f75747075742e7573622d432d4d656469615f456c656374726f6e6963735f496e632e5f5553425f417564696f5f4465766963652d30302e616e616c6f672d73746572656f \
      --digest 2a67af7707ab7d5769f5dd6efae8ff9dbaccf6f063edaf4590f64e58ed120648

The exact prompt phrase is `AUTHORIZE M2J discovery
2a67af7707ab7d5769f5dd6efae8ff9dbaccf6f063edaf4590f64e58ed120648`. After completion,
verify the JSON `discovery` object is `passed`, its measurements identify the exact
identity and `negotiated_facts` is `not-measured`, and the Markdown row agrees. The
discovery service only enumerates; its existing provider instrumentation and the absence
of an AudioStream/device initialization prove no endpoint was opened.

Use `scanline-sstv-cli hil-manifest` with the explicit metadata options shown by
`--help`. Supply placeholders only when testing the schema. For a real session, replace
each placeholder with an operator-verified value. The command creates no discovery,
audio, PTT, or socket object and writes `m2j-evidence-v1.json` plus a Markdown view to an
explicit nonsymlink directory.

Example shape, intentionally incomplete:

```text
scanline-sstv-cli hil-manifest --output-dir HIL_DIRECTORY \
    --utc-start UTC --git-commit COMMIT --compiler COMPILER \
    --compiler-version VERSION --preset PRESET ...
```

Do not create a reusable command containing every arm or real endpoint value.

## Arming

Every resource-bearing stage requires its exact stage name, current configuration digest,
affirmed checklist, fresh interactive phrase, and a single-use permit. Editing any
configuration invalidates the permit. There is no `--yes`, environment-only, stored, or
unattended bypass. Rejection must happen before constructing stage resources.

Before any keyed stage affirm and record:

- Dummy load or documented safe RF arrangement.
- Lowest practical transmitter power.
- Correct radio mode and frequency.
- VOX disabled and compressor state recorded.
- Audio gain minimized and exact interface/channel verified.
- Hardware PTT release tested.
- Earlier flrig/rigctld final-unkey readback confirmed.
- Radio timeout/TOT enabled where available, or recorded unavailable.
- Observer ready to use the hardware release.
- Antenna state and local legal/station-control responsibility.

The application does not configure frequency, power, VOX, compression, or TOT.

## Calibration

Before Stage 2, the operator must supply which playback channel (0 or 1) feeds the G90
AUX input, current PipeWire/PulseAudio mute and volume state, the radio AUX input level,
whether hardware attenuation is present, the cable-loopback or measurement arrangement,
and the monitoring equipment. Stage 2 starts at `-60 dBFS`. The native PulseAudio S16
format is an enumeration fact; later evidence records the negotiated float application
format and backend conversion separately.

Begin at the lowest permitted explicit software level. Record M2C peak, RMS, DC,
clipping, underruns, gain, polarity, latency, correlation, and discontinuities without
automatic adjustment. Record instruments and observations separately. For SSB, record
audio drive, ALC, and occupied bandwidth. FM deviation is optional and only meaningful
for an applicable radio mode. No universal deviation, ALC, RF power, or compressor target
is implied.

## Fault checks

Select and confirm only one fault per run: GUI Stop or CLI cancellation, SIGINT/SIGTERM,
window close, device removal, backend disconnect, local daemon loss, failed readback, or
shutdown cleanup. Daemon loss while keyed additionally requires dummy load, hardware
release, TOT where supported, an observer, and the separate high-risk acknowledgement.

Never deliberately test SIGKILL, power loss, OS crash, or loss of every unkey mechanism
while keyed.

## Abort and emergency unkey

1. Use the visible Stop action or send SIGINT.
2. Observe signal gating and the unkeying state.
3. If readback is not definitely unkeyed, use the verified physical release immediately.
4. Remove audio drive if safe to do so.
5. Confirm the radio state locally and record the automatic result separately from the
   operator observation.
6. Do not start another keyed stage while the evidence session contains an unresolved
   hazard.

An in-process handler cannot guarantee cleanup after SIGKILL, power loss, OS failure,
daemon failure, broken cables, or radio hardware failure.

## Evidence retention

Evidence stays local and is never uploaded automatically. Use a hostname-redacted stable
identifier. Do not record usernames, home paths, serial numbers, callsigns, image
contents, credentials, or secrets. Raw audio is optional, separately armed, capped at
64 MiB, atomically written, and stored under the ignored `hil-evidence/` directory.
Record failures and inconclusive results without changing them to success.
