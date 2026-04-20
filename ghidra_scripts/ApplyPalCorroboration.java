// Apply PAL 2002 name corroboration to IDA-tagged functions only.
// Args:
//   0: path to matched_names.csv

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.HashMap;
import java.util.Map;

public class ApplyPalCorroboration extends GhidraScript {

    private static String normalizeName(String name) {
        String s = name == null ? "" : name.trim();
        if (s.isEmpty()) {
            return s;
        }
        if (s.startsWith("?")) {
            return s;
        }
        while (s.startsWith("@") || s.startsWith("_")) {
            s = s.substring(1);
            if (s.isEmpty()) {
                return s;
            }
        }
        int at = s.lastIndexOf('@');
        if (at > 0 && at < s.length() - 1) {
            boolean digits = true;
            for (int i = at + 1; i < s.length(); i++) {
                if (!Character.isDigit(s.charAt(i))) {
                    digits = false;
                    break;
                }
            }
            if (digits) {
                s = s.substring(0, at);
            }
        }
        return s;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: ApplyPalCorroboration <matched_names.csv>");
            return;
        }

        File csv = new File(args[0]);
        if (!csv.exists()) {
            printerr("Missing CSV: " + csv.getAbsolutePath());
            return;
        }

        Map<String, String> nameToMatchType = new HashMap<>();
        try (BufferedReader br = new BufferedReader(new FileReader(csv))) {
            String line = br.readLine(); // header
            while ((line = br.readLine()) != null) {
                if (line.trim().isEmpty()) {
                    continue;
                }
                String[] parts = line.split(",", 6);
                if (parts.length < 4) {
                    continue;
                }
                String idaNorm = parts[2].trim();
                String matchType = parts[3].trim();
                if (idaNorm.isEmpty()) {
                    continue;
                }
                String prev = nameToMatchType.get(idaNorm);
                if (prev == null || ("normalized".equals(prev) && "exact".equals(matchType))) {
                    nameToMatchType.put(idaNorm, matchType);
                }
            }
        }

        Listing listing = currentProgram.getListing();

        int scanned = 0;
        int idaTagged = 0;
        int matched = 0;
        int commented = 0;
        int bookmarked = 0;

        for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
            scanned++;
            String fname = fn.getName();
            if (fname == null || fname.startsWith("FUN_")) {
                continue;
            }

            Address entry = fn.getEntryPoint();
            String plate = listing.getComment(ghidra.program.model.listing.CodeUnit.PLATE_COMMENT, entry);
            boolean idaTaggedFn = (plate != null && plate.contains("IDA2247"));
            if (!idaTaggedFn) {
                continue;
            }
            idaTagged++;

            String norm = normalizeName(fname);
            String matchType = nameToMatchType.get(norm);
            if (matchType == null) {
                continue;
            }
            matched++;

            String line = "PAL2002 Corroborated: name '" + fname + "' matched cvdump publics (" + matchType + ").";
            String newPlate;
            if (plate == null || plate.trim().isEmpty()) {
                newPlate = line;
            } else if (plate.contains("PAL2002 Corroborated:")) {
                newPlate = plate;
            } else {
                newPlate = plate + "\n" + line;
            }

            if (plate == null || !plate.equals(newPlate)) {
                setPlateComment(entry, newPlate);
                commented++;
            }

            if (plate == null || !plate.contains("PAL2002 Corroborated:")) {
                createBookmark(entry, "PAL2002-Corroborated", fname + " | " + matchType);
                bookmarked++;
            }
        }

        println("scanned=" + scanned);
        println("ida_tagged=" + idaTagged);
        println("matched=" + matched);
        println("comments_added_or_updated=" + commented);
        println("bookmarks_added=" + bookmarked);
    }
}
