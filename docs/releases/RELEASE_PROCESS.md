# Release Process

A release tag means the exact commit builds and launches the playable native
devroom under the documented requirements.

## Procedure

```text
1. Update VERSION
2. Update CHANGELOG.md [Unreleased] -> [version]
3. Add docs/releases/<version>.md
4. ./scripts/dev.sh --no-build   (or full build/test/manual smoke)
5. Run Debug ASan/UBSan tests
6. Run RelWithDebInfo tests
7. Run Vulkan validation smoke test
8. Run the Wayland/Vulkan devroom manual smoke
9. Commit
10. Tag annotated: git tag -a v<version> -m "LiteKnight <version>"
11. Push commit and tag to the canonical remote
12. Create GitHub Release from the tag
```

Do not tag a known-broken commit. Do not publish reference-derived/local-only
assets. Public release packages must contain only `allowed` assets or a separate
properly licensed asset pack.

## Repository

Canonical remote:

```text
https://github.com/AdaLEX-dev/LiteKnight.git
```

## CI

`.github/workflows/ci.yml` builds and tests source/metadata only in an Arch Linux
container. It does not use the proprietary reference tree.
