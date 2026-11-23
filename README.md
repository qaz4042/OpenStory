<div align="center">

<a href="https://www.buymeacoffee.com/rdiol12" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" height="60" width="217"></a>

*If my work helped you, a coffee keeps the next project coming.*

**OpenStory resurrected - A v83 MapleStory client for cosmic servers**

<img src="https://img.shields.io/badge/C++-2b3137?style=for-the-badge" alt="C++"> <img src="https://img.shields.io/badge/Open%20Source-2b3137?style=for-the-badge" alt="Open Source"> <img src="https://img.shields.io/badge/Maintained-2026-2b3137?style=for-the-badge" alt="Maintained">

</div>

---

# OpenStory

## PS5 port look at ps5 branch

The experimental native PS5 port provides controller navigation, native keyboard entry, server selection and streamed NX loading. See the [PS5 build and play guide](https://github.com/rdiol12/OpenStory/blob/ps5/platform/ps5/README.md).

## Features

- Full v83 client connecting to Cosmic servers
- NX file-based assets, OpenGL rendering, GLFW windowing
- Login flow, character select, world/channel select, PIC entry
- Combat, skills, buffs, inventory, quests, NPCs, chat
- Storage, buddy list, skill macros, gamepad support
- Distance-based (spatial) sound for world events
- Fullscreen, UI scaling, drag-and-drop windows

### Text & input

- Fonts are **compiled into the binary** (Roboto + Noto Sans Hebrew) — the client needs no
  font files at runtime and does not depend on `C:/Windows/Fonts`, so glyphs never
  silently vanish on another machine.
- **Right-to-left text** (Hebrew): Unicode bidi reordering, right-aligned wrapping, and a
  caret that tracks the visual position rather than the byte offset.
- **Unicode input** via the OS keyboard layout, with character-safe editing — backspace,
  delete and the arrow keys move whole characters, not bytes.
- Inline icons in chat (`#v/#i/#q/#s/#f/#e`) and an emoticon picker on the chat bar.

Non-ASCII text requires the server to use UTF-8. On Cosmic that is
`CHARSET: UTF-8` in `config.yaml`; note its `CharsetConstants` whitelist must contain
UTF-8 or it silently falls back to US-ASCII and mangles everything to `?`.

### Custom / experimental

Some features are client-side additions not supported by a stock Cosmic server:

| Feature | Status | Notes |
|---------|--------|-------|
| Event System | WIP | Custom `EVENT_INFO` / `REQUEST_EVENT_INFO` packets; needs a server-side handler |
| HP/MP Warning | Working | Client-only |
| Graphics/Effects Quality | Working | Client-only |

## Building

For Apple Silicon macOS, see the [macOS build and app bundle guide](resource/macos/README.md).

### Requirements
- Visual Studio 2022+, Windows SDK, CMake 3.15+
- Dependencies: GLFW, GLEW, FreeType, Bass audio, NoLifeNx, Asio

### Build
```bash
cmake -S . -B build
cmake --build build --config Debug --target OpenStory
# Output: wz/OpenStory.exe
```

Place the NX files described in Configuration in the `wz/` directory.

Windowed mode fits and centers the window inside the desktop work area and
allows resizing. Alt+Enter switches to fullscreen and restores the previous
window size and position when switching back. Both modes preserve the game's
aspect ratio (with black bars where necessary), including on Retina displays;
desktop fitting does not overwrite the saved game resolution.

Fonts are compiled in via the generated `src/Graphics/EmbeddedFonts.{h,cpp}`, which are
committed — no extra build step.

## Language support (incremental)

System Settings retains its original NX frame, colors, controls and layout.
Click the bottom-left `中/EN` button to switch English / Simplified Chinese.
The language choice is saved immediately, including when other settings are
cancelled with Cancel / Esc. The ordinary Settings file in the resolved data
directory stores `UILanguage = en` or `UILanguage = zh-CN`. Unknown values use English.

Chinese currently covers the main settings labels, original-style common
buttons, character/stat labels, inventory titles/tabs, skill-window titles,
quest-tracker text, keyboard prompts/action tooltips, system-menu entries and selected client notices (quit,
quest forfeiture, item dropping and numeric validation). Some text baked into
artwork remains English. Other windows retain their original skin; no complete
client translation or replacement NX pack is installed. Server text is unchanged.

Translations and source notes are in `src/Localization/` and
`docs/localization/terms.csv`. Official-page wording is distinguished from project
translations; no newly obtained official Chinese client resources are claimed.
On macOS, STHeiti provides CJK fallback even with an old Windows font setting.
Monochrome glyphs use a reserved atlas region to avoid sprite collisions.

`python3 tests/localization-settings.py` checks immediate language persistence,
reload in a separate process, cancellation of other options,
bilingual notice callbacks and renders the settings screens using production
objects and local NX assets without connecting to a server. Screenshots are in
`build-macos/localization-check/`. Game-session regression remains necessary.

## Configuration

Use v83 game data in `wz/` and a compatible interface archive as `UI.nx`. The v154 interface archive supplies `UIWindow2.img`, `UIWindow4.img`, and `StatusBar2.img`, which the equipment, inventory, skill, stats, and status bar views use.
The optional `UI.Login.nx` archive in the data directory supplies v83 login
buttons, the save-ID checkbox, the three-class race selection screen, and matching world/channel selection controls. This avoids v154 login controls clashing with
the wooden v83 signboard. If absent, the existing UI.nx login art is used.
See `docs/localization/login-resource.md` for the local resource record.

Create a local `Settings` file beside the client executable and set `ServerIP`
and `ServerPort` to your Cosmic server's reachable address and login port.
The default address is loopback and does not select a public server.

```ini
ServerIP = 127.0.0.1
ServerPort = 8484
SaveLogin = false
```

Use `127.0.0.1` only when the server runs on the same computer. Start the server,
open the client, then log in with an account created on that server. On PS5,
enter the address on the server selection screen. Display preferences such as
`VSync` and `Fullscreen` can also be changed in `Settings`.

## Inventory

Click the coin button in either inventory size to drop mesos. The amount must
be 10–50,000 and cannot exceed your balance; Cancel / Esc sends no request.
Click Gather to merge stacks and automatically sort that inventory tab after
the server acknowledges the merge, even if you switch tabs while waiting.
These actions use Cosmic's existing v83 packets; no server deployment is needed.

## Quest Helper

Track up to 5 quests at once with live progress updates.

- **Add a quest**: Open the Quest Log (Q), go to In-Progress, then drag a quest into the Quest Helper (or double-click an opened quest)
- **Remove a quest**: Click the X button next to the quest name
- **Reorder**: Drag a quest name up or down within the Quest Helper
- **Collapse/Expand**: Click a quest name to toggle its requirements
- **Auto-track**: Click the AUTO button to fill the helper with your active quests

## Credits

- **Daniel Allendorf & Ryan Payton** — Original [HeavenClient](https://github.com/HeavenClient/HeavenClient)
- **rdiol12** — v83 Cosmic compatibility, UI systems, packet handlers

## License

GNU Affero General Public License v3. See [LICENSE](LICENSE).
