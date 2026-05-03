//Export function size cache for tools/progress.py, then print coverage summary
//@category Analysis
import ghidra.app.script.GhidraScript;
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

        TreeMap<Long, long[]> funcs = new TreeMap<>();
        int allTextFuncs = 0;
        long allTextBytes = 0;

        for (Function func : fm.getFunctions(true)) {
            long funcAddr = func.getEntryPoint().getOffset();
            if (funcAddr >= textStart && funcAddr <= textEnd) {
                allTextFuncs++;
                long fsize = func.getBody().getNumAddresses();
                allTextBytes += fsize;
                funcs.put(funcAddr, new long[]{fsize, 0});
            }
        }

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
