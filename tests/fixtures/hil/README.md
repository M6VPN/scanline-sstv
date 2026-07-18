# M2J HIL reference fixture

`robot-36-reference.png` is generated entirely by this project using
`generate_fixture.py`. It is the accepted 320 by 240 diagnostic pattern with colour bars,
horizontal and vertical gradients, distinct coloured corners, checker blocks, and line
markers. It contains no callsign or third-party image content.

Robot 36 is selected because it is the shortest currently accepted analogue image mode.
The protocol encoder is not shortened or changed. FSK ID is disabled by default.

Regenerate with:

```text
python3 tests/fixtures/hil/generate_fixture.py
```

Frozen hashes:

| Artifact | SHA-256 |
| --- | --- |
| Raw tightly packed RGB8 pixels | `2ab0388b27325de68bdb3246cb4eaa043ba85bf27d65f3aebb5d0ba164cbc9d2` |
| `robot-36-reference.png` | `1676228514e96b46f144f05875ec7c74e3e16dca4d98291ea7f5d93658c1a1b7` |

The M2J hardware-free test constructs the same frame through
`makeDiagnosticPattern(320, 240)`, verifies the raw-pixel hash, encodes it through the
accepted Robot 36 offline dispatch, and compares the streamed float source against the
accepted renderer sample by sample after the explicit constant gain.
