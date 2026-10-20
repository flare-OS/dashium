#pragma once

namespace dashium {

/// Settings that the render hooks read every frame. Kept in plain globals so
/// the hot path never touches the settings JSON.
struct Config {
    static inline bool forceLdm = true;
    static inline bool noGlow = true;
    static inline bool noOrbEffects = true;
    static inline bool noObjectAnimations = false;
    static inline bool noEffects = true;
    static inline bool noShaderEffects = true;
};

} // namespace dashium
