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
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/Window/InputImpl.hpp>
#include <SFML/Window/Switch/WindowImplSwitch.hpp>
#include <SFML/System/String.hpp>
#include <SFML/System/Utf.hpp>
#include <SFML/System/Err.hpp>

#include <string>
#include <switch.h>


namespace sf
{
namespace priv
{

namespace
{
    SwkbdInline s_kbd;
    bool        s_kbdCreated = false;
    bool        s_kbdActive  = false;
    std::string s_prevString;  // shadow of swkbd's buffer, UTF-8
    String      s_initialText;

    void emitTextEvent(Uint32 codepoint)
    {
        Event ev;
        ev.type = Event::TextEntered;
        ev.text.unicode = codepoint;
        WindowImplSwitch::forwardEvent(ev);
    }

    // Walk prev vs next codepoint-by-codepoint. Emit backspaces for codepoints
    // beyond the common prefix in prev, then TextEntered for codepoints beyond
    // the common prefix in next.
    void diffAndEmit(const std::string& prev, const std::string& next)
    {
        const char* pPrev    = prev.data();
        const char* pPrevEnd = pPrev + prev.size();
        const char* pNext    = next.data();
        const char* pNextEnd = pNext + next.size();

        // Common prefix
        while (pPrev < pPrevEnd && pNext < pNextEnd)
        {
            Uint32 cp1 = 0, cp2 = 0;
            const char* pPrevAdv = Utf<8>::decode(pPrev, pPrevEnd, cp1);
            const char* pNextAdv = Utf<8>::decode(pNext, pNextEnd, cp2);
            if (cp1 != cp2) break;
            pPrev = pPrevAdv;
            pNext = pNextAdv;
        }

        // Remaining in prev -> backspaces
        while (pPrev < pPrevEnd)
        {
            Uint32 cp = 0;
            pPrev = Utf<8>::decode(pPrev, pPrevEnd, cp);
            emitTextEvent(0x08);  // '\b'
        }

        // Remaining in next -> text entered
        while (pNext < pNextEnd)
        {
            Uint32 cp = 0;
            pNext = Utf<8>::decode(pNext, pNextEnd, cp);
            if (cp != 0) emitTextEvent(cp);
        }
    }

    void onChangedString(const char* str, SwkbdChangedStringArg*)
    {
        std::string current = str ? str : "";
        diffAndEmit(s_prevString, current);
        s_prevString = current;
    }

    void onDecidedEnter(const char* str, SwkbdDecidedEnterArg*)
    {
        // Catch any unsynced edits
        std::string current = str ? str : "";
        diffAndEmit(s_prevString, current);
        s_prevString = current;

        // PSDK's NameInput checks for ord 10/13 in the text stream to confirm
        emitTextEvent(0x0D);  // '\r'

        swkbdInlineDisappear(&s_kbd);
        s_kbdActive = false;
        s_prevString.clear();
    }

    void onDecidedCancel()
    {
        // Treat cancel as confirm (Switch swkbd offers no way to suppress cancel)
        emitTextEvent(0x0D);
        swkbdInlineDisappear(&s_kbd);
        s_kbdActive = false;
        s_prevString.clear();
    }

    bool ensureCreated()
    {
        if (s_kbdCreated) return true;

        Result rc = swkbdInlineCreate(&s_kbd);
        if (R_FAILED(rc))
        {
            err() << "swkbdInlineCreate failed: 0x" << std::hex << rc << std::dec << std::endl;
            return false;
        }
        rc = swkbdInlineLaunchForLibraryApplet(&s_kbd, SwkbdInlineMode_AppletDisplay, 0);
        if (R_FAILED(rc))
        {
            err() << "swkbdInlineLaunchForLibraryApplet failed: 0x" << std::hex << rc << std::dec << std::endl;
            swkbdInlineClose(&s_kbd);
            return false;
        }

        swkbdInlineSetChangedStringCallback(&s_kbd, onChangedString);
        swkbdInlineSetDecidedEnterCallback (&s_kbd, onDecidedEnter);
        swkbdInlineSetDecidedCancelCallback(&s_kbd, onDecidedCancel);

        s_kbdCreated = true;
        return true;
    }
}

////////////////////////////////////////////////////////////
bool InputImpl::isKeyPressed(Keyboard::Key key)
{
    // Not applicable
    return false;
}

bool InputImpl::isKeyPressed(Keyboard::Scancode codes)
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
    if (visible)
    {
        if (!ensureCreated()) return;

        // convert SFML String to libnx UTF8 string
        std::basic_string<Uint8> u8 = s_initialText.toUtf8();
        std::string initialUtf8(u8.begin(), u8.end());

        // sync shadow BEFORE swkbdInlineSetInputText, so the first
        // ChangedString callback (which fires with the initial text) diffs
        // against the same string and emits no events.
        s_prevString = initialUtf8;

        swkbdInlineSetInputText(&s_kbd, initialUtf8.c_str());

        SwkbdAppearArg appearArg;
        swkbdInlineMakeAppearArg(&appearArg, SwkbdType_Normal);


        swkbdInlineAppear(&s_kbd, &appearArg);
        s_kbdActive = true;
    }
    else
    {
        if (s_kbdActive)
        {
            swkbdInlineDisappear(&s_kbd);
            s_kbdActive = false;
            s_prevString.clear();
        }
    }
}

////////////////////////////////////////////////////////////
void InputImpl::setVirtualKeyboardInitState(String text)
{
    s_initialText = text;
}

////////////////////////////////////////////////////////////
void InputImpl::processVirtualKeyboardEvents()
{
    if (s_kbdCreated && s_kbdActive)
    {
        swkbdInlineUpdate(&s_kbd, nullptr);
    }
}

////////////////////////////////////////////////////////////
bool InputImpl::isMouseButtonPressed(Mouse::Button button)
{
    // TODO: use touch?

    return false;
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition()
{
    // TODO: use touch?
    return {0, 0};
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getMousePosition(const WindowBase& relativeTo)
{
    // TODO: use touch?
    return {0, 0};
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position)
{
    // Injecting events is impossible on Android
}


////////////////////////////////////////////////////////////
void InputImpl::setMousePosition(const Vector2i& position, const WindowBase& relativeTo)
{
    // Not implemented
}


////////////////////////////////////////////////////////////
bool InputImpl::isTouchDown(unsigned int finger)
{
    HidTouchScreenState state = {0};
    if (hidGetTouchScreenStates(&state, 1))
        return (finger < (unsigned int)state.count);
    else
        return (false);
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int finger)
{
    HidTouchScreenState state = {0};
    if (hidGetTouchScreenStates(&state, 1) && finger < (unsigned int)state.count)
        return sf::Vector2i(state.touches[finger].x, state.touches[finger].y);
    else
        return sf::Vector2i(0, 0);
}


////////////////////////////////////////////////////////////
Vector2i InputImpl::getTouchPosition(unsigned int finger, const WindowBase& relativeTo)
{
    return getTouchPosition(finger);
}

} // namespace priv

} // namespace sf
