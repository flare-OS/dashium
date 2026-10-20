#pragma once

#include <Geode/Geode.hpp>

#include <cstdint>
#include <vector>

namespace dashium {

/// Interleaved vertex layout, byte-for-byte compatible with cocos2d's
/// ccV3F_C4B_T2F (position, packed color, texcoord = 24 bytes).
struct BatchVertex {
    float x, y, z;
    std::uint8_t r, g, b, a;
    float u, v;
};

static_assert(sizeof(BatchVertex) == 24, "BatchVertex must match ccV3F_C4B_T2F");

enum class SubmitResult {
    /// The draw was folded into the current batch (or culled outright).
    Consumed,
    /// Not handled; the caller must flush and run the original draw.
    PassThrough,
};

struct FrameStats {
    /// GameObjects drawn outside the engine's batch nodes (players, etc).
    std::uint32_t objects = 0;
    /// Quads Dashium submitted itself.
    std::uint32_t quads = 0;
    /// Draw calls Dashium issued for those quads.
    std::uint32_t draws = 0;
    std::uint32_t culled = 0;
    std::uint32_t passThrough = 0;

    /// Quads already sitting in the engine's CCSpriteBatchNodes this frame.
    std::uint32_t batchQuads = 0;
    /// CCSpriteBatchNode draw calls this frame.
    std::uint32_t batchDraws = 0;
    /// Objects in the level's object array (LDM shrinks this).
    std::uint32_t levelObjects = 0;
};

/// Sodium-style immediate-mode batcher for cocos2d sprites.
///
/// Every sprite that takes part in the batch is transformed into clip space
/// once, then all consecutive quads that share a texture / program / blend /
/// z-layer are emitted with a single glDrawArrays. Draw order is never
/// reordered: the batch is flushed the moment the render state changes, so
/// alpha blending behaves exactly like the unbatched renderer.
class SpriteBatcher {
public:
    static SpriteBatcher& get();

    void setEnabled(bool value);
    bool enabled() const { return m_enabled; }
    void setCullEnabled(bool value) { m_cull = value; }
    void setMaxQuads(std::uint32_t value);

    void beginFrame();
    void endFrame();
    void flush();
    void reset();

    SubmitResult submit(cocos2d::CCSprite* sprite);

    /// Called from the CCSpriteBatchNode::draw hook so the stats reflect the
    /// geometry the engine's own batch nodes push every frame.
    void noteBatchNode(cocos2d::CCSpriteBatchNode* node);

    void setLevelObjects(std::uint32_t count) { m_frame.levelObjects = count; }

    FrameStats const& currentFrame() const { return m_frame; }
    FrameStats const& lastFrame() const { return m_last; }

    bool active() const { return m_active; }

private:
    SpriteBatcher() = default;

    void pushQuad(cocos2d::CCSprite* sprite);
    bool ensureVbo();

    bool m_enabled = true;
    bool m_cull = true;
    bool m_active = false;
    bool m_vboTried = false;
    bool m_useVbo = false;

    std::uint32_t m_maxQuads = 8192;

    std::vector<BatchVertex> m_vertices;

    GLuint m_vbo = 0;

    // Current batch render state. When any of these changes we flush.
    cocos2d::CCGLProgram* m_program = nullptr;
    GLuint m_texture = 0;
    GLenum m_blendSrc = 0;
    GLenum m_blendDst = 0;
    void* m_parent = nullptr;
    int m_zOrder = 0;
    bool m_stateValid = false;

    // The engine's default sprite program, resolved once per frame. We only
    // batch draws that use it: custom shaders may depend on uniforms we would
    // not be able to reproduce.
    std::uint32_t m_defaultProgram = 0;

    FrameStats m_frame;
    FrameStats m_last;

    bool m_loggedSuccess = false;
    bool m_warnedShaders = false;
    std::uint32_t m_logTimer = 0;
};

} // namespace dashium
