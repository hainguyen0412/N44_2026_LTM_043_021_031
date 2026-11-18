param(
    [ValidateSet('gateway','observer','controller','demo','replay','web')][string]$Target='demo',
    [ValidateSet('B0','B1','B2')][string]$Mode='B1',
    [string]$Config='config/gateway.example.json',
    [string]$RunId='',
    [string]$Name='',
    [ValidateRange(0,86400)][int]$RunSeconds=0,
    [string]$StopFile='',
    [ValidateRange(1,65535)][int]$WebPort=8080
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location -LiteralPath $projectRoot
try {
    if($Target -eq 'web' -and -not $PSBoundParameters.ContainsKey('Config')){$Config='config/gateway.mission.example.json'}
    $requiredPrograms=switch($Target) {
        'demo' { @('gateway','c_client') }
        'replay' { @('replay_udp') }
        'gateway' { @('gateway') }
        'web' { @('web_client') }
        default { @('c_client') }
    }
    function ProgramPath([string]$program) {
        $directory=if($program -in @('gateway','c_client','web_client')){'bin'}else{'build\bin'}
        return Join-Path $projectRoot ($directory+'\'+$program+'.exe')
    }
    if($Target -eq 'replay' -and -not(Test-Path -LiteralPath (ProgramPath 'replay_udp'))) {
        & "$PSScriptRoot\build.ps1"
    }
    foreach($program in $requiredPrograms) {
        if(-not(Test-Path -LiteralPath (ProgramPath $program))) {
            throw "Missing executable $program.exe. Run scripts/build.ps1 first, or unpack a Windows release."
        }
    }
    if($Target -eq 'demo') {
        if(-not(Test-Path -LiteralPath "$projectRoot\build\test_support\bin\gateway.exe") -or -not(Test-Path -LiteralPath "$projectRoot\build\test_support\bin\c_client.exe")) {
            & "$PSScriptRoot\build.ps1"
        }
        $demoMode=$Mode
        if(-not $PSBoundParameters.ContainsKey('Mode')) {
            $demoMode=(Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json).measurement.mode
        }
        python -B tools/local_demo.py --mode $demoMode
    } elseif($Target -eq 'replay') {
        $settings=Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
        & "$projectRoot\build\bin\replay_udp.exe" 'tests/fixtures/v1/valid.bin' '--host' '127.0.0.1' '--port' "$($settings.network.udp_bind.port)" '--rate' '50' '--seconds' '60'
    } elseif($Target -eq 'gateway') {
        $effectiveConfig=$Config
        if($PSBoundParameters.ContainsKey('Mode')) {
            $settings=Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
            $settings.measurement.mode=$Mode
            $configDirectory=Join-Path $projectRoot 'output\configs'
            New-Item -ItemType Directory -Force -Path $configDirectory | Out-Null
            $effectiveConfig=Join-Path $configDirectory ('gateway_'+$Mode+'_'+[guid]::NewGuid().ToString('N')+'.json')
            [IO.File]::WriteAllText($effectiveConfig,($settings | ConvertTo-Json -Depth 20),(New-Object Text.UTF8Encoding($false)))
        }
        $runArgs=@('--config',$effectiveConfig)
        if($RunId){$runArgs+=@('--run-id',$RunId)}
        if($RunSeconds){$runArgs+=@('--run-seconds',"$RunSeconds")}
        if($StopFile){$runArgs+=@('--stop-file',$StopFile)}
        & "$projectRoot\bin\gateway.exe" @runArgs
    } elseif($Target -eq 'web') {
        $runArgs=@('--config',$Config,'--web-port',"$WebPort")
        if($RunId){$runArgs+=@('--run-id',$RunId)}
        if($StopFile){$runArgs+=@('--stop-file',$StopFile)}
        & "$projectRoot\bin\web_client.exe" @runArgs
    } else {
        $runArgs=@('--config',$Config,'--role',$Target)
        if($PSBoundParameters.ContainsKey('Mode')){$runArgs+=@('--mode',$Mode)}
        if($RunId){$runArgs+=@('--run-id',$RunId)}
        if($Name){$runArgs+=@('--name',$Name)}
        & "$projectRoot\bin\c_client.exe" @runArgs
    }
    if($LASTEXITCODE -ne 0){throw "Run failed (exit $LASTEXITCODE). If bind reports 10048, a configured port is already in use. Stop your previous run or change the JSON ports."}
} finally { Pop-Location }
