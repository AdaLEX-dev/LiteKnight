// Soul Knight Linux — Audio Engine (miniaudio wrapper)
#pragma once

#include <string>
#include <memory>
#include <array>
#include <engine/core/Types.hpp>

namespace soulknight::platform {

class AudioEngine {
public:
    enum class Bus : uint8_t {
        Master = 0,
        Music = 1,
        SFX = 2,
        UI = 3,
        Ambience = 4,
        COUNT = 5
    };

    enum class SoundType { Static, Streaming };

    struct Config {
        int sample_rate = 48000;
        int channels = 2;
        int buffer_size = 1024;
        int num_periods = 3;
    };

    struct SoundHandle {
        SoundHandle() = default;
        SoundHandle(void* sound, bool streaming) : sound_(sound), streaming_(streaming) {}
        SoundHandle(const SoundHandle&) = delete;
        SoundHandle(SoundHandle&& other) noexcept : sound_(other.sound_), streaming_(other.streaming_) { other.sound_ = nullptr; }
        SoundHandle& operator=(SoundHandle&& other) noexcept {
            if (this != &other) {
                if (valid()) Uninit();
                sound_ = other.sound_;
                streaming_ = other.streaming_;
                other.sound_ = nullptr;
            }
            return *this;
        }
        SoundHandle& operator=(const SoundHandle&) = delete;
        ~SoundHandle() { if (valid()) Uninit(); }

        bool valid() const { return sound_ != nullptr; }

        void Play();
        void Stop();
        // Note: miniaudio 0.11.x doesn't have pause/resume, use Stop/Play instead
        // void Pause();
        // void Resume();
        bool IsPlaying() const;

        void SetVolume(float volume);      // 0.0 - 1.0 (multiplied by bus volume)
        void SetPitch(float pitch);        // 0.5 - 2.0
        void SetPan(float pan);            // -1.0 (left) to 1.0 (right)
        void SetLooping(bool loop);

        // Streaming only
        float GetDuration() const;         // Seconds
        float GetCurrentTime() const;      // Seconds
        void Seek(float seconds);

    private:
        friend class AudioEngine;
        void* sound_ = nullptr;
        bool streaming_ = false;
        void Init(void* engine, const char* path, Bus bus, SoundType type);
        void Uninit();
    };

    AudioEngine() = default;
    ~AudioEngine() { Shutdown(); }

    bool Initialize(const Config& cfg);
    void Shutdown();

    // Volume control (0.0 - 1.0)
    void SetBusVolume(Bus bus, float volume);
    float GetBusVolume(Bus bus) const;
    void SetMasterVolume(float volume);

    // Sound playback
    SoundHandle PlayStatic(const std::string& path, Bus bus, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
    SoundHandle PlayStreaming(const std::string& path, Bus bus, float volume = 1.0f, bool loop = true);

    // Engine control
    void PauseAll();
    void ResumeAll();
    // Engine-wide stop. Bus-specific stop is not implemented; do not expose a
    // parameter that implies otherwise.
    void StopAll();

    // Stats (NOT IMPLEMENTED: currently returns zeros; no voice tracking exists)
    struct Stats { int active_voices = 0; int peak_voices = 0; };
    Stats GetStats() const;

private:
    void* engine_ = nullptr;
    Config config_;
    std::array<float, 5> bus_volumes_ = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace soulknight::platform