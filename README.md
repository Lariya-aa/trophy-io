# Trophy IO v0.1.0

本项目第一版原生奖杯导出工具，包名 `TrophyBatchManager-v0.1.0.vpk`，独立 Title ID `TROPIO001`，气泡名称 `Trophy IO 0.1`。源码与挂载桥采用 GPL-3.0-or-later；完整第三方说明见 [THIRD_PARTY.md](THIRD_PARTY.md)。

## 独立项目入口

本目录是独立 Git 仓库，默认分支 `main`，首版基线标签 `v0.1.0`。从本目录执行构建与测试，不依赖上层游戏资产库、其 Git 历史或私有备份。Agent 先读 [AGENTS.md](AGENTS.md)，架构与下一阶段计划见[开发设计](docs/DESIGN.md)，版本记录见 [CHANGELOG.md](CHANGELOG.md)。

[v0.1.0 安装包](releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk)、[校验值](releases/v0.1.0/TrophyBatchManager-v0.1.0.vpk.sha256)、[构建证据](releases/v0.1.0/TrophyBatchManager-v0.1.0-build.json)与[首版对应源码归档](releases/v0.1.0/TrophyBatchManager-v0.1.0-source.tar.gz)随此基线保存。源码归档保留首次生成 VPK 时的版本；本次独立化只调整项目说明与 Git 边界，没有改变 C 源码或 VPK 字节。

```sh
git log --oneline
git show v0.1.0 --stat
```

## 当前功能

- 枚举 `ur0:user/00/trophy/data/` 下实际存在的 NPWR 目录，以 `conf/<NPWR>/TROP.SFM` 显示标题。
- × 导出选中项，△ 顺序导出全部，方向键切换，□ 重新扫描，○ 退出。首版使用英文界面与内置小字库。
- 加载随包独立编译的挂载模块，通过 VitaShell 的 `0x12F` 奖杯挂载流程读取原路径。检查返回值、句柄、配置与两份 DAT 中的 NPWR，拒绝密文、截断或串号数据。
- 导出到 `ux0:data/trophy_io/out/run-<计时值>-<序号>/<NPWR>/`，包含 `data/TRPTITLE.DAT`、`data/TRPTRANS.DAT`、`conf/TROP.SFM` 与 `manifest.json`。
- 新建批次及排他创建文件，处理短读/短写，同步、关闭、回读并核对 SHA-256；结束前再次比较源文件摘要，卸载成功后发布清单。失败目录保留用于诊断；`.pending` 清单不算完成。完整性以清单中每个文件的长度和摘要共同判定。
- 日志在 `ux0:data/trophy_io/diagnostic.log`，记录 NPWR、路径、返回码、模块 NID 与挂载句柄，不记录 DAT 内容或账号标识。卸载失败停止队列，○ 可重试清理。

首版只提供导出。导入、PFS 写回、账号校验、DAT 修改、数据库刷新与恢复事务尚未实现；导出清单不是导入授权或账号匹配凭据。最大 128 套，每个文件上限 1 MiB，超过限制明确报错。

## 通用构建

不把 3.74 设为开发基线或最低运行版本：SFO 的 `PSP2_SYSTEM_VER=0`，SELF 的最低固件字段为零，不按系统版本字符串限制启动。挂载桥按上游已知 `SceAppMgr` NID 解析接口，未知接口返回 `FFFF8EFF`，不猜测内核偏移。打包字段没有高版本门槛，不代表所有设备已通过运行测试。

需要已有的 homebrew/taiHEN 环境及相应权限。自有模块临时加载；不修改 `tai/config.txt`，不替换 VitaShell 模块。运行前退出正在使用对应奖杯数据的游戏与奖杯应用，避免并发修改使本次导出基线变化。

构建使用 `vitasdk/vitasdk:2026.08-20260815`，固定镜像摘要：

```text
sha256:7f5eee50ff95b73c8c847dbfef6227aa0035886444b4d0254c21da8369ff1efc
```

该镜像包含 GCC 15.2.0、CMake 3.28 及 taiHEN stubs；不需要在宿主机安装 VitaSDK。共享盘不适合作为 CMake 临时构建目录，使用本机临时盘：

```sh
sh tools/build.sh
python3 tests/test_core.py
python3 tools/verify_vpk.py /实际构建目录/TrophyBatchManager-v0.1.0.vpk
python3 tests/test_package.py /实际构建目录/TrophyBatchManager-v0.1.0.vpk
```

脚本打印实际构建目录并保存日志；容器构建时禁用网络，源码只读挂载。源码含可重复生成的 LiveArea 图片、字库和许可证。主机测试不调用 PSV API，也不连接设备。

## 第一版验收边界

已完成交叉编译、8 项主机核心测试、VPK CRC/目录/SFO/SELF/LiveArea 静态检查和 4 项包验证测试（包括错 Title ID、损坏 SELF、高最低固件字段被拒绝）。尚未在实机安装、启动或挂载；已有 FTP 快照仅用于确定目录与样本约束。

首次设备测试先导出一个已有奖杯集，回传诊断日志及导出目录，检查两个 DAT 的 NPWR 和摘要；静止状态下再次导出并比较，再测试三套批量导出。PC 侧通过 FTP 读取导出目录即可，不需要运行现有注入脚本。错误码 `FFFF8EFF` 为未知 AppMgr NID，`FFFF8EFE` 为接口解析失败，`FFFF8EFD` 为进程信息异常，`FFFF8EFC` 为非法 NPWR；`FFFF8DFF` 为 I/O/校验失败，`FFFF8DFE` 为 NPWR 不一致，`FFFF8DFD` 为源变化，`FFFF8DFC` 为容量限制。

上游参考：[VitaShell 挂载桥](https://github.com/TheOfficialFloW/VitaShell/blob/81af70971ba18b8ce86215b04180f1e3d21cdfc9/modules/kernel/main.c)、[VitaSDK API](https://docs.vitasdk.org/)、[工具链 SELF 生成](https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-make-fself/vita-make-fself.c)。核验日期：2026-10-01。项目内设计与验收入口见[开发设计](docs/DESIGN.md)。
