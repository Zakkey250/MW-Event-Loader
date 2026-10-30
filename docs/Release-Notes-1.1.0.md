# MW Event Loader 1.1.0

Changes since **1.0.3**.

## English

- **Added Drift races.** Create a Drift Sprint or Circuit in the Web Event Editor. Finish with the highest drift-point total to win. Police are always disabled in this mode.
- **MW Arcade Drift 0.2.0 support.** Install its Core package separately, including both MWArcadeDrift and MWArcadeDriftAI. Drift events automatically select Special mode for the player and use drift assistance for NeoAI opponents. Your previous player mode is restored after leaving the event.
- **Drift scoring and combos.** Points reward moving drifts, with a multiplier that increases by one level for every two seconds of qualifying drifting, up to x10. Close passes beside walls or other vehicles add bonuses. A heavy wall hit loses the current unbanked chain; points already banked remain safe.
- **Dedicated Drift HUD and event icon.** The top-left HUD shows the active chain, multiplier, angle and bonuses, with the total at the lower left and banking notices on the right. It slides down while the EA TRAX banner is visible and returns afterward.
- **Point-based splits and results.** Checkpoint split messages show “SPLIT POINTS” and a signed points difference. Standings, victory displays and results show drift points instead of race times or speeds.
- **Results without waiting for every car.** When the player finishes, unfinished opponents receive an estimate for the remaining course based on their driving so far. Their cars are not fast-forwarded. Estimated totals are marked with “~” in the results.
- **NeoAI drift driving.** Added corner entry, sustained slides and transitions between bends, with improved speed control, use of road width and recovery after contact. Results still depend on the car and course.
- **Wall-hit scoring recovery.** Corrected a case where drifting stopped earning points after a wall hit, even though the previous total remained visible.
- **Six-car cutscene starts.** Corrected compatible six-car introductions being skipped on the first start but playing on retry in edited events, including Drift races.

### Updating and using Drift

Close the game and launcher, then extract **MW-Event-Loader-v1.1.0.zip** and copy its `scripts` folder into the game folder containing `speed.exe`. Back up your installation first. Replace the ASIs while preserving your existing INIs, events, `Profiles` and converted cutscenes. ArmsAssist and HUD Lifecycle Fix are unchanged from 1.0.3.

For Drift races, install [MW Arcade Drift 0.2.0 — Core](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/v0.2.0) separately and keep both included ASIs together. Its optional Editor provides per-car Special-mode settings and the Pad/Keyboard or Wheel control choice. Create or edit a Sprint/Circuit in the [Web Event Editor](https://zakkey250.github.io/MW-Event-Editor/), select Drift, and export the event. Existing events only need re-exporting if you want to change their settings or turn them into Drift races.

NFRR/LB and an x86 ASI loader remain separate requirements. MWArsenal and its assets are needed for weapon modes, not for Drift alone. **No Cutscene Converter update or cutscene reconversion is required.** No event packs, converted cutscenes, source-game assets, third-party MODs or installer are included.

### Known limitations

Drift performance varies with cars, tuning and route design. Tight corners, close traffic and recovery situations can still challenge NeoAI. Estimated opponent totals are predictions rather than scores from a completed run. Large fields or resource-heavy cars can still cause frame drops or incomplete vehicle rendering; this update does not claim to eliminate every such case.

---

## 日本語

**1.0.3からの変更点です。**

- **ドリフトレースを追加。** Webイベントエディタで、スプリント／サーキットをドリフト種目にできます。ゴール時のドリフトポイントで勝敗を決め、警察は常に無効になります。
- **MW Arcade Drift 0.2.0と連携。** CoreパッケージのMWArcadeDrift・MWArcadeDriftAIを別途導入してください。ドリフトイベント中はプレイヤーをスペシャルモードへ自動切り替えし、NeoAIの相手車両にもドリフト補助を適用します。イベントを離れると、プレイヤーの元のモードへ戻ります。
- **ドリフト得点とコンボ倍率。** 走行中のドリフトに応じて加点し、有効なドリフトの継続2秒ごとに倍率が1段階上がります。最大は10倍です。壁や他車への接近ボーナスも加わります。強い壁ヒットでは未確定のコンボを失いますが、確定済みの合計ポイントは残ります。
- **専用HUDとイベントアイコン。** 左上のHUDで加算中ポイント・倍率・角度・ボーナスを確認できます。合計は左下、得点確定の通知は右側に表示します。EA TRAXの曲名表示中はHUDが下へスライドし、表示終了後に戻ります。
- **区間差・順位・結果をポイント表示へ。** CP通過時には「スプリットポイント」と符号付きのポイント差を表示します。順位表、勝利画面、リザルトもドリフトポイントで確認できます。
- **全車のゴールを待たずに結果を確定。** プレイヤーのゴール時に、未完走の相手の走行実績から残り区間の得点を予測します。車両を早送りで走らせることはありません。予測を含む合計には、リザルトで「~」を付けます。
- **NeoAIのドリフト走行に対応。** コーナーへの進入、パワースライド、連続するカーブでの切り返しに対応しました。速度維持、道幅の使い方、接触後の立て直しも調整しています。走りや得点は車両とコースによって変わります。
- **壁ヒット後の加点停止を修正。** 合計ポイントは残る一方、その後のドリフトで加点されなくなる場合がある問題に対応しました。
- **6台用カットシーンの初回再生を修正。** ドリフトを含む編集イベントで、対応する6台用演出が初回に再生されず、リトライ時だけ再生される問題を修正しました。

### 更新方法・ドリフトの始め方

ゲームとランチャーを終了し、バックアップ後に **MW-Event-Loader-v1.1.0.zip** を展開してください。中の `scripts` フォルダーを `speed.exe` のあるゲームフォルダーへコピーします。ASIを差し替え、既存のINI・イベント・`Profiles`・変換済みカットシーンは保持してください。ArmsAssistとHUD Lifecycle Fixは1.0.3から変更していません。

ドリフト種目には、[MW Arcade Drift 0.2.0のCore](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/v0.2.0)を別途導入し、同梱のASI 2本を両方配置してください。任意の専用Editorでは、車種別のスペシャル設定とPad/Keyboard・Wheelの操作設定を調整できます。[Webイベントエディタ](https://zakkey250.github.io/MW-Event-Editor/)でスプリント／サーキットを作成・編集し、ドリフトを選んで書き出すと遊べます。既存イベントは、設定変更やドリフト種目への変更を行う場合だけ再書き出ししてください。

NFRR/LB・32bit ASIローダーは引き続き別途必要です。MWArsenal本体とアセットは武器種目用で、ドリフトだけなら不要です。**カットシーンコンバーターの更新・カットシーンの再変換は不要です。** イベントパック、変換済みカットシーン、元ゲームの資源、他のMOD、インストーラーは同梱しません。

### 既知の制限

ドリフトの走りと得点は、車両・チューニング・コースの形状で変わります。狭い折り返し、密集した車両、復帰が必要な場面では、NeoAIが苦戦することがあります。相手の予測得点は実際に完走して得た点数とは異なります。多台数や高負荷な車両による処理落ち・車体の一部欠落について、すべてのケースでの解消を保証する更新ではありません。
