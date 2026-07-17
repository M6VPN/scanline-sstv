# PD120 compact reference vector

This vector independently freezes the M1E PD120 descriptor, VIS sequence, limited-range
luma and colour-difference conversion, vertical pair averages, complete pair schedule,
cumulative 48 kHz boundaries, total event count, duration, and supported sample-rate
frame counts.

The evidence and resolved implementation disagreements are recorded in
`docs/protocols/analogue/pd-120.md`. The generator uses only the Python standard library.
It does not import, invoke, parse, or derive values from Scanline SSTV production code.

## Generate and compare

```sh
python3 tests/vectors/analogue/pd-120/generate_reference.py \
	> /tmp/pd-120-reference.json
cmp /tmp/pd-120-reference.json \
	tests/vectors/analogue/pd-120/reference.json
```

## Frozen hashes

| Artifact | SHA-256 |
| --- | --- |
| `generate_reference.py` | `e66e4032d441c6d4c96e680a22a2fa75939cfceab830e1897eea6a602f72aaff` |
| `reference.json` | `bda82ec62d236ab8f52274dccab4b719fee68b9c7ec467e7896fb66f1571ce5b` |

No reference WAV is committed. The event, boundary, schedule, and integer conversion data
detect protocol drift without a large binary fixture.
