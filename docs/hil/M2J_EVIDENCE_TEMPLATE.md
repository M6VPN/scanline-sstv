# M2J evidence template

Create one evidence directory per exact hardware and software combination. Start with
Stage 0 and retain each later result even when it fails or is inconclusive.

## Build

- UTC start and end:
- Git commit and dirty-worktree state:
- Compiler and version:
- CMake preset and options:
- miniaudio version:
- Qt version, if used:
- OS, kernel, architecture, and redacted host identifier:
- Session platform: CLI, native Wayland, or XCB/XWayland:

## Exact configuration

- Audio backend, exact playback identity, persistence class, and collision state:
- Selected channel/count and negotiated format/rate/channels/period:
- PTT provider, literal-loopback address/port, and flrig path:
- Radio manufacturer/model/firmware:
- Interface, cable, dummy load, and test arrangement:
- Operator-observed mode, frequency, power, VOX, compressor, TOT, and antenna state:
- SSTV mode, fixture hash, FSK state, duration, frame count, gain, and delays:
- Instruments and calibration method:

## Stage result

For each stage record `not-run`, `passed`, `failed`, `inconclusive`, or `skipped` with a
reason. Mark evidence as `operator-observed` or `automatically-measured`. Record unknown
and unmeasured fields explicitly. For keyed stages also record the signal gate,
coordinator terminal state, unkey attempts, readback, final PTT certainty, audio cleanup,
primary error, cleanup errors, and the operator-observed physical radio state.

An operator observation of unkeyed hardware does not convert an indeterminate automatic
PTT result into a pass.
