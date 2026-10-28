"""Offline checks for demo command bytes and telemetry decoder. No paho needed."""
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[3] / "tools" / "buddy1_mqtt.py"
spec = importlib.util.spec_from_file_location("buddy1_mqtt", path)
demo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(demo)


class DemoTest(unittest.TestCase):
    def test_commands(self):
        self.assertEqual(demo.command_payload("stop"), b"\x00")
        self.assertEqual(demo.command_payload("start"), b"\x01")
        with self.assertRaises(ValueError):
            demo.command_payload("snapshot")

    def test_decode(self):
        self.assertEqual(demo.decode("x/terrain", bytes.fromhex("d02f")),
                         {"height_mm": 2000, "phase": 5})
        self.assertEqual(demo.decode("x/vehicle", b"\xff"), {"state": 7, "fault_mask": 31})
        self.assertEqual(demo.decode("x/line", b"\x27"), {"ir_bits": 7, "barcode": 4})
        self.assertEqual(demo.decode("x/ack", b"\x03"), {"opcode": 1, "rejected": True})
        self.assertIn("error", demo.decode("x/motion", b"\x00"))


if __name__ == "__main__":
    unittest.main()
