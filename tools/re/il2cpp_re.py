#!/usr/bin/env python3
"""Reproducible IL2CPP static-analysis helper for Soul Knight 2.6.8.

Workflow:
    method name -> dump.cs/script.json entry -> RVA -> libil2cpp.so offset
                -> llvm-objdump disassembly -> call/xref map -> evidence note

This tool deliberately uses only Python's standard library plus the system
``llvm-objdump`` binary. Ghidra is optional and can be layered on later.

Default inputs (override with flags or environment variables):
    --dump    $SOULKNIGHT_DUMP    or /home/jules/bin/dump.cs
    --script  $SOULKNIGHT_SCRIPT  or /home/jules/bin/script.json
    --binary  $SOULKNIGHT_BINARY  or analysis/apktool_out/lib/arm64-v8a/libil2cpp.so

Examples:
    tools/re/il2cpp_re.py lookup RGRandom
    tools/re/il2cpp_re.py class RGRandom
    tools/re/il2cpp_re.py method RGRandom.Range
    tools/re/il2cpp_re.py disasm RGRandom.Range --size 0x120
    tools/re/il2cpp_re.py address 0xDD07E8
    tools/re/il2cpp_re.py xref RGRandom.Range
    tools/re/il2cpp_re.py xref --address 0xDD07E8
"""

from __future__ import annotations

import argparse
import bisect
import json
import os
import re
import struct
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DUMP = Path(os.environ.get("SOULKNIGHT_DUMP", "/home/jules/bin/dump.cs"))
DEFAULT_SCRIPT = Path(os.environ.get("SOULKNIGHT_SCRIPT", "/home/jules/bin/script.json"))
DEFAULT_BINARY = Path(
    os.environ.get(
        "SOULKNIGHT_BINARY",
        str(ROOT / "analysis/apktool_out/lib/arm64-v8a/libil2cpp.so"),
    )
)
DEFAULT_LLVM_OBJDUMP = os.environ.get("LLVM_OBJDUMP", "llvm-objdump")


# ---------------------------------------------------------------------------
# Metadata loading
# ---------------------------------------------------------------------------


@dataclass
class Method:
    address: int
    name: str
    signature: str

    @property
    def class_name(self) -> str:
        return self.name.split("$$", 1)[0] if "$$" in self.name else ""

    @property
    def short_name(self) -> str:
        return self.name.split("$$", 1)[1] if "$$" in self.name else self.name


@dataclass
class ClassInfo:
    name: str
    header: str = ""
    fields: list[tuple[str, str, int]] = field(default_factory=list)  # type, name, offset

    def methods(self, methods: Iterable[Method]) -> list[Method]:
        return [m for m in methods if m.class_name == self.name]


class Metadata:
    def __init__(self, dump_path: Path, script_path: Path):
        self.dump_path = dump_path
        self.script_path = script_path
        self.methods: list[Method] = []
        self.by_address: dict[int, list[Method]] = {}
        self.classes: dict[str, ClassInfo] = {}
        self._load_methods()
        self._load_classes()

    def _load_methods(self) -> None:
        with self.script_path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
        for entry in data.get("ScriptMethod", []):
            method = Method(
                address=int(entry["Address"]),
                name=entry.get("Name", ""),
                signature=entry.get("Signature", ""),
            )
            self.methods.append(method)
            self.by_address.setdefault(method.address, []).append(method)
        self.methods.sort(key=lambda m: m.address)
        self._addresses = [m.address for m in self.methods]

    def _load_classes(self) -> None:
        class_re = re.compile(
            r"^\s*(?:\[[^\]]*\]\s*)*(?:(?:public|private|protected|internal|static|sealed|abstract|partial)\s+)*"
            r"(?:class|struct|enum|interface)\s+([A-Za-z_][A-Za-z0-9_./`<>]*)"
        )
        field_re = re.compile(
            r"^\s*(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;\s*//\s*0x([0-9A-Fa-f]+)"
        )
        stack: list[tuple[str, int]] = []
        depth = 0
        current: ClassInfo | None = None
        with self.dump_path.open("r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                header = class_re.match(line)
                if header:
                    name = header.group(1)
                    info = ClassInfo(name=name, header=line.strip())
                    self.classes.setdefault(name, info)
                    stack.append((name, depth + 1))
                    current = info
                    # Class headers do not contain the opening brace in dump.cs;
                    # do not pop the stack before the next line is read.
                    continue
                if current is not None:
                    match = field_re.match(line)
                    if match:
                        type_name = match.group(1).strip()
                        field_name = match.group(2)
                        offset = int(match.group(3), 16)
                        current.fields.append((type_name, field_name, offset))
                depth += line.count("{") - line.count("}")
                while stack and depth < stack[-1][1]:
                    stack.pop()
                current = self.classes.get(stack[-1][0]) if stack else None

    def find_methods(self, query: str) -> list[Method]:
        q = query.lower()
        variants = {q, q.replace(".", "$$")}
        exact = [m for m in self.methods if m.name.lower() in variants]
        if exact:
            return exact
        return [m for m in self.methods if q in m.name.lower() or q.replace(".", "$$") in m.name.lower()]

    def find_class(self, query: str) -> list[ClassInfo]:
        q = query.lower()
        exact = [c for c in self.classes.values() if c.name.lower() == q]
        if exact:
            return exact
        return [c for c in self.classes.values() if q in c.name.lower()]

    def address_to_method(self, address: int) -> Method | None:
        idx = bisect.bisect_right(self._addresses, address) - 1
        if idx < 0:
            return None
        return self.methods[idx]

    def methods_at(self, address: int) -> list[Method]:
        return self.by_address.get(address, [])


# ---------------------------------------------------------------------------
# ELF helpers
# ---------------------------------------------------------------------------


@dataclass
class Section:
    name: str
    address: int
    offset: int
    size: int
    flags: int


def read_elf_sections(path: Path) -> list[Section]:
    with path.open("rb") as fh:
        data = fh.read()
    if data[:4] != b"\x7fELF" or data[4] != 2:
        raise ValueError(f"not an ELF64 file: {path}")
    endian = "<" if data[5] == 1 else ">"
    (e_shoff,) = struct.unpack_from(endian + "Q", data, 0x28)
    (e_shentsize,) = struct.unpack_from(endian + "H", data, 0x3A)
    (e_shnum,) = struct.unpack_from(endian + "H", data, 0x3C)
    (e_shstrndx,) = struct.unpack_from(endian + "H", data, 0x3E)
    headers = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack_from(
            endian + "IIQQQQ", data, off
        )
        headers.append((sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size))
    strtab_off = headers[e_shstrndx][4]
    strtab_size = headers[e_shstrndx][5]
    strtab = data[strtab_off : strtab_off + strtab_size]
    sections = []
    for sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size in headers:
        end = strtab.find(b"\0", sh_name)
        name = strtab[sh_name:end].decode("ascii", "replace")
        sections.append(Section(name, sh_addr, sh_offset, sh_size, sh_flags))
    return sections


def executable_sections(path: Path) -> list[Section]:
    # SHF_EXECINSTR = 0x4
    return [s for s in read_elf_sections(path) if (s.flags & 0x4) and s.size > 0]


def rva_to_file_offset(path: Path, rva: int) -> int | None:
    for section in read_elf_sections(path):
        if section.address <= rva < section.address + section.size and section.offset:
            return section.offset + (rva - section.address)
    # For this binary the executable segment is identity-mapped; fall back to
    # RVA as an offset only if it is inside the file.
    if rva < path.stat().st_size:
        return rva
    return None


# ---------------------------------------------------------------------------
# Disassembly and xrefs
# ---------------------------------------------------------------------------


def sign_extend(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)


def iter_branch_targets(binary: Path) -> Iterable[tuple[int, int, str]]:
    """Yield (instruction_address, target_address, mnemonic) for direct B/BL."""
    with binary.open("rb") as fh:
        data = fh.read()
    for section in executable_sections(binary):
        chunk = data[section.offset : section.offset + section.size]
        for i in range(0, len(chunk) - 3, 4):
            word = struct.unpack_from("<I", chunk, i)[0]
            if (word & 0xFC000000) == 0x94000000:
                mnemonic = "bl"
            elif (word & 0xFC000000) == 0x14000000:
                mnemonic = "b"
            else:
                continue
            imm26 = word & 0x03FFFFFF
            target = section.address + i + sign_extend(imm26 << 2, 28)
            yield section.address + i, target, mnemonic


def next_method_address(meta: Metadata, address: int) -> int:
    idx = bisect.bisect_right(meta._addresses, address)
    if idx < len(meta._addresses):
        nxt = meta._addresses[idx]
        if 0 < nxt - address <= 0x4000:
            return nxt
    return address + 0x200


def disassemble(binary: Path, start: int, stop: int, meta: Metadata) -> str:
    cmd = [
        DEFAULT_LLVM_OBJDUMP,
        "-d",
        "--no-show-raw-insn",
        f"--start-address=0x{start:X}",
        f"--stop-address=0x{stop:X}",
        str(binary),
    ]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or "llvm-objdump failed")
    lines = []
    call_re = re.compile(r"\b(bl|b)\s+0x([0-9A-Fa-f]+)")
    for line in proc.stdout.splitlines():
        match = call_re.search(line)
        if match:
            target = int(match.group(2), 16)
            names = meta.methods_at(target)
            if names:
                line += "  ; " + " | ".join(m.name for m in names)
            else:
                caller = meta.address_to_method(target)
                if caller:
                    line += f"  ; {caller.name}+0x{target - caller.address:X}"
        lines.append(line)
    return "\n".join(lines)


def xrefs(binary: Path, meta: Metadata, target: int) -> list[str]:
    results = []
    target_names = meta.methods_at(target)
    target_label = " | ".join(m.name for m in target_names) if target_names else f"0x{target:X}"
    for insn_addr, branch_target, mnemonic in iter_branch_targets(binary):
        if branch_target != target:
            continue
        caller = meta.address_to_method(insn_addr)
        if caller:
            delta = insn_addr - caller.address
            caller_label = f"{caller.name}+0x{delta:X}"
        else:
            caller_label = f"0x{insn_addr:X}"
        results.append(
            f"0x{insn_addr:08X}  {mnemonic:2s} -> 0x{target:08X}  {target_label}  from {caller_label}"
        )
    return results


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def fmt_method(method: Method) -> str:
    return f"0x{method.address:08X}  {method.name}\n             {method.signature}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="IL2CPP metadata/RVA/disassembly helper",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--dump", type=Path, default=DEFAULT_DUMP)
    parser.add_argument("--script", type=Path, default=DEFAULT_SCRIPT)
    parser.add_argument("--binary", type=Path, default=DEFAULT_BINARY)
    sub = parser.add_subparsers(dest="command", required=True)

    p_lookup = sub.add_parser("lookup", help="search classes, fields, and methods")
    p_lookup.add_argument("query")

    p_class = sub.add_parser("class", help="show a class's metadata fields and methods")
    p_class.add_argument("query")

    p_method = sub.add_parser("method", help="show method metadata and RVA")
    p_method.add_argument("query")

    p_disasm = sub.add_parser("disasm", help="disassemble a method (annotates direct calls)")
    p_disasm.add_argument("query", nargs="?")
    p_disasm.add_argument("--address", type=lambda x: int(x, 0), default=None)
    p_disasm.add_argument("--size", type=lambda x: int(x, 0), default=None)

    p_addr = sub.add_parser("address", help="map a native address/RVA to method metadata")
    p_addr.add_argument("address", type=lambda x: int(x, 0))

    p_xref = sub.add_parser("xref", help="find direct B/BL call sites for a method")
    p_xref.add_argument("query", nargs="?")
    p_xref.add_argument("--address", type=lambda x: int(x, 0))

    p_export = sub.add_parser(
        "export-symbols", help="export address<TAB>name TSV for Ghidra annotation"
    )
    p_export.add_argument(
        "--out",
        type=Path,
        default=ROOT / "analysis/re/generated/il2cpp_symbols.tsv",
    )

    args = parser.parse_args(argv)

    if not args.dump.exists():
        print(f"error: metadata dump not found: {args.dump}", file=sys.stderr)
        return 2
    if not args.script.exists():
        print(f"error: script.json not found: {args.script}", file=sys.stderr)
        return 2
    if not args.binary.exists():
        print(f"error: libil2cpp.so not found: {args.binary}", file=sys.stderr)
        return 2

    meta = Metadata(args.dump, args.script)

    if args.command == "lookup":
        q = args.query.lower()
        classes = [c for c in meta.classes.values() if q in c.name.lower()]
        methods = [m for m in meta.methods if q in m.name.lower()]
        print(f"classes: {len(classes)}  methods: {len(methods)}")
        for cls in classes[:20]:
            print(f"\nCLASS {cls.name}")
            print(f"  {cls.header}")
            for type_name, field_name, offset in cls.fields[:20]:
                print(f"  +0x{offset:02X}  {type_name} {field_name}")
            if len(cls.fields) > 20:
                print(f"  ... {len(cls.fields) - 20} more fields")
        for method in methods[:40]:
            print(f"\nMETHOD {method.name}")
            print(f"  RVA 0x{method.address:08X}")
            print(f"  {method.signature}")
        if len(methods) > 40:
            print(f"\n... {len(methods) - 40} more methods; narrow the query")
        return 0

    if args.command == "class":
        matches = meta.find_class(args.query)
        if not matches:
            print(f"no class matching {args.query!r}", file=sys.stderr)
            return 1
        for cls in matches:
            print(f"CLASS {cls.name}")
            print(f"  {cls.header}")
            print("  Fields:")
            for type_name, field_name, offset in cls.fields:
                print(f"    +0x{offset:02X}  {type_name} {field_name}")
            methods = cls.methods(meta.methods)
            print(f"  Methods: {len(methods)}")
            for method in methods:
                print(f"    0x{method.address:08X}  {method.short_name}")
                print(f"               {method.signature}")
        return 0

    if args.command == "method":
        matches = meta.find_methods(args.query)
        if not matches:
            print(f"no method matching {args.query!r}", file=sys.stderr)
            return 1
        for method in matches:
            print(fmt_method(method))
        return 0

    if args.command == "address":
        methods = meta.methods_at(args.address)
        predecessor = meta.address_to_method(args.address)
        if methods:
            for method in methods:
                print(fmt_method(method))
        elif predecessor:
            print(
                f"0x{args.address:08X} is inside {predecessor.name}+0x{args.address - predecessor.address:X}"
            )
        else:
            print(f"no metadata method at or before 0x{args.address:08X}", file=sys.stderr)
            return 1
        offset = rva_to_file_offset(args.binary, args.address)
        if offset is not None:
            print(f"file offset: 0x{offset:X}")
        return 0

    if args.command == "disasm":
        if args.address is not None:
            methods = meta.methods_at(args.address)
            if not methods:
                predecessor = meta.address_to_method(args.address)
                if predecessor:
                    methods = [predecessor]
                else:
                    print(f"no method at 0x{args.address:08X}", file=sys.stderr)
                    return 1
            method = methods[0]
        else:
            if not args.query:
                print("disasm requires a method query or --address", file=sys.stderr)
                return 1
            matches = meta.find_methods(args.query)
            if not matches:
                print(f"no method matching {args.query!r}", file=sys.stderr)
                return 1
            if len(matches) > 1:
                print(f"multiple methods match {args.query!r}; choose --address:", file=sys.stderr)
                for candidate in matches:
                    print(f"  0x{candidate.address:08X}  {candidate.name}", file=sys.stderr)
                return 1
            method = matches[0]
        stop = method.address + args.size if args.size else next_method_address(meta, method.address)
        print(f"; {method.name}")
        print(f"; RVA 0x{method.address:08X} .. 0x{stop:08X}")
        print(disassemble(args.binary, method.address, stop, meta))
        return 0

    if args.command == "xref":
        if args.address is not None:
            target = args.address
        elif args.query:
            matches = meta.find_methods(args.query)
            unique = {m.address for m in matches}
            if not unique:
                print(f"no method matching {args.query!r}", file=sys.stderr)
                return 1
            if len(unique) > 1:
                print(f"multiple targets match {args.query!r}; choose --address:", file=sys.stderr)
                for method in matches:
                    print(f"  0x{method.address:08X}  {method.name}", file=sys.stderr)
                return 1
            target = unique.pop()
        else:
            print("xref requires a method query or --address", file=sys.stderr)
            return 1
        results = xrefs(args.binary, meta, target)
        print(f"xrefs to 0x{target:08X}: {len(results)}")
        for line in results:
            print(line)
        return 0

    if args.command == "export-symbols":
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with args.out.open("w", encoding="utf-8") as fh:
            for method in meta.methods:
                # Ghidra labels cannot contain arbitrary punctuation; the
                # annotation script sanitizes further. Keep the original name
                # here so the TSV remains a faithful mapping.
                fh.write(f"{method.address:016X}\t{method.name}\n")
        print(f"wrote {len(meta.methods)} symbols to {args.out}")
        return 0

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
