// Minimal Vulkan header for Soul Knight - core Vulkan 1.3 API only
// Excludes ALL video codec extensions and their associated headers
// This replaces the system vulkan.h entirely for our use case

#ifndef SOUL_KNIGHT_MINIMAL_VULKAN_H
#define SOUL_KNIGHT_MINIMAL_VULKAN_H

// Prevent inclusion of video codec extensions in any included headers
#define VK_NO_VIDEO_EXTENSIONS 1
#define VK_ENABLE_BETA_EXTENSIONS 0

// Disable all video codec extension macros
#define VK_KHR_video_queue 0
#define VK_KHR_video_decode_queue 0
#define VK_KHR_video_encode_queue 0
#define VK_KHR_video_decode_h264 0
#define VK_KHR_video_decode_h265 0
#define VK_KHR_video_encode_h264 0
#define VK_KHR_video_encode_h265 0
#define VK_KHR_video_encode_av1 0
#define VK_KHR_video_decode_av1 0
#define VK_KHR_video_encode_av1 0
#define VK_KHR_video_decode_av1 0
#define VK_KHR_video_maintenance1 0
#define VK_KHR_video_encode_quantization_map 0
#define VK_KHR_video_decode_h264 0
#define VK_KHR_video_decode_h265 0
#define VK_KHR_video_encode_h264 0
#define VK_KHR_video_encode_h265 0
#define VK_KHR_video_encode_av1 0
#define VK_KHR_video_decode_av1 0
#define VK_KHR_video_encode_av1 0
#define VK_KHR_video_decode_av1 0
#define VK_KHR_video_maintenance1 0
#define VK_KHR_video_encode_quantization_map 0

// Prevent std video headers from being included
#define VK_NO_STD_VIDEO_H264 1
#define VK_NO_STD_VIDEO_H265 1
#define VK_NO_STD_VIDEO_AV1 1
#define VK_NO_STD_VIDEO_VP9 1

// Include core Vulkan headers we actually need
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

#endif // SOUL_KNIGHT_MINIMAL_VULKAN_H