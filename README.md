# MW Event Loader

[日本語](README.ja.md) · [Event Editor](https://zakkey250.github.io/MW-Event-Editor/) · [All releases](https://github.com/Zakkey250/MW-Event-Loader/releases)


> **1.0.3:** Adds last-survivor Arms Race victories and fixes native-event start placement and UG2 GO gestures. Existing cutscenes need no reconversion. [Release notes](docs/Release-Notes-1.0.3.md)

| Download | Contents |
| --- | --- |
| [MW Event Loader 1.0.3](https://github.com/Zakkey250/MW-Event-Loader/releases/tag/v1.0.3) | Event Loader + bonus ArmsAssist + HUD Lifecycle Fix |
| [MW Cutscene Converter 1.0.0](https://github.com/Zakkey250/MW-Event-Loader/releases/tag/cutscene-converter-v1.0.0) | Standalone Windows x64 tool; approve reader installation on first conversion |

Download the named ZIP under **Assets**. GitHub’s automatic “Source code” archives are not the playable MOD. Install NFRR/LB and an x86 ASI loader separately; weapon modes additionally need MWArsenal and its assets.


INI settings: [English](docs/INI-Settings-EN.txt) · [日本語](docs/INI-Settings-JA.txt)

## Overview

“You’ve built a world of your own, but the race events are still the same? Then make your own! No risk!”

Bring your own races to your favorite city. MW Event Loader adds custom events created in your browser to Need for Speed: Most Wanted (2005). Duplicate and adjust an existing course, or create a new race with your own checkpoints and starting positions. Additional game modes introduce weapons and vehicle health for new ways to race on familiar roads.

Install events as additional files without directly rewriting the original game data. “No risk” describes that approach; it is not a guarantee against bugs or save corruption. Back up your game and saves before installing.

## Features

### 1. MW Event Loader

- Add custom events or edit existing races. Set event names, routes, checkpoints, start and finish positions, racer counts, rewards, traffic, police and starting Heat. Available settings depend on the event type.
- When editing a story event, its dedicated markers and other information needed for career progression are preserved.
- Choose NeoAI for custom routes, or keep the original AI for stock courses. After creating a custom route, check how the AI drives it in the game.
- Automatically generate replacement preview and radar maps. The preview draws your route over the full world map and zooms to fit the course.
- Enable checkpoint display to show course markers and a yellow radar marker for the next checkpoint.
- Automatic opponent selection chooses suitable rivals from the cars installed in your game, with performance adjustments that follow career progression.
- Use race-start cutscenes converted locally from Carbon or Underground 2, or your own scenes in the supported format. Select an introduction matching your racer count, or use random selection.
- Additional event progress is stored in scripts/EventLoader/Profiles. Keep this folder when updating. You can use an existing save, but back it up before installation.
- Share events between installations using the same world, even with different cars, tuning, languages or visual mods. Events cannot be shared between worlds with different road geometry, such as stock Rockport and W2C.

### 2. MW Arms Assist (Bonus Mod)

- Extends your separately installed MWArsenal with police and AI racer weapon use, spike deployment, tire repair and support for the additional game modes.
- Select a weapon-enabled mode in the editor. Turning police on does not automatically create an Arms Race. In normal events, player and police weapons are disabled and the weapon HUD is hidden.
- Starting ammunition uses the editor’s settings first, with the ArmsAssist INI used for unspecified weapons. Missing charges are topped up at the start and on retry. Your equipped weapons remain controlled by the INI.
- A scrolling, fading combat log displays up to three lines above the MWArsenal HUD. Racers use their driver names; police are identified by role.
- Lock-on warnings and notifications about your own weapon use and hits also appear in the game’s standard message area. Combat logs and additional messages follow the game language, or you can choose a language in the ArmsAssist INI.
- MWArsenal and its assets must be installed separately. ArmsAssist source code is not publicly available.

### 3. NFSMWHudLifecycleFix (HUD Lifecycle Fix)

- Adds the player health bar for the additional modes, AI racer health displays through supported Healthbars versions, and a star marker for Gauntlet’s invincible leader.
- Healthbars is optional. The player health bar works without it; overhead display integration requires a supported Healthbars version.
- Supports use alongside existing HUD mods such as Widescreen Fix, NFSMWHUDAdapter and CustomHUD, with limitations for some combinations. See Known Issues & Limitations for Redux’s older CustomHUD and Healthbars.

## Requirements & Recommended Mods

You need the PC version of MW 2005, a supported speed.exe and a 32-bit ASI loader. Install NFRR and LB as well. Obtain these mods from their authors’ download pages.

### NFRR / MW Native Free Roam Racer (Required)

Used for opponent selection and NeoAI integration. Use a version compatible with Event Loader.
Author’s GitHub: <https://github.com/Zakkey250/MW-NativeFreeRoamRacer>

### LB / MW Limit Breaker (Required)

Included in the NFRR download. LB is optional for NFRR on its own, but should be installed for Event Loader’s larger race fields. It is not bundled again with Event Loader or event-data ZIPs.
NFRR download page (includes LB): <https://github.com/Zakkey250/MW-NativeFreeRoamRacer>

### MWArsenal (Required for ArmsAssist & Weapon Modes)

Install both MWArsenal and its assets. MWArsenal alone does not enable the three additional modes. You do not need the weapon components if you only want normal events.
There is no need to install MWArsenal’s separate string files; ArmsAssist supplies the required on-screen text.
Download by Shiftersc & LEE-RYO: <https://www.nfsaddons.com/downloads/nfsmw/tools/11267/mw-arsenal.html>

### MWHealthbars / NFSMW Cop Car Healthbars (Recommended / Optional)

Shows police durability and, with supported integration, overhead health bars for AI racers. There is a known issue when combined with Redux’s older CustomHUD.
trelbutate’s GitHub: <https://github.com/trelbutate/MWHealthbars>

### Ultimate ASI Loader (Required — x86 / 32-bit)

Use the 32-bit version. If your game already has a working ASI loader, you do not need to add another one.
Official releases by ThirteenAG: <https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases>

### DirectX End-User Runtimes (June 2010; If Needed)

If you receive a missing d3dx9_43.dll error, install Microsoft’s official runtime package.
Official Microsoft download: <https://www.microsoft.com/en-us/download/details.aspx?id=8109>

### LAA / 4GB Patch (Tested Configuration)

The tested configurations use a supported speed.exe with LAA enabled. If your executable already has LAA enabled, you do not need to apply it again. Game executables are not included.
NTCore 4GB Patch: <https://ntcore.com/4gb-patch/>

W2C events require the matching W2C world and its loader. Follow the event author’s instructions for any additional cars or other required mods.

## Tested Worlds & Executables

### Worlds

- Stock Rockport City.
- The custom-converted W2C Palmont + Rockport world. The world itself is not included.

### Executables

- MW English 1.3 speed.exe prepared with NFSPatcher, with LAA (4GB Patch).
- Redux 3.04 speed.exe with LAA.

“1.3” refers to the MW game version. A file named speed.exe may still be unsupported if its contents differ. You can retain non-English game resources, but must use a supported executable. Compatibility with every combination of mods is not guaranteed.

## Downloads & Package Contents

### Main Package

- MW Event Loader: MWEventLoader.asi, configuration and supporting assets.
- MW Arms Assist: MWArmsAssist.asi, configuration, dedicated HUD assets and replacement SFX.
- NFSMWHudLifecycleFix: ASI and configuration.
- Documentation, terms of use and third-party credits.

### GitHub Downloads

- The main package.
- MW Cutscene Converter.

The main package and converter are separate downloads. Sample events are not included in this release. The editor runs in your browser. Event-data ZIPs contain no ASIs or installer. Install NFRR, LB, MWArsenal, Healthbars and your ASI loader separately as needed. Game executables, worlds and converted cutscenes are not included.

## Update notifications

At startup, Event Loader checks the published Event Loader versions on this GitHub repository. A newer eligible version opens the shared self-authored MOD notification, with an optional button to open the fixed GitHub releases page. Cutscene Converter releases are ignored. Notifications are suppressed during driving; failed or offline checks are silent. No files are downloaded or installed automatically.

To disable this check, set `[Updates] Enabled=0` in `scripts/EventLoader/EventLoader.ini`. `Language=auto`, `ja` or `en` selects the notification language (non-Japanese languages use English). Keep your own INI and `Profiles` when updating.

## Installation 1: Main Package

- Close the game and launcher, then back up your settings, saves and existing events.
- Check your executable and required mods. Install MWArsenal and its assets if you want to use the weapon modes.
- Extract the main archive, then drag its scripts folder into the game folder containing speed.exe and merge the folders. No dedicated installer is needed.
- When updating, keep your customized INIs, EventLoader/Profiles and installed CustomEvents and CustomCutscenes folders. Avoid replacing all your settings with the defaults.
- Keep only one copy of each ASI. Move any older duplicate copies in the game root or scripts folder outside the game directory.
- Launch the game and check the event list. Try a normal event first, including finishing, retrying and returning to free roam.

## Installation 2: Additional Cutscenes

Convert supported Carbon or Underground 2 files locally on your PC. You only need one of the two games. The converter checks the required resource files without relying on the source game’s language, registry entries, folder name or executable. Missing files and unsupported formats are reported during the file check.

- Extract MW Cutscene Converter outside the game folder and run MWCutsceneConverter.exe. No separate Python or .NET installation is needed.
- Select the Carbon or UG2 folder containing NIS, GLOBAL and the other required files. Files from either game alone are sufficient for its supported scenes.
- Choose an output folder separate from the source game, then run the file check. Review the available scenes and their supported racer counts.
- Select the scenes you want and start conversion. Supported content includes characters, car staging, cameras, crowds, torches and smoke flares.
- Engine revs use the sounds of the cars actually loaded on the starting grid. Other SFX can be used when a matching WAV or audio bank is available for the scene. You can assign audio manually where needed. A scene can still be converted when no matching sound is available.
- Close the game and launcher, then copy the generated scripts/EventLoader/CustomCutscenes/<scene ID>/ folder to the same location in your game. If you created an installation ZIP, extract it and merge its scripts folder into the game folder.
- Update your game configuration in the editor. Select a scene matching the racer count, or choose automatic selection, then check the introduction, GO, skipping and retrying in the game.

With custom cutscenes enabled, compatible stock races can also use random introductions. Rival races, story events and events with dedicated introductions are excluded. Gauntlet uses the original Tollbooth introduction. If no compatible custom scene is available, the original introduction is used.

You can also add your own scenes in the supported format; simply placing a video file in the folder will not work. Converted visuals, sound and particles may differ from the original game. Scenes converted from game files, including their installation ZIPs, are for personal use only and must not be uploaded or redistributed.

## Installation 3: Replacing ArmsAssist Sounds

The included replacement SFX use filtered audio. To replace sounds for your own use, close the game, back up the original WAV files and replace them using the same filenames.

- scripts/MWArmsAssist/Drop.wav: police and AI racer spike deployment.
- scripts/MWArmsAssist/PlayerDrop.wav: player spike deployment.
- scripts/MWArmsAssist/Lock_by_police.wav: incoming EMP lock-on warning.
- scripts/MWArmsAssist/repair-complete.wav: tire repair completion.

Use PCM WAV files matching the included sounds: 16-bit stereo at 44.1 or 48 kHz. Renaming an MP3 to .wav does not convert it. Check the volume, leading silence and clipping. These replacements affect only the listed ArmsAssist sounds.

If you record your own replacements, capture only the short effect you need and check the source’s usage terms.

## Creating Events & Installing Community Events

MW Event Editor is a browser app for editing courses and event settings while viewing the world.
Open MW Event Editor: <https://zakkey250.github.io/MW-Event-Editor/>

- Open the editor in a supported browser and select your game folder to create or update a configuration. Folder access permission allows the editor to read that game locally.
- Edit or duplicate a stock event, or create a new one. Set its name, mode, racer count, reward and police settings, plus starting ammunition and cutscenes if needed.
- Adjust checkpoints, the start and finish, and the route. Apply Tollbooth time settings to tollbooths rather than ordinary checkpoints. For NeoAI courses, check directions, turnarounds and elevation changes as well.
- Check the preview and validation results, then save your editable project JSON. Export the current event or the selected set of events for the game.
- Extract the exported ZIP and drag its scripts folder into your game folder. Install or update required mods separately from the event data.
- Check the start, checkpoints and tollbooths, finish, retry, return to free roam and switching to another event. Keep your editable JSON and backups separately.

You can play events made by other MW Event Editor users when you have the required world and mods. Merge the download’s scripts folder into your game folder. To edit them, import events from your game, another folder, a ZIP or the author’s editable JSON.

Events can be shared across different car setups and languages when the world is the same. Events for a world with different road geometry cannot be loaded. If a duplicate or overwrite warning appears, choose which version to keep and move unwanted old folders outside the game directory. Renaming an event does not resolve a duplicate event ID.

Do not upload your game files or the editor’s extracted cache as part of an event download.

## Folder Layout

```text
Game folder (containing speed.exe)/
|-- speed.exe
|-- dinput8.dll                         Example ASI loader; installed separately
`-- scripts/
    |-- MWEventLoader.asi               Included
    |-- MWArmsAssist.asi                Included
    |-- MWArmsAssist.ini
    |-- NFSMWHudLifecycleFix.asi         Included
    |-- NFSMWHudLifecycleFix.ini
    |-- EventLoader/
    |   |-- EventLoader.ini
    |   |-- CustomEvents/
    |   |   `-- <event UUID>/
    |   |       |-- manifest.json
    |   |       |-- event.json
    |   |       |-- course/
    |   |       `-- maps/               For events that need them
    |   |-- CustomCutscenes/
    |   |   `-- <scene ID>/             Converted locally and added by you
    |   |-- Profiles/                   Event progress; keep when updating
    |   |-- WorldGeometry/              Automatically generated cache
    |   `-- UI/                         Automatically generated display data
    |-- MWArmsAssist/
    |   |-- Drop.wav
    |   |-- PlayerDrop.wav
    |   |-- Lock_by_police.wav
    |   |-- repair-complete.wav
    |   `-- (dedicated HUD assets)
    |-- NFSMWNativeFreeRoamRacers.asi    NFRR; installed separately
    |-- NFSMWNativeFreeRoamRacers.ini
    |-- NFSMWLimitBreaker.asi           LB; from the NFRR download
    |-- NFSMWLimitBreaker.ini
    |-- MWArsenal.asi                  Installed separately
    |-- MWArsenalAssets/               Follow the author's instructions
    `-- healthbars.asi                 Optional; supported setups only
```

This example shows a full installation; third-party mods are not included in the package. The main package, event ZIPs and locally converted cutscene ZIPs are separate. Diagnostic logging is off by default.

## Shared Rules for the Additional Modes

All three modes require MWArsenal, ArmsAssist and a compatible Event Loader. The additional health rules below do not apply to normal events.

- Player and AI racers have 300 HP, three times the 100-point police reference.
- Base weapon damage is 25 for EMP, 50 for spikes and 15 for Shockwave. These are fixed HP values, not percentages of maximum health. A full 300 HP therefore equals six spike hits before any mode-specific reductions.
- EMP stun, spike puncture/spin and Shockwave knockback still apply. In these modes, a puncture causes slowdown and a spin, followed by repair. Free roam keeps its existing repair rules.
- Wall and vehicle collisions also reduce HP. The car initiating a ram takes less self-damage, while the recipient’s damage depends on speed and impact severity. Hitting destructible objects does not reduce HP.
- Sparks appear in the yellow health zone and smoke in the red zone. Effect placement and appearance may vary on add-on cars.
- AI racers carry EMP, spikes, Shockwave and Jammer, subject to their ammunition and cooldowns. Permission to attack from any race position does not mean unlimited ammunition.
- At zero HP, a racer crashes and retires. If you retire, the game opens a defeat screen with a retry option. Arrests use the original arrest sequence.

## Additional Mode 1: Arms Race

Race for position with weapons and vehicle health. Arms Race supports Sprint, Speedtrap, Circuit and Lap Knockout, and can be played with police disabled.

- AI racers may attack from any race position, subject to ammunition, range, circumstances and cooldowns.
- Racers retire at zero HP. If your health runs out, you lose.
- Finish, lap, Speedtrap and elimination rules follow your selected race type.

## Additional Mode 2: Gauntlet

A timed escape based on Tollbooth racing. Reach tollbooths to gain time and break through police interference to reach the finish. You are the only racer.

- Up to four cars chase you: one invincible leader and up to three regular police cars. Roadblock vehicles and the helicopter are separate from this chase-car limit.
- The leader’s vehicle depends on the starting Heat level. It has enhanced pursuit performance and aggressively rams and blocks you. Its weapon ammunition is unlimited, but cooldowns still apply.
- The leader cannot lose HP, but EMP stun and puncture-induced spins still work. Supported overhead HUD displays show a star instead of a health bar.
- If you leave the leader far behind, a nearby police car takes over or a replacement joins the pursuit. Regular police cars can be destroyed and will be replenished.
- SUVs are excluded from the regular chase-car selection. Roadblocks and helicopters remain active.
- Reach the finish in time to win. Running out of time or health, or being arrested, means defeat. The pursuit stays active rather than entering cooldown during the event.
- Uses the original Tollbooth introduction, tollbooth counter, remaining time and lower pursuit bar. When you drive into a roadblock police vehicle, your own collision damage from that impact is disabled.

## Additional Mode 3: Hot Pursuit

A survival race in which every racer is a police target. Supports Sprint and Speedtrap, with a maximum of eight racers.

- Police begin pursuing each racer once that racer passes 25% course progress.
- The chase-car budget is twice the starting racer count: up to 16 police cars for an eight-racer event. Allocation prioritizes giving each eligible racer a first pursuer. When a racer retires, police are reassigned to survivors. Destroyed police cars can be replaced after three seconds.
- Roadblocks and the helicopter are separate from the chase-car budget. Roadblock support is directed toward the front of the race, and the helicopter follows the leader as positions change.
- AI racers use weapons against police, not against one another while fighting for the lead. Your own weapons follow their equipment and targeting rules.
- Police durability uses the normal rules. Police weapon damage to racers is reduced by 25%, and collision damage racers take from police is reduced by 50%.
- At Heat 5 or above, a racer destroying Cross restores 15% of maximum HP to every surviving racer: 45 HP out of 300. Health cannot exceed the maximum, and retired racers are not revived. Mods that change Cross’s vehicle identity may prevent this bonus from triggering.
- Finishing counts as a clear regardless of your position. Arrest or zero HP means defeat. AI racers that finish ahead of you continue escaping.
- The standings HUD alternates between racer names and remaining health. The pursuit bar appears at the bottom of the screen.

Your reward = configured reward × surviving racers ÷ your finishing position
Survivors are the starting field minus retired or eliminated racers, including cars that have already finished. A higher position earns you a larger share, while more surviving teammates increase the reward pool.

For example, a configured reward of 25,000 with six survivors and a third-place finish pays 50,000. If all eight survive and you finish eighth, you receive 25,000. If you are the only survivor, the reward pool is the configured reward amount.

## Copyright, Game Data & Redistribution

- This is an unofficial fan-made mod, not an official or endorsed EA product. Rights to Need for Speed and the original games’ names, images, audio and other assets remain with their respective owners.
- Game executables, original GLOBAL/TRACKS/NIS/MOVIES/SOUND data, source audio banks, complete worlds and other authors’ mods are not included. Obtain them separately.
- The converter processes game files you provide locally on your PC. It does not grant rights to distribute the source games or converted assets.
- Do not redistribute cutscenes extracted or converted from game files. Do not upload converted scene folders, installation ZIPs or their characters, textures, animations, audio or other assets.
- You may share your own event data, including event settings, courses and placements. Do not add original game data, converted cutscenes, separately distributed ASIs or other authors’ mods. Follow the original author’s terms when editing or redistributing someone else’s event.
- Do not include extracted caches, personal profiles, saves, Shadow/vault data or diagnostic logs in event packs. Check usage conditions for any included game-derived assets, such as stock preview images.
- If you distribute replacement audio or assets, follow the terms for those materials. Recording or processing a sound does not automatically transfer ownership of the source material.
- ArmsAssist source code is not public. See the included terms for redistribution of the mod and its assets. Permission to share your own event data is separate.

## Disclaimer

This mod and its tools are provided as is. Operation on every system, compatibility with other mods, and freedom from effects on game files or saves are not guaranteed. Back up your game, settings and saves, and check your setup before use.

To the extent permitted by applicable law, the author is not liable for data loss or other damage resulting from use, installation, editing or conversion. This does not exclude liability that cannot legally be excluded. When reporting a problem, include your executable, world, event name, required mod versions and reproduction steps. You do not need to publish your saves or complete game files.

## Known Issues & Limitations

- Large race fields, dense pursuits and HD cars can cause frame-rate drops or incomplete vehicle loading.
- To protect vehicle loading, events with six or more racers may limit aftermarket exterior parts to roughly half the field. NFRR applies a similar limit when at least five racers are actually present. Paint is unrestricted, and NFRR adjusts exteriors as cars naturally leave and are replaced.
- Healthbars can cause a rendering conflict and crash when combined with Redux’s older CustomHUD. Adding Healthbars to that setup is not recommended.
- Some event and mod combinations may cause problems when retrying after a finish, progressing through consecutive rival races or switching modes. Include the previous event and your actions when reporting these issues.
- NeoAI may struggle with tight turnarounds, overlapping roads, elevation changes or being knocked off course. Test custom courses with multiple racers.
- Cutscene lighting, particles and audio may differ from the original games. SFX without a matching source are not added. An incompatible racer count or missing files may cause the original introduction to be used instead.
- Older cropped preview maps cannot be corrected by updating the ASI alone. Re-export the event with map replacement enabled in a compatible editor.
- Unsupported executables or required mods, different world geometry, missing dependencies or duplicate events may prevent features or events from loading. Check the editor’s validation results.
- Some text may fall back to English depending on your game fonts, HUD setup and available translations.

## Credits & Links

- MWArsenal: Shiftersc & LEE-RYO.
MWArsenal download: <https://www.nfsaddons.com/downloads/nfsmw/tools/11267/mw-arsenal.html>
- MWHealthbars: trelbutate.
MWHealthbars: <https://github.com/trelbutate/MWHealthbars>
- Ultimate ASI Loader: ThirteenAG.
Ultimate ASI Loader: <https://github.com/ThirteenAG/Ultimate-ASI-Loader>
- NFRR, LB & MW Event Editor: Zakkey250.
NFRR: <https://github.com/Zakkey250/MW-NativeFreeRoamRacer>
LB: <https://github.com/Zakkey250/MW-Limit-breaker>
MW Event Editor: <https://zakkey250.github.io/MW-Event-Editor/>

See the included documentation for other library credits, third-party notices and usage terms.

## Converter guides

[English](docs/Cutscene-Converter-EN.md) · [日本語](docs/Cutscene-Converter-JA.md). The converter downloads its pinned reader libraries only after you approve; afterward it works offline. It does not include the source games or converted cutscenes.

## Licenses

The current Event Loader and HUD Lifecycle Fix runtime sources are available in this repository. See [source layout and build instructions](docs/Source-Build.md). Editor, Cutscene Converter and ArmsAssist source code is not included.

Repository material retains the [MIT license](LICENSE). Bundled components and third-party material retain their own included notices and terms. ArmsAssist source is not included.
