# MW Cutscene Converter 1.0.0

Convert ordinary race-start introductions from Carbon or Underground 2 locally for MW Event Loader. Either source game works on its own. The source game's language and executable are not used to decide compatibility.

## Requirements

- 64-bit Windows 10 or 11. The converter is 64-bit; MW itself remains the supported 32-bit game.
- Supported Carbon or Underground 2 resource files.
- A compatible MW Event Loader to play the results.
- At least 50 GiB free on the output drive after the operation.

You do not need to install Python or audio decoding tools yourself. Extract the entire application folder; do not move the EXE out on its own. Before the first conversion, the app shows the required readers, their sources and installation folder. They are installed automatically only after you approve. No login or update checks are used.

Internet access is needed only for the first approved dependency installation. The app downloads pinned versions from official GitHub and Microsoft sources (up to about 300 MB), verifies checksums and builds the readers locally. Temporary space is up to 3 GB. Readers and the build-only .NET SDK are handled under `%LOCALAPPDATA%/MWCutsceneConverter/dependencies/`, without administrator rights or system configuration changes. After installation, conversion works offline. You may decline and still open the app and check source files, but conversion remains disabled.

## Quick start

1. Extract the application outside your game folder and run `MWCutsceneConverter.exe`.
2. Choose one of 23 interface languages, or System default. The choice is remembered. It does not change which source files are accepted.
3. Select the Carbon and/or UG2 folder containing `NIS` and `GLOBAL`. One game is enough.
4. Choose an output folder separate from the source games, then select Check files.
5. Select scenes and convert them. Source game files are read-only. Identical existing output is reused; differing output is never silently overwritten.
6. Select Create install ZIP if wanted. Only verified runtime files listed in the manifests are included.
7. Close MW and its launcher, then merge the generated `scripts/EventLoader/CustomCutscenes/` into the same location in the game. If using the ZIP, extract it and merge its `scripts` folder.
8. Update your game configuration in the editor. Choose a matching racer-count scene or automatic selection, then test the introduction, GO, skipping and retrying in the game.

Changing a source folder clears the previous scene selection; run the file check again. Cancel after scene finishes the current scene and keeps completed output. Closing while work is active also waits for safe cleanup.

## Converted content

Supports 8 Carbon and 22 UG2 scenes, covering ordinary race introductions rather than story-only sequences. Converts characters, props, car staging, cameras and start cues, plus supported crowds, torches and smoke flares. Lighting and particle appearance are not exact reproductions of the original games.

Engine revs use the cars actually loaded on the grid. UG2 car/time commands are used where present; otherwise a staggered pattern is assigned. RPM envelopes are approximations for MW. No fixed engine WAV is needed.

### Source files

Carbon uses the supported `NIS/Scene_*_BundleB.bun` files and `GLOBAL/InGameB.bun`. Torch scenes also need `attributes.bin`, `gameplay.bin` and the BUN containing the particle textures.

UG2 uses supported `NIS/Scene_IntroNis*_BundleB.bun` files and their cameras in `GLOBAL/InGameRace.bun` or the other supported camera banks. Flare scenes also use `GLOBAL/GLOBALB.BUN` and `GLOBAL/InGameCommon.bun`.

Names are case-insensitive. A resource-only folder retaining the required structure is sufficient. Game EXEs, registry entries, installation folder names and language-specific files are not required. Missing or unsupported files are reported per game/scene; usable scenes can still be processed.

## Audio

Matching scene audio is detected automatically by default. To choose a sound manually, select an output folder, select one scene and use Assign audio. Choose a WAV or original audio bank. Banks require an explicit subsong number starting at 1. Your assignment is saved automatically in the output folder; you do not need to create or select a JSON mapping file.

A single exact-match WAV in `NIS/<scene name>.wav`, `NIS/Scene_<scene name>.wav` or `SOUND/NIS/<scene name>.wav` is discovered automatically. Ambiguous matches need explicit mapping. The converter does not guess localized dialogue or substitute unrelated sounds.

WAV input must be 16-bit PCM, mono/stereo, 8–96 kHz and no longer than 60 seconds. Original banks use the included decoder. Output is one checked PCM track per scene, with timing, length and volume validation. If a fixed SFX source cannot be identified, the report records that absence; this does not itself mean the scene failed.

## Output and replacing scenes

```
Output/
  scripts/EventLoader/CustomCutscenes/<scene ID>/  Copy these files to MW
  manifests/<scene ID>.json                      Output validation record
  conversion-report.json                        Per-scene results
  MW-Cutscenes-Local.zip                         Optional installation ZIP
```

Do not partially merge a new scene into an old one: obsolete effects or audio could remain. Move the old scene folder outside the game first, then install the complete replacement. Use a new output folder when generating different results for an existing scene.

## Command line

```
MWCutsceneConverterCLI.exe --version
MWCutsceneConverterCLI.exe --install-dependencies --accept-downloads
MWCutsceneConverterCLI.exe --language en --carbon "C:\Games\Carbon" --list
MWCutsceneConverterCLI.exe --ug2 "C:\Games\Underground 2" --output "D:\ConvertedScenes" --zip
MWCutsceneConverterCLI.exe --carbon "C:\Games\Carbon" --output "D:\Scenes" --scene INTRO05NG02 --zip
```

Repeat `--scene` for multiple scenes. Use `--audio-map` for explicit audio bindings. `--language` / `--lang` controls interface and operation messages, not the source game language. `--list` JSON keys remain language-independent. Exit codes: success 0, conversion failure 1, argument error 2. Low-level parsing details may be in English.

## Data and rights

The application includes no source-game or converted scene assets. Generated scenes and installation ZIPs are for use in your own game. Do not publish or redistribute game-derived converted cutscenes, characters, textures, animation or audio. See `THIRD_PARTY_NOTICES.md` and `licenses` for software notices.

Conversion validation is separate from in-game playback acceptance. Check the required files and racer count, then test event starts and exits in the game.
