# Development Workflow

## Daily loop

```bash
git pull
./scripts/dev.sh
```

`dev.sh` builds, tests, prepares assets, and launches. It refuses to launch when
tests fail.

## When changing code

1. Recover/record evidence first when behaviour is reference-derived.
2. Implement the smallest complete native slice.
3. Add deterministic tests.
4. Run:

   ```bash
   ./scripts/test.sh
   ./scripts/smoke-devroom.sh
   ```

5. Run manual devroom checks.
6. Update changelog, evidence, inventory and docs.
7. Commit a focused Conventional Commit.

## Debugging

```bash
LITEKNIGHT_BUILD_DIR=build-debug ./scripts/build-dev.sh
LITEKNIGHT_BUILD_DIR=build-debug ./scripts/test.sh
LITEKNIGHT_BUILD_DIR=build-debug ./scripts/run-devroom.sh
```

Use `--locomotion-debug` for fixed-step/player/enemy/projectile logs.

## Asset changes

```bash
tools/asset/pipeline.py scene
tools/asset/pipeline.py inventory
tools/asset/pipeline.py audit
# local only:
tools/asset/pipeline.py check-local
```

See `docs/assets/PIPELINE.md`.

## Rules

- Never commit reference binaries or proprietary pixels.
- Never silently replace a recovered texture with programmer art.
- Mark placeholders `PLACEHOLDER` and record them in the inventory.
- Keep the tested executable playable.
- Keep metadata/provenance for every generated asset.
