#!/usr/bin/env python3
"""Unit tests for ESP32 simulator protocol helpers (no serial hardware)."""

import json
import unittest
import unittest.mock

import msgpack

from esp32_simulator import (
    ESP32Simulator,
    WIFI_DEMO_SSID,
    human_bytes,
    load_mcud_version,
)


class FakeSerial:
    def __init__(self):
        self.written = []

    def write(self, data):
        self.written.append(data)
        return len(data)

    def flush(self):
        pass

    def lines(self):
        out = []
        buf = b""
        for chunk in self.written:
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if line:
                    out.append(line)
        return out


class TestSimulatorProtocol(unittest.TestCase):
    def setUp(self):
        self.sim_json = ESP32Simulator(format="json")
        self.sim_msgpack = ESP32Simulator(format="msgpack")

    def test_format_json_default_path(self):
        self.assertEqual(self.sim_json.format, "json")

    def test_send_data_json_encoding(self):
        ser = FakeSerial()
        self.sim_json.serial_conn = ser
        self.sim_json.send_data({"cpu": "12"})
        self.assertEqual(len(ser.written), 1)
        line = ser.written[0].decode("utf-8")
        self.assertTrue(line.endswith("\n"))
        payload = json.loads(line.strip())
        self.assertEqual(payload["cpu"], "12")

    def test_send_data_msgpack_encoding(self):
        ser = FakeSerial()
        self.sim_msgpack.serial_conn = ser
        self.sim_msgpack.send_data({"cpu": "42"})
        self.assertEqual(len(ser.written), 2)
        unpacked = msgpack.unpackb(ser.written[0], strict_map_key=False)
        self.assertEqual(unpacked["cpu"], "42")
        self.assertEqual(ser.written[1], b"\n")

    def test_process_legacy_request_dispatch(self):
        with unittest.mock.patch.object(self.sim_json, "handle_cpu_request") as mock_cpu:
            self.sim_json.process_command('{"request":"cpu"}')
            mock_cpu.assert_called_once()

    def test_load_mcud_version(self):
        ver = load_mcud_version()
        self.assertEqual(ver["rdcp"], 1)
        self.assertEqual(ver["component"], "mcudd")
        self.assertGreaterEqual(ver["release"], 1)
        self.assertEqual(self.sim_json.hello_payload()["stack"], ver["stack"])

    def test_human_bytes(self):
        self.assertEqual(human_bytes(512), "1K")
        self.assertTrue(human_bytes(50 * 1024 * 1024).endswith("M"))

    def test_scope_system_network_wifi_storage_clients(self):
        system = self.sim_json.build_scope_metrics("system")
        for key in ("hostname", "cpu", "cpu_temp", "ram_pct", "ram_used",
                    "load_short", "uptime_short"):
            self.assertIn(key, system)

        net = self.sim_json.build_scope_metrics("network")
        for key in ("wan_ip", "wan_dev", "rx_rate", "tx_rate", "ping_ms", "ping_ok",
                    "eth0_role", "eth0_up", "eth0_speed", "eth1_up", "eth2_up"):
            self.assertIn(key, net)
        self.assertEqual(net["eth0_role"], "WAN")

        wifi = self.sim_json.build_scope_metrics("wifi")
        self.assertEqual(wifi["wifi_ssid"], WIFI_DEMO_SSID)
        self.assertEqual(wifi["wifi_enc"], "WPA2")
        self.assertEqual(wifi["wifi_ap_state"], "up")
        self.assertTrue(wifi["wifi_qr"].startswith("WIFI:T:WPA;S:"))

        storage = self.sim_json.build_scope_metrics("storage")
        for key in ("root_usage", "root_pct", "root_dev", "data_kind", "data_usage",
                    "swap_usage", "swap_pct"):
            self.assertIn(key, storage)
        self.assertIn("/", storage["root_usage"])

        clients = self.sim_json.build_scope_metrics("clients")
        for key in ("wifi_24", "wifi_5", "dhcp_summary", "dhcp_pool", "dhcp_pct",
                    "clients_total"):
            self.assertIn(key, clients)

        security = self.sim_json.build_scope_metrics("security")
        for key in ("firewall_state", "blocked_24h", "vpn_tunnels",
                    "blocky_blocked", "banip_blocked"):
            self.assertIn(key, security)

    def test_rdcp_metrics_request_sends_res(self):
        ser = FakeSerial()
        self.sim_json.serial_conn = ser
        self.sim_json.process_command(
            '{"v":1,"t":"req","id":7,"op":"metrics","scope":"wifi"}'
        )
        frames = [json.loads(x.decode("utf-8")) for x in ser.lines()]
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0]["t"], "res")
        self.assertEqual(frames[0]["id"], 7)
        self.assertEqual(frames[0]["data"]["wifi_enc"], "WPA2")

    def test_gesture_sends_cmd_screen(self):
        ser = FakeSerial()
        self.sim_json.serial_conn = ser
        self.sim_json.active_screen = "router_system"
        self.sim_json.process_command(
            '{"v":1,"t":"evt","op":"input","data":{"type":"gesture","dir":"left"}}'
        )
        frames = [json.loads(x.decode("utf-8")) for x in ser.lines()]
        self.assertEqual(frames[0]["t"], "cmd")
        self.assertEqual(frames[0]["op"], "screen")
        self.assertEqual(frames[0]["data"]["screen"], "router_network")

    def test_hello_and_ping_frames(self):
        ser = FakeSerial()
        self.sim_json.serial_conn = ser
        self.sim_json.send_hello()
        self.sim_json.send_req_ping(3)
        frames = [json.loads(x.decode("utf-8")) for x in ser.lines()]
        self.assertEqual(frames[0]["t"], "push")
        self.assertEqual(frames[0]["op"], "hello")
        self.assertIn("release", frames[0]["data"])
        self.assertEqual(frames[1]["t"], "req")
        self.assertEqual(frames[1]["op"], "ping")
        self.assertEqual(frames[1]["id"], 3)

    def test_pong_from_mcu(self):
        self.sim_json.process_command(
            '{"v":1,"t":"res","id":3,"data":{"pong":1,"uptime_ms":1234}}'
        )

    def test_wifi_qr_escapes(self):
        qr = self.sim_json.wifi_qr("Cafe;WiFi", "p:ass", "psk2")
        self.assertIn(r"S:Cafe\;WiFi", qr)
        self.assertIn(r"P:p\:ass", qr)

    def test_send_host_frame_rdcp_always_json(self):
        ser = FakeSerial()
        self.sim_msgpack.serial_conn = ser
        self.sim_msgpack.send_host_frame(
            {"v": 1, "t": "cmd", "op": "nav", "data": {"dir": "next"}}
        )
        self.assertEqual(len(ser.written), 1)
        self.assertTrue(ser.written[0].startswith(b"{"))


if __name__ == "__main__":
    unittest.main()
