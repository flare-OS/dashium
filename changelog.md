# v1.2.0 (Early Access)

- **Disable particle effects** now works end to end: object particle spawning is
  blocked, existing particle systems are hidden, and the layer's particle update
  loop is skipped.
- **Disable level shader effects**: the level's shader objects (shock wave,
  glitch, lens, blur, ...) no longer update.
- **Disable object animations** now freezes the engine's real animation paths:
  `updateSyncedAnimation`, animated sprites and their animation managers, plus
  cocos actions/schedulers on level objects.
- **Disable object glow** now also hides the per-object glow sprite, not just
  the glow batch layers.
- Orb activation bursts (`spawnCircle`) and orb shine effects are skipped when
  orb effects are disabled.
- "Dashium used | Early Access" badge on the level complete screen.
- Stats are logged at debug level and less often.

# v1.1.0 (Early Access)

- Force Low Detail Mode: high-detail decorative objects are no longer created.
- Disable object glow: hides the engine's glow batch layers.
- Disable orb effects & pulse: skips orb activation bursts and stops orb pulse
  animations.
- Stats HUD now reports real frame data: FPS, level object count, quads and
  draw calls (including the engine's own batch nodes).
- "Dashium used | Early Access" badge on the level complete screen.
- Fixed a crash caused by touching particle batch-node fields that do not match
  the Windows 2.2081 layout.

# v1.0.0

- Initial release.
- Clip-space sprite batcher for level objects: merges consecutive quads that
  share texture, shader, blend mode, parent and z-layer into one draw call.
- Viewport culling and fully-transparent quad rejection before submission.
- Stats HUD with quad count, draw calls, batch ratio, culled count and FPS.
- Settings for batching, culling, HUD visibility and max quads per batch.
