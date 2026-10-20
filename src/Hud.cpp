#include "Hud.hpp"
#include "Batcher.hpp"

#include <fmt/format.h>

using namespace geode::prelude;

namespace dashium {

void Hud::attach(cocos2d::CCNode* parent) {
    if (!parent) return;

    if (s_label) {
        s_label->removeFromParentAndCleanup(false);
        s_label = nullptr;
    }

    s_label = cocos2d::CCLabelBMFont::create("Dashium", "bigFont.fnt");
    if (!s_label) {
        log::warn("[Dashium] could not create the stats label");
        return;
    }

    s_label->setAnchorPoint({ 0.f, 1.f });
    s_label->setScale(0.4f);
    s_label->setOpacity(190);
    s_label->setZOrder(9999);

    auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
    s_label->setPosition({ 8.f, winSize.height - 6.f });
    s_label->setVisible(s_enabled);

    parent->addChild(s_label);
    s_updateAccum = 0.f;
}

void Hud::detach() {
    if (s_label) {
        s_label->removeFromParentAndCleanup(false);
        s_label = nullptr;
    }
    s_updateAccum = 0.f;
}

void Hud::setEnabled(bool value) {
    s_enabled = value;
    if (s_label) {
        s_label->setVisible(value);
    }
}

void Hud::tick(float dt) {
    if (!s_label) return;

    if (dt > 0.f) {
        float instant = 1.f / dt;
        s_fps = s_fps <= 0.f ? instant : s_fps * 0.9f + instant * 0.1f;
    }

    s_updateAccum += dt;
    if (s_updateAccum < 0.25f) return;
    s_updateAccum = 0.f;

    auto const& stats = SpriteBatcher::get().lastFrame();
    std::uint32_t quads = stats.quads + stats.batchQuads;
    std::uint32_t draws = stats.draws + stats.batchDraws;

    auto text = fmt::format(
        "{:.0f} fps  |  {} objects  |  {} quads  |  {} draws",
        s_fps, stats.levelObjects, quads, draws
    );
    s_label->setString(text.c_str());
}

} // namespace dashium
