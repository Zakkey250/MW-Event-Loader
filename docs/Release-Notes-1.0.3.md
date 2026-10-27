# MW Event Loader 1.0.3

Changes since **1.0.2 Hotfix**.

## English

- **Arms Race: last racer standing wins.** If you are still alive when every AI rival is wrecked, the race ends with a victory without requiring a finish-line crossing. This applies to Sprint, Circuit, Lap Knockout and Speedtrap variants. Ordinary knockout eliminations or temporarily missing cars do not count as wrecks. If your car is wrecked too, the normal defeat rules apply.
- **More accurate starts in original events.** Additional cutscenes now use the event's actual start marker for the grid, starter and crowd. This corrects the offset seen between an untouched original event and the same event exported without edits, while retaining the MW camera adaptation.
- **Improved cutscene handover between events.** Vehicle control is reacquired and released more carefully when starting another event from free roam. If a selected scene cannot be placed safely on the road, the original introduction is retained.
- **UG2 starting gestures match GO.** Corrected the late GO in the two-woman `UG2_INTRONIS23` scene and the early GO in the radio-starter `UG2_INTRONIS28` scene. Existing standard conversions are recognized by their motion data and corrected during playback. Custom timing and modified scene data are left unchanged.
- **Speedtrap standings show all the useful information.** Armed Speedtrap events cycle between driver names, HP percentages and total speed, so the score used to decide the result remains visible.
- **Arms Race result/retry handling improved.** Survival wins are recorded separately in additional-event progress, and restarting clears the previous survival-victory state.

### Updating

Close the game and launcher, back up your installation, then replace the ASI files from **MW-Event-Loader-v1.0.3.zip**. Keep your existing INIs, events, `Profiles` and converted cutscenes. **No event re-export, cutscene reconversion or converter update is required.** ArmsAssist and HUD Lifecycle Fix are included unchanged from 1.0.2 Hotfix. NFRR/LB, MWArsenal and the ASI loader remain separate downloads.

This package contains no source-game assets, converted cutscenes, event packs or installer. The automatic GitHub source archives are not the playable MOD.

The intermittent report of a missing vehicle or skipped introduction is not claimed to be eliminated in every setup. If it persists, report the event, game setup and the sequence of events before it happened.

---

## 日本語

**1.0.2 Hotfixからの変更点です。**

- **アームズレースに生存勝利を追加。** プレイヤーが生存したままAIレーサー全車が大破すると、ゴールまで走らずに勝利で終了します。スプリント・サーキット・ラップKO・スピードトラップに対応します。通常のKO脱落や、一時的に見えなくなった車両は大破に数えません。プレイヤーも大破した場合は、通常の敗北判定になります。
- **純正イベントのスタート配置を修正。** 追加カットシーンで使用するグリッド・合図役・群衆を、イベント本来の開始位置に合わせます。無編集の純正イベントと、ノータッチで再出力したイベントの位置ずれを修正し、MW向けのカメラ調整は維持しています。
- **イベント間のカットシーン切り替えを改善。** 完走後、フリーロームから次のイベントを始める際の車両制御を見直しました。選ばれた演出を道路上へ安全に配置できない場合は、純正演出を維持します。
- **UG2の合図とGOのタイミングを修正。** 二人組の`UG2_INTRONIS23`でGOが遅れる問題と、無線を持つ合図役の`UG2_INTRONIS28`でGOが早い問題に対応しました。標準の変換済みデータをモーションの内容で識別し、再生時に補正します。独自のタイミングや変更済みシーンには適用しません。
- **スピードトラップの順位表示を改善。** 武装スピードトラップでは、ドライバー名・HP％・合計速度を順番に表示します。順位を決める合計速度も確認できます。
- **アームズレースの結果・リトライ処理を改善。** 追加イベントの進捗に生存勝利を区別して記録し、リトライ時に前回の生存勝利状態を引き継がないようにしました。

### 更新方法

ゲームとランチャーを終了し、バックアップ後に **MW-Event-Loader-v1.0.3.zip** のASIを差し替えてください。既存のINI・イベント・`Profiles`・変換済みカットシーンは保存してください。**イベントの再書き出し、カットシーンの再変換、コンバーターの更新は不要です。** ArmsAssistとHUD Lifecycle Fixは1.0.2 Hotfixから変更していません。NFRR/LB・MWArsenal・ASIローダーは引き続き別途導入してください。

元ゲームのデータ、変換済みカットシーン、イベントパック、インストーラーは同梱しません。GitHubの自動生成ソースアーカイブはゲーム用MODではありません。

稀な車両欠落・演出スキップについては、すべての環境での完全解消を保証するものではありません。再発した場合は、イベント名・構成・直前のイベント移動手順を添えてご報告ください。
