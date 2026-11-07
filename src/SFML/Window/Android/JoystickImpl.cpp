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
#include <SFML/Window/JoystickImpl.hpp>
#include <SFML/System/Lock.hpp>

#include <android/keycodes.h>

#include <algorithm>
#include <vector>
#include <unordered_map>

namespace
{
    struct JoystickRecord
    {
        sf::Int32 deviceId = 0;
        sf::priv::JoystickState state {};
        float hatX = 0.f;           ///< Last AXIS_HAT_X, Android's sign
        float hatY = 0.f;           ///< Last AXIS_HAT_Y, Android's sign (-1 is up)
        unsigned int dpadMask = 0;  ///< JoystickDpad bits of the held AKEYCODE_DPAD_* keys
    };
    typedef std::vector<JoystickRecord> JoystickList;
    typedef std::unordered_map<sf::Int32, unsigned int> JoystickMapIndex;
    JoystickList joystickList;
    JoystickMapIndex joystickMapIndex;

    void lazyInitJoystick(sf::Int32 deviceId)
    {
        // Add a joystick in containers if it's not already done
        if (joystickMapIndex.count(deviceId) == 0)
        {
            JoystickList::iterator it = std::find_if(joystickList.begin(), joystickList.end(), [&deviceId] (const JoystickRecord& record) { return record.deviceId == deviceId; } );

            unsigned int index;
            if (it != joystickList.end())
            {
                index = std::distance(joystickList.begin(), it);
            }
            else
            {
                index = joystickList.size();
                joystickList.resize(index + 1);
            }

            joystickMapIndex[deviceId] = index;
            joystickList[index].deviceId = deviceId;
            joystickList[index].state.connected = true;
        }
    }

    JoystickRecord* getRecord(sf::Int32 deviceId)
    {
        if (joystickMapIndex.count(deviceId) == 0) {
            return nullptr;
        }
        unsigned int index = joystickMapIndex[deviceId];
        if (index >= joystickList.size()) {
            return nullptr;
        }
        return &joystickList[index];
    }

    // The D-pad directions of an AKEYCODE_DPAD_* key, 0 for any other key
    unsigned int androidKeyToDpad(int key)
    {
        using namespace sf::priv;
        switch (key)
        {
            case AKEYCODE_DPAD_UP:         return DpadUp;
            case AKEYCODE_DPAD_DOWN:       return DpadDown;
            case AKEYCODE_DPAD_LEFT:       return DpadLeft;
            case AKEYCODE_DPAD_RIGHT:      return DpadRight;
            // Diagonals, sent by some D-pads since Android 7
            case AKEYCODE_DPAD_UP_LEFT:    return DpadUp | DpadLeft;
            case AKEYCODE_DPAD_UP_RIGHT:   return DpadUp | DpadRight;
            case AKEYCODE_DPAD_DOWN_LEFT:  return DpadDown | DpadLeft;
            case AKEYCODE_DPAD_DOWN_RIGHT: return DpadDown | DpadRight;
            default:                       return 0;
        }
    }

    // The SFML button of a gamepad key, Joystick::ButtonCount for any other key
    unsigned int androidKeyToButton(int key)
    {
        switch (key)
        {
            case AKEYCODE_BUTTON_A:      return 0;
            case AKEYCODE_BUTTON_B:      return 1;
            case AKEYCODE_BUTTON_X:      return 2;
            case AKEYCODE_BUTTON_Y:      return 3;
            case AKEYCODE_BUTTON_L1:     return 4;
            case AKEYCODE_BUTTON_R1:     return 5;
            case AKEYCODE_BUTTON_L2:     return 6;
            case AKEYCODE_BUTTON_R2:     return 7;
            case AKEYCODE_BUTTON_THUMBL: return 8;
            case AKEYCODE_BUTTON_THUMBR: return 9;
            case AKEYCODE_BUTTON_START:  return 10;
            case AKEYCODE_BUTTON_SELECT: return 11;
            case AKEYCODE_BUTTON_MODE:   return 12;
            default:                     return sf::Joystick::ButtonCount;
        }
    }

    // PovX / PovY from the D-pad keys while one is held, else from the hat
    // axes. A controller sending D-pad keys still sends motion events for
    // its sticks, with hat axes at 0: they must not cancel a held direction.
    void updatePov(JoystickRecord& record)
    {
        using namespace sf::priv;
        sf::priv::JoystickState& state = record.state;
        if (record.dpadMask != 0)
        {
            const unsigned int mask = record.dpadMask;
            state.axes[sf::Joystick::PovX] = ((mask & DpadRight) ? 100.f : 0.f) - ((mask & DpadLeft) ? 100.f : 0.f);
            state.axes[sf::Joystick::PovY] = ((mask & DpadUp)    ? 100.f : 0.f) - ((mask & DpadDown) ? 100.f : 0.f);
        }
        else
        {
            // Android's hat Y is negative up, SFML's PovY is positive up
            // (as on Windows, and as PSDK expects)
            state.axes[sf::Joystick::PovX] = record.hatX * 100.f;
            state.axes[sf::Joystick::PovY] = -record.hatY * 100.f;
        }
    }
}

namespace sf
{
namespace priv
{
Mutex JoystickImpl::eventMutex {};
std::vector<JoystickEvent>  JoystickImpl::events {};

////////////////////////////////////////////////////////////
void JoystickImpl::initialize()
{
    // To implement
}



////////////////////////////////////////////////////////////
void JoystickImpl::cleanup()
{
    // To implement
}


////////////////////////////////////////////////////////////
bool JoystickImpl::isConnected(unsigned int index)
{
    bool alreadyConnected = index < joystickList.size() && joystickList[index].state.connected;
    if (alreadyConnected)
    {
        return true;
    }

    // Otherwise updates current joystick connection status from the events
    Lock guard(eventMutex);
    for (const JoystickEvent& event: events)
    {
        lazyInitJoystick(event.deviceId);
    }
    return index < joystickList.size() && joystickList[index].state.connected;
}


////////////////////////////////////////////////////////////
bool JoystickImpl::open(unsigned int index)
{
    if (index < joystickList.size())
    {
        joystickList[index].state.connected = true;
    }

    m_index = index;
    m_state.connected = true;

    return true;
}


////////////////////////////////////////////////////////////
void JoystickImpl::close()
{
    // To implement
}


////////////////////////////////////////////////////////////
JoystickCaps JoystickImpl::getCapabilities() const
{
    // TODO: find a real way to get caps ?
    JoystickCaps caps;
    caps.buttonCount = Joystick::ButtonCount;

    caps.axes[Joystick::X]    = true;
    caps.axes[Joystick::Y]    = true;
    caps.axes[Joystick::Z]    = true;
    caps.axes[Joystick::R]    = true;
    caps.axes[Joystick::U]    = true;
    caps.axes[Joystick::V]    = true;
    caps.axes[Joystick::PovX] = true;
    caps.axes[Joystick::PovY] = true;

    return caps;
}


////////////////////////////////////////////////////////////
Joystick::Identification JoystickImpl::getIdentification() const
{
    return m_identification;
}


////////////////////////////////////////////////////////////
JoystickState JoystickImpl::update()
{
    {
        // Global update of all joysticks
        Lock guard(eventMutex);
        for (const JoystickEvent& event: events)
        {
            lazyInitJoystick(event.deviceId);
            JoystickRecord* record = getRecord(event.deviceId);
            if (record != nullptr) {
                JoystickState& state = record->state;
                switch (event.type)
                {
                    case JoystickEventType::Key:
                        if (event.index < Joystick::ButtonCount) {
                            state.buttons[event.index] = event.pressed;
                        }
                        state.connected = true;
                        break;
                    case JoystickEventType::Dpad:
                        if (event.pressed) {
                            record->dpadMask |= event.index;
                        } else {
                            record->dpadMask &= ~event.index;
                        }
                        updatePov(*record);
                        state.connected = true;
                        break;
                    case JoystickEventType::Motion:
                        state.axes[Joystick::X] = event.motion.xAxis * 100.f;
                        state.axes[Joystick::Y] = event.motion.yAxis * 100.f;
                        state.axes[Joystick::Z] = event.motion.zAxis * 100.f;
                        state.axes[Joystick::R] = event.motion.rzAxis * 100.f;
                        state.axes[Joystick::U] = event.motion.leftTrigger * 100.f;
                        state.axes[Joystick::V] = event.motion.rightTrigger * 100.f;
                        record->hatX = event.motion.xHatAxis;
                        record->hatY = event.motion.yHatAxis;
                        updatePov(*record);
                        state.connected = true;
                        break;
                    case JoystickEventType::Connection:
                        state.connected = event.pressed;
                        if (!event.pressed) {
                            // Nothing stays held across a reconnection
                            record->dpadMask = 0;
                            record->hatX = record->hatY = 0.f;
                            updatePov(*record);
                        }
                        break;
                    default:
                        // Not supported
                        break;
                }
            }
        }
        events.clear();
    }

    // Specific update of the current one
    if (m_index < joystickList.size())
    {
        m_state = joystickList[m_index].state;
    }
    else
    {
        m_state.connected = false;
    }

    return m_state;
}

void JoystickImpl::pushEvent(JoystickEvent event)
{
    Lock guard(eventMutex);
    events.push_back(std::move(event));
}


////////////////////////////////////////////////////////////
bool JoystickImpl::pushKeyEvent(Int32 deviceId, int androidKeyCode, bool pressed)
{
    const unsigned int dpad = androidKeyToDpad(androidKeyCode);
    if (dpad != 0)
    {
        pushEvent(JoystickEvent { deviceId, JoystickEventType::Dpad, dpad, pressed });
        return true;
    }

    const unsigned int button = androidKeyToButton(androidKeyCode);
    if (button < Joystick::ButtonCount)
    {
        pushEvent(JoystickEvent { deviceId, JoystickEventType::Key, button, pressed });
        return true;
    }

    return false;
}

} // namespace priv

} // namespace sf
