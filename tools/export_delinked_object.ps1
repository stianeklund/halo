param(
  [Parameter(Mandatory = $true)] [string] $GhidraRoot,
  [Parameter(Mandatory = $true)] [string] $ProjectDir,
  [Parameter(Mandatory = $true)] [string] $ProjectName,
  [Parameter(Mandatory = $true)] [string] $Program,
  [Parameter(Mandatory = $true)] [string] $ScriptDir,
  [Parameter(Mandatory = $true)] [string] $Exporter,
  [Parameter(Mandatory = $true)] [string] $Output,
  [string] $Symbol,
  [string] $Range,
  [switch] $Analyze
)

if ([string]::IsNullOrWhiteSpace($Symbol) -eq [string]::IsNullOrWhiteSpace($Range)) {
  throw 'Specify exactly one of -Symbol or -Range.'
}

$argsList = @(
  $ProjectDir,
  $ProjectName,
  '-process', $Program,
  '-scriptPath', $ScriptDir,
  '-postScript', 'DelinkProgram.java',
  '/exporter', $Exporter
)

if ($Symbol) {
  $argsList += @('/include', $Symbol)
} else {
  $argsList += @('/include-range', $Range)
}

$argsList += @('/export', $Output)

if (-not $Analyze) {
  $argsList += '-noanalysis'
}

& cmd.exe /c "$GhidraRoot\support\analyzeHeadless.bat" @argsList
exit $LASTEXITCODE
