#ifndef CVDSP_CONVOLUTION_IRLOADER_HPP
#define CVDSP_CONVOLUTION_IRLOADER_HPP

/**
 * @file IRLoader.hpp
 * @brief Impulse Response WAV Loader
 *
 * Header-Only
 * C++20
 *
 * Supported:
 * - PCM 16-bit
 * - PCM 24-bit
 * - IEEE Float 32-bit
 * - Mono WAV
 * - Stereo WAV
 *
 * Designed for:
 * - Cabinet IR
 * - Room IR
 * - Hall IR
 * - Reverb IR
 *
 * No allocations during audio processing.
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>

namespace cvdsp
{

template<
    typename T,
    std::size_t MaxSamples = 262144>
class IRLoader
{
public:

    IRLoader() = default;

    /**
     * Load WAV file.
     */
    bool load(
        const std::string& filePath,
        bool normalize = true,
        bool trimSilence = true,
        T trimThreshold =
            static_cast<T>(1e-5))
    {
        unload();

        std::ifstream file(
            filePath,
            std::ios::binary);

        if (!file)
        {
            return false;
        }

        if (!readHeader(file))
        {
            return false;
        }

        if (!readData(file))
        {
            return false;
        }

        if (normalize)
        {
            normalizeIR();
        }

        if (trimSilence)
        {
            trimTail(
                trimThreshold);
        }

        return true;
    }

    /**
     * Unload current IR.
     */
    void unload() noexcept
    {
        length_ = 0;

        sampleRate_ = 0;

        numChannels_ = 0;

        for (auto& s : left_)
            s = T(0);

        for (auto& s : right_)
            s = T(0);
    }

    /**
     * Mono access.
     */
    [[nodiscard]]
    const T* getSamples() const noexcept
    {
        return left_.data();
    }

    /**
     * Channel access.
     */
    [[nodiscard]]
    const T* getChannel(
        std::size_t channel) const noexcept
    {
        if (channel == 0)
        {
            return left_.data();
        }

        return right_.data();
    }

    /**
     * Length in samples.
     */
    [[nodiscard]]
    std::size_t getLength() const noexcept
    {
        return length_;
    }

    /**
     * Number of channels.
     */
    [[nodiscard]]
    std::uint16_t getNumChannels()
        const noexcept
    {
        return numChannels_;
    }

    /**
     * Sample rate.
     */
    [[nodiscard]]
    std::uint32_t getSampleRate()
        const noexcept
    {
        return sampleRate_;
    }

    /**
     * Stereo?
     */
    [[nodiscard]]
    bool isStereo() const noexcept
    {
        return numChannels_ == 2;
    }

    /**
     * Empty?
     */
    [[nodiscard]]
    bool isLoaded() const noexcept
    {
        return length_ > 0;
    }

private:

#pragma pack(push, 1)

    struct RIFFHeader
    {
        char chunkID[4];
        std::uint32_t chunkSize;
        char format[4];
    };

    struct ChunkHeader
    {
        char id[4];
        std::uint32_t size;
    };

    struct FormatChunk
    {
        std::uint16_t audioFormat;
        std::uint16_t numChannels;
        std::uint32_t sampleRate;
        std::uint32_t byteRate;
        std::uint16_t blockAlign;
        std::uint16_t bitsPerSample;
    };

#pragma pack(pop)

private:

    bool readHeader(
        std::ifstream& file)
    {
        RIFFHeader riff{};

        file.read(
            reinterpret_cast<char*>(&riff),
            sizeof(riff));

        if (!file)
        {
            return false;
        }

        if (std::strncmp(
                riff.chunkID,
                "RIFF",
                4) != 0)
        {
            return false;
        }

        if (std::strncmp(
                riff.format,
                "WAVE",
                4) != 0)
        {
            return false;
        }

        bool foundFmt = false;
        bool foundData = false;

        while (file &&
               (!foundFmt || !foundData))
        {
            ChunkHeader chunk{};

            file.read(
                reinterpret_cast<char*>(&chunk),
                sizeof(chunk));

            if (!file)
            {
                return false;
            }

            if (std::strncmp(
                    chunk.id,
                    "fmt ",
                    4) == 0)
            {
                file.read(
                    reinterpret_cast<char*>(&format_),
                    sizeof(FormatChunk));

                if (chunk.size >
                    sizeof(FormatChunk))
                {
                    file.seekg(
                        chunk.size -
                        sizeof(FormatChunk),
                        std::ios::cur);
                }

                foundFmt = true;
            }
            else if (
                std::strncmp(
                    chunk.id,
                    "data",
                    4) == 0)
            {
                dataOffset_ =
                    static_cast<std::uint64_t>(
                        file.tellg());

                dataSize_ =
                    chunk.size;

                file.seekg(
                    chunk.size,
                    std::ios::cur);

                foundData = true;
            }
            else
            {
                file.seekg(
                    chunk.size,
                    std::ios::cur);
            }
        }

        if (!foundFmt ||
            !foundData)
        {
            return false;
        }

        sampleRate_ =
            format_.sampleRate;

        numChannels_ =
            format_.numChannels;

        return true;
    }

    bool readData(
        std::ifstream& file)
    {
        if (numChannels_ < 1 ||
            numChannels_ > 2)
        {
            return false;
        }

        file.clear();

        file.seekg(
            dataOffset_,
            std::ios::beg);

        const std::size_t bytesPerSample =
            format_.bitsPerSample / 8;

        const std::size_t totalFrames =
            dataSize_
            /
            (
                bytesPerSample
                *
                numChannels_);

        length_ =
            std::min(
                totalFrames,
                MaxSamples);

        switch(
            format_.audioFormat)
        {
            case 1:
                return readPCM(file);

            case 3:
                return readFloat(file);

            default:
                return false;
        }
    }

    bool readPCM(
        std::ifstream& file)
    {
        if (format_.bitsPerSample == 16)
        {
            return readPCM16(file);
        }

        if (format_.bitsPerSample == 24)
        {
            return readPCM24(file);
        }

        return false;
    }

    bool readPCM16(
        std::ifstream& file)
    {
        for (std::size_t i = 0;
             i < length_;
             ++i)
        {
            std::int16_t leftSample;

            file.read(
                reinterpret_cast<char*>(
                    &leftSample),
                sizeof(leftSample));

            left_[i] =
                static_cast<T>(
                    leftSample)
                /
                static_cast<T>(32768);

            if (numChannels_ == 2)
            {
                std::int16_t rightSample;

                file.read(
                    reinterpret_cast<char*>(
                        &rightSample),
                    sizeof(rightSample));

                right_[i] =
                    static_cast<T>(
                        rightSample)
                    /
                    static_cast<T>(32768);
            }
        }

        return true;
    }

    bool readPCM24(
        std::ifstream& file)
    {
        for (std::size_t i = 0;
             i < length_;
             ++i)
        {
            left_[i] =
                read24Bit(file);

            if (numChannels_ == 2)
            {
                right_[i] =
                    read24Bit(file);
            }
        }

        return true;
    }

    bool readFloat(
        std::ifstream& file)
    {
        if (format_.bitsPerSample != 32)
        {
            return false;
        }

        for (std::size_t i = 0;
             i < length_;
             ++i)
        {
            float leftSample;

            file.read(
                reinterpret_cast<char*>(
                    &leftSample),
                sizeof(float));

            left_[i] =
                static_cast<T>(
                    leftSample);

            if (numChannels_ == 2)
            {
                float rightSample;

                file.read(
                    reinterpret_cast<char*>(
                        &rightSample),
                    sizeof(float));

                right_[i] =
                    static_cast<T>(
                        rightSample);
            }
        }

        return true;
    }

    T read24Bit(
        std::ifstream& file)
    {
        unsigned char bytes[3];

        file.read(
            reinterpret_cast<char*>(bytes),
            3);

        std::int32_t sample =
            bytes[0]
            |
            (bytes[1] << 8)
            |
            (bytes[2] << 16);

        if (sample &
            0x800000)
        {
            sample |=
                ~0xFFFFFF;
        }

        return
            static_cast<T>(sample)
            /
            static_cast<T>(8388608.0);
    }

    void normalizeIR()
    {
        T peak = T(0);

        for (std::size_t i = 0;
             i < length_;
             ++i)
        {
            peak =
                std::max(
                    peak,
                    std::abs(
                        left_[i]));

            if (numChannels_ == 2)
            {
                peak =
                    std::max(
                        peak,
                        std::abs(
                            right_[i]));
            }
        }

        if (peak <= T(0))
        {
            return;
        }

        const T gain =
            T(1) / peak;

        for (std::size_t i = 0;
             i < length_;
             ++i)
        {
            left_[i] *= gain;

            if (numChannels_ == 2)
            {
                right_[i] *= gain;
            }
        }
    }

    void trimTail(
        T threshold)
    {
        if (length_ == 0)
        {
            return;
        }

        std::size_t last =
            length_ - 1;

        while (last > 0)
        {
            bool silent =
                std::abs(
                    left_[last])
                < threshold;

            if (numChannels_ == 2)
            {
                silent &=
                    std::abs(
                        right_[last])
                    < threshold;
            }

            if (!silent)
            {
                break;
            }

            --last;
        }

        length_ = last + 1;
    }

private:

    FormatChunk format_{};

    std::uint64_t dataOffset_ = 0;

    std::uint32_t dataSize_ = 0;

    std::uint32_t sampleRate_ = 0;

    std::uint16_t numChannels_ = 0;

    std::size_t length_ = 0;

    std::array<T, MaxSamples>
        left_{};

    std::array<T, MaxSamples>
        right_{};
};

} // namespace cvdsp

#endif
