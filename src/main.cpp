#include "Batcher.hpp"
#include "Config.hpp"
#include "Hud.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace {

void applySettings() {
    auto* mod = Mod::get();
    auto& batcher = dashium::SpriteBatcher::get();

    batcher.setEnabled(mod->getSettingValue<bool>("enabled"));
    batcher.setCullEnabled(mod->getSettingValue<bool>("cull-offscreen"));
    batcher.setMaxQuads(static_cast<std::uint32_t>(mod->getSettingValue<int64_t>("max-quads")));
    dashium::Hud::setEnabled(mod->getSettingValue<bool>("show-hud"));

    dashium::Config::forceLdm = mod->getSettingValue<bool>("force-ldm");
    dashium::Config::noGlow = mod->getSettingValue<bool>("no-glow");
    dashium::Config::noOrbEffects = mod->getSettingValue<bool>("orb-effects");
    dashium::Config::noObjectAnimations = mod->getSettingValue<bool>("object-animations");
    dashium::Config::noEffects = mod->getSettingValue<bool>("effect-layers");
    dashium::Config::noShaderEffects = mod->getSettingValue<bool>("shader-effects");
}

} // namespace

$on_mod(Loaded) {
    applySettings();

    listenForSettingChanges<bool>("enabled", [](bool value) {
        dashium::SpriteBatcher::get().setEnabled(value);
    });
    listenForSettingChanges<bool>("cull-offscreen", [](bool value) {
        dashium::SpriteBatcher::get().setCullEnabled(value);
    });
    listenForSettingChanges<bool>("show-hud", [](bool value) {
        dashium::Hud::setEnabled(value);
    });
    listenForSettingChanges<int64_t>("max-quads", [](int64_t value) {
        dashium::SpriteBatcher::get().setMaxQuads(static_cast<std::uint32_t>(value));
    });
    listenForSettingChanges<bool>("force-ldm", [](bool value) {
        dashium::Config::forceLdm = value;
    });
    listenForSettingChanges<bool>("no-glow", [](bool value) {
        dashium::Config::noGlow = value;
    });
    listenForSettingChanges<bool>("orb-effects", [](bool value) {
        dashium::Config::noOrbEffects = value;
    });
    listenForSettingChanges<bool>("object-animations", [](bool value) {
        dashium::Config::noObjectAnimations = value;
    });
    listenForSettingChanges<bool>("effect-layers", [](bool value) {
        dashium::Config::noEffects = value;
    });
    listenForSettingChanges<bool>("shader-effects", [](bool value) {
        dashium::Config::noShaderEffects = value;
    });

    log::info(
        "[Dashium] loaded (batching={}, culling={}, force-ldm={}, no-glow={}, orb-effects={}, object-animations={}, effects={}, shaders={}, max quads={})",
        Mod::get()->getSettingValue<bool>("enabled") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("cull-offscreen") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("force-ldm") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("no-glow") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("orb-effects") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("object-animations") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("effect-layers") ? "on" : "off",
        Mod::get()->getSettingValue<bool>("shader-effects") ? "on" : "off",
        Mod::get()->getSettingValue<int64_t>("max-quads")
    );
}
