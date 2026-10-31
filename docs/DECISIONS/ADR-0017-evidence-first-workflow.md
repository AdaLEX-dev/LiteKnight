# ADR-0017: Evidence-First Reverse-Engineering Workflow

## Status

Accepted — 2026-10-31.

## Context

The first project phase produced extensive documents and a native skeleton before
behaviour was reconstructed. Metadata was treated as behavioural evidence, and
inferred fields/mechanics were labelled as architecture. Build and test results
were falsely green.

Diagnosis: `analysis/PROJECT_DIAGNOSIS.md`. Evidence audit:
`analysis/EVIDENCE_AUDIT.md`.

## Decision

All future subsystem work follows:

```text
method name
  → metadata entry / RVA
  → native function
  → disassembly
  → behavioural reconstruction
  → evidence document
  → native implementation
  → tests
```

Rules:

1. Metadata is structural evidence, not behavioural evidence.
2. A class or field name never establishes a mechanic.
3. Disassembly or runtime observation is required for behaviour.
4. Inferred behaviour is labelled `HYPOTHESIS`/`UNKNOWN`, never `CONFIRMED`.
5. Every reconstructed subsystem gets `analysis/re/<subsystem>.md`.
6. Every implemented subsystem gets a test that can fail.
7. Gate B/C status is based on evidence, not document volume.
8. The vertical slice is evidence-gated (`docs/VERTICAL_SLICE.md`).

Tooling: `tools/re/il2cpp_re.py` (metadata/RVA/disassembly/xrefs), optional Ghidra
annotation, and `tools/asset/` for local asset extraction.

## Consequences

- Implementation speed is intentionally limited by evidence quality.
- A 500-line verified subsystem is preferred over a large speculative port.
- The PCG32 decision and speculative architecture claims were superseded.
- Runtime observation remains desirable; static analysis is the current
  substitute until an approved native/observation method exists. Android
  runtimes, Waydroid, emulators, Wine, and X11/XWayland remain forbidden.
