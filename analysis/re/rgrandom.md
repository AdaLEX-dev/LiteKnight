# RGRandom — Evidence-Backed Reconstruction

**Status:** behavioural reconstruction in progress; core algorithm and wrapper semantics reconstructed from ARM64 disassembly.
**Date:** 2026-10-31
**Reference:** Soul Knight 2.6.8, Unity **2018.4.12f1**, IL2CPP metadata **v24**
**Binaries:**
- managed methods: `analysis/apktool_out/lib/arm64-v8a/libil2cpp.so`
- engine RNG: `analysis/apktool_out/lib/arm64-v8a/libunity.so`
- metadata: `/home/jules/bin/dump.cs`, `/home/jules/bin/script.json`

> **Evidence rule:** metadata supplies structure; the disassembly below supplies behaviour. Every claim is labelled.

---

## 1. Question

Reconstruct `RGRandom` before writing a native replacement:

- fields and layout,
- constructor / initialization,
- seed handling,
- state handling,
- `Range` overloads and `RandomInCircle`,
- call sites,
- whether state is global, local, or scoped,
- how map generation and gameplay systems obtain streams,
- whether multiple independent `RGRandom` instances exist.

---

## 2. Structural evidence (metadata)

`dump.cs` (`RGRandom`, TypeDefIndex 7662) declares:

```csharp
public class RGRandom
{
    public RGRandom.RANDOM_STATE seed_state; // 0x10
    public int seed;                         // 0x14
    private Random.State this_state;         // 0x18

    public void SetRandomSeed(int seed);
    public int Range(int min, int max);
    public float Range(float min, float max);
    public Vector3 RandomInCircle(float radius);
    public void .ctor();
}

public enum RGRandom.RANDOM_STATE
{
    C_NOT_READY = 0,
    C_READY = 1,
    S_SEND_SEED = 2,
    GET_SEED = 3
}
```

`UnityEngine.Random.State` is four 32-bit words:

```csharp
public struct Random.State
{
    private int s0; // 0x0
    private int s1; // 0x4
    private int s2; // 0x8
    private int s3; // 0xC
}
```

**Inference from layout:** the object header is 0x10 bytes on IL2CPP; `RGRandom`'s own data occupies 0x10..0x27, so the object is at least 0x28 bytes. This is structural only.

Method RVAs (from `script.json`):

| Method | RVA |
|---|---:|
| `RGRandom..ctor` | `0xDE95C8` |
| `RGRandom.SetRandomSeed` | `0xDE9370` |
| `RGRandom.Range(int,int)` | `0xDD07E8` |
| `RGRandom.Range(float,float)` | `0xDE93DC` |
| `RGRandom.RandomInCircle` | `0xDE94B0` |

Reproduce with:

```bash
tools/re/il2cpp_re.py class RGRandom
tools/re/il2cpp_re.py method RGRandom.Range
tools/re/il2cpp_re.py disasm --address 0xDD07E8 --size 0xE0
```

---

## 3. `RGRandom` behaviour (CONFIRMED from disassembly)

### 3.1 Constructor — `0xDE95C8`

The whole function is:

```text
mov x1, xzr
b   System.Object::.ctor
```

It only calls the base constructor. It does **not** initialize `seed_state`, `seed`, or `this_state`. Those fields therefore have their CLR default values until `SetRandomSeed` or a lazy seed path runs:

```text
seed_state = C_NOT_READY (0)
seed       = 0
this_state = {0,0,0,0}
```

**Confidence:** CONFIRMED.

### 3.2 `SetRandomSeed(int seed)` — `0xDE9370`

Reconstructed pseudocode:

```csharp
void SetRandomSeed(int newSeed)
{
    seed = newSeed;

    var oldGlobal = UnityEngine.Random.state;       // get_state, 0x1AF7EC4
    UnityEngine.Random.InitState(newSeed);          // 0x1AF7E58
    this_state = UnityEngine.Random.state;          // get_state
    UnityEngine.Random.state = oldGlobal;           // set_state, 0x1AF7F58

    seed_state = RANDOM_STATE.GET_SEED;             // 3
}
```

Evidence:

- `str w1, [x20, #0x14]` — stores `seed`.
- `bl 0x1AF7EC4` — `UnityEngine.Random.get_state`.
- `bl 0x1AF7E58` — `UnityEngine.Random.InitState`.
- second `bl 0x1AF7EC4` — captures the newly initialized state into `this_state` at `+0x18`.
- `bl 0x1AF7F58` — restores the previous **global** state.
- `orr w8, wzr, #0x3; str w8, [x20, #0x10]` — sets `seed_state = GET_SEED`.

**Important:** setting an `RGRandom` seed does **not** disturb the global Unity RNG state after the call returns.

**Confidence:** CONFIRMED.

### 3.3 `Range(int min, int max)` — `0xDD07E8`

Reconstructed pseudocode:

```csharp
int Range(int min, int max)
{
    if (seed_state != RANDOM_STATE.GET_SEED)
    {
        // lazy initialization from the global sample seed
        SetRandomSeed(RGGameInfo.Inst.sample_random_seed); // +0x18
    }

    UnityEngine.Random.state = this_state;   // set_state, 0x1AF7F58
    int result = UnityEngine.Random.Range(min, max); // 0x1AF8058 -> 0x1AF805C

    this_state = UnityEngine.Random.state;   // get_state, 0x1AF7EC4
    return result;
}
```

Evidence:

- `ldr w8, [x19, #0x10]; cmp w8, #3; b.eq ...` — checks `seed_state == GET_SEED`.
- If not, obtains `Singleton<RGGameInfo>.Inst` and reads `[x0, #0x18]` (`sample_random_seed`), then calls `SetRandomSeed`.
- `ldp x0, x1, [x19, #0x18]` + `bl 0x1AF7F58` — loads `this_state` and sets global state.
- `bl 0x1AF8058` — `UnityEngine.Random.Range(int,int)`.
- `bl 0x1AF7EC4` — captures the advanced state into `this_state`.

**Important:** unlike `SetRandomSeed`, `Range` does **not** restore the previous global state. After the call, the global Unity RNG state is left equal to this stream's new state. The next `RGRandom` call sets its own state before drawing, so per-instance streams remain independent, but code that samples `UnityEngine.Random` directly can see the last stream's state.

**Confidence:** CONFIRMED.

### 3.4 `Range(float min, float max)` — `0xDE93DC`

Same wrapper structure as the int overload, except the engine call is `UnityEngine.Random.Range(float,float)` at `0x1AF7FE8`.

Pseudocode:

```csharp
float Range(float min, float max)
{
    if (seed_state != RANDOM_STATE.GET_SEED)
        SetRandomSeed(RGGameInfo.Inst.sample_random_seed);

    UnityEngine.Random.state = this_state;
    float result = UnityEngine.Random.Range(min, max);
    this_state = UnityEngine.Random.state;
    return result;
}
```

**Confidence:** CONFIRMED.

### 3.5 `RandomInCircle(float radius)` — `0xDE94B0`

Same wrapper structure, then:

```csharp
Vector3 RandomInCircle(float radius)
{
    if (seed_state != RANDOM_STATE.GET_SEED)
        SetRandomSeed(RGGameInfo.Inst.sample_random_seed);

    UnityEngine.Random.state = this_state;
    Vector2 p = UnityEngine.Random.insideUnitCircle; // 0x1AF8230
    Vector3 result = (Vector3)(p * radius);          // Vector2.op_Multiply
    this_state = UnityEngine.Random.state;
    return result;
}
```

Evidence:

- `bl 0x1AF8230` — `UnityEngine.Random.get_insideUnitCircle`.
- `bl 0x1EA1E58` — `UnityEngine.Vector2.op_Multiply`.
- tail branch to `0x1EA2DF0` — `UnityEngine.Vector2.op_Implicit` to `Vector3`.

**Confidence:** CONFIRMED for the wrapper; engine-side circle algorithm below.

---

## 4. Engine RNG — `libunity.so` (CONFIRMED from disassembly)

The managed `UnityEngine.Random` methods are icall wrappers. Their native implementations are in `libunity.so`. The engine export registration table around `0x37AF78`..`0x37B0xx` maps names to functions:

| Engine method | Native function |
|---|---:|
| `UnityEngine.Random::get_seed` | `0x37AAA0` |
| `UnityEngine.Random::set_seed` | `0x37AAB4` |
| `UnityEngine.Random::InitState` | `0x37AAE8` |
| `UnityEngine.Random::get_state_Injected` | `0x37AB1C` |
| `UnityEngine.Random::set_state_Injected` | `0x37AB38` |
| `UnityEngine.Random::Range` (float) | `0x37AB54` |
| `UnityEngine.Random::RandomRangeInt` | `0x37ABC0` (tail to `0x37B9EC`) |
| `UnityEngine.Random::get_value` | `0x37ABE8` |
| `UnityEngine.Random::GetRandomUnitCircle` | `0x37AD64` |

Reproduce:

```bash
llvm-objdump -d --no-show-raw-insn --start-address=0x37AAA0 --stop-address=0x37AE60 \
  analysis/apktool_out/lib/arm64-v8a/libunity.so
```

### 4.1 State and `InitState` / `set_seed`

The state is `s0,s1,s2,s3` at offsets `0x0,0x4,0x8,0xC`.

`0x37AAB4` (`set_seed`) and `0x37AAE8` (`InitState`) are identical:

```text
s0 = seed
s1 = seed * 0x6C078965 + 1
s2 = s1   * 0x6C078965 + 1
s3 = s2   * 0x6C078965 + 1
```

`0x6C078965` is **1812433253** decimal. All arithmetic is 32-bit wrapping. There is **no warm-up loop**.

Pseudocode:

```csharp
void InitState(int seed)
{
    unchecked
    {
        s0 = (uint)seed;
        s1 = s0 * 1812433253u + 1u;
        s2 = s1 * 1812433253u + 1u;
        s3 = s2 * 1812433253u + 1u;
    }
}
```

**Confidence:** CONFIRMED.

`get_seed` (`0x37AAA0`) reads `s0`. `get_state_Injected` (`0x37AB1C`) copies 16 bytes out; `set_state_Injected` (`0x37AB38`) copies 16 bytes in.

### 4.2 Next state — Xorshift128

`get_value` (`0x37ABE8`) and all Range paths use the same transition:

```text
t = s0 ^ (s0 << 11)
t = t ^ (t >> 8)
r = t ^ s3 ^ (s3 >> 19)
s0 = s1
s1 = s2
s2 = s3
s3 = r
return r
```

This is Xorshift128 with shifts 11, 8, and 19. The compiler folds the sequence into the exact instruction pattern:

```text
eor w8, w8, w8, lsl #11
eor w8, w8, w8, lsr #8
eor w8, w8, w9
eor w8, w8, w9, lsr #19
```

**Confidence:** CONFIRMED.

### 4.3 `Random.value` / float Range

`get_value` at `0x37ABE8` computes:

```text
f = (r & 0x7FFFFF) * (1.0f / 8388608.0f)
```

- mask: `#0x7FFFFF` (23 bits)
- scale at `0xC9D5D8`: `0x34000001` = `1.1920930376163597e-07` = `2^-23`

`Range(float min, float max)` at `0x37AB54` computes:

```text
f = NextFloat()
result = f * max + (1.0f - f) * min
```

This is algebraically `min + f*(max-min)`, but the instruction order is the first form. For bit-exact parity, implement the same operation order, not the simplified form.

**Confidence:** CONFIRMED.

### 4.4 `Range(int min, int max)`

`RandomRangeInt` at `0x37ABC0` tail-branches to `0x37B9EC`. Disassembly gives:

```text
if (min < max) {
    range = (uint32)(max - min);
    r = Next();
    return min + (int32)(r % range);      // udiv/msub, unsigned modulo
}
if (max >= min) {                          // only possible when min == max here
    return min;
}
// min > max
range = (uint32)(min - max);
r = Next();
return min - (int32)(r % range);
```

The modulo is unsigned (`udiv`/`msub` on 32-bit values). The `min > max` branch returns values in `(max, min]` — it includes `min` and excludes `max`. This is the exact reference behaviour even if it differs from the documented `[min,max)` convention.

**Confidence:** CONFIRMED from code path; runtime confirmation still desirable.

### 4.5 `Random.insideUnitCircle`

`GetRandomUnitCircle` at `0x37AD64` advances the RNG twice and computes:

```text
u1 = NextFloat()
angle = (1.0f - u1) * 2π
u2 = NextFloat()
radius = sqrt(1.0f - u2)
x = cos(angle) * radius
y = sin(angle) * radius
```

- `2π` constant at `0xCB1BD0` = `6.2831854820251465`.
- `NextFloat` is the 23-bit value above.
- `sqrtf`, `cosf`, `sinf` are libc calls (`0xB2470`, `0xB25A0`, `0xB2180` in the disassembly).

**Confidence:** CONFIRMED for algorithm and operation order. Exact cross-platform float parity depends on the same libm behaviour; see unresolved issues.

---

## 5. Call-site and stream analysis

All counts below are direct `BL`/`B` xrefs found by `tools/re/il2cpp_re.py xref`. Virtual/interface dispatch is not counted.

| Target | Direct xrefs |
|---|---:|
| `RGRandom.Range(int,int)` | 414 |
| `RGRandom.Range(float,float)` | 276 |
| `RGRandom.SetRandomSeed` | 50 |
| `RGRandom..ctor` | 25 |
| `RGRandom.RandomInCircle` | 4 |

Representative call sites:

- `UILoading.UpdateRandomSeed` (`0xCC3014`) — creates a temporary `RGRandom`, seeds it, and generates the run's map/sample seeds.
- `RandomBehaviour.SetUpRGRandom` (`0xD31E18`) — creates a per-object stream.
- `SerializedRandomBahaviour.SetUpRGRandom` (`0xCF62AC`) — same pattern for Odin-serialized objects.
- `MapManager.SetRGRandomSeed` (`0xD26D8C`) — seeds `MapManager.rg_random` at `+0x58`.
- `MapManagerLevel.SetRGRandomSeed` (`0x103B1D8`) — delegates to base then applies additional map-length logic.
- `RGWeapon.Start` (`0xF88BC0`) — seeds `RGWeapon.rg_random` at `+0x78` with `RGGameInfo.sample_random_seed`.
- `EnemyMaker.CreateEnemy` (`0x136907C`) — draws a seed from the maker stream and seeds the enemy's `RandomBehaviour` stream.
- `SBEnemyMaker.CreateEnemy` (`0xD48BF0`) — same pattern for serialized rooms.
- `RGEController.SetUpRGRandom` (`0x11105F8`) — delegates to `RandomBehaviour.SetUpRGRandom`.
- `TaskDatas.get_rg_random`, `PlayerSaveData.get_general_random_seed`, `NetSyncVar.UpdateSampleRandomSeed`/`UpdateMapRandomSeed`.

### 5.1 Multiple independent streams: YES

`RGRandom` instances are fields on many objects:

| Owner | Field offset | Evidence |
|---|---:|---|
| `RandomBehaviour` | `+0x18` | `dump.cs`; `SetUpRGRandom` |
| `SerializedRandomBahaviour` | `+0x58` | `dump.cs` |
| `MapManager` | `+0x58` | `dump.cs`; `SetRGRandomSeed` |
| `RGWeapon` | `+0x78` | `dump.cs`; `Start` |
| `RGController` | `+0x130` | `dump.cs`; seeded in `RGController.AwakeController` (`0x1177318`, seed = `RGGameInfo.sample_random_seed` + `skillIndex`) |
| `EnemyMaker` | `+0x18` | `dump.cs`; `CreateEnemy` |
| `TaskDatas` | property-created | `get_rg_random` |
| `C15Controller` | field/ctor-created | ctor xref |

Each instance stores its own `this_state`. Sampling sets the global Unity state to that local state, draws, and copies the advanced state back. Streams therefore do not consume each other's sequence. The global state is a shared scratch register, not the source of stream identity.

### 5.2 How map generation obtains streams

`UILoading.UpdateRandomSeed` (`0xCC3014`) single-player path:

1. `playerCount == 0` branch.
2. Create a temporary `RGRandom`.
3. `seed = DateTime.Now.Ticks_low + UnityEngine.Random.Range(int.MinValue, int.MaxValue)`.
4. `temp.SetRandomSeed(seed)`.
5. `map_seed = temp.Range(int.MinValue, int.MaxValue)`; store via `RGGameInfo.SetMapRandomSeed` (`+0x1C`).
6. `sample_seed = temp.Range(int.MinValue, int.MaxValue)`; store via `RGGameInfo.SetSampleRandomSeed` (`+0x18`).

In multiplayer, the same function calls `NetSyncVar.UpdateSampleRandomSeed` / `UpdateMapRandomSeed` instead.

`MapManager.SetRGRandomSeed(seed)` seeds `MapManager.rg_random` at `+0x58`. `MapManagerLevel.SetRGRandomSeed` calls the base implementation and then adds a `Range(0,100)`-based map-length adjustment under specific normal-mode conditions.

The map-seed call path is now resolved. `MapManager.Start` at `0xD26A10`:

```text
ldr w1, [RGGameInfo, #0x1C]       ; map_random_seed
mov x0, MapManager instance
ldp x9, x2, [vtable, #0x1E8]
blr x9                            ; virtual SetRGRandomSeed(seed)
```

So the map manager's virtual `SetRGRandomSeed` receives `RGGameInfo.map_random_seed` directly. For a `MapManagerLevel` instance, that dispatches to `MapManagerLevel.SetRGRandomSeed` (`0x103B1D8`), which calls the base `MapManager.SetRGRandomSeed` (`0xD26D8C`) and then applies the extra map-length logic.

**Confidence:** CONFIRMED.

### 5.3 How gameplay objects obtain streams

`RandomBehaviour.SetUpRGRandom` (`0xD31E18`):

1. Create `RGRandom` (`+0x18`).
2. Single-player or multiplayer branch.
3. Seed a temporary stream from `DateTime.Now` + `UnityEngine.Random.Range(int.MinValue,int.MaxValue)`.
4. Draw `seed = temp.Range(int.MinValue,int.MaxValue)`.
5. Call `this.SetRGRandomSeed(seed)`; `RandomBehaviour.SetRGRandomSeed` seeds `rg_random` only if `seed_state != GET_SEED`, then invokes virtual `OnSetRGRandomSeed(seed)`.
6. In multiplayer, send the seed via `RGGameInfo.SendRandomSeedMessage`.

`EnemyMaker.CreateEnemy`:

1. `seed = rg_random.Range(0, int.MaxValue)`.
2. `enemyRandomBehaviour.SetRGRandomSeed(seed)`.

`RGWeapon.Start`:

1. `rg_random.SetRandomSeed(RGGameInfo.sample_random_seed)`.

**Conclusion:** the reference uses a seed-distribution hierarchy: run seeds → map/room/enemy-maker streams → per-object streams. A native port must preserve both the seed values and the **order** in which each stream is consumed.

---

## 6. Confidence summary

| Claim | Confidence | Evidence |
|---|---|---|
| `RGRandom` layout and method RVAs | CONFIRMED | `dump.cs`, `script.json` |
| `.ctor` leaves fields zero | CONFIRMED | `0xDE95C8` |
| `SetRandomSeed` saves/restores global state and sets `seed_state=3` | CONFIRMED | `0xDE9370` |
| `Range` lazy-seeds from `RGGameInfo.sample_random_seed` | CONFIRMED | `0xDD07E8`, `0xDE93DC`, `0xDE94B0` |
| `Range` leaves global state at the stream's advanced state | CONFIRMED | no restore branch after draw |
| Engine RNG is Xorshift128 after LCG-expanded 4-word seed | CONFIRMED | `0x37AAB4`, `0x37AAE8`, `0x37AB54`, `0x37ABE8`, `0x37B9EC` |
| Float RNG uses 23-bit fraction and `f*max+(1-f)*min` | CONFIRMED | `0x37AB54`, constant `0xC9D5D8` |
| Int RNG uses unsigned modulo and the branch behaviour above | CONFIRMED (static) | `0x37B9EC` |
| `insideUnitCircle` algorithm | CONFIRMED (static) | `0x37AD64` |
| Multiple independent streams | CONFIRMED (structure + ctor xrefs) | fields and `SetUpRGRandom` |
| Map seed virtual call path | CONFIRMED | `MapManager.Start` `0xD26A10` loads `RGGameInfo+0x1C` and calls vtable `+0x1E8` |
| `seed_state` values 1/2 usage | UNRESOLVED (not required for single-player) | no static write of 1/2; only 0 (ctor) and 3 (`SetRandomSeed`) observed |
| MapManagerLevel LongMap branch | CONFIRMED (static) | `0x103B1D8`: Normal mode, this_index 1..15, CompareFactor(10), 50% `Range(0,100)<=49`, increment `map_long` |
| `RandomBehaviour` time/network seed path | HIGH CONFIDENCE | `SetUpRGRandom` `0xD31E18`; `ClientReady` `0xD31CB0` |
| `RGController.rg_random` initialization | CONFIRMED | `AwakeController` at `0x11772A4` loads `+0x130`, reads `RGGameInfo.sample_random_seed` (`+0x18`) and `skillIndex` (`+0x150`), then calls `SetRandomSeed` with their sum |
| Cross-platform float bit-exactness (`sinf/cosf/sqrtf`) | RISK | libm differences possible |

---

## 7. Unresolved questions and next experiments

1. ~~**Map seed call path.**~~ RESOLVED: `MapManager.Start` loads `RGGameInfo.map_random_seed` (`+0x1C`) and calls virtual `SetRGRandomSeed` via vtable `+0x1E8`.
2. **`seed_state` values 1/2.** No static write path to `seed_state` with value 1 or 2 was found; only the constructor default 0 and `SetRandomSeed` value 3 are written. `RandomBehaviour.SetUpRGRandom` creates and seeds `RGRandom` (time-based in the single-player branch; network-seeded in the multiplayer branch), and `SetRGRandomSeed` returns early when `seed_state == 3`. The 1/2 enum values are unused in all observed single-player paths. Exact missing evidence: a runtime/network transition that writes these states. Do not assign semantics without it.
3. ~~**Map/room seed derivation.**~~ RESOLVED: `MapManagerLevel.SetRGRandomSeed` calls base `MapManager.SetRGRandomSeed`, then for `RGGameProcess.game_type == Normal`, `this_index >= 1`, `BattleData.IsNormalMode`, `CompareFactor(10)` (LongMap), and `this_index <= 15`, it draws `Range(0,100)` and increments `map_long` when the result is `<= 49` (50% chance). See `0x103B1D8`.
4. **Reference vectors.** Obtain runtime observations (or a trusted Unity 2018.4 reference) to compare against the reconstructed algorithm. Until then, computed vectors are implementation tests, not parity proof.
5. **Float parity.** The engine calls `sinf`, `cosf`, `sqrtf`; native code must either use the same libm implementation or accept last-bit differences. Document the chosen policy.

---

## 7.1 Seed-state and initialization findings

### `RGRandom.ctor` (`0xDE95C8`)

Only calls `System.Object..ctor`. It leaves:

```text
seed_state = 0   // C_NOT_READY
seed       = 0
this_state = zeroed four-word state
```

### `RGRandom.SetRandomSeed` (`0xDE9370`)

```text
seed = argument
save global Unity RNG state
InitState(seed)              // four-word LCG expansion
this_state = global state
restore global Unity RNG state
seed_state = 3               // GET_SEED
```

### `RandomBehaviour.SetRGRandomSeed` (`0xD32224`)

```text
if (rg_random.seed_state == 3) return;   // already seeded
rg_random.SetRandomSeed(seed);
OnSetRGRandomSeed(seed);                 // virtual
```

### `RandomBehaviour.SetUpRGRandom` (`0xD31E18`)

- Creates a new `RGRandom`.
- In the single-player / no-network branch, derives a seed from
  `DateTime.Now` and `UnityEngine.Random.Range(int.MinValue, int.MaxValue)`:
  the observed sequence is `Random.Range(int.Min, int.Max)`, multiply by the
  DateTime-derived 64-bit value, then pass the low 32 bits to
  `RGRandom.SetRandomSeed`.
- Draws `RGRandom.Range(int.MinValue, int.MaxValue)` after seeding and passes that
  to `SetRGRandomSeed`.
- In the multiplayer branch, `ClientReady` checks `seed_state == 3`; if not ready,
  it obtains the object's net ID and calls
  `RGGameInfo.SendRandomSeedMessage(...)` so the host can distribute a seed.

The exact DateTime conversion (ticks/local time vs UTC) and the network seed
message handling are not fully decoded; they are not required for a single-player
vertical slice, but must be labelled in any implementation.

### `RGController.rg_random`

`RGController.AwakeController` (`0x1177318`) loads `rg_random` (`+0x130`), reads
`RGGameInfo.sample_random_seed` (`+0x18`) and `skillIndex` (`+0x150`), and calls
`RGRandom.SetRandomSeed(sample_random_seed + skillIndex)`. This is separate from
`RandomBehaviour` streams and is confirmed.

## 7.2 Status

```text
CORE ENGINE RNG:
COMPLETE — Unity 2018.4 Xorshift128, 4-word LCG seeding, 23-bit floats,
            integer branches, float operation order, insideUnitCircle.
            Native implementation and regression tests exist.

MAP RNG:
COMPLETE (static, normal single-player) — MapManager.Start -> map_random_seed ->
            SetRGRandomSeed; MapManagerLevel LongMap branch decoded.
            Multiplayer seed distribution and seed_state 1/2 remain unresolved.

REFERENCE VECTORS:
NOT AVAILABLE — tests use reconstructed implementation vectors, not runtime traces.

UNRESOLVED ITEMS:
- seed_state C_READY/S_SEND_SEED (1/2) write paths;
- exact DateTime-derived seed expression;
- multiplayer SendRandomSeedMessage/receive path;
- bit-exact libm sinf/cosf/sqrtf behaviour;
- runtime reference vectors.
```

---

## 8. Native implementation requirements (core implemented; no gameplay use yet)

The core is implemented in `src/engine/math/UnityRandom.*` and covered by
`tests/unit/unity_random_test.cpp`. This section records the requirements it must
continue to satisfy; it is not a fresh implementation plan.

1. Create an explicit `UnityRandomState { uint32 s0,s1,s2,s3; }`.
2. Implement `InitState`, `Next`, `NextFloat`, `Range(int,int)`, `Range(float,float)`, `Value`, `InsideUnitCircle` exactly as above:
   - 32-bit wrapping arithmetic,
   - unsigned modulo,
   - 23-bit mask `0x7FFFFF`,
   - `2^-23` scale,
   - `f*max + (1-f)*min` operation order,
   - Xorshift128 transition order.
3. Create `RGRandomStream` with `seed_state`, `seed`, `this_state` and the wrapper semantics:
   - `SetRandomSeed` saves/restores the global engine state,
   - sampling sets global state and leaves it advanced,
   - lazy seed from `RGGameInfo.sample_random_seed`.
4. Model the global engine RNG as a shared scratch state, not as the per-object stream.
5. Preserve stream ownership and call order:
   - run seed → map/sample seeds,
   - map manager stream,
   - enemy-maker stream → per-enemy streams,
   - weapon streams seeded from `sample_random_seed`.
6. Do not use PCG32 for any parity-sensitive path.
7. Add tests for:
   - known seed → state expansion,
   - first N Xorshift outputs,
   - int Range branch behaviour, including `min == max` and `min > max`,
   - float Range operation order,
   - `insideUnitCircle` radius/angle formula,
   - save/restore semantics of `SetRandomSeed`,
   - lazy-seed behaviour,
   - stream independence when interleaved.

---

## 9. Reproduction commands

```bash
# Metadata and call sites
tools/re/il2cpp_re.py class RGRandom
tools/re/il2cpp_re.py disasm --address 0xDE9370 --size 0x80
tools/re/il2cpp_re.py disasm --address 0xDD07E8 --size 0xE0
tools/re/il2cpp_re.py disasm --address 0xDE93DC --size 0xE0
tools/re/il2cpp_re.py disasm --address 0xDE94B0 --size 0x120
tools/re/il2cpp_re.py xref --address 0xDD07E8
tools/re/il2cpp_re.py xref --address 0xDE9370

# Engine implementation
llvm-objdump -d --no-show-raw-insn --start-address=0x37AAA0 --stop-address=0x37AE60 \
  analysis/apktool_out/lib/arm64-v8a/libunity.so
llvm-objdump -d --no-show-raw-insn --start-address=0x37B9EC --stop-address=0x37BA70 \
  analysis/apktool_out/lib/arm64-v8a/libunity.so
```

---

**Bottom line:** `RGRandom` is a local-state wrapper around Unity 2018.4's Xorshift128 RNG. It is not PCG32, and the global Unity RNG is only a shared scratch state. `UnityRandomState` + `RGRandomStream` are implemented in `src/engine/math/UnityRandom.*` and covered by `tests/unit/unity_random_test.cpp` (55 cases / 1838 assertions pass overall). They must not be used by gameplay until the remaining `seed_state` 1/2 / DateTime seed questions are resolved or explicitly accepted as gaps.
