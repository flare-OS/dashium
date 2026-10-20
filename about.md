# Dashium

**Sodium-class rendering for Geometry Dash.**

Geometry Dash 2.2 already batches level sprites into a handful of texture
atlases, so the bottleneck is not draw-call count — it is how much geometry is
created, how much overdraw it produces, and how much per-frame animation work
runs for decoration. Dashium removes that work.

## What you get

- **Force Low Detail Mode** — high-detail decorative objects are never created.
  This is the same win as enabling LDM manually, applied to every level.
- **Disable object glow** — hides the huge additive glow layers and the
  per-object glow sprites. Glow-heavy levels gain the most here.
- **Disable orb effects & pulse** — no activation burst or shine when you hit an
  orb.
- **Disable particle effects** — blocks object particle spawning, hides existing
  particle systems and stops the particle update loop.
- **Disable level shader effects** — stops the level's shader objects (shock
  wave, glitch, lens, blur, ...) from updating. This is the big one for
  "3D"-style levels.
- **Disable object animations** — freezes the engine's real animation paths
  (synced frame animations, animated sprites, actions and schedulers). Trigger
  effects that are gameplay-relevant still work.
- **Adaptive sprite batching** — sprites outside the engine's batch nodes are
  merged into one buffer per render state, with transparent and off-screen quads
  rejected before submission.
- **Stats HUD** — FPS, object count, quads and draw calls.

Gameplay never changes: physics, triggers, collision, hitboxes and level data
are untouched. Only rendering and the objects that exist for decoration are
affected.

## Verification

Enable the stats overlay; it shows `fps | objects | quads | draws`. Compare a
heavy level with **Force Low Detail Mode** on and off to see the object count,
and with **Disable object glow** on and off to see the frame rate move in
glow-heavy sections.

## Notes

- Every optimization is a separate setting and can be disabled.
- `Disable object animations` is off by default because it is the most visible.
- If a level looks wrong with an option on, turn that option off and report it.
