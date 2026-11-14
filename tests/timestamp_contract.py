#!/usr/bin/env python3
"""Regression checks for the full-duration timestamp release gate."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('timestamp_gate', ROOT / 'data-sim/timestamp_contract.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def records(first=0, end=240000, formatter=lambda t: format(t, '.17g')):
    yield 'time,disp_x\n'
    for step in range(first, end):
        yield formatter(step / 200) + ',0\n'


class TimestampTests(unittest.TestCase):
    def test_full_duration_and_fenton_endpoint(self):
        self.assertEqual(gate.validate_timestamps(records(), 'wave_data_jonswap_test.csv', 1200), 240000)
        self.assertEqual(gate.validate_timestamps(records(2, 240001), 'wave_data_fenton_test.csv', 1200), 239999)

    def test_six_significant_digits_fail_at_first_loss(self):
        # Non-Fenton clocks are representable at six digits until 1000.005 s.
        with self.assertRaisesRegex(ValueError, r'row 200003: time 1000.0, expected 1000.005'):
            gate.validate_timestamps(records(formatter=lambda t: format(t, '.6g')),
                                     'wave_data_gerstner_test.csv', 1200)

    def test_drifted_clock_and_missing_endpoint_fail(self):
        drifted = iter(['time,disp_x\n', '0,0\n', '0.004999999888241291,0\n'])
        with self.assertRaises(ValueError):
            gate.validate_timestamps(drifted, 'wave_data_gerstner_test.csv')
        with self.assertRaisesRegex(ValueError, 'samples, expected'):
            gate.validate_timestamps(records(2, 240000), 'wave_data_fenton_test.csv', 1200)

    def test_nonfinite_empty_and_non_grid_duration(self):
        for value in ('nan', 'inf', '-inf'):
            with self.assertRaises(ValueError):
                gate.validate_timestamps(iter(['time,x\n', value + ',0\n']), 'wave_data_x.csv')
        with self.assertRaises(ValueError):
            gate.validate_timestamps(iter(['time,x\n']), 'wave_data_x.csv')
        self.assertEqual(gate.validate_timestamps(records(2, 3), 'wave_data_fenton_test.csv', 0.012), 1)


if __name__ == '__main__':
    unittest.main()
