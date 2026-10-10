# Thorny

> A little spike companion that reacts to what you do in Geometry Dash.

Thorny is a **Geode** mod for Geometry Dash **2.2.x** (built and tested against
GD `2.2081` / Geode `5.11.1`). It adds a small living spike character that
hovers around the **main menu only**, flies between random points, shows a
speech bubble, and changes its mood and lines based on what you just did:
started a level, died, beat a level, came back from the editor, or just sat
there doing nothing for a while.

There is no separate app and no direct tampering with the game's files - it is a
regular Geode mod.

---

## Features

- **Lives in the main menu only.** Thorny is added to `MenuLayer` and removed
  with the scene. It is never created in `PlayLayer`, `LevelEditorLayer`,
  `LevelSelectLayer`, `PauseLayer`, or any other scene.
- **Smooth, bounded movement.** Thorny picks a random destination, eases toward
  it, occasionally darts or pauses, and is clamped to the visible screen with
  padding - so it works on 16:9, 18:9, 20:9, tablets, and small screens.
- **Animation states.** `Idle`, `Flying`, `Happy`, `Sad`, `Excited`, `Sleeping`,
  `Talking`. Idle bobs and sways; happy spins; sad sinks and slows; excited does
  a burst; sleeping drifts and bobs slowly.
- **Speech bubble** with a tail, drawn with `CCScale9Sprite` + `CCLabelBMFont`.
  It auto-sizes, flips to the opposite side near the screen edge, and never
  leaves the screen.
- **Contextual dialogue** loaded from JSON, in **English and Russian**, with
  `Auto` detection from the game language. No more than a few `if` statements -
  lines live in a data file and are picked by category and weight.
- **Cooldowns + priority.** Ordinary lines respect a cooldown and a
  per-event repeat cooldown; high-priority events (level completed, returned
  from the editor, first greeting) may bypass it.
- **Remembers you.** The first greeting plays exactly once, tracked as a Geode
  saved value.
- **Mod settings** (Geode's own settings UI): enabled, movement, dialogue,
  dialogue frequency, language.
- **Lightweight and safe.** Movement is driven by `CCAction`s plus one low-rate
  scheduler for decisions. No per-frame scene searches, no object churn. Every
  pointer is null-checked, all scheduled callbacks are unscheduled on exit, and
  no global pointers to game objects are kept.

---

## Installation

1. Install [Geode](https://geode-sdk.org/) for your platform.
2. Build the mod (see below) or download a released `.geode` file.
3. Drop the `.geode` file into the game's `geode/mods/` folder
   (on Android: `/storage/emulated/0/Android/media/com.geode.launcher/game/geode/mods/`).
4. Restart the game and open the main menu.

---

## Settings

| Setting | Type | Default | Notes |
| --- | --- | --- | --- |
| Thorny enabled | bool | `true` | Show the character at all. |
| Movement | bool | `true` | Let Thorny fly around. |
| Size | float | `1.5` | How big Thorny is drawn (`0.5`–`3.0`). |
| Dialogue | bool | `true` | Let Thorny talk. |
| Dialogue frequency | one-of | `Normal` | `Rare` / `Normal` / `Frequent`; scales the cooldown. |
| Language | one-of | `Auto` | `Auto` / `English` / `Russian`. |

Settings that affect live state (enabled, movement, size, dialogue, language)
are read and re-applied immediately when changed.

---

## Building

Requirements: the Geode SDK (version matching the `geode` key in `mod.json`),
CMake 3.21+, and a C++23 toolchain. The easiest path is the Geode CLI.

```bash
# once
geode sdk install            # installs the SDK and the CLI
# then, from the project root
geode build                  # produces build-*/norman.thorny.geode
```

The Geode CLI also runs the resource pipeline: it packs the PNGs in
`resources/thorny/` into a spritesheet, adds medium/low variants, and converts
`resources/fonts/ThornyFont.ttf` into a GD-compatible bitmap font (with the
`charset` in `mod.json` so Cyrillic glyphs are included).

Manual CMake build (needs `GEODE_SDK` set in the environment):

```bash
export GEODE_SDK=/path/to/geode-sdk
cmake -B build -S .
cmake --build build --config Release
```

### Local syntax check (optional, non-Windows)

`scripts/syntax-check.sh` parses the sources with GCC against stub Windows
headers. It is a convenience for development on Linux and is **not** part of the
real build. It expects the Geode SDK headers and a bindings checkout; see the
top of the script for the environment variables it reads.

---

## Project structure

```
Thorny/
├── CMakeLists.txt
├── mod.json
├── README.md
├── LICENSE
├── .gitignore
├── scripts/
│   └── syntax-check.sh        # dev-only GCC parse check
├── src/
│   ├── main.cpp               # $on_mod(Loaded) entry point
│   ├── Thorny.hpp/.cpp        # the character, its visuals, movement, bubble
│   ├── ThornyManager.hpp/.cpp # session, event routing, cooldowns, lifecycle
│   ├── ThornyDialogue.hpp/.cpp# dialogue database + language selection
│   ├── ThornyEvents.hpp/.cpp  # ThornyEvent / ThornyContext / session state
│   ├── ThornySettings.hpp/.cpp# typed access to mod settings + language
│   └── ThornyHooks.cpp        # all $modify hooks / event wiring
└── resources/
    ├── thorny_dialogue.json   # lines, by language and category
    ├── fonts/
    │   └── ThornyFont.ttf
    └── thorny/
        ├── thorny_idle_001.png
        ├── thorny_happy_001.png
        ├── thorny_sad_001.png
        ├── thorny_angry_001.png
        ├── thorny_sleep_001.png
        ├── thorny_surprised_001.png
        ├── thorny_bubble_001.png
        └── thorny_tail_001.png
```

---

## How it works

### Lifecycle

`main.cpp` runs `ThornyManager::initialize()` on `$on_mod(Loaded)`. The manager
holds the session/context and never touches a scene directly. A `$modify` hook
on `MenuLayer::init` calls `ThornyManager::onMainMenuEntered(MenuLayer*)`, which
creates a `Thorny` node, adds it as a child of the layer, starts its
idle/flying behaviour, and then plays whatever reaction matches the last event.

Cleanup does **not** hook `MenuLayer::onExit`: `MenuLayer` does not override
`onExit`, so it has no address in the bindings and such an override would never
run. Instead Thorny is a self-contained node - its own `onExit` unschedules
every callback and stops all actions. The manager additionally holds only a
`geode::WeakRef<Thorny>` and a `geode::WeakRef<CCLayer>` to the menu, so the
references invalidate themselves when the scene is torn down and a stale
pointer can never be dereferenced, even if cleanup happens in an unexpected
order.

### Event detection

| Event | How it is detected |
| --- | --- |
| Entered main menu | `MenuLayer::init` hook |
| Started level | `PlayLayer::init` hook (also records level name/author/completion) |
| Player died | `PlayLayer::destroyPlayer` hook, then `PlayerObject::m_isDead` |
| Level completed | `PlayLayer::levelComplete` hook (respects `m_isPracticeMode`) |
| Exited level without finishing | `PlayLayer::onQuit` hook |
| Entered editor | `LevelEditorLayer::init` hook (records the level) |
| Edited a level | `LevelEditorLayer::levelSettingsUpdated`; the "was modified" bit is read from `GJGameLevel::m_hasBeenModified` |
| Editor playtest | `LevelEditorLayer::onPlaytest` / `onStopPlaytest` mark the session as "in the editor"; gameplay events during a playtest are gated out |
| Exited editor | Detected when the main menu is entered again while the editor was still latched as open |

Completion is decided from `PlayLayer::levelComplete`, **not** from a raw
`percentage == 100` check, which the spec explicitly discouraged.

The **editor-vs-test-play** case is handled explicitly: while the editor session
is latched open (from `LevelEditorLayer::init` until the main menu reappears),
`PlayLayer` events are treated as playtest and swallowed, so testing from the
editor is never reported as a real level start, death, or completion - and
returning from it is never mistaken for leaving the editor.

### Dialogue

Lines live in `resources/thorny_dialogue.json` under `languages.<lang>.<category>`.
At load time they are flattened into weighted entries tagged with a category.
Selection is a weighted random pick within the category, preferring the active
language and falling back to the other one if a category is missing. A
per-category "avoid the last few lines" memory plus the cooldowns keep Thorny
from repeating itself.

### Cooldowns

- Base cooldown: `15s` (`Rare` = 30s, `Frequent` = 8s).
- Per-event repeat cooldown: `30s`.
- High-priority events (level completed, returned from editor, first greeting)
  bypass the base cooldown but still avoid immediate repeats.

---

## Geode APIs used

- `geode::Mod::get()`, `getSettingValue<T>`, `listenForSettingChanges`,
  `getSavedValue` / `setSavedValue`, `getResourcesDir`
- `geode::log::info/debug/warn`
- `geode::WeakRef<T>` (`Geode/utils/cocos.hpp`)
- `geode::CallFuncExt` (`Geode/utils/cocos.hpp`)
- `geode::file::readString` (`Geode/utils/file.hpp`)
- `$on_mod(Loaded)`, `$modify`
- `geode::cocos::` node lookup helpers (`getChildByIDRecursive`)

## Geometry Dash / cocos2d APIs used

- `cocos2d::CCLayer`, `CCNode`, `CCSprite`, `CCLabelBMFont`, `CCSpriteFrameCache`
- `cocos2d::extension::CCScale9Sprite`
- Actions: `CCMoveTo`, `CCMoveBy`, `CCRotateBy`, `CCScaleTo`, `CCSequence`,
  `CCSpawn`, `CCRepeatForever`, `CCDelayTime`, `CCEaseSineInOut`,
  `CCEaseBackOut`
- `CCDirector::get()->getVisibleSize()` / `getVisibleOrigin()`
- `CCApplication::get()->getCurrentLanguage()` / `kLanguageRussian` for `Auto`
  language detection
- Bindings: `MenuLayer`, `PlayLayer`, `LevelEditorLayer`, `GJGameLevel`,
  `PlayerObject`

## Hooks

| Class | Function | Purpose |
| --- | --- | --- |
| `MenuLayer` | `init` | Create Thorny and play the pending reaction |
| `LevelInfoLayer` | `init` | Remember the level the player is looking at |
| `PlayLayer` | `init` | Record that a level started |
| `PlayLayer` | `destroyPlayer` | Detect a death (via `PlayerObject::m_isDead`) |
| `PlayLayer` | `levelComplete` | Detect a real completion (respects practice mode) |
| `PlayLayer` | `onQuit` | Detect a quit without finishing |
| `LevelEditorLayer` | `init` | Record that the editor was opened |
| `LevelEditorLayer` | `levelSettingsUpdated` | Record that the level was modified |
| `LevelEditorLayer` | `onPlaytest` | Mark the editor's playtest as running |
| `LevelEditorLayer` | `onStopPlaytest` | Mark the editor's playtest as finished |

Editor exit and menu exit are not hooks: the editor exit is inferred when the
main menu is entered again, and menu cleanup happens inside `Thorny::onExit`.
See "Lifecycle" above for why.

---

## Known limitations

- **Main menu only.** Thorny intentionally does not appear anywhere else.
- **"Level completed" from `levelComplete`.** Very unusual level-end paths
  (custom end triggers, some platformer flows) may not raise the exact hook;
  the completion flag is therefore best-effort.
- **Editor "modified"?** The modified flag is read from
  `GJGameLevel::m_hasBeenModified`, which is about the loaded level, not about
  whether you pressed save. Thorny never claims a level was saved.
- **Language detection** relies on the reported game language; on an
  unsupported language Thorny falls back to English. `Auto`/`English`/`Russian`
  are the supported choices.
- **Platform coverage.** The bindings and hook signatures are for GD `2.2081`;
  other 2.2.x builds may need address/signature updates from upstream bindings;
  that is out of this mod's control.
- **Assets** are simple, original placeholder art (flat-coloured shapes) - no
  copyrighted game art is bundled. Swap the PNGs in `resources/thorny/` to
  restyle Thorny.

---

## Links

- Geode SDK: <https://github.com/geode-sdk/geode>
- Geode bindings: <https://github.com/geode-sdk/bindings>
- Geode docs: <https://docs.geode-sdk.org/>
- Geode example mod: <https://github.com/geode-sdk/example-mod>

## License

MIT - see [LICENSE](LICENSE). Author: NormanRus.
