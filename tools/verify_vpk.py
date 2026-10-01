#!/usr/bin/env python3
"""Verify release structure and metadata without executing the binaries."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile


def sfo_values(data):
    magic, version, keys, values, count = struct.unpack_from("<4s4I", data)
    if magic != b"\0PSF" or count > 128:
        raise ValueError("Invalid SFO header")
    result = {}
    for index in range(count):
        keyoff, kind, size, capacity, valueoff = struct.unpack_from("<HHIII", data, 20 + 16 * index)
        key = data[keys + keyoff:].split(b"\0", 1)[0].decode("ascii")
        value = data[values + valueoff:values + valueoff + size]
        if len(value) != size or size > capacity:
            raise ValueError("Truncated SFO")
        result[key] = struct.unpack("<I", value)[0] if kind == 0x404 else value.rstrip(b"\0").decode("utf-8")
    return result


def verify(path):
    expected = {"eboot.bin", "sce_sys/param.sfo", "sce_module/trophy_kernel.skprx",
                "sce_module/trophy_user.suprx", "sce_sys/icon0.png",
                "sce_sys/livearea/contents/template.xml", "sce_sys/livearea/contents/bg.png",
                "sce_sys/livearea/contents/startup.png", "LICENSE", "THIRD_PARTY.md",
                "third_party/PSPSDK-LICENSE"}
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or set(names) != expected:
            raise ValueError(f"Unexpected VPK entries: {names}")
        if archive.testzip() is not None:
            raise ValueError("VPK CRC failure")
        sfo = sfo_values(archive.read("sce_sys/param.sfo"))
        if sfo["TITLE_ID"] != "TROPIO001" or sfo["APP_VER"] != "00.10" or sfo["PSP2_SYSTEM_VER"] != 0:
            raise ValueError(f"Unexpected metadata: {sfo}")
        binaries = {}
        for name in ["eboot.bin", "sce_module/trophy_kernel.skprx", "sce_module/trophy_user.suprx"]:
            data = archive.read(name)
            if len(data) < 4096 or data[:4] != b"SCE\0":
                raise ValueError(f"Invalid SELF: {name}")
            # SCE header offsets: appinfo at 0x38, controlinfo at 0x68/0x70.
            appinfo = struct.unpack_from("<Q", data, 0x38)[0]
            authid = struct.unpack_from("<Q", data, appinfo)[0]
            control, length = struct.unpack_from("<QQ", data, 0x68)
            end = control + length
            minimum = None
            while control < end:
                kind, size = struct.unpack_from("<II", data, control)
                if size < 16 or control + size > end:
                    raise ValueError("Invalid SELF control record")
                if kind == 4:
                    minimum = struct.unpack_from("<Q", data, control + 16 + 20 + 32 + 4)[0]
                control += size
            if minimum != 0:
                raise ValueError(f"SELF firmware minimum is not zero: {minimum}")
            binaries[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                              "authid": f"0x{authid:016x}", "minimum_required_firmware": minimum}
        for name, dimensions in [("icon0.png", (128, 128)), ("livearea/contents/bg.png", (840, 500)),
                                  ("livearea/contents/startup.png", (280, 158))]:
            data = archive.read("sce_sys/" + name)
            width, height, depth, kind, compression, filtertype, interlace = struct.unpack_from(">IIBBBBB", data, 16)
            if (width, height) != dimensions or (depth, kind, compression, filtertype, interlace) != (8, 3, 0, 0, 0):
                raise ValueError(f"Invalid indexed PNG asset: {name}")
        template = ET.fromstring(archive.read("sce_sys/livearea/contents/template.xml"))
        if template.tag != "livearea" or template.find("gate/startup-image").text != "startup.png":
            raise ValueError("Invalid LiveArea template")
    return {"artifact": path.name, "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "entries": sorted(expected),
            "sfo": sfo, "self": binaries, "device_execution_verified": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vpk", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.vpk), ensure_ascii=False, indent=2))
