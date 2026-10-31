// Soul Knight Linux — Wayland Window Implementation
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <engine/core/Log.hpp>

namespace soulknight::platform {

Window::Window(const WindowConfig& config) : config_(config) {}

Window::~Window() {
    Shutdown();
}

Window::Window(Window&& other) noexcept
    : config_(std::move(other.config_))
    , sdl_window_(other.sdl_window_)
    , width_(other.width_)
    , height_(other.height_)
    , fullscreen_(other.fullscreen_)
    , minimized_(other.minimized_)
    , initialized_(other.initialized_)
    , resize_callback_(other.resize_callback_) {
    other.sdl_window_ = nullptr;
    other.initialized_ = false;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        Shutdown();
        config_ = std::move(other.config_);
        sdl_window_ = other.sdl_window_;
        width_ = other.width_;
        height_ = other.height_;
        fullscreen_ = other.fullscreen_;
        minimized_ = other.minimized_;
        initialized_ = other.initialized_;
        resize_callback_ = other.resize_callback_;

        other.sdl_window_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool Window::Initialize() {
    if (initialized_) return true;

    // The project target is Wayland-only. Force the SDL video driver so an
    // accidental X11/XWayland fallback is not silently used.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");

    // Set Wayland-specific hints before window creation
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "1");
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    // Window flags
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (config_.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (config_.high_dpi) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

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

    // Get actual window size (may differ due to DPI scaling)
    int w, h;
    SDL_GetWindowSize(sdl_window_, &w, &h);
    width_ = w;
    height_ = h;

    // Show window
    SDL_ShowWindow(sdl_window_);

    // Register event callback. The callback must be the same function pointer
    // when removing the watch; using a lambda twice would leak the watch.
    SDL_AddEventWatch(OnWindowEvent, this);

    initialized_ = true;
    LOG_INFO_RENDER("Window created: {}x{} (fullscreen: {})", width_, height_, fullscreen_);

    return true;
}

void Window::Shutdown() {
    if (!initialized_) return;

    SDL_RemoveEventWatch(OnWindowEvent, this);

    if (sdl_window_) {
        SDL_DestroyWindow(sdl_window_);
        sdl_window_ = nullptr;
    }

    initialized_ = false;
    LOG_INFO_RENDER("Window destroyed");
}

void Window::Resize(int width, int height) {
    if (width == width_ && height == height_) return;

    width_ = width;
    height_ = height;

    if (resize_callback_) {
        resize_callback_(width, height);
    }
}

void Window::Present() {
    // NOT IMPLEMENTED: there is no Vulkan swapchain yet. A future renderer will
    // call vkQueuePresentKHR; this function is intentionally a no-op.
}

VkSurfaceKHR Window::CreateVulkanSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(sdl_window_, instance, nullptr, &surface)) {
        LOG_ERROR_RENDER("Failed to create Vulkan surface: {}", SDL_GetError());
        return VK_NULL_HANDLE;
    }
    return surface;
}

bool SDLCALL Window::OnWindowEvent(void* userdata, SDL_Event* event) {
    auto* self = static_cast<Window*>(userdata);
    if (!self || !self->initialized_) return false;

    switch (event->type) {
        case SDL_EVENT_WINDOW_RESIZED:
            self->width_ = event->window.data1;
            self->height_ = event->window.data2;
            if (self->resize_callback_) {
                self->resize_callback_(self->width_, self->height_);
            }
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            self->minimized_ = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            self->minimized_ = false;
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            self->fullscreen_ = true;
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            self->fullscreen_ = false;
            break;
        default:
            break;
    }
    return false;
}

} // namespace soulknight::platform