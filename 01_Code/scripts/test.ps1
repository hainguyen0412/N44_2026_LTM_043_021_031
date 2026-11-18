param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [switch]$Build,
    [switch]$UnitOnly
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$localTools=[IO.Path]::GetFullPath((Join-Path $projectRoot '..\output\tooling\cmake_ninja'))
if(-not(Get-Command ctest -CommandType Application -ErrorAction SilentlyContinue) -and (Test-Path -LiteralPath "$localTools\cmake\data\bin\ctest.exe")) {
    $env:Path="$localTools\cmake\data\bin;$localTools\bin;C:\msys64\ucrt64\bin;$env:Path"
    $env:PYTHONPATH=$localTools
}

$preset='windows-'+$Configuration.ToLowerInvariant()
Push-Location -LiteralPath $projectRoot
try {
    if(-not(Test-Path -LiteralPath "build\$preset\CTestTestfile.cmake")){$Build=$true}
    if($Build){& "$PSScriptRoot\build.ps1" -Configuration $Configuration}
    if(-not(Test-Path -LiteralPath "build\$preset\CTestTestfile.cmake")) {
        throw 'CTest configuration missing. Run scripts/build.ps1 first, or use test.ps1 -Build.'
    }
    $testArguments=@('--preset',$preset,'--output-on-failure')
    if($UnitOnly){$testArguments+=@('-L','unit')}
    & ctest @testArguments
    if($LASTEXITCODE -ne 0){throw 'Tests failed. See CTest output and output/test_results.'}
} finally {Pop-Location}
