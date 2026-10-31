# Soul Knight 2.6.8 — Resources & Asset Inventory

Generated: 2026-10-30
Source: `analysis/apktool_out/res/`, `analysis/apktool_out/assets/AssetBundles/`
Reference APK: SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`

---

## 1. Android Resources (res/)

### Resource Directory Structure
```
res/
├── anim/           # Animation XML (Unity-generated)
├── color/          # Color state lists
├── drawable/       # Drawables (XML + PNG)
├── drawable-*/     # Density-specific drawables
├── layout/         # Layout XML (Unity-generated, minimal)
├── mipmap-*/       # App icons
├── raw/            # Raw assets (Unity config, certs)
├── values/         # Strings, colors, dims, styles
├── values-*/       # Locale-specific values
├── xml/            # XML configs (network security, file provider)
└── font/           # Font definitions (likely empty — Unity uses TMP)
```

### Key Resource Files

#### `res/values/strings.xml` (via R$string.smali)
- `app_name`: "Soul Knight"
- Unity splash/loading strings
- Ad network IDs (AdMob, Unity Ads, etc.)
- Firebase/Google Play config strings
- KTPlay config strings
- **No gameplay strings** — all localized text in AssetBundles

#### `res/xml/network_security_config.xml`
- Standard Android network security config
- Cleartext traffic permitted for specific domains (ads, analytics)

#### `res/xml/file_paths.xml` / `share_file_paths.xml`
- FileProvider paths for sharing/screenshots

#### `res/raw/`
- Unity device profiles
- Certificates for SSL pinning (ads/analytics)

---

## 2. AssetBundles — Primary Game Content

### Master Files
| File | Size | Purpose |
|------|------|---------|
| `assets/AssetBundles/AssetBundles` | 3,990 B | Bundle catalog (names, dependencies, CRCs) |
| `assets/AssetBundles/AssetBundles.manifest` | 16,709 B | Full dependency manifest (Unity 2018.4 format; original "2021" label was wrong) |

### Content Bundles (by Category)

#### Core Gameplay (Largest)
| Bundle | Size | Manifest | Content Type |
|--------|------|----------|--------------|
| `common` | 10.5 MB | ✓ | Shared: UI atlas, fonts, shaders, common prefabs, VFX |
| `level-objects` | 11.6 MB | ✓ | Room props, traps, interactive objects, enemy prefabs |
| `defence` | 4.4 MB | ✓ | Defence mode: waves, towers, special enemies |
| `scene_plot_defence` | 2.6 MB | ✓ | Defence mode scenes |
| `level-common` | 1.1 MB | ✓ | Shared level geometry, tilesets |

#### Scenes
| Bundle | Size | Manifest | Content |
|--------|------|----------|---------|
| `hall` | 1.3 MB | ✓ | Main menu / lobby scene |
| `scene_game` | 564 KB | ✓ | Core gameplay scene |
| `scene_tutorial` | 563 KB | ✓ | Tutorial scene |
| `scene_plot` | 11 KB | ✓ | Plot/story scene (data-driven, minimal assets) |
| `scene_plot_defence` | 2.6 MB | ✓ | Defence plot scene |

#### Modes
| Bundle | Size | Manifest | Content |
|--------|------|----------|---------|
| `multi_room` | 663 KB | ✓ | Multiplayer room assets |
| `race` | 53 KB | ✓ | Race mode |
| `sandbox` | 557 KB | ✓ | Sandbox/creative mode |
| `draw` | 636 KB | ✓ | Gacha/draw system |
| `celler` | 324 KB | ✓ | Cellar/dungeon entrance |

#### UI / Systems
| Bundle | Size | Manifest | Content |
|--------|------|----------|---------|
| `loading` | 292 KB | ✓ | Loading screen assets |
| `statements` | 370 KB | ✓ | Achievement/statement UI |
| `tutorial` | 256 KB | ✓ | Tutorial UI |
| `toturial_advance` | 812 KB | ✓ | Advanced tutorial |
| `test` | 734 KB | ✓ | Test/debug content |

#### Room Templates (Directory)
| Path | Description |
|------|-------------|
| `assets/AssetBundles/room/` | Individual room AssetBundles (procedural templates) |
| `assets/AssetBundles/res/` | Resource-only bundles (textures, audio) |

### AssetBundle Format Details
- **Unity Version:** **2018.4.12f1** (bundle header build string; the original "2021.x" claim was wrong)
- **Compression:** LZ4/LZMA as used by the reference build
- **Serialization:** UnityFS (AssetBundle format used by Unity 2018.4)
- **Platform:** Android (ASTC textures, ETC2 fallback)
- **Endianess:** Little-endian (ARM)
- **Metadata version:** IL2CPP metadata version 24

---

## 3. AssetBundle Extraction Strategy

### Tools Required
| Tool | Purpose | Status |
|------|---------|--------|
| `UnityPy` 1.25.4 | Python extraction (verified in this environment) | ✅ Works; tool at `tools/asset/extract_unity.py` |
| `AssetStudio` / UABE | GUI/CLI extraction alternatives | Optional; not required for first pass |
| `il2cppdumper` + `AssetRipper` | Combined IL2CPP + asset extraction | Optional; may help with prefabs/materials |
| Custom Python script | Batch extraction, manifest | ✅ `tools/asset/extract_unity.py` (first pass; conversion not started) |

### Target Native Formats
| Asset Type | Source Format | Native Target | Conversion Tool |
|------------|---------------|---------------|-----------------|
| Textures | ASTC/ETC2 (KTX/PVR) | KTX2 (Vulkan) / PNG (intermediate) | `toktx`, `astcenc` |
| Meshes | Unity Mesh (internal) | glTF 2.0 / custom binary | AssetRipper → glTF |
| Animations | Unity AnimationClip | glTF 2.0 / custom | AssetRipper → glTF |
| Audio | Unity AudioClip (Vorbis/ADPCM) | Ogg Vorbis / WAV | `ffmpeg` / UnityAudioExtractor |
| Shaders | Unity ShaderLab (compiled) | SPIR-V / GLSL | **Manual rewrite** — not portable |
| Prefabs | Unity Prefab (internal) | Custom scene format (JSON/binary) | AssetRipper → custom |
| ScriptableObjects | Unity SerializedObject | JSON / flatbuffers | AssetRipper → JSON |
| Text (TMP) | TextMeshPro font atlas | BMFont / SDF atlas | AssetRipper → BMFont |
| Localization | Unity Localization / CSV | CSV / JSON | Direct export |

### Asset Pipeline Stages (Per Master Spec §26)

```
Reference APK
    ↓
Extraction (UnityPy first pass; AssetRipper optional)
    ↓
Validation (bundle SHA-256 + object path IDs in manifest)
    ↓
Conversion (NOT STARTED: KTX2/OGG/JSON)
    ↓
Native Packaging (NOT STARTED)
```

Only the extraction/manifest stage is implemented. Do not claim a native asset
pipeline until conversion and runtime loading exist.

### Extraction Priority
1. **Sprites/Atlases** — Player, enemies, weapons, UI, particles
2. **Audio** — SFX, music, UI sounds
3. **Room templates** — Procedural generation data
4. **Prefabs** — Entity definitions (player, enemies, objects)
5. **Shaders** — Document only; rewrite natively
6. **Animations** — Frame data, curves
7. **Fonts** — TMP atlas → BMFont/SDF
8. **Localization** — All languages

---

## 4. Other Assets

### `assets/bin/`
- Unity `data.unity3d` — Main data file (may contain global-metadata.dat)
- `settings.json` — Unity player settings

### `assets/DefineCloudSaveID.json`
- Cloud save identifiers for Google Play Games

### `assets/google-services-desktop.json`
- Firebase config placeholder (desktop)

### `assets/sensitive_words.txt`
- Chat filter (empty in this build)

### `assets/Switch/`
- Nintendo Switch specific assets (if any)

---

## 5. Resource Mapping for Native Port

| APK Resource | Native Equivalent | Status |
|--------------|-------------------|--------|
| `AssetBundles/common` | `assets/converted/common/` | ⏳ Pending |
| `AssetBundles/level-objects` | `assets/converted/level-objects/` | ⏳ Pending |
| `AssetBundles/hall` | `assets/converted/scenes/hall/` | ⏳ Pending |
| `AssetBundles/scene_game` | `assets/converted/scenes/game/` | ⏳ Pending |
| `AssetBundles/room/*.bundle` | `data/rooms/` (room templates) | ⏳ Pending |
| `AssetBundles/*.manifest` | Dependency graph (internal) | ⏳ Pending |
| `res/values/strings.xml` | `data/localization/en.json` | ⏳ Pending |
| `res/raw/*.*` | Discard (Android-only) | 🗑️ Remove |

---

## 6. Estimated Asset Counts (from bundle sizes)

| Asset Type | Estimated Count | Notes |
|------------|-----------------|-------|
| Textures (sprites/atlases) | 2,000–5,000 | ASTC compressed |
| Audio clips | 500–1,000 | Vorbis/ADPCM |
| Prefabs | 500–1,500 | Entities, props, VFX |
| Animation clips | 300–800 | Mecanim |
| Shaders | 50–100 | **Rewrite natively** |
| Fonts (TMP) | 5–10 | Atlas + SDF |
| Room templates | 100–300 | Procedural pieces |
| Localization entries | 5,000–10,000 | 10+ languages |

---

## 7. Next Steps

1. **Acquire AssetRipper** — Best all-in-one for IL2CPP + AssetBundle extraction
2. **Extract `global-metadata.dat`** — Check `assets/bin/data.unity3d` or embedded in `libil2cpp.so`
3. **Batch extract all AssetBundles** → `assets/generated/`
4. **Convert to native formats** → `assets/converted/`
5. **Catalog in `data/`** — Room templates, weapon data, enemy data, item data
6. **Build asset pipeline tooling** — `tools/asset-pipeline.py`

---

## 8. Legal / Distribution Note

Per Master Spec §4: **Never commit proprietary extracted assets to Git.**
- `assets/generated/` — `.gitignore` (extracted, intermediate)
- `assets/converted/` — `.gitignore` (converted, intermediate)
- `data/` — **Commit** (data definitions: JSON/CSV for weapons, enemies, items, rooms)
- Only **data definitions** (not textures/audio) go in repo
- Runtime loads converted assets from `$XDG_DATA_HOME/soul-knight/assets/`

---

**Status:** Resource inventory complete. AssetBundle catalog documented. First-pass extraction is implemented (`tools/asset/extract_unity.py`, 284 manifest entries); asset identity and native conversion remain incomplete.