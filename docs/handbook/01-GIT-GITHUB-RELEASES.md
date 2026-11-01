# 01 — Git, GitHub, CI, and Beta Releases

## 1. Repository

Canonical remote:

```text
https://github.com/AdaLEX-dev/LiteKnight.git
```

Default branch: `main`.

The working tree on the current development machine is usually ahead of the
remote because releases are cut in batches. Always check:

```bash
git status -sb
git log --oneline --decorate -10
git remote -v
```

Do not assume `origin/main` is current until you have fetched.

## 2. Branch model

The project uses a simple mainline model:

- `main` is the only long-lived branch.
- Feature/fix branches are optional for isolated work.
- Release tags point at commits on `main`.
- Experimental code that might not survive is kept in a branch or not committed
  at all; do not leave dead half-features in `main`.

Recommended branch names:

```text
feat/living-room-coordinate-pass
fix/hub-chest-persistence
docs/successor-handbook
```

## 3. Commit messages

Use conventional prefixes, lowercase subject, imperative mood, and a short
scope:

```text
feat(hub): restore task board and blueprints
fix(weapons): keep weapon anchored while walking
fix(save): persist materials and trader stock
test(coord): add strict Living Room coordinate regression
docs(handbook): add successor operating manual
chore(release): prepare v0.6.0-beta.1
```

The subject must describe the user-visible or architectural outcome, not the
mechanical edit. Examples from this repository:

```text
fix(hub,rooms): seal Living Room and restore depth, collision and dungeon flow
```

A body is required for non-trivial changes. State:

- what was broken;
- the recovered evidence or coordinate that drives the fix;
- what changed in the source of truth;
- what tests were run;
- any remaining visual/reference gap.

Example:

```text
fix(hub): make the Living Room weapon chest a real random chest

The static chest prefab was removed and replaced at runtime with a
hub_weapon_chest item-world chest. chest00's appointed weapon_021 was
replaced by the recovered random RGChest rule (weapon_drop_level = -2),
otherwise every new run produced the same free weapon.

Tests: item_test multi-seed loot, save/restore chest state, coordinate suite,
gameplay smoke.
```

## 4. Staging and reviewing

```bash
git status --short
git diff --stat
git diff -- src/app/Application.cpp
git add <paths>
git diff --cached --stat
git diff --cached
git commit
```

Golden rules:

- Do not commit `original/`, `analysis/apktool_out/`, generated reference PNGs,
  logs, saves, or build outputs.
- Do not commit `build/`, `build-debug/`, `build-release/`, or test artifacts.
- Do not commit local save slots under `~/.local/share/soul-knight`.
- Generated metadata under `analysis/assets/` and generated scene JSON under
  `analysis/assets/scenes/` **is** committed; generated pixels are not.
- If a large metadata file changed only because a builder was rerun, confirm the
  change is meaningful before staging it.
- Never stage a binary or reference asset to “make CI pass”.

Useful ignore checks:

```bash
git check-ignore -v original/SoulKnight-2.6.8.apk
git check-ignore -v assets/generated/visual/hero_room/textures/hub_tex_1.png
```

## 5. Syncing with the remote

```bash
git fetch origin
git status -sb
git log --oneline --left-right --graph main...origin/main
```

If the local branch is behind:

```bash
git pull --rebase origin main
```

Do not rebase a branch that has already been pushed and reviewed by someone else
unless the team agrees. For this project, mainline commits are usually local
until a release, then pushed as a fast-forward.

## 6. CI

Workflow: `.github/workflows/ci.yml`.

It runs on Arch Linux in a container and:

1. installs CMake/Ninja/GCC/SDL3/Vulkan/libpng/fmt;
2. configures `RelWithDebInfo`;
3. builds;
4. runs CTest;
5. runs `tools/asset/audit_assets.py`;
6. verifies `build/bin/liteknight-devroom` exists.

CI does **not** have the proprietary reference tree and does not generate
reference pixels. Tests must not require the APK to be present. Use committed
metadata fixtures or skip cleanly when local reference data is absent.

Before pushing:

```bash
ctest --test-dir build --output-on-failure
python3 tools/asset/audit_assets.py
```

If CI fails only in a reference-dependent test, the test is wrong for CI. Make it
skip with an explicit message or move it to a local QA target.

## 7. Tags and versions

`VERSION` contains the human version. `CHANGELOG.md` contains the release
history. `docs/releases/<version>.md` contains the release note.

The current beta is:

```text
0.6.0-beta.1
v0.6.0-beta.1
```

Version rules used by this project:

- `x.y.z-dev` — unreleased development snapshot.
- `x.y.z-beta.N` — feature-complete enough for testing, not production.
- `x.y.z` — tagged release that passes the release gate.

The project is not yet at a 1.0 production release. Do not label a beta as
“latest stable” in GitHub. Use pre-release.

## 8. Creating a beta release

Full procedure:

```bash
# 1. Ensure you are on main and clean
git switch main
git status

# 2. Build and test the exact tree
./scripts/prepare-assets.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
./build/soul-knight-test
python3 tools/qa/living_room_coordinate_regression.py
python3 tools/qa/trader_stock_regression.py
ctest --test-dir build --output-on-failure

# 3. Update VERSION and CHANGELOG
#    Add docs/releases/v0.6.0-beta.1.md

# 4. Re-run the smoke that exercises the release binary
./build/soul-knight --gameplay-smoke --smoke-seconds 25

# 5. Commit the release
git add VERSION CHANGELOG.md docs/releases/v0.6.0-beta.1.md
git commit -m "chore(release): prepare v0.6.0-beta.1"

# 6. Annotated tag
git tag -a v0.6.0-beta.1 -m "LiteKnight v0.6.0-beta.1"

# 7. Push both
git push origin main
git push origin v0.6.0-beta.1
```

Then create the GitHub release:

```bash
gh release create v0.6.0-beta.1 \
  --title "LiteKnight v0.6.0-beta.1 — coordinate and systems beta" \
  --notes-file docs/releases/v0.6.0-beta.1.md \
  --prerelease
```

If the release already exists:

```bash
gh release edit v0.6.0-beta.1 --prerelease \
  --title "..." --notes-file docs/releases/v0.6.0-beta.1.md
```

Verify:

```bash
gh release view v0.6.0-beta.1
git ls-remote --tags origin | tail
```

## 9. GitHub authentication

The local `gh` account is authenticated as `NagyHunorMate` with `repo`,
`workflow`, `gist`, and `read:org` scopes. The git remote is HTTPS. If `gh` is
not authenticated:

```bash
gh auth login
gh auth status
```

If push fails with a credential error, use the GitHub CLI credential helper or a
personal access token with `repo` scope. Never commit a token.

## 10. Release content rules

A public release may include:

- native source;
- build instructions;
- committed metadata and scene JSON;
- tests and QA scripts;
- documentation;
- an executable built from the release commit, if you choose to attach one.

A public release must **not** include:

- `original/SoulKnight-2.6.8.apk`;
- `analysis/apktool_out/`;
- extracted reference textures/audio;
- `assets/generated/` reference pixels;
- save files containing local user data;
- a `.local/share` asset bundle.

The release note must state that reference assets are not included and that the
user must supply their own local reference for the asset-backed mode. See
`LEGAL.md`, `docs/legal/clean-room-boundary.md`, and
`docs/legal/reference-assets.md`.

## 11. If a release is wrong

Do not delete a published tag silently. Prefer:

```bash
gh release delete vX.Y.Z-beta.N --yes
git tag -d vX.Y.Z-beta.N
git push origin :refs/tags/vX.Y.Z-beta.N
```

Then cut a new beta with an incremented number. If the tag is already consumed by
users, document the mistake in `CHANGELOG.md` and publish a follow-up. Never move
a tag that other people may have fetched.

## 12. Current release status note

The tree has many local commits ahead of `origin/main` because the project was
developed in a long session. A beta release should push the full local mainline,
not just the release commit. The expected release sequence is:

```bash
git push origin main
git push origin v0.6.0-beta.1
gh release create ...
```

If the remote rejects the push, fetch and inspect; do not force-push `main`
without explicit owner approval.
