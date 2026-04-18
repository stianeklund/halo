//Compute byte-level coverage of ported functions vs .text section
//@category Analysis
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.io.*;
import java.util.*;

public class TextSectionCoverage extends GhidraScript {
    @Override
    public void run() throws Exception {
        String kbPath = "/mnt/g/dev/halo/kb_meta.json";
        BufferedReader br = new BufferedReader(new FileReader(kbPath));
        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = br.readLine()) != null) sb.append(line);
        br.close();

        String metaJson = sb.toString();

        Set<Long> portedAddrs = new TreeSet<>();
        int depth = 0;
        boolean inKey = false;
        boolean inValue = false;
        String currentKey = "";
        String currentValue = "";
        boolean inAddr = false;
        boolean inStatus = false;
        boolean isFunction = false;

        for (int i = 0; i < metaJson.length(); i++) {
            char c = metaJson.charAt(i);
            if (c == '{') depth++;
            else if (c == '}') depth--;
        }

        String[] parts = metaJson.replace("{", "\n{").replace("}", "\n}").replace(",", "\n").split("\n");
        String addrKey = null;
        String statusVal = null;
        String kindVal = null;

        for (int i = 0; i < parts.length; i++) {
            String p = parts[i].trim();
            if (p.contains("\"status\"") && p.contains("\"ported\"")) statusVal = "ported";
            if (p.contains("\"kind\"") && p.contains("\"function\"")) kindVal = "function";
            if (p.startsWith("\"0x")) {
                addrKey = p.replaceAll("[\" ]", "").split(":")[0];
            }
            if (statusVal != null && kindVal != null && addrKey != null) {
                if (statusVal.equals("ported") && kindVal.equals("function")) {
                    portedAddrs.add(Long.parseLong(addrKey.substring(2), 16));
                }
                addrKey = null;
                statusVal = null;
                kindVal = null;
            }
            if (p.equals("{")) {
                addrKey = null;
                statusVal = null;
                kindVal = null;
            }
        }

        FunctionManager fm = currentProgram.getFunctionManager();
        Memory mem = currentProgram.getMemory();
        MemoryBlock textBlock = mem.getBlock(".text");

        if (textBlock == null) {
            println("ERROR: .text block not found");
            return;
        }

        long textStart = textBlock.getStart().getOffset();
        long textEnd = textBlock.getEnd().getOffset();
        long textSize = textEnd - textStart + 1;

        int allTextFuncs = 0;
        long allTextBytes = 0;
        int portedFound = 0;
        long portedBytes = 0;

        for (Function func : fm.getFunctions(true)) {
            long funcAddr = func.getEntryPoint().getOffset();
            if (funcAddr >= textStart && funcAddr <= textEnd) {
                allTextFuncs++;
                long fsize = func.getBody().getNumAddresses();
                allTextBytes += fsize;
                if (portedAddrs.contains(funcAddr)) {
                    portedFound++;
                    portedBytes += fsize;
                }
            }
        }

        println("=== .text Section Byte-Level Coverage ===");
        println("");
        println(".text section size:     " + textSize + " bytes (" + String.format("%.1f", textSize/1024.0) + " KB)");
        println("All Ghidra functions:   " + allTextFuncs);
        println("All function bytes:     " + allTextBytes + " (" + String.format("%.1f", allTextBytes*100.0/textSize) + "% of .text)");
        println("");
        println("Ported in kb_meta:      " + portedAddrs.size());
        println("Ported found in Ghidra: " + portedFound);
        println("Ported function bytes:  " + portedBytes + " (" + String.format("%.2f", portedBytes*100.0/textSize) + "% of .text section)");
        println("Ported / function bytes:" + String.format("%.1f", portedBytes*100.0/allTextBytes) + "% of Ghidra function bytes");
        println("");
        println("Unported Ghidra funcs:  " + (allTextFuncs - portedFound));
        println("Unported func bytes:    " + (allTextBytes - portedBytes) + " bytes");
    }
}
