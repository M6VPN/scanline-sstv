#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/fixtures/image/generate_fixtures.py
# SPDX-License-Identifier: GPL-3.0-or-later

import binascii
import hashlib
import pathlib
import struct
import subprocess
import zlib


ROOT = pathlib.Path(__file__).resolve().parent
ORIENTATION_ROOT = ROOT / 'orientation'
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'


def chunk(name: bytes, payload: bytes) -> bytes:
	'''Build one PNG chunk.'''

	return (struct.pack('>I', len(payload)) + name + payload
		+ struct.pack('>I', binascii.crc32(name + payload) & 0xffffffff))


def exif_payload(orientation: int) -> bytes:
	'''Build a minimal little-endian TIFF orientation directory.'''

	return (b'II\x2a\x00\x08\x00\x00\x00\x01\x00'
		+ b'\x12\x01\x03\x00\x01\x00\x00\x00'
		+ struct.pack('<H', orientation) + b'\x00\x00\x00\x00\x00\x00')


def marker_pixels() -> bytes:
	'''Return a recognisable 5 by 3 RGB marker raster.'''

	rows = [
		[(255, 0, 0), (32, 32, 32), (64, 64, 64), (96, 96, 96), (0, 255, 0)],
		[(16, 16, 16), (255, 255, 0), (0, 255, 255), (255, 0, 255), (128, 128, 128)],
		[(0, 0, 255), (160, 160, 160), (192, 192, 192), (224, 224, 224), (255, 255, 255)],
	]
	return bytes(channel for row in rows for pixel in row for channel in pixel)


def png_bytes(width: int, height: int, colour_type: int, bit_depth: int,
	pixels: bytes, extra_chunks: list[tuple[bytes, bytes]] | None = None) -> bytes:
	'''Build a non-interlaced PNG with filter type zero.'''

	channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour_type]
	row_bytes = width * channels * bit_depth // 8
	raw = b''.join(b'\x00' + pixels[offset:offset + row_bytes]
		for offset in range(0, len(pixels), row_bytes))
	header = struct.pack('>IIBBBBB', width, height, bit_depth, colour_type, 0, 0, 0)
	chunks = [chunk(b'IHDR', header)]
	for name, payload in extra_chunks or []:
		chunks.append(chunk(name, payload))
	chunks.extend((chunk(b'IDAT', zlib.compress(raw, 9)), chunk(b'IEND', b'')))
	return PNG_SIGNATURE + b''.join(chunks)


def run_vips(*arguments: str):
	'''Run libvips for fixtures that require native JPEG or ICC encoding.'''

	subprocess.run(('vips', *arguments), check=True)


def write_animated_png(path: pathlib.Path):
	'''Write a valid two-frame 1 by 1 APNG.'''

	header = chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
	animation = chunk(b'acTL', struct.pack('>II', 2, 0))
	frame_zero = chunk(b'fcTL', struct.pack('>IIIIIHHBB', 0, 1, 1, 0, 0, 1, 10, 0, 0))
	red = chunk(b'IDAT', zlib.compress(b'\x00\xff\x00\x00\xff', 9))
	frame_one = chunk(b'fcTL', struct.pack('>IIIIIHHBB', 1, 1, 1, 0, 0, 1, 10, 0, 0))
	blue_data = struct.pack('>I', 2) + zlib.compress(b'\x00\x00\x00\xff\xff', 9)
	path.write_bytes(PNG_SIGNATURE + header + animation + frame_zero + red
		+ frame_one + chunk(b'fdAT', blue_data) + chunk(b'IEND', b''))


def write_hashes():
	'''Write SHA-256 hashes for generated fixture artifacts.'''

	paths = sorted(path for path in ROOT.rglob('*')
		if path.is_file() and path.name not in {'SHA256SUMS', 'generate_fixtures.py', 'README.md'})
	lines = [f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(ROOT)}'
		for path in paths]
	(ROOT / 'SHA256SUMS').write_text('\n'.join(lines) + '\n', encoding='ascii')


def write_png_fixtures():
	'''Write compact PNG fixtures using only Python standard-library code.'''

	marker = marker_pixels()
	(ROOT / 'marker.png').write_bytes(png_bytes(5, 3, 2, 8, marker,
		[(b'tEXt', b'Comment\x00must be stripped')]))
	(ROOT / 'spoofed.jpg').write_bytes(png_bytes(5, 3, 2, 8, marker))
	gray = bytes((0, 64, 128, 192, 255, 32, 96, 160, 224))
	(ROOT / 'gray.png').write_bytes(png_bytes(3, 3, 0, 8, gray))
	gray16 = b''.join(struct.pack('>H', value) for value in (0, 32768, 65535, 8192))
	(ROOT / 'gray16.png').write_bytes(png_bytes(2, 2, 0, 16, gray16))
	alpha = bytes((
		255, 0, 0, 0, 255, 0, 0, 64, 0, 0, 255, 128,
		255, 255, 255, 192, 0, 0, 0, 255, 255, 0, 0, 255,
	))
	(ROOT / 'alpha.png').write_bytes(png_bytes(3, 2, 6, 8, alpha))
	invalid_profile = b'Broken profile\x00\x00' + zlib.compress(b'not an ICC profile', 9)
	(ROOT / 'invalid-profile.png').write_bytes(png_bytes(5, 3, 2, 8, marker,
		[(b'iCCP', invalid_profile)]))
	(ROOT / 'oversized-header.png').write_bytes(
		png_bytes(20_000, 1, 2, 8, bytes(20_000 * 3)))
	(ROOT / 'truncated.png').write_bytes((ROOT / 'marker.png').read_bytes()[:41])
	ORIENTATION_ROOT.mkdir(exist_ok=True)
	for orientation in range(1, 9):
		(ORIENTATION_ROOT / f'orientation-{orientation}.png').write_bytes(
			png_bytes(5, 3, 2, 8, marker, [(b'eXIf', exif_payload(orientation))]))
	(ORIENTATION_ROOT / 'orientation-9.png').write_bytes(
		png_bytes(5, 3, 2, 8, marker, [(b'eXIf', exif_payload(9))]))
	write_animated_png(ROOT / 'animated.png')


def write_vips_fixtures():
	'''Write JPEG, CMYK, and embedded-profile fixtures with libvips.'''

	run_vips('jpegsave', str(ROOT / 'marker.png'), str(ROOT / 'marker.jpg'),
		'--Q', '95', '--strip')
	run_vips('colourspace', str(ROOT / 'marker.png'), str(ROOT / 'cmyk.v'), 'cmyk')
	run_vips('jpegsave', str(ROOT / 'cmyk.v'), str(ROOT / 'cmyk.jpg'),
		'--Q', '95', '--strip')
	run_vips('webpsave', str(ROOT / 'marker.png'), str(ROOT / 'marker.webp'), '--strip')
	run_vips('tiffsave', str(ROOT / 'marker.png'), str(ROOT / 'marker.tiff'), '--strip')
	(ROOT / 'cmyk.v').unlink()
	profiles = (
		pathlib.Path('/usr/share/color/icc/ghostscript/srgb.icc'),
		pathlib.Path('/usr/share/color/icc/sRGB.icc'),
	)
	profile = next((path for path in profiles if path.is_file()), None)
	if profile is None:
		raise RuntimeError('no system sRGB ICC profile was found')
	run_vips('icc_export', str(ROOT / 'marker.png'), str(ROOT / 'profiled.png'),
		'--output-profile', str(profile))


def main():
	'''Generate all image fixtures and their hash manifest.'''

	write_png_fixtures()
	write_vips_fixtures()
	write_hashes()


if __name__ == '__main__':
	main()
