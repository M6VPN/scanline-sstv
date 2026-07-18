#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/fixtures/hil/generate_fixture.py
# SPDX-License-Identifier: GPL-3.0-or-later

import hashlib
import struct
import zlib

from pathlib import Path


HEIGHT = 240
OUTPUT = Path(__file__).with_name('robot-36-reference.png')
WIDTH  = 320


def base_pixel(x: int, y: int) -> tuple[int, int, int]:
	'''Generate the accepted project diagnostic pattern base pixel.'''

	bars    = ((255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
		(255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0))
	quarter = HEIGHT // 4
	if y < quarter:
		return bars[x * len(bars) // WIDTH]
	if y < quarter * 2:
		level = (x * 255 + (WIDTH - 1) // 2) // (WIDTH - 1)
		return level, level, level
	if y < quarter * 3:
		horizontal = (x * 255 + (WIDTH - 1) // 2) // (WIDTH - 1)
		vertical   = ((y - quarter * 2) * 255 + (quarter - 1) // 2) // (quarter - 1)
		return horizontal, vertical, 255 - horizontal
	level = 224 if ((x // 16) + (y // 16)) % 2 == 0 else 32
	return level, level, level


def diagnostic_pixel(x: int, y: int) -> tuple[int, int, int]:
	'''Add orientation corners and line markers to the base pattern.'''

	if x < 12 and y < 12:
		return 255, 0, 0
	if x >= WIDTH - 12 and y < 12:
		return 0, 255, 0
	if x < 12 and y >= HEIGHT - 12:
		return 0, 0, 255
	if x >= WIDTH - 12 and y >= HEIGHT - 12:
		return 255, 255, 255
	if y % 32 == 0 and (x < 8 or x >= WIDTH - 8):
		return (255, 255, 255) if (y // 32) % 2 == 0 else (0, 0, 0)
	return base_pixel(x, y)


def png_chunk(kind: bytes, data: bytes) -> bytes:
	'''Build one deterministic PNG chunk.'''

	return struct.pack('>I', len(data)) + kind + data + struct.pack(
		'>I', zlib.crc32(kind + data) & 0xffffffff)


def main():
	'''Generate the lossless callsign-neutral Robot 36 HIL image.'''

	rgb  = bytes(component for y in range(HEIGHT) for x in range(WIDTH)
		for component in diagnostic_pixel(x, y))
	raws = b''.join(b'\x00' + rgb[y * WIDTH * 3:(y + 1) * WIDTH * 3]
		for y in range(HEIGHT))
	png  = b'\x89PNG\r\n\x1a\n'
	png += png_chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
	png += png_chunk(b'IDAT', zlib.compress(raws, level=9))
	png += png_chunk(b'IEND', b'')
	OUTPUT.write_bytes(png)
	print(f'raw-rgb-sha256 {hashlib.sha256(rgb).hexdigest()}')
	print(f'png-sha256 {hashlib.sha256(png).hexdigest()}')



if __name__ == '__main__':
	main()
