# Trophy IO

[English](README.md) · **简体中文** · [日本語](README.ja.md)

[GitHub 仓库](https://github.com/Lariya-aa/trophy-io)

PS Vita 原生奖杯挂载与明文导出工具，挂载桥改编自 [VitaShell](https://github.com/TheOfficialFloW/VitaShell)，作为独立应用和 Git 项目开发。

**v0.1.2 是覆盖已安装游戏全部奖杯资源的预发布测试版。** 扫描 `ux0:app`、`gr0:app`、`ux0:patch`、`ux0:rePatch`，不按游戏 Title ID 前缀筛选；清点 `sce_sys/trophy` 内所有文件，并合并用户奖杯 `conf` 与 `data`。未玩过的游戏显示 `Game files`，个人数据是否可导出另行判断。2026-10-02 全量 FTP 清点覆盖 308 个应用/补丁目录，发现 254 个奖杯编号、275 个游戏内奖杯资源文件。v0.1.1 已实机确认安装与三套独立明文导出，9 个文件哈希均匹配；**v0.1.2 新扫描与资源复制仍待实机验证**。导入和加密写回未实现。


## 下载

[v0.1.2 预发布版](https://github.com/Lariya-aa/trophy-io/releases/tag/v0.1.2)：完整安装游戏奖杯资源扫描与导出。

| 制品 | 入口 |
| --- | --- |
| VPK | [TrophyBatchManager-v0.1.2.vpk](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2.vpk) |
| SHA-256 | [SHA-256](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2.vpk.sha256) |
| 对应源码 | [对应源码](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2-source.tar.gz) |
| 构建证据 | [JSON](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2-build.json) · [Log](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2-build.log) |

气泡名称：**Trophy IO 0.1.2**；Title ID：**`TROPIO001`**。

[首版 v0.1.0](https://github.com/Lariya-aa/trophy-io/releases/tag/v0.1.0) 继续保留。各版 VPK 与对应源码归档保持原字节；构建元数据记录发布前的源码快照，发布标签还包含随后更新的发布文档。

## 功能

扫描后自动保存 `ux0:data/trophy_io/catalog-<计时值>.json`，记录完整文件路径、来源 Title ID、大小及扫描错误。左右键翻页；× 导出选中项，△ 导出全部可读奖杯集，包含游戏原始奖杯包及用户配置文件。这些资源按原字节流式复制到 `resources/`，`resources.json` 记录来源路径、长度及回读一致的 SHA-256。已有个人 DAT 配对仍走已验证的挂载明文导出，使用 `manifest.json`。共享系统元数据单独清点，不按游戏导出。原始奖杯包不是个人进度，程序不会为未玩游戏生成个人数据。存档扫描保留独立 provider 接口，本版未启用。

- 枚举 `ur0:user/00/trophy/data/` 下实际存在的 NPWR 目录，从配套 `conf/<NPWR>/TROP.SFM` 显示标题。
- 使用随包独立编译的用户/内核模块挂载，导出选中项或全部已列出的奖杯集。
- 导出前检查挂载返回值、句柄、配置与两份 DAT 的 NPWR。
- 新建输出批次，处理短读/短写、同步与关闭错误，回读校验 SHA-256，并在卸载前复查源摘要。
- 卸载成功后发布 `manifest.json`。失败目录保留用于诊断，`.pending` 清单不算完成。
- 日志记录 NPWR、路径、结果码、模块 NID 与句柄，不记录 DAT 内容或账号标识。

上限为 **动态扩容目录、个人 DAT/配置读取每文件 1 MiB**；游戏资源采用流式复制。NPWR 检查不等同于完整 DAT 格式校验或账号匹配。源奖杯文件只读打开，输出写入应用自有目录；不更新奖杯数据库、不修改插件配置、不替换 VitaShell 模块。

## 环境与使用

需要已有 homebrew/taiHEN 环境及相应权限。测试导出前，退出可能使用所选数据的游戏和奖杯应用。

将 VPK 传至 PSV，用 VitaShell 安装，启动 **Trophy IO 0.1.2**。应用界面为英文；三语 README 不代表应用已有三语界面。

| 按键 | 操作 |
| --- | --- |
| 上 / 下 | 选择奖杯集 |
| × | 导出选中项 |
| △ | 导出全部已列出的奖杯集 |
| □ | 重新扫描 |
| ○ | 退出；卸载失败时重试清理 |

输出结构：

```text
ux0:data/trophy_io/
├── diagnostic.log
└── out/run-<timer>-<sequence>/<NPWR>/
    ├── data/TRPTITLE.DAT
    ├── data/TRPTRANS.DAT
    ├── conf/TROP.SFM
    └── manifest.json
```

接收导出时，应按清单核对每个文件的长度与 SHA-256。清单不是导入授权或账号匹配凭据。卸载失败会停止队列并记录日志。

## 构建与检查

从仓库根执行。构建需要 Docker，主机测试需要 Python 3.9+ 和 C 编译器 `cc`。

```sh
sh tools/build.sh
python3 tests/test_core.py
python3 tests/test_storage.py
python3 tests/test_catalog.py
python3 tools/verify_vpk.py releases/v0.1.2/TrophyBatchManager-v0.1.2.vpk --app-version 00.12
python3 tests/test_package.py releases/v0.1.2/TrophyBatchManager-v0.1.2.vpk --app-version 00.12
```

构建脚本打印实际临时构建目录并在其中保存日志；检查新包时替换为新构建 VPK 的路径。容器内编译时源码只读、网络关闭；启动容器前 Docker 需要已有镜像或能够下载镜像。

固定镜像 `vitasdk/vitasdk:2026.08-20260815`，包含 GCC 15.2.0 和 CMake 3.28：

```text
sha256:7f5eee50ff95b73c8c847dbfef6227aa0035886444b4d0254c21da8369ff1efc
```

CMake 输出放在本机临时盘；首轮共享盘构建出现过临时文件操作失败。仓库包含资源、字库、许可证和构建说明。主机与包检查不连接设备。

采用通用构建：SFO 的 `PSP2_SYSTEM_VER=0`，SELF 最低固件字段为零，不按系统版本字符串限制启动。挂载桥解析上游已知 AppMgr NID，未知接口返回错误。打包字段不能证明实机兼容性。

## 开发

[设计与下一阶段](docs/DESIGN.md) · [版本记录](CHANGELOG.md) · [Agent 指南](AGENTS.md) · [GitHub 发布说明（英文）](docs/PUBLISHING.md)

下一步先完成实机单套及重复导出验证，再实现带成对备份、账号检查、恢复日志和重挂回读的原样导入往返。数据库与奖杯显示需要单独验证。v0.1.0 不提供导入、时间戳修改或 PSN 同步。

报告实机问题时提供 NPWR、相关错误码与复现步骤。日志中的身份路径应脱敏；不要在公开 Issue 上传原始 DAT、奖杯数据库、账号标识或凭据。

## 许可证与致谢

Trophy IO 使用 **GNU GPL 第 3 版或任何后续版本（`GPL-3.0-or-later`）**。详见 [LICENSE](LICENSE)、[NOTICE](NOTICE) 与 [THIRD_PARTY.md](THIRD_PARTY.md)。

挂载桥和用户模块包装改编自 VitaShell，**Copyright © 2015–2018 TheFloW**，固定参考提交 `81af70971ba18b8ce86215b04180f1e3d21cdfc9`。上游文件明确允许 GPL v3 或更高版本；本项目的修改代码及组合应用继续沿用该协议，分发 VPK 时同时按 GPL 适用条款提供对应源码。

调试字库保留原来的 **BSD 三条款**许可证与作者声明，见 [PSPSDK-LICENSE](third_party/PSPSDK-LICENSE)。VitaSDK 与 taiHEN 提供构建工具和平台接口，Apollo Vita 是设计研究来源；具体范围与固定参考版本记录在第三方说明中。本项目未打包 VitaShell 的预编译二进制。
