#!/usr/bin/env python3
# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/tests/vectors/analogue/fsk-id/generate_reference.py
# SPDX-License-Identifier: GPL-3.0-or-later

import json

from fractions import Fraction


IDENTIFIER   = 'M6VPN-1'
LEADER_HZ    = 1_500
MARK_HZ      = 1_900
SPACE_HZ     = 2_100
SAMPLE_RATES = [8_000, 11_025, 16_000, 22_050, 32_000, 44_100,
	48_000, 88_200, 96_000, 176_400, 192_000]


def append_code(events: list[dict], code: int):
	'''
	Append six least-significant-bit-first FSK events.

	:param events: Destination event list
	:param code: Six-bit wire value
	'''

	for bit in range(6):
		events.append({'duration_us': 22_000,
			'frequency_hz': MARK_HZ if code & (1 << bit) else SPACE_HZ})


def build_reference() -> dict:
	'''Build the independently calculated FSK ID reference data.'''

	events = [
		{'duration_us': 300_000, 'frequency_hz': LEADER_HZ},
		{'duration_us': 100_000, 'frequency_hz': SPACE_HZ},
		{'duration_us': 22_000, 'frequency_hz': MARK_HZ},
	]
	append_code(events, 0x2a)
	checksum = 0
	character_codes = []
	for character in IDENTIFIER:
		code = ord(character) - 0x20
		character_codes.append(code)
		append_code(events, code)
		checksum ^= code
	append_code(events, 0x01)
	append_code(events, checksum & 0x3f)
	events.append({'duration_us': 100_000, 'frequency_hz': MARK_HZ})
	elapsed = Fraction(0, 1)
	boundaries = []
	for event in events:
		elapsed += Fraction(event['duration_us'], 1_000_000)
		boundaries.append({'numerator': elapsed.numerator,
			'denominator': elapsed.denominator})
	return {
		'identifier': IDENTIFIER,
		'character_codes': character_codes,
		'checksum': checksum & 0x3f,
		'event_count': len(events),
		'duration': {'numerator': elapsed.numerator,
			'denominator': elapsed.denominator},
		'frame_counts': {str(rate): (elapsed.numerator * rate)
			// elapsed.denominator for rate in SAMPLE_RATES},
		'events': events,
		'boundaries': boundaries,
	}


if __name__ == '__main__':
	print(json.dumps(build_reference(), indent=2, sort_keys=True))
