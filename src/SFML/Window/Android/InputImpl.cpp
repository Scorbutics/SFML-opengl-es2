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
#include <SFML/System/Android/Activity.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <jni.h>


namespace sf
{
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

Keyboard::Key InputImpl::localize(Keyboard::Scancode /* code */)
{
    // Not applicable
    return Keyboard::Unknown;
}

Keyboard::Scancode InputImpl::delocalize(Keyboard::Key /* key */)
{
    // Not applicable
    return Keyboard::Scan::Unknown;
}

String InputImpl::getDescription(Keyboard::Scancode /* code */)
{
    // Not applicable
    return "";
}

////////////////////////////////////////////////////////////
void InputImpl::setVirtualKeyboardVisible(bool visible)
{
    // todo: Check if the window is active

    ActivityStates& states = getActivity();
    Lock lock(states.mutex);

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
