// Soul Knight Linux — apply Il2CppDumper symbols to a Ghidra import.
//
// Usage (headless):
//   analyzeHeadless <projectDir> <projectName> \
//     -import analysis/apktool_out/lib/arm64-v8a/libil2cpp.so \
//     -scriptPath tools/re/ghidra \
//     -postScript ApplyIl2CppSymbols.java <symbols.tsv>
//
// The TSV is produced by:
//   tools/re/il2cpp_re.py export-symbols --out <symbols.tsv>
//
// Each line is: <16-hex-digit RVA>\t<Il2CppDumper method name>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

public class ApplyIl2CppSymbols extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("usage: ApplyIl2CppSymbols.java <symbols.tsv>");
            return;
        }

        File input = new File(args[0]);
        if (!input.isFile()) {
            println("symbol file not found: " + input);
            return;
        }

        int applied = 0;
        int skipped = 0;
        try (BufferedReader reader = new BufferedReader(new FileReader(input))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (line.isBlank() || line.startsWith("#")) {
                    continue;
                }
                int tab = line.indexOf('\t');
                if (tab <= 0) {
                    skipped++;
                    continue;
                }

                String addressText = line.substring(0, tab).trim();
                String rawName = line.substring(tab + 1).trim();
                if (addressText.isEmpty() || rawName.isEmpty()) {
                    skipped++;
                    continue;
                }

                long rva;
                try {
                    rva = Long.parseUnsignedLong(addressText, 16);
                } catch (NumberFormatException e) {
                    skipped++;
                    continue;
                }

                Address address = toAddr(rva);
                if (address == null || !currentProgram.getMemory().contains(address)) {
                    skipped++;
                    continue;
                }

                String name = sanitize(rawName);

                // Avoid duplicate labels at the same address; prefer the first
                // metadata entry (generic/singleton methods often share an
                // address and cannot all be represented as one label).
                if (hasLabel(address)) {
                    skipped++;
                    continue;
                }

                try {
                    createLabel(address, name, true, SourceType.IMPORTED);
                    Function function = getFunctionAt(address);
                    if (function == null) {
                        createFunction(address, name);
                    } else {
                        function.setName(name, SourceType.IMPORTED);
                    }
                    applied++;
                } catch (Exception e) {
                    skipped++;
                    if (skipped < 20) {
                        println("skip " + addressText + " " + rawName + ": " + e.getMessage());
                    }
                }
            }
        }

        println("ApplyIl2CppSymbols: applied=" + applied + " skipped=" + skipped);
    }

    private boolean hasLabel(Address address) {
        Symbol[] symbols = currentProgram.getSymbolTable().getSymbols(address);
        for (Symbol symbol : symbols) {
            if (!symbol.isDynamic()) {
                return true;
            }
        }
        return false;
    }

    private String sanitize(String name) {
        String cleaned = name.replaceAll("[^A-Za-z0-9_.$<>]", "_");
        if (cleaned.isEmpty()) {
            return "il2cpp_unnamed";
        }
        if (Character.isDigit(cleaned.charAt(0))) {
            cleaned = "il2cpp_" + cleaned;
        }
        return cleaned;
    }
}
