# Robot 36 compact reference vector

This vector independently freezes the M1D Robot 36 descriptor, VIS sequence, selected
limited-range luma and colour-difference conversions, 2 by 2 chroma averages, cumulative
48 kHz event boundaries, total event count, duration, and supported sample-rate frame
counts.

The evidence and resolved implementation disagreements are recorded in
`docs/protocols/analogue/robot-36.md`. The generator uses only the Python standard
library. It does not import, invoke, parse, or derive values from Scanline SSTV production
code.

## Generate and compare

```sh
python3 tests/vectors/analogue/robot-36/generate_reference.py \
	> /tmp/robot-36-reference.json
cmp /tmp/robot-36-reference.json \
	tests/vectors/analogue/robot-36/reference.json
```

## Frozen hashes

| Artifact | SHA-256 |
| --- | --- |
| `generate_reference.py` | `07db9022f6303bae475e0bb40cc8d34d996ef781b1d27cb247662eb37d56b243` |
| `reference.json` | `0fca9257f8b083cb7b74420c035d543140fa4bc59ab09fbe748d4b62c6928b91` |

No reference WAV is committed. The event, boundary, and integer conversion data are
sufficient to detect protocol drift without a large binary fixture.
