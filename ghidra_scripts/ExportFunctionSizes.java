//Export function size cache for tools/progress.py, then print coverage summary
//@category Analysis
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.io.*;
import java.util.*;

public class ExportFunctionSizes extends GhidraScript {
    @Override
    public void run() throws Exception {
        String defaultOutPath = "/mnt/g/dev/halo/build/function_sizes.json";
        String outPath = defaultOutPath;

        if (isRunningHeadless()) {
            String[] scriptArgs = getScriptArgs();
            if (scriptArgs.length > 0 && !scriptArgs[0].trim().isEmpty()) {
                outPath = scriptArgs[0].trim();
            }
        } else {
            File outFile = askFile("Choose function size cache output", "Save");
            outPath = outFile.getAbsolutePath();
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

        // entry addr -> highest address in the function's body. Using the max
        // address (not getBody().getNumAddresses()) survives bodies that Ghidra
        // has fragmented with internal holes -- e.g. a callee correctly marked
        // noreturn drops the (unreachable but still-emitted) cleanup bytes after
        // the call out of the body's address SET, even though those bytes are
        // still real code before the function's true end.
        TreeMap<Long, Long> bodyMax = new TreeMap<>();

        for (Function func : fm.getFunctions(true)) {
            long funcAddr = func.getEntryPoint().getOffset();
            if (funcAddr >= textStart && funcAddr <= textEnd) {
                Address maxAddr = func.getBody().getMaxAddress();
                bodyMax.put(funcAddr, maxAddr.getOffset());
            }
        }

        // Clamp each function's extent to the next function's entry point (or
        // .text end), mirroring tools/verify/function_bounds.py's `end = min(...)`
        // rule. Without the clamp, a stray disassembled range unrelated to the
        // function's real code can balloon its reported size by tens of KB.
        Long[] addrs = bodyMax.keySet().toArray(new Long[0]);
        TreeMap<Long, long[]> funcs = new TreeMap<>();
        long allTextBytes = 0;

        for (int idx = 0; idx < addrs.length; idx++) {
            long addr = addrs[idx];
            long rawMax = bodyMax.get(addr);
            long nextEntry = (idx + 1 < addrs.length) ? addrs[idx + 1] : (textEnd + 1);
            long end = Math.min(rawMax + 1, nextEntry);
            long size = Math.max(end - addr, 1);
            allTextBytes += size;
            funcs.put(addr, new long[]{size, 0});
        }
        int allTextFuncs = funcs.size();

        StringBuilder json = new StringBuilder();
        json.append("{\n");
        json.append("  \"text_section_start\": \"0x" + Long.toHexString(textStart) + "\",\n");
        json.append("  \"text_section_end\": \"0x" + Long.toHexString(textEnd) + "\",\n");
        json.append("  \"text_section_size\": " + textSize + ",\n");
        json.append("  \"total_functions\": " + allTextFuncs + ",\n");
        json.append("  \"total_function_bytes\": " + allTextBytes + ",\n");
        json.append("  \"functions\": {\n");

        int i = 0;
        for (Map.Entry<Long, long[]> e : funcs.entrySet()) {
            String comma = (i < funcs.size() - 1) ? "," : "";
            json.append("    \"0x" + Long.toHexString(e.getKey()) + "\": {\"size\": " + e.getValue()[0] + "}" + comma + "\n");
            i++;
        }
        json.append("  }\n");
        json.append("}\n");

        File outFile = new File(outPath);
        outFile.getParentFile().mkdirs();
        FileWriter fw = new FileWriter(outFile);
        fw.write(json.toString());
        fw.close();

        println("Exported " + funcs.size() + " function sizes to " + outPath);
        println("Total .text: " + textSize + " bytes, " + allTextFuncs + " functions, " + allTextBytes + " function bytes");
    }
}
