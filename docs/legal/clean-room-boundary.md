# Clean-Room / Interoperability Boundary

LiteKnight is a native reimplementation informed by static analysis of a
locally obtained reference build and by serialized metadata. It is not a
copy of the reference source tree.

## Native code

The runtime, renderer, input loop, fixed-step simulation, combat model, asset
loader, and developer UI are written natively in C++ for Linux. They do not link
against Unity, Android, the APK, or ChillyRoom binaries.

## Behaviour evidence

The project records method addresses, field offsets, formulas, serialized
values, and confidence labels in `analysis/`. These are evidence notes, not
copied proprietary source or assets.

## Assets

Recovered textures/sprites are converted locally for development testing. Their
inclusion in a local generated build does not make them redistributable.
Project-authored UI code uses a small generated placeholder text atlas until a
properly licensed font/UI solution is selected.

## Boundaries

- No claim of official status or endorsement.
- No claim of ownership over Soul Knight.
- No promise that every reconstructed behaviour is complete or parity-perfect.
- No redistribution of reference binaries or proprietary pixels in this
  repository.
