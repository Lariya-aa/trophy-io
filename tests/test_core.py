#!/usr/bin/env python3
"""Build and test only the portable C core; no device or network access."""
import ctypes
import hashlib
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
WRITE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)


class CoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="trophy-io-host-")
        library = Path(cls.temp.name) / "core.so"
        subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                        "-I", str(ROOT / "include"), str(ROOT / "src/core.c"),
                        str(ROOT / "src/sha256.c"), "-o", str(library)], check=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.trophy_npwr_valid.argtypes = [ctypes.c_char_p]
        cls.lib.trophy_data_matches_npwr.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p]
        cls.lib.trophy_xml_text.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
        cls.lib.trophy_sha256.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p]
        cls.lib.trophy_write_all.argtypes = [WRITE, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_npwr_rejects_traversal_and_malformed_ids(self):
        for value in [None, b"", b"../NPWR16080_00", b"NPWR16080_00/", b"NPWR16080_0",
                      b"NPWR16A80_00", b"NPWR16080-00", b"NPWR16080_000"]:
            self.assertEqual(self.lib.trophy_npwr_valid(value), 0, value)
        self.assertEqual(self.lib.trophy_npwr_valid(b"NPWR16080_00"), 1)

    def test_decrypted_identity_at_boundary(self):
        npwr = b"NPWR16080_00"
        for data in [npwr, b"\0" * 372 + npwr, npwr + b"\0" * 5 + npwr]:
            self.assertEqual(self.lib.trophy_data_matches_npwr(data, len(data), npwr), 1)

    def test_ciphertext_truncated_and_cross_title_rejected(self):
        npwr = b"NPWR16080_00"
        for data in [b"\xff" * 32768, npwr[:-1], b"NPWR16079_00", npwr + b"NPWR16079_00"]:
            self.assertEqual(self.lib.trophy_data_matches_npwr(data, len(data), npwr), 0)

    def test_xml_exact_capacity_and_truncation(self):
        out = ctypes.create_string_buffer(13)
        xml = b"<npcommid>NPWR16080_00</npcommid>"
        self.assertEqual(self.lib.trophy_xml_text(xml, b"npcommid", out, 13), 0)
        self.assertEqual(out.value, b"NPWR16080_00")
        for content, capacity in [(xml, 12), (xml[:-5], 13), (b"<wrong>x</wrong>", 13), (xml, 0)]:
            self.assertLess(self.lib.trophy_xml_text(content, b"npcommid", out, capacity), 0)

    def test_sha256_known_vectors_and_padding_boundaries(self):
        rng = random.Random(42)
        samples = [b"", b"abc", b"a" * 1_000_000]
        samples += [rng.randbytes(n) for n in [1, 55, 56, 63, 64, 65, 119, 120, 127, 128, 4097]]
        for data in samples:
            out = ctypes.create_string_buffer(65)
            self.lib.trophy_sha256(data, len(data), out)
            self.assertEqual(out.value.decode(), hashlib.sha256(data).hexdigest(), len(data))

    def test_short_writes_preserve_every_byte(self):
        captured = bytearray()
        @WRITE
        def short_write(context, data, size):
            n = min(size, 3)
            captured.extend(ctypes.string_at(data, n))
            return n
        payload = b"0123456789abcdef"
        self.assertEqual(self.lib.trophy_write_all(short_write, None, payload, len(payload)), 0)
        self.assertEqual(captured, payload)

    def test_write_failure_and_zero_progress_stop(self):
        for failure in [0, -1234]:
            calls = []
            @WRITE
            def failing_write(context, data, size):
                calls.append(size)
                return failure
            result = self.lib.trophy_write_all(failing_write, None, b"hello", 5)
            self.assertLess(result, 0)
            if failure < 0:
                self.assertEqual(result, failure)
            self.assertEqual(calls, [5])

    def test_oversized_write_result_rejected(self):
        @WRITE
        def invalid_write(context, data, size):
            return size + 1
        self.assertLess(self.lib.trophy_write_all(invalid_write, None, b"hello", 5), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
