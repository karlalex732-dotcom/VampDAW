param([switch]$SkipBuild,[string]$Python='python',[string]$Binary='')
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler=Join-Path $PSScriptRoot 'tools\innosetup\ISCC.exe'
if(-not (Test-Path -LiteralPath $compiler)){throw 'Inno Setup 6.7.3 missing: tools\innosetup\ISCC.exe'}
if(-not $SkipBuild){ & (Join-Path $PSScriptRoot 'build.ps1') -NoCopy -OutputName 'VampDAW-0.4.19.exe' }
$stageArguments=@('packaging\stage-release.py')
if($Binary){if(-not $SkipBuild){throw '-Binary requires -SkipBuild'};$stageArguments+=@('--binary',$Binary)}
$json=& $Python @stageArguments
if($LASTEXITCODE -ne 0){throw 'Release staging failed'}
$release=$json | ConvertFrom-Json
& $compiler ('/DStage='+$release.stage) 'packaging\VampDAW.iss'
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed'}
Copy-Item -LiteralPath 'packaging\README-ALPHA.txt' -Destination 'dist\README-ALPHA.txt' -Force
$files=@('VampDAW-0.4.19-alpha-win64-Setup.exe','VampDAW-0.4.19-source.zip')
$lines=foreach($name in $files){(Get-FileHash -LiteralPath (Join-Path 'dist' $name) -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$name}
[IO.File]::WriteAllLines((Join-Path $PSScriptRoot 'dist\SHA256SUMS.txt'),$lines,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'build\last-installer-stage.json'),$json,[Text.UTF8Encoding]::new($false))
Write-Host 'Installer and complete source: dist\'
