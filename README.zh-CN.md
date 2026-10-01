# Trophy IO

[English](README.md) · **简体中文** · [日本語](README.ja.md)

[GitHub 仓库](https://github.com/Lariya-aa/trophy-io)

PS Vita 原生奖杯挂载与明文导出工具，挂载桥改编自 [VitaShell](https://github.com/TheOfficialFloW/VitaShell)，作为独立应用和 Git 项目开发。

**v0.1.0 是仅支持导出的第一版。** 已完成交叉编译、8 项主机核心测试与 4 项包验证测试；**尚未完成实机安装、启动、挂载或导出验证**。导入与加密写回属于后续计划，本版未提供。

## 下载

| 制品 | 入口 |
| --- | --- |
| VPK | [TrophyBatchManager-v0.1.0.vpk](releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk) |
| SHA-256 | [校验值](releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk.sha256) |
| 对应源码 | [v0.1.0 源码归档](releases/v0.1.0/TrophyBatchManager-v0.1.0-source.tar.gz) |
| 构建证据 | [元数据](releases/v0.1.0/TrophyBatchManager-v0.1.0-build.json) · [构建日志](releases/v0.1.0/TrophyBatchManager-v0.1.0-build.log) |

气泡名称：**Trophy IO 0.1**；Title ID：**`TROPIO001`**。VPK 文件名保留最初的 `TrophyBatchManager` 名称。

发行目录保留首次生成的 VPK 与对应源码归档。构建证据中的源码摘要对应该归档；仓库随后补充了项目文档与许可证声明。`v0.1.0` Git 标签记录独立项目的初始基线。

## 功能

- 枚举 `ur0:user/00/trophy/data/` 下实际存在的 NPWR 目录，从配套 `conf/<NPWR>/TROP.SFM` 显示标题。
- 使用随包独立编译的用户/内核模块挂载，导出选中项或全部已列出的奖杯集。
- 导出前检查挂载返回值、句柄、配置与两份 DAT 的 NPWR。
- 新建输出批次，处理短读/短写、同步与关闭错误，回读校验 SHA-256，并在卸载前复查源摘要。
- 卸载成功后发布 `manifest.json`。失败目录保留用于诊断，`.pending` 清单不算完成。
- 日志记录 NPWR、路径、结果码、模块 NID 与句柄，不记录 DAT 内容或账号标识。

上限为 **128 套奖杯、每个文件 1 MiB**。NPWR 检查不等同于完整 DAT 格式校验或账号匹配。源奖杯文件只读打开，输出写入应用自有目录；不更新奖杯数据库、不修改插件配置、不替换 VitaShell 模块。

## 环境与使用

需要已有 homebrew/taiHEN 环境及相应权限。测试导出前，退出可能使用所选数据的游戏和奖杯应用。

将 VPK 传至 PSV，用 VitaShell 安装，启动 **Trophy IO 0.1**。应用界面为英文；三语 README 不代表应用已有三语界面。

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
python3 tools/verify_vpk.py releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk
python3 tests/test_package.py releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk
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
