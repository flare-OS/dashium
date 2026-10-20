#include "Batcher.hpp"
#include "Config.hpp"
#include "Hud.hpp"

#include <Geode/binding/EnhancedGameObject.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/cocos/particle_nodes/CCParticleBatchNode.h>
#include <Geode/cocos/particle_nodes/CCParticleSystemQuad.h>
#include <Geode/modify/AnimatedGameObject.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/EnhancedGameObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/ParticleGameObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/RingObject.hpp>
#include <Geode/modify/SpecialAnimGameObject.hpp>

#include <vector>
using namespace geode::prelude;

namespace {

/// Geometry that the engine already draws through its own batch nodes is
/// where level rendering actually happens; this hook only feeds the HUD.
class $modify(DashiumBatchNode, cocos2d::CCSpriteBatchNode) {
    void draw() {
        dashium::SpriteBatcher::get().noteBatchNode(this);
        cocos2d::CCSpriteBatchNode::draw();
    }
};

/// Low detail mode removes the decorative "high detail" objects from the
/// level before they are ever created, which is the single largest win we can
/// take from Geometry Dash's object renderer.
class $modify(DashiumGameObject, GameObject) {
    static GameObject* objectFromVector(
        gd::vector<gd::string>& values,
        gd::vector<void*>& exists,
        GJBaseGameLayer* layer,
        bool lowDetail
    ) {
        if (dashium::Config::forceLdm && geode::cast::typeinfo_cast<PlayLayer*>(layer)) {
            lowDetail = true;
        }
        return GameObject::objectFromVector(values, exists, layer, lowDetail);
    }

    void playShineEffect() {
        if (dashium::Config::noOrbEffects) return;
        GameObject::playShineEffect();
    }

    void claimParticle() {
        if (dashium::Config::noEffects) return;
        GameObject::claimParticle();
    }
};

inline void hide(cocos2d::CCNode* node) {
    if (node) node->setVisible(false);
}

void stopActionsRecursive(cocos2d::CCNode* node, int depth = 0) {
    if (!node || depth > 16) return;
    node->stopAllActions();
    node->unscheduleAllSelectors();
    if (auto* children = node->getChildren()) {
        for (unsigned i = 0; i < children->count(); ++i) {
            stopActionsRecursive(static_cast<cocos2d::CCNode*>(children->objectAtIndex(i)), depth + 1);
        }
    }
}

/// Geometry Dash 2.2 does not animate objects with cocos actions; it advances
/// frames and pulse timers from its own update loop using these fields. Zeroing
/// them freezes the animation at its current frame while the object keeps its
/// normal behaviour (triggers, hitboxes, movement).
void freezeObjectAnimation(GameObject* object) {
    auto* enhanced = geode::cast::typeinfo_cast<EnhancedGameObject*>(object);
    if (!enhanced) return;

    enhanced->m_animationSpeed = 0.f;
    enhanced->m_animationShouldUseSpeed = true;
    enhanced->m_disableAnimShine = true;
    enhanced->m_disableDelayedLoop = true;
    enhanced->m_animateOnTrigger = false;
    enhanced->m_rotationSpeed = 0.f;
    enhanced->m_rotationDelta = 0.f;
    enhanced->m_rotationAnimationSpeed = 0.f;
}

/// Particle systems are the other half of effect-heavy levels: hiding the
/// systems (and the batch node that owns them) removes their overdraw without
/// touching any engine field whose offset is uncertain. The player's own
/// subtree is skipped so the trail/flame stay intact.
void hideParticleSystems(cocos2d::CCNode* node, int depth = 0, unsigned* hidden = nullptr) {
    if (!node || depth > 32) return;
    if (geode::cast::typeinfo_cast<PlayerObject*>(node)) return;

    if (geode::cast::typeinfo_cast<cocos2d::CCParticleBatchNode*>(node) ||
        geode::cast::typeinfo_cast<cocos2d::CCParticleSystemQuad*>(node)) {
        if (node->isVisible()) {
            node->setVisible(false);
            if (hidden) ++*hidden;
        }
        return;
    }

    if (auto* children = node->getChildren()) {
        for (unsigned i = 0; i < children->count(); ++i) {
            hideParticleSystems(static_cast<cocos2d::CCNode*>(children->objectAtIndex(i)), depth + 1, hidden);
        }
    }
}

std::vector<RingObject*> g_rings;
GJBaseGameLayer* g_optimizedLayer = nullptr;

void optimizeLevelObjects(GJBaseGameLayer* layer) {
    g_rings.clear();
    g_optimizedLayer = layer;

    if (dashium::Config::noEffects) {
        unsigned hidden = 0;
        hideParticleSystems(layer, 0, &hidden);
        log::debug("[Dashium] hid {} particle systems in the gameplay layer", hidden);
    }

    if (!dashium::Config::noOrbEffects && !dashium::Config::noObjectAnimations &&
        !dashium::Config::noEffects && !dashium::Config::noGlow) return;

    auto* objects = layer->m_objects;
    if (!objects) {
        log::warn("[Dashium] object optimization skipped: no object array");
        return;
    }

    bool freezeAll = dashium::Config::noObjectAnimations;
    unsigned touched = 0;
    unsigned particlesHidden = 0;
    unsigned animatedObjects = 0;

    for (unsigned i = 0; i < objects->count(); ++i) {
        auto* object = geode::cast::typeinfo_cast<GameObject*>(objects->objectAtIndex(i));
        if (!object) continue;

        if (freezeAll) {
            freezeObjectAnimation(object);
            stopActionsRecursive(object);
            ++touched;

            if (auto* animated = geode::cast::typeinfo_cast<AnimatedGameObject*>(object)) {
                ++animatedObjects;
                if (animated->m_animatedSprite) {
                    if (auto* manager = animated->m_animatedSprite->m_animationManager) {
                        manager->m_speed = 0.f;
                        manager->unscheduleAllSelectors();
                        manager->stopAllActions();
                    }
                    animated->m_animatedSprite->stopAllActions();
                    animated->m_animatedSprite->unscheduleAllSelectors();
                }
            }
        } else if (auto* ring = geode::cast::typeinfo_cast<RingObject*>(object)) {
            freezeObjectAnimation(ring);
            stopActionsRecursive(ring);
            g_rings.push_back(ring);
            ++touched;
        }

        if (dashium::Config::noEffects && geode::cast::typeinfo_cast<ParticleGameObject*>(object)) {
            object->setVisible(false);
            ++particlesHidden;
        }

        if (dashium::Config::noGlow && object->m_glowSprite) {
            object->m_glowSprite->setVisible(false);
        }

        if (dashium::Config::noEffects && object->m_particle) {
            object->m_particle->stopSystem();
            object->m_particle->setVisible(false);
            ++particlesHidden;
        }
    }

    log::debug(
        "[Dashium] animations frozen on {} of {} level objects ({} orbs/rings cached, {} particle objects hidden, {} animated sprites)",
        touched, objects->count(), g_rings.size(), particlesHidden, animatedObjects
    );
}

void applyLayerOverrides(GJBaseGameLayer* layer) {
    if (dashium::Config::noGlow) {
        hide(layer->m_glowLayerT1);
        hide(layer->m_glowLayerT2);
        hide(layer->m_glowLayerT3);
        hide(layer->m_glowLayerT4);
        hide(layer->m_glowLayerB1);
        hide(layer->m_glowLayerB2);
        hide(layer->m_glowLayerB3);
        hide(layer->m_glowLayerB4);
        hide(layer->m_glowLayerB5);
    }
}

} // namespace

/// Every sprite draw in the game funnels through here. While a gameplay layer
/// is being visited we try to fold the draw into the batcher; anything we do
/// not own is flushed first, so on-screen ordering is preserved.
class $modify(DashiumSprite, cocos2d::CCSprite) {
    void draw() {
        auto& batcher = dashium::SpriteBatcher::get();

        if (!batcher.active()) {
            cocos2d::CCSprite::draw();
            return;
        }

        if (batcher.submit(this) == dashium::SubmitResult::Consumed) {
            return;
        }

        batcher.flush();
        cocos2d::CCSprite::draw();
    }
};

/// Orb / ring activation bursts are pure eye candy; skipping them removes a
/// particle spawn per orb press without touching physics.
class $modify(DashiumRing, RingObject) {
    void spawnCircle() {
        if (dashium::Config::noOrbEffects) return;
        RingObject::spawnCircle();
    }
};

class $modify(DashiumAnimatedObject, AnimatedGameObject) {
    void updateObjectAnimation() {
        if (dashium::Config::noObjectAnimations) return;
        AnimatedGameObject::updateObjectAnimation();
    }
};

/// Geometry Dash 2.2 does not drive object animation with cocos actions: the
/// level loop calls `updateSyncedAnimation` on every animated object. Skipping
/// that call (and reporting no synced animation at all) freezes the animation
/// at its current frame while every hitbox and trigger keeps working.
class $modify(DashiumEnhanced, EnhancedGameObject) {
    bool getHasSyncedAnimation() {
        if (dashium::Config::noObjectAnimations) return false;
        if (dashium::Config::noOrbEffects && geode::cast::typeinfo_cast<RingObject*>(this)) return false;
        return EnhancedGameObject::getHasSyncedAnimation();
    }

    void updateSyncedAnimation(float totalTime, int frameIndex) {
        if (dashium::Config::noObjectAnimations) return;
        if (dashium::Config::noOrbEffects && geode::cast::typeinfo_cast<RingObject*>(this)) return;
        EnhancedGameObject::updateSyncedAnimation(totalTime, frameIndex);
    }
};

class $modify(DashiumSpecialAnim, SpecialAnimGameObject) {
    void updateSyncedAnimation(float totalTime, int frameIndex) {
        if (dashium::Config::noObjectAnimations) return;
        SpecialAnimGameObject::updateSyncedAnimation(totalTime, frameIndex);
    }
};

class $modify(DashiumParticleObject, ParticleGameObject) {
    void updateSyncedAnimation(float totalTime, int frameIndex) {
        if (dashium::Config::noObjectAnimations) return;
        ParticleGameObject::updateSyncedAnimation(totalTime, frameIndex);
    }

    void claimParticle() {
        if (dashium::Config::noEffects) return;
        ParticleGameObject::claimParticle();
    }

    void particleWasActivated() {
        if (dashium::Config::noEffects) return;
        ParticleGameObject::particleWasActivated();
    }
};

/// Small badge on the level complete screen.
class $modify(DashiumEndLevelLayer, EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();

        auto* parent = m_mainLayer ? m_mainLayer : this;
        if (parent->getChildByID("dashium-badge"_spr)) return;

        auto* label = cocos2d::CCLabelBMFont::create("Dashium used  |  Early Access", "goldFont.fnt");
        if (!label) return;

        label->setID("dashium-badge"_spr);
        label->setScale(0.35f);
        label->setOpacity(200);
        label->setAnchorPoint({ 0.f, 1.f });

        auto size = parent->getContentSize();
        if (size.width <= 0.f || size.height <= 0.f) {
            size = cocos2d::CCDirector::sharedDirector()->getWinSize();
        }
        label->setPosition({ 10.f, size.height - 6.f });
        parent->addChild(label, 100);
    }
};

/// The whole level hierarchy is visited inside GJBaseGameLayer::visit, so this
/// is the natural bracket for "a gameplay frame is being rendered".
class $modify(DashiumBaseLayer, GJBaseGameLayer) {
    void updateShaderLayer(float dt) {
        if (dashium::Config::noShaderEffects) return;
        GJBaseGameLayer::updateShaderLayer(dt);
    }

    void updateParticles(float dt) {
        if (dashium::Config::noEffects) return;
        GJBaseGameLayer::updateParticles(dt);
    }

    void visit() {
        auto& batcher = dashium::SpriteBatcher::get();
        batcher.beginFrame();
        batcher.setLevelObjects(m_objects ? m_objects->count() : 0);
        applyLayerOverrides(this);

        // The first visit happens once every object exists, so this is the
        // reliable place to stop the per-object animation loops.
        if (g_optimizedLayer != this) {
            optimizeLevelObjects(this);
        } else if (dashium::Config::noOrbEffects && !dashium::Config::noObjectAnimations) {
            // Orbs may restart their pulse when they are re-activated.
            for (auto* ring : g_rings) {
                stopActionsRecursive(ring);
            }
        }

        GJBaseGameLayer::visit();
        batcher.endFrame();
    }
};

class $modify(DashiumPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        dashium::Hud::attach(this);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        dashium::Hud::tick(dt);
    }

    void onExit() {
        g_rings.clear();
        g_optimizedLayer = nullptr;
        dashium::Hud::detach();
        dashium::SpriteBatcher::get().reset();
        PlayLayer::onExit();
    }
};
