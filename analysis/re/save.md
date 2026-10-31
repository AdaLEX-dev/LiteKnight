# Save / Runtime State Boundary

**Status:** format UNKNOWN; first slice does not require save compatibility.

---

## 1. Confirmed evidence

`RGSaveManager` (`dump.cs` TypeDefIndex 7353) fields:

```text
RGCharacterInfo[] char_list;   // +0x18
RGPetInfo[] pet_list;          // +0x20
RGSpriteList[] hero_skin_list; // +0x28
AboGameData game_data;         // +0x30
Sprite gems_icon;              // +0x38
int[] c_levelup_gems;          // +0x40
string store_key;              // +0x48
```

Methods:

- `Init` `0xF761AC`
- `UpdateVersion` `0xF770F8`
- `OnLoad` `0xF79B14`
- `Start` `0xF79C04`
- `Update` `0xF79C7C`
- `InitSave` `0xF769A4`
- `Save` `0xF79CF4`
- `Load` `0xF78D14`
- `LoadLastPetIndex` `0xF7A1E8`

`PlayerSaveData` (`Singleton<PlayerSaveData>`) exposes many properties such as
`difficulty`, `last_index`, `fail_time`, `sample_random_seed`, `coin_value`,
`gems`, `has_save_date`, `has_reborn`, `has_reborn_twice`, `reborn_card`, etc.

`PlayerSaveData.SetString` (`0xEE5C44`) tail-calls
`UnityEngine.PlayerPrefs.SetString`; `SetInt` (`0xEE5A98`) tail-calls
`PlayerPrefs.SetInt`; the corresponding getters call `PlayerPrefs.GetString/GetInt`.
Therefore the reference uses **Unity PlayerPrefs keys** for at least part of its
persistent state. The `AboGameData` payload/serialization is not decoded.

`RGSaveManager.Save` (`0xF79CF4`) and `Load` (`0xF78D14`) use
`PlayerSaveData.SetString/SetInt` and `PlayerSaveData.get_*`; `Init` calls
`LoadGameData` (`0xF767FC`) and `InitSave`. Exact key names and payload encoding are
not recovered.

## 2. What this means for the first slice

- **Not required** for the first vertical slice: the slice can use transient runtime
  state (`BattleData` equivalent) and a native reset/restart.
- Do **not** claim save compatibility or a reference save format.
- Do **not** invent `AboGameData`/Sirenix/Odin fields or a JSON schema as reference.
- If a native save is added later, it must be a new documented format; reference
  compatibility requires disassembling `RGSaveManager.Save/Load` and
  `AboGameData` first.

## 3. Confidence

| Claim | Confidence | Evidence |
|---|---|---|
| PlayerSaveData uses PlayerPrefs string/int keys | CONFIRMED | `0xEE5C44`, `0xEE5A98` |
| RGSaveManager calls PlayerSaveData save/load | HIGH CONFIDENCE | `0xF79CF4`, `0xF78D14` |
| Exact `AboGameData` serialization | UNKNOWN | not decoded |
| Save required for first slice | NO | runtime state can be transient |

## 4. Missing evidence

- exact PlayerPrefs key names,
- `AboGameData` field layout/serialization and version migration,
- `RGGameInfo`/`BattleData` persistent boundary,
- cloud-save interaction (out of scope),
- `store_key` use.
