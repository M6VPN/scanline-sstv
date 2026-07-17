# miniaudio dependency record

Scanline SSTV vendors the miniaudio single-file library for backend discovery only.

| Field | Value |
| --- | --- |
| Project | miniaudio by David Reid |
| Repository | https://github.com/mackron/miniaudio |
| Release | 0.11.25 |
| Commit | `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d` |
| Retrieved | 2026-07-17 |
| Licence used | MIT No Attribution, with the complete upstream dual-licence text retained in `LICENSE` |

## Imported files

| File | SHA-256 |
| --- | --- |
| `miniaudio.h` | `ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89` |
| `miniaudio.c` | `ab1984bb9804ffd7b0303813595d0b345a8a86c34da1daffc353a14b34102a65` |
| `LICENSE` | `457f1b500e0adf6bc059edddfa78a2f62012e7c3bb43476c20e0bd23b25ba0eb` |

## Configuration

The implementation enables only ALSA, PulseAudio, JACK, OSS, sndio, audio(4), and null
backends. Decoders, encoders, resource management, the node graph, engine, generators,
and format codecs are disabled. Scanline SSTV calls only context initialization,
device enumeration, selected safe capability queries, and context teardown. It never
initializes a miniaudio device in M2A.

The enabled backend list was checked against the support and configuration tables in
`miniaudio.h` at the pinned commit. Linux support covers ALSA, PulseAudio, and JACK.
The pinned source contains OSS, sndio, and audio(4) implementations for supported BSD
systems. BSD runtime behavior is not verified by Linux CI.

## Update procedure

1. Select a tagged upstream release and record its exact commit.
2. Download `miniaudio.h`, `miniaudio.c`, and `LICENSE` from that commit.
3. Review configuration macros and every context call used by the adapter.
4. Confirm enumeration does not initialize playback or capture devices.
5. Update the hashes above and run all build, unit, CLI, and sanitizer presets.
