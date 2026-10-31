# Asset Extraction

```bash
# First reproducible pass (creates a local .venv on first use)
tools/asset/extract.sh extract --profile first-pass

# Inspect a bundle without exporting
tools/asset/extract.sh list --bundle common --type Sprite --limit 20

# Ad-hoc extraction
tools/asset/extract.sh extract \
  --bundle res/charactor_common \
  --type Sprite \
  --name-regex '^enemy01_' \
  --out assets/generated/enemy01

# Controlled Texture2D -> deterministic PNG + RGBA8 + provenance
tools/asset/extract.sh convert-texture \
  --bundle level-objects \
  --path-id -8989119030688471403 \
  --name f903

# Controlled r1_1 + c00 visual scene metadata and runtime artifacts
tools/asset/.venv/bin/python tools/asset/build_visual_scene.py
```

Outputs go to `assets/generated/` (gitignored). The extraction manifest is
`analysis/assets/extraction_manifest.json`; the conversion provenance is
`analysis/assets/texture_conversion_manifest.json`; the visual scene metadata is
`analysis/assets/scenes/r1_1_visual.json`. The tool never commits proprietary
asset bytes.

The local venv and pinned dependencies (UnityPy 1.25.4, Pillow 13.0.0) are in
`requirements.txt`. The converter accepts only RGBA32/RGB24 single-mip textures,
writes a deterministic PNG and raw RGBA8 dump under gitignored
`assets/generated/texture-conversion/`, and records provenance/hashes in
`analysis/assets/texture_conversion_manifest.json`. The scene builder puts
runtime PNG/scene bytes under gitignored `assets/generated/visual/r1_1_visual/`.
