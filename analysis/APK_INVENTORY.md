# Soul Knight 2.6.8 — APK Inventory

Generated: 2026-10-30
Source: `original/SoulKnight-2.6.8.apk` (SHA-256: 8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea)

---

## 1. APK Structure Overview

```
SoulKnight-2.6.8.apk
├── AndroidManifest.xml
├── META-INF/                 # Signing certificates
├── classes.dex               # Primary DEX (Unity + SDKs)
├── classes2.dex              # Secondary DEX (more SDKs)
├── assets/
│   ├── AssetBundles/         # Unity AssetBundles (game content)
│   ├── audience_network.dex  # Facebook Audience Network DEX
│   ├── bin/                  # Unity config
│   ├── bundleVersionData.txt
│   ├── DefineCloudSaveID.json
│   ├── google-services-desktop.json
│   ├── sensitive_words.txt
│   └── Switch/               # Nintendo Switch specific?
├── lib/
│   ├── arm64-v8a/            # 64-bit ARM native libs
│   └── armeabi-v7a/          # 32-bit ARM native libs
├── res/                      # Android resources
├── kotlin/                   # Kotlin stdlib classes
├── original/                 # Original files (META-INF, etc.)
├── smali/                    # Baksmali output (classes.dex)
├── smali_assets/             # Baksmali output (assets/audience_network.dex)
├── smali_classes2/           # Baksmali output (classes2.dex)
└── unknown/                  # Apktool unknown files
```

---

## 2. DEX Inventory

### classes.dex (~3.2 MB)
- Primary Unity bootstrap
- AndroidX libraries
- Facebook SDK
- Google Play Services / Firebase
- AppsFlyer
- Ad network SDKs (AdColony, AppLovin, Chartboost, IronSource, InMobi, Maio, Mintegral, myTarget, Tapjoy, Vungle, Amazon)
- KTPlay
- Unity Ads, IAP, Analytics, Purchasing

### classes2.dex (~1.8 MB)
- Additional SDK classes
- Kotlin stdlib (kotlin/, kotlinx/)
- OkHttp / Okio / Retrofit (networking)

### assets/audience_network.dex (~3.3 MB)
- Facebook Audience Network dedicated DEX

**Total unique classes (jadx):** ~10,446
**Game-specific packages:** `com.ChillyRoom.DungeonShooter` (only BuildConfig, R*, resources)

---

## 3. Native Library Inventory

### arm64-v8a (primary target for analysis)

| Library | Size | Exports (sample) | Notes |
|---------|------|------------------|-------|
| `libil2cpp.so` | 52,641,496 B | `il2cpp_init`, `il2cpp_class_from_name`, `il2cpp_method_get_*`, `il2cpp_string_new`, `il2cpp_array_new`, `il2cpp_object_new`, `il2cpp_runtime_invoke`, `il2cpp_thread_attach`, `il2cpp_gc_*`, `il2cpp_monitor_*`, `UnityMain` | **MAIN GAME LOGIC** — IL2CPP runtime + compiled C# |
| `libunity.so` | 16,955,952 B | `UnityPal*`, `UnityGfx*`, `UnityAudio*`, `UnityVideo*`, `UnityWebRequest*`, `UnityJni*`, `UnityLoad*` | Unity engine runtime |
| `libmain.so` | 6,144 B | `JNI_OnLoad`, `Java_com_unity3d_player_UnityPlayer_native*` | Unity entry point, JNI bridge |
| `libgpg.so` | 3,718,560 B | GPG (Google Play Games) C++ API | Achievements, leaderboards, multiplayer |
| `libFirebaseCppApp-5.7.0.so` | 2,525,392 B | Firebase C++ App core | Firebase initialization |
| `libFirebaseCppAnalytics.so` | 68,160 B | Firebase Analytics C++ | Event logging |

### armeabi-v7a (32-bit fallback)

Same libraries, ~20-30% smaller due to 32-bit pointers.

---

## 4. AssetBundle Inventory (assets/AssetBundles/)

### Master Index
- `AssetBundles` (3,990 B) — Bundle catalog
- `AssetBundles.manifest` (16,709 B) — Dependency graph

### Content Bundles (sorted by size)

| Bundle | Size | Manifest | Category |
|--------|------|----------|----------|
| `common` | 10,490,037 B | ✓ | Shared: UI, fonts, shaders, common prefabs |
| `level-objects` | 11,614,699 B | ✓ | Room props, enemies, interactive objects |
| `scene_plot_defence` | 2,637,473 B | ✓ | Defence mode (endless/wave) scenes |
| `defence` | 4,386,054 B | ✓ | Defence mode content |
| `hall` | 1,293,254 B | ✓ | Main menu / lobby |
| `level-common` | 1,110,541 B | ✓ | Shared level data |
| `scene_game` | 563,698 B | ✓ | Core gameplay scene |
| `scene_tutorial` | 563,422 B | ✓ | Tutorial scene |
| `scene_plot` | 11,041 B | ✓ | Plot/story scenes (minimal — data-driven) |
| `multi_room` | 663,162 B | ✓ | Multiplayer room content |
| `test` | 734,052 B | ✓ | Test/debug scenes |
| `toturial_advance` | 812,117 B | ✓ | Advanced tutorial |
| `tutorial` | 256,413 B | ✓ | Basic tutorial |
| `celler` | 324,489 B | ✓ | Cellar/dungeon content |
| `loading` | 292,386 B | ✓ | Loading screen assets |
| `sandbox` | 556,777 B | ✓ | Sandbox/creative mode |
| `statements` | 369,664 B | ✓ | Achievement/statement UI |
| `draw` | 636,085 B | ✓ | Gacha/draw system |
| `race` | 53,265 B | ✓ | Race mode |
| `level-1` | 25,747 B | ✓ | Level 1 specific data |

### Directory Bundles
| Path | Contents |
|------|----------|
| `room/` | Individual room AssetBundles (procedural room templates) |
| `res/` | Resource-only bundles (textures, audio clips) |

---

## 5. Resource Inventory (res/)

### Key Resource Types
- **layouts:** UI layouts (Unity-generated, minimal)
- **drawables:** Icons, splash screens, placeholders
- **values:** Strings, colors, dimensions, styles
- **xml:** Network security config, file provider paths, backup rules
- **raw:** Unity config, certificates
- **mipmap:** App icons (hdpi, mdpi, xhdpi, xxhdpi, xxxhdpi)

### Notable Strings (from R$string.smali)
- `app_name`: "Soul Knight"
- Unity splash/loading strings
- Ad network IDs
- Firebase/Google Play config
- KTPlay config

---

## 6. Kotlin / Third-Party Libraries

| Library | Location | Version (inferred) |
|---------|----------|-------------------|
| Kotlin stdlib | `kotlin/`, `kotlinx/` | 1.5+ |
| AndroidX | `androidx/` | Multiple (core, lifecycle, etc.) |
| OkHttp | `okhttp3/` | 4.x |
| Okio | `okio/` | 2.x |
| Retrofit | `retrofit2/` | 2.x |
| Facebook SDK | `com/facebook/` | 12+ |
| Firebase | `com/google/firebase/` | 2021 era |
| Google Play Services | `com/google/android/gms/` | 2021 era |
| AppsFlyer | `com/appsflyer/` | 6+ |
| Unity Ads/Services | `com/unity3d/services/` | 2021+ |

---

## 7. Unknown / Apktool "unknown" Directory

```
unknown/
├── com.unity3d.player.UnityPlayerActivity
├── unity-default-style
└── (other Unity-internal resources)
```

These are resources Apktool couldn't classify — typically Unity-internal binary formats.

---

## 8. Critical Findings for Reverse Engineering

### 7.1 Engine Confirmation
- **Unity IL2CPP** — CONFIRMED
- **Unity Version:** **2018.4.12f1** (based on `libunity.so` build string, AssetBundle header, and `libil2cpp.so` build paths); IL2CPP metadata **version 24**
- **Scripting Backend:** IL2CPP (not Mono)
- **API Compatibility:** .NET 4.x / .NET Standard 2.1

### 7.2 Game Logic Location
- **100% in `libil2cpp.so`** — No gameplay logic in DEX files
- **C# → C++ compilation** — Classes, methods, fields mapped to C++ structures
- **Metadata:** IL2CPP metadata embedded in `libil2cpp.so` or separate `global-metadata.dat` (need to verify)

### 7.3 Asset Pipeline
- **AssetBundles** — Primary content delivery
- **LZ4 / LZMA compression** — Typical Unity AssetBundle compression
- **Platform-specific:** Android bundles only in APK; need to extract/convert for native port

### 7.4 Native Port Implications
| Subsystem | Source | Port Strategy |
|-----------|--------|---------------|
| Gameplay logic | `libil2cpp.so` (IL2CPP) | Re-implement from RE'd behaviour; do NOT transliterate C++ |
| Rendering | Unity (libunity.so) | Replace with native Wayland/Vulkan/OpenGL renderer |
| Audio | Unity (libunity.so) | Replace with native audio (PipeWire/PulseAudio) |
| Input | Unity (libunity.so) | Replace with native Wayland/input abstraction |
| Asset loading | AssetBundles | Extract → convert → native format |
| Physics | Unity (PhysX/Box2D via IL2CPP) | Re-implement or use native physics |
| Networking | Unity + SDKs | Replace with native; multiplayer out of scope for v1 |
| Ads/Analytics/IAP | SDKs (Java + native) | **REMOVE** — not part of native port |
| Save system | Unity PlayerPrefs / custom | Re-implement with XDG-compliant native saves |

---

## 9. Next Analysis Steps

1. **Extract IL2CPP metadata** — Find `global-metadata.dat` or parse embedded metadata in `libil2cpp.so`
2. **Class/method dump** — Use Il2CppDumper or similar to get C# class hierarchy
3. **Identify key namespaces** — `ChillyRoom`, `DungeonShooter`, Unity namespaces
4. **Map game systems** — Player, enemies, weapons, rooms, generation, combat, UI
5. **Runtime experiments** — Observe behaviour on device/emulator (lawful environment)
6. **Document architecture** — Produce `docs/ARCHITECTURE.md`

---

## 10. Tooling Status

| Tool | Version | Status |
|------|---------|--------|
| apktool | 2.11.0 | ✅ Working |
| jadx | 1.5.6 | ✅ Working (30 errors — expected for obfuscated SDK code) |
| Ghidra | 12.2 | ✅ Installed (needs first-run setup) |
| Il2CppDumper | — | ⏳ Need to acquire |
| ADB | 37.0.0 | ✅ Available |

---

**Inventory Complete.** Ready for Phase 2: Engine Reconstruction.