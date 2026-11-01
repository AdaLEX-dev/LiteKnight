#!/usr/bin/env python3
"""Extract the reference I2Languages table used by NPC dialogue and UI names.

The language asset is a serialized LanguageSourceAsset, not a TextAsset. Terms
and the 13 locale strings are stored as aligned length-prefixed strings. This
tool scans for term-shaped keys and recovers the first locale (English) without
inventing any text.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT / "analysis/apktool_out/assets/bin/Data/65e9ec86e4ca50c478ad2b19c9878f51"
DEFAULT_OUTPUT = ROOT / "analysis/assets/localization.json"

LOCALE_COUNT = 13


def parse_terms(data: bytes) -> dict[str, str]:
    terms: dict[str, str] = {}
    for match in re.finditer(rb"[A-Za-z][A-Za-z0-9_/\.-]{2,80}", data):
        start = match.start()
        # A term key is preceded by its own 4-byte length and NUL padding.
        length = struct.unpack_from("<I", data, start - 4)[0] if start >= 4 else 0
        if length != len(match.group()):
            continue
        for offset in range(0, 17):
            pos = match.end() + offset
            if pos + 4 > len(data):
                break
            count = struct.unpack_from("<I", data, pos)[0]
            if count != LOCALE_COUNT:
                continue
            cursor = pos + 4
            values: list[str] = []
            valid = True
            for _ in range(count):
                if cursor + 4 > len(data):
                    valid = False
                    break
                size = struct.unpack_from("<I", data, cursor)[0]
                cursor += 4
                if size > 1024 or cursor + size > len(data):
                    valid = False
                    break
                try:
                    values.append(data[cursor:cursor + size].decode("utf-8"))
                except UnicodeDecodeError:
                    valid = False
                    break
                cursor += size
                cursor = (cursor + 3) & ~3
            if valid and values:
                terms.setdefault(match.group().decode("utf-8", "replace"),
                                 values[0])
                break
    return terms


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    data = args.source.read_bytes()
    terms = parse_terms(data)
    document = {
        "schema": 1,
        "tool": "tools/asset/extract_localization.py",
        "source": str(args.source.relative_to(ROOT)),
        "source_sha256": __import__("hashlib").sha256(data).hexdigest(),
        "locale_count": LOCALE_COUNT,
        "english_locale_index": 0,
        "term_count": len(terms),
        "terms": dict(sorted(terms.items())),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8")
    print(f"Wrote {args.output.relative_to(ROOT)}: {len(terms)} terms")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
