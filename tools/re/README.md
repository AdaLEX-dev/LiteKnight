# Static Reverse-Engineering Pipeline

This directory provides a reproducible path from a method name to disassembly and call sites, without requiring an Android device.

```text
method name
    ↓  tools/re/il2cpp_re.py (script.json + dump.cs)
metadata entry / RVA
    ↓  ELF section mapping
native address / file offset
    ↓  llvm-objdump
disassembly
    ↓  direct-call annotation + xref scan
behavioural evidence
    ↓
analysis/re/<subsystem>.md
```

## Inputs

Defaults (override with flags or environment variables):

| Input | Default |
|---|---|
| `dump.cs` | `/home/jules/bin/dump.cs` (`SOULKNIGHT_DUMP`) |
| `script.json` | `/home/jules/bin/script.json` (`SOULKNIGHT_SCRIPT`) |
| `libil2cpp.so` | `analysis/apktool_out/lib/arm64-v8a/libil2cpp.so` (`SOULKNIGHT_BINARY`) |
| disassembler | `llvm-objdump` (`LLVM_OBJDUMP`) |

The complete Il2CppDumper output lives outside Git because it contains recovered
names and derived proprietary structure. The repository keeps the tooling and the
evidence notes, not the whole dump.

## Commands

```bash
# Find classes/fields/methods by substring
tools/re/il2cpp_re.py lookup RGRandom

# Show fields (with offsets) and method RVAs for a class
tools/re/il2cpp_re.py class RGRandom

# Show a method's metadata/RVA (overloads listed separately)
tools/re/il2cpp_re.py method 'RGRandom.Range'

# Disassemble one overload by address; direct calls are annotated
tools/re/il2cpp_re.py disasm --address 0xDD07E8 --size 0xE0

# Map an address back to metadata
tools/re/il2cpp_re.py address 0xDD07E8

# Find direct B/BL call sites, mapped to caller methods
tools/re/il2cpp_re.py xref --address 0xDD07E8

# Produce a TSV for the optional Ghidra annotation script
tools/re/il2cpp_re.py export-symbols --out analysis/re/generated/il2cpp_symbols.tsv
```

## Ghidra (optional)

```bash
tools/re/ghidra_import.sh
```

This imports the binary with `-noanalysis` for a fast first pass and applies
Il2CppDumper labels/functions from the generated TSV. Run a full Ghidra analysis
afterward for decompiler quality.

## Rules

1. Metadata is structural evidence only. A method name or field name does not
   establish a mechanic.
2. Disassembly is behavioural evidence. Record addresses and instruction-level
   reasoning in `analysis/re/<subsystem>.md`.
3. Direct B/BL xrefs do not capture virtual/interface dispatch. Treat xref
   counts as a lower bound.
4. Do not paste large decompiler dumps into the native source tree. Summarize
   the behaviour and cite addresses in the evidence document.
5. Every reconstructed subsystem must have a document under `analysis/re/` with
   confidence labels and unresolved questions.
