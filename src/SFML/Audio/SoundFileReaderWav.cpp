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
#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4242 4244 4267 4456 4706)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include <dr_wav.h>

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

#undef DR_WAV_NO_STDIO
#undef DR_WAV_IMPLEMENTATION

#include <SFML/Audio/SoundFileReaderWav.hpp>
#include <SFML/System/InputStream.hpp>
#include <SFML/System/Err.hpp>
#include <algorithm>
#include <cstring>


namespace
{
    std::size_t readCallback(void* pUserData, void* pBufferOut, std::size_t bytesToRead)
    {
        sf::InputStream* stream = static_cast<sf::InputStream*>(pUserData);
        sf::Int64 bytesRead = stream->read(pBufferOut, static_cast<sf::Int64>(bytesToRead));
        if (bytesRead < 0)
            return 0;
        return static_cast<std::size_t>(bytesRead);
    }

    drwav_bool32 seekCallback(void* pUserData, int offset, drwav_seek_origin origin)
    {
        sf::InputStream* stream = static_cast<sf::InputStream*>(pUserData);
        sf::Int64 base = 0;
        switch (origin)
        {
            case DRWAV_SEEK_SET: base = 0; break;
            case DRWAV_SEEK_CUR: base = stream->tell(); break;
            case DRWAV_SEEK_END: base = stream->getSize(); break;
            default: return DRWAV_FALSE;
        }
        if (base < 0)
            return DRWAV_FALSE;

        sf::Int64 target = base + static_cast<sf::Int64>(offset);
        return stream->seek(target) >= 0 ? DRWAV_TRUE : DRWAV_FALSE;
    }

    drwav_bool32 tellCallback(void* pUserData, drwav_int64* pCursor)
    {
        sf::InputStream* stream = static_cast<sf::InputStream*>(pUserData);
        sf::Int64 position = stream->tell();
        if (position < 0)
            return DRWAV_FALSE;
        *pCursor = static_cast<drwav_int64>(position);
        return DRWAV_TRUE;
    }
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
bool SoundFileReaderWav::check(InputStream& stream)
{
    // Same lightweight RIFF/WAVE magic probe as before — full validation
    // happens in open() via drwav_init().
    char header[12];
    if (stream.read(header, sizeof(header)) < static_cast<Int64>(sizeof(header)))
        return false;

    return (header[0] == 'R') && (header[1] == 'I') && (header[2] == 'F') && (header[3] == 'F')
        && (header[8] == 'W') && (header[9] == 'A') && (header[10] == 'V') && (header[11] == 'E');
}


////////////////////////////////////////////////////////////
SoundFileReaderWav::SoundFileReaderWav() :
m_decoder      (),
m_initialized  (false),
m_channelCount (0)
{
    std::memset(&m_decoder, 0, sizeof(m_decoder));
}


////////////////////////////////////////////////////////////
SoundFileReaderWav::~SoundFileReaderWav()
{
    if (m_initialized)
    {
        drwav_uninit(&m_decoder);
        m_initialized = false;
    }
}


////////////////////////////////////////////////////////////
bool SoundFileReaderWav::open(InputStream& stream, Info& info)
{
    // dr_wav reads from the beginning, but check() leaves the stream just past the RIFF/WAVE magic.
    if (stream.seek(0) < 0)
    {
        err() << "Failed to open WAV sound file (could not rewind stream)" << std::endl;
        return false;
    }

    if (!drwav_init(&m_decoder, &readCallback, &seekCallback, &tellCallback, &stream, NULL))
    {
        err() << "Failed to open WAV sound file (invalid or unsupported file)" << std::endl;
        return false;
    }
    m_initialized = true;

    m_channelCount    = static_cast<unsigned int>(m_decoder.channels);
    info.channelCount = m_channelCount;
    info.sampleRate   = static_cast<unsigned int>(m_decoder.sampleRate);
    info.sampleCount  = static_cast<Uint64>(m_decoder.totalPCMFrameCount) * m_channelCount;

    err() << "[WAV debug] channels=" << m_decoder.channels
          << " sampleRate=" << m_decoder.sampleRate
          << " bitsPerSample=" << m_decoder.bitsPerSample
          << " totalPCMFrameCount=" << m_decoder.totalPCMFrameCount
          << " translatedFormatTag=0x" << std::hex << m_decoder.translatedFormatTag << std::dec
          << " -> info.sampleCount=" << info.sampleCount
          << std::endl;

    return true;
}


////////////////////////////////////////////////////////////
void SoundFileReaderWav::seek(Uint64 sampleOffset)
{
    if (!m_initialized || m_channelCount == 0)
        return;

    drwav_uint64 frame = sampleOffset / m_channelCount;
    drwav_seek_to_pcm_frame(&m_decoder, frame);
}


////////////////////////////////////////////////////////////
Uint64 SoundFileReaderWav::read(Int16* samples, Uint64 maxCount)
{
    if (!m_initialized || m_channelCount == 0)
        return 0;

    drwav_uint64 framesToRead = static_cast<drwav_uint64>(maxCount) / m_channelCount;
    if (framesToRead == 0)
        return 0;

    drwav_uint64 framesRead = drwav_read_pcm_frames_s16(&m_decoder, framesToRead, samples);
    return static_cast<Uint64>(framesRead) * m_channelCount;
}

} // namespace priv

} // namespace sf
