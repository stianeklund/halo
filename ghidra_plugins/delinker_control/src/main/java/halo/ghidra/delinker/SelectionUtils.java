package halo.ghidra.delinker;

import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressFactory;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Program;
import ghidra.program.util.ProgramSelection;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class SelectionUtils {
  private SelectionUtils() {}

  public static AddressSet resolveSelection(Program program, ProgramSelection currentSelection,
      String selectionMode, String rangeText) {
    if ("current_selection".equals(selectionMode)) {
      if (currentSelection == null || currentSelection.isEmpty()) {
        throw new RpcException("NO_SELECTION", "No current selection");
      }
      return new AddressSet(currentSelection);
    }

    if ("range".equals(selectionMode)) {
      return new AddressSet(parseRange(program, rangeText));
    }

    throw new RpcException("BAD_SELECTION_MODE", "Unsupported selection mode: " + selectionMode);
  }

  public static AddressSetView parseRange(Program program, String rangeText) {
    if (rangeText == null || rangeText.isBlank()) {
      throw new RpcException("BAD_RANGE", "Range must be provided");
    }

    String[] parts = rangeText.split("-", 2);
    if (parts.length != 2) {
      throw new RpcException("BAD_RANGE", "Expected range in start-end form");
    }

    AddressFactory addressFactory = program.getAddressFactory();
    Address start = addressFactory.getAddress(parts[0]);
    Address end = addressFactory.getAddress(parts[1]);
    if (start == null || end == null) {
      throw new RpcException("BAD_RANGE", "Could not parse range addresses");
    }
    return addressFactory.getAddressSet(start, end);
  }

  public static List<Map<String, String>> toRangeList(AddressSetView set) {
    List<Map<String, String>> ranges = new ArrayList<>();
    if (set == null) {
      return ranges;
    }

    AddressRangeIterator it = set.getAddressRanges();
    while (it.hasNext()) {
      AddressRange range = it.next();
      Map<String, String> item = new LinkedHashMap<>();
      item.put("min", range.getMinAddress().toString());
      item.put("max", range.getMaxAddress().toString());
      ranges.add(item);
    }

    return ranges;
  }
}
