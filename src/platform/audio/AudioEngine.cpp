// Soul Knight Linux — Audio Engine Implementation (miniaudio 0.11.x)
#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.hpp"
#include "miniaudio.h"
#include <engine/core/Log.hpp>
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

    ma_engine_config config = ma_engine_config_init();
    config.channels = config_.channels;
    config.sampleRate = config_.sample_rate;
    config.periodSizeInFrames = config_.buffer_size;
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

void AudioEngine::StopAll(Bus bus) {
    // miniaudio doesn't have bus-level stop, would need to track all sounds
    (void)bus;
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