#!/usr/bin/env python3
"""Audit which native UI elements use recovered reference assets."""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "assets/generated/ui/manifest.json"
OUT_JSON = ROOT / "analysis/assets/ui_audit.json"
OUT_MD = ROOT / "analysis/assets/ui_audit.md"

# name, screen/use, status, note
ENTRIES = [
    ("title_en0", "Main menu logo", "RECOVERED/INTEGRATED", "local-only title sprite"),
    ("ui_33", "HUD health icon", "RECOVERED/INTEGRATED", "actual heart sprite"),
    ("ui_34", "HUD armor icon", "RECOVERED/INTEGRATED", "actual shield sprite"),
    ("ui_67", "HUD energy icon", "RECOVERED/INTEGRATED", "actual crystal sprite"),
    ("ui_94", "HUD pause button", "RECOVERED/INTEGRATED", "two-bar pause icon"),
    ("ui_5", "HUD currency icon", "RECOVERED/INTEGRATED", "coin sprite"),
    ("ui_98", "Pause/home icon", "RECOVERED/INTEGRATED", "home sprite"),
    ("ui_92", "Pause/resume icon", "RECOVERED/INTEGRATED", "play sprite"),
    ("ui_93", "Pause/settings icon", "RECOVERED/INTEGRATED", "gear sprite"),
    ("ui_99", "Power-slot plus", "RECOVERED/INTEGRATED", "plus sprite"),
    ("ui_btn1", "Pause/settings button frame", "RECOVERED/INTEGRATED", "nine-sliced"),
    ("ui_btn5", "Main-menu NEW GAME frame", "RECOVERED/INTEGRATED", "nine-sliced yellow"),
    ("ui_btn7", "Main-menu CONTINUE frame", "RECOVERED/INTEGRATED", "nine-sliced blue"),
    ("ui_116", "Cloud/save icon", "RECOVERED/INTEGRATED", "local-only cloud icon"),
    ("pixel_bold", "All UI text", "RECOVERED/INTEGRATED", "TTF rasterized at 28 px; local-only atlas"),
    ("ui_buff_*", "Power/buff icons", "DISCOVERED", "not assigned; no fabricated power"),
    ("panel/banner frames", "Pause/settings/character panels", "APPROXIMATED", "native pixel borders; exact frame mapping UNKNOWN"),
    ("HUD outer plate", "HUD frame", "APPROXIMATED", "native frame around recovered icons"),
    ("main menu background", "Main menu", "APPROXIMATED", "native atmospheric bands, no copied screenshot"),
    ("character portraits", "Character station", "RECOVERED/INTEGRATED", "recovered c00..c14 body sprites"),
    ("weapon icons", "Weapon station/HUD", "RECOVERED/INTEGRATED", "recovered weapon visual sprites"),
    ("mobile joystick/touch controls", "Not present", "EXCLUDED", "desktop controls only"),
    ("social tab", "Not present", "EXCLUDED", "intentionally omitted"),
]

def main() -> int:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.is_file() else {"sprites": [], "textures": []}
    names = {s.get("name") for s in manifest.get("sprites", [])}
    document = {
        "schema": 1,
        "tool": "tools/asset/audit_ui_assets.py",
        "manifest": "assets/generated/ui/manifest.json",
        "status_vocabulary": ["CONFIRMED", "RECOVERED", "INTEGRATED", "APPROXIMATED", "UNKNOWN", "MISSING", "EXCLUDED"],
        "entries": [
            {"asset": name, "use": use, "status": status, "note": note,
             "present_in_manifest": ("*" in name or name in names)}
            for name, use, status, note in ENTRIES
        ],
        "counts": {
            "recovered_integrated": sum(1 for e in ENTRIES if "RECOVERED" in e[2]),
            "approximated": sum(1 for e in ENTRIES if e[2] == "APPROXIMATED"),
            "unknown": sum(1 for e in ENTRIES if e[2] == "UNKNOWN"),
            "excluded": sum(1 for e in ENTRIES if e[2] == "EXCLUDED"),
        },
        "note": "Reference-derived pixels/font remain LOCAL_ONLY / NOT_FOR_REDISTRIBUTION.",
    }
    OUT_JSON.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = ["# UI Asset Audit", "", "Reference-derived UI pixels are LOCAL_ONLY / NOT_FOR_REDISTRIBUTION.", "", "| Asset | Use | Status | Note |", "|---|---|---|---|"]
    lines += [f"| {e[0]} | {e[1]} | {e[2]} | {e[3]} |" for e in ENTRIES]
    lines += ["", "## Deliberately excluded", "", "- Mobile joystick/touch controls", "- Social tab", ""]
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT_JSON.relative_to(ROOT)} and {OUT_MD.relative_to(ROOT)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
