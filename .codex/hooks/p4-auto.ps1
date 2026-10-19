[CmdletBinding()]
param()

$ErrorActionPreference = 'SilentlyContinue'
$ProgressPreference = 'SilentlyContinue'
$script:LogPath = Join-Path $PSScriptRoot 'logs\p4-auto.log'

function Write-Log {
    param([string]$Message)
    try {
        $directory = Split-Path -Parent $script:LogPath
        if (-not (Test-Path -LiteralPath $directory)) { New-Item -ItemType Directory -Path $directory -Force | Out-Null }
        if ((Test-Path -LiteralPath $script:LogPath) -and (Get-Item -LiteralPath $script:LogPath).Length -gt 1MB) { Clear-Content -LiteralPath $script:LogPath }
        "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Message" | Out-File -LiteralPath $script:LogPath -Append -Encoding utf8
    } catch {
    }
}

function Resolve-P4Exe {
    $candidates = @($env:P4EXE, 'C:\Program Files\Perforce\p4.exe', 'C:\Program Files (x86)\Perforce\p4.exe')
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    $found = Get-Command 'p4.exe' -CommandType Application -ErrorAction SilentlyContinue |
        Where-Object { (Get-Item -LiteralPath $_.Source).Length -gt 0 } |
        Select-Object -First 1
    if ($found) { return $found.Source }
    return $null
}

function Invoke-P4 {
    param([string[]]$P4Args, [string]$WorkingDirectory)
    $previous = Get-Location
    try {
        if ($WorkingDirectory -and (Test-Path -LiteralPath $WorkingDirectory)) { Set-Location -LiteralPath $WorkingDirectory }
        $output = & $script:P4Exe @P4Args 2>&1
        return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = ($output | Out-String).Trim() }
    } finally {
        Set-Location -LiteralPath $previous
    }
}

function Test-InDepot {
    param([string]$Path)
    $directory = Split-Path -Parent $Path
    $result = Invoke-P4 -P4Args @('fstat', '-T', 'depotFile,headAction', $Path) -WorkingDirectory $directory
    return ($result.ExitCode -eq 0 -and $result.Output -match 'depotFile' -and $result.Output -notmatch 'headAction\s+delete')
}

function Resolve-LocalPath {
    param([string]$Path, [string]$WorkingDirectory)
    if (-not $Path) { return $null }
    try {
        if (-not [System.IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $WorkingDirectory $Path }
        return [System.IO.Path]::GetFullPath($Path)
    } catch { return $null }
}

function Get-PatchPaths {
    param([string]$Patch)
    $result = New-Object System.Collections.Generic.List[object]
    foreach ($line in ($Patch -split "`r?`n")) {
        if ($line -match '^\*\*\* (Update|Add|Delete) File: (.+)$') {
            $result.Add([pscustomobject]@{ Action = $Matches[1]; Path = $Matches[2].Trim() })
        }
    }
    return $result
}

function Invoke-Checkout {
    param([string]$Path)
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $item = Get-Item -LiteralPath $Path -Force
    if (-not $item.IsReadOnly) { return }
    $result = Invoke-P4 -P4Args @('edit', $Path) -WorkingDirectory $item.DirectoryName
    Write-Log "edit $Path -> exit=$($result.ExitCode) $($result.Output)"
    if ((Get-Item -LiteralPath $Path -Force).IsReadOnly) {
        Set-ItemProperty -LiteralPath $Path -Name IsReadOnly -Value $false
        Write-Log "fallback clear read-only $Path"
    }
}

function Invoke-Add {
    param([string]$Path)
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Test-InDepot -Path $Path)) { return }
    $directory = Split-Path -Parent $Path
    $result = Invoke-P4 -P4Args @('add', $Path) -WorkingDirectory $directory
    Write-Log "add $Path -> exit=$($result.ExitCode) $($result.Output)"
}

function Invoke-Delete {
    param([string]$Path, [string]$WorkingDirectory)
    if (-not $Path -or (Test-Path -LiteralPath $Path)) { return }
    $opened = Invoke-P4 -P4Args @('opened', $Path) -WorkingDirectory $WorkingDirectory
    if ($opened.ExitCode -eq 0 -and $opened.Output -match '\s-\s+add\s') {
        $result = Invoke-P4 -P4Args @('revert', $Path) -WorkingDirectory $WorkingDirectory
        Write-Log "revert-add $Path -> exit=$($result.ExitCode) $($result.Output)"
        return
    }
    if (Test-InDepot -Path $Path) {
        $result = Invoke-P4 -P4Args @('delete', $Path) -WorkingDirectory $WorkingDirectory
        Write-Log "delete $Path -> exit=$($result.ExitCode) $($result.Output)"
        return
    }
    $wildcard = ($Path.TrimEnd('\', '/')) + '\...'
    $probe = Invoke-P4 -P4Args @('fstat', '-T', 'depotFile', $wildcard) -WorkingDirectory $WorkingDirectory
    if ($probe.ExitCode -eq 0 -and $probe.Output -match 'depotFile') {
        $result = Invoke-P4 -P4Args @('delete', $wildcard) -WorkingDirectory $WorkingDirectory
        Write-Log "delete $wildcard -> exit=$($result.ExitCode) $($result.Output)"
    }
}

$script:P4Exe = Resolve-P4Exe
if (-not $script:P4Exe) { exit 0 }
try { $payload = [Console]::In.ReadToEnd() | ConvertFrom-Json } catch { exit 0 }

$cwd = $payload.cwd
if (-not $cwd) { $cwd = (Get-Location).Path }
$searchDirectory = Get-Item -LiteralPath $cwd -ErrorAction SilentlyContinue
while ($searchDirectory) {
    if (Test-Path -LiteralPath (Join-Path $searchDirectory.FullName '.p4config') -PathType Leaf) {
        $env:P4CONFIG = '.p4config'
        break
    }
    $searchDirectory = $searchDirectory.Parent
}

if ($payload.hook_event_name -eq 'PreToolUse' -and $payload.tool_name -eq 'apply_patch') {
    foreach ($entry in (Get-PatchPaths -Patch $payload.tool_input.command)) {
        if ($entry.Action -in @('Update', 'Delete')) {
            Invoke-Checkout -Path (Resolve-LocalPath -Path $entry.Path -WorkingDirectory $cwd)
        }
    }
    exit 0
}

if ($payload.hook_event_name -eq 'PostToolUse' -and $payload.tool_name -eq 'apply_patch') {
    foreach ($entry in (Get-PatchPaths -Patch $payload.tool_input.command)) {
        $path = Resolve-LocalPath -Path $entry.Path -WorkingDirectory $cwd
        if ($entry.Action -eq 'Add') { Invoke-Add -Path $path }
        elseif ($entry.Action -eq 'Delete') { Invoke-Delete -Path $path -WorkingDirectory $cwd }
    }
    exit 0
}

if ($payload.hook_event_name -eq 'PostToolUse' -and $payload.tool_name -eq 'Bash') {
    $command = $payload.tool_input.command
    if ($command -notmatch '(?i)(\bRemove-Item\b|\bri\b|\brm\b|\brmdir\b|\bdel\b|\berase\b|\bunlink\b)') { exit 0 }
    $candidates = New-Object System.Collections.Generic.HashSet[string]
    foreach ($pattern in @('"([^"]+)"', "'([^']+)'", '(?<![\w"''])([A-Za-z]:[\\/][^\s"'';|&]+|\.{0,2}[\\/][^\s"'';|&]+)')) {
        foreach ($match in [regex]::Matches($command, $pattern)) {
            $token = $match.Groups[1].Value.Trim()
            if ($token -and $token -match '[\\/]') { [void]$candidates.Add($token) }
        }
    }
    foreach ($token in $candidates) {
        if ($token -match '[\*\?]') { continue }
        Invoke-Delete -Path (Resolve-LocalPath -Path $token -WorkingDirectory $cwd) -WorkingDirectory $cwd
    }
}

exit 0
