# Dashium

**Sodium-class rendering for Geometry Dash.**

Geometry Dash 2.2 already puts level objects into `CCSpriteBatchNode` layers, so
"fewer draw calls" alone buys almost nothing — a normal frame is ~30 draws. What
actually costs frames in an object-heavy level is *how much geometry exists and
how much of it is drawn*. Dashium attacks that, Sodium-style.

## What it does

| Lever | Effect |
| --- | --- |
| **Force Low Detail Mode** | High-detail decorative objects are never created. This is the single biggest win, and it is what LDM does when you enable it by hand. |
| **Disable object glow** | Glow batch layers and per-object glow sprites are hidden, removing the huge additive overdraw that dominates glow-heavy levels. |
| **Disable orb effects & pulse** | Skips `RingObject::spawnCircle` (the activation burst) and orb shine effects. |
| **Disable particle effects** | Blocks object particle spawning, hides existing particle systems and skips the layer's particle update loop. |
| **Disable level shader effects** | Stops the level's shader objects (shock wave, glitch, lens, blur, ...) from updating. |
| **Disable object animations** | Freezes the engine's real animation paths: synced frame animations, animated sprites and cocos actions/schedulers on level objects. |
| **Draw batching** | Sprites that are *not* in an engine batch node (players, attached sprites) are folded into one buffer per render state, with off-screen and fully transparent quads dropped. |
| **Stats HUD** | Live FPS, object count, quads and draw calls so the win is visible. |

Everything is a setting; turn any piece off and it goes back to the vanilla path.

## Why this is the "Sodium" of GD

Sodium's headline win is not a magic shader: it is *doing less work for things
the player cannot see* and *not paying CPU for geometry that never reaches the
screen*. In 2D the equivalent is:

1. Do not create decorative objects the player cannot meaningfully see (LDM).
2. Do not rasterise giant additive glow quads (fill-rate).
3. Do not run per-frame animation loops for static decoration.
4. Batch what the engine does not batch, and cull quads outside the viewport.

Gameplay is untouched: physics, triggers, collision, hitboxes and level data are
never modified.

## Settings

- `Enable batching` — master switch for the sprite batcher.
- `Cull off-screen quads` — drop quads fully outside the viewport.
- `Force Low Detail Mode` — skip high-detail decorations on load.
- `Disable object glow` — hide glow layers and per-object glow sprites.
- `Disable orb effects & pulse` — no orb activation burst / shine.
- `Disable object animations` — freeze synced, animated-sprite and action-driven animations.
- `Disable particle effects` — no object particles and no particle simulation.
- `Disable level shader effects` — stop level shader objects from updating.
- `Show stats overlay` — FPS / objects / quads / draws.
- `Quads per batch` — vertex buffer flush threshold.

## End screen

A small gold badge, **Dashium used | Early Access**, appears on the level
complete screen.

## Build

```sh
GEODE_SDK=/path/to/geode-sdk cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

or simply `geode build`.
