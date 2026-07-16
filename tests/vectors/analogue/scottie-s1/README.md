# Scottie S1 Compact Reference Vector

`reference.json` freezes the evidence-selected VIS tones, line event order, first,
second, and final-line cumulative boundaries, exact event count, duration, tone mapping,
and frame counts. The standard-library-only generator implements the resolved handbook
schedule directly and does not import or call Scanline SSTV production code.

The generator uses *Image Communication on Short Waves*, dated 2019-11-17, SHA-256
`e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`.
The full source record and the PySSTV v0.5.8 and QSSTV commit
`8c27d6d169d8c6c197eb47c2089870e39bc06a02` comparisons are recorded in
`docs/protocols/analogue/scottie-s1.md`.

Artifact hashes:

- `generate_reference.py`: `7dd35637b6143cefa7fc4a2e076a2b865b8529b30891f3fb7bc2b5dfdc30c43d`
- `reference.json`: `4b74435f2bd4263b83f15dd2fd6c33199f3cd4508a2a96524508323549cf9116`

Regenerate and compare without project binaries or libraries:

```sh
python3 tests/vectors/analogue/scottie-s1/generate_reference.py \
	> /tmp/scottie-s1-reference.json
python3 -c 'import json; assert json.load(open("/tmp/scottie-s1-reference.json")) == \
	json.load(open("tests/vectors/analogue/scottie-s1/reference.json"))'
```

No WAV is stored in this vector. Integer schedules and RIFF metadata are tested exactly;
portable waveform comparisons retain the accepted numerical tolerance for `std::sin`.
