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
#include <SFML/Audio/AudioDevice.hpp>
#include <SFML/Audio/Android/HostedAudio.hpp>
#include <SFML/Audio/ALCheck.hpp>
#include <SFML/Audio/Listener.hpp>
#include <SFML/System/Err.hpp>
#include <condition_variable>
#include <mutex>
#include <vector>

#if defined(__APPLE__)
    #if defined(__clang__)
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    #elif defined(__GNUC__)
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #endif
#endif

namespace
{
    ALCdevice*  audioDevice  = NULL;
    ALCcontext* audioContext = NULL;

    float        listenerVolume = 100.f;
    sf::Vector3f listenerPosition (0.f, 0.f, 0.f);
    sf::Vector3f listenerDirection(0.f, 0.f, -1.f);
    sf::Vector3f listenerUpVector (0.f, 1.f, 0.f);

    // AudioDevice::setPaused's state; pauseMutex also guards audioDevice
    // against a pause from another thread while it is opened or closed
    std::mutex              pauseMutex;
    std::condition_variable pauseChanged;
    bool                    outputPaused = false;
    // The device really stopped mixing: only then may the streams wait
    // (a device still playing would drain their queues)
    bool                    devicePaused = false;

    // Apply outputPaused to the device. Caller holds pauseMutex.
    void applyDevicePause()
    {
        devicePaused = false;
        if (!audioDevice || alcIsExtensionPresent(audioDevice, "ALC_SOFT_pause_device") == ALC_FALSE)
            return;

        typedef void (ALC_APIENTRY *DevicePauseFunc)(ALCdevice*);
        DevicePauseFunc pauseOrResume = reinterpret_cast<DevicePauseFunc>(
            alcGetProcAddress(audioDevice, outputPaused ? "alcDevicePauseSOFT" : "alcDeviceResumeSOFT"));
        if (pauseOrResume)
        {
            pauseOrResume(audioDevice);
            devicePaused = outputPaused;
        }
    }
}

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
AudioDevice::AudioDevice()
{
    std::lock_guard<std::mutex> lock(pauseMutex);

    // Create the device
    audioDevice = alcOpenDevice(NULL);

    if (audioDevice)
    {
        // Create the context
        audioContext = alcCreateContext(audioDevice, NULL);

        if (audioContext)
        {
            // Set the context as the current one (we'll only need one)
            alcMakeContextCurrent(audioContext);

            // Apply the listener properties the user might have set
            float orientation[] = {listenerDirection.x,
                                   listenerDirection.y,
                                   listenerDirection.z,
                                   listenerUpVector.x,
                                   listenerUpVector.y,
                                   listenerUpVector.z};
            alCheck(alListenerf(AL_GAIN, listenerVolume * 0.01f));
            alCheck(alListener3f(AL_POSITION, listenerPosition.x, listenerPosition.y, listenerPosition.z));
            alCheck(alListenerfv(AL_ORIENTATION, orientation));

            if (outputPaused)
                applyDevicePause();
        }
        else
        {
            err() << "Failed to create the audio context" << std::endl;
        }
    }
    else
    {
        err() << "Failed to open the audio device" << std::endl;
    }
}


////////////////////////////////////////////////////////////
AudioDevice::~AudioDevice()
{
    std::lock_guard<std::mutex> lock(pauseMutex);

    // Destroy the context
    alcMakeContextCurrent(NULL);
    if (audioContext)
        alcDestroyContext(audioContext);
    audioContext = NULL;

    // Destroy the device (NULL after: setPaused may come later)
    if (audioDevice)
        alcCloseDevice(audioDevice);
    audioDevice = NULL;
    devicePaused = false;
}


////////////////////////////////////////////////////////////
bool AudioDevice::isExtensionSupported(const std::string& extension)
{
    // Create a temporary audio device in case none exists yet.
    // This device will not be used in this function and merely
    // makes sure there is a valid OpenAL device for extension
    // queries if none has been created yet.
    //
    // Using an std::vector for this since auto_ptr is deprecated
    // and we have no better STL facility for dynamically allocating
    // a temporary instance with strong exception guarantee.
    std::vector<AudioDevice> device;
    if (!audioDevice)
        device.resize(1);

    if ((extension.length() > 2) && (extension.substr(0, 3) == "ALC"))
        return alcIsExtensionPresent(audioDevice, extension.c_str()) != AL_FALSE;
    else
        return alIsExtensionPresent(extension.c_str()) != AL_FALSE;
}


////////////////////////////////////////////////////////////
int AudioDevice::getFormatFromChannelCount(unsigned int channelCount)
{
    // Create a temporary audio device in case none exists yet.
    // This device will not be used in this function and merely
    // makes sure there is a valid OpenAL device for format
    // queries if none has been created yet.
    //
    // Using an std::vector for this since auto_ptr is deprecated
    // and we have no better STL facility for dynamically allocating
    // a temporary instance with strong exception guarantee.
    std::vector<AudioDevice> device;
    if (!audioDevice)
        device.resize(1);

    // Find the good format according to the number of channels
    int format = 0;
    switch (channelCount)
    {
        case 1:  format = AL_FORMAT_MONO16;                    break;
        case 2:  format = AL_FORMAT_STEREO16;                  break;
        case 4:  format = alGetEnumValue("AL_FORMAT_QUAD16");  break;
        case 6:  format = alGetEnumValue("AL_FORMAT_51CHN16"); break;
        case 7:  format = alGetEnumValue("AL_FORMAT_61CHN16"); break;
        case 8:  format = alGetEnumValue("AL_FORMAT_71CHN16"); break;
        default: format = 0;                                   break;
    }

    // Fixes a bug on OS X
    if (format == -1)
        format = 0;

    return format;
}


////////////////////////////////////////////////////////////
void AudioDevice::setGlobalVolume(float volume)
{
    if (audioContext)
        alCheck(alListenerf(AL_GAIN, volume * 0.01f));

    listenerVolume = volume;
}


////////////////////////////////////////////////////////////
float AudioDevice::getGlobalVolume()
{
    return listenerVolume;
}


////////////////////////////////////////////////////////////
void AudioDevice::setPosition(const Vector3f& position)
{
    if (audioContext)
        alCheck(alListener3f(AL_POSITION, position.x, position.y, position.z));

    listenerPosition = position;
}


////////////////////////////////////////////////////////////
Vector3f AudioDevice::getPosition()
{
    return listenerPosition;
}


////////////////////////////////////////////////////////////
void AudioDevice::setDirection(const Vector3f& direction)
{
    if (audioContext)
    {
        float orientation[] = {direction.x, direction.y, direction.z, listenerUpVector.x, listenerUpVector.y, listenerUpVector.z};
        alCheck(alListenerfv(AL_ORIENTATION, orientation));
    }

    listenerDirection = direction;
}


////////////////////////////////////////////////////////////
Vector3f AudioDevice::getDirection()
{
    return listenerDirection;
}


////////////////////////////////////////////////////////////
void AudioDevice::setUpVector(const Vector3f& upVector)
{
    if (audioContext)
    {
        float orientation[] = {listenerDirection.x, listenerDirection.y, listenerDirection.z, upVector.x, upVector.y, upVector.z};
        alCheck(alListenerfv(AL_ORIENTATION, orientation));
    }

    listenerUpVector = upVector;
}


////////////////////////////////////////////////////////////
Vector3f AudioDevice::getUpVector()
{
    return listenerUpVector;
}

////////////////////////////////////////////////////////////
void AudioDevice::setPaused(bool paused)
{
    {
        std::lock_guard<std::mutex> lock(pauseMutex);
        if (outputPaused == paused)
            return;

        outputPaused = paused;
        applyDevicePause();
    }
    pauseChanged.notify_all();
}


////////////////////////////////////////////////////////////
bool AudioDevice::waitWhilePaused(const std::function<bool()>& isStreaming)
{
    std::unique_lock<std::mutex> lock(pauseMutex);
    if (!devicePaused)
        return false;

    pauseChanged.wait(lock, [&] { return !devicePaused || !isStreaming(); });
    return true;
}


////////////////////////////////////////////////////////////
void AudioDevice::wakePausedStreams()
{
    // Under the mutex: a stream checking isStreaming right now waits
    // before this notification, not after it
    {
        std::lock_guard<std::mutex> lock(pauseMutex);
    }
    pauseChanged.notify_all();
}

} // namespace priv

} // namespace sf


#ifdef SFML_SYSTEM_ANDROID
////////////////////////////////////////////////////////////
extern "C" SFML_AUDIO_API void sf_android_set_hosted_audio_paused(int paused)
{
    sf::priv::AudioDevice::setPaused(paused != 0);
}
#endif
