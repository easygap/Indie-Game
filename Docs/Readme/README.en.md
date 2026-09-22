[한국어](../../README.md) · **English** · [日本語](README.ja.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md)

# The Missing Floor

A first-person horror game set in an old Korean apartment building. Search for your missing brother by talking to the neighbors and following clues through the rooms and corridors.
At night, an enemy hunts by sound. Even opening a door can give you away.

Windows PC · Single-player · In-game language: Korean only · In development

[Download for Windows](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip) · [Controls](#controls)

![The Missing Floor title screen, showing an old apartment building and the Korean main menu.](../Media/readme/title-menu-first-run-1080.webp)

## The story

Yudam moves into Apartment 403 at Dalbit Villa to find her missing brother. The address on a returned package says Apartment 501, but the building only has four floors.

On her first night, she hears knocking from the ceiling at 4:30 a.m.

![Yudam’s room, with moving boxes and everyday belongings still unpacked.](../Media/readme/game-bedroom.webp)

## Explore the neighborhood

During the day, visit the neighbors and look around the alley and convenience store. Conversations and objects left behind can help you trace your brother’s whereabouts.

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/game-alley.webp" alt="The alley between the apartment building and the convenience store."></td>
    <td width="50%"><img src="../Media/readme/game-store.webp" alt="A clerk behind the counter at the neighborhood convenience store."></td>
  </tr>
  <tr>
    <td>The alley outside</td>
    <td>The convenience store</td>
  </tr>
</table>

[More views of the apartment and neighborhood · 4.9 MB GIF](../Media/readme/readme-route-preview.gif)

## Keep quiet after dark

Running down the corridor or opening a door too quickly can bring the enemy straight to you. Open doors slowly and hold your breath for a moment to make less noise as you pass.

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/game-corridor-day.webp" alt="The fourth-floor corridor during the day, with the lights on."></td>
    <td width="50%"><img src="../Media/readme/game-corridor-night.webp" alt="The same corridor after dark, at 4:30 a.m."></td>
  </tr>
  <tr>
    <td>During the day</td>
    <td>4:30 a.m.</td>
  </tr>
</table>

![Gameplay footage of the enemy approaching and catching the player in the corridor.](../Media/readme/night-listener-chase.gif)

## Follow the clues

Listen through walls, read the building records, check the CCTV, and inspect the meter cabinet.
You can review collected clues with `Tab` during the day. Press `H` for a hint if you get stuck.

<table>
  <tr>
    <td width="50%"><img src="../Media/readme/p1-meter-cabinet.webp" alt="Electricity meters and a reading sheet inside the meter cabinet."></td>
    <td width="50%"><img src="../Media/readme/game-booth.webp" alt="Documents and CCTV monitors on the building manager’s desk."></td>
  </tr>
  <tr>
    <td>The meter cabinet</td>
    <td>The management office</td>
  </tr>
</table>

## Download and play

The **September 22, 2026 test version** is available now. **The game currently supports Korean only**, including menus, dialogue, and clues.

1. [Download the Windows ZIP (727 MB)](https://github.com/easygap/Indie-Game/releases/download/v1.0.0-test.20260922/MissingFloor-Windows-20260922.zip).
2. Extract the entire archive, then run `IndieGame.exe`.
3. Select **게임 시작** (New Game). To resume a saved game, select **이어하기** (Continue).

Progress saves automatically. Keep the `Engine` and `IndieGame` folders alongside the executable.

Tested on Windows 11 with a Ryzen 9 7900X, RTX 3060, and 32 GB RAM. Minimum requirements have not been established yet.

If a DLL is missing, check that you extracted all the files, then run `Engine/Extras/Redist/en-us/vc_redist.x64.exe`. If the game stutters, try a lower resolution or graphics setting.

## Controls

Move with `WASD` and look around with the mouse. Press `E` to inspect objects or open doors; hold it to open a door quietly.

Controllers are supported, and you can remap keys and buttons in the settings. Press `F1` or D-pad Up to bring up the controls again.

<details>
<summary>Full control list</summary>

| Input | Action |
|---|---|
| `W A S D` / mouse | Move / look around |
| `Left Shift` / `C` / `Space` | Run / crouch / jump |
| `E` | Inspect / open door |
| Hold `E` | Open door quietly / listen through a wall |
| `Q` / `Left Ctrl` | Knock / hold breath |
| `F` | Flashlight |
| `F1` | Objectives and controls |
| `Tab` / `H` | Clue journal (daytime) / hint |
| `← →` / mouse wheel | Turn document pages |
| `Esc` / `F10` | Pause / accessibility settings |

</details>

## Difficulty and accessibility

If you’d rather explore without being chased, press `F10` and choose **추격 없음** (No Chases). You can still solve the puzzles and finish the story without being caught. Easy, Normal, and Hard are also available, and you can change difficulty while playing.

Adjust subtitle size, background opacity, and how long text stays on screen. Visual sound cues and controller vibration can help you notice sounds and knocks. You can also reduce camera shake and flashing, replace button holds with toggles, and adjust music and ambient sound separately.

![The Korean accessibility menu, with subtitle size, background opacity, and display time settings.](../Media/readme/settings-accessibility-20260922.webp)

Contains dark scenes, jump scares, and loud sounds. Adjust the volume and brightness before you start.

## Need help?

[Report a problem](https://github.com/easygap/Indie-Game/issues/new) or [leave feedback](https://github.com/easygap/Indie-Game/issues/new). For bugs, include what you were doing, what went wrong, and a screenshot or error message if you have one.
