# LiteKnight Legal Boundary

LiteKnight is a native Linux development project that reconstructs behaviour
from a locally obtained reference build for interoperability, testing, and
research. It is not an official Soul Knight product and is not affiliated with
ChillyRoom.

## Native source code

The source code authored for LiteKnight is licensed under the MIT license in
`LICENSE`. This covers native C++ source, build scripts, tests, documentation,
and project-authored metadata unless a file states otherwise.

## Reference material

The following are **not** covered by the MIT license and are not distributed by
this repository:

- the Soul Knight APK or other reference binaries;
- Unity bundles, AssetBundles, serialized assets, decompiled code, or dump files;
- extracted textures, sprites, animations, audio, fonts, UI art, or other
  reference-derived content;
- Soul Knight names, logos, characters, and trademarks.

The repository may contain metadata derived from the local reference (asset IDs,
hashes, dimensions, sprite rects, pivot values, animation timing, and similar
facts). Metadata is kept for reproducibility and evidence. It does not grant a
license to the underlying artwork.

## Distribution status

The asset inventory records one of:

- `allowed` — project-authored or explicitly licensed for release;
- `restricted` — may be used only under a stated condition;
- `unknown` — license not established;
- `local_only` — reference-derived material that must not be redistributed.

Reference-derived pixels are `local_only` by default. A build generated from a
local reference tree is a local development build, not a public release asset
pack.

## No affiliation or endorsement

Do not describe LiteKnight as official, endorsed, authorized, or sponsored by
ChillyRoom. Do not use ChillyRoom trademarks as LiteKnight branding.

## Contributor rule

Do not commit proprietary reference binaries or extracted pixels/audio. Do not
change an asset's redistribution classification without documenting the basis.
