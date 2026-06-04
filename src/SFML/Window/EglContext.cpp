////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
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
#include <SFML/Window/EglContext.hpp>
#include <SFML/Window/WindowImpl.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/System/Sleep.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#ifdef SFML_SYSTEM_ANDROID
    #include <SFML/System/Android/Activity.hpp>
#endif
#if defined(SFML_SYSTEM_LINUX) && !defined(SFML_USE_DRM)
    #include <X11/Xlib.h>
#endif

#if defined(SFML_SYSTEM_SWITCH)
#include <EGL/egl.h>
#else
// We check for this definition in order to avoid multiple definitions of GLAD
// entities during unity builds of SFML.
#ifndef SF_GLAD_EGL_IMPLEMENTATION_INCLUDED
#define SF_GLAD_EGL_IMPLEMENTATION_INCLUDED
#define SF_GLAD_EGL_IMPLEMENTATION
#include <glad/egl.h>
#endif
#endif

namespace
{
    // A nested named namespace is used here to allow unity builds of SFML.
    namespace EglContextImpl
    {
#if defined(SFML_SYSTEM_SWITCH)
        // The EGL display is a process-wide singleton shared by every EglContext.
        // On Switch it owns the default nwindow framebuffer, so it MUST be
        // terminated exactly once -- but only after the LAST context is gone.
        sf::Mutex& displayMutex()
        {
            static sf::Mutex m;
            return m;
        }
        EGLDisplay g_display       = EGL_NO_DISPLAY;
        unsigned int g_displayRefs = 0;
    
        void releaseDisplay()  // call once per context (release)
        {
            sf::Lock lock(displayMutex());
            assert(g_displayRefs > 0);
            if (--g_displayRefs == 0) {
                eglCheck(eglTerminate(g_display));  // hands framebuffer back to OS
                g_display = EGL_NO_DISPLAY;         // allow clean re-init later
            }
        }
#endif

        EGLDisplay getInitializedDisplay()
        {
#if defined(SFML_SYSTEM_ANDROID)

            // On Android, its native activity handles this for us
            sf::priv::ActivityStates& states = sf::priv::getActivity();
            sf::Lock lock(states.mutex);

            return states.display;

#elif defined(SFML_SYSTEM_SWITCH)

            sf::Lock lock(displayMutex());
            if (g_display == EGL_NO_DISPLAY)
            {
                eglCheck(g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY));
                eglCheck(eglInitialize(g_display, NULL, NULL));
            }
            ++g_displayRefs;
            return g_display;

#else
            static EGLDisplay display = EGL_NO_DISPLAY;
            
            if (display == EGL_NO_DISPLAY)
            {
                eglCheck(display = eglGetDisplay(EGL_DEFAULT_DISPLAY));
                eglCheck(eglInitialize(display, NULL, NULL));
            }
            
            return display;
#endif
        }


        ////////////////////////////////////////////////////////////
        // GLAD's EGL function pointers are process-wide: loading them must
        // be serialized. In hosted Android mode the UI thread (surface
        // attach) and the render thread (its first context) get here at the
        // same time; the one finding `initialized` already set while the
        // other was still loading went on with eglGetDisplay loaded but not
        // yet eglInitialize, and called address 0.
        sf::Mutex& gladMutex()
        {
#if defined(SFML_SYSTEM_ANDROID)
            // The activity's mutex: getInitializedDisplay() takes it while
            // loading, and threads holding it create contexts. A mutex of
            // our own would be taken in the opposite order by them.
            return sf::priv::getActivity().mutex;
#else
            static sf::Mutex mutex;
            return mutex;
#endif
        }


        ////////////////////////////////////////////////////////////
        void ensureInit()
        {
#if defined(SFML_SYSTEM_SWITCH)
            return;
#else
            sf::Lock lock(gladMutex());
            static bool initialized = false;
            if (!initialized)
            {
                // We don't check the return value since the extension
                // flags are cleared even if loading fails
                gladLoaderLoadEGL(EGL_NO_DISPLAY);

                // Continue loading with a display
                gladLoaderLoadEGL(getInitializedDisplay());

                // Only once loaded: another thread may be waiting on the lock
                initialized = true;
            }
#endif
        }
    }
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
EglContext::EglContext(EglContext* shared) :
m_display (EGL_NO_DISPLAY),
m_context (EGL_NO_CONTEXT),
m_surface (EGL_NO_SURFACE),
m_config  (NULL)
{
    EglContextImpl::ensureInit();

    // Get the initialized EGL display
    m_display = EglContextImpl::getInitializedDisplay();

    // Get the best EGL config matching the default video settings
    m_config = getBestConfig(m_display, VideoMode::getDesktopMode().bitsPerPixel, ContextSettings());
    updateSettings();

    // Note: The EGL specs say that attrib_list can be NULL when passed to eglCreatePbufferSurface,
    // but this is resulting in a segfault. Bug in Android?
    EGLint attrib_list[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT,1,
        EGL_NONE
    };

#if defined(SFML_SYSTEM_SWITCH)    
    // Clear previous errors
    while (eglGetError() != EGL_SUCCESS);

    // Try to create a PBuffer (Tiny 1x1 surface)
    m_surface = eglCreatePbufferSurface(m_display, m_config, attrib_list);

    // Check for failure (EGL_BAD_MATCH or EGL_BAD_CONFIG)
    if (m_surface == EGL_NO_SURFACE || eglGetError() != EGL_SUCCESS)
    {
        // If PBuffers are not supported, we fall back to a "Surfaceless Context".
        // This is valid in EGL 1.5 or via KHR extensions, and widely supported 
        // by drivers for internal dummy contexts.
        sf::err() << "Notice: PBuffer failed. Using Surfaceless Dummy Context." << std::endl;
        m_surface = EGL_NO_SURFACE; 
    }
#else
    eglCheck(m_surface = eglCreatePbufferSurface(m_display, m_config, attrib_list));
#endif

    // Create EGL context
    createContext(shared);
}


////////////////////////////////////////////////////////////
EglContext::EglContext(EglContext* shared, const ContextSettings& settings, const WindowImpl* owner, unsigned int bitsPerPixel) :
m_display (EGL_NO_DISPLAY),
m_context (EGL_NO_CONTEXT),
m_surface (EGL_NO_SURFACE),
m_config  (NULL)
{
    EglContextImpl::ensureInit();

#ifdef SFML_SYSTEM_ANDROID

    // On Android, we must save the created context
    ActivityStates& states = getActivity();
    Lock lock(states.mutex);

    states.context = this;

#endif

    // Get the initialized EGL display
    m_display = EglContextImpl::getInitializedDisplay();

    // Get the best EGL config matching the requested video settings
    m_config = getBestConfig(m_display, bitsPerPixel, settings);
    updateSettings();

    // Create EGL context
    createContext(shared);

#if defined(SFML_SYSTEM_ANDROID) && !defined(SFML_SYSTEM_SWITCH)
    // Create EGL surface (except on Android because the window is created
    // asynchronously, its activity manager will call it for us)
    createSurface(owner->getSystemHandle());
#elif defined(SFML_SYSTEM_SWITCH)
    createSurface(nwindowGetDefault());
#else
    (void) owner;
    // On Android with the hosted-Activity flow (sf::android::prepareHostedActivity
    // populated states.window before this ctor ran), the surface is already
    // valid and the immediately-following setActive(true) needs it bound —
    // so create it eagerly here. The legacy NativeActivity flow leaves
    // states.window == NULL until onNativeWindowCreated fires later, in
    // which case forwardEvent(GainedFocus) sets m_windowBeingCreated and
    // WindowImplAndroid::processEvents picks up the deferred surface
    // creation on the next event pump (unchanged behaviour).
    {
        ActivityStates& s = getActivity();
        Lock l(s.mutex);
        if (s.window != NULL)
        {
            createSurface(s.window);
        }
    }
#endif
}


////////////////////////////////////////////////////////////
EglContext::EglContext(EglContext* /*shared*/, const ContextSettings& /*settings*/, unsigned int /*width*/, unsigned int /*height*/) :
m_display (EGL_NO_DISPLAY),
m_context (EGL_NO_CONTEXT),
m_surface (EGL_NO_SURFACE),
m_config  (NULL)
{
    EglContextImpl::ensureInit();

    sf::err() << "Warning: context has not been initialized. The constructor EglContext(shared, settings, width, height) is currently not implemented." << std::endl;
}


////////////////////////////////////////////////////////////
EglContext::~EglContext()
{
    // Notify unshared OpenGL resources of context destruction
    cleanupUnsharedResources();

    // Deactivate the current context
    EGLContext currentContext = EGL_NO_CONTEXT;
    eglCheck(currentContext = eglGetCurrentContext());

    if (currentContext == m_context)
    {
        eglCheck(eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    }

    // Destroy context
    if (m_context != EGL_NO_CONTEXT)
    {
        eglCheck(eglDestroyContext(m_display, m_context));
    }

    // Destroy surface
    if (m_surface != EGL_NO_SURFACE)
    {
        eglCheck(eglDestroySurface(m_display, m_surface));
    }

#if defined(SFML_SYSTEM_ANDROID)
    // The pause-resume hosted-Activity flow keeps ActivityStates alive
    // across cgss::DisplayWindow lifetimes. WindowImplAndroid::processEvents
    // dereferences states.context unconditionally; without this clear,
    // a NEW DisplayWindow's first event pump would call into a freshly
    // freed EglContext (this one) instead of the new one. Note this is
    // only safe because EglContext::ctor populated states.context = this
    // when *this* was the active context — if another EglContext has
    // since taken over states.context, we leave it alone.
    //
    // Read via getActivityStatesPtr() (which returns the raw pointer
    // and can be NULL) rather than getActivity() (which assert(!= NULL)s):
    // on process shutdown or in test harnesses an EglContext can be
    // destructed after releaseHostedActivity or before any hosted setup,
    // and we don't want to abort on what is otherwise legitimate cleanup.
    if (sf::priv::ActivityStates* const states = sf::priv::getActivityStatesPtr())
    {
        sf::Lock lock(states->mutex);
        if (states->context == this)
        {
            states->context = NULL;
        }
    }
#elif defined(SFML_SYSTEM_SWITCH)
    if (m_display)
    {
        EglContextImpl::releaseDisplay();
        m_display = NULL;
    }
#endif
}


////////////////////////////////////////////////////////////
GlFunctionPointer EglContext::getFunction(const char* name)
{
    EglContextImpl::ensureInit();

    return eglGetProcAddress(name);
}


////////////////////////////////////////////////////////////
void EglContext::ensureGladLoaded()
{
    // Same idempotent guard the rest of EglContext uses internally;
    // the function-local static inside EglContextImpl::ensureInit means
    // repeated calls are cheap.
    EglContextImpl::ensureInit();
}


#if !defined(SFML_SYSTEM_SWITCH)
////////////////////////////////////////////////////////////
void EglContext::reloadGladForDisplay(EGLDisplay display)
{
    sf::Lock lock(EglContextImpl::gladMutex());

    EglContextImpl::ensureInit();

    gladLoaderLoadEGL(display);
}
#endif


////////////////////////////////////////////////////////////
bool EglContext::makeCurrent(bool current)
{
    // If we are in "Surfaceless Mode" (m_surface is NO_SURFACE), we pass EGL_NO_SURFACE to eglMakeCurrent.
#if !defined(SFML_SYSTEM_SWITCH)
    // Binding needs a surface; releasing does not. Refusing to release
    // without a surface left the context current on a thread about to exit
    // (hosted mode tears the surface down asynchronously), and the next
    // render thread then failed every eglMakeCurrent with EGL_BAD_ACCESS.
    if (current && m_surface == EGL_NO_SURFACE)
        return false;
#endif

    EGLBoolean result = EGL_FALSE;

    if (current)
    {
        eglCheck(result = eglMakeCurrent(m_display, m_surface, m_surface, m_context));
    }
    else
    {
        eglCheck(result = eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    }

    return (result != EGL_FALSE);
}


////////////////////////////////////////////////////////////
void EglContext::display()
{
    if (m_surface != EGL_NO_SURFACE)
        eglCheck(eglSwapBuffers(m_display, m_surface));
}


////////////////////////////////////////////////////////////
void EglContext::setVerticalSyncEnabled(bool enabled)
{
    eglCheck(eglSwapInterval(m_display, enabled ? 1 : 0));
}


////////////////////////////////////////////////////////////
void EglContext::createContext(EglContext* shared)
{
    const EGLint contextVersion[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext toShared;

    if (shared)
        toShared = shared->m_context;
    else
        toShared = EGL_NO_CONTEXT;

    if (toShared != EGL_NO_CONTEXT)
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // Create EGL context
    eglCheck(m_context = eglCreateContext(m_display, m_config, toShared, contextVersion));
}


////////////////////////////////////////////////////////////
void EglContext::createSurface(EGLNativeWindowType window)
{
    // Belt-and-suspenders: if a prior LostFocus's destroy flag was
    // cancelled by a back-to-back GainedFocus (see WindowImplAndroid::
    // forwardEvent), m_surface still points to the EGLSurface backed
    // by the now-freed ANativeWindow. Releasing it before creating the
    // new one keeps us from leaking the stale EGLSurface and ensures
    // eglMakeCurrent picks the right one on the next setActive.
    //
    // Unbind through setActive(false), same as destroySurface(), never a
    // raw eglMakeCurrent: GlContext tracks the thread's current context
    // and setActive(true) is a no-op while it still points at us. A raw
    // unbind left that tracking stale, so the next setActive(true) never
    // rebound the context and every GL call ran with no current context
    // (black screen after a detach/attach with no event pump in between).
    if (m_surface != EGL_NO_SURFACE)
    {
        setActive(false);
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
    }
    eglCheck(m_surface = eglCreateWindowSurface(m_display, m_config, window, NULL));
}


////////////////////////////////////////////////////////////
void EglContext::destroySurface()
{
    // Ensure that this context is no longer active since our surface is going to be destroyed
    setActive(false);

    eglCheck(eglDestroySurface(m_display, m_surface));
    m_surface = EGL_NO_SURFACE;
}


////////////////////////////////////////////////////////////
EGLConfig EglContext::getBestConfig(EGLDisplay display, unsigned int bitsPerPixel, const ContextSettings& settings)
{
    EglContextImpl::ensureInit();

#ifdef SFML_SYSTEM_SWITCH
    const EGLint attributes[] = {
        // We linked libGLESv2, so we must ask for ES.
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,

        // PBUFFER_BIT is not supported on Switch, and we don't need it since 
        // we support Surfaceless Contexts, so we only ask for WINDOW_BIT.
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

        // Standard Switch color depths
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
#else
    // Set our video settings constraint
    const EGLint attributes[] = {
        EGL_BUFFER_SIZE, static_cast<EGLint>(bitsPerPixel),
        EGL_DEPTH_SIZE, static_cast<EGLint>(settings.depthBits),
        EGL_STENCIL_SIZE, static_cast<EGLint>(settings.stencilBits),
        EGL_SAMPLE_BUFFERS, settings.antialiasingLevel ? 1 : 0,
        EGL_SAMPLES, static_cast<EGLint>(settings.antialiasingLevel),
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
#endif

    EGLint configCount;
    EGLConfig configs[1];

    // Ask EGL for the best config matching our video settings
    eglCheck(eglChooseConfig(display, attributes, configs, 1, &configCount));

    // TODO: This should check EGL_CONFORMANT and pick the first conformant configuration.

    return configs[0];
}


////////////////////////////////////////////////////////////
void EglContext::updateSettings()
{
    EGLBoolean result = EGL_FALSE;
    EGLint tmp = 0;

    // Update the internal context settings with the current config
    eglCheck(result = eglGetConfigAttrib(m_display, m_config, EGL_DEPTH_SIZE, &tmp));

    if (result == EGL_FALSE)
        err() << "Failed to retrieve EGL_DEPTH_SIZE" << std::endl;

    m_settings.depthBits = static_cast<unsigned int>(tmp);

    eglCheck(result = eglGetConfigAttrib(m_display, m_config, EGL_STENCIL_SIZE, &tmp));

    if (result == EGL_FALSE)
        err() << "Failed to retrieve EGL_STENCIL_SIZE" << std::endl;

    m_settings.stencilBits = static_cast<unsigned int>(tmp);

    eglCheck(result = eglGetConfigAttrib(m_display, m_config, EGL_SAMPLES, &tmp));

    if (result == EGL_FALSE)
        err() << "Failed to retrieve EGL_SAMPLES" << std::endl;

    m_settings.antialiasingLevel = static_cast<unsigned int>(tmp);

    m_settings.majorVersion = 1;
    m_settings.minorVersion = 1;
    m_settings.attributeFlags = ContextSettings::Default;
}


#if defined(SFML_SYSTEM_LINUX) && !defined(SFML_USE_DRM)
////////////////////////////////////////////////////////////
XVisualInfo EglContext::selectBestVisual(::Display* XDisplay, unsigned int bitsPerPixel, const ContextSettings& settings)
{
    EglContextImpl::ensureInit();

    // Get the initialized EGL display
    EGLDisplay display = EglContextImpl::getInitializedDisplay();

    // Get the best EGL config matching the default video settings
    EGLConfig config = getBestConfig(display, bitsPerPixel, settings);

    // Retrieve the visual id associated with this EGL config
    EGLint nativeVisualId;

    eglCheck(eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &nativeVisualId));

    if (nativeVisualId == 0)
    {
        // Should never happen...
        err() << "No EGL visual found. You should check your graphics driver" << std::endl;

        return XVisualInfo();
    }

    XVisualInfo vTemplate;
    vTemplate.visualid = static_cast<VisualID>(nativeVisualId);

    // Get X11 visuals compatible with this EGL config
    XVisualInfo *availableVisuals, bestVisual;
    int visualCount = 0;

    availableVisuals = XGetVisualInfo(XDisplay, VisualIDMask, &vTemplate, &visualCount);

    if (visualCount == 0)
    {
        // Can't happen...
        err() << "No X11 visual found. Bug in your EGL implementation ?" << std::endl;

        return XVisualInfo();
    }

    // Pick up the best one
    bestVisual = availableVisuals[0];
    XFree(availableVisuals);

    return bestVisual;
}
#endif

} // namespace priv

} // namespace sf
