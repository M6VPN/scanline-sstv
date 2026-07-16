#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/vectors/analogue/robot-36/generate_reference.py
# SPDX-License-Identifier: GPL-3.0-or-later

import json
from fractions import Fraction


DENOMINATOR = 1_000_000_000
HEIGHT      = 240
WIDTH       = 320

BLUE_WEIGHTS = (-148_213_170, -290_973_564, 439_186_734)
LUMA_WEIGHTS = (256_772_628, 504_096_642, 97_899_984)
RED_WEIGHTS  = (439_186_734, -367_765_524, -71_421_210)


def convert_component(pixel: tuple[int, int, int], weights: tuple[int, int, int], offset: int) -> int:
	'''Convert one nonlinear RGB pixel using the frozen Dayton rational coefficients'''

	numerator = offset * DENOMINATOR + sum(value * weight for value, weight in zip(pixel, weights))
	return min(255, max(0, numerator // DENOMINATOR))


def convert_pixel(pixel: tuple[int, int, int]) -> tuple[int, int, int]:
	'''Return luma, red-difference, and blue-difference codes'''

	return (
		convert_component(pixel, LUMA_WEIGHTS, 16),
		convert_component(pixel, RED_WEIGHTS, 128),
		convert_component(pixel, BLUE_WEIGHTS, 128),
	)


def diagnostic_pixel(x: int, y: int) -> tuple[int, int, int]:
	'''Recreate the deterministic project diagnostic pattern without importing production code'''

	if x < 12 and y < 12:
		return (255, 0, 0)
	if x >= WIDTH - 12 and y < 12:
		return (0, 255, 0)
	if x < 12 and y >= HEIGHT - 12:
		return (0, 0, 255)
	if x >= WIDTH - 12 and y >= HEIGHT - 12:
		return (255, 255, 255)
	if y % 32 == 0 and (x < 8 or x >= WIDTH - 8):
		return (255, 255, 255) if (y // 32) % 2 == 0 else (0, 0, 0)
	quarter = HEIGHT // 4
	if y < quarter:
		bars = (
			(255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
			(255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0),
		)
		return bars[x * len(bars) // WIDTH]
	if y < quarter * 2:
		level = (x * 255 + (WIDTH - 1) // 2) // (WIDTH - 1)
		return (level, level, level)
	if y < quarter * 3:
		horizontal = (x * 255 + (WIDTH - 1) // 2) // (WIDTH - 1)
		vertical = ((y - quarter * 2) * 255 + (quarter - 1) // 2) // (quarter - 1)
		return (horizontal, vertical, 255 - horizontal)
	return (224, 224, 224) if ((x // 16) + (y // 16)) % 2 == 0 else (32, 32, 32)


def make_event_durations() -> list[Fraction]:
	'''Construct the complete independent VIS and Robot 36 event-duration sequence'''

	vis = [Fraction(300, 1_000), Fraction(10, 1_000), Fraction(300, 1_000)]
	vis.extend([Fraction(30, 1_000)] * 10)
	line = [Fraction(9, 1_000), Fraction(3, 1_000)]
	line.extend([Fraction(88, 1_000) / WIDTH] * WIDTH)
	line.extend([Fraction(4_500, 1_000_000), Fraction(1_500, 1_000_000)])
	line.extend([Fraction(44, 1_000) / (WIDTH // 2)] * (WIDTH // 2))
	return vis + line * HEIGHT


def main():
	'''Print the compact independently calculated Robot 36 reference vector'''

	colours = {
		'black': (0, 0, 0),
		'white': (255, 255, 255),
		'grey_127': (127, 127, 127),
		'grey_128': (128, 128, 128),
		'red': (255, 0, 0),
		'green': (0, 255, 0),
		'blue': (0, 0, 255),
		'near_black': (1, 2, 3),
		'near_white': (254, 253, 252),
	}
	block = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255))
	converted_block = tuple(convert_pixel(pixel) for pixel in block)
	events = make_event_durations()
	selected = (12, 13, 14, 333, 334, 335, 496, 497, 498, 817, 818, 819,
		115_205, 115_689, 116_172)
	boundaries = {}
	elapsed = Fraction(0, 1)
	for index, duration in enumerate(events):
		elapsed += duration
		if index in selected:
			boundaries[str(index)] = (elapsed * 48_000).numerator // (elapsed * 48_000).denominator
	result = {
		'descriptor': {
			'id': 'robot-36',
			'vis_code': 8,
			'width': WIDTH,
			'height': HEIGHT,
			'line_duration_us': 150_000,
			'image_duration_us': 36_000_000,
			'complete_duration_us': 36_910_000,
			'event_count': len(events),
		},
		'conversion': {name: convert_pixel(pixel) for name, pixel in colours.items()},
		'subsampling': {
			'pixels_row_major': block,
			'converted_row_major': converted_block,
			'red_difference_average': sum(value[1] for value in converted_block) // 4,
			'blue_difference_average': sum(value[2] for value in converted_block) // 4,
		},
		'vis_frequencies_hz': (1900, 1200, 1900, 1200, 1300, 1300, 1300,
			1100, 1300, 1300, 1300, 1100, 1200),
		'boundaries_48000': boundaries,
		'frame_counts': {
			str(rate): (elapsed * rate).numerator // (elapsed * rate).denominator
			for rate in (8_000, 11_025, 44_100, 48_000, 96_000)
		},
		'diagnostic_samples': {
			f'{x},{y}': convert_pixel(diagnostic_pixel(x, y))
			for x, y in ((0, 0), (12, 0), (40, 0), (160, 60),
				(0, 120), (319, 120), (0, 239), (319, 239))
		},
	}
	print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == '__main__':
	main()
