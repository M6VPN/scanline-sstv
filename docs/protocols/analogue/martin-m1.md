# Martin M1 protocol evidence

Status: **accepted for offline diagnostic-pattern and prepared-image transmission only**

This record freezes the evidence used by the M1a Martin M1 test-pattern encoder. It does
not establish receive interoperability, arbitrary-image support, live audio, or live
transmit capability.

## Sources

### Image Communication on Short Waves

- **Title and author:** *Image Communication on Short Waves*, Martin Bruchanov, OK2MNM.
- **Version:** PDF dated 2019-11-17 in the preface and PDF metadata.
- **URL:** <https://www.sstv-handbook.com/download/sstv-handbook.pdf>
- **Locations used:** section 2.2, page 10; section 3.6.2 and figures 3.11-3.12,
  pages 24-26; section 4.2.3, figures 4.5-4.6 and table 4.4, pages 35-38.
- **Licence:** No licence statement was found in the artifact. It is used only as a factual
  protocol citation; no text, image, or code is copied into the implementation.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`.

The handbook defines the common SSTV video band as 1500 Hz for black through 2300 Hz
for white, with 1200 Hz sync. It documents standard VIS framing, Martin M1 VIS code 44,
320 by 256 pixels, GBR channel order, 4.862 ms line sync, 0.572 ms black gaps, and
146.432 ms per colour scan.

### PySSTV

- **Title and project:** PySSTV, Andras Veres-Szentkiralyi and contributors.
- **Version:** tag `v0.5.8`, commit
  `884cddb36844c745fcfc6d69eab166b3b23a442b`.
- **URL:** <https://github.com/dnet/pySSTV/tree/884cddb36844c745fcfc6d69eab166b3b23a442b>
- **Locations used:** `pysstv/sstv.py` symbols `FREQ_*`, `MSEC_VIS_*`,
  `SSTV.gen_freq_bits`, and `byte_to_freq`; `pysstv/color.py` classes `ColorSSTV` and
  `MartinM1`; `pysstv/tests/assets/MartinM1_freq_bits.p` for an independently frozen
  event vector.
- **Licence:** MIT.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `pysstv/sstv.py`
  `e467e51a2ccf9ed86f6ede9ce6be161113a788b818a947434c6db3caecdab4b7`;
  `pysstv/color.py`
  `f52fb9d8fa8f0043180818a4560a0817ecdfee053d15d3fc76ca6f14a1317999`;
  `MartinM1_freq_bits.p`
  `ff4aaa5cdac3ab31a98f25c6fcaa4d9650aa71c9c3100c5950196dd80ee92259`.

PySSTV independently agrees with the handbook values and supplies the reference event
sequence used to freeze the compact project vector. No PySSTV code is copied or adapted.
The resulting project artifacts are `reference.json`, SHA-256
`a0dbc65688ed56b03f0c2383e096dcd7f98aa18922422bc0e57b1e40d928a838`, and
`generate_reference.py`, SHA-256
`db29c2a3fb2b91bdca7eef026d8e5200ca892c71b80a95bfab05b605ae749091`.

### QSSTV

- **Title and project:** QSSTV, Johan Maes, ON4QZ, and contributors.
- **Version:** commit `8c27d6d169d8c6c197eb47c2089870e39bc06a02`, dated
  2026-03-14.
- **URL:** <https://github.com/ON4QZ/QSSTV/tree/8c27d6d169d8c6c197eb47c2089870e39bc06a02>
- **Locations used:** `src/sstv/sstvtx.cpp`, function `sstvTx::sendVIS`;
  `src/sstv/sstvparam.cpp`, `SSTVTable`; `src/sstv/modes/modegbr.cpp`, functions
  `modeGBR::setupParams` and `modeGBR::txSetupLine`.
- **Licence:** The repository contains GPL-3.0; the inspected `modegbr.cpp` header says
  GPL-2.0-or-later. Both are compatible with this project. No code is copied or adapted.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `sstvtx.cpp`
  `60c435380ef49056570125dd6d08eaf9ea920e4d06ba0d025c7d0e02233a2891`;
  `sstvparam.cpp`
  `2144682be94fdd090207886503b51b66a52687073eda5a85ca1c1ba4100694af`;
  `modegbr.cpp`
  `047a944d28da399cbaef40b6fa5d635c08dd904a00d07ca2cd9a7a7342080876`.

QSSTV provides an attributable interoperability cross-check for VIS ordering and the
Martin GBR strategy. Its table stores `0xAC`, which is the seven-bit code `0x2C` with the
even-parity bit already set.

## Resolved wire values

| Field | Exact value | Evidence |
| --- | --- | --- |
| VIS code | 44 (`0x2C`), seven bits | Handbook; PySSTV |
| VIS order | Least-significant data bit first | Handbook; PySSTV; QSSTV |
| VIS parity | Even over seven data bits plus parity | Handbook; PySSTV; QSSTV table representation |
| VIS framing | 1900 Hz 300 ms, 1200 Hz 10 ms, 1900 Hz 300 ms, 1200 Hz start, seven data bits, parity, 1200 Hz stop; each bit 30 ms | Handbook; PySSTV |
| VIS data tones | 1100 Hz one; 1300 Hz zero | Handbook; PySSTV; QSSTV |
| Dimensions | 320 by 256 RGB8 pixels | Handbook; PySSTV; QSSTV |
| Channel order | Green, blue, red | Handbook; PySSTV; QSSTV `modeGBR` |
| Line sync | 1200 Hz for 4.862 ms | Handbook; PySSTV |
| Porch/separators | 1500 Hz for 0.572 ms before green, between channels, and after red | Handbook; PySSTV |
| Channel scan | 146.432 ms for each of 320 pixels | Handbook; PySSTV |
| Pixel mapping | Linear: `1500 + 800 * value / 255` Hz | Handbook endpoints; PySSTV `byte_to_freq` |
| Image duration | 256 times 446.446 ms = 114.290176 s | Exact calculation from the selected timings |
| Full duration | 910 ms VIS header plus image = 115.200176 s | Exact calculation from the selected timings |

Martin M1 data bits for code 44 are `0, 0, 1, 1, 0, 1, 0` on the wire. They contain
three ones, so the even-parity bit is one. Including framing, the VIS bit frequencies are
`1200, 1300, 1300, 1100, 1100, 1300, 1100, 1300, 1100, 1200` Hz.

## QSSTV timing divergence

The inspected QSSTV parameter table uses rounded or implementation-adjusted transmit
values: 5.0 ms sync, 0.8 ms front porch, no transmit back porch, and 0.5 ms blanking.
`modeGBR::setupParams` derives the remaining visible duration from its line-length model.
Those subintervals differ from the handbook and PySSTV values above.

M1a selects the handbook's explicit Martin-system timing because an independent PySSTV
implementation and frozen event vector agree with it exactly. QSSTV is retained as a
documented compatibility comparison, not as the source of golden expectations. This is a
resolved implementation divergence, not an inferred average between conflicting values.

## Project waveform policy

The offline diagnostic renderer uses an explicit amplitude of `0.8` for every event. This
is a project output-level choice, not an on-air protocol constant. Samples are not
normalised, limited, dithered, or automatically played. PCM16 conversion rejects
non-finite samples, clips only outside `[-1, 1]`, maps `-1` to `-32768` and `+1` to
`32767`, and rounds other values to the nearest integer with halfway cases away from zero.

M1B does not revise this evidence or any waveform constant. Arbitrary raster inputs are
prepared into the same immutable 320 by 256 RGB8 frame contract and passed to the frozen
M1a encoder. Live TX and receive remain unsupported.
