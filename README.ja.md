# Trophy IO

[English](README.md) · [简体中文](README.zh-CN.md) · **日本語**

[GitHub リポジトリ](https://github.com/Lariya-aa/trophy-io)

[VitaShell](https://github.com/TheOfficialFloW/VitaShell) のマウントコードを基にした、PS Vita 用のトロフィーデータのマウント・復号済みエクスポートツールです。独立したアプリケーションおよび Git プロジェクトとして開発しています。

**v0.1.2 はインストール済みゲームの全トロフィーリソースを対象とするプレリリースのテスト版です。** `ux0:app`、`gr0:app`、`ux0:patch`、`ux0:rePatch` を Title ID の接頭辞で絞らず走査し、`sce_sys/trophy` 内の全ファイルとユーザーの `conf`・`data` を統合します。未プレイのゲームも `Game files` として表示し、個人データの準備状態は区別します。2026-10-02 の FTP 全体調査では 308 個のアプリ・パッチディレクトリから 254 個のトロフィー ID、275 個のゲームリソースファイルを確認しました。v0.1.1 の実機インストール、3 セットの個別復号エクスポート、9 ファイルのハッシュ一致は確認済みですが、**v0.1.2 の新しい走査とリソースコピーは実機検証待ちです。** インポートと暗号化書き戻しは未実装です。


## ダウンロード

[v0.1.2 プレリリース](https://github.com/Lariya-aa/trophy-io/releases/tag/v0.1.2)：インストール済みゲームの全トロフィーリソース走査とエクスポート。

| ファイル | リンク |
| --- | --- |
| VPK | [TrophyBatchManager-v0.1.2.vpk](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2.vpk) |
| SHA-256 | [SHA-256](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2.vpk.sha256) |
| 対応ソース | [対応ソース](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2-source.tar.gz) |
| ビルド記録 | [JSON](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2-build.json) · [Log](https://github.com/Lariya-aa/trophy-io/releases/download/v0.1.2/TrophyBatchManager-v0.1.2-build.log) |

アプリ名：**Trophy IO 0.1.2**。Title ID：**`TROPIO001`**。

[初期 v0.1.0 プレリリース](https://github.com/Lariya-aa/trophy-io/releases/tag/v0.1.0) も引き続き利用できます。各版の VPK と対応ソースは元のバイト列を保存します。ビルド記録は公開前のソーススナップショットに対応し、公開タグには更新済みの公開文書も含まれます。

## 機能

走査後は `ux0:data/trophy_io/catalog-<timer>.json` に全パス、元 Title ID、サイズ、走査エラーを保存します。左右でページを切り替え、× は選択したセット、△ は読み取り可能な全セットをエクスポートします。ゲームの元トロフィーパッケージとユーザー設定は変更せず `resources/` にストリームコピーし、`resources.json` に元パス、サイズ、読み戻し検証済み SHA-256 を記録します。既存の個人 DAT ペアには従来の復号エクスポートと `manifest.json` を使います。共有システムメタデータは一覧に含めますがゲームとしてコピーしません。元パッケージは個人の進捗ではなく、未プレイの個人データは生成しません。セーブデータ用の独立 provider インターフェースは予約済みですが、走査は無効です。

- `ur0:user/00/trophy/data/` 内に実在する NPWR ディレクトリを列挙し、対応する `conf/<NPWR>/TROP.SFM` からタイトルを表示します。
- 同梱の独自ビルドしたユーザー／カーネルモジュールでマウントし、選択したセットまたは一覧の全セットをエクスポートします。
- マウントの戻り値とハンドル、設定ファイルと 2 つの DAT の NPWR を確認します。
- 新しい出力バッチを作成し、不完全な読み書き、同期、クローズを処理します。SHA-256 による読み戻し検証と、アンマウント前の元データのハッシュ再確認を行います。
- アンマウント成功後に `manifest.json` を公開します。失敗した出力は診断用に残り、`.pending` マニフェストは未完了を示します。
- NPWR、パス、結果コード、モジュール NID、マウントハンドルをログに記録します。DAT の内容やアカウント識別子は記録しません。

上限は **動的に拡張する一覧、個人 DAT・設定の読み込みは 1 ファイルあたり 1 MiB**。ゲームリソースはストリームコピー です。NPWR の確認は DAT 全体の形式検証やアカウント照合を意味しません。元のトロフィーファイルは読み取り専用で開き、出力はアプリ専用ディレクトリに保存します。トロフィーデータベースやプラグイン設定を変更せず、VitaShell のモジュールも置き換えません。

## 動作環境と使い方

必要な権限を持つ既存の homebrew/taiHEN 環境が必要です。エクスポートのテスト前に、対象データを使用する可能性があるゲームとトロフィーアプリを終了してください。

VPK を Vita に転送し、VitaShell でインストールして **Trophy IO 0.1.2** を起動します。アプリの UI は英語です。README の翻訳は UI の多言語対応を意味しません。

| ボタン | 操作 |
| --- | --- |
| 上 / 下 | トロフィーセットを選択 |
| × | 選択したセットをエクスポート |
| △ | 一覧の全セットをエクスポート |
| □ | 再スキャン |
| ○ | 終了。アンマウント失敗時はクリーンアップを再試行 |

出力構成：

```text
ux0:data/trophy_io/
├── diagnostic.log
└── out/run-<timer>-<sequence>/<NPWR>/
    ├── data/TRPTITLE.DAT
    ├── data/TRPTRANS.DAT
    ├── conf/TROP.SFM
    └── manifest.json
```

エクスポートを受け取ったら、各ファイルのサイズと SHA-256 をマニフェストと照合してください。マニフェストはインポート許可やアカウント一致の証明ではありません。アンマウントに失敗すると処理キューを停止し、ログに記録します。

## ビルドと検証

リポジトリのルートから実行します。ビルドには Docker、ホストテストには Python 3.9 以降と C コンパイラ `cc` が必要です。

```sh
sh tools/build.sh
python3 tests/test_core.py
python3 tests/test_storage.py
python3 tests/test_catalog.py
python3 tools/verify_vpk.py releases/v0.1.2/TrophyBatchManager-v0.1.2.vpk --app-version 00.12
python3 tests/test_package.py releases/v0.1.2/TrophyBatchManager-v0.1.2.vpk --app-version 00.12
```

ビルドスクリプトは一時ビルドディレクトリを表示し、そこにログを保存します。新しいビルドを検証する場合は、その VPK のパスに置き換えてください。コンパイル中はソースを読み取り専用でマウントし、コンテナのネットワークを無効にします。コンテナ起動前に Docker イメージがローカルに存在するか、Docker がダウンロードできる必要があります。

固定イメージは `vitasdk/vitasdk:2026.08-20260815` で、GCC 15.2.0 と CMake 3.28 を含みます。

```text
sha256:7f5eee50ff95b73c8c847dbfef6227aa0035886444b4d0254c21da8369ff1efc
```

CMake の出力にはローカルの一時ファイルシステムを使ってください。初回ビルドでは共有ディスク上の一時ファイル操作が失敗しました。リポジトリにはアセット、フォント、ライセンス、ビルド手順が含まれます。ホストテストとパッケージ検証は Vita に接続しません。

汎用ビルドを使用し、SFO の `PSP2_SYSTEM_VER=0`、SELF の最低ファームウェア値はゼロです。起動時にバージョン文字列で制限しません。マウントコードは上流の既知の AppMgr NID を解決し、未知のインターフェースではエラーを返します。パッケージの値だけでは実機互換性を証明できません。

## 開発

[設計と今後の計画（中国語）](docs/DESIGN.md) · [変更履歴（中国語）](CHANGELOG.md) · [Agent ガイド（中国語）](AGENTS.md) · [GitHub 公開手順（英語）](docs/PUBLISHING.md)

次の段階では、実機で単一セットと繰り返しエクスポートを検証し、その後ペアのバックアップ、アカウント照合、復旧ログ、再マウント後の読み戻しを備えた未変更データのインポートを実装します。データベースとトロフィー表示は別途検証が必要です。v0.1.0 にインポート、タイムスタンプ編集、PSN 同期は含まれません。

実機の問題を報告する際は、NPWR、関連する結果コード、再現手順を記載してください。ログの個人を識別できるパスは削除し、公開 Issue に元の DAT、トロフィーデータベース、アカウント識別子、認証情報を添付しないでください。

## ライセンスと謝辞

Trophy IO は **GNU GPL 第 3 版、または任意の後続版（`GPL-3.0-or-later`）** で提供します。[LICENSE](LICENSE)、[NOTICE](NOTICE)、[THIRD_PARTY.md](THIRD_PARTY.md) を参照してください。

マウントコードとユーザーモジュールのラッパーは VitaShell を基にしており、**Copyright © 2015–2018 TheFloW** です。参照コミットは `81af70971ba18b8ce86215b04180f1e3d21cdfc9` です。上流ファイルは GPL v3 以降を明示しています。本プロジェクトの変更コードと組み合わせたアプリにもその条件を適用し、VPK を配布する際は GPL の該当する配布条件に従って対応するソースを提供します。

デバッグフォントは元の **BSD 3 条項**の条件と著作権表記を維持し、[PSPSDK-LICENSE](third_party/PSPSDK-LICENSE) に保存しています。VitaSDK と taiHEN はビルドツールとプラットフォームインターフェースを提供し、Apollo Vita は設計調査の参考になりました。利用範囲と参照バージョンは第三者通知に記載しています。VitaShell のビルド済みバイナリは同梱していません。
