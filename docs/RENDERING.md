# Soul Knight 2.6.8 — Rendering Design

> **Current implementation (2026-10-31):** the Vulkan 1.3 context, Wayland
> surface, swapchain, render pass, frame resources, GLSL sprite shaders, dynamic
> multi-texture sprite batch, RGBA8 staging upload, alpha blending, 2D camera, and
> aspect-fit resize path are implemented and manually verified. A reconstructed
> `r1_1` native test reconstruction + `c00` player composition using real Unity
> Sprite metadata renders successfully; the player plays real idle/run clips and
> moves under fixed-step keyboard input with collision and camera follow.
> `docs/PARITY.md` Gate C is **PARTIAL**.
>
> **Not implemented:** general KTX2/atlas packing, full AnimatorController
> transitions/layers, all game content, camera follow, VMA, GPU timestamp
> profiling, or rendering parity. PNG decoding and controlled Unity
> texture/sprite/animation conversion are implemented. Evidence:
> `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`, `docs/ASSET_PIPELINE.md`,
> `analysis/assets/sprites.md`, `analysis/assets/coordinate_system.md`,
> `analysis/assets/animation.md`.

**Target:** Vulkan 1.3, 1920×1080, 60 FPS, 2D sprite batching

---

## 0. Implemented smoke renderer (2026-10-31)

```text
Wayland window / SDL3 surface
  -> Vulkan 1.3 instance (validation in Debug)
  -> Intel UHD 620 (integrated fallback)
  -> graphics+present queue family 0
  -> B8G8R8A8_SRGB / FIFO / 3-image swapchain
  -> render pass + 2 frames in flight
  -> sprite.vert/sprite.frag SPIR-V
  -> synthetic PNG -> libpng -> RGBA8 TextureData
  -> staging upload -> sampler
  -> batched textured quad (alpha, UV, tint)
  -> present
```

The Unity `TagManager.m_SortingLayers` mapping to native `SortLayer` values is
recovered (`Default`=0, `BackGround`=1, `Floor`=2, `Shadow`=3, `Wall`=4,
`Character`=5, `WallFont`=6, `Effect`=7, `UI`=8); the mapping is evidence-backed,
not a guessed index. The default textured quad remains a synthetic renderer
bring-up asset, not APK content. The decoded PNG is asymmetric so vertical flips
and channel swaps are visible.

A controlled real Unity Texture2D (`level-objects`/`f903`, 16x24 RGBA32) was
also converted to a deterministic PNG, native-round-tripped byte-for-byte, and
rendered through the same `--texture` path. This proves the asset boundary, not
reference rendering parity. See `docs/ASSET_PIPELINE.md`.

## 0. Implemented native sprite/scene path (2026-10-31)

```text
Unity Texture2D + Sprite metadata + shared atlas
  -> deterministic PNG / native scene JSON
  -> TextureData RGBA8 + SpriteData
  -> multi-texture SpriteBatch
  -> Camera2D world transform (Y-up, PPU=16)
  -> Vulkan sprite pipeline
  -> Wayland presentation
```

- `SpriteData` stores top-left pixel rect, adjusted pivot, source pivot,
  pixels-per-unit, border, filter, and wrap.
- `SpriteInstance` stores world position, rotation, scale, color, flips, and
  Unity sorting layer/order.
- `Camera2D` is orthographic, centered at (20,20) with half-height 10.5 for the
  recovered room. The reference camera size is not recovered; this is a
  **PROJECT DECISION**.
- The `Mat4` column-major multiplication bug that dropped camera translation was
  fixed; the projection maps world +Y up.
- The `r1_1` scene has 111 instances across 3 textures and 13 sprite assets:
  109 room sprites, 1 player body (four idle-frame sprites selected over time),
  and 1 player shadow.
- The player uses the real `char_1_skin_0_ide` and `char_1_skin_0_run` clips:
  four discrete sprite keys each at `0`, `0.125`, `0.25`, `0.375 s`, looping at
  `0.5 s`. Movement presence selects run; zero movement selects idle.
- The renderer groups contiguous texture runs (5 draw groups for this scene) and
  uses global index-buffer quad offsets with `vertexOffset=0`.
- Screenshot evidence is temporary and not committed; the actual native scene is
  generated from the local reference bundles.

## 0.1 Native sprite animation (2026-10-31)

```text
Unity AnimatorController/state
  -> AnimationClip (real SpriteRenderer.m_Sprite keys)
  -> native AnimationClip/AnimationPlayer
  -> active SpriteData
  -> pivot-adjusted quad
  -> existing SpriteBatch/SpriteRenderer
```

The native animation model is deliberately separate from sprite and texture data:

- `AnimationClip` owns duration, sample rate, loop flag, update-mode metadata, and
  `SpriteTrack` key lists.
- `SpriteTrack` contains only target path, property (`m_Sprite`), and discrete
  time/sprite-ID keys.
- `AnimationPlayer` owns playback time and loop count; it does not own pixels or
  sprite interpretation.
- `Application` advances the player at the 50 Hz fixed step and evaluates the
  active sprite after simulation, then rebuilds the existing batched render list.
  The original Animator `m_UpdateMode` is Normal (`0`); fixed-step evaluation is a
  deterministic native decision, not proven Unity parity.
- The selected clip is `char_1_skin_0_ide` (0.5 s, 8 Hz, loop) from controller
  `char_0_skin_0`, default state `Base Layer.ide`. The clip is evaluated
  discretely: exact key time selects the key, between keys holds the preceding
  key, and loop duration wraps to zero.
- The four frame sprites have slightly different tight-crop widths and adjusted
  pivots. The renderer recomputes size and pivot from the active `SpriteData`, so
  the player's feet stay anchored while the image changes.
- This implements only the evidence-backed idle/run paths, not a general Unity
  Animator clone. `analysis/assets/animation.md` records the exact IDs, binding,
  mapping, streamed curve values, hashes, and remaining UNKNOWNs.

Player movement is fixed-step keyboard input only:

```text
PollEvents -> MovementInput snapshot
FixedUpdate(0.02)
  -> PlayerLocomotion (9.75 units/s from recovered speed/speed_rate formula)
  -> swept AABB collision
  -> LocomotionAnimator idle/run
  -> Camera2D follow
  -> SpriteBatch / Vulkan
```

Collision is a native test-chamber approximation: a 13×13 interior AABB derived
from the recovered 15×15 room dimensions and a 1-unit wall ring. The current
`r1_1_visual` scene is a reverse-engineering test reconstruction, not a proven
canonical gameplay level. The reference camera-follow values are not recovered;
`Camera2D::follow_smoothing = 8.0` is native design. See
`analysis/assets/locomotion.md`.

## 1. Rendering Architecture

### Pipeline Overview
```
Game Systems
     │
     ▼
Render Commands (per frame)
     │
     ▼
Sprite Batcher → Instanced Draw Calls
     │
     ▼
Vulkan Render Pass (Single Subpass)
     │
     ▼
Swapchain Present
```

### Design Principles
- **Single render pass** per frame (no post-process chain initially)
- **Instanced drawing** — one draw call per texture atlas
- **Push constants** for per-instance data (transform, color, UV)
- **Descriptor indexing** for bindless texture arrays
- **No dynamic uniform buffers** — push constants only

---

## 2. Sprite Batcher

### Instance Data (Push Constant)
```glsl
// shader/sprite.vert
#version 460
layout(push_constant) uniform SpritePush {
    mat4  transform;      // 64 bytes (model * view * proj)
    vec4  color;          // 16 bytes
    vec4  uv_rect;        // 16 bytes (x, y, width, height in atlas)
    uint  texture_index;  // 4 bytes
    uint  flags;          // 4 bytes (flip_x, flip_y, additive, etc.)
    // Total: 104 bytes < 128 byte push constant limit
} sprite;
```

### Batch Structure
```cpp
// src/engine/rendering/SpriteBatcher.cpp
struct SpriteInstance {
    Mat4 transform;       // Position, rotation, scale
    Color32 color;        // RGBA8
    Rect uv;              // Atlas coordinates (normalized 0..1)
    uint32_t texture_id;  // Index into texture array
    uint32_t flags;       // FLIP_X=1, FLIP_Y=2, ADDITIVE=4, etc.
};

class SpriteBatcher {
    static constexpr size_t MAX_INSTANCES = 10000;
    static constexpr size_t MAX_TEXTURES = 256;

    std::vector<SpriteInstance> instances_;
    std::array<TextureHandle, MAX_TEXTURES> textures_;
    size_t texture_count_ = 0;

    // Per-frame
    void BeginFrame();
    void Draw(const SpriteInstance& instance);
    void Flush(VkCommandBuffer cmd);  // Sort by texture, submit draws
    void EndFrame();
};
```

### Texture Atlas
```cpp
// Atlas layout: 4096x4096 or 8192x8192 KTX2 array texture
// Each sprite → one layer in array texture
// UV rect = (x/atlas_w, y/atlas_h, w/atlas_w, h/atlas_h)

struct Atlas {
    VkImage image_;
    VkImageView view_;
    VkSampler sampler_;
    uint32_t width_, height_, layers_;

    // Runtime: pack sprites at startup
    uint32_t AddSprite(const KTX2Texture& sprite);  // Returns layer index
};
```

---

## 3. Vulkan Resources

### Core Objects
```cpp
// src/engine/rendering/VulkanContext.cpp
struct VulkanContext {
    VkInstance instance_;
    VkPhysicalDevice physical_device_;
    VkDevice device_;
    VkQueue graphics_queue_;
    VkQueue present_queue_;
    uint32_t graphics_queue_family_;
    uint32_t present_queue_family_;

    VkSurfaceKHR surface_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_views_;
    VkFormat swapchain_format_;
    VkExtent2D swapchain_extent_;

    VkRenderPass render_pass_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;

    VmaAllocator allocator_;

    // Frame resources (triple-buffered)
    struct FrameData {
        VkCommandBuffer cmd_buffer_;
        VkFence fence_;
        VkSemaphore image_available_;
        VkSemaphore render_finished_;
        VkBuffer instance_buffer_;  // Sprite instances
        VkDeviceMemory instance_memory_;
    } frames_[3];

    uint32_t current_frame_ = 0;
};
```

### Render Pass
```cpp
// Single subpass, one color attachment (swapchain), no depth
VkAttachmentDescription color_att = {
    .format = swapchain_format_,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
};

VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_ref
};
```

---

## 4. Shader Pipeline

### Vertex Shader (sprite.vert)
```glsl
#version 460
#extension GL_EXT_debug_printf : enable

layout(push_constant) uniform SpritePush {
    mat4 transform;
    vec4 color;
    vec4 uv_rect;
    uint texture_index;
    uint flags;
} sprite;

layout(location = 0) in vec2 in_pos;      // Quad: (-0.5,-0.5) to (0.5,0.5)
layout(location = 1) in vec2 in_uv;       // Quad UV: (0,0) to (1,1)

layout(set = 0, binding = 0) uniform sampler2DArray u_atlas;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out float v_texture_index;

void main() {
    vec4 world_pos = sprite.transform * vec4(in_pos, 0.0, 1.0);
    gl_Position = world_pos;

    // Atlas UV calculation
    vec2 atlas_uv = sprite.uv_rect.xy + in_uv * sprite.uv_rect.zw;
    v_uv = atlas_uv;
    v_color = sprite.color;
    v_texture_index = float(sprite.texture_index);

    // Flip flags
    if (sprite.flags & 1u) v_uv.x = 1.0 - v_uv.x;  // FLIP_X
    if (sprite.flags & 2u) v_uv.y = 1.0 - v_uv.y;  // FLIP_Y
}
```

### Fragment Shader (sprite.frag)
```glsl
#version 460

layout(set = 0, binding = 0) uniform sampler2DArray u_atlas;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in float v_texture_index;

layout(location = 0) out vec4 out_color;

void main() {
    uint layer = uint(v_texture_index);
    vec4 tex_color = texture(u_atlas, vec3(v_uv, layer));

    // Premultiplied alpha blending handled by pipeline
    out_color = tex_color * v_color;

    // Additive flag
    // if (flags & 4u) { ... } // handled via blend state or separate pipeline
}
```

---

## 5. Camera System

### Orthographic Camera

> **Historical design snippet.** The implemented camera is `Camera2D`
> (section 0 and `analysis/assets/coordinate_system.md`): Y-up world units,
> `orthographic_size` as half-height, aspect-fit viewport, and column-major
> `Mat4` multiplication. The snippet below is not current code.

```cpp
// src/engine/rendering/Camera.cpp
struct Camera {
    Vec2 position = {0, 0};      // World center
    float zoom = 1.0f;           // 1.0 = 1:1 pixels
    float rotation = 0.0f;       // Radians
    float shake_intensity = 0.0f;
    float shake_timer = 0.0f;
    Vec2 target_position = {0, 0};  // For smooth follow

    Mat4 ViewProjection() const {
        // Ortho: left=-width/2, right=width/2, bottom=-height/2, top=height/2
        float half_w = 960.0f / zoom;   // 1920/2
        float half_h = 540.0f / zoom;   // 1080/2

        Mat4 proj = Mat4::Orthographic(-half_w, half_w, -half_h, half_h, -1000, 1000);
        Mat4 view = Mat4::Translate(-position) * Mat4::Rotate(rotation);

        // Shake offset
        if (shake_timer > 0) {
            Vec2 offset = RandomInCircle(shake_intensity);
            view = Mat4::Translate(offset) * view;
        }

        return proj * view;
    }

    void Follow(Vec2 target, float lerp_speed = 10.0f);
    void Shake(float intensity, float duration);
    void Update(float dt);
};
```

---

## 6. Frame Rendering Flow

```cpp
void RenderFrame() {
    // 1. Acquire swapchain image
    uint32_t image_index;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                          frames[current_frame].image_available,
                          VK_NULL_HANDLE, &image_index);

    // 2. Wait for fence
    vkWaitForFences(device, 1, &frames[current_frame].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &frames[current_frame].fence);

    // 3. Record command buffer
    VkCommandBuffer cmd = frames[current_frame].cmd_buffer;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &begin_info);

    // 4. Begin render pass
    VkRenderPassBeginInfo rp_info = {
        .renderPass = render_pass_,
        .framebuffer = framebuffers[image_index],
        .renderArea = {{0,0}, swapchain_extent_},
        .clearValueCount = 1,
        .pClearValues = &clear_color  // Black or sky color
    };
    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    // 5. Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // 6. Bind descriptor set (atlas texture array)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

    // 7. Draw batched sprites (sorted by texture)
    sprite_batcher.Flush(cmd);

    // 8. End render pass
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // 9. Submit
    VkSubmitInfo submit = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames[current_frame].image_available,
        .pWaitDstStageMask = &(VkPipelineStageFlags){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frames[current_frame].render_finished
    };
    vkQueueSubmit(graphics_queue_, 1, &submit, frames[current_frame].fence);

    // 10. Present
    VkPresentInfoKHR present = {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames[current_frame].render_finished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &image_index
    };
    vkQueuePresentKHR(present_queue_, &present);

    current_frame = (current_frame + 1) % 3;
}
```

---

## 7. Texture Loading (KTX2)

```cpp
// src/engine/resources/TextureLoader.cpp
struct KTX2Texture {
    uint32_t width, height, layers;
    VkFormat format;  // VK_FORMAT_ASTC_4x4_SRGB_BLOCK, etc.
    std::vector<uint8_t> data;  // Supercompressed (Zstd) or raw
};

TextureHandle LoadKTX2(const std::string& path) {
    // 1. Parse KTX2 header (ktx.h or custom)
    // 2. Decompress supercompression (Zstd) if needed
    // 3. Create VkImage (VK_IMAGE_TYPE_2D, array layers)
    // 4. Upload via staging buffer (vkCmdCopyBufferToImage)
    // 5. Generate mipmaps (vkCmdBlitImage) if not present
    // 6. Create VkImageView (VK_IMAGE_VIEW_TYPE_2D_ARRAY)
    // 7. Return handle
}
```

---

## 8. Debug Rendering

```cpp
// Debug overlay (ImGui or custom)
struct DebugRenderer {
    void DrawRect(Vec2 pos, Vec2 size, Color color, float thickness = 1.0f);
    void DrawCircle(Vec2 pos, float radius, Color color);
    void DrawLine(Vec2 a, Vec2 b, Color color);
    void DrawText(Vec2 pos, std::string_view text, Color color, float size);
    void Flush(VkCommandBuffer cmd);
};

// Debug flags (config.toml)
[debug]
show_colliders = true
show_hitboxes = true
show_entity_ids = true
show_fps_graph = true
show_frame_times = true
```

---

## 9. Performance Targets

| Metric | Budget |
|--------|--------|
| Draw calls/frame | < 50 (1 per atlas + UI) |
| Triangles/frame | < 50k |
| Texture binds | 1 (atlas array) |
| Pipeline switches | 2 (sprite + UI) |
| GPU time | < 8 ms |
| VRAM | < 256 MB (textures + buffers) |

---

## 10. Future Extensions (Post-v1)

- Post-process: Bloom, CRT shader, color grading
- Lighting: 2D normal maps, lightmaps
- Particles: GPU compute simulation
- UI: Separate atlas, signed distance field fonts
- Multi-view: Split-screen, picture-in-picture

---

**Status:** DESIGN PROPOSAL ONLY — not reference evidence. Implementation is blocked by the evidence gates; this design is not a completion claim.