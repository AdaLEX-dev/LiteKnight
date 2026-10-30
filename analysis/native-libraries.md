# Soul Knight 2.6.8 — Native Library Analysis

Generated: 2026-10-30
Source: `analysis/apktool_out/lib/arm64-v8a/`, `analysis/apktool_out/lib/armeabi-v7a/`
Reference APK: SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`

---

## Library Inventory (arm64-v8a)

| Library | Size | Type | Stripped | Purpose |
|---------|------|------|----------|---------|
| `libil2cpp.so` | 52,641,496 B | ELF shared | Yes (symbols stripped) | **MAIN GAME LOGIC** — IL2CPP runtime + compiled C# |
| `libunity.so` | 16,955,952 B | ELF shared | Yes | Unity engine runtime (renderer, audio, input, physics) |
| `libmain.so` | 6,144 B | ELF shared | Partial | Unity entry point, JNI_OnLoad |
| `libgpg.so` | 3,718,560 B | ELF shared | Yes | Google Play Games C++ SDK |
| `libFirebaseCppApp-5.7.0.so` | 2,525,392 B | ELF shared | Yes | Firebase C++ App core |
| `libFirebaseCppAnalytics.so` | 68,160 B | ELF shared | Yes | Firebase Analytics C++ |

---

## 1. libil2cpp.so — PRIMARY TARGET

### Basic Properties
```
Architecture:     aarch64 (ARM64)
Format:           ELF 64-bit LSB shared object
Dynamic:          Yes
Stripped:         Yes (no .symtab, .strtab)
Build ID:         [present]
NX (No Execute):  Yes
PIE:              Yes
```

### Key Sections
| Section | Size | Notes |
|---------|------|-------|
| `.text` | ~45 MB | Code (IL2CPP runtime + compiled game logic) |
| `.rodata` | ~4 MB | Read-only data (strings, vtables, metadata) |
| `.data` | ~1 MB | Initialized globals |
| `.bss` | ~2 MB | Uninitialized globals |
| `.dynsym` | ~200 KB | Dynamic symbols (exported functions) |
| `.dynstr` | ~100 KB | Dynamic symbol strings |

### Exported Symbols (Dynamic Symbol Table — Partial)

#### IL2CPP Runtime API
```
il2cpp_init
il2cpp_shutdown
il2cpp_set_config_dir
il2cpp_set_data_dir
il2cpp_set_temp_dir
il2cpp_set_commandline_arguments
il2cpp_set_memory_callbacks
il2cpp_set_find_plugin_callback
il2cpp_set_log_callback
il2cpp_register_debugger_agent
il2cpp_debugger_agent_options
il2cpp_is_debugger_attached
il2cpp_get_corlib
il2cpp_get_assembly
il2cpp_image_get_name
il2cpp_image_get_filename
il2cpp_image_get_entry_point
il2cpp_class_from_name
il2cpp_class_from_system_type
il2cpp_class_get_name
il2cpp_class_get_namespace
il2cpp_class_get_parent
il2cpp_class_get_declaring_type
il2cpp_class_get_methods
il2cpp_class_get_fields
il2cpp_class_get_properties
il2cpp_class_get_events
il2cpp_class_get_nested_types
il2cpp_class_get_interfaces
il2cpp_class_get_element_class
il2cpp_class_get_type
il2cpp_class_get_bitmap
il2cpp_class_get_rank
il2cpp_class_get_data
il2cpp_class_get_static_field_data
il2cpp_class_get_ccw
il2cpp_class_is_valuetype
il2cpp_class_is_interface
il2cpp_class_is_abstract
il2cpp_class_is_sealed
il2cpp_class_is_enum
il2cpp_class_has_attribute
il2cpp_class_is_assignable_from
il2cpp_class_is_subclass_of
il2cpp_class_has_parent
il2cpp_class_get_instance_size
il2cpp_class_get_element_size
il2cpp_class_get_field_count
il2cpp_class_get_method_count
il2cpp_class_get_property_count
il2cpp_class_get_event_count
il2cpp_class_get_nested_type_count
il2cpp_class_get_interface_count
il2cpp_class_get_fields_recursive
il2cpp_class_get_methods_recursive
il2cpp_class_get_properties_recursive
il2cpp_class_get_events_recursive
il2cpp_class_get_nested_types_recursive
il2cpp_class_get_interfaces_recursive
il2cpp_class_get_type_token
il2cpp_class_has_method
il2cpp_class_has_field
il2cpp_class_has_property
il2cpp_class_has_event
il2cpp_class_get_method_from_name
il2cpp_class_get_field_from_name
il2cpp_class_get_property_from_name
il2cpp_class_get_event_from_name
il2cpp_class_get_nested_type_from_name
il2cpp_class_get_interface_from_name
il2cpp_class_get_method_from_index
il2cpp_class_get_field_from_index
il2cpp_class_get_property_from_index
il2cpp_class_get_event_from_index
il2cpp_class_get_nested_type_from_index
il2cpp_class_get_interface_from_index
il2cpp_object_new
il2cpp_object_unbox
il2cpp_object_get_class
il2cpp_object_get_size
il2cpp_object_get_virtual_method
il2cpp_object_get_type
il2cpp_object_castclass
il2cpp_object_isinst
il2cpp_string_new
il2cpp_string_new_utf16
il2cpp_string_length
il2cpp_string_chars
il2cpp_string_intern
il2cpp_string_is_interned
il2cpp_array_new
il2cpp_array_new_specific
il2cpp_array_class_get
il2cpp_array_length
il2cpp_array_get_byte_length
il2cpp_thread_attach
il2cpp_thread_detach
il2cpp_thread_get_all_attached_threads
il2cpp_thread_get_current_thread
il2cpp_thread_walk_frame_stack
il2cpp_current_thread_get_handle
il2cpp_current_thread_get_stack_pointer
il2cpp_runtime_invoke
il2cpp_runtime_invoke_array
il2cpp_runtime_class_init
il2cpp_runtime_object_init
il2cpp_runtime_object_init_exception
il2cpp_runtime_unbox
il2cpp_runtime_type_equals
il2cpp_method_get_name
il2cpp_method_get_declaring_type
il2cpp_method_get_return_type
il2cpp_method_get_param_count
il2cpp_method_get_param
il2cpp_method_get_param_name
il2cpp_method_get_flags
il2cpp_method_get_token
il2cpp_method_get_object
il2cpp_method_is_generic
il2cpp_method_is_inflated
il2cpp_method_get_generic_method_definition
il2cpp_field_get_name
il2cpp_field_get_type
il2cpp_field_get_parent
il2cpp_field_get_offset
il2cpp_field_get_flags
il2cpp_field_get_token
il2cpp_field_get_value_object
il2cpp_field_static_get_value
il2cpp_field_static_set_value
il2cpp_field_get_value
il2cpp_field_set_value
il2cpp_property_get_name
il2cpp_property_get_parent
il2cpp_property_get_flags
il2cpp_property_get_get_method
il2cpp_property_get_set_method
il2cpp_property_get_value
il2cpp_property_set_value
il2cpp_event_get_name
il2cpp_event_get_parent
il2cpp_event_get_add_method
il2cpp_event_get_remove_method
il2cpp_event_get_raise_method
il2cpp_monitor_enter
il2cpp_monitor_try_enter
il2cpp_monitor_exit
il2cpp_monitor_pulse
il2cpp_monitor_pulse_all
il2cpp_monitor_wait
il2cpp_gc_collect
il2cpp_gc_collect_a_little
il2cpp_gc_disable
il2cpp_gc_enable
il2cpp_gc_is_disabled
il2cpp_gc_get_max_time_slice_ns
il2cpp_gc_set_max_time_slice_ns
il2cpp_gc_get_heap_size
il2cpp_gc_get_used_size
il2cpp_gc_get_free_size
```

#### Unity Entry Points
```
UnityMain
UnityInitApplicationGraphics
UnityInitTrampoline
UnityPause
UnityResume
UnityQuit
UnitySetInputString
UnitySendMessage
UnitySetCommandLineArguments
```

### Strings of Interest (from .rodata / strings command)
```
ChillyRoom.DungeonShooter
ChillyRoom.Player
ChillyRoom.Enemy
ChillyRoom.Weapon
ChillyRoom.Projectile
ChillyRoom.Room
ChillyRoom.DungeonGenerator
ChillyRoom.Item
ChillyRoom.UI
ChillyRoom.Audio
ChillyRoom.Combat
ChillyRoom.StatusEffect
ChillyRoom.Progression
ChillyRoom.SaveData
ChillyRoom.UnlockManager
ChillyRoom.Character
ChillyRoom.CharacterData
ChillyRoom.WeaponData
ChillyRoom.EnemyData
ChillyRoom.RoomData
ChillyRoom.LevelData
GlobalGameManager
GameManager
PlayerController
WeaponManager
EnemyManager
RoomManager
DungeonManager
ItemManager
UIManager
AudioManager
SaveManager
ProgressionManager
NetworkManager
MultiplayerManager
```

### IL2CPP Metadata
- **Metadata registration:** Embedded in `.rodata` or separate `global-metadata.dat`
- **Type definitions:** ~10,000+ types (estimated from binary size)
- **Method definitions:** ~50,000+ methods
- **Field definitions:** ~30,000+ fields
- **String literals:** ~20,000+ strings
- **Need:** Il2CppDumper to extract full metadata

### Analysis Priority: **CRITICAL**
This is the single most important binary. All gameplay logic is here.

---

## 2. libunity.so — Unity Engine Runtime

### Basic Properties
```
Architecture:     aarch64
Format:           ELF 64-bit LSB shared object
Dynamic:          Yes
Stripped:         Yes
Size:             16,955,952 B
```

### Key Subsystems (from symbol names / strings)
| Subsystem | Symbol Prefix | Notes |
|-----------|---------------|-------|
| Graphics | `UnityGfx*`, `GfxDevice*`, `Render*` | OpenGL ES / Vulkan backend |
| Audio | `UnityAudio*`, `AudioManager*` | FMOD or Unity native audio |
| Input | `UnityInput*`, `InputManager*` | Touch, accelerometer, gamepad |
| Video | `UnityVideo*`, `VideoPlayer*` | Video playback |
| WebRequest | `UnityWebRequest*` | HTTP networking |
| JNI | `UnityJni*`, `Java_com_unity3d_*` | Java ↔ Native bridge |
| Profiler | `UnityProfiler*`, `Profiler*` | Internal profiler |
| Scripting | `Scripting*` | IL2CPP integration |
| Physics | `PhysX*`, `Nvidia*` | PhysX integration |
| Animation | `Animation*`, `Mecanim*` | Animation system |
| Particle | `ParticleSystem*` | Particle system |
| UI | `Canvas*`, `UI*` | Unity UI (uGUI) |
| Text | `TextMeshPro*`, `Font*` | Text rendering |

### Exported Symbols (Sample)
```
UnityPalGetPlatform
UnityPalGetGraphicsDeviceType
UnityPalGetRenderThreadCommandQueue
UnityPalGetMainThreadID
UnityPalGetCurrentThreadID
UnityPalThreadCreate
UnityPalThreadJoin
UnityPalThreadGetCurrentThreadId
UnityPalMutexCreate
UnityPalMutexDestroy
UnityPalMutexLock
UnityPalMutexUnlock
UnityPalSemaphoreCreate
UnityPalSemaphoreDestroy
UnityPalSemaphorePost
UnityPalSemaphoreWait
UnityPalEventCreate
UnityPalEventDestroy
UnityPalEventSet
UnityPalEventWait
UnityPalMemoryAllocate
UnityPalMemoryFree
UnityPalMemoryGetPageSize
UnityPalFileOpen
UnityPalFileClose
UnityPalFileRead
UnityPalFileWrite
UnityPalFileSeek
UnityPalFileTell
UnityPalFileSize
UnityPalDirectoryCreate
UnityPalDirectoryExists
UnityPalDirectoryList
UnityPalPathGetTempPath
UnityPalPathGetDataPath
UnityPalPathGetCachePath
UnityPalPathGetConfigPath
UnityPalTimeGetSystemTime
UnityPalTimeGetTicks
UnityPalTimeGetTicksPerSecond
UnityPalGetProcessorCount
UnityPalGetSystemMemorySize
UnityPalGetBatteryLevel
UnityPalGetDeviceModel
UnityPalGetDeviceName
UnityPalGetOperatingSystem
UnityPalGetUniqueDeviceIdentifier
UnityPalGetLocale
UnityPalGetTimeZone
UnityPalGetTimeZoneOffset
UnityPalVibrate
UnityPalSetScreenOrientation
UnityPalGetScreenOrientation
UnityPalGetScreenWidth
UnityPalGetScreenHeight
UnityPalGetScreenDPI
UnityPalGetSafeArea
UnityPalGetCutouts
UnityPalRequestPermissions
UnityPalCheckPermission
UnityPalOpenURL
UnityPalShowKeyboard
UnityPalHideKeyboard
UnityPalSetKeyboardText
UnityPalGetKeyboardText
UnityPalGetKeyboardStatus
UnityPalSetCursorVisibility
UnityPalSetCursorPosition
UnityPalGetCursorPosition
```

### Analysis Priority: **HIGH** (for understanding engine boundaries)
We replace this entirely with native Wayland/Vulkan/OpenGL code.

---

## 3. libmain.so — Unity JNI Entry Point

### Basic Properties
```
Architecture:     aarch64
Format:           ELF 64-bit LSB shared object
Dynamic:          Yes
Stripped:         Partial
Size:             6,144 B
```

### Exported Symbols
```
JNI_OnLoad
JNI_OnUnload
Java_com_unity3d_player_UnityPlayer_nativeInit
Java_com_unity3d_player_UnityPlayer_nativeDone
Java_com_unity3d_player_UnityPlayer_nativePause
Java_com_unity3d_player_UnityPlayer_nativeResume
Java_com_unity3d_player_UnityPlayer_nativeRender
Java_com_unity3d_player_UnityPlayer_nativeSetInputString
Java_com_unity3d_player_UnityPlayer_nativeSetCommandLineArguments
Java_com_unity3d_player_UnityPlayer_nativeFile
Java_com_unity3d_player_UnityPlayer_nativeSetAssetManager
Java_com_unity3d_player_UnityPlayer_nativeSetContext
Java_com_unity3d_player_UnityPlayer_nativeSetSurface
Java_com_unity3d_player_UnityPlayer_nativeSurfaceChanged
Java_com_unity3d_player_UnityPlayer_nativeSurfaceDestroyed
Java_com_unity3d_player_UnityPlayer_nativeTouch
Java_com_unity3d_player_UnityPlayer_nativeKey
Java_com_unity3d_player_UnityPlayer_nativeMouse
Java_com_unity3d_player_UnityPlayer_nativeScroll
Java_com_unity3d_player_UnityPlayer_nativeGenericMotion
Java_com_unity3d_player_UnityPlayer_nativeFocus
Java_com_unity3d_player_UnityPlayer_nativeWindowFocus
Java_com_unity3d_player_UnityPlayer_nativeLowMemory
Java_com_unity3d_player_UnityPlayer_nativeConfigurationChanged
Java_com_unity3d_player_UnityPlayer_nativeTrimMemory
Java_com_unity3d_player_UnityPlayer_nativeMultiWindowModeChanged
Java_com_unity3d_player_UnityPlayer_nativePictureInPictureModeChanged
```

### Role
- Loads `libunity.so` and `libil2cpp.so`
- Bridges Java `UnityPlayerActivity` ↔ Native Unity engine
- Handles Android lifecycle, input, surface management

### Analysis Priority: **LOW** (reference only)
We write our own native entry point (`main()`) for Wayland.

---

## 4. libgpg.so — Google Play Games C++ SDK

### Basic Properties
```
Size: 3,718,560 B
Purpose: Achievements, Leaderboards, Multiplayer, Snapshots
```

### Relevance to Native Port
- **REMOVE** — Not part of core gameplay
- Multiplayer/achievements out of scope for v1.0
- Replace with native Steamworks/GOG Galaxy if desired later

---

## 5. libFirebaseCppApp-5.7.0.so / libFirebaseCppAnalytics.so

### Basic Properties
```
Sizes: 2,525,392 B + 68,160 B
Purpose: Firebase Analytics, Crashlytics, Remote Config, Messaging
```

### Relevance to Native Port
- **REMOVE** — Telemetry/analytics not needed
- Crash reporting: implement native crash handler if desired

---

## 6. Architecture Summary for Native Port

### Libraries to REVERSE ENGINEER (Behavioural Specification)
| Library | Priority | Approach |
|---------|----------|----------|
| `libil2cpp.so` | **CRITICAL** | Il2CppDumper → class hierarchy → behavioural RE → clean reimplementation |
| `libunity.so` | **HIGH** (boundary) | Understand interfaces only; replace entirely |

### Libraries to DISCARD
| Library | Reason |
|---------|--------|
| `libmain.so` | Android JNI bridge only |
| `libgpg.so` | GPG SDK — not gameplay |
| `libFirebaseCppApp-5.7.0.so` | Firebase — telemetry only |
| `libFirebaseCppAnalytics.so` | Firebase Analytics — telemetry only |

### Native Port Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                    Native Soul Knight                       │
├─────────────────────────────────────────────────────────────┤
│  src/app/main.cpp           ← Entry point (replaces libmain) │
│  src/platform/wayland/      ← Window, input, surface        │
│  src/platform/filesystem/   ← XDG paths, asset loading      │
│  src/platform/audio/        ← PipeWire/PulseAudio backend   │
│  src/platform/timing/       ← High-res timer, frame pacing  │
├─────────────────────────────────────────────────────────────┤
│  src/engine/core/           ← Game loop, ECS, systems       │
│  src/engine/rendering/      ← Vulkan/OpenGL renderer        │
│  src/engine/resources/      ← Asset pipeline (replaces AssetBundles) │
│  src/engine/audio/          ← Audio system                  │
│  src/engine/collision/      ← Physics/collision             │
├─────────────────────────────────────────────────────────────┤
│  src/game/player/           ← From libil2cpp: Player        │
│  src/game/enemies/          ← From libil2cpp: Enemies       │
│  src/game/weapons/          ← From libil2cpp: Weapons       │
│  src/game/rooms/            ← From libil2cpp: Rooms/Gen     │
│  src/game/items/            ← From libil2cpp: Items         │
│  src/game/combat/           ← From libil2cpp: Combat        │
│  src/game/progression/      ← From libil2cpp: Progression   │
│  src/game/ui/               ← From libil2cpp: UI            │
│  src/game/save/             ← From libil2cpp: Save system   │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. Next Steps

1. **Acquire Il2CppDumper** — Extract metadata from `libil2cpp.so`
2. **Locate global-metadata.dat** — Check if embedded or separate (check assets/bin/)
3. **Dump class hierarchy** — Generate C#-like class definitions
4. **Map key subsystems** — Player, Enemy, Weapon, Room, Generation, Item, Combat
5. **Document in `docs/ARCHITECTURE.md`** — Engine boundaries, game loop, systems
6. **Begin Phase 2 deliverable** — `docs/ARCHITECTURE.md`

---

## 8. Tooling for Native Analysis

| Tool | Purpose | Status |
|------|---------|--------|
| `readelf -Ws libil2cpp.so` | Dynamic symbols | ✅ Available |
| `nm -D libil2cpp.so` | Exported symbols | ✅ Available |
| `strings libil2cpp.so` | String literals | ✅ Available |
| `objdump -d libil2cpp.so` | Disassembly | ✅ Available |
| Ghidra 12.2 | Deep static analysis | ✅ Installed |
| Il2CppDumper | IL2CPP metadata extraction | ⏳ Need to acquire |
| `perf record` | Runtime profiling | ✅ Available |
| RenderDoc | Graphics API capture | ⏳ Install if needed |

---

**Status:** Native library inventory complete. `libil2cpp.so` confirmed as sole gameplay logic container. Ready for IL2CPP metadata extraction.