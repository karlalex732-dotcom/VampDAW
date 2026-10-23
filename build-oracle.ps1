param([switch]$Tests)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PSScriptRoot 'work\zig-cache-04'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $PSScriptRoot 'work\zig-local-04'
$oracleBuildTemp = Join-Path $PSScriptRoot 'work\build-temp'
New-Item -ItemType Directory -Force -Path $oracleBuildTemp | Out-Null
$env:TEMP = $oracleBuildTemp
$env:TMP = $oracleBuildTemp
$compiler = Join-Path $PSScriptRoot 'tools\zig-x86_64-windows-0.15.2\zig.exe'
$oracleBundle = Join-Path $PSScriptRoot 'build\Oracle Rack.vst3\Contents\x86_64-win'
New-Item -ItemType Directory -Force -Path $oracleBundle | Out-Null
$sources = @('src/assistant.cpp','src/oracle_chain.cpp','src/ara_host.cpp','src/native_fx.cpp','src/native_fx_specs.cpp','src/native_fx_editor.cpp','src/model.cpp','src/warp.cpp','src/editing.cpp','src/export.cpp','src/note_tools.cpp','src/sample_edit.cpp','src/midi_file.cpp','src/engine.cpp','src/capture.cpp','src/aggregate.cpp','src/asio.cpp','src/plugin.cpp','src/vst2_host.cpp','third_party/pluginterfaces/base/funknown.cpp','third_party/pluginterfaces/base/ustring.cpp')
$flags = @('c++','-target','x86_64-windows-gnu','-mcpu=baseline','-std=c++17','-O2','-fms-extensions','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0a00','-DNOMINMAX','-DORACLE_RACK_BUILD','-Ithird_party','-Isrc','-Iplugins/oracle')
$libraries = @('-lwinmm','-lole32','-luuid','-lavrt','-lpropsys','-luser32','-lgdi32','-ldwmapi','-lcomdlg32','-lgdiplus','-lcomctl32','-lshlwapi','-lshell32','-ladvapi32','-lmfplat','-lmfreadwrite','-lmfuuid')
& $compiler rc /fo 'build/oracle-resource.o' /:output-format coff /I 'plugins/oracle' 'plugins/oracle/oracle.rc'
if ($LASTEXITCODE -ne 0) { throw 'Oracle DLL manifest compilation failed' }
& $compiler rc /fo 'build/windows-manifest.o' /:output-format coff /I 'assets' 'assets/windows.rc'
if ($LASTEXITCODE -ne 0) { throw 'Oracle utility manifest compilation failed' }
& $compiler @flags @sources 'plugins/oracle/oracle_rack.cpp' 'plugins/oracle/oracle_vst3.cpp' 'build/oracle-resource.o' @libraries '-shared' '-o' (Join-Path $oracleBundle 'Oracle Rack.vst3')
if ($LASTEXITCODE -ne 0) { throw 'Oracle VST3 build failed' }
& $compiler @flags @sources 'plugins/oracle/oracle_scan.cpp' 'build/windows-manifest.o' @libraries '-municode' '-Wl,--subsystem,windows' '-o' (Join-Path $oracleBundle 'OracleScan.exe')
if ($LASTEXITCODE -ne 0) { throw 'Oracle scan helper build failed' }
Copy-Item -LiteralPath 'LICENSE.txt' -Destination (Join-Path $PSScriptRoot 'build\Oracle Rack.vst3\LICENSE.txt') -Force
Copy-Item -LiteralPath 'THIRD_PARTY_NOTICES.txt' -Destination (Join-Path $PSScriptRoot 'build\Oracle Rack.vst3\THIRD_PARTY_NOTICES.txt') -Force
if ($Tests) {
    & $compiler @flags '-DFIXTURE_KIND=0' 'tests/vst2_fixture.cpp' '-luser32' '-shared' '-o' 'build/vst2-fixture-0.dll'
    if ($LASTEXITCODE -ne 0) { throw 'Oracle nested effect fixture build failed' }
    & $compiler @flags 'tests/oracle_parameter_fixture.cpp' '-shared' '-o' 'build/oracle-parameter-fixture.dll'
    if ($LASTEXITCODE -ne 0) { throw 'Oracle parameter fixture build failed' }
    & $compiler @flags @sources 'plugins/oracle/oracle_rack.cpp' 'tests/oracle_rack_tests.cpp' 'build/windows-manifest.o' @libraries '-o' 'build/Oracle-tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Oracle tests build failed' }
    & '.\build\Oracle-tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Oracle tests failed' }
    & $compiler @flags 'tests/oracle_module_load.cpp' 'third_party/pluginterfaces/base/funknown.cpp' '-lole32' '-o' 'build/Oracle-module-load.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Oracle independent module test build failed' }
    & '.\build\Oracle-module-load.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Oracle requires an unexpected host manifest' }
}
