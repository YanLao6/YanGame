<#
    /submit 命令的执行后端：收集变更、写入 changelist 描述、提交。

    把 Perforce 的 spec 文本格式封装掉，调用方只需传纯文本描述，
    避免在命令层手工拼接 changelist spec 时出错。

    Action:
      Collect  汇总待提交文件（超量时源码全列、资产按目录聚合）
      Prepare  将描述写入 changelist，返回其编号
      Submit   提交指定 changelist
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Collect', 'Prepare', 'Submit')]
    [string]$Action,

    # Prepare：描述正文所在的文件（UTF-8），避免命令行引号转义
    [string]$DescriptionFile,

    # 目标 changelist 编号；省略则取 default
    [string]$Change
)

$ErrorActionPreference = 'Stop'

# PATH 最前的 System32\p4 是 0 字节占位文件，必须用绝对路径
function Resolve-P4Exe {
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
    throw 'p4.exe not found'
}

$p4 = Resolve-P4Exe
$SourceExtensions = @('.h', '.cpp', '.cs', '.as', '.ini', '.md', '.py', '.ps1', '.uplugin', '.uproject')

function Get-OpenedFiles {
    $target = if ($Change) { $Change } else { 'default' }
    $raw = @(& $p4 opened -c $target 2>&1)

    $files = @()
    foreach ($line in $raw) {
        # //depot/path/file.cpp#3 - edit change 123 (text)
        if ($line -match '^(?<depot>//[^#]+)#(?<rev>\d+)\s-\s(?<action>\w+)\s') {
            $files += [pscustomobject]@{
                Depot  = $Matches.depot
                Action = $Matches.action
                Ext    = [System.IO.Path]::GetExtension($Matches.depot).ToLower()
            }
        }
    }
    return $files
}

switch ($Action) {

    'Collect' {
        $files = Get-OpenedFiles
        if ($files.Count -eq 0) {
            Write-Output 'EMPTY'
            exit 0
        }

        Write-Output "TOTAL: $($files.Count)"
        Write-Output '--- BY ACTION ---'
        $files | Group-Object Action | Sort-Object Count -Descending | ForEach-Object {
            Write-Output "$($_.Name): $($_.Count)"
        }

        $source = @($files | Where-Object { $SourceExtensions -contains $_.Ext })
        $assets = @($files | Where-Object { $SourceExtensions -notcontains $_.Ext })

        # 源码是描述的主要依据，始终逐条列出
        if ($source.Count -gt 0) {
            Write-Output '--- SOURCE ---'
            $source | Sort-Object Depot | ForEach-Object {
                Write-Output "$($_.Action)`t$($_.Depot)"
            }
        }

        if ($assets.Count -gt 0) {
            Write-Output '--- ASSETS ---'
            if ($assets.Count -le 60) {
                $assets | Sort-Object Depot | ForEach-Object {
                    Write-Output "$($_.Action)`t$($_.Depot)"
                }
            } else {
                # 资产量大时逐条列出没有信息增量，按目录聚合
                $assets | Group-Object { Split-Path $_.Depot -Parent } |
                    Sort-Object Count -Descending | ForEach-Object {
                        Write-Output "$($_.Count)`t$($_.Name -replace '\\', '/')"
                    }
            }
        }
    }

    'Prepare' {
        if (-not $DescriptionFile -or -not (Test-Path -LiteralPath $DescriptionFile)) {
            throw 'DescriptionFile is required and must exist'
        }

        $descLines = @(Get-Content -LiteralPath $DescriptionFile -Encoding utf8)
        if (($descLines -join '').Trim().Length -eq 0) {
            throw 'Description is empty'
        }

        # p4 change -o 不接受 default 字面量，省略参数才是 default changelist
        $spec = if ($Change) {
            @(& $p4 change -o $Change 2>&1)
        } else {
            @(& $p4 change -o 2>&1)
        }

        $out = New-Object System.Collections.Generic.List[string]
        $inDescription = $false
        foreach ($line in $spec) {
            if ($line -match '^Description:') {
                $out.Add('Description:')
                foreach ($d in $descLines) {
                    # spec 要求描述正文缩进
                    $out.Add("`t$d")
                }
                $inDescription = $true
                continue
            }
            if ($inDescription) {
                # 缩进行属于原描述，丢弃；空行标志该段结束
                if ($line -eq '') {
                    $inDescription = $false
                    $out.Add('')
                }
                continue
            }
            $out.Add($line)
        }

        $result = ($out -join "`n") | & $p4 change -i 2>&1
        $text = ($result | Out-String).Trim()
        Write-Output $text

        # Change 123 created with 42 open file(s).
        if ($text -match 'Change\s+(\d+)') {
            Write-Output "CHANGELIST: $($Matches[1])"
        }
    }

    'Submit' {
        if (-not $Change) {
            throw 'Change is required'
        }
        $result = & $p4 submit -c $Change 2>&1
        Write-Output (($result | Out-String).Trim())
        Write-Output "EXITCODE: $LASTEXITCODE"
    }
}
