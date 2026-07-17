#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/vectors/analogue/pd-120/generate_reference.py
# SPDX-License-Identifier: GPL-3.0-or-later

import json
from fractions import Fraction


DENOMINATOR = 1_000_000_000
HEIGHT      = 496
WIDTH       = 640

BLUE_WEIGHTS = (-148_213_170, -290_973_564, 439_186_734)
LUMA_WEIGHTS = (256_772_628, 504_096_642, 97_899_984)
RED_WEIGHTS  = (439_186_734, -367_765_524, -71_421_210)


def average_pixel(first: tuple[int, int, int], second: tuple[int, int, int]) -> tuple[int, int, int]:
	'''Average two RGB pixels before conversion for a disagreement fixture'''

	return tuple((left + right) // 2 for left, right in zip(first, second))


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
	'''Construct the complete independent VIS and PD120 event-duration sequence'''

	vis = [Fraction(300, 1_000), Fraction(10, 1_000), Fraction(300, 1_000)]
	vis.extend([Fraction(30, 1_000)] * 10)
	pair = [Fraction(20, 1_000), Fraction(2_080, 1_000_000)]
	pair.extend([Fraction(121_600, 1_000_000) / WIDTH] * (WIDTH * 4))
	return vis + pair * (HEIGHT // 2)


def main():
	'''Print the compact independently calculated PD120 reference vector'''

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
	rows = (
		((255, 0, 0), (0, 255, 0), (255, 0, 0), (0, 0, 255)),
		((0, 0, 255), (255, 255, 255), (0, 255, 0), (255, 255, 255)),
	)
	converted_rows = tuple(tuple(convert_pixel(pixel) for pixel in row) for row in rows)
	events = make_event_durations()
	selected = (12, 13, 14, 15, 654, 655, 1_294, 1_295, 1_934, 1_935,
		2_574, 2_575, 2_576, 317_701, 317_702, 632_827, 632_828, 635_388)
	boundaries = {}
	elapsed = Fraction(0, 1)
	for index, duration in enumerate(events):
		elapsed += duration
		if index in selected:
			boundary = elapsed * 48_000
			boundaries[str(index)] = boundary.numerator // boundary.denominator
	result = {
		'descriptor': {
			'id': 'pd-120',
			'vis_code': 95,
			'width': WIDTH,
			'height': HEIGHT,
			'pair_count': HEIGHT // 2,
			'pixel_duration_us': 190,
			'pair_duration_us': 508_480,
			'image_duration_us': 126_103_040,
			'complete_duration_us': 127_013_040,
			'event_count': len(events),
		},
		'conversion': {name: convert_pixel(pixel) for name, pixel in colours.items()},
		'paired_chroma': {
			'pixels_by_row': rows,
			'converted_by_row': converted_rows,
			'red_difference': tuple(
				(converted_rows[0][x][1] + converted_rows[1][x][1]) // 2
				for x in range(len(rows[0]))
			),
			'blue_difference': tuple(
				(converted_rows[0][x][2] + converted_rows[1][x][2]) // 2
				for x in range(len(rows[0]))
			),
			'average_rgb_then_convert': tuple(
				convert_pixel(average_pixel(rows[0][x], rows[1][x]))
				for x in range(len(rows[0]))
			),
		},
		'pair_schedule': (
			('sync', 20_000, 1_200),
			('porch', 2_080, 1_500),
			('first_luma', 121_600, WIDTH),
			('red_difference', 121_600, WIDTH),
			('blue_difference', 121_600, WIDTH),
			('second_luma', 121_600, WIDTH),
		),
		'vis_frequencies_hz': (
			1_900, 1_200, 1_900, 1_200, 1_100, 1_100, 1_100,
			1_100, 1_100, 1_300, 1_100, 1_300, 1_200,
		),
		'boundaries_48000': boundaries,
		'frame_counts': {
			str(rate): (elapsed * rate).numerator // (elapsed * rate).denominator
			for rate in (8_000, 11_025, 44_100, 48_000, 96_000)
		},
		'diagnostic_samples': {
			f'{x},{y}': convert_pixel(diagnostic_pixel(x, y))
			for x, y in ((0, 0), (12, 0), (80, 0), (320, 124),
				(0, 248), (639, 248), (0, 495), (639, 495))
		},
	}
	print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == '__main__':
	main()
