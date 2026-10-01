# Third-party notices

Trophy IO is distributed under **GPL-3.0-or-later**, with third-party code retaining the notices and terms identified below. See [LICENSE](LICENSE) for the full application license and [NOTICE](NOTICE) for dated modification notices. References were checked on 2026-10-01.

## VitaShell mounting code — GPL-3.0-or-later

**Copyright (C) 2015–2018, TheFloW.**

Upstream: [TheOfficialFloW/VitaShell](https://github.com/TheOfficialFloW/VitaShell), commit `81af70971ba18b8ce86215b04180f1e3d21cdfc9`.

| Upstream file | Trophy IO adaptation |
| --- | --- |
| [modules/kernel/main.c](https://github.com/TheOfficialFloW/VitaShell/blob/81af70971ba18b8ce86215b04180f1e3d21cdfc9/modules/kernel/main.c) | `modules/kernel/main.c`: AppMgr interface resolution, process lookup and mounting bridge |
| [modules/user/main.c](https://github.com/TheOfficialFloW/VitaShell/blob/81af70971ba18b8ce86215b04180f1e3d21cdfc9/modules/user/main.c) | `modules/user/main.c`: user-module forwarding wrapper |

Both upstream files explicitly permit GNU GPL version 3 or any later version. The root LICENSE is an unchanged copy of [VitaShell's GPL v3 license text](https://github.com/TheOfficialFloW/VitaShell/blob/81af70971ba18b8ce86215b04180f1e3d21cdfc9/LICENSE). The GPL-3.0-or-later choice comes from the source notices, not just the license document's title.

Modified on **2026-10-01** for Trophy IO: independent module names and Title ID, fixed NPWR trophy paths, bounded request copying, request/interface/result checks, and removal of unrelated USB, storage redirection and RIF-key operations. The source headers retain the original author and GPL grant/warranty notices. The combined application remains under GPL-3.0-or-later; third-party BSD notices below are preserved as well.

The app bundles its own compiled adaptations, **not prebuilt VitaShell binaries**. The original v0.1.0 source archive remains alongside its VPK in `releases/v0.1.0/`. The current repository's supplemental licensing notices also identify the modifications present in that release.

## Debug font — BSD-3-Clause

File: `third_party/debugScreenFont.c`.

Copyright (c) 2005 Marcus R. Brown, James Forshaw and John Kelley. The original source header and full copyright/conditions/disclaimer are preserved in [third_party/PSPSDK-LICENSE](third_party/PSPSDK-LICENSE), including the original license's additional author notices. These notices must also accompany binary distributions.

Copied from [vitasdk/samples common/debugScreenFont.c](https://github.com/vitasdk/samples/blob/fe8fbef570f3280586c0c20157146e3faefb2181/common/debugScreenFont.c), commit `fe8fbef570f3280586c0c20157146e3faefb2181`. The font's original BSD terms remain in effect; this project's GPL notice does not replace them.

## Build tools and platform interfaces

[VitaSDK](https://vitasdk.org/) supplies the compiler, headers, API stubs and packaging tools. [taiHEN](https://github.com/yifanlu/taiHEN) supplies the module-loading and kernel-interface ecosystem used through the SDK stubs. The build requires an already enabled homebrew environment on the device.

The SDK build image is not bundled with the application. Its pinned tag and digest are recorded in the README and release metadata; upstream tool/library projects retain their own licenses. VitaSDK documentation is community SDK documentation, not Sony/SCE's restricted SDK documentation.

## Design references

[Apollo Vita](https://github.com/bucanero/apollo-vita/tree/ed0ac564750fed38e4587b1a870c9fca93ff523e), commit `ed0ac564750fed38e4587b1a870c9fca93ff523e`, informed the research on independent applications and trophy mount/import/export workflows. Its application binaries are not bundled. Trophy IO v0.1.0 does not implement the planned import or database refresh workflows.
