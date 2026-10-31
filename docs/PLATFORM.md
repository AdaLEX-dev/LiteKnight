# Soul Knight 2.6.8 — Platform Layer Design

> **Current implementation (2026-10-31):** the Phase 4A-4E foundation is
> implemented. `Application` owns SDL3/wayland enforcement, window state, Vulkan
> renderer lifecycle, the 50 Hz/60 FPS loop, and shutdown. The window is
> Wayland-only; a Vulkan 1.3 device/swapchain, a multi-texture sprite renderer,
> a libpng → RGBA8 asset path, controlled Unity Texture2D/Sprite conversion,
> Camera2D, an actual `r1_1`+`c00` visual composition, and real idle animation are
> present. Gate C is
> **PARTIAL** — launch/present/resize are manually verified; input/gameplay are
> not. Evidence: `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`,
> `docs/ASSET_PIPELINE.md`, `analysis/assets/sprites.md`.
>
> **Status:** mixed. Sections 1-3 describe the implemented platform contract;
> later snippets remain design guidance. Confirmed reference facts: fixed timestep
> 0.02 s, Physics2D gravity `(0,-9.81)`, legacy input, Wayland-only target.

**Target:** CachyOS, x86-64, Wayland, SDL3, Vulkan 1.3

---

## 1. Window Management (SDL3)

### Window Creation
```cpp
// src/platform/wayland/Window.cpp
struct WindowConfig {
    int width = 1920;
    int height = 1080;
    bool fullscreen = false;
    bool vsync = true;           // Mailbox preferred, FIFO fallback
    bool high_dpi = true;
    std::string title = "Soul Knight";
};

class Window {
    SDL_Window* sdl_window_ = nullptr;
    WindowState state_;   // implemented: resize/minimize/focus/close transitions
    // VkSurfaceKHR is created through SDL_Vulkan_CreateSurface and owned by
    // VulkanContext, not by Window.

    // Wayland-specific: no X11 fallback
    // SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR=1
    // SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY=1
};
```

### Wayland-Specific Hints
```cpp
// Set before SDL_Init
SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "1");
SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland"); // Force Wayland; no X11 fallback
```

### Surface Creation
```cpp
VkSurfaceKHR CreateVulkanSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(sdl_window_, instance, nullptr, &surface)) {
        // The project builds with -fno-exceptions; return an invalid surface.
        return VK_NULL_HANDLE;
    }
    return surface;
}
```

---

## 2. Input Abstraction

### Action Mapping (Data-Driven)
```toml
# config/input.toml
[keyboard]
MoveUp = ["W", "Up"]
MoveDown = ["S", "Down"]
MoveLeft = ["A", "Left"]
MoveRight = ["D", "Right"]
Attack = ["Space", "J"]
Skill = ["LeftShift", "K"]
Dodge = ["LeftCtrl", "L"]
Interact = ["E", "F"]
Inventory = ["Tab", "I"]
Pause = ["Escape", "P"]
Confirm = ["Enter", "Space"]
Cancel = ["Escape", "Backspace"]

[gamepad]
MoveUp = "DPadUp"
MoveDown = "DPadDown"
MoveLeft = "DPadLeft"
MoveRight = "DPadRight"
Attack = "A"
Skill = "B"
Dodge = "X"
Interact = "Y"
Inventory = "Start"
Pause = "Guide"
Confirm = "A"
Cancel = "B"

# Left stick = movement (analog)
# Right stick = aim (if twin-stick)
```

### Input Snapshot (Per Frame)
```cpp
// src/platform/input/InputManager.cpp
struct InputSnapshot {
    // Digital (pressed this frame)
    std::bitset<Action::COUNT> pressed;
    std::bitset<Action::COUNT> released;
    std::bitset<Action::COUNT> held;

    // Analog
    Vec2 move_axis = {0, 0};      // -1..1
    Vec2 aim_axis = {0, 0};       // -1..1
    float trigger_left = 0.0f;    // 0..1
    float trigger_right = 0.0f;   // 0..1

    // Mouse (if used)
    Vec2 mouse_pos = {0, 0};
    Vec2 mouse_delta = {0, 0};
    bool mouse_pressed[3] = {false};

    // Window events
    bool window_focused = true;
    bool window_resized = false;
    Vec2 window_size = {1920, 1080};
};

class InputManager {
    InputSnapshot current_;
    InputSnapshot previous_;
    std::unordered_map<SDL_Scancode, Action> key_map_;
    std::unordered_map<SDL_GamepadButton, Action> gamepad_map_;

    void ProcessSDLEvent(const SDL_Event& event);
    InputSnapshot GetSnapshot() const;
};
```

---

## 3. Frame Timing

### Fixed Timestep Loop
```cpp
// src/platform/timing/FrameTimer.cpp
class FrameTimer {
    static constexpr double TARGET_DT = 1.0 / 50.0;  // 20 ms / 50 Hz confirmed
    static constexpr double MAX_ACCUMULATOR = 0.3333333333;  // reference max allowed timestep

    double accumulator_ = 0.0;
    double current_time_ = 0.0;

    struct FrameStats {
        double frame_time_ms = 0;
        double sim_time_ms = 0;
        double render_time_ms = 0;
        int sim_steps = 0;
    } stats_;

    void Tick() {
        double new_time = GetTimeSeconds();
        double frame_time = new_time - current_time_;
        current_time_ = new_time;

        // Clamp spiral of death
        if (frame_time > MAX_ACCUMULATOR) frame_time = MAX_ACCUMULATOR;
        accumulator_ += frame_time;

        stats_.frame_time_ms = frame_time * 1000.0;
        stats_.sim_steps = 0;

        // Fixed simulation steps
        while (accumulator_ >= TARGET_DT) {
            SimStep(TARGET_DT);
            accumulator_ -= TARGET_DT;
            stats_.sim_steps++;
        }

        // Interpolation alpha for rendering
        double alpha = accumulator_ / TARGET_DT;
        RenderStep(alpha);
    }

    void SimStep(double dt);      // Physics, AI, gameplay
    void RenderStep(double alpha); // Interpolated render
};
```

### High-Resolution Timer
```cpp
// Linux: clock_gettime(CLOCK_MONOTONIC_RAW)
inline double GetTimeSeconds() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
```

---

## 4. File System (XDG Compliance)

### Paths
```cpp
// src/platform/filesystem/FileSystem.cpp
class FileSystem {
    std::string data_home_;      // $XDG_DATA_HOME/soul-knight/ or ~/.local/share/soul-knight/
    std::string config_home_;    // $XDG_CONFIG_HOME/soul-knight/ or ~/.config/soul-knight/
    std::string cache_home_;     // $XDG_CACHE_HOME/soul-knight/ or ~/.cache/soul-knight/

    // Subdirectories
    std::string saves_dir_;      // data_home/saves/
    std::string assets_dir_;     // data_home/assets/ (converted native assets)
    std::string logs_dir_;       // cache_home/logs/
    std::string config_file_;    // config_home/config.toml
};
```

### Asset Loading
```cpp
// Priority: 1. User overrides (data_home/assets/)
//           2. Built-in (executable directory /assets/)
//           3. System install (/usr/share/soul-knight/assets/)

std::optional<std::vector<uint8_t>> ReadFile(const std::string& relative_path);
std::optional<std::string> ReadTextFile(const std::string& relative_path);
```

---

## 5. Audio Backend (miniaudio)

### Initialization
```cpp
// src/platform/audio/AudioEngine.cpp
class AudioEngine {
    ma_engine engine_;
    ma_engine_config config_;

    // Bus indices
    enum Bus : uint32_t {
        MASTER = 0,
        MUSIC = 1,
        SFX = 2,
        UI = 3,
        AMBIENCE = 4,
        COUNT = 5
    };

    bool Init() {
        config_ = ma_engine_config_init();
        config_.channels = 2;
        config_.sampleRate = 48000;
        config_.periodSizeInFrames = 1024;  // ~21ms @ 48kHz
        config_.periods = 3;
        config_.noAutoStart = true;

        // Enable PipeWire/PulseAudio/ALSA backends
        config_.backend = ma_backend_pipewire; // Try PipeWire first

        if (ma_engine_init(&config_, &engine_) != MA_SUCCESS) {
            // Fallback to auto
            config_.backend = ma_backend_null;
            if (ma_engine_init(&config_, &engine_) != MA_SUCCESS) return false;
        }

        // Create bus connections
        for (uint32_t i = 1; i < COUNT; i++) {
            ma_engine_node_set_output_bus(&engine_, i, MASTER);
        }

        ma_engine_start(&engine_);
        return true;
    }

    ma_sound* PlaySound(const char* path, Bus bus, bool looping = false);
    void SetBusVolume(Bus bus, float volume);  // 0.0..1.0
    void SetMasterVolume(float volume);
};
```

### Sound Handles
```cpp
struct SoundHandle {
    ma_sound sound_;
    bool is_streaming_ = false;  // Music vs SFX

    void Play();
    void Stop();
    void SetVolume(float vol);
    void SetPitch(float pitch);
    void SetLooping(bool loop);
    bool IsPlaying() const;
};
```

---

## 6. Platform Layer Interface (Game-Agnostic)

```cpp
// src/platform/Platform.h — Game code includes ONLY this
namespace platform {

struct Window { /* opaque */ };
struct InputSnapshot { /* as above */ };
struct AudioEngine { /* opaque */ };
struct FileSystem { /* opaque */ };
struct Timer { /* opaque */ };

// Lifecycle
std::unique_ptr<Window> CreateWindow(const WindowConfig&);
std::unique_ptr<AudioEngine> CreateAudioEngine();
std::unique_ptr<FileSystem> CreateFileSystem();
std::unique_ptr<Timer> CreateTimer();

// Per-frame
InputSnapshot PollInput(Window&);
void BeginFrame(Timer&);
void EndFrame(Timer&, Window&);  // Present

// Time
double GetTimeSeconds();
void SleepMilliseconds(int ms);

} // namespace platform
```

**Game code NEVER includes SDL, Vulkan, miniaudio, or Wayland headers directly.**

---

## 7. Build Configuration

### CMake Targets
```cmake
# Platform layer
add_library(platform_static
    src/platform/wayland/Window.cpp
    src/platform/input/InputManager.cpp
    src/platform/timing/FrameTimer.cpp
    src/platform/filesystem/FileSystem.cpp
    src/platform/audio/AudioEngine.cpp
)

target_link_libraries(platform_static PRIVATE
    SDL3::SDL3
    miniaudio
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(platform_static PRIVATE
    MINIAUDIO_IMPLEMENTATION
    SDL_MAIN_HANDLED
)
```

---

## 8. Debugging Support

- `SDL_HINT_RENDER_DRIVER=opengl` for RenderDoc compatibility
- `VK_LAYER_KHRONOS_validation` in Debug builds
- `MA_LOG_LEVEL=MA_LOG_LEVEL_DEBUG` for miniaudio
- `SDL_LOG_PRIORITY_VERBOSE` for SDL events

---

## 9. Testing

- Headless mode: `SDL_VIDEODRIVER=dummy` for CI
- Input injection: `InputManager::InjectEvent()` for tests
- Time control: `FrameTimer::SetTimeScale()` for tests
- Virtual filesystem: `FileSystem::MountMemory()` for tests

---

**Status:** DESIGN PROPOSAL ONLY — not reference evidence. Implementation is blocked by the evidence gates; this design is not a completion claim.