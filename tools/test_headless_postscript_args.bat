@echo off
setlocal

set GHIDRA_ROOT=G:\ghidra_12.0.3_PUBLIC_20260210\ghidra_12.0.3_PUBLIC
set PROJECT_DIR=G:\dev\Reverse Engineering\Halo 1
set PROJECT_NAME=Halo 1 Xbox
set PROGRAM_NAME=cachebeta.xbe
set SCRIPT_DIR=C:\Users\stian\ghidra_scripts

"%GHIDRA_ROOT%\support\analyzeHeadless.bat" "%PROJECT_DIR%" "%PROJECT_NAME%" -process "%PROGRAM_NAME%" -scriptPath "%SCRIPT_DIR%" -postScript "PrintScriptArgs.java" /exporter "COFF relocatable object" /include-range "000a8220-000a833e" /export "G:\dev\halo\artifacts\objdiff\game_engine_evaluate_game_complexity\orig.o" -noanalysis

endlocal
