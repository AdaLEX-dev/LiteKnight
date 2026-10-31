// Soul Knight Linux — Wayland Window Implementation
#include "Window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <engine/core/Log.hpp>
#include <utility>

namespace soulknight::platform {

Window::Window(const WindowConfig& config) : config_(config) {
    state_.logical_width = config.width;
    state_.logical_height = config.height;
    state_.pixel_width = config.width;
    state_.pixel_height = config.height;
}

Window::~Window() {
    Shutdown();
}

Window::Window(Window&& other) noexcept
    : config_(std::move(other.config_))
    , state_(other.state_)
    , sdl_window_(other.sdl_window_)
    , fullscreen_(other.fullscreen_)
    , initialized_(other.initialized_) {
    other.sdl_window_ = nullptr;
    other.initialized_ = false;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        Shutdown();
        config_ = std::move(other.config_);
        state_ = other.state_;
        sdl_window_ = other.sdl_window_;
        fullscreen_ = other.fullscreen_;
        initialized_ = other.initialized_;

        other.sdl_window_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool Window::Initialize() {
    if (initialized_) return true;

    // These hints are set before SDL video initialization by Application. They
    // are repeated here so Window cannot silently become an X11 window if used
    // independently in a future tool.
    (void)SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    (void)SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "0");
    (void)SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
    (void)SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    (void)SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN;
    if (config_.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (config_.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (config_.high_dpi) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

    sdl_window_ = SDL_CreateWindow(
        config_.title.c_str(),
        config_.width,
        config_.height,
        flags
    );

    if (!sdl_window_) {
        LOG_ERROR_RENDER("Failed to create SDL window: {}", SDL_GetError());
        return false;
    }

    RefreshSizes();
    SDL_ShowWindow(sdl_window_);

    initialized_ = true;
    LOG_INFO_RENDER("Window created: {}x{} logical, {}x{} pixels",
                    state_.logical_width, state_.logical_height,
                    state_.pixel_width, state_.pixel_height);

    return true;
}

void Window::Shutdown() {
    if (sdl_window_) {
        SDL_DestroyWindow(sdl_window_);
        sdl_window_ = nullptr;
    }
    initialized_ = false;
}

bool Window::Resize(int width, int height) {
    if (!sdl_window_ || width <= 0 || height <= 0) return false;

    if (!SDL_SetWindowSize(sdl_window_, width, height)) {
        LOG_WARN_RENDER("SDL_SetWindowSize failed: {}", SDL_GetError());
        return false;
    }

    state_.OnLogicalResize(width, height);
    return true;
}

void Window::SetTitle(const std::string& title) {
    if (sdl_window_) SDL_SetWindowTitle(sdl_window_, title.c_str());
}

bool Window::SetFullscreen(bool fullscreen) {
    if (!sdl_window_) return false;
    if (!SDL_SetWindowFullscreen(sdl_window_, fullscreen)) {
        LOG_WARN_RENDER("SDL_SetWindowFullscreen failed: {}", SDL_GetError());
        return false;
    }
    fullscreen_ = fullscreen;
    RefreshSizes();
    LOG_INFO_RENDER("Fullscreen: {} ({}x{} logical, {}x{} pixels)",
                    fullscreen_ ? "on" : "off", state_.logical_width,
                    state_.logical_height, state_.pixel_width,
                    state_.pixel_height);
    return true;
}

VkSurfaceKHR Window::CreateVulkanSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!sdl_window_) {
        LOG_ERROR_RENDER("Cannot create Vulkan surface without an SDL window");
        return VK_NULL_HANDLE;
    }
    if (!SDL_Vulkan_CreateSurface(sdl_window_, instance, nullptr, &surface)) {
        LOG_ERROR_RENDER("Failed to create Vulkan surface: {}", SDL_GetError());
        return VK_NULL_HANDLE;
    }
    return surface;
}

void Window::RefreshSizes() {
    if (!sdl_window_) return;

    int logical_width = 0;
    int logical_height = 0;
    if (SDL_GetWindowSize(sdl_window_, &logical_width, &logical_height)) {
        state_.OnLogicalResize(logical_width, logical_height);
    }

    int pixel_width = 0;
    int pixel_height = 0;
    if (SDL_GetWindowSizeInPixels(sdl_window_, &pixel_width, &pixel_height)) {
        state_.OnPixelResize(pixel_width, pixel_height);
    } else {
        state_.OnPixelResize(state_.logical_width, state_.logical_height);
    }
}

void Window::SyncRuntimeState() {
    if (!sdl_window_) return;

    const SDL_WindowFlags flags = SDL_GetWindowFlags(sdl_window_);
    const bool minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    const bool focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;

    if (minimized != state_.minimized) {
        if (minimized) {
            state_.OnMinimized();
        } else {
            state_.OnRestored();
        }
        LOG_INFO_RENDER("Window state synchronized: minimized={}", minimized);
    }
    if (focused != state_.focused) {
        if (focused) {
            state_.OnFocusGained();
        } else {
            state_.OnFocusLost();
        }
        LOG_INFO_RENDER("Window state synchronized: focused={}", focused);
    }
}

void Window::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_WINDOW_RESIZED:
            state_.OnLogicalResize(event.window.data1, event.window.data2);
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            state_.OnPixelResize(event.window.data1, event.window.data2);
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            state_.OnMinimized();
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
            state_.OnRestored();
            break;
        case SDL_EVENT_WINDOW_SHOWN:
            RefreshSizes();
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            state_.OnFocusGained();
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            state_.OnFocusLost();
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            state_.RequestClose();
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            fullscreen_ = true;
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            fullscreen_ = false;
            break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            RefreshSizes();
            break;
        default:
            break;
    }
}

} // namespace soulknight::platform
