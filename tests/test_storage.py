#!/usr/bin/env python3
"""Exercise output bootstrap against a fake Vita filesystem contract."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
NOENT = ctypes.c_int(0x80010002).value
EXIST = ctypes.c_int(0x80010011).value
NOTDIR = ctypes.c_int(0x80010014).value
INVAL = ctypes.c_int(0x80010016).value
DENIED = ctypes.c_int(0x8001000D).value
STAT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
MKDIR = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint)


class Ops(ctypes.Structure):
    _fields_ = [("stat", STAT), ("mkdir", MKDIR)]


class FakeVita:
    def __init__(self, nodes=None):
        self.nodes = {"ux0:": 1} if nodes is None else dict(nodes)
        self.failures = {}
        self.created = []
        self.race = None
        self.stat_callback = STAT(self.stat)
        self.mkdir_callback = MKDIR(self.mkdir)
        self.ops = Ops(self.stat_callback, self.mkdir_callback)

    def stat(self, context, path):
        return self.nodes.get(path.decode().rstrip("/"), NOENT)

    def mkdir(self, context, path, mode):
        path = path.decode().rstrip("/")
        self.created.append((path, mode))
        # SDK requires both system permission bits for unsafe applications.
        if mode & 0o006 != 0o006:
            return INVAL
        if path in self.failures:
            return self.failures[path]
        if self.race and path == self.race[0]:
            self.nodes[path] = self.race[1]
            return EXIST
        parent = path.rsplit("/", 1)[0] if "/" in path else "ux0:"
        if self.nodes.get(parent) != 1:
            return NOENT
        self.nodes[path] = 1
        return 0


class StorageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="trophy-io-storage-")
        library = Path(cls.temp.name) / "storage.so"
        subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                        "-I", str(ROOT / "include"), str(ROOT / "src/storage.c"),
                        "-o", str(library)], check=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.trophy_prepare_output.argtypes = [ctypes.POINTER(Ops), ctypes.c_void_p,
                                                  ctypes.POINTER(ctypes.c_char_p)]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def prepare(self, fs):
        failed = ctypes.c_char_p(b"stale")
        result = self.lib.trophy_prepare_output(ctypes.byref(fs.ops), None, ctypes.byref(failed))
        return result, failed.value

    def test_bootstrap_missing_parents_with_system_rw(self):
        fs = FakeVita()
        self.assertEqual(fs.mkdir(None, b"ux0:data", 0o700), INVAL)
        fs.created.clear()
        self.assertEqual(self.prepare(fs), (0, None))
        self.assertEqual([path for path, mode in fs.created],
                         ["ux0:data", "ux0:data/trophy_io", "ux0:data/trophy_io/out"])
        self.assertTrue(all(mode & 0o006 == 0o006 for path, mode in fs.created))

    def test_existing_tree_is_not_recreated(self):
        fs = FakeVita()
        self.assertEqual(self.prepare(fs), (0, None))
        fs.created.clear()
        self.assertEqual(self.prepare(fs), (0, None))
        self.assertEqual(fs.created, [])

    def test_first_failure_stops_and_retry_preserves_parent(self):
        fs = FakeVita()
        fs.failures["ux0:data/trophy_io"] = INVAL
        self.assertEqual(self.prepare(fs), (INVAL, b"ux0:data/trophy_io"))
        self.assertNotIn("ux0:data/trophy_io/out", fs.nodes)
        fs.failures.clear()
        fs.created.clear()
        self.assertEqual(self.prepare(fs), (0, None))
        self.assertEqual([path for path, mode in fs.created],
                         ["ux0:data/trophy_io", "ux0:data/trophy_io/out"])

    def test_stat_permission_failure_does_not_attempt_creation(self):
        fs = FakeVita({"ux0:data": DENIED})
        self.assertEqual(self.prepare(fs), (DENIED, b"ux0:data/"))
        self.assertEqual(fs.created, [])

    def test_regular_file_collision_is_reported(self):
        fs = FakeVita({"ux0:data": 1, "ux0:data/trophy_io": 0})
        self.assertEqual(self.prepare(fs), (NOTDIR, b"ux0:data/trophy_io"))
        self.assertEqual(fs.created, [])

    def test_mkdir_race_requires_actual_directory(self):
        for kind, expected in [(1, (0, None)), (0, (NOTDIR, b"ux0:data/trophy_io"))]:
            with self.subTest(kind=kind):
                fs = FakeVita({"ux0:data": 1})
                fs.race = ("ux0:data/trophy_io", kind)
                self.assertEqual(self.prepare(fs), expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
