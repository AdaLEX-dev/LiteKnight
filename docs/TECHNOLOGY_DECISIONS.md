# Soul Knight 2.6.8 — Technology Decisions

Generated: 2026-10-30
Status: **ACCEPTED** — All decisions recorded here are binding for implementation

---

## Decision Record Format

Each decision follows:
- **Context** — Why this decision is needed
- **Options** — Alternatives considered
- **Decision** — Chosen option with rationale
- **Consequences** — Trade-offs and follow-up requirements

---

## ADR-0001: Implementation Language — C++20

**Status:** ACCEPTED

### Context
Need a systems language for native Linux game engine with:
- Wayland/Vulkan integration
- Deterministic simulation
- Performance (60 FPS @ 1080p)
- CachyOS/Arch availability
- Team familiarity (single developer)

### Options
| Language | Pros | Cons |
|----------|------|------|
| **C++20** | Mature game ecosystem, direct Vulkan/Wayland, deterministic, CachyOS GCC 16, zero-overhead abstraction | Manual memory, longer compile times |
| Rust | Memory safety, modern tooling, Cargo, growing game ecosystem | Vulkan/Wayland bindings less mature, steeper learning for game patterns, deterministic SIMD harder |
| C | Maximum control, simplest | No RAII, no generics, manual everything |
| Zig | Comptime, safety, C interop | Immature ecosystem, no game libraries |

### Decision
**C++20** with:
- GCC 16 (CachyOS default) / Clang 18 option
- CMake 4 + Ninja
- Standard library only (no Boost)
- RAII + smart pointers (`unique_ptr`, `shared_ptr` where justified)
- No exceptions (use `std::expected` / error codes)
- No RTTI (`-fno-rtti`)

### Consequences
- Manual memory management for resources (textures, audio, meshes)
- Custom allocator for frame-arena allocations
- Deterministic floating-point (`-ffp-contract=off` for simulation)
- Vendor: GCC 16 on CachyOS is primary; Clang for sanitizers

---

## ADR-0002: Window & Input Backend — SDL3

**Status:** ACCEPTED

### Context
Need Wayland-native windowing and input abstraction:
- No X11/XWayland
- Keyboard, mouse, gamepad
- High-DPI support
- Minimal dependencies

### Options
| Backend | Pros | Cons |
|---------|------|------|
| **SDL3** | Wayland-native, gamepad DB, C API, CachyOS package, mature, single header option | Slightly heavier than raw Wayland |
| Raw Wayland (wlroots/libwayland) | Minimal, direct control | High effort, gamepad/keyboard repeat logic, cursor themes |
| GLFW | Wayland support, lighter than SDL | No gamepad DB, less maintained for Wayland |
| wlroots + custom | Full control | Massive effort, reinventing SDL |

### Decision
**SDL3** (development branch, stable API)
- Package: `sdl3` on CachyOS (or build from git)
- Static link preferred
- Use SDL3's `SDL_Renderer` **NOT** — use only for window/input
- Custom Vulkan renderer (see ADR-0003)

### Consequences
- SDL3 dependency (runtime + build)
- Gamepad mappings via SDL_gamepad_db
- Input abstraction: `InputAction` enum mapped from SDL scancodes/gamepad buttons

---

## ADR-0003: Rendering API — Vulkan 1.3

**Status:** ACCEPTED

### Context
2D sprite-based game, 1920×1080 @ 60 FPS, Wayland-native:
- Sprite batching (thousands of draw calls → few)
- Atlas textures (ASTC/BC7)
- Simple shaders (vertex + fragment)
- No 3D, no compute (initially)

### Options
| API | Pros | Cons |
|-----|------|------|
| **Vulkan 1.3** | Explicit control, Wayland native (VK_KHR_wayland_surface), modern, deterministic, CachyOS drivers | Verbose, steep learning |
| OpenGL 4.6 / ES 3.2 | Simpler, mature | Legacy, less control, Wayland via EGL/GLX complexity |
| SDL3 GPU API | Abstraction, multiple backends | New, not yet stable, less control |
| wgpu (Rust) / DiligentEngine (C++) | Higher-level, cross-API | Extra dependency, abstraction leaks |

### Decision
**Vulkan 1.3** with:
- **Custom minimal renderer** (no engine framework)
- Single render pass per frame
- Descriptor indexing for texture arrays (bindless-style)
- Push constants for per-draw data (transform, color, UV)
- Shader: SPIR-V compiled from GLSL (glslangValidator at build)
- Memory: VMA (Vulkan Memory Allocator) — single dependency
- Swapchain: Mailbox mode (low latency) or FIFO (vsync)

### Consequences
- ~2000 lines of Vulkan bootstrap (once)
- Shader hot-reload in debug
- RenderDoc capture support
- No implicit synchronization bugs (explicit barriers)

---

## ADR-0004: Audio Backend — miniaudio

**Status:** ACCEPTED

### Context
2D game audio: music (looped), SFX (polyphonic), UI sounds:
- 16-32 simultaneous voices
- Ogg Vorbis / WAV / FLAC
- Per-source volume/pitch/pan
- Low latency (< 20ms)
- PipeWire/PulseAudio/JACK/ALSA on Linux

### Options
| Backend | Pros | Cons |
|---------|------|------|
| **miniaudio** | Single header, no deps, PipeWire/Pulse/ALSA/Wasapi, mixer built-in, public domain | C API, no 3D audio (not needed) |
| OpenAL Soft | Industry standard, 3D audio | Extra dep, config complexity |
| PipeWire directly | Native, low latency | Complex API, less portable |
| SDL3 Audio | Built-in | Limited features, no resampling quality control |
| FMOD / Wwise | Pro features | Proprietary, licensing, overkill |

### Decision
**miniaudio** (single header, `miniaudio.h`)
- Build with `MA_NO_RUNTIME_LINKING` for PipeWire/Pulse
- Custom wrapper: `AudioEngine` → `Sound` (static) / `Music` (streaming)
- Bus system: Master, Music, SFX, UI, Ambience
- Format: Ogg Vorbis (music), WAV/Opus (SFX)

### Consequences
- Zero external audio dependencies
- ~500 lines wrapper code
- Deterministic mixing (fixed sample rate 48kHz)

---

## ADR-0005: Image/Texture Format — KTX2 + ASTC

**Status:** ACCEPTED

### Context
AssetBundles contain ASTC/ETC2 textures. Need native format:
- GPU-native compression (no runtime decompression)
- Fast load (mmap-able)
- Mipmaps for minification
- Atlas packing

### Options
| Format | Pros | Cons |
|--------|------|------|
| **KTX2 + ASTC** | GPU-native, Vulkan standard, supercompression (Zstd), mipmaps, cubemap/array | Requires `toktx` / `astcenc` at build |
| KTX2 + BC7 | Desktop GPU universal | Larger than ASTC, no mobile |
| PNG + runtime compress | Simple | Slow load, CPU compress |
| Custom | Control | Reinventing KTX2 |

### Decision
**KTX2 with ASTC 4x4/6x6/8x8** (per texture quality/size tradeoff)
- Pipeline: AssetBundle → extract → `astcenc` → `toktx` → KTX2
- Runtime: `ktx` C library (or custom minimal parser)
- Atlas: Pack sprites at build, store as KTX2 array texture
- Fallback: BC7 for desktop-only if ASTC unsupported (unlikely on modern Mesa/RADV)

### Consequences
- Build-time dependency: `astcenc`, `toktx` (CachyOS packages)
- Runtime KTX2 parser (~300 lines or `ktx` lib)
- Texture streaming support for large atlases

---

## ADR-0006: Configuration Format — TOML

**Status:** ACCEPTED

### Context
User config (video, audio, controls) + game data (balance, progression):
- Human-readable, version-controllable
- Type-safe parsing
- C++ library available

### Options
| Format | Pros | Cons |
|--------|------|------|
| **TOML** | Types, comments, arrays/tables, C++ libs (toml++), std-like | Slightly verbose |
| JSON | Universal, fast parsers | No comments, trailing commas, no types |
| YAML | Human-friendly | Complex spec, security issues |
| INI | Simple | No nesting, no types |
| Binary (flatbuffers) | Fast, schema | Not human-editable |

### Decision
**TOML** with **toml++** (header-only C++20)
- `config.toml` — user settings (XDG_CONFIG_HOME)
- `data/*.toml` — game balance (weapons, enemies, rooms) — **version controlled**
- Schema validation at load

### Consequences
- toml++ header-only (single include)
- Config migration on version change

---

## ADR-0007: Save Format — JSON + Schema Versioning

**Status:** ACCEPTED

### Context
Save games must be:
- Portable, inspectable, versionable
- Corruption-resistant
- Fast enough (< 100ms save/load)

### Options
| Format | Pros | Cons |
|--------|------|------|
| **JSON** | Universal, inspectable, schema versioning, nlohmann/json header-only | Larger than binary |
| MessagePack | Compact, fast, schema | Binary, less inspectable |
| flatbuffers | Zero-copy, schema | Complex, codegen |
| SQLite | Queryable, ACID | Overkill, locking |

### Decision
**JSON** with **nlohmann/json** (header-only)
- Schema version in file: `"save_version": 3`
- Migration functions per version
- Compressed: gzip/zstd optional for large saves
- Path: `$XDG_DATA_HOME/soul-knight/saves/slot_N.json`

### Consequences
- nlohmann/json single header
- Save/load ~10ms typical
- Human-readable for debugging

---

## ADR-0008: Test Framework — doctest

**Status:** ACCEPTED

### Context
Unit + integration tests, headless CI, deterministic:
- Single header, no deps
- Fast compile
- BDD-style syntax
- C++20 compatible

### Options
| Framework | Pros | Cons |
|-----------|------|------|
| **doctest** | Header-only, fast, BDD, C++20, no deps | Less features than Catch2 |
| Catch2 v3 | Mature, feature-rich | Multi-header, slower compile |
| GoogleTest | Standard, XML output | Heavy, requires build |
| Custom | Minimal | Reinventing |

### Decision
**doctest** (header-only)
- `tests/unit/`, `tests/integration/`
- Headless: `./build/test --reporter=console`
- CI: `--reporter=junit` for GitHub Actions

---

## ADR-0009: Logging — Custom + spdlog (Optional)

**Status:** ACCEPTED

### Context
Structured logging with levels, subsystems, file + stdout:
- TRACE/DEBUG/INFO/WARN/ERROR/FATAL
- Subsystem prefixes: `[APP]`, `[RENDER]`, `[AUDIO]`, etc.
- Zero cost in Release (compile-out)

### Decision
**Custom lightweight logger** (header-only, ~100 lines)
- Macros: `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_FATAL`
- Compile-time level: `LOG_LEVEL` define (DEBUG=TRACE, Release=INFO)
- Sinks: stdout (color), file (rotating)
- Subsystem: `LOG_RENDER("message")` → `[RENDER] message`

**Optional:** spdlog if needed later (header-only, fast)

---

## ADR-0010: Profiling — Built-in + perf/RenderDoc

**Status:** ACCEPTED

### Context
Frame-time analysis, CPU/GPU profiling, memory tracking:
- No external profiling service
- Integrated frame graph
- Compatible with Linux tools

### Decision
**Built-in frame profiler** + external tools:
- **Internal:** `FrameProfiler` — CPU zones (SCOPED_ZONE), GPU timestamps (Vulkan query pool)
- **External:** `perf record -g`, `heaptrack`, `RenderDoc` (Vulkan), `hotspot` (GUI for perf)
- **Metrics:** Frame time, sim time, render time, draw calls, triangles, VRAM, heap

### Consequences
- Custom `Perfetto` trace export for chrome://tracing
- `PROFILE_SCOPE(name)` macro in Debug/RelWithDebInfo

---

## ADR-0011: Math Library — Custom (GLM-style) or GLM

**Status:** ACCEPTED — **Custom minimal math**

### Context
2D game needs: Vec2, Vec3, Mat3, Mat4, Quat, Rect, Color, Random
- Deterministic (no SIMD variance)
- Minimal compile time
- No external dependency

### Decision
**Custom `math` module** (`src/engine/math/`)
- `Vec2`, `Vec3`, `Mat3x3`, `Mat4x4`, `Quat`, `Rect`, `Color32`
- `Random` — **RNG choice superseded by ADR-0016**; the reference uses Unity 2018.4 `Random`, and PCG32 must not be used for parity-sensitive streams
- `Easing`, `Interpolation` (lerp, slerp, smoothstep)
- No SIMD in simulation (scalar for determinism)
- SIMD allowed in render backend only (culling, skinning)

### Consequences
- ~500 lines, fully tested
- Deterministic across architectures
- Easy to audit

---

## ADR-0012: ECS vs Traditional — Component-Based (Manual)

**Status:** ACCEPTED

### Context
Game objects: Player, Enemies, Projectiles, Items, Chests, Particles
- ~500-2000 active entities
- Component composition (Transform, Sprite, Physics, AI, Health, Weapon)

### Options
| Approach | Pros | Cons |
|----------|------|------|
| **Manual Component System** | Simple, deterministic, no framework, full control | Boilerplate |
| EnTT / flecs | Fast, mature, features | Extra dep, abstraction |
| Custom Archetype ECS | Optimal cache | Complex, overengineering |
| Traditional OOP (GameObject) | Familiar | Cache misses, inflexible |

### Decision
**Manual Component System** (Data-Oriented Design)
- `Entity` = `uint32_t` (index + generation)
- `ComponentPool<T>` — `std::vector<T>` + free list
- `World` — manages pools, systems iterate `ComponentPool<T>&`
- Systems: `MovementSystem`, `CombatSystem`, `AISystem`, `RenderSystem`, `ParticleSystem`
- No reflection, no queries — explicit system loops

### Consequences
- ~1000 lines core ECS
- Explicit system order (deterministic)
- Easy to debug (plain vectors)
- No archetype fragmentation for 2D game scale

---

## ADR-0013: Asset Pipeline — Offline Build + Runtime Load

**Status:** ACCEPTED

### Context
AssetBundles → Native formats at build time:
- Reproducible, version-controlled data definitions
- Runtime loads only native formats (KTX2, JSON, Ogg)

### Pipeline Stages
```
1. Extract: AssetRipper / Il2CppDumper → raw assets (PNG, WAV, prefab JSON)
2. Convert: 
   - Textures → astcenc → toktx → KTX2 (atlased)
   - Audio → ffmpeg → Ogg Vorbis (music) / Opus (SFX)
   - Prefabs → custom JSON (entity + components)
   - Rooms → custom JSON (tilemap + spawn points)
   - Data tables → TOML (weapons, enemies, items, buffs)
3. Validate: Checksums, required fields, references
4. Package: data.pak (zip) or loose files in XDG_DATA_HOME
```

### Runtime
- `AssetManager` — loads on demand, reference counts, LRU cache
- Hot-reload in Debug (file watcher)
- No AssetBundle runtime dependency

---

## ADR-0014: Deterministic RNG — PCG32

**Status:** **SUPERSEDED — 2026-10-31**

> **Superseded by ADR-0016.** This decision was made before the reference RNG was inspected. `RGRandom.Range` at RVA `0xDD07E8` saves/restores `UnityEngine.Random.state` and calls `UnityEngine.Random.Range` at `0x1AF8058`. PCG32 cannot reproduce the reference sequence. The historical decision is retained below for audit.

### Context
Game requires deterministic runs (same seed = same dungeon):
- World generation
- Combat rolls
- Enemy AI decisions
- Loot drops
- Visual variance (particle jitter)

### Decision
**PCG32** (Permuted Congruential Generator)
- 64-bit state, 32-bit output
- Fast, small, statistically good, portable
- Multiple streams via `sequence` parameter
- Seed: `uint64_t` from `/dev/urandom` or user input
- **Isolated streams per subsystem** (see Architecture §7)

### Consequences
- Zero external RNG dependency
- Reproducible across platforms/compilers
- Stream isolation prevents cross-contamination

---

## ADR-0015: Build System — CMake 4 + Ninja

**Status:** ACCEPTED

### Context
Cross-platform (Linux only for now), reproducible, fast:
- CachyOS GCC 16 / Clang 18
- Debug / RelWithDebInfo / Release
- Sanitizers (ASan, UBSan, TSan) in Debug
- LTO in Release

### Decision
**CMake 4** (modern CMake) + **Ninja**
- `CMakePresets.json` for configure presets
- `tools/configure.sh` — wrapper for common configs
- `tools/build.sh` — wrapper for build
- `tools/run.sh` — wrapper for run (sets LD_LIBRARY_PATH, XDG)
- `tools/test.sh` — runs tests
- `tools/profile.sh` — runs with perf/heaptrack

### Consequences
- Single `CMakeLists.txt` at root
- `FetchContent` for header-only deps (toml++, doctest, nlohmann/json, miniaudio, VMA)
- VMA as submodule or FetchContent

---

## ADR-0016: Reference-Compatible RNG (provisional)

**Status:** PROPOSED — supersedes ADR-0014

### Context

`RGRandom` is not an independent RNG. Metadata and disassembly show:

- fields `seed_state`, `seed`, and `this_state` (`UnityEngine.Random.State`),
- `RGRandom.Range(int,int)` at RVA `0xDD07E8`,
- calls to `UnityEngine.Random.get_state` (`0x1AF7EC4`), `UnityEngine.Random.set_state` (`0x1AF7F58`), and `UnityEngine.Random.Range` (`0x1AF8058`).

The exact wrapper semantics (when the seed is applied, how local state is saved/restored, whether multiple independent streams exist) are being reconstructed in `analysis/re/rgrandom.md`.

### Decision

The native RNG must be **reference-compatible**, not a convenient substitute. Implementation must reproduce the observed `RGRandom` wrapper semantics and the underlying Unity 2018.4 `Random` state transition. PCG32 must not be used for parity-sensitive systems.

### Consequences

- The native `RandomStream` abstraction is blocked until `analysis/re/rgrandom.md` reaches sufficient confidence.
- Determinism tests must compare against reference-observed sequences or an exact reconstructed algorithm, not merely self-consistency.
- If exact parity is abandoned, that is a scope decision requiring a new ADR, not a silent substitution.

---

## Summary Table

| ID | Decision | Category |
|----|----------|----------|
| ADR-0001 | C++20 (GCC 16) | Language |
| ADR-0002 | SDL3 | Window/Input |
| ADR-0003 | Vulkan 1.3 | Rendering |
| ADR-0004 | miniaudio | Audio |
| ADR-0005 | KTX2 + ASTC | Textures |
| ADR-0006 | TOML (toml++) | Config |
| ADR-0007 | JSON (nlohmann) | Saves |
| ADR-0008 | doctest | Testing |
| ADR-0009 | Custom logger | Logging |
| ADR-0010 | Built-in + perf/RenderDoc | Profiling |
| ADR-0011 | Custom math (RNG portion superseded) | Math |
| ADR-0012 | Manual Component System | Architecture |
| ADR-0013 | Offline asset pipeline | Assets |
| ADR-0014 | ~~PCG32 (multi-stream)~~ **SUPERSEDED** | RNG |
| ADR-0015 | CMake 4 + Ninja | Build |
| ADR-0016 | Reference-compatible RNG (provisional) | RNG |

---

Decisions are binding unless marked **SUPERSEDED**. ADR-0014 is superseded by ADR-0016. In particular:

- The "PCG32" language in ADR-0011 applies only to non-parity math utilities until the reference RNG is reconstructed.
- The reference game uses Unity 2018.4.12f1; any port of `UnityEngine.Random` must match that version's state transition.