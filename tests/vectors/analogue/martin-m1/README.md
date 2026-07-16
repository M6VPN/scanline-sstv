# Martin M1 Compact Reference Vector

`reference.json` freezes event counts, cumulative sample boundaries, VIS tones, and full
transmission frame counts generated independently with PySSTV v0.5.8 at commit
`884cddb36844c745fcfc6d69eab166b3b23a442b`. The generator supplies a project-created
immutable diagnostic image through PySSTV's public image interface. It does not import
Scanline SSTV code.

Regenerate from a clean temporary directory:

```sh
git clone --branch v0.5.8 --depth 1 https://github.com/dnet/pySSTV.git /tmp/pysstv-v0.5.8
test "$(git -C /tmp/pysstv-v0.5.8 rev-parse HEAD)" = \
	884cddb36844c745fcfc6d69eab166b3b23a442b
PYTHONPATH=/tmp/pysstv-v0.5.8 \
	python3 tests/vectors/analogue/martin-m1/generate_reference.py \
	> /tmp/martin-m1-reference.json
python3 -c 'import json; assert json.load(open("/tmp/martin-m1-reference.json")) == \
	json.load(open("tests/vectors/analogue/martin-m1/reference.json"))'
```

The exact source locations, licences, retrieval date, and artifact hashes are recorded in
`docs/protocols/analogue/martin-m1.md`. No WAV is required for this vector.
