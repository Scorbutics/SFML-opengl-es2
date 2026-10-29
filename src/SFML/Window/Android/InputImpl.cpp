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
#include <SFML/Window/Android/InputImpl.hpp>
#include <SFML/Window/Android/HostedAndroidWindow.hpp>
#include <SFML/System/Android/Activity.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <jni.h>


namespace
{
// Set by the host in hosted mode; guarded by the activity states' mutex
sf::android::HostedVirtualKeyboardHandler hostedVirtualKeyboardHandler = NULL;
}


namespace sf
{
namespace android
{
////////////////////////////////////////////////////////////
void setHostedVirtualKeyboardHandler(HostedVirtualKeyboardHandler handler)
{
    priv::ActivityStates& states = priv::getActivity();
    Lock lock(states.mutex);
    hostedVirtualKeyboardHandler = handler;
}
} // namespace android


namespace priv
{
////////////////////////////////////////////////////////////
bool InputImpl::isKeyPressed(Keyboard::Key /* key */)
{
    // Not applicable
    return false;
}

bool InputImpl::isKeyPressed(Keyboard::Scancode /* codes */)
{
    // Not applicable
    return false;
}

////////////////////////////////////////////////////////////
// Android exposes no keyboard layout to native code: keys are mapped as on a
// US QWERTY layout, the layout Android key codes are named after. Each entry
// is a key, the scancode producing it and its description.
namespace
{
struct KeyMapping
{
    Keyboard::Key      key;
    Keyboard::Scancode scancode;
    const char*        description;
};

const KeyMapping keyMappings[] =
{
    { Keyboard::A, Keyboard::Scan::A, "A" },
    { Keyboard::B, Keyboard::Scan::B, "B" },
    { Keyboard::C, Keyboard::Scan::C, "C" },
    { Keyboard::D, Keyboard::Scan::D, "D" },
    { Keyboard::E, Keyboard::Scan::E, "E" },
    { Keyboard::F, Keyboard::Scan::F, "F" },
    { Keyboard::G, Keyboard::Scan::G, "G" },
    { Keyboard::H, Keyboard::Scan::H, "H" },
    { Keyboard::I, Keyboard::Scan::I, "I" },
    { Keyboard::J, Keyboard::Scan::J, "J" },
    { Keyboard::K, Keyboard::Scan::K, "K" },
    { Keyboard::L, Keyboard::Scan::L, "L" },
    { Keyboard::M, Keyboard::Scan::M, "M" },
    { Keyboard::N, Keyboard::Scan::N, "N" },
    { Keyboard::O, Keyboard::Scan::O, "O" },
    { Keyboard::P, Keyboard::Scan::P, "P" },
    { Keyboard::Q, Keyboard::Scan::Q, "Q" },
    { Keyboard::R, Keyboard::Scan::R, "R" },
    { Keyboard::S, Keyboard::Scan::S, "S" },
    { Keyboard::T, Keyboard::Scan::T, "T" },
    { Keyboard::U, Keyboard::Scan::U, "U" },
    { Keyboard::V, Keyboard::Scan::V, "V" },
    { Keyboard::W, Keyboard::Scan::W, "W" },
    { Keyboard::X, Keyboard::Scan::X, "X" },
    { Keyboard::Y, Keyboard::Scan::Y, "Y" },
    { Keyboard::Z, Keyboard::Scan::Z, "Z" },
    { Keyboard::Num0, Keyboard::Scan::Num0, "0" },
    { Keyboard::Num1, Keyboard::Scan::Num1, "1" },
    { Keyboard::Num2, Keyboard::Scan::Num2, "2" },
    { Keyboard::Num3, Keyboard::Scan::Num3, "3" },
    { Keyboard::Num4, Keyboard::Scan::Num4, "4" },
    { Keyboard::Num5, Keyboard::Scan::Num5, "5" },
    { Keyboard::Num6, Keyboard::Scan::Num6, "6" },
    { Keyboard::Num7, Keyboard::Scan::Num7, "7" },
    { Keyboard::Num8, Keyboard::Scan::Num8, "8" },
    { Keyboard::Num9, Keyboard::Scan::Num9, "9" },
    { Keyboard::Escape, Keyboard::Scan::Escape, "Escape" },
    { Keyboard::LControl, Keyboard::Scan::LControl, "Left Control" },
    { Keyboard::LShift, Keyboard::Scan::LShift, "Left Shift" },
    { Keyboard::LAlt, Keyboard::Scan::LAlt, "Left Alt" },
    { Keyboard::LSystem, Keyboard::Scan::LSystem, "Left Meta" },
    { Keyboard::RControl, Keyboard::Scan::RControl, "Right Control" },
    { Keyboard::RShift, Keyboard::Scan::RShift, "Right Shift" },
    { Keyboard::RAlt, Keyboard::Scan::RAlt, "Right Alt" },
    { Keyboard::RSystem, Keyboard::Scan::RSystem, "Right Meta" },
    { Keyboard::Menu, Keyboard::Scan::Menu, "Menu" },
    { Keyboard::LBracket, Keyboard::Scan::LBracket, "[" },
    { Keyboard::RBracket, Keyboard::Scan::RBracket, "]" },
    { Keyboard::Semicolon, Keyboard::Scan::Semicolon, ";" },
    { Keyboard::Comma, Keyboard::Scan::Comma, "," },
    { Keyboard::Period, Keyboard::Scan::Period, "." },
    { Keyboard::Apostrophe, Keyboard::Scan::Apostrophe, "'" },
    { Keyboard::Slash, Keyboard::Scan::Slash, "/" },
    { Keyboard::Backslash, Keyboard::Scan::Backslash, "\\" },
    { Keyboard::Grave, Keyboard::Scan::Grave, "`" },
    { Keyboard::Equal, Keyboard::Scan::Equal, "=" },
    { Keyboard::Hyphen, Keyboard::Scan::Hyphen, "-" },
    { Keyboard::Space, Keyboard::Scan::Space, "Space" },
    { Keyboard::Enter, Keyboard::Scan::Enter, "Enter" },
    { Keyboard::Backspace, Keyboard::Scan::Backspace, "Backspace" },
    { Keyboard::Tab, Keyboard::Scan::Tab, "Tab" },
    { Keyboard::PageUp, Keyboard::Scan::PageUp, "Page Up" },
    { Keyboard::PageDown, Keyboard::Scan::PageDown, "Page Down" },
    { Keyboard::End, Keyboard::Scan::End, "End" },
    { Keyboard::Home, Keyboard::Scan::Home, "Home" },
    { Keyboard::Insert, Keyboard::Scan::Insert, "Insert" },
    { Keyboard::Delete, Keyboard::Scan::Delete, "Delete" },
    { Keyboard::Add, Keyboard::Scan::NumpadPlus, "Numpad +" },
    { Keyboard::Subtract, Keyboard::Scan::NumpadMinus, "Numpad -" },
    { Keyboard::Multiply, Keyboard::Scan::NumpadMultiply, "Numpad *" },
    { Keyboard::Divide, Keyboard::Scan::NumpadDivide, "Numpad /" },
    { Keyboard::Left, Keyboard::Scan::Left, "Left" },
    { Keyboard::Right, Keyboard::Scan::Right, "Right" },
    { Keyboard::Up, Keyboard::Scan::Up, "Up" },
    { Keyboard::Down, Keyboard::Scan::Down, "Down" },
    { Keyboard::Numpad0, Keyboard::Scan::Numpad0, "Numpad 0" },
    { Keyboard::Numpad1, Keyboard::Scan::Numpad1, "Numpad 1" },
    { Keyboard::Numpad2, Keyboard::Scan::Numpad2, "Numpad 2" },
    { Keyboard::Numpad3, Keyboard::Scan::Numpad3, "Numpad 3" },
    { Keyboard::Numpad4, Keyboard::Scan::Numpad4, "Numpad 4" },
    { Keyboard::Numpad5, Keyboard::Scan::Numpad5, "Numpad 5" },
    { Keyboard::Numpad6, Keyboard::Scan::Numpad6, "Numpad 6" },
    { Keyboard::Numpad7, Keyboard::Scan::Numpad7, "Numpad 7" },
    { Keyboard::Numpad8, Keyboard::Scan::Numpad8, "Numpad 8" },
    { Keyboard::Numpad9, Keyboard::Scan::Numpad9, "Numpad 9" },
    { Keyboard::F1, Keyboard::Scan::F1, "F1" },
    { Keyboard::F2, Keyboard::Scan::F2, "F2" },
    { Keyboard::F3, Keyboard::Scan::F3, "F3" },
    { Keyboard::F4, Keyboard::Scan::F4, "F4" },
    { Keyboard::F5, Keyboard::Scan::F5, "F5" },
    { Keyboard::F6, Keyboard::Scan::F6, "F6" },
    { Keyboard::F7, Keyboard::Scan::F7, "F7" },
    { Keyboard::F8, Keyboard::Scan::F8, "F8" },
    { Keyboard::F9, Keyboard::Scan::F9, "F9" },
    { Keyboard::F10, Keyboard::Scan::F10, "F10" },
    { Keyboard::F11, Keyboard::Scan::F11, "F11" },
    { Keyboard::F12, Keyboard::Scan::F12, "F12" },
    { Keyboard::F13, Keyboard::Scan::F13, "F13" },
    { Keyboard::F14, Keyboard::Scan::F14, "F14" },
    { Keyboard::F15, Keyboard::Scan::F15, "F15" },
    { Keyboard::Pause, Keyboard::Scan::Pause, "Pause" },
};
}


////////////////////////////////////////////////////////////
Keyboard::Key InputImpl::localize(Keyboard::Scancode code)
{
    for (const KeyMapping& mapping : keyMappings)
    {
        if (mapping.scancode == code)
            return mapping.key;
    }
    return Keyboard::Unknown;
}

Keyboard::Scancode InputImpl::delocalize(Keyboard::Key key)
{
    for (const KeyMapping& mapping : keyMappings)
    {
        if (mapping.key == key)
            return mapping.scancode;
    }
    return Keyboard::Scan::Unknown;
}

String InputImpl::getDescription(Keyboard::Scancode code)
{
    for (const KeyMapping& mapping : keyMappings)
    {
        if (mapping.scancode == code)
            return mapping.description;
    }
    return "";
}

////////////////////////////////////////////////////////////
void InputImpl::setVirtualKeyboardVisible(bool visible)
{
    // todo: Check if the window is active

    ActivityStates& states = getActivity();
    Lock lock(states.mutex);

    // Hosted mode: the host shows the keyboard for its text field
    if (hostedVirtualKeyboardHandler != NULL)
    {
        hostedVirtualKeyboardHandler(visible ? 1 : 0);
        return;
    }

    // Pick the JNI context: prefer the NativeActivity-provided one when
    // present, otherwise fall back to the hosted-mode context populated by
    // sf::android::setHostedJniContext. Without either, this is a no-op
    // rather than a NULL deref.
    JavaVM* lJavaVM      = NULL;
    jobject lActivityObj = NULL;
    if (states.activity != NULL)
    {
        lJavaVM      = states.activity->vm;
        lActivityObj = states.activity->clazz;
    }
    else if (states.hostJvm != NULL && states.hostActivity != NULL)
    {
        lJavaVM      = states.hostJvm;
        lActivityObj = states.hostActivity;
    }
    else
    {
        err() << "setVirtualKeyboardVisible: no JNI context (no NativeActivity, and "
                 "sf::android::setHostedJniContext was not called)" << std::endl;
        return;
    }

    // Attach the calling thread if it isn't already. We must only detach the
    // ones we attached ourselves — if the caller is on a thread that the JVM
    // already knows about (e.g. a Ruby worker that has JNI-attached for other
    // reasons), DetachCurrentThread would yank that attachment out from
    // under it.
    JNIEnv* lJNIEnv  = NULL;
    bool    attached = false;
    const jint getStatus = lJavaVM->GetEnv(reinterpret_cast<void**>(&lJNIEnv), JNI_VERSION_1_6);
    if (getStatus == JNI_EDETACHED)
    {
        JavaVMAttachArgs lJavaVMAttachArgs;
        lJavaVMAttachArgs.version = JNI_VERSION_1_6;
        lJavaVMAttachArgs.name    = "SFML-VirtualKeyboard";
        lJavaVMAttachArgs.group   = NULL;
        if (lJavaVM->AttachCurrentThread(&lJNIEnv, &lJavaVMAttachArgs) != JNI_OK)
        {
            err() << "Failed to attach JNI thread; can't switch the keyboard visibility" << std::endl;
            return;
        }
        attached = true;
    }
    else if (getStatus != JNI_OK)
    {
        err() << "Failed to get JNIEnv; can't switch the keyboard visibility" << std::endl;
        return;
    }

    jint lFlags = 0;

    jclass ClassActivity = lJNIEnv->GetObjectClass(lActivityObj);

    // Retrieves Context.INPUT_METHOD_SERVICE
    jclass ClassContext = lJNIEnv->FindClass("android/content/Context");
    jfieldID FieldINPUT_METHOD_SERVICE = lJNIEnv->GetStaticFieldID(ClassContext,
        "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
    jobject INPUT_METHOD_SERVICE = lJNIEnv->GetStaticObjectField(ClassContext,
        FieldINPUT_METHOD_SERVICE);
    lJNIEnv->DeleteLocalRef(ClassContext);

    // Runs getSystemService(Context.INPUT_METHOD_SERVICE)
    jclass ClassInputMethodManager =
        lJNIEnv->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID MethodGetSystemService = lJNIEnv->GetMethodID(ClassActivity,
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject lInputMethodManager = lJNIEnv->CallObjectMethod(lActivityObj,
        MethodGetSystemService, INPUT_METHOD_SERVICE);
    lJNIEnv->DeleteLocalRef(INPUT_METHOD_SERVICE);

    // Runs getWindow().getDecorView()
    jmethodID MethodGetWindow = lJNIEnv->GetMethodID(ClassActivity,
        "getWindow", "()Landroid/view/Window;");
    jobject lWindow = lJNIEnv->CallObjectMethod(lActivityObj, MethodGetWindow);
    jclass ClassWindow = lJNIEnv->FindClass("android/view/Window");
    jmethodID MethodGetDecorView = lJNIEnv->GetMethodID(ClassWindow,
        "getDecorView", "()Landroid/view/View;");
    jobject lDecorView = lJNIEnv->CallObjectMethod(lWindow, MethodGetDecorView);
    lJNIEnv->DeleteLocalRef(lWindow);
    lJNIEnv->DeleteLocalRef(ClassWindow);

    if (visible)
    {
        // Runs lInputMethodManager.showSoftInput(...)
        jmethodID MethodShowSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager,
            "showSoftInput", "(Landroid/view/View;I)Z");
        lJNIEnv->CallBooleanMethod(lInputMethodManager,
            MethodShowSoftInput, lDecorView, lFlags);
    }
    else
    {
        // Runs lWindow.getViewToken()
        jclass ClassView = lJNIEnv->FindClass("android/view/View");
        jmethodID MethodGetWindowToken = lJNIEnv->GetMethodID(ClassView,
            "getWindowToken", "()Landroid/os/IBinder;");
        jobject lBinder = lJNIEnv->CallObjectMethod(lDecorView,
            MethodGetWindowToken);
        lJNIEnv->DeleteLocalRef(ClassView);

        // lInputMethodManager.hideSoftInput(...)
        jmethodID MethodHideSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager,
            "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
        lJNIEnv->CallBooleanMethod(lInputMethodManager,
            MethodHideSoftInput, lBinder, lFlags);
        lJNIEnv->DeleteLocalRef(lBinder);
    }
    lJNIEnv->DeleteLocalRef(ClassActivity);
    lJNIEnv->DeleteLocalRef(ClassInputMethodManager);
    lJNIEnv->DeleteLocalRef(lDecorView);

    if (attached)
        lJavaVM->DetachCurrentThread();
}

////////////////////////////////////////////////////////////
bool InputImpl::isMouseButtonPressed(Mouse::Button button)
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates& states = priv::getActivity();
    Lock lock(states.mutex);

    return states.isButtonPressed[button];
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition()
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates& states = priv::getActivity();
    Lock lock(states.mutex);

    return states.mousePosition;
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition(const WindowBase& /* relativeTo */)
{
    return getMousePosition();
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& /* position */)
{
    // Injecting events is impossible on Android
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position, const WindowBase& /* relativeTo */)
{
    setMousePosition(position);
}


////////////////////////////////////////////////////////////
bool InputImpl::isTouchDown(unsigned int finger)
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates& states = priv::getActivity();
    Lock lock(states.mutex);

    return states.touchEvents.find(static_cast<int>(finger)) != states.touchEvents.end();
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int finger)
{
    ALooper_pollAll(0, NULL, NULL, NULL);

    priv::ActivityStates& states = priv::getActivity();
    Lock lock(states.mutex);

    return states.touchEvents.find(static_cast<int>(finger))->second;
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int finger, const WindowBase& /* relativeTo */)
{
    return getTouchPosition(finger);
}

} // namespace priv

} // namespace sf


////////////////////////////////////////////////////////////
extern "C" void sf_android_set_hosted_virtual_keyboard_handler(void (*handler)(int visible))
{
    sf::android::setHostedVirtualKeyboardHandler(handler);
}
