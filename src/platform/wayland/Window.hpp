// Soul Knight Linux — Wayland Window
#pragma once

#include <SDL3/SDL.h>
#include "minimal_vulkan.h"
#include <string>
#include <memory>
#include <engine/core/Types.hpp>

namespace soulknight::platform {

struct WindowConfig {
    std::string title = "Soul Knight";
    int width = 1920;
    int height = 1080;
    bool fullscreen = false;
    bool vsync = true;
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

    void Resize(int width, int height);
    void Present();

    SDL_Window* GetSDLWindow() const { return sdl_window_; }
    VkSurfaceKHR CreateVulkanSurface(VkInstance instance) const;

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    bool IsFullscreen() const { return fullscreen_; }
    bool IsMinimized() const { return minimized_; }

    // Callbacks
    using ResizeCallback = void(*)(int, int);
    void SetResizeCallback(ResizeCallback cb) { resize_callback_ = cb; }

private:
    WindowConfig config_;
    SDL_Window* sdl_window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    int width_ = 0;
    int height_ = 0;
    bool fullscreen_ = false;
    bool minimized_ = false;
    bool initialized_ = false;

    void (*resize_callback_)(int, int) = nullptr;

    static void SDLCALL OnWindowEvent(void* userdata, SDL_Event* event);
};

} // namespace soulknight::platform