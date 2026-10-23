param([int]$Jobs=4)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$archiveRoot=Join-Path $PSScriptRoot 'work\packaging'
$codecSource=Join-Path $PSScriptRoot 'work\codec-src'
New-Item -ItemType Directory -Force -Path $archiveRoot,$codecSource | Out-Null
$dependencies=Get-Content -LiteralPath 'packaging\dependencies.json' -Raw | ConvertFrom-Json
foreach($entry in $dependencies.PSObject.Properties){
    if($entry.Name -in @('innosetup-6.7.3.exe','zig-x86_64-windows-0.15.2.zip')){continue}
    $archive=Join-Path $archiveRoot $entry.Name
    if(-not (Test-Path -LiteralPath $archive)){
        $bundled=Join-Path (Join-Path $PSScriptRoot 'dependency-sources') $entry.Name
        if(Test-Path -LiteralPath $bundled){Copy-Item -LiteralPath $bundled -Destination $archive}
        else{Invoke-WebRequest -Uri $entry.Value.url -OutFile $archive}
    }
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value.sha256){throw "Checksum mismatch: $($entry.Name)"}
    if($entry.Name -like '*.tar.*'){
        & tar.exe -xf $archive -C $codecSource
        if($LASTEXITCODE -ne 0){throw "Source extraction failed: $($entry.Name)"}
    }
}
$devkit=Join-Path $PSScriptRoot 'tools\w64devkit\bin'
if(-not (Test-Path -LiteralPath (Join-Path $devkit 'gcc.exe'))){
    $archive=Join-Path $archiveRoot 'w64devkit-x64-2.10.1.7z.exe'
    $tools=Join-Path $PSScriptRoot 'tools'
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    $process=Start-Process -FilePath $archive -ArgumentList '-y',('-o"'+$tools+'"') -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw 'Compiler extraction failed'}
}
$env:PATH=$devkit+';'+$env:PATH
$env:VAMP_BUILD_JOBS=[Math]::Max(1,[Math]::Min(32,$Jobs))
& (Join-Path $devkit 'sh.exe') 'packaging/build-codecs.sh'
if($LASTEXITCODE -ne 0){throw 'Codec build failed; see work/codec-src/*/*-vamp.log'}
Write-Host 'Audio encoder: work\codec-prefix\bin\ffmpeg.exe'
