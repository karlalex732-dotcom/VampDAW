param([string]$OutputDirectory = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'VampDAW-Defender-Report'))
# Read-only diagnostics. Does not restore threats, change protection or upload data.
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$vampReport = [ordered]@{ created = [DateTime]::UtcNow.ToString('o'); version = '0.4.11-alpha'; detections = @(); threats = @(); errors = @() }
try {
    $vampStatus = Get-MpComputerStatus
    $vampReport.status = $vampStatus | Select-Object AMServiceEnabled,AntivirusEnabled,RealTimeProtectionEnabled,AntivirusSignatureVersion,AntivirusSignatureLastUpdated
} catch { $vampReport.errors += $_.Exception.Message }
try {
    $vampDetections = @(Get-MpThreatDetection | Where-Object { ($_.Resources -join ' ') -match 'VampDAW' })
    $vampReport.detections = @($vampDetections | Select-Object ThreatID,InitialDetectionTime,LastThreatStatusChangeTime,ActionSuccess,Resources)
    $vampIds = @($vampDetections | Select-Object -ExpandProperty ThreatID)
    $vampReport.threats = @(Get-MpThreat | Where-Object { $_.ThreatID -in $vampIds } | Select-Object ThreatID,ThreatName,SeverityID,IsActive,DidThreatExecute)
} catch { $vampReport.errors += $_.Exception.Message }
try {
    $vampReport.events = @(Get-WinEvent -FilterHashtable @{ LogName='Microsoft-Windows-Windows Defender/Operational'; Id=1116,1117,1118; StartTime=(Get-Date).AddDays(-14) } -ErrorAction Stop | Where-Object { $_.Message -match 'VampDAW' } | Select-Object TimeCreated,Id,Message)
} catch { $vampReport.errors += $_.Exception.Message }
$vampJson = $vampReport | ConvertTo-Json -Depth 7
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'Defender-report.json'), $vampJson, [Text.UTF8Encoding]::new($false))
Write-Host ('Report saved: ' + (Join-Path $OutputDirectory 'Defender-report.json'))
