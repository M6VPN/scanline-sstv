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
only; live TX, receive, and other Scottie modes remain unsupported.

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

- flrig XML-RPC uses introspection where available and the documented `rig.set_ptt` and
  `rig.get_ptt` methods.
- rigctld uses extended responses where practical and parses complete `RPRT` results.
- Network endpoints default to loopback. Remote endpoints require explicit user action.
- PTT commands have request IDs/deadlines in the application layer even when the external
  protocol lacks them.

Mock servers preserve real request/response fixtures for regression tests.
