#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/vectors/analogue/martin-m1/generate_reference.py
# SPDX-License-Identifier: GPL-3.0-or-later

import json
import sys

from decimal import Decimal, ROUND_FLOOR

try:
	from pysstv.color import MartinM1
except ImportError:
	raise ImportError('missing pinned PySSTV source on PYTHONPATH')


COLOUR_BARS = (
	(255, 255, 255),
	(255, 255, 0),
	(0, 255, 255),
	(0, 255, 0),
	(255, 0, 255),
	(255, 0, 0),
	(0, 0, 255),
	(0, 0, 0),
)
HEIGHT       = 256
RATES        = (8_000, 11_025, 44_100, 48_000, 96_000)
WIDTH        = 320


class ReferenceImage:
	'''Provide the minimal immutable image interface used by PySSTV.'''

	def convert(self, mode: str):
		'''Validate the requested color mode and return this image.'''

		if mode != 'RGB':
			raise ValueError(f'unexpected color mode: {mode}')
		return self

	def load(self):
		'''Return this image as its read-only pixel accessor.'''

		return self

	def __getitem__(self, coordinate: tuple[int, int]) -> tuple[int, int, int]:
		'''Return one deterministic diagnostic pixel.'''

		x, y = coordinate
		return diagnostic_pixel(x, y)


def base_pattern_pixel(x: int, y: int) -> tuple[int, int, int]:
	'''
	Return one bars, gradient, or checker pixel.

	:param x: Horizontal pixel coordinate
	:param y: Vertical pixel coordinate
	'''

	quarter = HEIGHT // 4
	if y < quarter:
		return COLOUR_BARS[x * len(COLOUR_BARS) // WIDTH]
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


def main():
	'''Generate compact event and cumulative-boundary expectations as JSON.'''

	encoder         = MartinM1(ReferenceImage(), 48_000, 16)
	events          = list(encoder.gen_freq_bits())
	durations_ms    = [
		Decimal(str(duration)).quantize(Decimal('0.000001'))
		for _, duration in events
	]
	durations       = [duration / 1_000 for duration in durations_ms]
	total_duration  = sum(durations, Decimal(0))
	selected_events = (13, 14, 15, 334, 335, 336, 655, 656, 657, 976, 977, 978)
	boundaries      = []
	elapsed         = Decimal(0)
	selected_set    = set(selected_events)
	for index, ((frequency, _), duration, seconds) in enumerate(
		zip(events, durations_ms, durations)
	):
		elapsed += seconds
		if index in selected_set:
			boundaries.append([
				index,
				int(frequency),
				str(duration),
				int((elapsed * 48_000).to_integral_value(rounding=ROUND_FLOOR)),
			])
	reference = {
		'source': {
			'project': 'PySSTV',
			'tag': 'v0.5.8',
			'commit': '884cddb36844c745fcfc6d69eab166b3b23a442b',
		},
		'dimensions': [WIDTH, HEIGHT],
		'event_count': len(events),
		'full_duration_seconds': str(total_duration),
		'frame_counts': {
			str(rate): int((total_duration * rate).to_integral_value(rounding=ROUND_FLOOR))
			for rate in RATES
		},
		'vis_frequencies_hz': [
			int(frequency)
			for frequency, _ in events[:13]
		],
		'selected_event_boundaries_48000': boundaries,
	}
	json.dump(reference, sys.stdout, indent=2, sort_keys=True)
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
