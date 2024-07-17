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

#include <vector>
#include <unordered_map>

namespace
{
    struct JoystickRecord
    {
        sf::Int32 deviceId = 0;
        sf::priv::JoystickState state {};
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

    sf::priv::JoystickState* getState(sf::Int32 deviceId)
    {
        if (joystickMapIndex.count(deviceId) == 0) {
            return nullptr;
        }
        unsigned int index = joystickMapIndex[deviceId];
        if (index >= joystickList.size()) {
            return nullptr;
        }
        return &joystickList[index].state;
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
            JoystickState* statePtr = getState(event.deviceId);
            if (statePtr != nullptr) {
                JoystickState& state = *statePtr;
                switch (event.type)
                {
                    case JoystickEventType::Key:
                        if (event.index < Joystick::ButtonCount) {
                            state.buttons[event.index] = event.pressed;
                        }
                        state.connected = true;
                        break;
                    case JoystickEventType::Motion:
                        state.axes[Joystick::X] = event.motion.xAxis * 100.f;
                        state.axes[Joystick::Y] = event.motion.yAxis * 100.f;
                        state.axes[Joystick::Z] = event.motion.zAxis * 100.f;
                        state.axes[Joystick::R] = event.motion.rzAxis * 100.f;
                        state.axes[Joystick::U] = event.motion.leftTrigger * 100.f;
                        state.axes[Joystick::V] = event.motion.rightTrigger * 100.f;
                        state.axes[Joystick::PovX] = event.motion.xHatAxis * 100.f;
                        state.axes[Joystick::PovY] = event.motion.yHatAxis * 100.f;
                        state.connected = true;
                        break;
                    case JoystickEventType::Connection:
                        state.connected = event.pressed;
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

} // namespace priv

} // namespace sf
