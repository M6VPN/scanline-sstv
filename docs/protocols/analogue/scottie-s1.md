# Scottie S1 protocol evidence

Status: **accepted for offline diagnostic-pattern and prepared-image transmission only**

This record freezes the evidence used by the M1C Scottie S1 offline encoder. It does not
establish receive interoperability, live audio, live transmission, or support for another
Scottie mode.

## Sources

### Image Communication on Short Waves

- **Title and author:** *Image Communication on Short Waves*, Martin Bruchanov, OK2MNM.
- **Version:** PDF dated 2019-11-17 in the preface and PDF metadata.
- **URL:** <https://www.sstv-handbook.com/download/sstv-handbook.pdf>
- **Locations used:** section 2.2, page 10; section 3.6.2, table 3.2, page 26;
  section 4.2.3, figures 4.5-4.6, pages 35-38; section 4.2.4 and table 4.5,
  pages 38-40.
- **Licence:** No licence statement was found in the artifact. It is used only as a factual
  protocol citation; no text, image, or code is copied into the implementation.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`.

The handbook identifies Scottie S1 VIS code 60, 320 by 256 pixels, GBR colour order,
9.0 ms sync, 1.5 ms 1500 Hz gaps, and 138.240 ms per colour scan. It describes each
line after vertical synchronization as a 1500 Hz gap, green scan, 1500 Hz gap, blue
scan, 1200 Hz sync, 1500 Hz gap, then red scan. It separately notes a Robot 1200C
variation with an additional 9.0 ms sync before the first line. The project implements
the described Scottie sequence, not that Robot 1200C variation.

The handbook also defines the common 1200 Hz sync tone, 1500 Hz black through 2300 Hz
white video range, and standard seven-bit LSB-first even-parity VIS framing used by the
accepted shared VIS generator.

### PySSTV

- **Title and project:** PySSTV, Andras Veres-Szentkiralyi and contributors.
- **Version:** tag `v0.5.8`, commit
  `884cddb36844c745fcfc6d69eab166b3b23a442b`.
- **URL:** <https://github.com/dnet/pySSTV/tree/884cddb36844c745fcfc6d69eab166b3b23a442b>
- **Locations used:** `pysstv/sstv.py` symbols `FREQ_*`, `MSEC_VIS_*`,
  `SSTV.gen_freq_bits`, `SSTV.horizontal_sync`, and `byte_to_freq`;
  `pysstv/grayscale.py`, `GrayscaleSSTV.gen_image_tuples`; `pysstv/color.py`,
  classes `ColorSSTV`, `MartinM1`, and `ScottieS1`.
- **Licence:** MIT.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `pysstv/sstv.py`
  `e467e51a2ccf9ed86f6ede9ce6be161113a788b818a947434c6db3caecdab4b7`;
  `pysstv/grayscale.py`
  `b0a623ee947d9b385e922c29ff5e9a072b145ae0dee70a0b6801d9b083348b79`;
  `pysstv/color.py`
  `f52fb9d8fa8f0043180818a4560a0817ecdfee053d15d3fc76ca6f14a1317999`.

PySSTV agrees on VIS code, dimensions, GBR order, tone mapping, total 428.220 ms line
duration, and sync placement before red. Its inherited channel hooks divide the line into
136.740 ms visible scans and six 1.5 ms gap events, including adjacent same-frequency
events between channels. This differs from the handbook's explicit 138.240 ms visible
scans and three 1.5 ms gaps, although both arrangements have the same total duration.
PySSTV has no additional sync before the first line.

### QSSTV

- **Title and project:** QSSTV, Johan Maes, ON4QZ, and contributors.
- **Version:** commit `8c27d6d169d8c6c197eb47c2089870e39bc06a02`, dated
  2026-03-14.
- **URL:** <https://github.com/ON4QZ/QSSTV/tree/8c27d6d169d8c6c197eb47c2089870e39bc06a02>
- **Locations used:** `src/sstv/sstvtx.cpp`, function `sstvTx::sendVIS`;
  `src/sstv/sstvparam.cpp`, `SSTVTable`; `src/sstv/modes/modegbr2.cpp`,
  functions `modeGBR2::setupParams`, `modeGBR2::calcPixelPositionTable`, and
  `modeGBR2::txSetupLine`.
- **Licence:** The repository contains GPL-3.0; the inspected `modegbr2.cpp` header says
  GPL-2.0-or-later. Both are compatible with this project. No code is copied or adapted.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `sstvtx.cpp`
  `60c435380ef49056570125dd6d08eaf9ea920e4d06ba0d025c7d0e02233a2891`;
  `sstvparam.cpp`
  `2144682be94fdd090207886503b51b66a52687073eda5a85ca1c1ba4100694af`;
  `modegbr2.cpp`
  `5e05245c1901f5e1048ab21ad3c32e05d7246449881698b1f9fcd9486db3e415`.

QSSTV agrees on VIS code 60, 320 by 256 dimensions, GBR order, and sync between blue
and red. Its current TX table uses an adjusted 109.63250 s image duration, 9.0 ms sync,
0.8 ms front and back porches, and 1.25 ms blanks. Its TX state begins with green and
ends each line with a blank, so the first-line boundary also differs from the handbook.

## Resolved wire values

| Field | Exact value | Evidence |
| --- | --- | --- |
| VIS code | 60 (`0x3C`), seven bits | Handbook; PySSTV; QSSTV |
| VIS order | Least-significant data bit first | Handbook; PySSTV; QSSTV |
| VIS parity | Even over seven data bits plus parity | Handbook; PySSTV; QSSTV |
| VIS framing | 1900 Hz 300 ms, 1200 Hz 10 ms, 1900 Hz 300 ms, 1200 Hz start, seven data bits, parity, 1200 Hz stop; each bit 30 ms | Handbook; PySSTV |
| VIS data tones | 1100 Hz one; 1300 Hz zero | Handbook; PySSTV; QSSTV |
| Dimensions | 320 by 256 RGB8 pixels | Handbook; PySSTV; QSSTV |
| Channel order | Green, blue, red | Handbook; PySSTV; QSSTV `modeGBR2` |
| Line order | 1500 Hz gap, green, 1500 Hz gap, blue, 1200 Hz sync, 1500 Hz gap, red | Handbook |
| First line | Same line order, with no additional leading sync | Handbook base sequence; PySSTV |
| Line sync | 1200 Hz for 9.0 ms between blue and red | Handbook; PySSTV; QSSTV |
| Gaps | 1500 Hz for 1.5 ms before green, before blue, and before red | Handbook |
| Channel scan | 138.240 ms for each of 320 pixels | Handbook table 4.5 |
| Pixel mapping | Linear: `1500 + 800 * value / 255` Hz | Handbook endpoints; PySSTV `byte_to_freq` |
| Line duration | 428.220 ms | Exact calculation from selected timings; PySSTV total |
| Image duration | 256 times 428.220 ms = 109.624320 s | Exact calculation from selected timings |
| Full duration | 910 ms VIS header plus image = 110.534320 s | Exact calculation from selected timings |

Scottie S1 data bits for code 60 are `0, 0, 1, 1, 1, 1, 0` on the wire. They contain
four ones, so the even-parity bit is zero. Including framing, the VIS frequencies are
`1900, 1200, 1900, 1200, 1300, 1300, 1100, 1100, 1100, 1100, 1300, 1300, 1200` Hz.

## Resolved disagreements

The production encoder follows the handbook's explicit visible-scan and gap boundaries.
PySSTV is retained as an independently implemented cross-check for total duration, sync
placement, VIS, dimensions, colour order, and tone mapping, but not for individual pixel
boundaries. QSSTV is retained as an interoperability comparison; its adjusted timing
table and first-line placement are not averaged into the selected values.

The compact vector under `tests/vectors/analogue/scottie-s1` independently calculates the
selected event and cumulative sample boundaries without importing or calling Scanline
SSTV production code. `generate_reference.py` has SHA-256
`7dd35637b6143cefa7fc4a2e076a2b865b8529b30891f3fb7bc2b5dfdc30c43d` and
`reference.json` has SHA-256
`4b74435f2bd4263b83f15dd2fd6c33199f3cd4508a2a96524508323549cf9116`.

## Project waveform policy

The offline renderer uses the existing explicit amplitude of `0.8` for every event. This
is a project output-level choice, not an on-air protocol constant. Samples are not
normalised, limited, dithered, or automatically played. The accepted M1a PCM16 conversion
and atomic WAV publication behavior remain unchanged.
