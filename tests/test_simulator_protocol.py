#!/usr/bin/env python3
"""Unit tests for ESP32 simulator protocol helpers (no serial hardware)."""

import json
import unittest
import unittest.mock

import msgpack

from esp32_simulator import ESP32Simulator


class TestSimulatorProtocol(unittest.TestCase):
    def setUp(self):
        self.sim_json = ESP32Simulator(format='json')
        self.sim_msgpack = ESP32Simulator(format='msgpack')

    def test_format_json_default_path(self):
        self.assertEqual(self.sim_json.format, 'json')

    def test_send_data_json_encoding(self):
        written = []

        class FakeSerial:
            def write(self, data):
                written.append(data)
                return len(data)

            def flush(self):
                pass

        self.sim_json.serial_conn = FakeSerial()
        self.sim_json.send_data({'cpu': '12'})
        self.assertEqual(len(written), 1)
        line = written[0].decode('utf-8')
        self.assertTrue(line.endswith('\n'))
        payload = json.loads(line.strip())
        self.assertEqual(payload['cpu'], '12')

    def test_send_data_msgpack_encoding(self):
        written = []

        class FakeSerial:
            def write(self, data):
                written.append(data)
                return len(data)

            def flush(self):
                pass

        self.sim_msgpack.serial_conn = FakeSerial()
        self.sim_msgpack.send_data({'cpu': '42'})
        self.assertEqual(len(written), 2)
        packed = written[0]
        unpacked = msgpack.unpackb(packed, strict_map_key=False)
        self.assertEqual(unpacked['cpu'], '42')
        self.assertEqual(written[1], b'\n')

    def test_process_legacy_request_dispatch(self):
        with unittest.mock.patch.object(self.sim_json, 'handle_cpu_request') as mock_cpu:
            self.sim_json.process_command('{"request":"cpu"}')
            mock_cpu.assert_called_once()


if __name__ == '__main__':
    unittest.main()
