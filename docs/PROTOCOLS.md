# Protocol and interoperability policy

Radio compatibility depends on small timing and framing details. Protocol evidence is
therefore part of the implementation, not optional background reading.

## Evidence levels

Every mode or control operation records one or more of:

1. Published standard or original author documentation.
2. Attributed GPL-compatible reference source.
3. Independently captured and annotated reference-program audio.
4. Independently generated calculation/vector.
5. Over-the-air recording with known transmitter and mode.

Memory, an unlabelled web table, or “it looks correct in the waterfall” is insufficient.

Each descriptor should cite evidence in a neighbouring source comment or machine-readable
metadata. Adapted source retains its copyright and licence notice.

## Analogue SSTV descriptor

A descriptor or strategy must define:

- Stable mode ID and human name.
- Family and VIS code/parity.
- Width, height, colour space, channel order, and subsampling.
- Leader, break, VIS, sync, porch, separator, and scan timing.
- Tone mapping, black/white endpoints, and any gamma convention.
- Expected nominal duration and tolerances.
- Encoder vector, decoder vector, and provenance.

Timing uses integer/rational units or a monotonic phase accumulator. Repeated floating
point duration rounding must not accumulate line drift.

## Accepted analogue records

Martin M1 is accepted for M1a offline diagnostic-pattern transmission and M1B offline
prepared-image transmission. Its
[protocol evidence record](protocols/analogue/martin-m1.md) fixes the source versions,
artifact hashes, VIS framing, GBR scan order, exact rational timings, tone mapping,
nominal duration, and the documented QSSTV timing divergence.

The production constants follow the attributable handbook values. A compact event and
sample-boundary vector generated independently with pinned PySSTV v0.5.8 is stored under
`tests/vectors/analogue/martin-m1`. M1B does not alter those constants or vectors: both
offline commands pass an immutable 320 by 256 RGB8 frame to the same `encodeMartinM1`
implementation. No claim is made for live transmission, receive interoperability, or
other Martin modes.

Scottie S1 is accepted for M1C offline diagnostic-pattern and prepared-image
transmission. Its [protocol evidence record](protocols/analogue/scottie-s1.md) fixes VIS
code 60, 320 by 256 dimensions, GBR order, the 9.0 ms sync between blue and red, three
1.5 ms gaps, 138.240 ms visible scans, tone mapping, first-line behavior, and exact
duration. The independent compact vector under `tests/vectors/analogue/scottie-s1` does
not import production code.

The Scottie record documents two comparisons rather than hiding their differences.
PySSTV v0.5.8 preserves the selected total line duration but redistributes visible-scan
and gap boundaries. The inspected QSSTV revision uses adjusted TX subintervals and a
different first-line placement. Production follows the handbook's explicit schedule and
does not average those values. Scottie S1 advertises offline test-pattern and image TX
plus the M1F optional offline FSK suffix; live TX, receive, and other Scottie modes remain
unsupported.

Robot 36 is accepted for M1D offline diagnostic-pattern and prepared-image transmission.
Its [protocol evidence record](protocols/analogue/robot-36.md) fixes VIS code 8, 320 by
240 dimensions, the limited-range nonlinear-RGB luma/red-difference/blue-difference
conversion, post-conversion 2 by 2 chroma averaging, even R-Y and odd B-Y placement,
exact 150 ms line schedule, and 36.910 s complete duration. The independent compact
vector under `tests/vectors/analogue/robot-36` imports no production code.

Production follows the internally consistent Dayton proposal and SSTV Encoder 2.13
definition. QSSTV's full-range matrix, full horizontal chroma resolution, and adjusted
timings are retained only as an interoperability comparison. PySSTV's line-local Pillow
conversion and reversed component identifiers are also documented and not mixed into the
selected values. Robot 36 advertises offline test-pattern and image TX plus the M1F
optional offline FSK suffix; live TX, receive, and other Robot modes remain unsupported.

PD120 is accepted for M1E offline diagnostic-pattern and prepared-image transmission. Its
[protocol evidence record](protocols/analogue/pd-120.md) fixes VIS code 95, 640 by 496
dimensions, 248 paired scan groups, limited-range nonlinear-RGB luma/red-difference/
blue-difference conversion, conversion-before-average, full-width vertically averaged
chroma, the Y0/R-Y/B-Y/Y1 order, nominal 2.08 ms porch, 121.6 ms component scans, and
127.013040 s complete duration. The independent compact vector under
`tests/vectors/analogue/pd-120` imports no production code.

Production follows Paul Turner's original PD bulletin, the Dayton colour definition, and
SSTV Encoder 2.13's deterministic truncation and averaging behaviour. QSSTV's adjusted
2.30 ms TX porch, adjusted duration, full-range matrix, and average-before-conversion path
are retained only as interoperability comparisons. PySSTV's Pillow conversion is also not
mixed into the selected values. PD120 advertises offline test-pattern and image TX plus
the M1F optional offline FSK suffix; live TX, receive, and other PD modes remain
unsupported.

Optional analogue FSK ID is accepted for the M1F offline suffix used by Martin M1,
Scottie S1, Robot 36, and PD120. Its
[protocol evidence record](protocols/analogue/fsk-id.md) fixes the post-image placement,
physical leader/start sequence, six-bit LSB-first alphabet, exact 22 ms bits, 1500/1900/
2100 Hz tones, checksum, trailer, nine-character limit, and ASCII uppercase rule. The
compact independent vector under `tests/vectors/analogue/fsk-id` imports no production
code.

Production follows the complete QSSTV and independently matching mySSTV transmitted
sequence. The dreamport/libsstvenc logical-prefix representation and libsstvenc omission
of checksum and trailer are documented differences rather than mixed into production.
The suffix is optional and mode-neutral. It advertises only offline FSK ID composition;
live FSK ID and FSK ID reception remain unsupported.

## HamDRM

QSSTV is an allowed GPL-3.0 reference, but compatibility is validated with external
captures and bidirectional tests rather than assumed from code ancestry. Maintain an
attribution record for any adapted implementation.

Before advertising a HamDRM profile, record:

- Robustness/occupancy and OFDM parameters.
- FAC, SDC, MSC, interleaver, CRC, and FEC configuration.
- QAM mapping and soft-decision convention.
- Payload/file header and image encoding behaviour.
- Callsign/metadata and retransmission/partial-recovery behaviour.
- Reference sender and receiver versions used in the test.

## KG-STV

Treat KG-STV as a separate codec and framing family. Record exact MSK and 4LFSK symbol
mapping, synchronisation, block numbering, compression, checksum/FEC, text framing, and
missing-block behaviour from verified evidence.

Unknown or ambiguous fields remain experimental and are not enabled for live TX by
default.

## Rig-control protocols

- M2E flrig XML-RPC is pinned to flrig 2.0.11 commit
  `e2058cbd5bf6dc4e471d60a077a2ee65289a50a2` and uses only documented
  `rig.set_ptt` and `rig.get_ptt` methods. Set acknowledgement is followed by mandatory
  integer readback. See `docs/protocols/rig/flrig-xmlrpc.md`.
- rigctld uses extended responses where practical and parses complete `RPRT` results.
- M2E accepts only explicit literal `127.0.0.1` or `::1` endpoints and provides no
  enabled default. Remote flrig access is outside this slice.
- PTT commands have request IDs/deadlines in the application layer even when the external
  protocol lacks them.

Mock servers preserve real request/response fixtures for regression tests.
