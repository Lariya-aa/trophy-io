# Trophy IO

**English** · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

[GitHub repository](https://github.com/Lariya-aa/trophy-io)

A native PS Vita utility for mounting and exporting decrypted trophy data. Its mounting bridge is derived from [VitaShell](https://github.com/TheOfficialFloW/VitaShell); Trophy IO is developed as an independent application and Git repository.

**v0.1.0 is an export-only first build.** Cross-compilation, eight host core tests and four package validation tests have passed. Installation, startup, mounting and export on a physical Vita have **not yet been verified**. Import and encrypted write-back are planned, not available in this version.

## Download

[v0.1.0 prerelease](https://github.com/Lariya-aa/trophy-io/releases/tag/v0.1.0) — download the VPK and corresponding source below.

| Artifact | Link |
| --- | --- |
| VPK | [TrophyBatchManager-v0.1.0.vpk](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.0/TrophyBatchManager-v0.1.0.vpk) |
| SHA-256 | [Checksum](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.0/TrophyBatchManager-v0.1.0.vpk.sha256) |
| Corresponding source | [v0.1.0 source archive](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.0/TrophyBatchManager-v0.1.0-source.tar.gz) |
| Build evidence | [Metadata](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.0/TrophyBatchManager-v0.1.0-build.json) · [Build log](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.0/TrophyBatchManager-v0.1.0-build.log) |

Application name: **Trophy IO 0.1**. Title ID: **`TROPIO001`**. The VPK filename retains the original `TrophyBatchManager` name.

The release directory preserves the original VPK and its corresponding source archive. Source hashes in the build metadata describe that archive; project documentation and licensing notices have since been expanded. The `v0.1.0` Git tag records the initial project baseline.

## Features

- Enumerate existing NPWR directories under `ur0:user/00/trophy/data/` and display titles from the matching `conf/<NPWR>/TROP.SFM`.
- Export the selected trophy set or all listed sets through bundled, separately compiled user and kernel mounting modules.
- Check the mount result, handle and NPWR identities in the configuration and both DAT files before exporting.
- Create a new output batch, handle short reads/writes, synchronize and close files, verify output with SHA-256, and recheck source hashes before unmounting.
- Publish `manifest.json` after successful unmounting. Partial directories remain for diagnosis; `.pending` manifests are incomplete.
- Record NPWR, paths, result codes, module NID and mount handles without logging DAT contents or account identifiers.

Limits: **128 trophy sets**, **1 MiB per file**. NPWR checks do not constitute full DAT format validation or account matching. Existing trophy data is opened read-only; output is written to the application's own directory. The app does not update trophy databases, modify plugin configuration or replace VitaShell modules.

## Requirements and use

Use an existing homebrew/taiHEN environment with the required permissions. Close games and the trophy application that may be using the selected data before testing an export.

Transfer the VPK to the Vita, install it with VitaShell, then launch **Trophy IO 0.1**. The in-app interface is English; translated READMEs do not change the UI language.

| Button | Action |
| --- | --- |
| Up / Down | Select a trophy set |
| × | Export the selected set |
| △ | Export all listed sets |
| □ | Rescan |
| ○ | Exit; retry cleanup if unmounting failed |

Output layout:

```text
ux0:data/trophy_io/
├── diagnostic.log
└── out/run-<timer>-<sequence>/<NPWR>/
    ├── data/TRPTITLE.DAT
    ├── data/TRPTRANS.DAT
    ├── conf/TROP.SFM
    └── manifest.json
```

When receiving an export, verify every file's length and SHA-256 against its manifest. A manifest is not an import authorization or proof of account matching. An unmount failure stops the queue and is recorded in the log.

## Build and verify

Run from the repository root. Building requires Docker; host tests require Python 3.9+ and a C compiler (`cc`).

```sh
sh tools/build.sh
python3 tests/test_core.py
python3 tools/verify_vpk.py releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk
python3 tests/test_package.py releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk
```

The build script prints the temporary build directory and saves its log there. Substitute the newly built VPK's path to verify a new build. Source is mounted read-only, and container networking is disabled during compilation. The image must be available locally or downloadable by Docker before the container starts.

Pinned image: `vitasdk/vitasdk:2026.08-20260815`, with GCC 15.2.0 and CMake 3.28:

```text
sha256:7f5eee50ff95b73c8c847dbfef6227aa0035886444b4d0254c21da8369ff1efc
```

Use a local temporary filesystem for CMake output; shared-disk temporary file operations failed during the first build. Assets, fonts, licenses and build instructions are included. Host and package checks do not connect to a Vita.

The VPK uses a general build: `PSP2_SYSTEM_VER=0` and zero minimum-firmware fields in its SELF files. The application does not gate startup on a firmware-version string. The mounting bridge resolves known upstream AppMgr NIDs and returns an error for unknown interfaces; packaging metadata does not establish device compatibility.

## Development

[Design and next steps (Chinese)](docs/DESIGN.md) · [Changelog (Chinese)](CHANGELOG.md) · [Agent instructions (Chinese)](AGENTS.md) · [GitHub publishing](docs/PUBLISHING.md)

Next: verify single-set and repeated exports on hardware, then implement unmodified import round-trips with paired backups, account checks, recovery logs and remount/read-back verification. Database/display behavior needs separate validation. Import, timestamp editing and PSN synchronization are outside v0.1.0.

When reporting a device problem, include the NPWR, relevant result codes and reproduction steps. Remove identifying paths from logs; do not attach raw DAT files, trophy databases, account identifiers or credentials to public issues.

## License and credits

Trophy IO is licensed under **GNU GPL version 3 or, at your option, any later version (`GPL-3.0-or-later`)**. See [LICENSE](LICENSE), [NOTICE](NOTICE) and [THIRD_PARTY.md](THIRD_PARTY.md).

The mounting bridge and user-module wrapper are adapted from VitaShell, **Copyright © 2015–2018 TheFloW**, at commit `81af70971ba18b8ce86215b04180f1e3d21cdfc9`. The reused files explicitly permit GPL v3 or later. Modified code and the combined application retain those GPL terms; distributing a VPK also requires providing its corresponding source under the applicable GPL distribution terms.

The debug font retains its original **BSD 3-clause** terms and copyright notices in [PSPSDK-LICENSE](third_party/PSPSDK-LICENSE). VitaSDK and taiHEN provide the build tools and platform interfaces; Apollo Vita informed the design research. Their roles and pinned references are documented in the third-party notices. No prebuilt VitaShell binaries are bundled.
