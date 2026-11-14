# MW Event Loader 1.2.0

Changes since 1.1.0.

## English

- **RPD launch on W2C.** Fixed a transition that could leave Interceptor or RPD Hot Pursuit on the loading screen. Re-exported RC4 events now remain eligible for the Blacklist event list.

- **Downhill Battle.** Create a W2C Sprint or Circuit with downhill routes and horizontal fall triggers. NeoAI route guidance is always enabled. Police are disabled.
- **W2C to MW v2 RC4 support.** The loader recognizes events authored for the public RC4 world and compatible Rockport events on its merged road network. Native and editor-placed barriers use the installed `GLOBAL/InGameA.bun`; reinstall HQ Track Barriers after W2C to restore its blue assets.
- **RPD Case Files (prototype).** Interceptor is a one-suspect free-roam event. RPD Hot Pursuit is a sprint with up to eight suspects. Arrests and takedowns count toward a bounty award; racers who reach the finish escape. Available vehicles depend on the installed police roster and event rules.
- **RPD presentation.** Added police-side introductions, rolling starts, original intro jingles, arrest and takedown cameras, bounty displays and pursuit notifications. Normal arrests in RPD Hot Pursuit use a close-range hold. Compatible HUD mods can show the target's health and arrest progress above the car.
- **Combat feedback and stability.** Added hit, jam, weapon damage, dispatch and takedown notifications. Updated pursuit and weapon handling to reduce repeated work during heavy pursuits, and fixed several retry, recovery and event-transition cases.
- **Event Editor.** Detects W2C when updating a linked Rockport configuration, offers one-click migration, and outlines checkpoint warnings in red with white text. Untouched events with warnings stay out of bulk export.

### Installation

Close the game and launcher. Copy the release ZIP's `scripts` folder into the folder containing `speed.exe`. Keep existing INIs, `Profiles`, custom events, and any independently converted scenes. Use the INIs in the ZIP as references for new settings. The package contains Event Loader, ArmsAssist, HUD Lifecycle Fix, the supported HP-style scene playback files and their accompanying effects. Install [NFRR 0.1.4 with Limit Breaker](https://github.com/Zakkey250/MW-NativeFreeRoamRacer/releases/tag/v0.1.4) and a compatible [32-bit Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/tag/v10.0.2) separately.

Weapon modes and RPD need [MWArsenal and its assets](https://www.nfsaddons.com/downloads/nfsmw/tools/11267/mw-arsenal.html). Drift needs both ASIs in [MW Arcade Drift 0.2.1 Core](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/v0.2.1). Downhill Battle requires [W2C to MW v2 RC4](https://github.com/Zakkey250/W2C-to-MW-Converter/releases/tag/v2.0.0-rc4). EA TRAX Expansion and supported Healthbars are optional. Create or export an event for the receiving installation in the [Event Editor](https://zakkey250.github.io/MW-Event-Editor/). RPD events appear in the Safe House Blacklist menu at their assigned rank once installed.

## 日本語

- **W2CでのRPD開始を修正。** インターセプターとホットパースートRPDでロード画面が残る問題を修正しました。公開RC4向けに移行して再出力したイベントもブラックリストに表示されます。

**1.1.0からの変更点です。**

- **ダウンヒルバトルを追加。** W2Cのスプリント／サーキットで、下りのコースや水平の転落判定ゲートを設定できます。NeoAI経路誘導が常に有効になり、警察は無効になります。
- **W2C to MW v2 RC4に対応。** 公開RC4用に書き出したイベントと、統合道路網で互換性のある純正ロックポート用イベントを読み込めます。純正配置とエディタ追加のバリアは、導入済みの `GLOBAL/InGameA.bun` を描画に使います。青いバリアを使う場合はW2C導入後にHQ Track Barriersを再導入してください。
- **RPD CASE FILES（試作）。** インターセプターは容疑車1台を追うフリーランです。ホットパースートRPDは最大8台を追うスプリントで、逮捕・テイクダウン数に応じてバウンティが決まり、ゴールしたレーサーは逃走扱いになります。出撃車両は導入済みの車種とイベントの条件に応じて選べます。
- **RPDの演出とHUDを追加。** 警察側の開始シーン、ランニングスタート、オリジナルジングル、逮捕・テイクダウン時のカメラ、バウンティ表示を追加しました。ホットパースートRPDの通常逮捕は対象に近接して確保します。対応HUDではレーサーのHPと逮捕ゲージを車両上に表示できます。
- **戦闘通知と安定性を改善。** 被弾、命中、妨害、警察の配備、テイクダウンをカットインで通知します。混戦時の追跡・武器処理の重複を抑え、リトライ、車両の復帰、イベント間の遷移を修正しました。
- **イベントエディタ。** 純正構成の更新時にW2Cを検出すると、ワンクリック移行を案内します。チェックポイントの警告は一覧で白字・赤枠に表示し、未編集の警告イベントは一括出力から除外します。

### 更新方法

ゲームとランチャーを終了し、ZIPの `scripts` を `speed.exe` のあるフォルダーへコピーします。既存のINI、`Profiles`、カスタムイベント、個別に変換したシーンは保持してください。ZIPのINIは追加設定の確認用に使えます。同梱物はEvent Loader、ArmsAssist、HUD Lifecycle Fix、対応するHP再現シーンの再生用ファイルと付随する演出資源です。[NFRR 0.1.4と同梱のLimit Breaker](https://github.com/Zakkey250/MW-NativeFreeRoamRacer/releases/tag/v0.1.4)、対応する[32bit Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/tag/v10.0.2)を別途導入してください。

武器種目・RPDには[MWArsenal本体とアセット](https://www.nfsaddons.com/downloads/nfsmw/tools/11267/mw-arsenal.html)、ドリフトには[MW Arcade Drift 0.2.1 Core](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/v0.2.1)のASI 2本、ダウンヒルバトルには[W2C to MW v2 RC4](https://github.com/Zakkey250/W2C-to-MW-Converter/releases/tag/v2.0.0-rc4)が必要です。EA TRAX Expansion・対応Healthbarsは任意です。[イベントエディタ](https://zakkey250.github.io/MW-Event-Editor/)で使用先の構成からイベントを書き出してください。RPDは導入後、設定したBL順位のセーフハウス内ブラックリストに表示されます。
