# Thorny — repository notes

Geode mod for Geometry Dash 2.2081 / Geode 5.11.1. A companion spike character
that lives in the main menu. See README.md for user-facing docs.

## Build / check

- Real build: GitHub Actions (`.github/workflows/*`). Pushing main builds all
  platforms; pushing a `vX.Y.Z` tag also publishes a `.geode` release asset.
- Local syntax-only check (no Geode toolchain needed):

  ```bash
  bash scripts/syntax-check.sh          # all src/*.cpp
  bash scripts/syntax-check.sh src/Thorny.cpp
  ```

- Push with the full-scope token: `GH_TOKEN`. The auto-injected `GITHUB_TOKEN`
  is a limited GitHub-App token and returns 403 on git push / ref writes.
- Bindings for lookup live under `/tmp/modb/bindings/bindings/Geode/...`.

## Hard-won lessons (do not regress)

- **Never cache a pointer to a game node across scene transitions.**
  Geode's `WeakRef::lock()` is best-effort: if a scene is freed without Geode
  intercepting it (common on Android), the weak slot is never cleared and
  `lock()` returns a dangling pointer. `MenuLayer::addChild` on that pointer
  crashed with a SIGSEGV whose vtable resolved to `CCSpriteBatchNode`.
  Resolve the live menu from `CCDirector::getRunningScene()` every time via
  `Thorny::findMainMenu()`, and find Thorny by node ID in that scene.
- **Never start/stop cocos2d actions from a Geode setting listener.** It
  re-enters the action manager mid-application and can crash. Defer all node
  work with `geode::queueInMainThread`.
- **MenuLayer does not override `onExit`**, so `$modify`-ing it is a no-op.
  Clean up in our own `Thorny::onExit` instead.
- The speech bubble is a plain `CCNode`, NOT a `CCLayer`. A geometry-dash
  `CCLayer` has screen-sized content with a centre anchor, so
  `bubble->convertToWorldSpace()` returns `local - halfScreen`; using that to
  position/clamp made the bubble fly far from Thorny. Do all bubble positioning
  in the parent space, never via `convertToWorldSpace`. Hide it until a line is
  shown, else an empty bubble frame reads as a second, detached sprite.
- Clamp the bubble horizontally against the REAL visible screen rect
  (`visibleRectInParent(6, 6)`), not `m_bounds`. `m_bounds` is the padded flight
  area, so clamping to it shoved a centred bubble sideways by roughly its own
  half-width whenever Thorny sat near a screen edge.
- Sit the bubble just above the sprite by measuring the sprite and clamping the
  half-height into a tight band (`[12, 24]`). `getContentSize()` can disagree
  with what is actually drawn across Geode's `-hd`/`-uhd` asset scales, so never
  trust the raw value; a fixed pixel gap goes stale when the size setting moves.
