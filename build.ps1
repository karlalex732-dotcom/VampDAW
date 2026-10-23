param([switch]$Tests,[switch]$DspBenchmark,[switch]$NoCopy,[string]$OutputName='VampDAW.exe')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PSScriptRoot 'work\zig-cache-04'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $PSScriptRoot 'work\zig-local-04'
$vampBuildTemp = Join-Path $PSScriptRoot 'work\build-temp'
New-Item -ItemType Directory -Force -Path $vampBuildTemp | Out-Null
$env:TEMP = $vampBuildTemp
$env:TMP = $vampBuildTemp
$compiler = Join-Path $PSScriptRoot 'tools\zig-x86_64-windows-0.15.2\zig.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Portable compiler missing. Run work/bootstrap.py first.' }
New-Item -ItemType Directory -Force -Path 'build' | Out-Null
if ([IO.Path]::GetFileName($OutputName) -ne $OutputName -or -not $OutputName.EndsWith('.exe')) { throw 'OutputName must be an .exe filename without a directory' }
$binary = Join-Path 'build' $OutputName
$sources = @('src/assistant.cpp','src/oracle_chain.cpp','src/ara_host.cpp','src/native_fx.cpp','src/native_fx_specs.cpp','src/native_fx_editor.cpp','src/model.cpp','src/warp.cpp','src/editing.cpp','src/export.cpp','src/note_tools.cpp','src/sample_edit.cpp','src/midi_file.cpp','src/engine.cpp','src/capture.cpp','src/aggregate.cpp','src/asio.cpp','src/plugin.cpp','src/vst2_host.cpp','third_party/pluginterfaces/base/funknown.cpp','third_party/pluginterfaces/base/ustring.cpp')
$flags = @('c++','-target','x86_64-windows-gnu','-mcpu=baseline','-std=c++17','-O2','-fms-extensions','-DVAMP_ENABLE_ARA=1','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0a00','-DNOMINMAX','-Ithird_party','-Isrc')
$libraries = @('-lwinmm','-lole32','-luuid','-lavrt','-lpropsys','-luser32','-lgdi32','-ldwmapi','-lcomdlg32','-lgdiplus','-lcomctl32','-lshlwapi','-lshell32','-ladvapi32','-lmfplat','-lmfreadwrite','-lmfuuid')
if (-not $DspBenchmark) {
    & $compiler rc /fo 'build/windows-manifest.o' /:output-format coff /I 'assets' 'assets/windows.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Windows interface manifest compilation failed' }
    $vampEncoder = Join-Path $PSScriptRoot 'tools\ffmpeg\bin\ffmpeg.exe'
    if (-not (Test-Path -LiteralPath $vampEncoder)) { throw 'Audio encoder missing. Build it with build-codecs.ps1 and copy work\codec-prefix\bin\ffmpeg.exe to tools\ffmpeg\bin\ffmpeg.exe (see packaging\SOURCE-BUILD.txt).' }
    $vampEncoderDigest = (Get-FileHash -LiteralPath $vampEncoder -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'src\encoder_digest.hpp'), "#pragma once`n#define VAMP_ENCODER_SHA256 `"$vampEncoderDigest`"`n", [Text.UTF8Encoding]::new($false))
}
if ($DspBenchmark) {
    & $compiler @flags @sources 'tests/dsp_benchmark.cpp' @libraries '-o' $binary
}
elseif ($Tests) {
    & $compiler @flags 'tests/ara_fixture.cpp' 'third_party/pluginterfaces/base/funknown.cpp' '-lole32' '-shared' '-o' 'build/ara-fixture.vst3'
    if ($LASTEXITCODE -ne 0) { throw 'ARA fixture compilation failed' }
    for ($vampFixtureKind=0; $vampFixtureKind -lt 4; $vampFixtureKind++) {
        & $compiler @flags "-DFIXTURE_KIND=$vampFixtureKind" 'tests/vst2_fixture.cpp' '-luser32' '-shared' '-o' "build/vst2-fixture-$vampFixtureKind.dll"
        if ($LASTEXITCODE -ne 0) { throw 'VST2 fixture compilation failed' }
    }
    & $compiler @flags 'tests/processing_fixture.cpp' 'third_party/pluginterfaces/base/funknown.cpp' '-lole32' '-shared' '-o' 'build/processing-fixture.vst3'
    if ($LASTEXITCODE -ne 0) { throw 'VST3 lifecycle fixture compilation failed' }
    & $compiler @flags 'tests/midi_fixture.cpp' 'third_party/pluginterfaces/base/funknown.cpp' '-lole32' '-shared' '-o' 'build/midi-fixture.vst3'
    if ($LASTEXITCODE -ne 0) { throw 'VST3 MIDI fixture compilation failed' }
    & $compiler @flags @sources 'tests/core_tests.cpp' 'build/windows-manifest.o' @libraries '-o' 'build/VampDAW-tests.exe'
}
else {
    & $compiler rc /fo 'build/vamp-resource.o' /:output-format coff /I 'assets' 'assets/vamp.rc'
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
    & $compiler @flags @sources 'src/main.cpp' 'build/vamp-resource.o' 'build/windows-manifest.o' @libraries '-municode' '-Wl,--subsystem,windows' '-o' $binary
}
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
if ($DspBenchmark) { & $binary; if ($LASTEXITCODE -ne 0) { throw 'DSP benchmark failed' } }
elseif ($Tests) { & '.\build\VampDAW-tests.exe'; if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' } }
elseif (-not $NoCopy) { Copy-Item -LiteralPath $binary -Destination $OutputName -Force }
