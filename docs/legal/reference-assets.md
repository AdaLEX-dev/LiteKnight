# Reference Asset Policy

## Source-controlled

- Native C++ source and shaders.
- Build scripts, tests, CI configuration.
- Metadata facts needed to reproduce the native runtime: asset path IDs, hashes,
  dimensions, sprite rectangles, pivots, animation key timings, scene topology.
- Project-authored documentation and generated placeholder developer font code.

## Generated locally and gitignored

- `assets/generated/` PNG/RGBA8 pixels and runtime scene bytes.
- `analysis/apktool_out/`, `analysis/jadx_out/`, and `analysis/il2cpp_dump/`.
- `original/*.apk` and other reference binaries.
- Python virtual environments and tool caches.

## Redistribution classes

| Class | Meaning | Release packages |
|---|---|---|
| `allowed` | project-authored or explicitly licensed | may include |
| `restricted` | only under stated condition | review required |
| `unknown` | license not established | exclude by default |
| `local_only` | reference-derived proprietary material | never include |

The texture inventory records `local_only` for recovered textures. The
developer executable may consume them from a local reference tree. A public
release must use a separate redistributable asset pack or an explicit,
documented asset license.
