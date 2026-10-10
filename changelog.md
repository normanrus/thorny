# Changelog

All notable changes to Thorny. Versions follow the released tags.

## 1.0.9

- Fixed the speech bubble sitting to the right of (and slightly above) Thorny.
  The horizontal clamp used the padded flight area instead of the screen edge,
  so a centred bubble was pushed sideways by roughly its own half-width when
  Thorny was left of centre.

## 1.0.7

- Fixed the speech bubble floating well above Thorny. The sprite's reported
  content size can disagree with what is actually drawn across Geode's
  `-hd`/`-uhd` asset scales; the measured half-height is now clamped so the
  bubble always sits right above the sprite.

## 1.0.6

- Fixed the speech bubble flying far away from Thorny. The bubble container was
  a `CCLayer` (screen-sized content, centre anchor), so the
  `convertToWorldSpace` clamp added a half-screen offset. It is now a plain
  `CCNode` positioned in the same space as the movement bounds.

## 1.0.5

- Restored Thorny's dialogue (and the bubble tail with it). The v1.0.3 refactor
  made the main-menu entry react through a running-scene lookup, which returns
  nothing during `MenuLayer::init`; the spawn now returns the node directly.

## 1.0.4

- Deferred the disable path of `onSettingChanged` as well, so no node work runs
  on Geode's settings-application call stack.

## 1.0.3

- Fixed an Android crash and a doubled sprite. The stale `MenuLayer` /
  `WeakRef` pointers could survive a scene teardown and be dereferenced again;
  the manager now resolves the live menu from the running scene on demand and
  never caches game-node pointers.

## 1.0.2

- Fixed Thorny's spawn placement, the speech-bubble lifetime, and a crash when
  changing settings.

## 1.0.1

- Fixed a crash in the movement code and corrected the spawn position.

## 1.0.0

- First release. Thorny lives in the main menu: bounded random flying, a speech
  bubble, animation states, contextual EN/RU dialogue, Geode settings, and
  hooks for level start / death / completion / editor return.
