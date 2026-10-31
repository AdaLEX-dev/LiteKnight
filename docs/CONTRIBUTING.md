# Soul Knight 2.6.8 — Contributing Guide

**Target:** CachyOS, x86-64, Wayland, 1920×1080, 60 FPS

---

## Quick Start

```bash
# 1. Clone
git clone <repo>
cd SoulKnight-Linux

# 2. Configure (installs deps via FetchContent)
./tools/configure.sh

# 3. Build
./tools/build.sh Debug

# 4. Run
./tools/run.sh

# 5. Test
./tools/test.sh
```

---

## Prerequisites

### System (CachyOS / Arch)
```bash
sudo pacman -S --needed \
    base-devel cmake ninja gcc \
    sdl3 vulkan-headers vulkan-icd-loader vulkan-validation-layers \
    libpng shaderc \
    libxkbcommon wayland wayland-protocols \
    pipewire alsa-lib \
    python3 git
```

### Optional (for RE/analysis)
```bash
# Install manually (not in repo)
# - apktool, jadx, Ghidra
# - Il2CppDumper (.NET 6)
# - AssetRipper
# - RenderDoc
```

---

## Build Configurations

| Config | Use Case | Flags |
|--------|----------|-------|
| `Debug` | Development, debugging | `-O0 -g -fsanitize=address,undefined` |
| `RelWithDebInfo` | Profiling, testing | `-O2 -g -DNDEBUG` |
| `Release` | Final binary | `-O3 -flto -DNDEBUG` (no `-march=native`; binaries must stay reproducible) |

```bash
./tools/build.sh Debug       # Default
./tools/build.sh RelWithDebInfo
./tools/build.sh Release
```

---

## Project Structure

Actual tracked source (do not document empty directories as if they exist):

```
src/
├── app/                    # Application lifecycle, config, bootstrap, main.cpp
├── platform/
│   ├── wayland/            # SDL3 window, Wayland enforcement, Vulkan surface creation
│   ├── input/              # SDL3 keyboard/mouse/gamepad snapshot + action mapping
│   ├── timing/             # 50 Hz fixed-step accumulator
│   ├── filesystem/         # XDG paths, asset lookup
│   └── audio/              # miniaudio wrapper + vendored header
├── engine/
│   ├── core/               # Log, foundational types
│   ├── math/               # Vec2/Vec3/Mat4, PCG32 utility, UnityRandom/RGRandomStream
│   ├── ecs/                # Entity, ComponentPool, World (no iteration API yet)
│   └── rendering/          # Vulkan context/swapchain, sprite batch/pipeline, RGBA8 texture
└── game/
    └── player/             # unimplemented player/input scaffolding only
```

Planned directories from the roadmap are not created until their evidence gates
pass. Do not add empty folder trees to make the layout look complete.

---

## Coding Standards

### C++20
- Modules not yet (header units OK)
- Concepts for constraints
- `std::expected` for errors (no exceptions)
- `std::span`, `std::string_view`, `std::format`
- Structured bindings, `if constexpr`

### Style (enforced by clang-format)
```cpp
// 4 spaces, no tabs
// Braces on same line
// 100 char line limit
// trailing commas in lists
```

Run: `./tools/format.sh` (clang-format)

### Naming
| Type | Convention | Example |
|------|------------|---------|
| Types | PascalCase | `PlayerController` |
| Functions/Methods | PascalCase | `Initialize()` |
| Variables | snake_case | `frame_time_ms` |
| Constants | kPascalCase | `kMaxEntities` |
| Macros | UPPER_SNAKE | `LOG_INFO` |
| Namespaces | lowercase | `engine::rendering` |

### Architecture Rules
1. **No platform headers in game code** — use `platform/` interfaces
2. **No Vulkan in game code** — use `engine::rendering` abstractions
3. **Deterministic simulation** — fixed-step scaffold and isolated RNG streams; the reference tick rate is unresolved
4. **Data-oriented** — struct of arrays, component pools
5. **No singletons** — pass dependencies explicitly
6. **RAII everywhere** — `unique_ptr`, custom deleters for Vulkan objects

---

## Git Workflow

### Branches
- `main` — Stable, tagged releases only
- `develop` — Integration branch (optional)
- `feature/*` — Feature branches (delete after merge)

### Commits (Conventional Commits)
```
<type>(<scope>): <subject>

<body>

<footer>
```

| Type | Use |
|------|-----|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Code restructuring |
| `perf` | Performance improvement |
| `test` | Test addition/modification |
| `docs` | Documentation |
| `build` | Build system |
| `ci` | CI/CD |
| `chore` | Maintenance |
| `reverse` | Reverse engineering work |

Examples:
```
feat(rendering): add sprite batching with instanced draws
fix(combat): correct crit roll during dodge invincibility
reverse(analysis): extract IL2CPP metadata for EnemyAI01-20
docs(architecture): update enemy AI state machine diagram
```

### Before Commit
```bash
git status
git diff --check
git diff
./tools/test.sh
./tools/build.sh Debug
```

### Pull Requests
- Target `main` (or `develop`)
- Squash commits
- Pass CI: format, build, tests
- Update relevant docs
- Link to issue/ADR

---

## Testing

### Run All
```bash
./tools/test.sh
```

### Categories
Only `all` and `unit` currently exist:

```bash
./tools/test.sh               # all 39 cases
./tools/test.sh unit          # unit suite
```

`integration`, `gameplay`, `regression`, `serialization`, `procedural`, and
`performance` are planned. `tools/test.sh` exits non-zero for them instead of
running an empty filter.

### Writing Tests
```cpp
// tests/unit/math_test.cpp
#include <doctest/doctest.h>
#include "engine/math/Vec2.h"

TEST_CASE("Vec2 operations") {
    Vec2 a{3, 4};
    Vec2 b{1, 2};

    CHECK(a.Length() == 5.0f);
    CHECK((a + b) == Vec2{4, 6});
    CHECK(a.Dot(b) == 11.0f);
}
```

### Test Naming
```cpp
// Good
TEST("weapon_fire_consumes_energy")
TEST("same_seed_generates_same_room_graph")
TEST("armor_reduces_damage")

// Bad
TEST("test1")
TEST("thing_test")
TEST("weapon_test")
```

---

## Documentation

### Update on Every Change
- `docs/ARCHITECTURE.md` — System boundaries
- `docs/PARITY.md` — Fidelity status
- `docs/BUGS.md` — Bug tracker
- `docs/PROJECT_STATE.md` — Session log
- `docs/ROADMAP.md` — Milestones
- `CHANGELOG.md` — User-visible changes

### ADR Process
For significant decisions:
1. Create `docs/DECISIONS/ADR-NNNN-title.md`
2. Use template from `docs/TECHNOLOGY_DECISIONS.md`
3. Link from PR and `docs/TECHNOLOGY_DECISIONS.md`

---

## Reverse Engineering Guidelines

### Evidence First
- Never paste decompiler output into source
- Document in `analysis/` with confidence labels
- Reference: `analysis/class-map.md`, `native-libraries.md`, `ARTIFACT.md`

### Clean Room
```
decompiler → understanding → clean design → native implementation → test
```

### Legal
- No APK in repo
- No proprietary assets in repo
- No extracted proprietary dumps in repo
- Native assets: converted at build, loaded at runtime

---

## Performance

### Budgets (16.67 ms/frame)
| System | Budget |
|--------|--------|
| Simulation | 4 ms |
| Render | 6 ms |
| Audio | 1 ms |
| Input/OS | 1 ms |
| Margin | 4.67 ms |

### Profiling
```bash
./tools/profile.sh          # perf + heaptrack
./tools/profile.sh renderdoc # RenderDoc capture
```

### Optimization Checklist
- [ ] Profile first (no premature optimization)
- [ ] Measure before/after
- [ ] Document in PR: Before/Change/After/Measurement
- [ ] Prefer algorithmic over micro-optimizations

---

## Release Process

```bash
# 1. Verify
git status
./tools/build.sh Release
./tools/test.sh

# 2. Version bump
echo "1.0.0" > VERSION

# 3. Changelog
# Edit CHANGELOG.md

# 4. Commit & Tag
git commit -am "release: v1.0.0"
git tag -a v1.0.0 -m "v1.0.0: First stable release"

# 5. Push (explicit)
git push origin main --tags
```

---

## Code Review Checklist

- [ ] Builds in Debug/RelWithDebInfo/Release
- [ ] All tests pass
- [ ] clang-format clean
- [ ] No platform leakage in game code
- [ ] Deterministic (no frame-rate dependent sim)
- [ ] RNG streams isolated
- [ ] Memory: no leaks (heaptrack clean)
- [ ] Docs updated
- [ ] CHANGELOG entry if user-visible
- [ ] Conventional commit message

---

## Getting Help

- Check `docs/ARCHITECTURE.md` for system design
- Check `analysis/UNKNOWN_SYSTEMS.md` for open questions
- Check `docs/PARITY.md` for fidelity status
- Run `./tools/run.sh --help` for runtime flags

---

**Welcome to the project!** Read `NEMOTRON_MASTER_PROJECT.md` for the full vision.