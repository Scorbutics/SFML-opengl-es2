////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2023 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <map>


// GL_QUADS is unavailable on OpenGL ES, thus we need to define GL_QUADS ourselves
#ifndef GL_QUADS

    #define GL_QUADS 0

#endif // GL_QUADS


namespace
{
    // A nested named namespace is used here to allow unity builds of SFML.
    namespace RenderTargetImpl
    {
        // Mutex to protect ID generation and our context-RenderTarget-map
        sf::Mutex mutex;

        // Unique identifier, used for identifying RenderTargets when
        // tracking the currently active RenderTarget within a given context
        sf::Uint64 getUniqueId()
        {
            sf::Lock lock(mutex);

            static sf::Uint64 id = 1; // start at 1, zero is "no RenderTarget"

            return id++;
        }

        // Map to help us detect whether a different RenderTarget
        // has been activated within a single context
        typedef std::map<sf::Uint64, sf::Uint64> ContextRenderTargetMap;
        ContextRenderTargetMap contextRenderTargetMap;

        // Check if a RenderTarget with the given ID is active in the current context
        bool isActive(sf::Uint64 id)
        {
            ContextRenderTargetMap::iterator iter = contextRenderTargetMap.find(sf::Context::getActiveContextId());

            if ((iter == contextRenderTargetMap.end()) || (iter->second != id))
                return false;

            return true;
        }

        // Convert an sf::BlendMode::Factor constant to the corresponding OpenGL constant.
        sf::Uint32 factorToGlConstant(sf::BlendMode::Factor blendFactor)
        {
            switch (blendFactor)
            {
                case sf::BlendMode::Zero:             return GL_ZERO;
                case sf::BlendMode::One:              return GL_ONE;
                case sf::BlendMode::SrcColor:         return GL_SRC_COLOR;
                case sf::BlendMode::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
                case sf::BlendMode::DstColor:         return GL_DST_COLOR;
                case sf::BlendMode::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
                case sf::BlendMode::SrcAlpha:         return GL_SRC_ALPHA;
                case sf::BlendMode::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
                case sf::BlendMode::DstAlpha:         return GL_DST_ALPHA;
                case sf::BlendMode::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
            }

            sf::err() << "Invalid value for sf::BlendMode::Factor! Fallback to sf::BlendMode::Zero." << std::endl;
            assert(false);
            return GL_ZERO;
        }


        // Convert an sf::BlendMode::BlendEquation constant to the corresponding OpenGL constant.
        sf::Uint32 equationToGlConstant(sf::BlendMode::Equation blendEquation)
        {
            switch (blendEquation)
            {
                case sf::BlendMode::Add:
                    return GLEXT_GL_FUNC_ADD;
                case sf::BlendMode::Subtract:
                    if (GLEXT_blend_subtract)
                        return GLEXT_GL_FUNC_SUBTRACT;
                    break;
                case sf::BlendMode::ReverseSubtract:
                    if (GLEXT_blend_subtract)
                        return GLEXT_GL_FUNC_REVERSE_SUBTRACT;
                    break;
                case sf::BlendMode::Min:
                    if (GLEXT_blend_minmax)
                        return GLEXT_GL_MIN;
                    break;
                case sf::BlendMode::Max:
                    if (GLEXT_blend_minmax)
                        return GLEXT_GL_MAX;
                    break;
            }

            static bool warned = false;
            if (!warned)
            {
                sf::err() << "OpenGL extension EXT_blend_minmax or EXT_blend_subtract unavailable" << std::endl;
                sf::err() << "Some blending equations will fallback to sf::BlendMode::Add" << std::endl;
                sf::err() << "Ensure that hardware acceleration is enabled if available" << std::endl;

                warned = true;
            }

            return GLEXT_GL_FUNC_ADD;
        }
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
RenderTarget::RenderTarget() :
m_defaultView(),
m_view       (),
m_cache      (),
m_id         (0)
{
    m_cache.glStatesSet = false;
    m_cache.programChanged = 0;
    m_cache.posAttrib = -1;
    m_cache.colAttrib = -1;
    m_cache.texAttrib = -1;
}


////////////////////////////////////////////////////////////
RenderTarget::~RenderTarget()
{
}


////////////////////////////////////////////////////////////
void RenderTarget::clear(const Color& color)
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Unbind texture to fix RenderTexture preventing clear
        applyTexture(NULL);

        glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
        glCheck(glClear(GL_COLOR_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::setView(const View& view)
{
    m_view = view;
    m_cache.viewChanged = true;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getView() const
{
    return m_view;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getDefaultView() const
{
    return m_defaultView;
}


////////////////////////////////////////////////////////////
IntRect RenderTarget::getViewport(const View& view) const
{
    float width  = static_cast<float>(getSize().x);
    float height = static_cast<float>(getSize().y);
    const FloatRect& viewport = view.getViewport();

    return IntRect(static_cast<int>(0.5f + width  * viewport.left),
                   static_cast<int>(0.5f + height * viewport.top),
                   static_cast<int>(0.5f + width  * viewport.width),
                   static_cast<int>(0.5f + height * viewport.height));
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point) const
{
    return mapPixelToCoords(point, getView());
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point, const View& view) const
{
    // First, convert from viewport coordinates to homogeneous coordinates
    Vector2f normalized;
    FloatRect viewport = FloatRect(getViewport(view));
    normalized.x = -1.f + 2.f * (static_cast<float>(point.x) - viewport.left) / viewport.width;
    normalized.y =  1.f - 2.f * (static_cast<float>(point.y) - viewport.top)  / viewport.height;

    // Then transform by the inverse of the view matrix
    return view.getInverseTransform().transformPoint(normalized);
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point) const
{
    return mapCoordsToPixel(point, getView());
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point, const View& view) const
{
    // First, transform the point by the view matrix
    Vector2f normalized = view.getTransform().transformPoint(point);

    // Then convert to viewport coordinates
    Vector2i pixel;
    FloatRect viewport = FloatRect(getViewport(view));
    pixel.x = static_cast<int>(( normalized.x + 1.f) / 2.f * viewport.width  + viewport.left);
    pixel.y = static_cast<int>((-normalized.y + 1.f) / 2.f * viewport.height + viewport.top);

    return pixel;
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states)
{
    drawable.draw(*this, states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Vertex* vertices, std::size_t vertexCount,
                        PrimitiveType type, const RenderStates& states)
{
    // Nothing to draw?
    if (!vertices || (vertexCount == 0))
        return;

    // GL_QUADS is unavailable on OpenGL ES
    #ifdef SFML_OPENGL_ES
        if (type == Quads)
        {
            err() << "sf::Quads primitive type is not supported on OpenGL ES platforms, drawing skipped" << std::endl;
            return;
        }
    #endif

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Check if the vertex count is low enough so that we can pre-transform them
        bool useVertexCache = (vertexCount <= StatesCache::VertexCacheSize);

        if (useVertexCache)
        {
            // Pre-transform the vertices and store them into the vertex cache
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                Vertex& vertex = m_cache.vertexCache[i];
                vertex.position = states.transform * vertices[i].position;
                vertex.color = vertices[i].color;
                vertex.texCoords = vertices[i].texCoords;
            }
        }

        setupDraw(useVertexCache, states);

#ifndef SFML_OPENGL_ES

        // Check if texture coordinates array is needed, and update client state accordingly
        bool enableTexCoordsArray = (states.texture || states.shader);
        if (!m_cache.enable || (enableTexCoordsArray != m_cache.texCoordsArrayEnabled))
        {
            if (enableTexCoordsArray)
                glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
            else
                glCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
        }

        // If we switch between non-cache and cache mode or enable texture
        // coordinates we need to set up the pointers to the vertices' components
        if (!m_cache.enable || !useVertexCache || !m_cache.useVertexCache)
        {
            const char* data = reinterpret_cast<const char*>(vertices);

            // If we pre-transform the vertices, we must use our internal vertex cache
            if (useVertexCache)
                data = reinterpret_cast<const char*>(m_cache.vertexCache);

            glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), data + 0));
            glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), data + 8));
            if (enableTexCoordsArray)
                glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), data + 12));
        }
        else if (enableTexCoordsArray && !m_cache.texCoordsArrayEnabled)
        {
            // If we enter this block, we are already using our internal vertex cache
            const char* data = reinterpret_cast<const char*>(m_cache.vertexCache);

            glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), data + 12));
        }

#else

        // Check if texture coordinates array is needed, and update client state accordingly
        bool enableTexCoordsArray = (states.texture || states.shader);

        // If we switch between non-cache and cache mode or enable texture
        // coordinates we need to set up the pointers to the vertices' components
        {
            const char* data = reinterpret_cast<const char*>(vertices);

            // If we pre-transform the vertices, we must use our internal vertex cache
            if (useVertexCache)
                data = reinterpret_cast<const char*>(m_cache.vertexCache);

            if (m_cache.posAttrib >= 0)
                glCheck(glVertexAttribPointer(m_cache.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), data + 0));
            if (m_cache.colAttrib >= 0)
                glCheck(glVertexAttribPointer(m_cache.colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), data + 8));
            if (enableTexCoordsArray)
                if (m_cache.texAttrib >= 0)
                    glCheck(glVertexAttribPointer(m_cache.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), data + 12));
        }

#endif

        drawPrimitives(type, 0, vertexCount);
        cleanupDraw(states);

        // Update the cache
        m_cache.useVertexCache = useVertexCache;
        m_cache.texCoordsArrayEnabled = enableTexCoordsArray;
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, const RenderStates& states)
{
    draw(vertexBuffer, 0, vertexBuffer.getVertexCount(), states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, std::size_t firstVertex,
                        std::size_t vertexCount, const RenderStates& states)
{
    // VertexBuffer not supported?
    if (!VertexBuffer::isAvailable())
    {
        err() << "sf::VertexBuffer is not available, drawing skipped" << std::endl;
        return;
    }

    // Sanity check
    if (firstVertex > vertexBuffer.getVertexCount())
        return;

    // Clamp vertexCount to something that makes sense
    vertexCount = std::min(vertexCount, vertexBuffer.getVertexCount() - firstVertex);

    // Nothing to draw?
    if (!vertexCount || !vertexBuffer.getNativeHandle())
        return;

    // GL_QUADS is unavailable on OpenGL ES
    #ifdef SFML_OPENGL_ES
        if (vertexBuffer.getPrimitiveType() == Quads)
        {
            err() << "sf::Quads primitive type is not supported on OpenGL ES platforms, drawing skipped" << std::endl;
            return;
        }
    #endif

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        setupDraw(false, states);

        // Bind vertex buffer
        VertexBuffer::bind(&vertexBuffer);

#ifndef SFML_OPENGL_ES

        // Always enable texture coordinates
        if (!m_cache.enable || !m_cache.texCoordsArrayEnabled)
            glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));

        glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), reinterpret_cast<const void*>(0)));
        glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
        glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), reinterpret_cast<const void*>(12)));

#else

        if (!m_cache.enable || !m_cache.texCoordsArrayEnabled)
            if (m_cache.texAttrib >= 0)
                glCheck(glEnableVertexAttribArray(m_cache.texAttrib));

        if (m_cache.posAttrib >= 0)
            glCheck(glVertexAttribPointer(m_cache.posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(0)));
        if (m_cache.colAttrib >= 0)
            glCheck(glVertexAttribPointer(m_cache.colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
        if (m_cache.texAttrib >= 0)
            glCheck(glVertexAttribPointer(m_cache.texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(12)));

#endif

        drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);

        // Unbind vertex buffer
        VertexBuffer::bind(NULL);

        cleanupDraw(states);

        // Update the cache
        m_cache.useVertexCache = false;
        m_cache.texCoordsArrayEnabled = true;
    }
}


////////////////////////////////////////////////////////////
bool RenderTarget::isSrgb() const
{
    // By default sRGB encoding is not enabled for an arbitrary RenderTarget
    return false;
}


////////////////////////////////////////////////////////////
bool RenderTarget::setActive(bool active)
{
    // Mark this RenderTarget as active or no longer active in the tracking map
    {
        sf::Lock lock(RenderTargetImpl::mutex);

        Uint64 contextId = Context::getActiveContextId();

        using RenderTargetImpl::contextRenderTargetMap;
        RenderTargetImpl::ContextRenderTargetMap::iterator iter = contextRenderTargetMap.find(contextId);

        if (active)
        {
            if (iter == contextRenderTargetMap.end())
            {
                contextRenderTargetMap[contextId] = m_id;

                m_cache.glStatesSet = false;
                m_cache.enable = false;
            }
            else if (iter->second != m_id)
            {
                iter->second = m_id;

                m_cache.enable = false;
            }
        }
        else
        {
            if (iter != contextRenderTargetMap.end())
                contextRenderTargetMap.erase(iter);

            m_cache.enable = false;
        }
    }

    return true;
}


////////////////////////////////////////////////////////////
void RenderTarget::pushGLStates()
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        #ifdef SFML_DEBUG
            // make sure that the user didn't leave an unchecked OpenGL error
            GLenum error = glGetError();
            if (error != GL_NO_ERROR)
            {
                err() << "OpenGL error (" << error << ") detected in user code, "
                      << "you should check for errors with glGetError()"
                      << std::endl;
            }
        #endif

        #ifndef SFML_OPENGL_ES
            glCheck(glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS));
            glCheck(glPushAttrib(GL_ALL_ATTRIB_BITS));
            glCheck(glMatrixMode(GL_MODELVIEW));
            glCheck(glPushMatrix());
            glCheck(glMatrixMode(GL_PROJECTION));
            glCheck(glPushMatrix());
            glCheck(glMatrixMode(GL_TEXTURE));
            glCheck(glPushMatrix());
        #endif
    }

    resetGLStates();
}


////////////////////////////////////////////////////////////
void RenderTarget::popGLStates()
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        #ifndef SFML_OPENGL_ES
            glCheck(glMatrixMode(GL_PROJECTION));
            glCheck(glPopMatrix());
            glCheck(glMatrixMode(GL_MODELVIEW));
            glCheck(glPopMatrix());
            glCheck(glMatrixMode(GL_TEXTURE));
            glCheck(glPopMatrix());
            glCheck(glPopClientAttrib());
            glCheck(glPopAttrib());
        #endif
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::resetGLStates()
{
    // Check here to make sure a context change does not happen after activate(true)
    bool shaderAvailable = Shader::isAvailable();
    bool vertexBufferAvailable = VertexBuffer::isAvailable();

    // Workaround for states not being properly reset on
    // macOS unless a context switch really takes place
    #if defined(SFML_SYSTEM_MACOS)
        setActive(false);
    #endif

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Make sure that extensions are initialized
        priv::ensureExtensionsInit();

        // Make sure that the texture unit which is active is the number 0
#ifndef SFML_OPENGL_ES
        if (GLEXT_multitexture)
        {
            glCheck(GLEXT_glClientActiveTexture(GLEXT_GL_TEXTURE0));
            glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
        }
#else
        if (GLEXT_multitexture)
        {
            glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
        }
#endif

        // Define the default OpenGL states
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glEnable(GL_BLEND));

#ifndef SFML_OPENGL_ES
        glCheck(glDisable(GL_ALPHA_TEST));
        glCheck(glDisable(GL_LIGHTING));
        glCheck(glEnable(GL_TEXTURE_2D));
        glCheck(glMatrixMode(GL_MODELVIEW));
        glCheck(glLoadIdentity());
        glCheck(glEnableClientState(GL_VERTEX_ARRAY));
        glCheck(glEnableClientState(GL_COLOR_ARRAY));
        glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
#else
        if (m_cache.posAttrib >= 0)
            glCheck(glDisableVertexAttribArray(m_cache.posAttrib));
        if (m_cache.colAttrib >= 0)
            glCheck(glDisableVertexAttribArray(m_cache.colAttrib));
        if (m_cache.texAttrib >= 0)
            glCheck(glDisableVertexAttribArray(m_cache.texAttrib));
#endif

        m_cache.glStatesSet = true;

        // Apply the default SFML states
        applyBlendMode(BlendAlpha);
        applyTexture(NULL);
        if (shaderAvailable)
            applyShader(NULL);

        if (vertexBufferAvailable)
            glCheck(VertexBuffer::bind(NULL));

        m_cache.texCoordsArrayEnabled = true;

        m_cache.useVertexCache = false;

        // Set the default view
        setView(getView());

        m_cache.enable = true;
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::initialize()
{
    // Setup the default and current views
    m_defaultView.reset(FloatRect(0, 0, static_cast<float>(getSize().x), static_cast<float>(getSize().y)));
    m_view = m_defaultView;

    // Set GL states only on first draw, so that we don't pollute user's states
    m_cache.glStatesSet = false;

    // Generate a unique ID for this RenderTarget to track
    // whether it is active within a specific context
    m_id = RenderTargetImpl::getUniqueId();
}


////////////////////////////////////////////////////////////
void RenderTarget::applyCurrentView()
{
    // Set the viewport
    IntRect viewport = getViewport(m_view);
    int top = static_cast<int>(getSize().y) - (viewport.top + viewport.height);
    glCheck(glViewport(viewport.left, top, viewport.width, viewport.height));

    // Set the projection matrix
    glCheck(glMatrixMode(GL_PROJECTION));
    glCheck(glLoadMatrixf(m_view.getTransform().getMatrix()));

    // Go back to model-view mode
    glCheck(glMatrixMode(GL_MODELVIEW));

    m_cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyBlendMode(const BlendMode& mode)
{
    using RenderTargetImpl::factorToGlConstant;
    using RenderTargetImpl::equationToGlConstant;

    // Apply the blend mode, falling back to the non-separate versions if necessary
    if (GLEXT_blend_func_separate)
    {
        glCheck(GLEXT_glBlendFuncSeparate(
            factorToGlConstant(mode.colorSrcFactor), factorToGlConstant(mode.colorDstFactor),
            factorToGlConstant(mode.alphaSrcFactor), factorToGlConstant(mode.alphaDstFactor)));
    }
    else
    {
        glCheck(glBlendFunc(
            factorToGlConstant(mode.colorSrcFactor),
            factorToGlConstant(mode.colorDstFactor)));
    }

    if (GLEXT_blend_minmax || GLEXT_blend_subtract)
    {
        if (GLEXT_blend_equation_separate)
        {
            glCheck(GLEXT_glBlendEquationSeparate(
                equationToGlConstant(mode.colorEquation),
                equationToGlConstant(mode.alphaEquation)));
        }
        else
        {
            glCheck(GLEXT_glBlendEquation(equationToGlConstant(mode.colorEquation)));
        }
    }
    else if ((mode.colorEquation != BlendMode::Add) || (mode.alphaEquation != BlendMode::Add))
    {
        static bool warned = false;

        if (!warned)
        {
#ifdef SFML_OPENGL_ES
            err() << "OpenGL ES extension OES_blend_subtract unavailable" << std::endl;
#else
            err() << "OpenGL extension EXT_blend_minmax and EXT_blend_subtract unavailable" << std::endl;
#endif
            err() << "Selecting a blend equation not possible" << std::endl;
            err() << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

    m_cache.lastBlendMode = mode;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyTransform(const Transform& transform)
{
    // No need to call glMatrixMode(GL_MODELVIEW), it is always the
    // current mode (for optimization purpose, since it's the most used)
    if (transform == Transform::Identity)
        glCheck(glLoadIdentity());
    else
        glCheck(glLoadMatrixf(transform.getMatrix()));
}


////////////////////////////////////////////////////////////
void RenderTarget::applyTexture(const Texture* texture)
{
    Texture::bind(texture, Texture::Pixels);

    m_cache.lastTextureId = texture ? texture->m_cacheId : 0;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyShader(const Shader* shader)
{
    Shader::bind(shader);
}


////////////////////////////////////////////////////////////
void RenderTarget::setupDraw(bool useVertexCache, const RenderStates& states)
{
    // Enable or disable sRGB encoding
    // This is needed for drivers that do not check the format of the surface drawn to before applying sRGB conversion
    if (!m_cache.enable)
    {
        if (isSrgb())
            glCheck(glEnable(GL_FRAMEBUFFER_SRGB));
        else
            glCheck(glDisable(GL_FRAMEBUFFER_SRGB));
    }

    // First set the persistent OpenGL states if it's the very first call
    if (!m_cache.glStatesSet)
        resetGLStates();

#ifdef SFML_OPENGL_ES

    Shader* usedShader = const_cast<Shader*>(states.shader);

    if (!states.shader && !states.texture)
        usedShader = const_cast<Shader*>(&Shader::getDefaultShader());
    else if (!states.shader && states.texture)
        usedShader = const_cast<Shader*>(&Shader::getDefaultTexShader());

#endif


    if (useVertexCache)
    {
        // Since vertices are transformed, we must use an identity transform to render them
#ifndef SFML_OPENGL_ES
        if (!m_cache.enable || !m_cache.useVertexCache)
        {
            glCheck(glLoadIdentity());
        }
#else
        usedShader->setUniform("sf_modelview", static_cast<Glsl::Mat4>(Transform::Identity.getMatrix()));
#endif
    }
    else
    {
#ifndef SFML_OPENGL_ES

        applyTransform(states.transform);

#else

        usedShader->setUniform("sf_modelview", static_cast<Glsl::Mat4>(states.transform.getMatrix()));

#endif
    }

#ifndef SFML_OPENGL_ES

    // Apply the view
    if (!m_cache.enable || m_cache.viewChanged)
        applyCurrentView();

#else

    // Set the viewport
    IntRect viewport = getViewport(m_view);
    int top = static_cast<int>(getSize().y) - (viewport.top + viewport.height);
    glCheck(glViewport(viewport.left, top, viewport.width, viewport.height));

    {
        // Set the projection matrix
        usedShader->setUniform("sf_projection", static_cast<Glsl::Mat4>(m_view.getTransform().getMatrix()));
    }

#endif



    // Apply the blend mode
    if (!m_cache.enable || (states.blendMode != m_cache.lastBlendMode))
        applyBlendMode(states.blendMode);

#ifdef SFML_OPENGL_ES

    bool setTexture = false;

#endif

    // Apply the texture
    if (!m_cache.enable || (states.texture && states.texture->m_fboAttachment))
    {
        // If the texture is an FBO attachment, always rebind it
        // in order to inform the OpenGL driver that we want changes
        // made to it in other contexts to be visible here as well
        // This saves us from having to call glFlush() in
        // RenderTextureImplFBO which can be quite costly
        // See: https://www.khronos.org/opengl/wiki/Memory_Model


        applyTexture(states.texture);

#ifdef SFML_OPENGL_ES

        if (states.texture)
            setTexture = true;

#endif

    }
    else
    {
        Uint64 textureId = states.texture ? states.texture->m_cacheId : 0;
        if (textureId != m_cache.lastTextureId)
        {

            applyTexture(states.texture);

#ifdef SFML_OPENGL_ES

            if (states.texture)
                setTexture = true;

#endif
        }
    }

#ifdef SFML_OPENGL_ES

    if (setTexture || states.texture && m_cache.programChanged != usedShader->getNativeHandle())
    {
        GLfloat matrix[16] = { 1.f, 0.f, 0.f, 0.f,
                                  0.f, 1.f, 0.f, 0.f,
                                  0.f, 0.f, 1.f, 0.f,
                                  0.f, 0.f, 0.f, 1.f };

        // If non-normalized coordinates (= pixels) are requested, we need to
        // setup scale factors that convert the range [0 .. size] to [0 .. 1]
        matrix[0] = 1.f / static_cast<float>(states.texture->m_actualSize.x);
        matrix[5] = 1.f / static_cast<float>(states.texture->m_actualSize.y);

        // If pixels are flipped we must invert the Y axis
        if (states.texture->m_pixelsFlipped)
        {
            matrix[5] = -matrix[5];
            matrix[13] = static_cast<float>(states.texture->m_size.y) / static_cast<float>(states.texture->m_actualSize.y);
        }

        usedShader->setUniform("sf_texture", static_cast<Glsl::Mat4>(matrix));
        // Defines an uniform that allows shaders to scale its texcoords depending on their size and not on their actual size
        if (states.texture->m_actualSize.x != 0 && states.texture->m_actualSize.y != 0) {
            GLfloat factor_npot[2] = {
                static_cast<GLfloat>(states.texture->m_size.x) / states.texture->m_actualSize.x,
                static_cast<GLfloat>(states.texture->m_size.y) / states.texture->m_actualSize.y
            };
            usedShader->setUniform("factor_npot", Glsl::Vec2 { factor_npot[0], factor_npot[1] });
        }
    }

    applyShader(usedShader);

    if (m_cache.programChanged != usedShader->getNativeHandle())
    {
        m_cache.programChanged = usedShader->getNativeHandle();
        m_cache.posAttrib = glGetAttribLocation(usedShader->getNativeHandle(), "position");
        m_cache.colAttrib = glGetAttribLocation(usedShader->getNativeHandle(), "color");
        m_cache.texAttrib = glGetAttribLocation(usedShader->getNativeHandle(), "texCoord");
        if (m_cache.posAttrib >= 0)
            glCheck(glEnableVertexAttribArray(m_cache.posAttrib));
        if (m_cache.colAttrib >= 0)
            glCheck(glEnableVertexAttribArray(m_cache.colAttrib));
        if (m_cache.texAttrib >= 0)
            glCheck(glEnableVertexAttribArray(m_cache.texAttrib));
    }

#else

    // Apply the shader
    if (states.shader)
        applyShader(states.shader);

#endif

}


////////////////////////////////////////////////////////////
void RenderTarget::drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount)
{
    // Find the OpenGL primitive type
    static const GLenum modes[] = {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES,
                                   GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_QUADS};
    GLenum mode = modes[type];

    // Draw the primitives
    glCheck(glDrawArrays(mode, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount)));
}


////////////////////////////////////////////////////////////
void RenderTarget::cleanupDraw(const RenderStates& states)
{
#ifdef SFML_OPENGL_ES

    applyShader(NULL);

#else

    // Unbind the shader, if any
    if (states.shader)
        applyShader(NULL);

#endif

    // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
    // This prevents a bug where some drivers do not clear RenderTextures properly.
    if (states.texture && states.texture->m_fboAttachment)
        applyTexture(NULL);

    // Re-enable the cache at the end of the draw if it was disabled
    m_cache.enable = true;
}

} // namespace sf


////////////////////////////////////////////////////////////
// Render states caching strategies
//
// * View
//   If SetView was called since last draw, the projection
//   matrix is updated. We don't need more, the view doesn't
//   change frequently.
//
// * Transform
//   The transform matrix is usually expensive because each
//   entity will most likely use a different transform. This can
//   lead, in worst case, to changing it every 4 vertices.
//   To avoid that, when the vertex count is low enough, we
//   pre-transform them and therefore use an identity transform
//   to render them.
//
// * Blending mode
//   Since it overloads the == operator, we can easily check
//   whether any of the 6 blending components changed and,
//   thus, whether we need to update the blend mode.
//
// * Texture
//   Storing the pointer or OpenGL ID of the last used texture
//   is not enough; if the sf::Texture instance is destroyed,
//   both the pointer and the OpenGL ID might be recycled in
//   a new texture instance. We need to use our own unique
//   identifier system to ensure consistent caching.
//
// * Shader
//   Shaders are very hard to optimize, because they have
//   parameters that can be hard (if not impossible) to track,
//   like matrices or textures. The only optimization that we
//   do is that we avoid setting a null shader if there was
//   already none for the previous draw.
//
////////////////////////////////////////////////////////////
