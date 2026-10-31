// Soul Knight Linux — Audio Engine Implementation (miniaudio 0.11.x)
#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.hpp"
// Vendored C header: suppress the project's strict C++ warning set for its
// implementation. Warnings in our own code below remain enabled.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wlogical-op"
#pragma GCC diagnostic ignored "-Wformat=2"
#endif
#include "miniaudio.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <engine/core/Log.hpp>
#include <algorithm>
#include <cstring>

namespace soulknight::platform {

// Internal sound data
struct SoundData {
    ma_sound sound;
    bool streaming = false;
    bool in_use = true;
};

// AudioEngine implementation
bool AudioEngine::Initialize(const Config& cfg) {
    config_ = cfg;

    if (config_.channels <= 0 || config_.sample_rate <= 0 || config_.buffer_size <= 0) {
        LOG_ERROR_AUDIO("Invalid audio configuration: channels={}, sample_rate={}, buffer_size={}",
                        config_.channels, config_.sample_rate, config_.buffer_size);
        return false;
    }

    ma_engine_config config = ma_engine_config_init();
    config.channels = static_cast<ma_uint32>(config_.channels);
    config.sampleRate = static_cast<ma_uint32>(config_.sample_rate);
    config.periodSizeInFrames = static_cast<ma_uint32>(config_.buffer_size);
    config.noAutoStart = true;

    // miniaudio 0.11.x: backend selection is automatic, no manual backend selection
    ma_engine* eng = new ma_engine();
    engine_ = eng;

    ma_result result = ma_engine_init(&config, eng);
    if (result != MA_SUCCESS) {
        delete eng;
        engine_ = nullptr;
        LOG_ERROR_AUDIO("Failed to initialize miniaudio: {}", static_cast<int>(result));
        return false;
    }

    // Start engine
    ma_engine_start(eng);

    LOG_INFO_AUDIO("Audio engine initialized ({} Hz, {} ch)",
                   config_.sample_rate, config_.channels);
    return true;
}

void AudioEngine::Shutdown() {
    if (engine_) {
        ma_engine* eng = static_cast<ma_engine*>(engine_);
        ma_engine_stop(eng);
        ma_engine_uninit(eng);
        delete eng;
        engine_ = nullptr;
    }
}

void AudioEngine::SetBusVolume(Bus bus, float volume) {
    if (bus < Bus::COUNT) {
        bus_volumes_[static_cast<size_t>(bus)] = std::clamp(volume, 0.0f, 1.0f);
        // miniaudio doesn't have built-in bus system, we apply volume per-sound
    }
}

float AudioEngine::GetBusVolume(Bus bus) const {
    if (bus < Bus::COUNT) return bus_volumes_[static_cast<size_t>(bus)];
    return 0.0f;
}

void AudioEngine::SetMasterVolume(float volume) {
    if (engine_) {
        ma_engine* eng = static_cast<ma_engine*>(engine_);
        ma_engine_set_volume(eng, std::clamp(volume, 0.0f, 1.0f));
    }
}

void AudioEngine::SoundHandle::Init(void* engine, const char* path, Bus bus, SoundType type) {
    (void)bus; // Bus volume is applied by AudioEngine::Play* after Init.
    ma_engine* eng = static_cast<ma_engine*>(engine);
    ma_sound* s = new ma_sound();
    sound_ = s;
    streaming_ = (type == SoundType::Streaming);

    ma_sound_config config = ma_sound_config_init();
    config.pFilePath = path;
    config.flags = MA_SOUND_FLAG_DECODE;

    if (type == SoundType::Streaming) {
        config.flags |= MA_SOUND_FLAG_STREAM;
    }

    ma_result result = ma_sound_init_ex(eng, &config, s);
    if (result != MA_SUCCESS) {
        delete s;
        sound_ = nullptr;
        LOG_ERROR_AUDIO("Failed to load sound: {} (error: {})", path, static_cast<int>(result));
        return;
    }

    ma_sound_set_volume(s, 1.0f);
}

void AudioEngine::SoundHandle::Play() {
    if (sound_) ma_sound_start(static_cast<ma_sound*>(sound_));
}

void AudioEngine::SoundHandle::Stop() {
    if (sound_) ma_sound_stop(static_cast<ma_sound*>(sound_));
}

bool AudioEngine::SoundHandle::IsPlaying() const {
    return sound_ && ma_sound_is_playing(static_cast<const ma_sound*>(sound_));
}

void AudioEngine::SoundHandle::SetVolume(float volume) {
    if (sound_) ma_sound_set_volume(static_cast<ma_sound*>(sound_), std::clamp(volume, 0.0f, 1.0f));
}

void AudioEngine::SoundHandle::SetPitch(float pitch) {
    if (sound_) ma_sound_set_pitch(static_cast<ma_sound*>(sound_), std::clamp(pitch, 0.01f, 4.0f));
}

void AudioEngine::SoundHandle::SetPan(float pan) {
    if (sound_) ma_sound_set_pan(static_cast<ma_sound*>(sound_), std::clamp(pan, -1.0f, 1.0f));
}

void AudioEngine::SoundHandle::SetLooping(bool loop) {
    if (sound_) ma_sound_set_looping(static_cast<ma_sound*>(sound_), loop ? MA_TRUE : MA_FALSE);
}

float AudioEngine::SoundHandle::GetDuration() const {
    float seconds = 0.0f;
    if (sound_ && ma_sound_get_length_in_seconds(static_cast<const ma_sound*>(sound_), &seconds) != MA_SUCCESS) {
        return 0.0f;
    }
    return seconds;
}

float AudioEngine::SoundHandle::GetCurrentTime() const {
    float seconds = 0.0f;
    if (sound_ && ma_sound_get_cursor_in_seconds(static_cast<const ma_sound*>(sound_), &seconds) != MA_SUCCESS) {
        return 0.0f;
    }
    return seconds;
}

void AudioEngine::SoundHandle::Seek(float seconds) {
    if (sound_) ma_sound_seek_to_second(static_cast<ma_sound*>(sound_), seconds);
}

void AudioEngine::SoundHandle::Uninit() {
    if (sound_) {
        auto* s = static_cast<ma_sound*>(sound_);
        ma_sound_uninit(s);
        delete s;
        sound_ = nullptr;
    }
}

AudioEngine::SoundHandle AudioEngine::PlayStatic(const std::string& path, Bus bus, float volume, float pitch, bool loop) {
    SoundHandle handle;
    handle.Init(engine_, path.c_str(), bus, SoundType::Static);
    if (handle.valid()) {
        ma_sound* s = static_cast<ma_sound*>(handle.sound_);
        ma_sound_set_volume(s, volume * GetBusVolume(bus));
        ma_sound_set_pitch(s, pitch);
        ma_sound_set_looping(s, loop);
        ma_sound_start(s);
    }
    return handle;
}

AudioEngine::SoundHandle AudioEngine::PlayStreaming(const std::string& path, Bus bus, float volume, bool loop) {
    SoundHandle handle;
    handle.Init(engine_, path.c_str(), bus, SoundType::Streaming);
    if (handle.valid()) {
        ma_sound* s = static_cast<ma_sound*>(handle.sound_);
        ma_sound_set_volume(s, volume * GetBusVolume(bus));
        ma_sound_set_looping(s, loop);
        ma_sound_start(s);
    }
    return handle;
}

void AudioEngine::PauseAll() {
    if (engine_) {
        ma_engine* eng = static_cast<ma_engine*>(engine_);
        ma_engine_stop(eng);
    }
}

void AudioEngine::ResumeAll() {
    if (engine_) {
        ma_engine* eng = static_cast<ma_engine*>(engine_);
        ma_engine_start(eng);
    }
}

void AudioEngine::StopAll() {
    // Engine-wide stop. Bus-level tracking is not implemented.
    if (engine_) {
        ma_engine* eng = static_cast<ma_engine*>(engine_);
        ma_engine_stop(eng);
    }
}

AudioEngine::Stats AudioEngine::GetStats() const {
    Stats stats{};
    if (engine_) {
        // miniaudio doesn't expose voice count directly
        stats.active_voices = 0;
        stats.peak_voices = 0;
    }
    return stats;
}

} // namespace soulknight::platform