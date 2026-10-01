#!/usr/bin/env python3
"""Check the real VPK and reject mutated archives; never execute its binaries."""
import importlib.util
import argparse
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify_vpk", ROOT / "tools/verify_vpk.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)
VPK = None
APP_VERSION = "00.10"


class PackageTests(unittest.TestCase):
    def mutated(self, change):
        with tempfile.TemporaryDirectory(prefix="trophy-io-package-") as directory:
            candidate = Path(directory) / "candidate.vpk"
            with zipfile.ZipFile(VPK) as source, zipfile.ZipFile(candidate, "w") as destination:
                for name in source.namelist():
                    destination.writestr(name, change(name, source.read(name)))
            with self.assertRaises(ValueError):
                verifier.verify(candidate, APP_VERSION)

    def test_release_structure_and_minimums(self):
        report = verifier.verify(VPK, APP_VERSION)
        self.assertEqual(report["sfo"]["PSP2_SYSTEM_VER"], 0)
        self.assertFalse(report["device_execution_verified"])
        self.assertEqual(len(report["self"]), 3)

    def test_changed_title_id_rejected(self):
        self.mutated(lambda name, data: data.replace(b"TROPIO001", b"TROPIO002")
                     if name == "sce_sys/param.sfo" else data)

    def test_corrupt_self_rejected(self):
        self.mutated(lambda name, data: b"BAD\0" + data[4:] if name == "eboot.bin" else data)

    def test_high_firmware_minimum_rejected(self):
        def change(name, data):
            if name != "sce_module/trophy_kernel.skprx":
                return data
            data = bytearray(data)
            control = struct.unpack_from("<Q", data, 0x68)[0]
            self.assertEqual(struct.unpack_from("<I", data, control)[0], 4)
            struct.pack_into("<Q", data, control + 72, 0x03740000)
            return data
        self.mutated(change)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vpk", type=Path)
    parser.add_argument("--app-version", default="00.10")
    args = parser.parse_args()
    VPK = args.vpk
    APP_VERSION = args.app_version
    unittest.main(argv=[sys.argv[0]], verbosity=2)
