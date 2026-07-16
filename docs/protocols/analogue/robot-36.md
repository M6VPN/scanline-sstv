# Robot 36 protocol evidence

Status: **accepted for offline diagnostic-pattern and prepared-image transmission only**

This record freezes the evidence used by the M1D Robot 36 offline encoder. It does not
establish receive interoperability, live audio, live transmission, or support for another
Robot mode.

## Sources

### Proposal for SSTV Mode Specifications

- **Title and author:** *Proposal for SSTV Mode Specifications*, JL Barber, N7CXI,
  Silicon Pixels.
- **Version:** Presented at the Dayton SSTV forum on 2000-05-20; no revision identifier
  appears in the inspected artifact.
- **URL:** <https://www.yumpu.com/en/document/view/7259948/proposal-for-sstv-mode-specifications>
- **Locations used:** pages 1-2, "Data sources" and "VIS Code and Robot calibration
  header"; pages 5-6, "ROBOT 36 COLOR"; pages 13-14, appendices A and B.
- **Licence:** The paper permits use or publication of its proposed specifications. No
  software licence is stated. It is used only as a factual protocol citation; no text,
  image, or code is copied into the implementation.
- **Retrieved:** 2026-07-16.
- **SHA-256:** Inspected HTML artifact
  `6bc160a4cd8f67c708983c011afececc16ba37c7196f057929f43d5ff17d9e59`.

The proposal identifies its Robot evidence as Robot 1200C firmware data. It defines VIS
code 8, 320 by 240 pixels, 240 lines, 9.0 ms sync, 3.0 ms sync porch, 88.0 ms luma,
4.5 ms component identification, 1.5 ms porch, and 44.0 ms chroma. Even lines carry
two-line-averaged R-Y after a 1500 Hz identifier; odd lines carry two-line-averaged B-Y
after a 2300 Hz identifier. Each line is exactly 150 ms, so the image lasts 36 seconds.

Appendix B explicitly operates on nonlinear 8-bit RGB values and defines limited-range
BT.601-derived Y, R-Y, and B-Y codes. It also warns that similarly named colour models
use different scaling. The project therefore names the transmitted components luma Y,
red difference, and blue difference instead of treating them as generic U/V or assuming
full-range YCbCr.

### SSTV Encoder

- **Title and project:** SSTV Encoder, Olga Miller.
- **Version:** version 2.13, commit
  `94821b169700e6db27452d9b2b40642c677f38fa`.
- **URL:** <https://github.com/olgamiller/SSTVEncoder2/tree/94821b169700e6db27452d9b2b40642c677f38fa>
- **Locations used:** `Modes/Robot36.java`, constructor and scan methods;
  `Modes/ImageFormats/YuvConverter.java`, `convertToY`, `convertToU`, `convertToV`, and
  `clamp`; `Modes/ImageFormats/NV21.java`, `convertBitmapToYuv`, `getU`, and `getV`.
- **Licence:** Apache-2.0. No code is copied or adapted.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `Robot36.java`
  `98c45c018ef438f53bc1c41bbb1f10ea4d0e0de4701ff20cae13847575626921`;
  `YuvConverter.java`
  `54469280a2d01718bf23d394e119807aafaa244833432e00d8505d14a1c46381`;
  `NV21.java`
  `ff3fa07158ed8bd21e6e60c19dfe4bd3ae0684a8eab5f26f8e41b68ef2adda6c`;
  `LICENSE`
  `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`.

This independent encoder follows the Dayton conversion coefficients and timing. It
converts each source pixel first, truncates each converted component toward zero, then
averages each 2 by 2 block of converted chroma codes with integer division. Each averaged
code is reused for both horizontal pixels and both source rows. The even line transmits
red difference after 1500 Hz and the odd line transmits blue difference after 2300 Hz.

### Image Communication on Short Waves

- **Title and author:** *Image Communication on Short Waves*, Martin Bruchanov, OK2MNM.
- **Version:** PDF dated 2019-11-17 in the preface and PDF metadata.
- **URL:** <https://www.sstv-handbook.com/download/sstv-handbook.pdf>
- **Locations used:** section 2.2, page 10; section 3.5.2, pages 20-21; section 4.2.2,
  table 4.3 and figures 4.3-4.4, pages 32-35.
- **Licence:** No licence statement was found in the artifact. It is used only as a factual
  protocol citation; no text, image, or code is copied into the implementation.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d`.

The handbook agrees on VIS code 8, 320 by 240 dimensions, 4:2:0 colour treatment,
two-line chroma averaging, and the R-Y/B-Y alternating line concept. Its condensed table
groups the line as 10.5 ms sync, 90 ms luma, and 45 ms chroma intervals. Those groups are
consistent with the Dayton decomposition of 9+3, 88, and 4.5+1.5+44 ms, respectively.
The handbook prose reverses the identifier association in one passage; the Dayton
sequence and both attributable encoders resolve even R-Y as 1500 Hz and odd B-Y as
2300 Hz.

### QSSTV

- **Title and project:** QSSTV, Johan Maes, ON4QZ, and contributors.
- **Version:** commit `8c27d6d169d8c6c197eb47c2089870e39bc06a02`, dated
  2026-03-14.
- **URL:** <https://github.com/ON4QZ/QSSTV/tree/8c27d6d169d8c6c197eb47c2089870e39bc06a02>
- **Locations used:** `src/sstv/sstvtx.cpp`, `sstvTx::sendVIS`;
  `src/sstv/sstvparam.cpp`, `SSTVTable`; `src/sstv/modes/modebase.cpp`,
  `modeBase::getLineY`; `src/sstv/modes/moderobot1.cpp`, `modeRobot1::setupParams` and
  `modeRobot1::txSetupLine`.
- **Licence:** GPL-3.0 repository; inspected mode files state GPL-2.0-or-later. No code is
  copied or adapted.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `sstvtx.cpp`
  `60c435380ef49056570125dd6d08eaf9ea920e4d06ba0d025c7d0e02233a2891`;
  `sstvparam.cpp`
  `2144682be94fdd090207886503b51b66a52687073eda5a85ca1c1ba4100694af`;
  `modebase.cpp`
  `29b57d1c72bc541d731b2f1e826d76005b53fc9960423d812a357416fe8193d6`;
  `moderobot1.cpp`
  `68cbbd58d37a552574f1dc26dd8fb7fed474af135ef702a124ae323d7f0c5b8d`.

QSSTV agrees on dimensions, alternating two-line chroma reuse, even red-difference and
odd blue-difference order, and identifier tones. It uses a full-range integer colour
matrix, no horizontal chroma averaging, and adjusted TX intervals derived from a
36.002-second image duration. Those values are an interoperability comparison and are
not mixed with the Dayton definition.

### PySSTV

- **Title and project:** PySSTV, Andras Veres-Szentkiralyi and contributors.
- **Version:** tag `v0.5.8`, commit
  `884cddb36844c745fcfc6d69eab166b3b23a442b`.
- **URL:** <https://github.com/dnet/pySSTV/tree/884cddb36844c745fcfc6d69eab166b3b23a442b>
- **Locations used:** `pysstv/sstv.py`, VIS and byte-frequency helpers;
  `pysstv/color.py`, class `Robot36`.
- **Licence:** MIT.
- **Retrieved:** 2026-07-16.
- **SHA-256:** `sstv.py`
  `e467e51a2ccf9ed86f6ede9ce6be161113a788b818a947434c6db3caecdab4b7`;
  `color.py`
  `f52fb9d8fa8f0043180818a4560a0817ecdfee053d15d3fc76ca6f14a1317999`.

PySSTV agrees on VIS, dimensions, the exact 150 ms line schedule, and video-frequency
mapping. It relies on Pillow YCbCr conversion, does not average chroma over line pairs,
and reverses the identifier/component association. It is not used for Robot colour or
line-parity expectations.

## Resolved wire values

| Field | Exact value | Evidence |
| --- | --- | --- |
| VIS code | 8 (`0x08`), seven bits | Dayton; SSTV Encoder; PySSTV |
| VIS order and parity | Least-significant bit first; even parity | Dayton; shared accepted VIS evidence |
| Dimensions | 320 by 240 sRGB RGB8 input pixels | Dayton; SSTV Encoder; QSSTV |
| Colour definition | Limited-range BT.601-derived luma Y, red difference, blue difference from nonlinear RGB | Dayton Appendix B; SSTV Encoder |
| Luma conversion | `trunc(16 + (65.738R + 129.057G + 25.064B) * 0.003906)` | Dayton; SSTV Encoder |
| Red-difference conversion | `trunc(128 + (112.439R - 94.154G - 18.285B) * 0.003906)` | Dayton; SSTV Encoder V |
| Blue-difference conversion | `trunc(128 + (-37.945R - 74.494G + 112.439B) * 0.003906)` | Dayton; SSTV Encoder U |
| Conversion clamp | Clamp the real result to 0 through 255, then truncate toward zero | SSTV Encoder |
| Chroma subsampling | Convert first; average each 2 by 2 block with integer division by four; reuse the result for both columns and rows | SSTV Encoder NV21 |
| Line order | Even: sync, porch, Y, 1500 Hz ID, 1900 Hz porch, red difference; odd: same with 2300 Hz ID and blue difference | Dayton; SSTV Encoder; QSSTV |
| Line sync and porch | 1200 Hz 9 ms; 1500 Hz 3 ms | Dayton; SSTV Encoder; PySSTV |
| Luma scan | 88 ms, 320 values | Dayton; SSTV Encoder |
| Component ID and porch | 1500 Hz even or 2300 Hz odd for 4.5 ms; 1900 Hz for 1.5 ms | Dayton; SSTV Encoder; QSSTV |
| Chroma scan | 44 ms, 160 averaged values each represented for two adjacent pixel intervals | Dayton period; SSTV Encoder NV21 reuse |
| Component mapping | Linear: `1500 + 800 * code / 255` Hz | Dayton Appendix B |
| Line duration | 150 ms | Exact selected interval sum |
| Image duration | 240 times 150 ms = 36 s | Dayton |
| Full duration | 910 ms VIS header plus image = 36.910 s | Exact calculation |

Robot 36 data bits for code 8 are `0, 0, 0, 1, 0, 0, 0` on the wire. They contain one
one, so the even-parity bit is one. Including framing, the VIS frequencies are
`1900, 1200, 1900, 1200, 1300, 1300, 1300, 1100, 1300, 1300, 1300, 1100, 1200` Hz.

## Deterministic arithmetic

The decimal coefficients are represented as exact integers over one million:
`0.003906 * 65.738 = 256772628 / 1000000000`, with corresponding signed numerators for
the other terms. The constant offsets use the same denominator. Each component is
clamped before truncation. All selected expressions are non-negative after the offset,
so integer division defines truncation toward zero without relying on floating-point
conversion. A conversion result exactly halfway between integers is truncated toward the
lower integer. Each four-code chroma average also uses integer division, so an average
exactly halfway between integers is truncated toward the lower integer.

This preserves the published decimal definition exactly as written instead of replacing
it with nominal BT.601 coefficients that produce different bytes. The conversion acts on
the gamma-encoded sRGB byte values supplied by the safe image boundary; it does not
linearise them.

## Resolved disagreements

The Dayton values and SSTV Encoder behaviour are selected as one internally consistent,
independently implemented definition. QSSTV's full-range matrix, lack of horizontal
averaging, and adjusted timing are not averaged into it. PySSTV's Pillow conversion,
line-local chroma, and reversed component identifiers are also rejected for the frozen
vector. The handbook's grouped intervals corroborate the exact Dayton line total, while
its reversed prose identifier association is resolved by the Dayton sequence and the two
interoperability implementations.

The compact vector under `tests/vectors/analogue/robot-36` independently calculates the
selected conversions, events, boundaries, event count, duration, and frame counts without
importing or calling Scanline SSTV production code. `generate_reference.py` has SHA-256
`07db9022f6303bae475e0bb40cc8d34d996ef781b1d27cb247662eb37d56b243` and
`reference.json` has SHA-256
`0fca9257f8b083cb7b74420c035d543140fa4bc59ab09fbe748d4b62c6928b91`.

## Project waveform policy

The offline renderer uses the existing explicit amplitude of `0.8` for every event. This
is a project output-level choice, not an on-air protocol constant. Samples are not
normalised, limited, dithered, or automatically played. The accepted PCM16 conversion
and atomic WAV publication behaviour remain unchanged.
