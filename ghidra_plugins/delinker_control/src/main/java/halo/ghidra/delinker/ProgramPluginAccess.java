package halo.ghidra.delinker;

import ghidra.program.model.listing.Program;
import ghidra.program.util.ProgramSelection;

public interface ProgramPluginAccess {
  Program getCurrentProgram();

  ProgramSelection getCurrentSelection();
}
