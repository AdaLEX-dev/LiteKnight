# ADR-0016: Reference-Compatible RNG

## Status

Accepted — 2026-10-31. Supersedes ADR-0014 (PCG32).

## Context

`RGRandom` was assumed to be a custom deterministic RNG that could be replaced
with PCG32. Disassembly of the reference shows otherwise:

- `RGRandom.Range` at `0xDD07E8` saves/restores `UnityEngine.Random.state`,
  calls `UnityEngine.Random.Range` at `0x1AF8058`, and stores the advanced state.
- The engine implementation in `libunity.so` is Xorshift128 with a four-word
  LCG-expanded seed (`0x37AAA0`..`0x37BA70`).
- `RGRandom` instances are per-object streams that borrow the global Unity RNG
  as a scratch state.

Evidence: `analysis/re/rgrandom.md`.

## Decision

Gameplay RNG must be reference-compatible:

- implement Unity 2018.4 `Random` state transition and seeding,
- implement the `RGRandom` wrapper semantics (local state, global scratch state,
  lazy seed from `RGGameInfo.sample_random_seed`),
- do not use PCG32 for parity-sensitive systems.

PCG32 remains only as a non-parity utility (e.g., debug/editor sampling).

## Consequences

- `src/engine/math/UnityRandom.*` is the gameplay RNG.
- Stream ownership and call order must be preserved (run seeds → map/sample
  seeds → object streams).
- Exact float parity depends on the same `sinf`/`cosf`/`sqrtf` behaviour; that
  risk is documented in the evidence file.
- Tests use reconstructed vectors until a runtime reference trace is available.

## References

- `analysis/re/rgrandom.md`
- `src/engine/math/UnityRandom.hpp`
- `tests/unit/unity_random_test.cpp`
