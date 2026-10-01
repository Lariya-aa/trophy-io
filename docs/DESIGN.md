# Trophy IO 开发设计

项目是 PS Vita 原生奖杯文件工具。v0.1 先交付可构建的挂载导出版本，后续目标是一键导入并通过设备的 PFS 写入路径完成加密落盘。应用不自行实现 PFS 密码学，不把 DAT 修改或数据库刷新混同于加密写入。

## v0.1 调用链

`src/main.c` 枚举 `ur0:user/00/trophy/data/<NPWR>`，读取配套 `conf/<NPWR>/TROP.SFM`。界面发起导出后加载随包用户和内核模块，`trophyUserMount` 转发至 `trophyKernelMount`。

内核桥仅接受格式正确的 NPWR，内部构造固定奖杯路径、使用独立 Title ID `TROPIO001` 与挂载类型 `0x12F`；通过上游已知 AppMgr NID 解析接口，初始化结构并检查返回值，不接受任意目标路径。未知接口返回错误，运行时 NID 写入诊断日志。

成功挂载后仍通过原容器路径以只读模式读取 `TRPTITLE.DAT` 和 `TRPTRANS.DAT`，要求配置与两份 DAT 的 NPWR 一致。输出采用新建批次目录及排他创建，逐文件处理完整写入、同步、关闭、回读摘要，再复查源摘要。卸载成功后发布完成清单；失败保留部分输出，卸载失败停止队列。

成功清单 `manifest.json` 中的 `state=export_verified` 只表示本次程序完成该导出检查；接收端仍应比较所有文件长度与 SHA-256。它不是账号匹配或导入凭据。首版不使用旧注入脚本的固定偏移，不修改 DAT、奖杯数据库、PSN 或插件配置。

## 源码接口

| 文件 | 责任 |
| --- | --- |
| `include/mount_api.h` | 固定长度的 NPWR/挂载句柄请求，返回实际 NID 和挂载类型 |
| `modules/kernel/main.c` | 受限路径、接口解析、用户/内核参数复制与挂载 |
| `modules/user/main.c` | 用户模块转发 |
| `src/main.c` | 枚举、模块所有权、输出队列、清单与卸载 |
| `src/core.c` | NPWR 校验、配置标签边界、完整写入循环 |
| `src/sha256.c` | 摘要计算 |
| `src/screen.c` | 英文界面与内置字库 |

## 下一阶段：原样导入往返

1. 先取得实机单套导出证据，两次静止状态导出应保持相同数据摘要；再测全量导出。
2. 新导出直接作为未修改输入。输入按批次、NPWR 隔离，检查完整清单、身份、文件长度及摘要，不接受任意设备目标路径或旧平铺中转作为当前基线。
3. 在任何写回前保存整对原始 DAT 的配套备份与恢复日志，复核来源基线，防止错号、错账号、过期或截断输入。
4. 通过可写 PFS 挂载使用普通 I/O 成对写回，再卸载、重新挂载和回读。两个文件顺序写入不具有事务原子性，不能仅以返回成功认定完整验收。
5. 验证第二文件失败、短写、空间不足、卸载失败、应用中断与重新启动恢复；恢复失败停止批次，不继续覆盖后续集合。
6. 存储往返成功后单独验证奖杯应用显示及数据库一致性。禁止根据旧文自动删除 `sce_trop` 或整个数据库。

批量导入需要在单套恢复闭环通过后实现。真实数据/设备写回由用户授权，不在默认测试中执行。

## 验收记录及来源

首版交叉编译、8 项核心测试与 4 项包测试已有记录，保存在 `releases/v0.1.0/`；这些不证明设备运行成功。该目录构建证据中的源码摘要对应首次 VPK 构建及同目录源码归档；独立仓库随后新增的项目文档由 Git 跟踪。

参考源码核验于 2026-10-01：

- [VitaShell 挂载桥](https://github.com/TheOfficialFloW/VitaShell/blob/81af70971ba18b8ce86215b04180f1e3d21cdfc9/modules/kernel/main.c)，GPL-3.0-or-later；[PFS 流程](https://github.com/TheOfficialFloW/VitaShell/blob/81af70971ba18b8ce86215b04180f1e3d21cdfc9/pfs.c)。
- [Apollo Vita](https://github.com/bucanero/apollo-vita/tree/ed0ac564750fed38e4587b1a870c9fca93ff523e)，独立应用挂载与奖杯导入导出的研究来源，本项目没有复制其导入实现。
- [VitaSDK 文档](https://docs.vitasdk.org/)，社区 SDK 的项目文档，不冒充 Sony/SCE 受限 SDK 技术文档。

外部服务、版本和设备兼容性出现新证据后按范围更新，不把历史结论当作当前实机状态。

## 首版输出失败及 v0.1.1 修正 — 2026-10-02

用户确认气泡启动、按 □ 可列出三套奖杯。进入时 `Cannot create output root: 80010016`，按 × 后 `output directory failed: 80010002`；FTP 检查也没有找到输出根或日志。

[VitaSDK 的 SceIoAccessMode 注释](https://github.com/vitasdk/vita-headers/blob/master/include/psp2common/kernel/iofilemgr.h) 明确要求 unsafe 应用创建对象时添加 `SCE_S_IRSYS | SCE_S_IWSYS`（`0006`）。首版目录 `0700`、文件 `0600` 漏了这些权限，`80010016` 对应 EINVAL；根目录没有创建，后续子目录遇到 ENOENT（`80010002`）。v0.1.1 改用 `0706`、`0606`，保留首次失败路径并允许重试；不通过 FTP 预建目录来掩盖初始化问题。

6 项存储合成测试覆盖 SDK 权限约束、缺失父目录、已存在目录、首次失败后的重试、权限错误、文件冲突及并发创建。另对齐 VitaShell 主程序使用用户模块弱导入，以支持显式运行时加载；首版已经能进入 UI，不能将其误报为启动失败的根因。交叉编译、8 核心和 4 包检查通过；新版实机目录初始化、日志、挂载和单套导出仍需安装后验证。

## 全安装游戏奖杯扫描 — v0.1.2，2026-10-02

读取旧文 17 第 5.1 节：游戏自带 `ux0:app/<TitleID>/sce_sys/trophy/<ID>/TROPHY.TRP`，个人记录另在 `ur0:user/00/trophy/`。本次 FTP 实查 PCSG00122 等目录确认未玩游戏仍有原始包，不能只合并用户 conf/data 代替全部游戏发现。遍历 ux0:app、gr0:app、ux0:patch、ux0:rePatch 的所有应用目录，不按地区或 Title ID 前缀筛选，清点 trophy 子树中的全部文件，关联来源 Title ID，再按目录标识合并用户配置和数据。

实时只读清点覆盖 308 个 app/patch 目录，268 个来源目录有资源，275 个资源文件对应 254 个唯一奖杯编号；用户配置 31 套、个人数据 3 套。总包资源约 348 MB（十进制），不是每次启动都读取完整包。源码动态保存文件路径、大小和来源；扫描错误使目录标为 incomplete，不能宣称全部覆盖。单套未知目录保留可见，但不放宽个人挂载身份校验。系统共享目录和 PFS 元数据进入元数据目录，不作为普通游戏资源复制。

每次扫描保存独立 catalog JSON。×/△ 对游戏包和可读配置按原字节导出 resources/，采用 64 KiB 流式复制、源与目标重读摘要，并在完成后发布 resources.json；已有成对 DAT 仍通过 PFS 明文导出及 manifest.json。文件前缀加目录内序号保留同名包的多版本来源，清单记录原路径。原始包不冒充个人进度，程序不创建未玩游戏的个人奖杯数据。TrophyCatalogProvider 抽离发现接口，未来存档 provider 独立注册和定义导出策略；本版不扫描存档。

v0.1.1 已实机核验三套单独导出与 9 文件哈希。v0.1.2 目录/SFO/JSON 合成检查和交叉编译不能替代新版实机扫描、资源复制及全量批次验收。
