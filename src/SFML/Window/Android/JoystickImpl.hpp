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

#ifndef SFML_JOYSTICKIMPLANDROID_HPP
#define SFML_JOYSTICKIMPLANDROID_HPP

#include <SFML/System/Mutex.hpp>
#include <vector>

namespace sf
{
namespace priv
{

struct JoystickMotionData
{
    float xHatAxis = 0.f;
    float yHatAxis = 0.f;
    float leftTrigger = 0.f;
    float rightTrigger = 0.f;
    float xAxis = 0.f;
    float yAxis = 0.f;
    float zAxis = 0.f;
    float rzAxis = 0.f;
};

enum class JoystickEventType
{
    Key,
    Motion,
    Connection
};
struct JoystickEvent
{
    Int32 deviceId = 0;
    JoystickEventType type;
    unsigned int index = Joystick::ButtonCount;
    bool pressed = false;
    JoystickMotionData motion {};
};

////////////////////////////////////////////////////////////
/// \brief Android implementation of joysticks
///
////////////////////////////////////////////////////////////
class JoystickImpl
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Perform the global initialization of the joystick module
    ///
    ////////////////////////////////////////////////////////////
    static void initialize();

    ////////////////////////////////////////////////////////////
    /// \brief Perform the global cleanup of the joystick module
    ///
    ////////////////////////////////////////////////////////////
    static void cleanup();

    ////////////////////////////////////////////////////////////
    /// \brief Check if a joystick is currently connected
    ///
    /// \param index Index of the joystick to check
    ///
    /// \return True if the joystick is connected, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    static bool isConnected(unsigned int index);

    ////////////////////////////////////////////////////////////
    /// \brief Open the joystick
    ///
    /// \param index Index assigned to the joystick
    ///
    /// \return True on success, false on failure
    ///
    ////////////////////////////////////////////////////////////
    bool open(unsigned int index);

    ////////////////////////////////////////////////////////////
    /// \brief Close the joystick
    ///
    ////////////////////////////////////////////////////////////
    void close();

    ////////////////////////////////////////////////////////////
    /// \brief Get the joystick capabilities
    ///
    /// \return Joystick capabilities
    ///
    ////////////////////////////////////////////////////////////
    JoystickCaps getCapabilities() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the joystick identification
    ///
    /// \return Joystick identification
    ///
    ////////////////////////////////////////////////////////////
    Joystick::Identification getIdentification() const;

    ////////////////////////////////////////////////////////////
    /// \brief Update the joystick and get its new state
    ///
    /// \return Joystick state
    ///
    ////////////////////////////////////////////////////////////
    JoystickState update();

    /// \brief Enqueue a new event to be updated next frame
    ///
    ////////////////////////////////////////////////////////////
    static void pushEvent(JoystickEvent event);

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Joystick::Identification           m_identification;  ///< Joystick identification
    JoystickState                      m_state;           ///< Buffered joystick state
    unsigned int                       m_index;           ///< Device index
    static Mutex                       eventMutex;        ///< Event mutex
    static std::vector<JoystickEvent>  events;            ///< Events to be dispatched
};

} // namespace priv

} // namespace sf


#endif // SFML_JOYSTICKIMPLANDROID_HPP
