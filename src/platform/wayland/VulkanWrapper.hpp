// Minimal Vulkan wrapper - includes only core Vulkan API, excludes all video codec extensions
// This avoids pulling in video codec headers which require massive amounts of stubs

#ifndef SOUL_KNIGHT_VULKAN_WRAPPER_H
#define SOUL_KNIGHT_VULKAN_WRAPPER_H

// Define all video-related extension macros to 0 to disable them at compile time
#define VK_NO_VIDEO_EXTENSIONS 1
#define VK_ENABLE_BETA_EXTENSIONS 0

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

// Prevent inclusion of video codec std headers
#define VK_NO_STD_VIDEO_H264 1
#define VK_NO_STD_VIDEO_H265 1
#define VK_NO_STD_VIDEO_AV1 1
#define VK_NO_STD_VIDEO_VP9 1

// Include only the core Vulkan headers we actually need
#include <vulkan/vulkan.h>

// Explicitly undef video-related constants that might leak through
#undef VK_KHR_video_queue
#undef VK_KHR_video_decode_queue
#undef VK_KHR_video_encode_queue
#undef VK_KHR_video_decode_h264
#undef VK_KHR_video_decode_h265
#undef VK_KHR_video_encode_h264
#undef VK_KHR_video_encode_h265
#undef VK_KHR_video_encode_av1
#undef VK_KHR_video_decode_av1
#undef VK_KHR_video_encode_av1
#undef VK_KHR_video_decode_av1
#undef VK_KHR_video_maintenance1
#undef VK_KHR_video_encode_quantization_map

#endif // SOUL_KNIGHT_VULKAN_WRAPPER_H