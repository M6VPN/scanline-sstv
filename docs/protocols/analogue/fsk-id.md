# Analogue SSTV FSK ID protocol evidence

Status: **accepted for optional offline analogue transmission suffixes only**

This record freezes the evidence used by the M1F FSK ID generator. FSK ID identifies the
sender of an analogue SSTV image and is appended after the image. It does not establish
live transmission, FSK ID reception, SSTV decoding, or regulatory station-identification
compliance.

## Sources

### DSP tutorial: SSTV encoder

- **Title and author:** *DSP tutorial: SSTV encoder*, Norbert Varga (Nonoo).
- **Version:** Web article last modified 2013-04-03; no revision identifier.
- **URL:** <https://dp.nonoo.hu/projects/ham-dsp-tutorial/21-sstv-encoder/index.html>
- **Location used:** Opening transmission sequence and FSK ID paragraph.
- **Licence:** No content licence is stated. It is used only as a factual protocol
  citation; no text or code is copied.
- **Retrieved:** 2026-07-17.
- **SHA-256:** HTML artifact
  `c501c8eaec2bed62b9b9fd8d8ba3f562f5a4a7f0385e9e89c7538cffb9ee1ccc`.

The article places optional FSK ID after image data and specifies six-bit values, least
significant bit first, 45.45 baud, 1900 Hz for one, 2100 Hz for zero, a logical `0x20,
0x2a` start sequence, an `0x01` end value, and the ASCII offset of `0x20`.

### QSSTV

- **Title and project:** QSSTV, Johan Maes, ON4QZ, and contributors.
- **Version:** commit `8c27d6d169d8c6c197eb47c2089870e39bc06a02`, dated
  2026-03-14.
- **URL:** <https://github.com/ON4QZ/QSSTV/tree/8c27d6d169d8c6c197eb47c2089870e39bc06a02>
- **Locations used:** `src/mainwidgets/txfunctions.cpp`, `TXSSTVPOST`, `sendFSKChar`, and
  `sendFSKID`; `src/sstv/sstvtx.cpp`, `FSKIDTime`; `src/sstv/visfskid.cpp`,
  `fskIdDecoder::assemble` and `fskIdDecoder::extract`; `src/sstv/visfskid.h`, FSK timing
  constants.
- **Licence:** GPL-3.0 repository; inspected files state GPL-2.0-or-later. No code is
  copied or adapted.
- **Retrieved:** 2026-07-17.
- **SHA-256:** `txfunctions.cpp`
  `03dc81452a7bd782d976fe1e005346e8b28c957d472d3f684f95cfe57b8812e7`;
  `sstvtx.cpp` `60c435380ef49056570125dd6d08eaf9ea920e4d06ba0d025c7d0e02233a2891`;
  `visfskid.cpp` `fbe04316c322abe080598abd58374c92f1e90f7e26bb338c8d8696a4e2d24ceb`;
  `visfskid.h` `f7d79a0def9d2587b5fd98af3a17d6338910d7ef7052443a51a7d13f864302c2`.

QSSTV appends FSK ID immediately after image transmission when CW identification is not
selected. It sends 300 ms at 1500 Hz, 100 ms at 2100 Hz, and 22 ms at 1900 Hz. The final
two tones represent the six-bit logical `0x20` preamble with five zero bits compressed to
100 ms and its final one bit sent for 22 ms. QSSTV then sends `0x2a`, the identifier,
`0x01`, the XOR checksum masked to six bits, and 100 ms at 1900 Hz. Six-bit values are
sent least significant bit first with 22 ms per bit. Its decoder rejects identifiers
longer than nine characters and reconstructs characters by adding `0x20`.

### mySSTV

- **Title and author:** *mySSTV, simple SSTV encoder*, Rae Forbes-Richardson (VK2GPU).
- **Version:** Gist commit `f090d7aceea5df18f831ad10beaeb0e49461e488`, dated
  2022-07-22.
- **URL:** <https://gist.github.com/vk2gpu/e2107abf62325b5d4b7afc6a575b76aa>
- **Locations used:** `main.c`, functions `fsk_ch` and `fsk_id`.
- **Licence:** No licence is stated. It is used only as an independent implementation
  comparison; no code is copied or adapted.
- **Retrieved:** 2026-07-17.
- **SHA-256:** `main.c`
  `23206ee0503893789f125bd0cca0be4b51aa0ecef5e2d5812395273c25aa54fa`.

This independent encoder matches QSSTV's complete transmitted sequence, including the
300 ms leader, shortened 100 ms zero run, 22 ms mark, `0x2a`, identifier, `0x01`, XOR
checksum, and 100 ms trailing mark.

### libsstvenc

- **Title and author/project:** *libsstvenc*, Stuart Longland, VK4MSL.
- **Version:** commit `01b60005405f9c33e79a233ce09b7ac05a3d6300`, dated
  2025-05-30.
- **URL:** <https://github.com/sjlongland/libsstvenc/tree/01b60005405f9c33e79a233ce09b7ac05a3d6300>
- **Locations used:** `src/sstv.c`, FSK state machine and `sstvenc_encoder_fsk_*` symbols;
  `include/libsstvenc/sstvmode.h`, `SSTVENC_PERIOD_FSKID_BIT`;
  `include/libsstvenc/sstvfreq.h`, FSK frequency constants.
- **Licence:** MIT. No code is copied or adapted.
- **Retrieved:** 2026-07-17.
- **SHA-256:** `sstv.c`
  `50ece2f4fd253ba5f14b80ad43014c71d3c6c0aaee1548fddbd06112c20503c9`;
  `sstvmode.h` `60aac42b386e884ab0538e9f380311450557aa7322cb55f04dd4acb87c8340d9`;
  `sstvfreq.h` `031c327f2dd87071eb098bb6415ff81122571a709196470889fac8128ff24599`.

libsstvenc corroborates 22 ms bits, LSB-first order, 1900/2100 Hz mapping, the logical
`0x20, 0x2a` prefix, six-bit ASCII offset, and `0x01` terminator. It does not send the
QSSTV/mySSTV physical leader, checksum, or trailing mark. That variant is documented but
is not selected for the project suffix.

No inspectable, version-pinned MMSTV source or vendor wire-format document was located.
The project therefore makes no unverified MMSTV-specific framing claim. The selected
sequence is evidenced by QSSTV and independently matched by mySSTV; differences from the
attributable dreamport description and libsstvenc are recorded below.

## Independent project vector

The standalone standard-library generator at
`tests/vectors/analogue/fsk-id/generate_reference.py` does not import, invoke, or read
production code. It freezes `M6VPN-1`, all ordered events, exact cumulative boundaries,
the XOR checksum, duration, event count, and frame counts at every supported rate.

- Generator SHA-256:
  `257e52ad1cb68de9f071b3de2a53f323e223133074ca8184d044a7e80b9dc6c6`.
- `reference.json` SHA-256:
  `75cb52395809c8cf02f930057a8712bc095929e3e4ac5aa3e5ee40690e1f2547`.

## Resolved wire values

| Field | Exact value |
| --- | --- |
| Purpose | Optional sender identifier appended after an analogue SSTV image |
| Placement | Immediately after the final image event; no inserted silence |
| Leader | 300 ms at 1500 Hz |
| Separate break or gap | None; the following space/mark tones encode logical `0x20` |
| Logical start | `0x20`, physically 100 ms at 2100 Hz then 22 ms at 1900 Hz |
| Header value | Six-bit `0x2a` |
| Character encoding | ASCII character minus `0x20`, six bits |
| Permitted normalized characters | ASCII `0x20` through `0x5f` |
| Case rule | ASCII `a` through `z` is converted to uppercase before validation |
| Maximum identifier length | Nine characters |
| Bit order | Least significant bit first |
| Bit duration and rate | 22 ms exactly; `500/11` baud, approximately 45.4545 baud |
| Logic one / mark | 1900 Hz |
| Logic zero / space | 2100 Hz |
| Per-character framing | Six contiguous data bits; no separate start or stop bit |
| End value | Six-bit `0x01` |
| Checksum | XOR of normalized identifier six-bit values, masked with `0x3f` |
| Trailer | 100 ms at 1900 Hz |

The identifier must contain one through nine characters. Empty strings, embedded NULs,
control characters, bytes outside ASCII, and normalized characters outside `0x20` through
`0x5f` are rejected. The generator never truncates, trims, or transliterates. ASCII case
conversion is selected because lowercase is outside the six-bit alphabet and QSSTV's
transmitter uppercases identifier characters.

For an identifier of length `n`, the suffix contains `6n + 22` tone events and has exact
duration `918 ms + 132n ms`. The first three events are the physical leader/start
sequence, the next six encode `0x2a`, then come the identifier, `0x01`, checksum, and one
100 ms trailing event.

## Resolved disagreements

The dreamport description and libsstvenc express the prefix as ordinary logical values
`0x20, 0x2a`. QSSTV and mySSTV instead transmit the zero run of `0x20` as 100 ms rather
than five independent 22 ms events, making that prefix 10 ms shorter. The latter two
independent implementations agree sample-for-sample at the event level and QSSTV's
decoder explicitly accepts that physical structure, so it is selected.

QSSTV and mySSTV append an XOR checksum and 100 ms trailing mark; libsstvenc does not.
The checksum-bearing form is selected because it is produced and decoded by the pinned
interoperability implementation and independently reproduced by mySSTV.

QSSTV's `FSKIDTime` preview omits the final 100 ms mark even though `sendFSKID` transmits
it. The project calculates duration from the actual selected event sequence and therefore
includes the trailer.

## Project waveform policy

The suffix uses the same explicit amplitude as its base transmission. It is composed once
by the central offline analogue service and rendered as one continuous event stream, so
oscillator phase remains continuous at the image/FSK boundary. No audio is played and no
radio or PTT path is accessed.
