<#
    Perforce 自动签出 / 登记 hook。

    由 Claude Code 的 PreToolUse / PostToolUse 直接拉起，stdin 收到 hook 输入 JSON。
    约束：永远静默、永远 exit 0 —— hook 失败不得阻断编辑工具本身。

    Mode:
      Checkout  文件已存在且只读 -> p4 edit（失败则退回清除只读位，保证编辑不被卡住）
      Add       文件不在 depot 中 -> p4 add
      Delete    shell 命令执行后，磁盘上已消失但 depot 中仍存在的路径 -> p4 delete
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Checkout', 'Add', 'Delete')]
    [string]$Mode
)

$ErrorActionPreference = 'SilentlyContinue'
$ProgressPreference = 'SilentlyContinue'

$script:LogPath = Join-Path $PSScriptRoot '..\logs\p4-auto.log'

function Write-Log {
    param([string]$Message)
    try {
        $dir = Split-Path -Parent $script:LogPath
        if (-not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        if ((Test-Path -LiteralPath $script:LogPath) -and (Get-Item -LiteralPath $script:LogPath).Length -gt 1MB) {
            Clear-Content -LiteralPath $script:LogPath
        }
        "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') [$Mode] $Message" |
            Out-File -LiteralPath $script:LogPath -Append -Encoding utf8
    } catch {
    }
}

function Resolve-P4Exe {
    # PATH 中排在最前的 System32\p4 是 0 字节占位文件，直接执行会失败，必须优先用绝对路径
    $candidates = @(
        $env:P4EXE,
        'C:\Program Files\Perforce\p4.exe',
        'C:\Program Files (x86)\Perforce\p4.exe'
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }
    $found = Get-Command 'p4.exe' -CommandType Application -ErrorAction SilentlyContinue |
        Where-Object { (Get-Item -LiteralPath $_.Source).Length -gt 0 } |
        Select-Object -First 1
    if ($found) {
        return $found.Source
    }
    return $null
}

function Invoke-P4 {
    param([string[]]$P4Args, [string]$WorkingDirectory)

    $previous = Get-Location
    try {
        if ($WorkingDirectory -and (Test-Path -LiteralPath $WorkingDirectory)) {
            Set-Location -LiteralPath $WorkingDirectory
        }
        $output = & $script:P4Exe @P4Args 2>&1
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Output   = ($output | Out-String).Trim()
        }
    } finally {
        Set-Location -LiteralPath $previous
    }
}

# depot 中是否存在该路径的活动版本（已被删除的版本视为不存在）
function Test-InDepot {
    param([string]$Path)

    $dir = Split-Path -Parent $Path
    $result = Invoke-P4 -P4Args @('fstat', '-T', 'depotFile,headAction', $Path) -WorkingDirectory $dir
    if ($result.ExitCode -ne 0 -or -not $result.Output) {
        return $false
    }
    if ($result.Output -notmatch 'depotFile') {
        return $false
    }
    if ($result.Output -match 'headAction\s+delete') {
        return $false
    }
    return $true
}

$script:P4Exe = Resolve-P4Exe
if (-not $script:P4Exe) {
    exit 0
}

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) {
    exit 0
}

try {
    $payload = $raw | ConvertFrom-Json
} catch {
    exit 0
}

switch ($Mode) {

    'Checkout' {
        $path = $payload.tool_input.file_path
        if (-not $path) { break }
        # 文件尚不存在 = 新建，交给 Add 模式在写入之后处理
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { break }

        $item = Get-Item -LiteralPath $path -Force
        if (-not $item.IsReadOnly) { break }

        $result = Invoke-P4 -P4Args @('edit', $path) -WorkingDirectory $item.DirectoryName
        Write-Log "edit $path -> exit=$($result.ExitCode) $($result.Output)"

        # p4 edit 未能清掉只读位（文件不在 depot、无权限等），退回本地解锁避免编辑被卡死
        if ((Get-Item -LiteralPath $path -Force).IsReadOnly) {
            Set-ItemProperty -LiteralPath $path -Name IsReadOnly -Value $false
            Write-Log "fallback clear read-only $path"
        }
    }

    'Add' {
        $path = $payload.tool_input.file_path
        if (-not $path) { break }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { break }
        if (Test-InDepot -Path $path) { break }

        $dir = Split-Path -Parent $path
        $result = Invoke-P4 -P4Args @('add', $path) -WorkingDirectory $dir
        Write-Log "add $path -> exit=$($result.ExitCode) $($result.Output)"
    }

    'Delete' {
        $command = $payload.tool_input.command
        if (-not $command) { break }
        # 只对删除类命令做补登记，避免对每条 shell 命令都扫描路径
        if ($command -notmatch '(?i)(\bRemove-Item\b|\bri\b|\brm\b|\brmdir\b|\bdel\b|\berase\b|\bunlink\b)') { break }

        $cwd = $payload.cwd
        if (-not $cwd) { $cwd = $PWD.Path }

        # 引号包裹的字符串 + 裸露的带分隔符 token，两类都视为候选路径
        $candidates = New-Object System.Collections.Generic.HashSet[string]
        foreach ($pattern in @('"([^"]+)"', "'([^']+)'", '(?<![\w"''])([A-Za-z]:[\\/][^\s"'';|&]+|\.{0,2}[\\/][^\s"'';|&]+)')) {
            foreach ($match in [regex]::Matches($command, $pattern)) {
                $token = $match.Groups[1].Value.Trim()
                if ($token -and ($token -match '[\\/]')) {
                    [void]$candidates.Add($token)
                }
            }
        }

        foreach ($token in $candidates) {
            # 通配符路径无法可靠还原为具体文件，跳过
            if ($token -match '[\*\?]') { continue }

            $full = $token
            if (-not [System.IO.Path]::IsPathRooted($full)) {
                $full = Join-Path $cwd $token
            }
            try {
                $full = [System.IO.Path]::GetFullPath($full)
            } catch {
                continue
            }

            # 仍存在于磁盘 = 没被删掉，不该动它
            if (Test-Path -LiteralPath $full) { continue }

            # 尚未提交就被删掉：撤销 add，而不是登记一次 delete
            $opened = Invoke-P4 -P4Args @('opened', $full) -WorkingDirectory $cwd
            if ($opened.ExitCode -eq 0 -and $opened.Output -match '\s-\s+add\s') {
                $result = Invoke-P4 -P4Args @('revert', $full) -WorkingDirectory $cwd
                Write-Log "revert-add $full -> exit=$($result.ExitCode) $($result.Output)"
                continue
            }

            if (Test-InDepot -Path $full) {
                $result = Invoke-P4 -P4Args @('delete', $full) -WorkingDirectory $cwd
                Write-Log "delete $full -> exit=$($result.ExitCode) $($result.Output)"
                continue
            }

            # 目录整体删除：depot 里按 dir/... 补登记
            $wildcard = ($full.TrimEnd('\', '/')) + '\...'
            $probe = Invoke-P4 -P4Args @('fstat', '-T', 'depotFile', $wildcard) -WorkingDirectory $cwd
            if ($probe.ExitCode -eq 0 -and $probe.Output -match 'depotFile') {
                $result = Invoke-P4 -P4Args @('delete', $wildcard) -WorkingDirectory $cwd
                Write-Log "delete $wildcard -> exit=$($result.ExitCode) $($result.Output)"
            }
        }
    }
}

exit 0
