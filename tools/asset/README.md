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
```

Outputs go to `assets/generated/` (gitignored). The manifest is written to
`analysis/assets/extraction_manifest.json` (committed). The tool never commits
proprietary asset bytes.

The local venv and pinned dependency are in `requirements.txt`.
