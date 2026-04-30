$ghidraRoot = 'G:\ghidra_12.0.3_PUBLIC_20260210\ghidra_12.0.3_PUBLIC'
$projectDir = 'G:\dev\Reverse Engineering\Halo 1'
$projectName = 'Halo 1 Xbox'
$programName = 'cachebeta.xbe'
$scriptDir = 'C:\Users\stian\ghidra_scripts'

$argsList = @(
  $projectDir,
  $projectName,
  '-process', $programName,
  '-scriptPath', $scriptDir,
  '-postScript', 'PrintScriptArgs.java',
  '/exporter', 'COFF relocatable object',
  '/include-range', '000a8220-000a833e',
  '/export', 'G:\dev\halo\artifacts\objdiff\game_engine_evaluate_game_complexity\orig.o',
  '-noanalysis'
)

& cmd.exe /c "$ghidraRoot\support\analyzeHeadless.bat" @argsList
exit $LASTEXITCODE
