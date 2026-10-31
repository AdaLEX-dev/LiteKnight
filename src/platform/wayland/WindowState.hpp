// Soul Knight Linux — Pure window state transitions
#pragma once

namespace soulknight::platform {

// The event transition logic is deliberately independent of SDL so it can be
// unit-tested without a compositor. Window owns one of these and feeds it real
// SDL events.
struct WindowState {
    int logical_width = 1920;
    int logical_height = 1080;
    int pixel_width = 1920;
    int pixel_height = 1080;
    bool minimized = false;
    bool focused = true;
    bool close_requested = false;
    bool resize_pending = false;

    void OnLogicalResize(int width, int height) noexcept {
        if (width <= 0 || height <= 0) return;
        logical_width = width;
        logical_height = height;
        resize_pending = true;
    }

    void OnPixelResize(int width, int height) noexcept {
        if (width <= 0 || height <= 0) return;
        pixel_width = width;
        pixel_height = height;
        resize_pending = true;
    }

    void OnMinimized() noexcept { minimized = true; }
    void OnRestored() noexcept { minimized = false; resize_pending = true; }
    void OnFocusGained() noexcept { focused = true; }
    void OnFocusLost() noexcept { focused = false; }
    void RequestClose() noexcept { close_requested = true; }
    void ClearResizeRequest() noexcept { resize_pending = false; }
};

} // namespace soulknight::platform
