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

#ifndef SFML_HOSTEDANDROIDWINDOW_HPP
#define SFML_HOSTEDANDROIDWINDOW_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Export.hpp>
#include <SFML/Window/Event.hpp>

namespace sf
{
namespace android
{

////////////////////////////////////////////////////////////
/// \brief Prepare SFML's global Android activity state to host an externally
///        provided ANativeWindow.
///
/// Use this when SFML is embedded inside a regular Android Activity that owns a
/// SurfaceView/TextureView, instead of being driven from a NativeActivity.
///
/// Must be called before constructing any sf::Window or sf::RenderWindow using
/// the WindowHandle constructor on Android. The caller retains ownership of
/// the ANativeWindow* and is responsible for calling ANativeWindow_release
/// after releaseHostedActivity().
///
/// \param nativeWindow  Pointer obtained from ANativeWindow_fromSurface (must be non-null).
/// \param width         Current width of the surface in pixels.
/// \param height        Current height of the surface in pixels.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void prepareHostedActivity(void* nativeWindow, int width, int height);

////////////////////////////////////////////////////////////
/// \brief Tear down the hosted activity state.
///
/// Call after the last sf::Window/sf::RenderWindow constructed with the hosted
/// flow has been destroyed. After this call the previously provided
/// ANativeWindow* must be released by the caller via ANativeWindow_release.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void releaseHostedActivity();

////////////////////////////////////////////////////////////
/// \brief Notify SFML that the hosted surface has been resized.
///
/// Updates the cached screen size and enqueues an Event::Resized event on the
/// active window if any.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void notifyHostedSurfaceResized(int width, int height);

////////////////////////////////////////////////////////////
/// \brief Inject an event into the active SFML window's queue.
///
/// Use this to forward touch/key events captured by the Android host View
/// (View.OnTouchListener etc.) into SFML's event pump. Mirrors what
/// SFML's NativeActivity callbacks do internally.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedEvent(const Event& event);

////////////////////////////////////////////////////////////
/// \brief Clear states.window without tearing down ActivityStates.
///
/// Used by the pause-resume path of the LiteCGSS hosted-surface bridge:
/// when the SurfaceView's surfaceDestroyed fires, the caller releases its
/// ANativeWindow* reference and needs SFML's cached pointer cleared so a
/// later EGL operation can't dereference it. The next prepareHostedActivity
/// (on surfaceCreated/Changed) repopulates states.window with the new
/// ANativeWindow*. Unlike releaseHostedActivity, this keeps the EGL
/// display, the GL context, and ActivityStates itself alive.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void clearHostedSurfaceWindow();

} // namespace android
} // namespace sf


#endif // SFML_HOSTEDANDROIDWINDOW_HPP
