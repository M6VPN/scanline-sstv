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
