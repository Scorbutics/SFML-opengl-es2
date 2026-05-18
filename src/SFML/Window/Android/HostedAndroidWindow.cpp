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
#include <SFML/Window/Android/HostedAndroidWindow.hpp>
#include <SFML/Window/Android/WindowImplAndroid.hpp>
#include <SFML/Window/EglContext.hpp>
#include <SFML/System/Android/Activity.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Err.hpp>

#include <android/native_window.h>

// Declarations only — the GLAD EGL implementation storage (function pointers,
// version flags) is emitted by EglContext.cpp via SF_GLAD_EGL_IMPLEMENTATION.
// Defining the implementation macro here too would double-emit those globals
// and break the link with duplicate-symbol errors in non-unity builds.
// gladLoaderLoadEGL is `static` inside that impl block (so unreachable from
// this TU); we drive it through sf::priv::EglContext::ensureGladLoaded()
// below, which lives in the same TU as the impl.
#include <glad/egl.h>

#include <cstddef>


namespace
{
    // Owned storage for the ActivityStates singleton when SFML is running in
    // hosted mode. The NativeActivity path heap-allocates this in
    // ANativeActivity_onCreate; we mirror that ownership model here.
    sf::priv::ActivityStates* g_hostedStates = NULL;
}

namespace sf
{
namespace android
{

////////////////////////////////////////////////////////////
void prepareHostedActivity(void* nativeWindow, int width, int height)
{
    if (g_hostedStates != NULL)
    {
        // Already prepared. Treat as a surface refresh: update the live
        // ANativeWindow* and dimensions, and let SFML rebuild the EGL surface
        // on the next event pump (forwardEvent(GainedFocus) sets that flag).
        Lock lock(g_hostedStates->mutex);
        g_hostedStates->window = static_cast<ANativeWindow*>(nativeWindow);
        g_hostedStates->screenSize.x = width;
        g_hostedStates->screenSize.y = height;
        return;
    }

    priv::ActivityStates* states = new priv::ActivityStates;

    // Mirror the field initialization in ANativeActivity_onCreate
    // (src/SFML/Main/MainAndroid.cpp).
    states->activity       = NULL;
    states->window         = static_cast<ANativeWindow*>(nativeWindow);
    states->looper         = NULL;
    states->inputQueue     = NULL;
    states->config         = NULL;
    states->context        = NULL; // populated by EglContext ctor on first RenderWindow
    states->savedState     = NULL;
    states->savedStateSize = 0;
    states->forwardEvent   = NULL;
    states->processEvent   = NULL;
    states->mainOver       = false;
    states->initialized    = false;
    states->terminated     = false;
    states->fullscreen     = false;
    states->updated        = false;
    states->screenSize.x   = width;
    states->screenSize.y   = height;

    for (unsigned int i = 0; i < Mouse::ButtonCount; i++)
        states->isButtonPressed[i] = false;

    // Initial display value before glad is loaded — getInitializedDisplay
    // (called from ensureGladLoaded → ensureInit) will read this and pass it
    // through to gladLoaderLoadEGL. EGL_NO_DISPLAY is fine: glad's first pass
    // doesn't need a real display, and we overwrite this immediately below
    // once eglGetDisplay is resolved.
    states->display = EGL_NO_DISPLAY;

    // Register as the global FIRST. Without this, the assert in
    // sf::priv::getActivity() fires from inside ensureGladLoaded and the
    // process aborts silently — the call chain is
    //   ensureGladLoaded → EglContextImpl::ensureInit
    //                    → EglContextImpl::getInitializedDisplay
    //                    → sf::priv::getActivity()  // asserts states != NULL
    priv::resetActivity(states);
    g_hostedStates = states;

    // Bootstrap EGL the same way SFML's NativeActivity does — this is what
    // EglContextImpl::getInitializedDisplay() expects to find later.
    // ensureGladLoaded populates the GLAD function pointers (eglGetDisplay,
    // eglInitialize, ...) for this TU; the loader function itself is
    // file-local to EglContext.cpp, hence the public helper.
    priv::EglContext::ensureGladLoaded();
    states->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(states->display, NULL, NULL);

    // Redirect stderr to logcat (matches NativeActivity behavior).
    err().rdbuf(&states->logcat);
}

////////////////////////////////////////////////////////////
void releaseHostedActivity()
{
    if (g_hostedStates == NULL)
        return;

    // Terminate EGL. The caller is responsible for releasing the
    // ANativeWindow* afterwards.
    if (g_hostedStates->display != EGL_NO_DISPLAY)
        eglTerminate(g_hostedStates->display);

    priv::resetActivity(NULL);

    delete g_hostedStates;
    g_hostedStates = NULL;
}

////////////////////////////////////////////////////////////
void notifyHostedSurfaceResized(int width, int height)
{
    if (g_hostedStates == NULL)
        return;

    {
        Lock lock(g_hostedStates->mutex);
        g_hostedStates->screenSize.x = width;
        g_hostedStates->screenSize.y = height;
    }

    Event event;
    event.type = Event::Resized;
    event.size.width  = static_cast<unsigned int>(width);
    event.size.height = static_cast<unsigned int>(height);
    injectHostedEvent(event);
}

////////////////////////////////////////////////////////////
void injectHostedEvent(const Event& event)
{
    // forwardEvent is a public static on WindowImplAndroid and already
    // performs the GainedFocus/LostFocus housekeeping plus pushEvent.
    priv::WindowImplAndroid::forwardEvent(event);
}

} // namespace android
} // namespace sf
