// Soul Knight Linux — Wayland Window
#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <string>
#include "WindowState.hpp"

namespace soulknight::platform {

struct WindowConfig {
    std::string title = "Soul Knight";
    int width = 1920;
    int height = 1080;
    bool fullscreen = false;
    bool resizable = true;
    bool high_dpi = true;
};

class Window {
public:
    Window() = default;
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    bool Initialize();
    void Shutdown();

    // Feeds one real SDL event into the pure window-state transition model.
    void HandleEvent(const SDL_Event& event);

    // Reconciles event state with SDL's current window flags. This is a safety
    // net for compositor/SDL combinations that omit a RESTORED or FOCUS event.
    void SyncRuntimeState();

    bool Resize(int width, int height);
    bool SetFullscreen(bool fullscreen);
    void SetTitle(const std::string& title);

    SDL_Window* GetSDLWindow() const { return sdl_window_; }
    VkSurfaceKHR CreateVulkanSurface(VkInstance instance) const;

    int GetWidth() const { return state_.logical_width; }
    int GetHeight() const { return state_.logical_height; }
    int GetPixelWidth() const { return state_.pixel_width; }
    int GetPixelHeight() const { return state_.pixel_height; }
    bool IsFullscreen() const { return fullscreen_; }
    bool IsMinimized() const { return state_.minimized; }
    bool IsFocused() const { return state_.focused; }
    bool ShouldClose() const { return state_.close_requested; }
    bool HasPendingResize() const { return state_.resize_pending; }
    void ClearResizeRequest() { state_.ClearResizeRequest(); }
    const WindowState& GetState() const { return state_; }

private:
    WindowConfig config_;
    WindowState state_;
    SDL_Window* sdl_window_ = nullptr;

    bool fullscreen_ = false;
    bool initialized_ = false;

    void RefreshSizes();
};

} // namespace soulknight::platform
