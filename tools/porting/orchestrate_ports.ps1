[CmdletBinding()]
param(
    [string[]]$Targets = @( "cdda-master", "cdda-0.H", "cdda-0.I", "ctlg-master" ),
    [switch]$RunCodex,
    [switch]$SkipBuild,
    [switch]$NoFetch,
    [switch]$NoBackup,
    [switch]$AllowDirty,
    [switch]$DryRun,
    [string]$CtglRemoteUrl = "https://github.com/Cataclysm-TLG/Cataclysm-TLG.git",
    [string]$CodexModel = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step {
    param( [string]$Message )
    Write-Host ""
    Write-Host "[porting] $Message"
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)] [string]$FilePath,
        [Parameter(Mandatory = $true)] [string[]]$Arguments,
        [switch]$IgnoreFailure
    )
    if( $DryRun ) {
        $isReadOnlyInDryRun = $false
        if( $FilePath -eq "git" -and $Arguments.Count -gt 0 ) {
            $sub = $Arguments[0]
            switch( $sub ) {
                "rev-parse" { $isReadOnlyInDryRun = $true }
                "status" { $isReadOnlyInDryRun = $true }
                "show-ref" { $isReadOnlyInDryRun = $true }
                "log" { $isReadOnlyInDryRun = $true }
                "remote" {
                    if( $Arguments.Count -eq 1 ) {
                        $isReadOnlyInDryRun = $true
                    }
                }
                "config" {
                    if( $Arguments -contains "--get" -or $Arguments -contains "--get-all" ) {
                        $isReadOnlyInDryRun = $true
                    }
                }
            }
        }
        if( -not $isReadOnlyInDryRun ) {
            Write-Host "DRY-RUN: $FilePath $($Arguments -join ' ')"
            return [PSCustomObject]@{
                ExitCode = 0
                Output = @()
            }
        }
    }
    $output = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if( $exitCode -ne 0 -and -not $IgnoreFailure ) {
        $rendered = [string]::Join( [Environment]::NewLine, $output )
        throw "Command failed ($exitCode): $FilePath $($Arguments -join ' ')`n$rendered"
    }
    $normalizedOutput = if( $null -eq $output ) {
        @()
    } else {
        [string[]]$output
    }
    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = $normalizedOutput
    }
}

function Invoke-CmdLogged {
    param(
        [Parameter(Mandatory = $true)] [string]$CommandLine,
        [Parameter(Mandatory = $true)] [string]$LogFile,
        [switch]$IgnoreFailure
    )
    if( $DryRun ) {
        Write-Host "DRY-RUN: cmd /d /c $CommandLine > $LogFile 2>&1"
        return 0
    }
    $wrapped = "$CommandLine > `"$LogFile`" 2>&1"
    & cmd.exe /d /c $wrapped
    $exitCode = $LASTEXITCODE
    if( $exitCode -ne 0 -and -not $IgnoreFailure ) {
        throw "Command failed ($exitCode): $CommandLine (see $LogFile)"
    }
    return $exitCode
}

function Test-RefExists {
    param( [Parameter(Mandatory = $true)] [string]$RefName )
    $probe = Invoke-External -FilePath "git" -Arguments @( "show-ref", "--verify", "--quiet", $RefName ) -IgnoreFailure
    return $probe.ExitCode -eq 0
}

function Resolve-BackupBranchName {
    param( [Parameter(Mandatory = $true)] [string]$BaseName )
    $candidate = $BaseName
    $i = 1
    while( Test-RefExists -RefName "refs/heads/$candidate" ) {
        $candidate = "$BaseName-$i"
        $i++
    }
    return $candidate
}

function New-CodexPromptFile {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$Title,
        [Parameter(Mandatory = $true)] [string]$Body
    )
    $prompt = @"
# $Title

Read these first:
- `Agents.md`
- `tools/porting/PORTING_CONTEXT.md`
- `Plan.md`

$Body
"@
    if( $DryRun ) {
        Write-Host "DRY-RUN: write prompt $Path"
        return
    }
    $prompt | Out-File -FilePath $Path -Encoding utf8
}

function Invoke-CodexExec {
    param(
        [Parameter(Mandatory = $true)] [string]$PromptFile,
        [Parameter(Mandatory = $true)] [string]$CodexLogFile,
        [Parameter(Mandatory = $true)] [string]$LastMessageFile,
        [Parameter(Mandatory = $true)] [string]$RepoRootPath
    )
    if( -not $RunCodex ) {
        return $false
    }
    $codexCmd = Get-Command codex -ErrorAction SilentlyContinue
    if( $null -eq $codexCmd ) {
        throw "RunCodex was requested, but `codex` is not available in PATH."
    }
    $args = New-Object System.Collections.Generic.List[string]
    [void]$args.Add( "exec" )
    [void]$args.Add( "--full-auto" )
    [void]$args.Add( "--cd" )
    [void]$args.Add( $RepoRootPath )
    [void]$args.Add( "--output-last-message" )
    [void]$args.Add( $LastMessageFile )
    if( -not [string]::IsNullOrWhiteSpace( $CodexModel ) ) {
        [void]$args.Add( "-m" )
        [void]$args.Add( $CodexModel )
    }
    [void]$args.Add( "-" )

    if( $DryRun ) {
        Write-Host "DRY-RUN: codex $($args -join ' ') < $PromptFile > $CodexLogFile"
        return $true
    }

    Get-Content -Raw -Path $PromptFile | & codex @args *> $CodexLogFile
    return ( $LASTEXITCODE -eq 0 )
}

$repoRootResult = Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--show-toplevel" )
$repoRoot = $repoRootResult.Output[-1].Trim()
if( [string]::IsNullOrWhiteSpace( $repoRoot ) ) {
    throw "Could not resolve repo root."
}
Set-Location $repoRoot

$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runDir = Join-Path $repoRoot ( Join-Path "tools/porting/logs" $runStamp )
if( -not $DryRun ) {
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
}

$targetMap = @{
    "cdda-master" = [PSCustomObject]@{
        Name = "cdda-master"
        Branch = "port/cdda-master"
        UpstreamRef = "upstream/master"
    }
    "cdda-0.H" = [PSCustomObject]@{
        Name = "cdda-0.H"
        Branch = "port/cdda-0.H"
        UpstreamRef = "upstream/0.H-branch"
    }
    "cdda-0.I" = [PSCustomObject]@{
        Name = "cdda-0.I"
        Branch = "port/cdda-0.I"
        UpstreamRef = "upstream/0.I-branch"
    }
    "ctlg-master" = [PSCustomObject]@{
        Name = "ctlg-master"
        Branch = "port/ctlg-master"
        UpstreamRef = "upstream-ctlg/master"
    }
}

$selectedTargets = New-Object System.Collections.Generic.List[object]
foreach( $target in $Targets ) {
    if( -not $targetMap.ContainsKey( $target ) ) {
        throw "Unknown target '$target'. Allowed: $($targetMap.Keys -join ', ')"
    }
    [void]$selectedTargets.Add( $targetMap[$target] )
}

Write-Step "Preflight checks"
$currentBranch = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--abbrev-ref", "HEAD" ) ).Output[-1].Trim()
if( $currentBranch -ne "master" ) {
    throw "This orchestrator must be started on branch 'master'. Current branch: '$currentBranch'."
}

$dirty = ( Invoke-External -FilePath "git" -Arguments @( "status", "--porcelain" ) ).Output
if( $dirty.Count -gt 0 -and -not $AllowDirty ) {
    throw "Working tree is dirty. Commit/stash first, or re-run with -AllowDirty."
}

$remotes = ( Invoke-External -FilePath "git" -Arguments @( "remote" ) ).Output | ForEach-Object {
    $_.Trim()
} | Where-Object { $_ -ne "" }
if( -not ( $remotes -contains "origin" ) ) {
    throw "Missing required remote: origin"
}
if( -not ( $remotes -contains "upstream" ) ) {
    throw "Missing required remote: upstream (CDDA)"
}

Write-Step "Ensuring CTLG remote configuration"
if( $remotes -contains "upstream-ctlg" ) {
    Invoke-External -FilePath "git" -Arguments @( "remote", "set-url", "upstream-ctlg", $CtglRemoteUrl ) | Out-Null
} else {
    Invoke-External -FilePath "git" -Arguments @( "remote", "add", "upstream-ctlg", $CtglRemoteUrl ) | Out-Null
}
Invoke-External -FilePath "git" -Arguments @( "config", "--replace-all", "remote.upstream-ctlg.fetch", "+refs/heads/master:refs/remotes/upstream-ctlg/master" ) | Out-Null
Invoke-External -FilePath "git" -Arguments @( "config", "remote.upstream-ctlg.tagOpt", "--no-tags" ) | Out-Null

if( -not $NoFetch ) {
    Write-Step "Fetching remotes"
    Invoke-External -FilePath "git" -Arguments @( "fetch", "origin", "--prune" ) | Out-Null
    Invoke-External -FilePath "git" -Arguments @( "fetch", "upstream", "--prune" ) | Out-Null
    Invoke-External -FilePath "git" -Arguments @( "fetch", "upstream-ctlg", "--prune" ) | Out-Null
}

foreach( $target in $selectedTargets ) {
    $probe = Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", $target.UpstreamRef ) -IgnoreFailure
    if( $probe.ExitCode -ne 0 ) {
        throw "Missing upstream ref for target '$($target.Name)': $($target.UpstreamRef)"
    }
}

if( -not $NoBackup ) {
    Write-Step "Creating backup branch from master"
    $backupBase = "backup/master-pre-port-$runStamp"
    $backupBranch = Resolve-BackupBranchName -BaseName $backupBase
    Invoke-External -FilePath "git" -Arguments @( "branch", $backupBranch, "master" ) | Out-Null
    Write-Host "[porting] backup branch created: $backupBranch"
}

$summary = New-Object System.Collections.Generic.List[object]

foreach( $target in $selectedTargets ) {
    Write-Step "Target: $($target.Name)"
    $targetRunDir = Join-Path $runDir $target.Name
    if( -not $DryRun ) {
        New-Item -ItemType Directory -Path $targetRunDir -Force | Out-Null
    }

    Invoke-External -FilePath "git" -Arguments @( "checkout", "master" ) | Out-Null

    if( Test-RefExists -RefName "refs/heads/$($target.Branch)" ) {
        Invoke-External -FilePath "git" -Arguments @( "branch", "-D", $target.Branch ) | Out-Null
    }

    Invoke-External -FilePath "git" -Arguments @( "checkout", "-B", $target.Branch, $target.UpstreamRef ) | Out-Null

    $mergeLogFile = Join-Path $targetRunDir "merge.log"
    $mergeResult = Invoke-External -FilePath "git" -Arguments @( "merge", "--no-ff", "--no-edit", "master" ) -IgnoreFailure
    if( -not $DryRun ) {
        $mergeResult.Output | Out-File -FilePath $mergeLogFile -Encoding utf8
    }

    $mergeOk = $mergeResult.ExitCode -eq 0
    $codexUsedForMerge = $false
    if( -not $mergeOk ) {
        $conflicts = ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output
        if( $RunCodex ) {
            $promptFile = Join-Path $targetRunDir "codex-merge-prompt.md"
            $codexLogFile = Join-Path $targetRunDir "codex-merge.log"
            $lastMessageFile = Join-Path $targetRunDir "codex-merge-last-message.txt"
            $conflictText = if( $conflicts.Count -eq 0 ) { "(none listed)" } else { [string]::Join( [Environment]::NewLine, $conflicts ) }
            $promptBody = @"
You are on branch `$($target.Branch)` after trying to merge `master` into `$($target.UpstreamRef)`.
The merge failed and needs to be completed while preserving AOL LLM behavior parity.

Conflict files:
$conflictText

Requirements:
1. Resolve merge conflicts and finish the merge commit.
2. Preserve AOL LLM feature behavior parity.
3. Keep changes focused to porting and required fixes only.
4. Run these checks after fixing:
   - `just_build.cmd --unclean > debug.txt 2>&1`
   - `just_build_linux.cmd --unclean > debug.txt 2>&1`
5. Summarize what you changed and any residual risk.
"@
            New-CodexPromptFile -Path $promptFile -Title "Merge fix for $($target.Name)" -Body $promptBody
            $codexUsedForMerge = Invoke-CodexExec -PromptFile $promptFile -CodexLogFile $codexLogFile -LastMessageFile $lastMessageFile -RepoRootPath $repoRoot
        }

        $remaining = ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output
        $mergeHead = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", "-q", "MERGE_HEAD" ) -IgnoreFailure ).ExitCode -eq 0
        if( $remaining.Count -eq 0 -and $mergeHead ) {
            $commitResult = Invoke-External -FilePath "git" -Arguments @( "commit", "--no-edit" ) -IgnoreFailure
            if( $commitResult.ExitCode -ne 0 ) {
                $mergeOk = $false
            } else {
                $mergeOk = $true
            }
        } else {
            $mergeOk = ( $remaining.Count -eq 0 -and -not $mergeHead )
        }

        if( -not $mergeOk ) {
            if( $mergeHead ) {
                Invoke-External -FilePath "git" -Arguments @( "merge", "--abort" ) -IgnoreFailure | Out-Null
            }
            [void]$summary.Add( [PSCustomObject]@{
                    Target = $target.Name
                    Branch = $target.Branch
                    Merge = "FAILED"
                    CodexMerge = $( if( $codexUsedForMerge ) { "USED" } else { "NO" } )
                    Build = "SKIPPED"
                    Notes = "Merge unresolved. See $targetRunDir"
                } )
            continue
        }
    }

    $windowsBuild = "SKIPPED"
    $linuxBuild = "SKIPPED"
    $buildNotes = ""

    if( -not $SkipBuild ) {
        $winLog = Join-Path $targetRunDir "build-windows.log"
        $linLog = Join-Path $targetRunDir "build-linux.log"

        $winExit = Invoke-CmdLogged -CommandLine "just_build.cmd --unclean" -LogFile $winLog -IgnoreFailure
        $windowsBuild = if( $winExit -eq 0 ) { "OK" } else { "FAIL" }
        if( $winExit -ne 0 -and $RunCodex ) {
            $promptFile = Join-Path $targetRunDir "codex-build-windows-prompt.md"
            $codexLogFile = Join-Path $targetRunDir "codex-build-windows.log"
            $lastMessageFile = Join-Path $targetRunDir "codex-build-windows-last-message.txt"
            $promptBody = @"
Windows build failed on branch `$($target.Branch)`.
Read log file: `$winLog`

Task:
1. Fix build issues on this branch.
2. Keep AOL LLM behavior parity.
3. Re-run Windows build:
   - `just_build.cmd --unclean > debug.txt 2>&1`
4. Summarize fixes and remaining risks.
"@
            New-CodexPromptFile -Path $promptFile -Title "Windows build fix for $($target.Name)" -Body $promptBody
            [void]( Invoke-CodexExec -PromptFile $promptFile -CodexLogFile $codexLogFile -LastMessageFile $lastMessageFile -RepoRootPath $repoRoot )
            $winExit = Invoke-CmdLogged -CommandLine "just_build.cmd --unclean" -LogFile $winLog -IgnoreFailure
            $windowsBuild = if( $winExit -eq 0 ) { "OK" } else { "FAIL" }
        }

        $linExit = Invoke-CmdLogged -CommandLine "just_build_linux.cmd --unclean" -LogFile $linLog -IgnoreFailure
        $linuxBuild = if( $linExit -eq 0 ) { "OK" } else { "FAIL" }
        if( $linExit -ne 0 -and $RunCodex ) {
            $promptFile = Join-Path $targetRunDir "codex-build-linux-prompt.md"
            $codexLogFile = Join-Path $targetRunDir "codex-build-linux.log"
            $lastMessageFile = Join-Path $targetRunDir "codex-build-linux-last-message.txt"
            $promptBody = @"
Linux build failed on branch `$($target.Branch)`.
Read log file: `$linLog`

Task:
1. Fix build issues on this branch.
2. Keep AOL LLM behavior parity.
3. Re-run Linux build:
   - `just_build_linux.cmd --unclean > debug.txt 2>&1`
4. Summarize fixes and remaining risks.
"@
            New-CodexPromptFile -Path $promptFile -Title "Linux build fix for $($target.Name)" -Body $promptBody
            [void]( Invoke-CodexExec -PromptFile $promptFile -CodexLogFile $codexLogFile -LastMessageFile $lastMessageFile -RepoRootPath $repoRoot )
            $linExit = Invoke-CmdLogged -CommandLine "just_build_linux.cmd --unclean" -LogFile $linLog -IgnoreFailure
            $linuxBuild = if( $linExit -eq 0 ) { "OK" } else { "FAIL" }
        }

        if( $windowsBuild -ne "OK" -or $linuxBuild -ne "OK" ) {
            $buildNotes = "Check logs in $targetRunDir"
        }
    }

    [void]$summary.Add( [PSCustomObject]@{
            Target = $target.Name
            Branch = $target.Branch
            Merge = "OK"
            CodexMerge = $( if( $codexUsedForMerge ) { "USED" } else { "NO" } )
            Build = "$( $windowsBuild )/$( $linuxBuild )"
            Notes = $buildNotes
        } )
}

Invoke-External -FilePath "git" -Arguments @( "checkout", "master" ) -IgnoreFailure | Out-Null

Write-Step "Summary"
$summary | Format-Table -AutoSize

$summaryFile = Join-Path $runDir "summary.txt"
if( -not $DryRun ) {
    $summary | Format-Table -AutoSize | Out-String | Out-File -FilePath $summaryFile -Encoding utf8
}

Write-Host ""
Write-Host "[porting] logs directory: $runDir"
Write-Host "[porting] next manual step: check out each port/* branch and package release artifacts."
