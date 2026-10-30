# Soul Knight 2.6.8 — Class Map (DEX Layer)

Generated: 2026-10-30
Source: `analysis/apktool_out/smali/`, `analysis/jadx_out/sources/`
Reference APK: SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`

---

## Overview

**Critical Finding:** This is a Unity IL2CPP game. The DEX layer contains **ZERO gameplay classes**. All game logic (player, enemies, weapons, rooms, combat, generation, UI, progression) resides in `libil2cpp.so` as compiled C++ from C#.

The Java/Kotlin DEX classes are exclusively:
1. Unity bootstrap (`UnityPlayerActivity`, `UnityPlayer`)
2. Third-party SDKs (ads, analytics, IAP, social, attribution)
3. AndroidX / Kotlin stdlib
4. Auto-generated `R.*` and `BuildConfig`

---

## Game-Specific Java Packages

### `com.ChillyRoom.DungeonShooter`

| Class | Type | Confidence | Notes |
|-------|------|------------|-------|
| `BuildConfig` | Auto-generated | CONFIRMED | `DEBUG=false`, `VERSION_CODE=20680`, `VERSION_NAME="2.6.8"` |
| `R` | Auto-generated | CONFIRMED | Resource IDs only |
| `R$anim` | Auto-generated | CONFIRMED | Animation resource IDs |
| `R$array` | Auto-generated | CONFIRMED | Array resource IDs |
| `R$attr` | Auto-generated | CONFIRMED | Attribute resource IDs |
| `R$bool` | Auto-generated | CONFIRMED | Boolean resource IDs |
| `R$color` | Auto-generated | CONFIRMED | Color resource IDs |
| `R$dimen` | Auto-generated | CONFIRMED | Dimension resource IDs |
| `R$drawable` | Auto-generated | CONFIRMED | Drawable resource IDs |
| `R$id` | Auto-generated | CONFIRMED | View IDs |
| `R$integer` | Auto-generated | CONFIRMED | Integer resource IDs |
| `R$interpolator` | Auto-generated | CONFIRMED | Interpolator resource IDs |
| `R$layout` | Auto-generated | CONFIRMED | Layout resource IDs |
| `R$menu` | Auto-generated | CONFIRMED | Menu resource IDs |
| `R$mipmap` | Auto-generated | CONFIRMED | Mipmap resource IDs |
| `R$raw` | Auto-generated | CONFIRMED | Raw resource IDs |
| `R$string` | Auto-generated | CONFIRMED | String resource IDs |
| `R$styleable` | Auto-generated | CONFIRMED | Styleable resource IDs |
| `R$style` | Auto-generated | CONFIRMED | Style resource IDs |
| `R$xml` | Auto-generated | CONFIRMED | XML resource IDs |

### `com.ChillyRoom.DungeonShooter.resources`

| Class | Type | Confidence | Notes |
|-------|------|------------|-------|
| `BuildConfig` | Auto-generated | CONFIRMED | Duplicate for resources module |
| `R` | Auto-generated | CONFIRMED | Resource IDs for resources module |

---

## Unity Engine Classes (Java)

### `com.unity3d.player`

| Class | Role | Confidence | Native Replacement |
|-------|------|------------|-------------------|
| `UnityPlayerActivity` | Main Activity entry point | CONFIRMED | Native Wayland window + game loop |
| `UnityPlayer` | Engine bridge | CONFIRMED | Native engine core |
| `NativeLoader` | Library loading | CONFIRMED | `dlopen`/`dlmopen` in native bootstrap |
| `ReflectionHelper` | JNI reflection | CONFIRMED | Not needed — no JNI in native port |

---

## Third-Party SDK Classes (Partial Inventory — For Reference Only)

These are **NOT part of the game** and will be **REMOVED** in the native port. Listed here for completeness of APK inventory.

### Facebook SDK (`com.facebook.*`, `com.facebook.unity.*`)
- Login, Sharing, App Links, Analytics, Audience Network, Ads
- ~50 classes

### Google Play Services / Firebase (`com.google.*`)
- Ads (AdMob), Analytics, Auth, Firebase (App, Messaging, Installations, Crashlytics), Play Games
- ~100+ classes

### Unity Services (`com.unity3d.services.*`)
- Ads, Analytics, IAP, Purchasing, Connect
- ~80+ classes

### Ad Networks (10+ networks)
- AdColony, AppLovin, Chartboost, IronSource, InMobi, Maio, Mintegral, myTarget, Tapjoy, Vungle, Amazon
- ~200+ classes total

### AppsFlyer (`com.appsflyer.*`)
- Install attribution
- ~10 classes

### KTPlay (`com.ktplay.*`)
- Community, chat, user-generated content
- ~30 classes

### Networking (`okhttp3`, `okio`, `retrofit2`, `kotlinx.coroutines`)
- HTTP client stack used by SDKs
- ~60 classes

### AndroidX / Jetpack
- Lifecycle, Core, Annotation, etc.
- ~40 classes

---

## Class Map Format (For IL2CPP Layer — To Be Populated)

Once IL2CPP metadata is extracted, the real class map will follow this format:

```markdown
## IL2CPP Class Map (from libil2cpp.so)

| C# Class (Namespace.Type) | IL2CPP TypeInfo Address | Methods (count) | Fields (count) | Subsystem | Confidence | Native Replacement | Impl Status |
|---------------------------|------------------------|-----------------|----------------|-----------|------------|-------------------|-------------|
| ChillyRoom.Player         | 0x...                  | 45              | 23             | Player    | HIGH       | src/game/player/Player.cpp | PENDING |
| ChillyRoom.Weapon         | 0x...                  | 32              | 18             | Weapons   | HIGH       | src/game/weapons/Weapon.cpp | PENDING |
| ChillyRoom.Enemy          | 0x...                  | 38              | 21             | Enemies   | HIGH       | src/game/enemies/Enemy.cpp | PENDING |
| ChillyRoom.Room           | 0x...                  | 28              | 15             | Rooms     | HIGH       | src/game/rooms/Room.cpp | PENDING |
| ChillyRoom.Generation.DungeonGenerator | 0x... | 22 | 12 | Generation | HIGH | src/game/generation/DungeonGenerator.cpp | PENDING |
```

### Priority Subsystems for Mapping

1. **Player** — `ChillyRoom.Player`, `ChillyRoom.PlayerController`, `ChillyRoom.Character`
2. **Enemies** — `ChillyRoom.Enemy`, `ChillyRoom.EnemyAI`, `ChillyRoom.Boss*`
3. **Weapons** — `ChillyRoom.Weapon`, `ChillyRoom.Projectile`, `ChillyRoom.Bullet`
4. **Rooms** — `ChillyRoom.Room`, `ChillyRoom.RoomController`, `ChillyRoom.DungeonGenerator`
5. **Generation** — `ChillyRoom.Generation.*`, `ChillyRoom.LevelGenerator`
6. **Items** — `ChillyRoom.Item`, `ChillyRoom.Pickup`, `ChillyRoom.Chest`
7. **Progression** — `ChillyRoom.Progression`, `ChillyRoom.SaveData`, `ChillyRoom.UnlockManager`
8. **UI** — `ChillyRoom.UI.*`, `ChillyRoom.HUD`, `ChillyRoom.Menu*`
9. **Combat** — `ChillyRoom.Combat*`, `ChillyRoom.Damage*`, `ChillyRoom.StatusEffect*`
10. **Audio** — `ChillyRoom.Audio*`, `ChillyRoom.SoundManager`

---

## Confidence Legend

| Label | Meaning |
|-------|---------|
| CONFIRMED | Directly observed in DEX/manifest |
| HIGH CONFIDENCE | Strong evidence from symbols/strings |
| HYPOTHESIS | Inferred from patterns, needs verification |
| UNKNOWN | Not yet located |
| CONTRADICTED | Evidence disproves assumption |

---

## Next Steps

1. Run Il2CppDumper on `libil2cpp.so` + `global-metadata.dat` (if separate)
2. Extract full C# class hierarchy
3. Map to native implementation files per `docs/ARCHITECTURE.md`
4. Update this file with IL2CPP class map

---

**Status:** DEX layer fully mapped. **Game logic is 0% in DEX — 100% in libil2cpp.so.** Proceed to native library analysis.