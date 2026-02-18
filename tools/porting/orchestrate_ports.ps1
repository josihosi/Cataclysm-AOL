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
    [string]$CodexModel = "",
    [string]$PatchsetFile = "tools/porting/patchsets/common.txt",
    [switch]$NoTargetPatchsets,
    [string]$PatchsetCommitRange = "",
    [string[]]$PatchsetPathFilter = @(),
    [int]$MaxConflictFiles = 100
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
                "rev-list" { $isReadOnlyInDryRun = $true }
                "cat-file" { $isReadOnlyInDryRun = $true }
                "diff" { $isReadOnlyInDryRun = $true }
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
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $FilePath @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
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

function Read-CommitListFile {
    param( [Parameter(Mandatory = $true)] [string]$FilePath )
    if( -not ( Test-Path -LiteralPath $FilePath ) ) {
        return @()
    }
    return @(
        ( Get-Content -Path $FilePath ) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" -and -not $_.StartsWith( "#" ) }
    )
}

function Select-TargetPatchsetFile {
    param(
        [Parameter(Mandatory = $true)] [string]$RepoRoot,
        [Parameter(Mandatory = $true)] [string]$TargetName
    )
    $candidate = Join-Path $RepoRoot ( Join-Path "tools/porting/patchsets" ( "$TargetName.txt" ) )
    if( Test-Path -LiteralPath $candidate ) {
        return $candidate
    }
    return ""
}

function Resolve-CommitRangeList {
    param(
        [Parameter(Mandatory = $true)] [string]$RangeSpec,
        [string[]]$Paths = @()
    )
    $args = New-Object System.Collections.Generic.List[string]
    [void]$args.Add( "rev-list" )
    [void]$args.Add( "--reverse" )
    [void]$args.Add( $RangeSpec )
    if( $Paths.Count -gt 0 ) {
        [void]$args.Add( "--" )
        foreach( $path in $Paths ) {
            [void]$args.Add( $path )
        }
    }
    return @(
        @( ( Invoke-External -FilePath "git" -Arguments $args.ToArray() ).Output ) |
        Where-Object { $_ -ne "" }
    )
}

function Get-UniqueOrdered {
    param( [string[]]$Items = @() )
    $seen = New-Object "System.Collections.Generic.HashSet[string]"
    $result = New-Object System.Collections.Generic.List[string]
    foreach( $item in $Items ) {
        if( $seen.Add( $item ) ) {
            [void]$result.Add( $item )
        }
    }
    $array = $result.ToArray()
    if( $null -eq $array ) {
        return @()
    }
    return @( $array )
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
$repoRoot = @( $repoRootResult.Output )[-1].Trim()
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
$currentBranch = @( ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--abbrev-ref", "HEAD" ) ).Output )[-1].Trim()
if( $currentBranch -ne "master" ) {
    throw "This orchestrator must be started on branch 'master'. Current branch: '$currentBranch'."
}

$dirty = @( ( Invoke-External -FilePath "git" -Arguments @( "status", "--porcelain" ) ).Output )
if( $dirty.Count -gt 0 -and -not $AllowDirty ) {
    throw "Working tree is dirty. Commit/stash first, or re-run with -AllowDirty."
}

$remotes = @( ( Invoke-External -FilePath "git" -Arguments @( "remote" ) ).Output ) | ForEach-Object {
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

$baseQueue = @()
Write-Step "Building patchset queue"
if( -not [string]::IsNullOrWhiteSpace( $PatchsetCommitRange ) ) {
    $baseQueue = @( Resolve-CommitRangeList -RangeSpec $PatchsetCommitRange -Paths $PatchsetPathFilter )
    Write-Host "[porting] patchset source: commit range $PatchsetCommitRange ($($baseQueue.Count) commits)"
} else {
    $basePatchsetPath = Join-Path $repoRoot $PatchsetFile
    $baseQueue = @( Read-CommitListFile -FilePath $basePatchsetPath )
    Write-Host "[porting] patchset source: file $PatchsetFile ($($baseQueue.Count) commits)"
}
if( $baseQueue.Count -eq 0 ) {
    Write-Host "[porting] WARNING: patchset queue is empty. No AOL commits will be replayed unless target-specific files are populated."
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

    $targetQueue = New-Object System.Collections.Generic.List[string]
    foreach( $sha in $baseQueue ) {
        [void]$targetQueue.Add( $sha )
    }
    if( -not $NoTargetPatchsets ) {
        $targetPatchset = Select-TargetPatchsetFile -RepoRoot $repoRoot -TargetName $target.Name
        if( -not [string]::IsNullOrWhiteSpace( $targetPatchset ) ) {
            $extraQueue = @( Read-CommitListFile -FilePath $targetPatchset )
            foreach( $sha in $extraQueue ) {
                [void]$targetQueue.Add( $sha )
            }
        }
    }
    $commitQueue = @( Get-UniqueOrdered -Items $targetQueue.ToArray() )
    foreach( $sha in $commitQueue ) {
        $exists = Invoke-External -FilePath "git" -Arguments @( "cat-file", "-e", "$sha^{commit}" ) -IgnoreFailure
        if( $exists.ExitCode -ne 0 ) {
            throw "Invalid commit in patchset for target '$($target.Name)': $sha"
        }
    }

    $queueFile = Join-Path $targetRunDir "commit-queue.txt"
    if( -not $DryRun ) {
        if( $commitQueue.Count -eq 0 ) {
            "# no commits queued" | Out-File -FilePath $queueFile -Encoding utf8
        } else {
            $commitQueue | Out-File -FilePath $queueFile -Encoding utf8
        }
    }

    $applyLogFile = Join-Path $targetRunDir "apply.log"
    $appliedCount = 0
    $applyOk = $true
    $codexUsedForApply = $false
    $failedCommit = ""
    $failureNotes = ""

    foreach( $sha in $commitQueue ) {
        $pickArgs = @( "cherry-pick", "--allow-empty", "-x", $sha )
        $pickResult = Invoke-External -FilePath "git" -Arguments $pickArgs -IgnoreFailure
        if( -not $DryRun ) {
            @(
                "## cherry-pick $sha"
                [string]::Join( [Environment]::NewLine, $pickResult.Output )
                ""
            ) | Out-File -FilePath $applyLogFile -Append -Encoding utf8
        }
        if( $pickResult.ExitCode -eq 0 ) {
            $appliedCount++
            continue
        }

        $failedCommit = $sha
        $conflicts = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
        if( $conflicts.Count -gt $MaxConflictFiles ) {
            $failureNotes = "Conflict threshold exceeded ($($conflicts.Count) files > $MaxConflictFiles). Manual port required."
            $applyOk = $false
        } elseif( $RunCodex ) {
            $shortSha = if( $sha.Length -gt 12 ) { $sha.Substring( 0, 12 ) } else { $sha }
            $promptFile = Join-Path $targetRunDir "codex-apply-$shortSha-prompt.md"
            $codexLogFile = Join-Path $targetRunDir "codex-apply-$shortSha.log"
            $lastMessageFile = Join-Path $targetRunDir "codex-apply-$shortSha-last-message.txt"
            $conflictText = if( $conflicts.Count -eq 0 ) { "(none listed)" } else { [string]::Join( [Environment]::NewLine, $conflicts ) }
            $promptBody = @"
You are on branch `$($target.Branch)` after cherry-picking commit `$sha` onto base `$($target.UpstreamRef)`.
Cherry-pick failed and must be completed while preserving AOL LLM behavior parity.

Conflict files:
$conflictText

Requirements:
1. Resolve conflicts for this cherry-pick and run `git cherry-pick --continue`.
2. Keep changes focused to AOL porting behavior parity.
3. Do not run full builds yet; orchestrator will run builds after patchset replay.
4. Summarize any manual decisions needed for future patchset curation.
"@
            New-CodexPromptFile -Path $promptFile -Title "Patchset apply fix for $($target.Name) $shortSha" -Body $promptBody
            $codexUsedForApply = Invoke-CodexExec -PromptFile $promptFile -CodexLogFile $codexLogFile -LastMessageFile $lastMessageFile -RepoRootPath $repoRoot
        }

        $remaining = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
        $cherryPickHead = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", "-q", "CHERRY_PICK_HEAD" ) -IgnoreFailure ).ExitCode -eq 0
        if( $remaining.Count -eq 0 -and $cherryPickHead ) {
            $continueResult = Invoke-External -FilePath "git" -Arguments @( "cherry-pick", "--continue" ) -IgnoreFailure
            if( $continueResult.ExitCode -eq 0 ) {
                $appliedCount++
                continue
            }
            $applyOk = $false
            if( [string]::IsNullOrWhiteSpace( $failureNotes ) ) {
                $failureNotes = "cherry-pick --continue failed for $sha"
            }
        } elseif( $remaining.Count -eq 0 -and -not $cherryPickHead ) {
            # cherry-pick may have been completed manually by codex
            $appliedCount++
            continue
        } else {
            $applyOk = $false
            if( [string]::IsNullOrWhiteSpace( $failureNotes ) ) {
                $failureNotes = "Cherry-pick unresolved for $sha"
            }
        }

        if( $cherryPickHead ) {
            Invoke-External -FilePath "git" -Arguments @( "cherry-pick", "--abort" ) -IgnoreFailure | Out-Null
        }
        break
    }

    if( -not $applyOk ) {
        [void]$summary.Add( [PSCustomObject]@{
                Target = $target.Name
                Branch = $target.Branch
                Queue = $commitQueue.Count
                Applied = $appliedCount
                Apply = "FAILED"
                CodexApply = $( if( $codexUsedForApply ) { "USED" } else { "NO" } )
                Build = "SKIPPED"
                Notes = "$( $failureNotes ) See $targetRunDir"
            } )
        continue
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
            Queue = $commitQueue.Count
            Applied = $appliedCount
            Apply = "OK"
            CodexApply = $( if( $codexUsedForApply ) { "USED" } else { "NO" } )
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
