#include "Batcher.hpp"

#include <Geode/binding/GameObject.hpp>
#include <Geode/cocos/kazmath/include/kazmath/GL/matrix.h>
#include <Geode/cocos/shaders/CCGLProgram.h>
#include <Geode/cocos/shaders/CCShaderCache.h>
#include <Geode/cocos/shaders/ccGLStateCache.h>

#include <cstddef>
#include <cstring>

using namespace geode::prelude;

namespace dashium {

namespace {

float kIdentity[16] = {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f,
};

inline bool isFullyTransparent(cocos2d::ccV3F_C4B_T2F_Quad const& q) {
    return q.tl.colors.a == 0 && q.bl.colors.a == 0 &&
           q.tr.colors.a == 0 && q.br.colors.a == 0;
}

} // namespace

SpriteBatcher& SpriteBatcher::get() {
    static SpriteBatcher instance;
    return instance;
}

void SpriteBatcher::setEnabled(bool value) {
    if (m_enabled == value) return;
    m_enabled = value;
    if (!value) {
        flush();
        m_active = false;
    }
}

void SpriteBatcher::setMaxQuads(std::uint32_t value) {
    m_maxQuads = std::clamp<std::uint32_t>(value, 512, 65536);
    m_vertices.reserve(static_cast<std::size_t>(m_maxQuads) * 6);
    flush();
}

void SpriteBatcher::beginFrame() {
    m_frame = {};
    m_stateValid = false;
    m_vertices.clear();

    if (!m_enabled) {
        m_active = false;
        return;
    }

    auto* program = cocos2d::CCShaderCache::sharedShaderCache()->programForKey(
        kCCShader_PositionTextureColor
    );
    m_defaultProgram = program ? program->getProgram() : 0;
    if (!m_defaultProgram) {
        m_active = false;
        return;
    }

    m_vertices.reserve(static_cast<std::size_t>(m_maxQuads) * 6);
    m_active = true;
}

void SpriteBatcher::endFrame() {
    flush();
    m_active = false;
    m_last = m_frame;
    m_stateValid = false;

    if (m_frame.objects > 0) {
        if (!m_loggedSuccess && m_frame.quads > 0) {
            m_loggedSuccess = true;
            log::info(
                "[Dashium] first batched frame: {} objects -> {} quads in {} draws ({} culled, {} passed through)",
                m_frame.objects, m_frame.quads, m_frame.draws, m_frame.culled, m_frame.passThrough
            );
        }
        if (!m_warnedShaders && m_frame.quads == 0 && m_frame.passThrough > 0) {
            m_warnedShaders = true;
            log::warn(
                "[Dashium] {} objects reached the batcher but none used the default sprite shader; batching is a no-op",
                m_frame.passThrough
            );
        }
    }

    if (++m_logTimer >= 600) {
        m_logTimer = 0;
        log::debug(
            "[Dashium] stats: {} level objects | {} sprites | {} own quads in {} draws | batch nodes: {} quads in {} draws | {} culled",
            m_frame.levelObjects, m_frame.objects, m_frame.quads, m_frame.draws,
            m_frame.batchQuads, m_frame.batchDraws, m_frame.culled
        );
    }
}

void SpriteBatcher::noteBatchNode(cocos2d::CCSpriteBatchNode* node) {
    if (!m_active || !node) return;

    ++m_frame.batchDraws;
    if (auto* atlas = node->getTextureAtlas()) {
        m_frame.batchQuads += atlas->getTotalQuads();
    }
}


void SpriteBatcher::reset() {
    flush();
    m_vertices.clear();
    m_frame = {};
    m_last = {};
    m_stateValid = false;
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    m_vboTried = false;
    m_useVbo = false;
    m_active = false;
}

bool SpriteBatcher::ensureVbo() {
    if (m_useVbo) return true;
    if (m_vboTried) return false;
    m_vboTried = true;

    if (!glGenBuffers) {
        log::warn("[Dashium] glGenBuffers unavailable, falling back to client arrays");
        return false;
    }

    glGenBuffers(1, &m_vbo);
    if (m_vbo == 0) {
        log::warn("[Dashium] could not allocate a vertex buffer, falling back to client arrays");
        return false;
    }

    m_useVbo = true;
    log::debug("[Dashium] batcher using VBO {}", m_vbo);
    return true;
}

SubmitResult SpriteBatcher::submit(cocos2d::CCSprite* sprite) {
    if (!m_active || !m_enabled || !sprite) {
        return SubmitResult::PassThrough;
    }

    auto* object = geode::cast::typeinfo_cast<GameObject*>(sprite);
    if (!object) {
        return SubmitResult::PassThrough;
    }

    ++m_frame.objects;

    auto* texture = sprite->getTexture();
    auto* program = sprite->getShaderProgram();
    if (!texture || !program || program->getProgram() != m_defaultProgram) {
        ++m_frame.passThrough;
        return SubmitResult::PassThrough;
    }

    auto quad = sprite->getQuad();
    if (isFullyTransparent(quad)) {
        ++m_frame.culled;
        return SubmitResult::Consumed;
    }

    kmMat4 projection, modelview, mvp;
    kmGLGetMatrix(KM_GL_PROJECTION, &projection);
    kmGLGetMatrix(KM_GL_MODELVIEW, &modelview);
    kmMat4Multiply(&mvp, &projection, &modelview);

    const float* m = mvp.mat;

    // Project the four corners of the sprite quad into normalized device
    // coordinates. This is exactly the transform the engine's own vertex
    // shader would apply, so the output is pixel-identical.
    float nx[4], ny[4];
    cocos2d::ccV3F_C4B_T2F const* corners[4] = { &quad.tl, &quad.bl, &quad.tr, &quad.br };
    for (int i = 0; i < 4; ++i) {
        float x = corners[i]->vertices.x;
        float y = corners[i]->vertices.y;
        float clipX = m[0] * x + m[4] * y + m[12];
        float clipY = m[1] * x + m[5] * y + m[13];
        float clipW = m[3] * x + m[7] * y + m[15];
        if (clipW == 0.f) {
            return SubmitResult::PassThrough;
        }
        nx[i] = clipX / clipW;
        ny[i] = clipY / clipW;
    }

    if (m_cull) {
        bool left = true, right = true, bottom = true, top = true;
        for (int i = 0; i < 4; ++i) {
            if (nx[i] > -1.f) left = false;
            if (nx[i] < 1.f) right = false;
            if (ny[i] > -1.f) bottom = false;
            if (ny[i] < 1.f) top = false;
        }
        if (left || right || bottom || top) {
            ++m_frame.culled;
            return SubmitResult::Consumed;
        }
    }

    auto blend = sprite->getBlendFunc();
    void* parent = sprite->getParent();
    int z = sprite->getZOrder();

    if (m_stateValid &&
        (m_program != program || m_texture != texture->getName() ||
         m_blendSrc != blend.src || m_blendDst != blend.dst ||
         m_parent != parent || z < m_zOrder)) {
        flush();
    }

    m_program = program;
    m_texture = texture->getName();
    m_blendSrc = blend.src;
    m_blendDst = blend.dst;
    m_parent = parent;
    m_zOrder = z;
    m_stateValid = true;

    m_vertices.reserve(m_vertices.size() + 6);

    auto push = [&](cocos2d::ccV3F_C4B_T2F const& v) {
        float x = v.vertices.x;
        float y = v.vertices.y;
        float clipX = m[0] * x + m[4] * y + m[12];
        float clipY = m[1] * x + m[5] * y + m[13];
        float clipW = m[3] * x + m[7] * y + m[15];

        BatchVertex out;
        out.x = clipX / clipW;
        out.y = clipY / clipW;
        out.z = 0.f;
        out.r = v.colors.r;
        out.g = v.colors.g;
        out.b = v.colors.b;
        out.a = v.colors.a;
        out.u = v.texCoords.u;
        out.v = v.texCoords.v;
        m_vertices.push_back(out);
    };

    // Same winding as the engine's GL_TRIANGLE_STRIP (tl, bl, tr, br).
    push(quad.tl);
    push(quad.bl);
    push(quad.tr);

    push(quad.tr);
    push(quad.bl);
    push(quad.br);

    ++m_frame.quads;

    if (m_vertices.size() >= static_cast<std::size_t>(m_maxQuads) * 6) {
        flush();
    }

    return SubmitResult::Consumed;
}

void SpriteBatcher::flush() {
    if (m_vertices.empty()) return;

    auto* program = m_program;
    GLuint textureId = m_texture;
    GLenum blendSrc = m_blendSrc;
    GLenum blendDst = m_blendDst;

    m_stateValid = false;

    if (!program || !textureId) {
        m_vertices.clear();
        return;
    }

    // Bind through the engine's state cache: the program must stay in sync
    // with cocos2d's cached "current program" or later draws would be skipped.
    ccGLUseProgram(program->getProgram());
    program->setUniformsForBuiltins();

    // The quads were already baked into clip space, so the engine's
    // projection/modellview matrices must be neutralized for this draw.
    if (program->m_uUniforms[cocos2d::kCCUniformPMatrix] != -1) {
        program->setUniformLocationWithMatrix4fv(
            program->m_uUniforms[cocos2d::kCCUniformPMatrix],
            kIdentity, 1
        );
    }
    if (program->m_uUniforms[cocos2d::kCCUniformMVMatrix] != -1) {
        program->setUniformLocationWithMatrix4fv(
            program->m_uUniforms[cocos2d::kCCUniformMVMatrix],
            kIdentity, 1
        );
    }
    if (program->m_uUniforms[cocos2d::kCCUniformMVPMatrix] != -1) {
        program->setUniformLocationWithMatrix4fv(
            program->m_uUniforms[cocos2d::kCCUniformMVPMatrix],
            kIdentity, 1
        );
    }

    ccGLBlendFunc(blendSrc, blendDst);
    ccGLBindTexture2D(textureId);
    ccGLEnableVertexAttribs(kCCVertexAttribFlag_PosColorTex);

    const GLsizei vertexCount = static_cast<GLsizei>(m_vertices.size());
    const GLsizei stride = static_cast<GLsizei>(sizeof(BatchVertex));

    bool useVbo = ensureVbo();
    if (useVbo) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(m_vertices.size() * sizeof(BatchVertex)),
            m_vertices.data(),
            GL_STREAM_DRAW
        );
        glVertexAttribPointer(
            cocos2d::kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void const*>(offsetof(BatchVertex, x))
        );
        glVertexAttribPointer(
            cocos2d::kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
            reinterpret_cast<void const*>(offsetof(BatchVertex, r))
        );
        glVertexAttribPointer(
            cocos2d::kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void const*>(offsetof(BatchVertex, u))
        );
    } else {
        auto const* base = m_vertices.data();
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(
            cocos2d::kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, stride,
            &base->x
        );
        glVertexAttribPointer(
            cocos2d::kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
            &base->r
        );
        glVertexAttribPointer(
            cocos2d::kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, stride,
            &base->u
        );
    }

    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    if (useVbo) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    m_vertices.clear();

    ++m_frame.draws;
}

} // namespace dashium
