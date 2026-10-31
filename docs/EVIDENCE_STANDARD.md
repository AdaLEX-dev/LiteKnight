# Evidence Standard

**Effective:** 2026-10-31  
**Applies to:** every reverse-engineering note, architecture claim, parity row, and native implementation.

---

## Evidence classes

| Label | Meaning | Examples |
|---|---|---|
| `CONFIRMED` | Directly observed in the artifact or a reproducible experiment | APK hash; metadata version; disassembled instruction sequence; runtime trace |
| `HIGH CONFIDENCE` | Multiple independent evidence sources agree, but one gap remains | Method body plus call-site agreement, with one unobserved branch |
| `HYPOTHESIS` | Plausible explanation with a named experiment to confirm/reject it | "Armor subtracts before HP" inferred from field names |
| `UNKNOWN` | Not investigated or insufficient evidence | No method body or runtime observation |
| `CONTRADICTED` | Existing claim disproven by evidence | "Unity 2021.x" when the binary says 2018.4.12f1 |

## What counts as evidence

1. **Artifact metadata** — class/field/method names, offsets, signatures, RVAs. This is **structural** evidence only. It can confirm that a thing exists, not what it does.
2. **Disassembly/decompilation** — actual instructions, control flow, constants, call graph. This is **behavioural** evidence.
3. **Runtime observation** — inputs, outputs, state transitions, logs, traces. This is **behavioural** evidence.
4. **Reproducible tests** — a test that exercises reconstructed behaviour and can fail.

## What does not count as evidence

- a class name,
- a field name,
- a plausible architecture pattern,
- a document written before the method body was inspected,
- a test that is disabled or selects zero test cases.

## Required record for a reconstructed subsystem

Create `analysis/re/<subsystem>.md` containing:

1. question and scope,
2. metadata entries (class, fields, methods, RVAs),
3. exact binary addresses and disassembly excerpts or summaries,
4. reconstructed pseudocode,
5. call-site analysis,
6. confidence per claim,
7. unresolved questions and the next experiment,
8. native implementation requirements and test plan.

## Rule for native code

Do not implement gameplay from a `HYPOTHESIS` unless the implementation is explicitly isolated behind a named experiment and cannot be mistaken for parity. Every implemented subsystem must link to its evidence document and have a test that can fail.

## Rule for tests

A test claim is valid only when:

- `--list-test-cases` shows the intended cases,
- CTest selects at least one case,
- an intentional failure makes CTest red,
- the restored suite passes.

## Rule for documentation

When correcting an old claim, keep the old claim visible in a dated correction block. Do not silently rewrite history. Mark it `SUPERSEDED` or `CONTRADICTED`, link the evidence, and state the corrected fact.

## Current reference baseline

- Game: Soul Knight 2.6.8
- APK SHA-256: `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`
- Engine: Unity **2018.4.12f1**
- IL2CPP metadata: **version 24**
- Native libraries: arm64-v8a and armeabi-v7a only
- Full metadata dump: `/home/jules/bin/dump.cs`, `script.json`, `il2cpp.h`, `DummyDll/`
- In-repo partial dump: `analysis/il2cpp_dump/` (not authoritative)
