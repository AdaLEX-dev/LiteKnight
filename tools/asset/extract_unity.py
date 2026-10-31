#!/usr/bin/env python3
"""Reproducible UnityPy extraction for Soul Knight 2.6.8.

The tool exports only local generated assets; proprietary assets must never be
committed. The manifest (analysis/assets/extraction_manifest.json) is safe to
commit because it records source bundle hashes, object names/types, and output
paths, not the asset bytes.

Examples:
    tools/asset/extract_unity.py list --bundle common --type Sprite --limit 20
    tools/asset/extract_unity.py extract --profile first-pass
    tools/asset/extract_unity.py extract --bundle room/r1_1 --type Sprite \
        --out assets/generated/room-r1-1

Requires UnityPy: pip install UnityPy
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

try:
    import UnityPy
except ImportError as exc:  # pragma: no cover - user environment issue
    raise SystemExit(
        "UnityPy is required. Install it with: pip install UnityPy\n" + str(exc)
    )

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUNDLE_ROOT = ROOT / "analysis/apktool_out/assets/AssetBundles"
DEFAULT_OUT = ROOT / "assets/generated/first-pass"
DEFAULT_MANIFEST = ROOT / "analysis/assets/extraction_manifest.json"


@dataclass
class Spec:
    bundle: str
    profile: str
    types: tuple[str, ...] = ()
    name_regex: str | None = None
    gameobjects: tuple[str, ...] = ()
    limit: int | None = None
    note: str = ""


@dataclass
class Manifest:
    tool: str = "tools/asset/extract_unity.py"
    unity_support: str = "Unity 2018.4.12f1 (reference bundle build string)"
    entries: list[dict] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def add(self, entry: dict) -> None:
        self.entries.append(entry)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def safe_name(name: str) -> str:
    name = name.strip() or "unnamed"
    name = re.sub(r"[\\/:*?\"<>|]+", "_", name)
    name = re.sub(r"\s+", "_", name)
    return name[:160]


def bundle_path(bundle: str, root: Path) -> Path:
    p = Path(bundle)
    if p.is_absolute():
        return p
    return root / bundle


def source_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def object_name(obj) -> str:
    try:
        data = obj.read()
        name = getattr(data, "m_Name", "") or getattr(data, "name", "")
        return str(name)
    except Exception:
        return ""


def unique_path(path: Path, path_id: int) -> Path:
    if not path.exists():
        return path
    return path.with_name(f"{path.stem}_{path_id & 0xFFFFFFFF:x}{path.suffix}")


def write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def export_sprite(obj, out_dir: Path, manifest: Manifest, spec: Spec, bundle: str, digest: str) -> None:
    data = obj.read()
    name = getattr(data, "m_Name", "") or f"sprite_{obj.path_id}"
    image = getattr(data, "image", None)
    if image is None:
        manifest.warn(f"{bundle}: sprite {name} has no image (external dependency?)")
        return
    target = unique_path(out_dir / "sprites" / f"{safe_name(name)}.png", obj.path_id)
    target.parent.mkdir(parents=True, exist_ok=True)
    image.save(target)
    manifest.add({
        "profile": spec.profile,
        "source_bundle": bundle,
        "source_sha256": digest,
        "path_id": str(obj.path_id),
        "unity_type": "Sprite",
        "name": name,
        "exported_path": str(target.relative_to(ROOT)),
        "status": "exported",
        "note": spec.note,
    })


def export_texture(obj, out_dir: Path, manifest: Manifest, spec: Spec, bundle: str, digest: str) -> None:
    data = obj.read()
    name = getattr(data, "m_Name", "") or f"texture_{obj.path_id}"
    image = getattr(data, "image", None)
    if image is None:
        manifest.warn(f"{bundle}: texture {name} has no image")
        return
    target = unique_path(out_dir / "textures" / f"{safe_name(name)}.png", obj.path_id)
    target.parent.mkdir(parents=True, exist_ok=True)
    image.save(target)
    manifest.add({
        "profile": spec.profile,
        "source_bundle": bundle,
        "source_sha256": digest,
        "path_id": str(obj.path_id),
        "unity_type": "Texture2D",
        "name": name,
        "exported_path": str(target.relative_to(ROOT)),
        "status": "exported",
        "note": spec.note,
    })


def export_audio(obj, out_dir: Path, manifest: Manifest, spec: Spec, bundle: str, digest: str) -> None:
    data = obj.read()
    name = getattr(data, "m_Name", "") or f"audio_{obj.path_id}"
    samples = getattr(data, "samples", None)
    if not samples:
        manifest.warn(f"{bundle}: audio {name} has no decodable samples")
        return
    for sample_name, sample_bytes in samples.items():
        ext = Path(sample_name).suffix or ".wav"
        target = unique_path(
            out_dir / "audio" / f"{safe_name(name)}_{safe_name(Path(sample_name).stem)}{ext}",
            obj.path_id,
        )
        write_bytes(target, sample_bytes)
        manifest.add({
            "profile": spec.profile,
            "source_bundle": bundle,
            "source_sha256": digest,
            "path_id": str(obj.path_id),
            "unity_type": "AudioClip",
            "name": name,
            "exported_path": str(target.relative_to(ROOT)),
            "status": "exported",
            "note": spec.note,
        })


def export_text_asset(obj, out_dir: Path, manifest: Manifest, spec: Spec, bundle: str, digest: str) -> None:
    data = obj.read()
    name = getattr(data, "m_Name", "") or f"text_{obj.path_id}"
    script = getattr(data, "m_Script", None)
    if script is None:
        script = getattr(data, "text", None)
    if isinstance(script, str):
        payload = script.encode("utf-8")
    elif isinstance(script, (bytes, bytearray)):
        payload = bytes(script)
    else:
        manifest.warn(f"{bundle}: TextAsset {name} has unsupported payload")
        return
    target = unique_path(out_dir / "text" / f"{safe_name(name)}.txt", obj.path_id)
    write_bytes(target, payload)
    manifest.add({
        "profile": spec.profile,
        "source_bundle": bundle,
        "source_sha256": digest,
        "path_id": str(obj.path_id),
        "unity_type": "TextAsset",
        "name": name,
        "exported_path": str(target.relative_to(ROOT)),
        "status": "exported",
        "note": spec.note,
    })


def gameobject_tree(go_reader, depth: int = 0, seen: set[int] | None = None) -> dict:
    if seen is None:
        seen = set()
    data = go_reader.read()
    name = getattr(data, "m_Name", "")
    node = {
        "name": name,
        "path_id": str(go_reader.path_id),
        "components": [],
        "children": [],
    }
    for component in getattr(data, "m_Component", []):
        try:
            node["components"].append(component.component.type.name)
        except Exception:
            node["components"].append("unknown")
        if component.component.type.name == "Transform" and depth < 32:
            transform = component.component.read()
            for child in getattr(transform, "m_Children", []):
                try:
                    child_go = child.read().m_GameObject
                    if child_go.path_id in seen:
                        continue
                    seen.add(child_go.path_id)
                    node["children"].append(gameobject_tree(child_go, depth + 1, seen))
                except Exception:
                    node["children"].append({"name": "<unresolved child>", "components": []})
    return node


def export_gameobject_json(obj, out_dir: Path, manifest: Manifest, spec: Spec, bundle: str, digest: str) -> None:
    name = object_name(obj) or f"gameobject_{obj.path_id}"
    tree = gameobject_tree(obj)
    target = unique_path(out_dir / "gameobjects" / f"{safe_name(name)}.json", obj.path_id)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(tree, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest.add({
        "profile": spec.profile,
        "source_bundle": bundle,
        "source_sha256": digest,
        "path_id": str(obj.path_id),
        "unity_type": "GameObject",
        "name": name,
        "exported_path": str(target.relative_to(ROOT)),
        "status": "metadata-exported",
        "note": (spec.note + " GameObject hierarchy JSON only; sprite references may be external.").strip(),
    })


def matches(spec: Spec, obj) -> bool:
    if spec.types and obj.type.name not in spec.types:
        return False
    if spec.name_regex:
        if not re.search(spec.name_regex, object_name(obj), re.IGNORECASE):
            return False
    return True


def run_spec(spec: Spec, bundle_root: Path, out_root: Path, manifest: Manifest, dry_run: bool) -> None:
    path = bundle_path(spec.bundle, bundle_root)
    if not path.exists():
        manifest.warn(f"bundle not found: {path}")
        return
    digest = source_sha256(path)
    print(f"[{spec.profile}] {spec.bundle}  sha256={digest[:16]}...")
    env = UnityPy.load(str(path))
    exported = 0
    for obj in env.objects:
        try:
            if not matches(spec, obj):
                continue
            if spec.limit is not None and exported >= spec.limit:
                break
            if dry_run:
                print(f"  would export {obj.type.name}: {object_name(obj)}")
                exported += 1
                continue
            out_dir = out_root / spec.profile
            if obj.type.name == "Sprite":
                export_sprite(obj, out_dir, manifest, spec, spec.bundle, digest)
            elif obj.type.name == "Texture2D":
                export_texture(obj, out_dir, manifest, spec, spec.bundle, digest)
            elif obj.type.name == "AudioClip":
                export_audio(obj, out_dir, manifest, spec, spec.bundle, digest)
            elif obj.type.name == "TextAsset":
                export_text_asset(obj, out_dir, manifest, spec, spec.bundle, digest)
            elif obj.type.name == "GameObject":
                name = object_name(obj)
                if spec.gameobjects and name not in spec.gameobjects:
                    continue
                export_gameobject_json(obj, out_dir, manifest, spec, spec.bundle, digest)
            else:
                continue
            exported += 1
        except Exception as exc:
            manifest.warn(f"{spec.bundle}: {obj.type.name} {object_name(obj)!r}: {type(exc).__name__}: {exc}")
    print(f"  exported {exported} object(s)")


def first_pass_specs() -> list[Spec]:
    return [
        Spec(
            bundle="room/r1_1",
            profile="room-r1-1",
            types=("Sprite", "Texture2D"),
            note="First-pass room r1_1 visual extraction; prefab/spawn semantics not yet interpreted.",
        ),
        Spec(
            bundle="common",
            profile="character-c01-candidate",
            types=("Sprite", "Texture2D"),
            name_regex=r"^c01(_|$)",
            note="c01 is a candidate player character; identity must be confirmed against character data.",
        ),
        Spec(
            bundle="common",
            profile="character-c01-candidate",
            types=("GameObject",),
            gameobjects=("c01",),
            note="c01 hierarchy only; child sprite references may be external.",
        ),
        Spec(
            bundle="res/charactor_common",
            profile="enemy-enemy01-slime-candidate",
            types=("Sprite", "Texture2D"),
            name_regex=r"^enemy01(_|$)",
            note="enemy01 sprite set; assumed to be the slime family, identity to be confirmed.",
        ),
        Spec(
            bundle="level-objects",
            profile="enemy-e-slime01",
            types=("GameObject",),
            gameobjects=("e_slime01",),
            note="e_slime01 hierarchy only; root has no SpriteRenderer, child sprite refs may be external.",
        ),
        Spec(
            bundle="common",
            profile="weapon-candidate",
            types=("Sprite", "Texture2D"),
            name_regex=r"^weapons?_0",
            limit=10,
            note="Small weapon sprite sample; exact starting weapon is unresolved.",
        ),
        Spec(
            bundle="res/floor",
            profile="room-r1-1",
            types=("Sprite", "Texture2D"),
            note="Shared floor/wall/door sprites referenced by room r1_1; the room bundle itself contains prefab references, not the atlas.",
        ),
        Spec(
            bundle="level-objects",
            profile="weapon-candidate",
            types=("GameObject",),
            gameobjects=("weapon", "weapon3", "weapon_staff", "weapon_gatlin"),
            limit=4,
            note="Generic weapon prefab hierarchies only; exact weapon identity unresolved.",
        ),
        Spec(
            bundle="common",
            profile="ui-sprites",
            types=("Sprite", "Texture2D"),
            name_regex=r"^ui_(joy|btn|buff|hp|weapon)",
            limit=30,
            note="Representative UI sprites; not a complete UI atlas.",
        ),
        Spec(
            bundle="common",
            profile="audio-sample",
            types=("AudioClip",),
            name_regex=r"^(fx_gun_1|fx_hit$|fx_hit_p1|fx_chest_open|human1 hurt2|bgm_1Low)$",
            note="Small audio sample; trigger semantics not yet reconstructed.",
        ),
    ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bundle-root", type=Path, default=DEFAULT_BUNDLE_ROOT)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    sub = parser.add_subparsers(dest="command", required=True)

    p_list = sub.add_parser("list", help="list matching objects without exporting")
    p_list.add_argument("--bundle", required=True)
    p_list.add_argument("--type", action="append", default=[])
    p_list.add_argument("--name-regex", default=None)
    p_list.add_argument("--limit", type=int, default=50)

    p_extract = sub.add_parser("extract", help="extract objects")
    p_extract.add_argument("--profile", choices=["first-pass"], default=None)
    p_extract.add_argument("--bundle", action="append", default=[])
    p_extract.add_argument("--type", action="append", default=[])
    p_extract.add_argument("--name-regex", default=None)
    p_extract.add_argument("--gameobject", action="append", default=[])
    p_extract.add_argument("--limit", type=int, default=None)
    p_extract.add_argument("--dry-run", action="store_true")

    args = parser.parse_args(argv)

    if args.command == "list":
        path = bundle_path(args.bundle, args.bundle_root)
        if not path.exists():
            print(f"bundle not found: {path}", file=sys.stderr)
            return 1
        env = UnityPy.load(str(path))
        shown = 0
        for obj in env.objects:
            if args.type and obj.type.name not in args.type:
                continue
            name = object_name(obj)
            if args.name_regex and not re.search(args.name_regex, name, re.IGNORECASE):
                continue
            print(f"{obj.type.name}\t{obj.path_id}\t{name}")
            shown += 1
            if shown >= args.limit:
                break
        print(f"# shown {shown}")
        return 0

    manifest = Manifest()
    specs: list[Spec] = []
    if args.profile == "first-pass":
        specs.extend(first_pass_specs())
    else:
        if not args.bundle:
            print("extract requires --bundle or --profile first-pass", file=sys.stderr)
            return 1
        for bundle in args.bundle:
            specs.append(Spec(
                bundle=bundle,
                profile=safe_name(Path(bundle).name or bundle),
                types=tuple(args.type),
                name_regex=args.name_regex,
                gameobjects=tuple(args.gameobject),
                limit=args.limit,
            ))

    for spec in specs:
        run_spec(spec, args.bundle_root, args.out, manifest, args.dry_run)

    if not args.dry_run:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(json.dumps({
            "tool": manifest.tool,
            "unity_support": manifest.unity_support,
            "entries": manifest.entries,
            "warnings": manifest.warnings,
        }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"manifest: {args.manifest} ({len(manifest.entries)} entries, {len(manifest.warnings)} warnings)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
