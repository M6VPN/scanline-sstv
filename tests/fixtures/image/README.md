# M1B image fixtures

These files are project-generated test artifacts. `generate_fixtures.py` builds the PNG
rasters with Python 3 standard-library PNG primitives, then uses the installed libvips
command-line tool for native JPEG, CMYK JPEG, and embedded-sRGB-profile output. No source
image or third-party creative work is included.

Regenerate from the repository root with:

```sh
tests/fixtures/image/generate_fixtures.py
```

Generation was last verified with Python 3.12.3 and libvips 8.15.1. `SHA256SUMS` freezes
the compact artifacts consumed by the tests. The orientation files use a minimal EXIF
TIFF directory with each value from 1 through 8 and a recognisable corner marker raster.
