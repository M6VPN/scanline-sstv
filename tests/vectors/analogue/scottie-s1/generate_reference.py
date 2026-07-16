#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/vectors/analogue/scottie-s1/generate_reference.py
# SPDX-License-Identifier: GPL-3.0-or-later

import json
import sys

from fractions import Fraction


BLACK_HZ     = 1_500
CHANNELS     = (('green', 1), ('blue', 2), ('red', 0))
GAP_US       = 1_500
HEIGHT       = 256
RATES        = (8_000, 11_025, 44_100, 48_000, 96_000)
SCAN_US      = 138_240
SYNC_HZ      = 1_200
SYNC_US      = 9_000
VIS_CODE     = 60
VIS_HEADER   = (
	(1_900, 300_000),
	(1_200, 10_000),
	(1_900, 300_000),
	(1_200, 30_000),
	(1_300, 30_000),
	(1_300, 30_000),
	(1_100, 30_000),
	(1_100, 30_000),
	(1_100, 30_000),
	(1_100, 30_000),
	(1_300, 30_000),
	(1_300, 30_000),
	(1_200, 30_000),
)
WHITE_HZ     = 2_300
WIDTH        = 320


def base_pattern_pixel(x: int, y: int) -> tuple[int, int, int]:
	'''
	Return one bars, gradient, or checker pixel.

	:param x: Horizontal pixel coordinate
	:param y: Vertical pixel coordinate
	'''

	colour_bars = (
		(255, 255, 255),
		(255, 255, 0),
		(0, 255, 255),
		(0, 255, 0),
		(255, 0, 255),
		(255, 0, 0),
		(0, 0, 255),
		(0, 0, 0),
	)
	quarter = HEIGHT // 4
	if y < quarter:
		return colour_bars[x * len(colour_bars) // WIDTH]
	if y < quarter * 2:
		level = scale_coordinate(x, WIDTH - 1)
		return level, level, level
	if y < quarter * 3:
		horizontal = scale_coordinate(x, WIDTH - 1)
		vertical   = scale_coordinate(y - quarter * 2, quarter - 1)
		return horizontal, vertical, 255 - horizontal
	level = 224 if ((x // 16) + (y // 16)) % 2 == 0 else 32
	return level, level, level


def diagnostic_pixel(x: int, y: int) -> tuple[int, int, int]:
	'''
	Apply corner and line-marker overlays to one diagnostic pixel.

	:param x: Horizontal pixel coordinate
	:param y: Vertical pixel coordinate
	'''

	if x < 12 and y < 12:
		return 255, 0, 0
	if x >= WIDTH - 12 and y < 12:
		return 0, 255, 0
	if x < 12 and y >= HEIGHT - 12:
		return 0, 0, 255
	if x >= WIDTH - 12 and y >= HEIGHT - 12:
		return 255, 255, 255
	if y % 32 == 0 and (x < 8 or x >= WIDTH - 8):
		level = 255 if (y // 32) % 2 == 0 else 0
		return level, level, level
	return base_pattern_pixel(x, y)


def generate_events() -> list[tuple[Fraction, int]]:
	'''Generate the evidence-selected frequency and duration event sequence.'''

	events = [(Fraction(frequency), duration) for frequency, duration in VIS_HEADER]
	pixel_duration = Fraction(SCAN_US, WIDTH)
	for y in range(HEIGHT):
		events.append((Fraction(BLACK_HZ), GAP_US))
		for channel, channel_index in CHANNELS:
			if channel == 'blue':
				events.append((Fraction(BLACK_HZ), GAP_US))
			elif channel == 'red':
				events.append((Fraction(SYNC_HZ), SYNC_US))
				events.append((Fraction(BLACK_HZ), GAP_US))
			for x in range(WIDTH):
				component = diagnostic_pixel(x, y)[channel_index]
				frequency = Fraction(BLACK_HZ) \
					+ Fraction(WHITE_HZ - BLACK_HZ, 255) * component
				events.append((frequency, pixel_duration))
	return events


def main():
	'''Generate compact event and cumulative-boundary expectations as JSON.'''

	events          = generate_events()
	first_line      = 13
	second_line     = first_line + 964
	final_line      = first_line + 255 * 964
	selected_events = (
		first_line,
		first_line + 1,
		first_line + 320,
		first_line + 321,
		first_line + 322,
		first_line + 641,
		first_line + 642,
		first_line + 643,
		first_line + 644,
		first_line + 963,
		second_line,
		second_line + 1,
		second_line + 642,
		final_line,
		final_line + 642,
		final_line + 963,
	)
	selected_set = set(selected_events)
	boundaries   = []
	elapsed_us   = Fraction(0)
	for index, (frequency, duration_us) in enumerate(events):
		elapsed_us += duration_us
		if index in selected_set:
			boundaries.append({
				'boundary_48000': elapsed_us * 48_000 // 1_000_000,
				'duration_us': str(duration_us),
				'frequency_hz': str(frequency),
				'index': index,
			})
	total_duration = elapsed_us / 1_000_000
	reference = {
		'channel_order': [channel for channel, _ in CHANNELS],
		'dimensions': [WIDTH, HEIGHT],
		'event_count': len(events),
		'frame_counts': {
			str(rate): total_duration * rate // 1
			for rate in RATES
		},
		'full_duration_seconds': str(total_duration),
		'line_duration_seconds': str(Fraction(428_220, 1_000_000)),
		'pixel_frequency_hz': {
			'0': str(Fraction(BLACK_HZ)),
			'128': str(Fraction(BLACK_HZ) + Fraction(800 * 128, 255)),
			'255': str(Fraction(WHITE_HZ)),
		},
		'selected_event_boundaries_48000': boundaries,
		'source': {
			'artifact_sha256': 'e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d',
			'project': 'Image Communication on Short Waves',
			'version': '2019-11-17',
		},
		'vis_code': VIS_CODE,
		'vis_frequencies_hz': [frequency for frequency, _ in VIS_HEADER],
	}
	json.dump(reference, fp=sys.stdout, indent=2, sort_keys=True)
	sys.stdout.write('\n')


def scale_coordinate(value: int, maximum: int) -> int:
	'''
	Scale a coordinate to RGB8 using the project rounding rule.

	:param value: Coordinate to scale
	:param maximum: Maximum coordinate
	'''

	if maximum == 0:
		return 0
	return (value * 255 + maximum // 2) // maximum



if __name__ == '__main__':
	main()
