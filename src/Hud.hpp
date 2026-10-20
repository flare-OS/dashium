#pragma once

#include <Geode/Geode.hpp>

namespace dashium {

/// Tiny in-game stats readout, drawn with a cocos label so Dashium does not
/// pull in an overlay framework just to show four numbers.
class Hud {
public:
    static void attach(cocos2d::CCNode* parent);
    static void detach();
    static void tick(float dt);
    static void setEnabled(bool value);

private:
    static inline cocos2d::CCLabelBMFont* s_label = nullptr;
    static inline bool s_enabled = true;
    static inline float s_updateAccum = 0.f;
    static inline float s_fps = 0.f;
};

} // namespace dashium
