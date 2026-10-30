# Runtime source scope and building

[日本語](#日本語)

This repository publishes **Event Loader** and **HUD Lifecycle Fix** source code. It does not publish NFRR, World2MW/W2C, Editor, Cutscene Converter or ArmsAssist source code. Download the playable binaries from [Releases](https://github.com/Zakkey250/MW-Event-Loader/releases).

## HUD Lifecycle Fix

The HUD module can be built from this repository alone using Windows, Visual Studio 2022 or newer with the C++ desktop workload, **MSVC v143 x86/x64 tools**, and a Windows 10/11 SDK. Its MinHook dependency and license are included.

```powershell
.\tools\Build-Source.ps1 -HudOnly -RunTests
```

The output is `NFSMWHudLifecycleFix/bin/NFSMWHudLifecycleFix.asi`, a 32-bit Release build with the static C++ runtime. `-RunTests` also runs the HUD native harness. The script finds MSBuild using `vswhere`; `-MSBuildPath` overrides it. It does not install dependencies, launch the game or deploy files.

## Event Loader

**The public repository alone cannot compile the complete Event Loader at present.** The current source still references external shared APIs, SDK dependencies and private geometry/NeoAI headers. Those projects have deliberately been removed from this repository rather than republished under different names.

- NFRR is published separately: [MW Native Free Roam Racer](https://github.com/Zakkey250/MW-NativeFreeRoamRacer). Its source dependencies and shared interfaces are not duplicated here.
- Drift integration additionally references `MW-Arcade-Drift-public/src/DriftModeAPI.h` from the separate [MW Arcade Drift](https://github.com/Zakkey250/MW-Arcade-Drift) project. Keep that external checkout outside this repository's tracked files.
- World2MW/W2C source publication has not been authorized. Its geometry/NeoAI headers are not included or downloaded by the build script. Do not upload them to this repository.
- A developer who already has authorized access to compatible external source trees can place them in the ignored `NFSMWNativeFreeRoamRacers/` and `World2MW/` directories to satisfy the current relative includes. These directory names describe local build requirements, not publicly bundled source.
- Full compilation also requires Microsoft's [Microsoft.DXSDK.D3DX 9.29.952.8](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX/9.29.952.8) package. Review and accept its license and extract it to `external/d3dx/`, or pass its extracted root through `-D3DXRoot`. Microsoft SDK headers, libraries and DLLs are not committed here.

With those external dependencies already available:

```powershell
.\tools\Build-Source.ps1 -D3DXRoot 'C:\Dependencies\Microsoft.DXSDK.D3DX.9.29.952.8' -RunTests
```

The Event Loader output is `NFSMWEventLoader/build/runtime/MWEventLoader.asi`, 32-bit Release, with `TorchRuntime=true`. Full `-RunTests` additionally runs seven Event Loader policy suites, including drift scoring and driving. Missing external projects produce an explanatory error. Native tests do not substitute for in-game validation.

## Published layout

- `NFSMWEventLoader/src/`: Event Loader runtime, policies, UI assets and native tests.
- `NFSMWEventLoader/tools/CarbonIntro/`: runtime playback code included by `CarbonIntro.cpp`. It reads already-converted scenes and is not the Cutscene Converter implementation.
- `NFSMWHudLifecycleFix/`: HUD runtime, configuration, native tests and MinHook with its license.
- `integration/`: shared interfaces used by these two runtimes; no ArmsAssist implementation.
- `docs/Source-Snapshot.json`: fingerprints of the source files that remain published. The portable D3DX reference changes are recorded there.

Diagnostic logging defaults to OFF. Game data, converted scenes and sample events are not included in the source tree. Project code uses the root MIT license; third-party code retains its own license.

## 日本語

公開対象は **Event LoaderとHUD Lifecycle Fix** です。NFRR・World2MW/W2C・エディター・カットシーンコンバーター・ArmsAssistのソースは含めていません。NFRRは[専用リポジトリ](https://github.com/Zakkey250/MW-NativeFreeRoamRacer)から別途参照してください。W2C関連コードの公開は未承認のため、本リポジトリには配置しません。

HUD Lifecycle Fixは、このリポジトリだけでビルドできます。Visual StudioのC++デスクトップ開発環境、MSVC v143（x86/x64）、Windows SDKを用意し、`tools/Build-Source.ps1 -HudOnly -RunTests` を実行してください。必要なMinHookとライセンスは同梱しています。

**Event Loader全体は、現時点では公開リポジトリ単体でビルドできません。** 現ソースが外部の共通API・SDK・非公開の地形／NeoAIヘッダーを参照するためです。利用権限のある開発者が互換性のある外部ソースを用意した場合のみ、従来の相対パスでビルドできます。外部プロジェクト用のディレクトリはGitの除外対象です。ビルドスクリプトが未公開コードを取得することはありません。必要な外部ファイルが無ければ、その理由を表示して停止します。

ドリフト連携には、別公開のMW Arcade Driftにある `MW-Arcade-Drift-public/src/DriftModeAPI.h` も参照します。その外部ソースを本リポジトリの追跡対象へ追加しないでください。

`tools/CarbonIntro/` は変換済みシーンの再生コードで、抽出・変換処理ではありません。ゲームの生データや変換済みカットシーンは同梱しません。診断ログは標準OFFです。ビルドスクリプトはゲーム起動・ゲームへの配備を行いません。
