# Soul Knight 2.6.8 — Audio Design

**Target:** miniaudio, PipeWire/PulseAudio/ALSA, 48kHz, 16-32 voices

---

## 1. Audio Architecture

### Bus Structure
```
Master (0.0 - 1.0)
  ├─ Music (0.0 - 1.0)      → BGM, looping
  ├─ SFX (0.0 - 1.0)        → Weapons, impacts, movement
  ├─ UI (0.0 - 1.0)         → Menu clicks, hover, notifications
  └─ Ambience (0.0 - 1.0)   → Room tone, environmental
```

### Voice Management
- **Music:** 1 voice (streaming, looping)
- **Ambience:** 1-2 voices (streaming, crossfade)
- **SFX:** 16-32 voices (pooled, priority-based)
- **UI:** 4 voices (short, high priority)

---

## 2. Audio Engine Wrapper

```cpp
// src/platform/audio/AudioEngine.h
class AudioEngine {
public:
    struct Config {
        int sample_rate = 48000;
        int channels = 2;
        int buffer_size = 1024;  // frames
        int num_periods = 3;
    };
    
    enum class Bus : uint8_t { Master = 0, Music = 1, SFX = 2, UI = 3, Ambience = 4, COUNT = 5 };
    enum class SoundType { Static, Streaming };
    
    bool Initialize(const Config& cfg = {});
    void Shutdown();
    
    // Volume control (0.0 - 1.0)
    void SetBusVolume(Bus bus, float volume);
    float GetBusVolume(Bus bus) const;
    void SetMasterVolume(float volume);
    
    // Sound playback
    SoundHandle PlayStatic(const std::string& path, Bus bus, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
    SoundHandle PlayStreaming(const std::string& path, Bus bus, float volume = 1.0f, bool loop = true);
    
    // Listener (for future 3D)
    void SetListenerPosition(const Vec2& pos);
    void SetListenerOrientation(float angle_rad);
    
    // Engine control
    void PauseAll();
    void ResumeAll();
    void StopAll(Bus bus = Bus::COUNT);  // COUNT = all
    
    // Debug
    struct Stats { int active_voices = 0; int peak_voices = 0; };
    Stats GetStats() const;

private:
    ma_engine engine_;
    ma_engine_config config_;
    std::array<float, 5> bus_volumes_ = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};
```

### Sound Handle
```cpp
// src/platform/audio/SoundHandle.h
class SoundHandle {
public:
    SoundHandle() = default;
    SoundHandle(ma_sound sound, bool streaming) : sound_(sound), streaming_(streaming) {}
    SoundHandle(const SoundHandle&) = delete;
    SoundHandle(SoundHandle&& other) noexcept : sound_(other.sound_), streaming_(other.streaming_) { other.sound_ = {}; }
    ~SoundHandle() { if (valid()) ma_sound_uninit(&sound_); }
    
    bool valid() const { return sound_.pDataSource != nullptr; }
    
    void Play();
    void Stop();
    void Pause();
    void Resume();
    bool IsPlaying() const;
    
    void SetVolume(float volume);      // 0.0 - 1.0 (multiplied by bus volume)
    void SetPitch(float pitch);        // 0.5 - 2.0
    void SetPan(float pan);            // -1.0 (left) to 1.0 (right)
    void SetLooping(bool loop);
    void SetPosition(const Vec2& pos); // For 3D (future)
    
    float GetVolume() const;
    float GetPitch() const;
    float GetPan() const;
    bool IsLooping() const;
    
    // Streaming only
    float GetDuration() const;         // Seconds
    float GetCurrentTime() const;      // Seconds
    void Seek(float seconds);

private:
    ma_sound sound_;
    bool streaming_ = false;
};
```

---

## 3. Game Audio System

### Audio Manager (Game-Level)
```cpp
// src/game/audio/AudioManager.h
class AudioManager {
public:
    enum class SoundID : uint16_t {
        // UI
        UI_Click,
        UI_Hover,
        UI_Back,
        UI_Error,
        UI_LevelUp,
        UI_Unlock,
        
        // Player
        Player_Hurt,
        Player_Death,
        Player_Dodge,
        Player_Heal,
        Player_Pickup,
        Player_WeaponSwitch,
        
        // Weapons (per weapon type)
        Weapon_Gun_Shot,
        Weapon_Gun_Reload,
        Weapon_Gun_Empty,
        Weapon_Melee_Swing,
        Weapon_Melee_Hit,
        Weapon_Laser_Charge,
        Weapon_Laser_Fire,
        Weapon_Explosion,
        Weapon_Magic_Cast,
        
        // Enemies
        Enemy_Hurt,
        Enemy_Death,
        Enemy_Attack_Melee,
        Enemy_Attack_Ranged,
        Enemy_Spawn,
        Enemy_Boss_Roar,
        Enemy_Boss_PhaseChange,
        
        // Environment
        Chest_Open,
        Chest_Locked,
        Door_Open,
        Door_Close,
        Teleport_In,
        Teleport_Out,
        Explosion_Small,
        Explosion_Large,
        Fire_Burn,
        Electric_Zap,
        Poison_Tick,
        Ice_Freeze,
        
        // Music (streaming)
        Music_Menu,
        Music_Dungeon_1,
        Music_Dungeon_2,
        Music_Boss,
        Music_Victory,
        Music_GameOver,
        
        // Ambience
        Ambience_Dungeon,
        Ambience_BossRoom,
        Ambience_SafeRoom,
        
        COUNT
    };
    
    struct SoundDef {
        SoundID id;
        std::string path;           // Relative to assets/audio/
        AudioEngine::Bus bus;
        float volume = 1.0f;
        float pitch = 1.0f;
        bool loop = false;
        bool stream = false;        // Music/ambience
        uint8_t priority = 128;     // 0-255 (higher = more important)
        uint8_t max_instances = 4;  // For SFX pooling
        float min_distance = 1.0f;  // For 3D
        float max_distance = 50.0f;
    };
    
    static const std::array<SoundDef, static_cast<size_t>(SoundID::COUNT)>& GetSoundDefs();
    
    // Playback
    SoundHandle Play(SoundID id, const Vec2* position = nullptr, float volume_mult = 1.0f);
    SoundHandle PlayAt(SoundID id, const Vec2& position, float volume_mult = 1.0f);
    void Stop(SoundID id);
    void StopAll();
    
    // Music control
    void PlayMusic(SoundID id, float fade_in = 1.0f);
    void StopMusic(float fade_out = 1.0f);
    void CrossfadeMusic(SoundID from, SoundID to, float duration = 2.0f);
    void SetMusicVolume(float volume);  // 0.0 - 1.0
    
    // Ambience
    void SetAmbience(SoundID id, float fade_in = 2.0f);
    void StopAmbience(float fade_out = 2.0f);
    
    // Listener (player position)
    void SetListenerPosition(const Vec2& pos);
    
    // Config
    void SetMasterVolume(float v);
    void SetSFXVolume(float v);
    void SetMusicVolume(float v);
    void SetUIVolume(float v);
    void SetAmbienceVolume(float v);
    
private:
    AudioEngine engine_;
    std::vector<SoundHandle> active_sounds_;
    SoundHandle current_music_;
    SoundHandle current_ambience_;
    Vec2 listener_pos_ = {0, 0};
    
    // Sound pool for SFX
    struct PooledSound {
        SoundHandle handle;
        SoundID id;
        float last_used = 0;
        bool in_use = false;
    };
    std::array<std::vector<PooledSound>, static_cast<size_t>(SoundID::COUNT)> pools_;
};
```

### Priority-Based Voice Stealing
```cpp
// When all voices busy, steal lowest priority
SoundHandle AudioManager::Play(SoundID id, const Vec2* pos, float vol_mult) {
    const auto& def = GetSoundDef(id);
    
    if (def.stream) {
        // Music/ambience — single instance
        return engine_.PlayStreaming(def.path, def.bus, def.volume * vol_mult, def.loop);
    }
    
    // SFX — use pool
    auto& pool = pools_[static_cast<size_t>(id)];
    
    // Find free or steal
    PooledSound* best = nullptr;
    for (auto& ps : pool) {
        if (!ps.in_use || !ps.handle.IsPlaying()) {
            best = &ps;
            break;
        }
        if (!best || ps.handle.GetPriority() < best->handle.GetPriority()) {
            best = &ps;
        }
    }
    
    if (!best) {
        // Expand pool if under max_instances
        if (pool.size() < def.max_instances) {
            pool.emplace_back();
            best = &pool.back();
        }
    }
    
    if (best) {
        if (best->in_use) best->handle.Stop();
        best->handle = engine_.PlayStatic(def.path, def.bus, def.volume * vol_mult, def.pitch, def.loop);
        best->id = id;
        best->in_use = true;
        best->last_used = GetTimeSeconds();
        
        // 3D positioning
        if (pos) {
            best->handle.SetPosition(*pos);
        }
        
        return best->handle;
    }
    
    return {}; // Invalid handle
}
```

---

## 4. Audio Assets

### Format Requirements
| Type | Format | Sample Rate | Channels | Bitrate/Quality |
|------|--------|-------------|----------|-----------------|
| Music | Ogg Vorbis | 48 kHz | Stereo | 128-192 kbps |
| Ambience | Ogg Vorbis | 48 kHz | Stereo | 96-128 kbps |
| SFX | Ogg Vorbis / Opus | 48 kHz | Mono | 64-96 kbps |
| UI | WAV (PCM) | 48 kHz | Mono | 16-bit (short, no decode) |

### Naming Convention
```
assets/audio/
├── music/
│   ├── menu.ogg
│   ├── dungeon_01.ogg
│   ├── dungeon_02.ogg
│   ├── boss.ogg
│   ├── victory.ogg
│   └── gameover.ogg
├── ambience/
│   ├── dungeon.ogg
│   ├── boss_room.ogg
│   └── safe_room.ogg
├── sfx/
│   ├── ui/
│   │   ├── click.ogg
│   │   ├── hover.ogg
│   │   └── error.ogg
│   ├── player/
│   │   ├── hurt.ogg
│   │   ├── death.ogg
│   │   ├── dodge.ogg
│   │   └── pickup.ogg
│   ├── weapons/
│   │   ├── gun_shot.ogg
│   │   ├── gun_reload.ogg
│   │   ├── melee_swing.ogg
│   │   └── laser_charge.ogg
│   ├── enemies/
│   │   ├── hurt.ogg
│   │   ├── death.ogg
│   │   └── attack.ogg
│   └── environment/
│       ├── chest_open.ogg
│       ├── explosion.ogg
│       └── teleport.ogg
└── voice/ (if any)
```

---

## 5. Audio Triggers (From RE)

| Event | Sound(s) | Bus | Notes |
|-------|----------|-----|-------|
| Player shoot | Weapon_Gun_Shot | SFX | Per weapon type |
| Player melee | Weapon_Melee_Swing + Weapon_Melee_Hit | SFX | Swing on anim start, hit on contact |
| Player dodge | Player_Dodge | SFX | |
| Player hurt | Player_Hurt | SFX | Pitch vary ±10% |
| Player death | Player_Death | SFX | |
| Player heal | Player_Heal | SFX | |
| Enemy hurt | Enemy_Hurt | SFX | Pitch vary by enemy size |
| Enemy death | Enemy_Death | SFX | |
| Enemy melee attack | Enemy_Attack_Melee | SFX | |
| Enemy ranged attack | Enemy_Attack_Ranged | SFX | |
| Boss phase change | Enemy_Boss_PhaseChange | SFX | Crossfade music |
| Chest open | Chest_Open | SFX | |
| Door open/close | Door_Open/Close | SFX | |
| Teleport | Teleport_In/Out | SFX | |
| Explosion | Explosion_Small/Large | SFX | |
| Element: Fire | Fire_Burn | SFX | Looping on burn |
| Element: Electric | Electric_Zap | SFX | |
| Element: Poison | Poison_Tick | SFX | Periodic |
| Element: Ice | Ice_Freeze | SFX | |
| Level up | UI_LevelUp | UI | |
| Unlock | UI_Unlock | UI | |
| Menu click | UI_Click | UI | |
| Menu hover | UI_Hover | UI | |
| Error | UI_Error | UI | |

---

## 6. Dynamic Music System

```cpp
class DynamicMusic {
    enum class Layer { Base, Drums, Melody, Intensity };
    
    struct Stem {
        SoundID id;
        float volume = 1.0f;
        bool active = false;
    };
    
    std::array<Stem, 4> stems_;
    float target_intensity_ = 0.0f;
    float current_intensity_ = 0.0f;
    
    void SetIntensity(float intensity) {  // 0.0 (calm) to 1.0 (combat)
        target_intensity_ = std::clamp(intensity, 0.0f, 1.0f);
    }
    
    void Update(float dt) {
        current_intensity_ = Lerp(current_intensity_, target_intensity_, dt * 2.0f);
        
        // Fade stems based on intensity
        stems_[0].volume = 1.0f;  // Base always
        stems_[1].volume = current_intensity_;           // Drums
        stems_[2].volume = current_intensity_ * 0.7f;    // Melody
        stems_[3].volume = current_intensity_ * current_intensity_; // Intensity
        
        for (auto& stem : stems_) {
            if (stem.handle.valid()) {
                stem.handle.SetVolume(stem.volume * master_music_volume_);
            }
        }
    }
    
    void OnCombatStart() { SetIntensity(1.0f); }
    void OnCombatEnd() { SetIntensity(0.0f); }
    void OnBossPhase(int phase) { SetIntensity(0.5f + phase * 0.2f); }
};
```

---

## 7. Configuration

```toml
# config/audio.toml
[volume]
master = 1.0
music = 0.8
sfx = 1.0
ui = 0.7
ambience = 0.6

[quality]
sample_rate = 48000
buffer_size = 1024

[behavior]
pause_on_focus_loss = true
duck_music_on_sfx = false  # Priority handles this
crossfade_duration = 2.0

[3d]
enabled = false  # Future: enable for positional audio
doppler_factor = 0.0
rolloff_factor = 1.0
```

---

## 8. Asset Pipeline

```bash
# Convert extracted assets to target formats
# Music: ffmpeg -i input.mp3 -c:a libvorbis -b:a 128k output.ogg
# SFX: ffmpeg -i input.wav -c:a libopus -b:a 64k -ac 1 output.ogg
# UI: ffmpeg -i input.wav -c:a pcm_s16le -ar 48000 -ac 1 output.wav

# Verify
ffprobe -show_streams output.ogg
```

---

## 9. Testing

```cpp
TEST("Audio engine initializes") {
    AudioEngine engine;
    CHECK(engine.Initialize());
    CHECK(engine.GetBusVolume(AudioEngine::Bus::Master) == 1.0f);
}

TEST("SFX pooling works") {
    AudioManager mgr;
    mgr.Initialize();
    
    // Play more than pool size
    for (int i = 0; i < 10; i++) {
        auto h = mgr.Play(AudioManager::SoundID::Weapon_Gun_Shot);
        CHECK(h.valid());
    }
    
    // Oldest should be stolen
    CHECK(mgr.GetActiveVoiceCount() <= 32);
}

TEST("Music crossfade") {
    AudioManager mgr;
    mgr.PlayMusic(AudioManager::SoundID::Music_Menu);
    mgr.CrossfadeMusic(AudioManager::SoundID::Music_Menu, AudioManager::SoundID::Music_Dungeon_1, 0.5f);
    // Verify both playing during crossfade
}
```

---

## 10. Platform Integration

### miniaudio Backend Priority
```cpp
// Try PipeWire first (Wayland native), then PulseAudio, then ALSA
ma_backend backends[] = {
    ma_backend_pipewire,
    ma_backend_pulseaudio,
    ma_backend_alsa,
    ma_backend_null
};
```

### SDL3 Audio (Not Used)
- We use miniaudio directly for full control
- SDL3 audio subsystem NOT initialized (`SDL_INIT_AUDIO` omitted)

---

**Status:** DESIGN COMPLETE — Ready for implementation in Phase 4.