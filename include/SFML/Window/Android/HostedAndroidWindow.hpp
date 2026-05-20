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


////////////////////////////////////////////////////////////
// Hosted input injection
//
// These mirror what WindowImplAndroid::processKeyEvent and
// processJoystick*Event do for the NativeActivity flow, but take
// raw Android event fields (keycodes, metaState, axis values) so the
// hosting Activity can drive them straight from dispatchKeyEvent /
// dispatchGenericMotionEvent without needing to assemble sf::Event by hand.
// Translation from Android keycodes to sf::Keyboard / joystick indices
// reuses WindowImplAndroid's existing tables.
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
/// \brief Inject a hardware key press.
///
/// repeatCount > 0 is silently dropped: Android's OS-level auto-repeat
/// would otherwise produce duplicate state updates in consumers that
/// run their own software repeat handler (e.g. PSDK's Input module).
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedKeyDown(int androidKeyCode, int metaState, int repeatCount);

////////////////////////////////////////////////////////////
/// \brief Inject a hardware key release.
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedKeyUp(int androidKeyCode, int metaState);

////////////////////////////////////////////////////////////
/// \brief Inject a TextEntered event for a single Unicode codepoint.
///
/// Codepoint of 0 is silently dropped. Use for both hardware-key text
/// (computed by the host via KeyEvent.getUnicodeChar(metaState)) and
/// soft-keyboard input from a hidden EditText TextWatcher.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedText(unsigned int unicodeCodepoint);

////////////////////////////////////////////////////////////
/// \brief Inject a gamepad button down/up event.
///
/// androidKeyCode must be one of the AKEYCODE_BUTTON_* / AKEYCODE_DPAD_*
/// values that androidJoystickKeyToIndex recognises. Other keycodes are
/// silently dropped.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedJoystickButton(int deviceId, int androidKeyCode, bool pressed);

////////////////////////////////////////////////////////////
/// \brief Inject gamepad analog-axis state.
///
/// Axis values are in [-1, 1] as returned by AMotionEvent_getAxisValue.
/// They are scaled to SFML's [-100, 100] range internally to match what
/// WindowImplAndroid::processJoystickMotionEvent feeds into JoystickImpl.
///
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedJoystickAxis(int   deviceId,
                                              float axisX,    float axisY,
                                              float axisZ,    float axisRz,
                                              float hatX,     float hatY,
                                              float lTrigger, float rTrigger);

////////////////////////////////////////////////////////////
/// \brief Inject a gamepad connect / disconnect event.
////////////////////////////////////////////////////////////
SFML_WINDOW_API void injectHostedJoystickConnected(int deviceId);
SFML_WINDOW_API void injectHostedJoystickDisconnected(int deviceId);

} // namespace android
} // namespace sf


#endif // SFML_HOSTEDANDROIDWINDOW_HPP
