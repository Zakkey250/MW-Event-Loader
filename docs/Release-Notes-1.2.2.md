# MW Event Loader 1.2.2

## 日本語

- イベント開始時の初期化を見直し、追跡や武器イベントの開始直後に集中していた処理を減らしました。レーサー側・警察側の両方に適用されます。
- W2C to MW 2.0.0 RC6と道路Addonに対応しました。道路が追加・変更されても、使用する道路や設置地点に影響がない既存イベントは引き続き読み込めます。
- イベント後に追跡を引き継いだ状態で逮捕されたり、セーフハウスへ移動したりするとゲームが落ちる問題を修正しました。
- イベントを連続して遊んだ際、一部のスピードトラップでコース上とミニマップのオービスマーカーが消える問題を修正しました。
- バリアの差し替え素材を導入している場合は、純正イベントに設置済みのバリアと追加配置したバリアの両方に適用されます。
- ライバル戦をドラッグからダウンヒルバトルへ変更したイベントで、走行中にゲームが停止する場合がある問題を修正しました。
- 同梱のArmsAssistを更新しました。対応するMWArsenalとDX11描画を併用したとき、ゲーム終了時にクラッシュする問題を修正しました。

更新時はゲームを終了し、ZIP内の3つのASIを上書きしてください。既存のINI、プロフィール、追加イベント、変換済みカットシーンは引き続き使用できます。

## English

- Reduced the work done when an event starts by preparing pursuit and weapon systems earlier. This applies whether you play as a racer or a cop.
- Added support for W2C to MW 2.0.0 RC6 and its road add-ons. Existing events still load after roads are added or changed if the roads and placement locations they use are unaffected.
- Fixed a crash when getting busted or returning to a safehouse after a pursuit carries over from an event.
- Fixed speed camera markers disappearing from the course and minimap in some Speedtrap events after playing multiple events in a row.
- Installed replacement barrier textures now apply to both barriers already placed in stock events and newly placed barriers.
- Fixed a crash that could occur when driving a rival race converted from Drag to Downhill Battle.
- Updated the bundled ArmsAssist to fix an exit crash when using a supported MWArsenal build with DX11 rendering.

Close the game before replacing the three ASI files in the ZIP. You can keep your existing INIs, profiles, custom events and converted cutscenes.
