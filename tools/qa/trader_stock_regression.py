#!/usr/bin/env python3
"""Trader stock randomization regression.

Runs the real native binary in Living Room mode with a fixed --map-seed and
captures the stock line emitted by ConfigureTraderOffers. The recovered trader
uses the run seed; this proves seed A -> stock A, seed B -> stock B and seed A
again -> stock A.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "build" / "soul-knight"
PATTERN = re.compile(
    r"Trader:\s+(\d+)\s+orb offers generated seed=(-?\d+)\s+stock=([0-9:,]+)")


def stock_for(seed: int, timeout: float = 20.0) -> tuple[str, str]:
    with tempfile.NamedTemporaryFile(
            mode="w", prefix="liteknight-trader-", suffix=".log",
            delete=False) as handle:
        log_path = Path(handle.name)
    command = [
        str(BINARY), "--hub", "--smoke-seconds", "2",
        "--map-seed", str(seed),
    ]
    try:
        result = subprocess.run(
            command, cwd=ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=timeout, text=True,
            env={**os.environ, "SDL_VIDEODRIVER": os.environ.get(
                "SDL_VIDEODRIVER", "wayland")})
        output = result.stdout
        log_path.write_text(output, encoding="utf-8")
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or "") + (error.stderr or "")
        log_path.write_text(output, encoding="utf-8")
    matches = PATTERN.findall(output)
    # The hub may be loaded more than once during startup; the final stock
    # line is the active room state.
    if not matches:
        print("TRADER_STOCK_REGRESSION")
        print(f"seed={seed}")
        print("Expected: one Trader stock line")
        print("Actual: none")
        print(f"Log: {log_path}")
        return "", ""
    count, reported_seed, stock = matches[-1]
    log_path.unlink(missing_ok=True)
    return reported_seed, f"{count}|{stock}"


def main() -> int:
    if not BINARY.is_file():
        print(f"Missing native binary: {BINARY}", file=sys.stderr)
        return 2
    seed_a = stock_for(11)
    seed_b = stock_for(22)
    seed_a_again = stock_for(11)
    checks = [
        ("seed_A_stock", bool(seed_a[0]) and seed_a[1].startswith("3|"),
         seed_a),
        ("seed_B_stock", bool(seed_b[0]) and seed_b[1].startswith("3|"),
         seed_b),
        ("seed_A_repeat", seed_a == seed_a_again, seed_a_again),
        ("different_seeds_differ", seed_a != seed_b, (seed_a, seed_b)),
    ]
    failures = 0
    print("TRADER_STOCK_REGRESSION")
    for name, ok, value in checks:
        print(f"Target: {name}")
        print(f"Expected: {'valid/deterministic' if name != 'different_seeds_differ' else 'different stock'}")
        print(f"Actual: seed={value[0]} stock={value[1]}")
        print("PASS" if ok else "FAIL")
        print()
        if not ok:
            failures += 1
    print(f"SUMMARY: {len(checks) - failures}/{len(checks)} trader stock checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
