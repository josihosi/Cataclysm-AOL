<#
.SYNOPSIS
Orchestrates AOL port branches against upstream branches with merge-first fallback behavior.

.DESCRIPTION
Runs a repeatable porting flow for one or more targets:
1. Optional fetch/update of remotes.
2. Optional backup branch management for master.
3. For each target branch, run upstream native merge (upstream -> port branch).
4. Sync master changes using the selected strategy:
   - delta-cherry-pick (default): replay only master commits not yet present on target.
   - native-merge: attempt native master -> port merge.
5. If configured, destructive rebuild fallback can recreate target branch and replay patchsets.
5. Optionally run Windows/Linux builds and summarize results in tools/porting/logs/<timestamp>.

This script must be started from branch 'master'.

.PARAMETER Targets
Port targets to process. Allowed values: cdda-master, cdda-0.H, cdda-0.I, ctlg-master.

.PARAMETER RunCodex
Enable Codex-assisted conflict/build fixes when automatic resolution is insufficient.

.PARAMETER NoCodex
Disable Codex-assisted conflict/build fixes. Codex is enabled by default.

.PARAMETER Mode
Execution mode:
- FullPort: run upstream lane and CAOL lane.
- UpstreamPort: run only upstream lane.
- CAOLPort: run only CAOL lane.
- Audit: report both lanes and planned route, no writes.

.PARAMETER SkipBuild
Skip Windows/Linux build steps after merge/apply.

.PARAMETER NoFetch
Do not fetch remotes before processing targets.

.PARAMETER NoBackup
Skip backup branch creation/reuse checks.

.PARAMETER AllowDirty
Allow running with a dirty working tree.

.PARAMETER DryRun
Simulate write operations. Useful for previewing actions and flow.
Note: merge/apply/build outcomes are simulated and reported as DRYRUN.

.PARAMETER AuditMode
Compatibility alias for `-Mode Audit`.
Run non-destructive audit for upstream merge and selected CAOL sync strategy.
Does not recreate branches, does not cherry-pick, and does not build.

.PARAMETER MasterSyncMode
CAOL sync strategy after upstream merge:
- delta-cherry-pick (default): replay only missing master commits.
- native-merge: perform native merge from master.

.PARAMETER MasterDeltaPathFilter
Optional path filters for CAOL delta-cherry-pick commit selection.

.PARAMETER MasterDeltaIgnorePathGlobs
Commits whose changed files all match these globs are skipped in delta-cherry-pick mode.

.PARAMETER AolDeltaMaxQueue
Maximum delta queue size before AOL lane falls back to patchset queue.

.PARAMETER AolDeltaPatchsetInflationRatio
Fallback to patchset when delta queue is this many times larger than patchset queue.

.PARAMETER AolSkipDeltaTargets
Targets that skip expensive AOL delta scanning and directly use patchset queue.
Default: cdda-0.H, cdda-0.I, ctlg-master.

.PARAMETER AolSourceRef
Source ref for CAOL lane. Defaults to `master`, later can be switched to `port/cdda-master`.

.PARAMETER AllowDestructiveFallback
Allow deleting/recreating existing port branches when non-destructive path cannot continue.

.PARAMETER CtglRemoteUrl
Remote URL used for upstream-ctlg.

.PARAMETER CodexModel
Optional model override passed to codex exec when -RunCodex is used.

.PARAMETER PatchsetFile
Path to base patchset file (relative to repo root by default).

.PARAMETER NoTargetPatchsets
Disable target-specific patchset overlays (tools/porting/patchsets/<target>.txt).

.PARAMETER PatchsetCommitRange
Optional commit range to build queue from instead of patchset file (e.g. A..B).

.PARAMETER PatchsetPathFilter
Optional path filters used with -PatchsetCommitRange.

.PARAMETER MaxConflictFiles
Upper conflict-file threshold for cherry-pick stage before failing fast.

.PARAMETER NativeMergeMaxConflicts
Upper conflict-file threshold for native merge stage before falling back to cherry-pick.

.PARAMETER BackupDiffIgnorePaths
Paths ignored when deciding whether an existing backup branch is equivalent to master.

.PARAMETER AutoResolveOursPaths
Paths auto-resolved with --ours during conflicts before optional Codex assist.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -DryRun -AllowDirty
Preview full run flow without writing repository state.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Targets cdda-master -SkipBuild -NoBackup
Process only cdda-master, skip build steps, and skip backup handling.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Targets cdda-0.I -RunCodex -CodexModel gpt-5.3-codex
Run one target with Codex conflict/build assistance.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Mode FullPort -Targets cdda-master
Run both lanes (upstream + CAOL) for cdda-master.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Mode UpstreamPort -Targets cdda-0.H,cdda-0.I
Run only upstream lane for 0.H and 0.I targets.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Mode CAOLPort -AolSourceRef master -Targets cdda-0.H
Run only CAOL lane from master for cdda-0.H.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Targets cdda-master -PatchsetCommitRange 1234abcd..89ef0123
Use a commit-range queue instead of the patchset file.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -NoFetch -NativeMergeMaxConflicts 25 -MaxConflictFiles 100
Reuse current fetched refs and tune merge/cherry-pick conflict thresholds.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -AuditMode -AllowDirty -NoBackup
Audit upstream + master sync pressure for all targets without rewriting port branches.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Targets cdda-master -MasterSyncMode delta-cherry-pick -SkipBuild
Merge upstream into port/cdda-master, then replay only missing master commits.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -Targets cdda-master -MasterSyncMode native-merge -AllowDestructiveFallback
Use legacy native master merge path and permit destructive fallback when required.

.EXAMPLE
.\tools\porting\orchestrate_ports.ps1 -AuditMode -AolDeltaMaxQueue 120 -AolDeltaPatchsetInflationRatio 4
Audit with stricter AOL delta->patchset fallback thresholds.
#>
[CmdletBinding()]
param(
    [string[]]$Targets = @( "cdda-master", "cdda-0.H", "cdda-0.I", "ctlg-master" ),
    [switch]$RunCodex,
    [switch]$NoCodex,
    [ValidateSet( "FullPort", "UpstreamPort", "CAOLPort", "Audit" )]
    [string]$Mode = "FullPort",
    [switch]$SkipBuild,
    [switch]$NoFetch,
    [switch]$NoBackup,
    [switch]$AllowDirty,
    [switch]$DryRun,
    [switch]$AuditMode,
    [string]$AolSourceRef = "master",
    [ValidateSet( "delta-cherry-pick", "native-merge" )]
    [string]$MasterSyncMode = "delta-cherry-pick",
    [string[]]$MasterDeltaPathFilter = @(
        "src/do_turn.cpp",
        "src/llm_intent.cpp",
        "src/llm_intent.h",
        "src/map.cpp",
        "src/npc.cpp",
        "src/npc.h",
        "src/npcmove.cpp",
        "src/npctalk.cpp",
        "src/npctalk_rules.cpp",
        "src/options.cpp",
        "src/options.h",
        "tools/llm_runner/background_summarizer.py",
        "tools/llm_runner/prompt_playground.py",
        "tools/llm_runner/runner.py"
    ),
    [string[]]$MasterDeltaIgnorePathGlobs = @(
        "Plan.md",
        "README.md",
        "TechnicalTome.md",
        "Agent.md",
        "Agents.md",
        ".gitignore",
        "tools/porting/*"
    ),
    [ValidateRange( 1, 200000 )]
    [int]$AolDeltaMaxQueue = 250,
    [ValidateRange( 1, 1000 )]
    [int]$AolDeltaPatchsetInflationRatio = 5,
    [string[]]$AolSkipDeltaTargets = @( "cdda-0.H", "cdda-0.I", "ctlg-master" ),
    [switch]$AllowDestructiveFallback,
    [string]$CtglRemoteUrl = "https://github.com/Cataclysm-TLG/Cataclysm-TLG.git",
    [string]$CodexModel = "",
    [string]$PatchsetFile = "tools/porting/patchsets/common.txt",
    [switch]$NoTargetPatchsets,
    [string]$PatchsetCommitRange = "",
    [string[]]$PatchsetPathFilter = @(),
    [int]$MaxConflictFiles = 100,
    [int]$NativeMergeMaxConflicts = 25,
    [string[]]$BackupDiffIgnorePaths = @( "Plan.md" ),
    [string[]]$AutoResolveOursPaths = @(
        "Plan.md",
        "README.md",
        "TechnicalTome.md",
        "Agents.md",
        ".gitignore",
        "tools/porting/README.md",
        "tools/porting/PORTING_CONTEXT.md"
    )
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if( $RunCodex -and $NoCodex ) {
    throw "Specify either -RunCodex or -NoCodex, not both."
}

if( $AuditMode ) {
    $Mode = "Audit"
}

$script:RunUpstreamLane = $Mode -in @( "FullPort", "UpstreamPort", "Audit" )
$script:RunAolLane = $Mode -in @( "FullPort", "CAOLPort", "Audit" )
$script:IsAuditMode = $Mode -eq "Audit"
$script:RunCodexEnabled = -not $NoCodex

if( -not $script:RunUpstreamLane -and -not $script:RunAolLane ) {
    throw "Mode '$Mode' selects no execution lanes."
}

function Write-Step {
    param( [string]$Message )
    Write-Host ""
    Write-Host "[porting] $Message"
}

function Get-CurrentBranch {
    $branchResult = Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--abbrev-ref", "HEAD" )
    return @( $branchResult.Output )[-1].Trim()
}

function Assert-CurrentBranch {
    param(
        [Parameter(Mandatory = $true)] [string]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )
    if( $DryRun ) {
        return
    }
    $current = Get-CurrentBranch
    if( $current -ne $Expected ) {
        throw "Branch safety check failed during $Context. Expected '$Expected' but on '$current'."
    }
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
                "for-each-ref" { $isReadOnlyInDryRun = $true }
                "rev-list" { $isReadOnlyInDryRun = $true }
                "cat-file" { $isReadOnlyInDryRun = $true }
                "diff" { $isReadOnlyInDryRun = $true }
                "show" { $isReadOnlyInDryRun = $true }
                "cherry" { $isReadOnlyInDryRun = $true }
                "merge-base" { $isReadOnlyInDryRun = $true }
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
        $rendered = if( $null -eq $output ) { "" } else { [string]::Join( [Environment]::NewLine, $output ) }
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

function Read-CommitListFromRefPath {
    param(
        [Parameter(Mandatory = $true)] [string]$RefName,
        [Parameter(Mandatory = $true)] [string]$RepoRelativePath
    )
    $spec = "$RefName`:$RepoRelativePath"
    $result = Invoke-External -FilePath "git" -Arguments @( "show", $spec ) -IgnoreFailure
    if( $result.ExitCode -ne 0 ) {
        return @()
    }
    return @(
        @( $result.Output ) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" -and -not $_.StartsWith( "#" ) }
    )
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

function Resolve-AutoConflicts {
    param(
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)] [string[]]$ConflictPaths,
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)] [string[]]$AutoPaths
    )
    $resolved = New-Object System.Collections.Generic.List[string]
    foreach( $conflict in $ConflictPaths ) {
        if( -not ( $AutoPaths -contains $conflict ) ) {
            continue
        }
        Invoke-External -FilePath "git" -Arguments @( "checkout", "--ours", "--", $conflict ) -IgnoreFailure | Out-Null
        Invoke-External -FilePath "git" -Arguments @( "add", "--", $conflict ) -IgnoreFailure | Out-Null
        [void]$resolved.Add( $conflict )
    }
    return $resolved.ToArray()
}

function Sync-GitignoreFromMaster {
    param(
        [Parameter(Mandatory = $true)] [string]$CommitMessage
    )
    $masterGitignore = Invoke-External -FilePath "git" -Arguments @( "show", "master:.gitignore" ) -IgnoreFailure
    if( $masterGitignore.ExitCode -ne 0 ) {
        return $false
    }
    Invoke-External -FilePath "git" -Arguments @( "checkout", "master", "--", ".gitignore" ) | Out-Null
    Invoke-External -FilePath "git" -Arguments @( "add", "--", ".gitignore" ) | Out-Null
    $gitignoreDiff = Invoke-External -FilePath "git" -Arguments @( "diff", "--cached", "--quiet", "--", ".gitignore" ) -IgnoreFailure
    if( $gitignoreDiff.ExitCode -eq 0 ) {
        return $false
    }
    Invoke-External -FilePath "git" -Arguments @( "commit", "-m", $CommitMessage ) | Out-Null
    return $true
}

function Sync-OrchestratorFilesFromMaster {
    param(
        [Parameter(Mandatory = $true)] [string]$CommitMessage,
        [string[]]$Paths = @(
            "tools/porting/orchestrate_ports.ps1",
            "tools/porting/README.md",
            "tools/porting/PORTING_CONTEXT.md",
            "tools/porting/patchsets/common.txt",
            "tools/porting/patchsets/cdda-master.txt",
            "tools/porting/patchsets/cdda-0.H.txt",
            "tools/porting/patchsets/cdda-0.I.txt",
            "tools/porting/patchsets/ctlg-master.txt"
        )
    )
    $existing = New-Object System.Collections.Generic.List[string]
    foreach( $path in $Paths ) {
        $probe = Invoke-External -FilePath "git" -Arguments @( "show", "master:$path" ) -IgnoreFailure
        if( $probe.ExitCode -eq 0 ) {
            [void]$existing.Add( $path )
        }
    }
    if( $existing.Count -eq 0 ) {
        return $false
    }
    $syncPaths = $existing.ToArray()
    $checkoutArgs = @( "checkout", "master", "--" ) + $syncPaths
    $addArgs = @( "add", "--" ) + $syncPaths
    Invoke-External -FilePath "git" -Arguments $checkoutArgs | Out-Null
    Invoke-External -FilePath "git" -Arguments $addArgs | Out-Null
    $diffArgs = New-Object System.Collections.Generic.List[string]
    [void]$diffArgs.Add( "diff" )
    [void]$diffArgs.Add( "--cached" )
    [void]$diffArgs.Add( "--quiet" )
    [void]$diffArgs.Add( "--" )
    foreach( $path in $syncPaths ) {
        [void]$diffArgs.Add( $path )
    }
    $diffResult = Invoke-External -FilePath "git" -Arguments $diffArgs.ToArray() -IgnoreFailure
    if( $diffResult.ExitCode -eq 0 ) {
        return $false
    }
    Invoke-External -FilePath "git" -Arguments @( "commit", "-m", $CommitMessage ) | Out-Null
    return $true
}

function Invoke-NativeMergeStep {
    param(
        [Parameter(Mandatory = $true)] [string]$TargetName,
        [Parameter(Mandatory = $true)] [string]$TargetBranch,
        [Parameter(Mandatory = $true)] [string]$SourceRef,
        [Parameter(Mandatory = $true)] [string]$StepLabel,
        [Parameter(Mandatory = $true)] [string]$TargetRunDir
    )

    $safeLabel = $StepLabel.Replace( " ", "-" ).Replace( "/", "-" )
    $stepLog = Join-Path $TargetRunDir "$safeLabel-merge.log"
    Assert-CurrentBranch -Expected $TargetBranch -Context "$StepLabel merge for $TargetName"
    $result = Invoke-External -FilePath "git" -Arguments @( "merge", "--no-commit", "--no-ff", $SourceRef ) -IgnoreFailure
    if( -not $DryRun ) {
        $result.Output | Out-File -FilePath $stepLog -Encoding utf8
    }

    $codexUsed = $false

    if( $result.ExitCode -eq 0 ) {
        $mergeHead = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", "-q", "MERGE_HEAD" ) -IgnoreFailure ).ExitCode -eq 0
        if( $mergeHead ) {
            $commitResult = Invoke-External -FilePath "git" -Arguments @( "commit", "--no-edit" ) -IgnoreFailure
            if( $commitResult.ExitCode -ne 0 ) {
                return [PSCustomObject]@{
                    Success = $false
                    CodexUsed = $false
                    Notes = "$StepLabel merge commit failed."
                }
            }
            return [PSCustomObject]@{
                Success = $true
                CodexUsed = $false
                Notes = "$StepLabel merge committed."
            }
        }
        return [PSCustomObject]@{
            Success = $true
            CodexUsed = $false
            Notes = "$StepLabel already up to date."
        }
    }

    $conflicts = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
    if( $conflicts.Count -gt $NativeMergeMaxConflicts ) {
        $mergeHead = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", "-q", "MERGE_HEAD" ) -IgnoreFailure ).ExitCode -eq 0
        if( $mergeHead ) {
            Invoke-External -FilePath "git" -Arguments @( "merge", "--abort" ) -IgnoreFailure | Out-Null
        }
        return [PSCustomObject]@{
            Success = $false
            CodexUsed = $false
            Notes = "$StepLabel conflicts exceed threshold ($($conflicts.Count) > $NativeMergeMaxConflicts)."
        }
    }

    $autoResolved = @( Resolve-AutoConflicts -ConflictPaths $conflicts -AutoPaths $AutoResolveOursPaths )
    if( $autoResolved.Count -gt 0 ) {
        $conflicts = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
    }

    if( $conflicts.Count -gt 0 -and $script:RunCodexEnabled ) {
        $promptFile = Join-Path $TargetRunDir "codex-$safeLabel-merge-prompt.md"
        $codexLogFile = Join-Path $TargetRunDir "codex-$safeLabel-merge.log"
        $lastMessageFile = Join-Path $TargetRunDir "codex-$safeLabel-merge-last-message.txt"
        $conflictText = [string]::Join( [Environment]::NewLine, $conflicts )
        $promptBody = @"
You are on branch `$TargetBranch` resolving the `$StepLabel` merge from `$SourceRef`.
Preserve AOL behavior parity and keep fixes minimal.

Conflict files:
$conflictText

Requirements:
1. Resolve this merge and finish with `git commit --no-edit`.
2. Keep changes Linux-compatible.
3. Summarize any manual decisions.
"@
        New-CodexPromptFile -Path $promptFile -Title "$StepLabel merge fix for $TargetName" -Body $promptBody
        $codexUsed = Invoke-CodexExec -PromptFile $promptFile -CodexLogFile $codexLogFile -LastMessageFile $lastMessageFile -RepoRootPath $repoRoot
    }

    $remaining = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
    $mergeHead = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", "-q", "MERGE_HEAD" ) -IgnoreFailure ).ExitCode -eq 0
    if( $remaining.Count -eq 0 -and $mergeHead ) {
        $commitResult = Invoke-External -FilePath "git" -Arguments @( "commit", "--no-edit" ) -IgnoreFailure
        if( $commitResult.ExitCode -eq 0 ) {
            return [PSCustomObject]@{
                Success = $true
                CodexUsed = $codexUsed
                Notes = "$StepLabel merge resolved."
            }
        }
    } elseif( $remaining.Count -eq 0 -and -not $mergeHead ) {
        return [PSCustomObject]@{
            Success = $true
            CodexUsed = $codexUsed
            Notes = "$StepLabel merge completed."
        }
    }

    if( $mergeHead ) {
        Invoke-External -FilePath "git" -Arguments @( "merge", "--abort" ) -IgnoreFailure | Out-Null
    }
    return [PSCustomObject]@{
        Success = $false
        CodexUsed = $codexUsed
        Notes = "$StepLabel merge unresolved."
    }
}

function Invoke-NativeMergeAuditStep {
    param(
        [Parameter(Mandatory = $true)] [string]$SourceRef,
        [Parameter(Mandatory = $true)] [string]$StepLabel
    )

    $result = Invoke-External -FilePath "git" -Arguments @( "merge", "--no-commit", "--no-ff", $SourceRef ) -IgnoreFailure
    $conflicts = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
    $mergeHead = ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", "-q", "MERGE_HEAD" ) -IgnoreFailure ).ExitCode -eq 0

    $notes = ""
    $success = $false
    if( $result.ExitCode -eq 0 -and $conflicts.Count -eq 0 ) {
        $success = $true
        if( $mergeHead ) {
            $notes = "$StepLabel mergeable (would create merge commit)."
        } else {
            $notes = "$StepLabel already up to date."
        }
    } elseif( $conflicts.Count -gt 0 ) {
        $notes = "$StepLabel conflicts: $($conflicts.Count)."
    } else {
        $notes = "$StepLabel merge failed (exit $($result.ExitCode))."
    }

    if( $mergeHead ) {
        Invoke-External -FilePath "git" -Arguments @( "merge", "--abort" ) -IgnoreFailure | Out-Null
    }

    return [PSCustomObject]@{
        Success = $success
        ConflictCount = $conflicts.Count
        Notes = $notes
    }
}

function Confirm-CherryPickFallback {
    param(
        [Parameter(Mandatory = $true)] [string]$TargetName,
        [Parameter(Mandatory = $true)] [string]$TargetBranch,
        [Parameter(Mandatory = $true)] [string]$Reason
    )

    if( $DryRun ) {
        Write-Host "[porting] DRY-RUN: would ask confirmation before destructive fallback for $TargetName ($TargetBranch)"
        return $true
    }

    Write-Host "[porting] Native merge path failed for ${TargetName}: $Reason"
    Write-Host "[porting] Fallback will DELETE and recreate branch '$TargetBranch' from upstream base, then cherry-pick AOL patchset."
    while( $true ) {
        $answer = Read-Host "[porting] Continue with destructive fallback? (y/N)"
        if( [string]::IsNullOrWhiteSpace( $answer ) ) {
            return $false
        }
        switch( $answer.Trim().ToLowerInvariant() ) {
            "y" { return $true }
            "yes" { return $true }
            "n" { return $false }
            "no" { return $false }
            default { Write-Host "[porting] Please enter y or n." }
        }
    }
}

function Test-PathMatchesAnyGlob {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string[]]$Globs
    )
    foreach( $glob in $Globs ) {
        if( [string]::IsNullOrWhiteSpace( $glob ) ) {
            continue
        }
        if( $Path -like $glob ) {
            return $true
        }
    }
    return $false
}

function Should-SkipMasterDeltaCommit {
    param(
        [Parameter(Mandatory = $true)] [string]$CommitSha,
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)] [string[]]$IgnorePathGlobs
    )
    if( $IgnorePathGlobs.Count -eq 0 ) {
        return $false
    }
    $changedPaths = @(
        @( ( Invoke-External -FilePath "git" -Arguments @( "show", "--pretty=format:", "--name-only", $CommitSha, "--" ) ).Output ) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" }
    )
    if( $changedPaths.Count -eq 0 ) {
        return $false
    }
    foreach( $path in $changedPaths ) {
        if( -not ( Test-PathMatchesAnyGlob -Path $path -Globs $IgnorePathGlobs ) ) {
            return $false
        }
    }
    return $true
}

function Get-MasterDeltaPlan {
    param(
        [Parameter(Mandatory = $true)] [string]$TargetBranch,
        [Parameter(Mandatory = $true)] [string]$MasterRef,
        [string[]]$PathFilter = @(),
        [string[]]$IgnorePathGlobs = @()
    )

    $masterSnapshot = @( ( Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", $MasterRef ) ).Output )[-1].Trim()
    if( [string]::IsNullOrWhiteSpace( $masterSnapshot ) ) {
        throw "Could not resolve source snapshot for '$MasterRef'."
    }

    $args = New-Object System.Collections.Generic.List[string]
    [void]$args.Add( "rev-list" )
    [void]$args.Add( "--reverse" )
    [void]$args.Add( "--cherry-pick" )
    [void]$args.Add( "--right-only" )
    [void]$args.Add( "$TargetBranch...$masterSnapshot" )
    if( $PathFilter.Count -gt 0 ) {
        [void]$args.Add( "--" )
        foreach( $path in $PathFilter ) {
            [void]$args.Add( $path )
        }
    }
    $queue = @(
        @( ( Invoke-External -FilePath "git" -Arguments $args.ToArray() ).Output ) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" }
    )

    $filtered = New-Object System.Collections.Generic.List[string]
    $skippedByIgnore = 0
    foreach( $sha in $queue ) {
        if( Should-SkipMasterDeltaCommit -CommitSha $sha -IgnorePathGlobs $IgnorePathGlobs ) {
            $skippedByIgnore++
            continue
        }
        [void]$filtered.Add( $sha )
    }

    $filteredArray = $filtered.ToArray()
    if( $null -eq $filteredArray ) {
        $filteredArray = @()
    }

    return [PSCustomObject]@{
        Queue = @( $filteredArray )
        MasterSnapshot = $masterSnapshot
        Source = "direct-cherry"
        RawCount = $queue.Count
        SkippedByIgnore = $skippedByIgnore
        Notes = "source=direct-cherry raw=$($queue.Count) skippedByIgnore=$skippedByIgnore final=$($filtered.Count)"
    }
}

function Test-CommitObjectExists {
    param(
        [Parameter(Mandatory = $true)] [string]$CommitSha
    )
    $probe = Invoke-External -FilePath "git" -Arguments @( "cat-file", "-e", "$CommitSha^{commit}" ) -IgnoreFailure
    return $probe.ExitCode -eq 0
}

function Test-CommitIsAncestorOfBranch {
    param(
        [Parameter(Mandatory = $true)] [string]$CommitSha,
        [Parameter(Mandatory = $true)] [string]$BranchRef
    )
    $probe = Invoke-External -FilePath "git" -Arguments @( "merge-base", "--is-ancestor", $CommitSha, $BranchRef ) -IgnoreFailure
    return $probe.ExitCode -eq 0
}

function Get-AolPatchsetPlan {
    param(
        [Parameter(Mandatory = $true)] [string]$TargetName,
        [Parameter(Mandatory = $true)] [string]$SourceRef,
        [Parameter(Mandatory = $true)] [string]$TargetBranch
    )

    $baseQueue = @()
    $sourceLabel = ""
    if( -not [string]::IsNullOrWhiteSpace( $PatchsetCommitRange ) ) {
        $baseQueue = @( Resolve-CommitRangeList -RangeSpec $PatchsetCommitRange -Paths $PatchsetPathFilter )
        $sourceLabel = "range:$PatchsetCommitRange"
    } else {
        $baseQueue = @( Read-CommitListFromRefPath -RefName $SourceRef -RepoRelativePath $PatchsetFile )
        $sourceLabel = "$SourceRef`:$PatchsetFile"
    }

    $targetOverlay = @()
    $targetOverlayPath = ""
    if( -not $NoTargetPatchsets ) {
        $targetOverlayPath = "tools/porting/patchsets/$TargetName.txt"
        $targetOverlay = @( Read-CommitListFromRefPath -RefName $SourceRef -RepoRelativePath $targetOverlayPath )
    }

    $combined = @( Get-UniqueOrdered -Items ( $baseQueue + $targetOverlay ) )
    $valid = New-Object System.Collections.Generic.List[string]
    $pending = New-Object System.Collections.Generic.List[string]
    $invalidCount = 0
    $alreadyPresentCount = 0
    foreach( $sha in $combined ) {
        if( Test-CommitObjectExists -CommitSha $sha ) {
            [void]$valid.Add( $sha )
            if( Test-CommitIsAncestorOfBranch -CommitSha $sha -BranchRef $TargetBranch ) {
                $alreadyPresentCount++
            } else {
                [void]$pending.Add( $sha )
            }
        } else {
            $invalidCount++
        }
    }

    $notes = "source=$sourceLabel base=$($baseQueue.Count) overlay=$($targetOverlay.Count) unique=$($combined.Count) invalid=$invalidCount alreadyPresent=$alreadyPresentCount pending=$($pending.Count)"
    if( $NoTargetPatchsets ) {
        $notes = "$notes overlay=disabled"
    } elseif( $targetOverlayPath -ne "" ) {
        $notes = "$notes overlayPath=$targetOverlayPath"
    }

    return [PSCustomObject]@{
        Queue = @( $valid.ToArray() )
        PendingQueue = @( $pending.ToArray() )
        RawCount = $combined.Count
        InvalidCount = $invalidCount
        AlreadyPresentCount = $alreadyPresentCount
        Source = "patchset"
        Notes = $notes
    }
}

function Select-AolQueuePlan {
    param(
        [Parameter(Mandatory = $true)] [string]$TargetName,
        [Parameter(Mandatory = $true)] [string]$TargetBranch,
        [Parameter(Mandatory = $true)] [string]$SourceRef,
        [string[]]$PathFilter = @(),
        [string[]]$IgnorePathGlobs = @()
    )

    $patchsetPlan = Get-AolPatchsetPlan -TargetName $TargetName -SourceRef $SourceRef -TargetBranch $TargetBranch
    $patchsetPendingArray = @( $patchsetPlan.PendingQueue )
    $deltaCount = -1
    $deltaNotes = "delta-skipped"

    $skipDelta = $AolSkipDeltaTargets -contains $TargetName
    if( $skipDelta -and $patchsetPendingArray.Count -gt 0 ) {
        return [PSCustomObject]@{
            Queue = @( $patchsetPendingArray )
            QueueLabel = "aol-patchset"
            Source = "patchset"
            Notes = "$($patchsetPlan.Notes); deltaSkippedForTarget=$TargetName"
            SelectionReason = "skip-delta-target"
            DeltaCount = $deltaCount
            DeltaNotes = $deltaNotes
            PatchsetCount = $patchsetPlan.Queue.Count
            PatchsetPendingCount = $patchsetPendingArray.Count
            PatchsetNotes = $patchsetPlan.Notes
        }
    }

    $deltaPlan = Get-MasterDeltaPlan -TargetBranch $TargetBranch -MasterRef $SourceRef -PathFilter $PathFilter -IgnorePathGlobs $IgnorePathGlobs
    $deltaCount = $deltaPlan.Queue.Count
    $deltaNotes = $deltaPlan.Notes

    $selectedQueue = @( $deltaPlan.Queue )
    $selectedLabel = "aol-delta"
    $selectedSource = "delta"
    $selectedNotes = $deltaPlan.Notes
    $selectionReason = "delta-default"

    if( $patchsetPendingArray.Count -gt 0 ) {
        $ratioExceeded = $deltaPlan.Queue.Count -gt ( $patchsetPendingArray.Count * $AolDeltaPatchsetInflationRatio )
        $maxExceeded = $deltaPlan.Queue.Count -gt $AolDeltaMaxQueue
        if( $deltaPlan.Queue.Count -eq 0 ) {
            $selectedQueue = $patchsetPendingArray
            $selectedLabel = "aol-patchset"
            $selectedSource = "patchset"
            $selectionReason = "delta-empty-patchset-pending"
            $selectedNotes = "$($patchsetPlan.Notes); fallbackReason=$selectionReason"
        } elseif( $maxExceeded -or $ratioExceeded ) {
            $selectedQueue = $patchsetPendingArray
            $selectedLabel = "aol-patchset"
            $selectedSource = "patchset"
            $reasons = New-Object System.Collections.Generic.List[string]
            if( $maxExceeded ) {
                [void]$reasons.Add( "delta>$AolDeltaMaxQueue" )
            }
            if( $ratioExceeded ) {
                [void]$reasons.Add( "delta>$AolDeltaPatchsetInflationRatio x patchsetPending" )
            }
            $selectionReason = [string]::Join( ",", $reasons )
            $selectedNotes = "$($patchsetPlan.Notes); fallbackReason=$selectionReason"
        }
    } elseif( $deltaPlan.Queue.Count -gt 0 ) {
        $selectionReason = "patchset-empty-using-delta"
    } else {
        $selectionReason = "delta-empty"
    }

    return [PSCustomObject]@{
        Queue = @( $selectedQueue )
        QueueLabel = $selectedLabel
        Source = $selectedSource
        Notes = $selectedNotes
        SelectionReason = $selectionReason
        DeltaCount = $deltaPlan.Queue.Count
        DeltaNotes = $deltaPlan.Notes
        PatchsetCount = $patchsetPlan.Queue.Count
        PatchsetPendingCount = $patchsetPendingArray.Count
        PatchsetNotes = $patchsetPlan.Notes
    }
}

function Invoke-CherryPickQueue {
    param(
        [Parameter(Mandatory = $true)] [string]$TargetName,
        [Parameter(Mandatory = $true)] [string]$TargetBranch,
        [Parameter(Mandatory = $true)] [string]$BaseRefForPrompt,
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)] [string[]]$CommitQueue,
        [Parameter(Mandatory = $true)] [string]$TargetRunDir,
        [Parameter(Mandatory = $true)] [string]$QueueLabel
    )

    $appliedCount = 0
    $applyOk = $true
    $codexUsedForApply = $false
    $failedCommit = ""
    $failureNotes = ""

    $queueFile = Join-Path $TargetRunDir "commit-queue-$QueueLabel.txt"
    if( -not $DryRun ) {
        if( $CommitQueue.Count -eq 0 ) {
            "# no commits queued" | Out-File -FilePath $queueFile -Encoding utf8
        } else {
            $CommitQueue | Out-File -FilePath $queueFile -Encoding utf8
        }
    }

    $applyLogFile = Join-Path $TargetRunDir "apply-$QueueLabel.log"
    foreach( $sha in $CommitQueue ) {
        Assert-CurrentBranch -Expected $TargetBranch -Context "cherry-pick $sha for $TargetName"
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
        $autoResolved = @( Resolve-AutoConflicts -ConflictPaths $conflicts -AutoPaths $AutoResolveOursPaths )
        if( $autoResolved.Count -gt 0 ) {
            $conflicts = @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "--diff-filter=U" ) ).Output )
            if( -not $DryRun ) {
                @(
                    "auto_resolved_conflicts:"
                    [string]::Join( ", ", $autoResolved )
                    ""
                ) | Out-File -FilePath $applyLogFile -Append -Encoding utf8
            }
        }
        if( $conflicts.Count -gt $MaxConflictFiles ) {
            $failureNotes = "Conflict threshold exceeded ($($conflicts.Count) files > $MaxConflictFiles). Manual port required."
            $applyOk = $false
        } elseif( $script:RunCodexEnabled ) {
            $shortSha = if( $sha.Length -gt 12 ) { $sha.Substring( 0, 12 ) } else { $sha }
            $promptFile = Join-Path $TargetRunDir "codex-apply-$QueueLabel-$shortSha-prompt.md"
            $codexLogFile = Join-Path $TargetRunDir "codex-apply-$QueueLabel-$shortSha.log"
            $lastMessageFile = Join-Path $TargetRunDir "codex-apply-$QueueLabel-$shortSha-last-message.txt"
            $conflictText = if( $conflicts.Count -eq 0 ) { "(none listed)" } else { [string]::Join( [Environment]::NewLine, $conflicts ) }
            $promptBody = @"
You are on branch `$TargetBranch` after cherry-picking commit `$sha` onto base `$BaseRefForPrompt`.
Cherry-pick failed and must be completed while preserving AOL LLM behavior parity.

Conflict files:
$conflictText

Requirements:
1. Resolve this cherry-pick and run `git cherry-pick --continue`.
2. Keep changes focused to AOL porting behavior parity.
3. Do not run full builds yet; orchestrator will run builds after replay.
4. Summarize any manual decisions needed for future queue curation.
"@
            New-CodexPromptFile -Path $promptFile -Title "Queue apply fix for $TargetName ($QueueLabel) $shortSha" -Body $promptBody
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

    return [PSCustomObject]@{
        Success = $applyOk
        AppliedCount = $appliedCount
        CodexUsed = $codexUsedForApply
        FailureNotes = $failureNotes
        FailedCommit = $failedCommit
    }
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
    if( -not $script:RunCodexEnabled ) {
        return $false
    }
    $codexCmd = Get-Command codex -ErrorAction SilentlyContinue
    if( $null -eq $codexCmd ) {
        throw "Codex is enabled but `codex` is not available in PATH. Re-run with -NoCodex, or install codex."
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

    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        Get-Content -Raw -Path $PromptFile | & codex @args *> $CodexLogFile
        return ( $LASTEXITCODE -eq 0 )
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
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
 $currentBranch = Get-CurrentBranch
if( $currentBranch -ne "master" ) {
    throw "This orchestrator must be started on branch 'master'. Current branch: '$currentBranch'."
}
Write-Host "[porting] mode: $Mode (upstream-lane=$script:RunUpstreamLane, aol-lane=$script:RunAolLane, codex=$script:RunCodexEnabled)"
Write-Host "[porting] AOL delta-skip targets: $([string]::Join(', ', $AolSkipDeltaTargets))"

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

if( $script:RunAolLane ) {
    $aolProbe = Invoke-External -FilePath "git" -Arguments @( "rev-parse", "--verify", $AolSourceRef ) -IgnoreFailure
    if( $aolProbe.ExitCode -ne 0 ) {
        throw "Missing AOL source ref: $AolSourceRef"
    }
}

if( -not $NoBackup ) {
    Write-Step "Checking backup branch reuse"
    $reusedBackup = ""
    $backupRefs = @(
        @( ( Invoke-External -FilePath "git" -Arguments @( "for-each-ref", "refs/heads/backup/master-pre-port-*", "--format=%(refname:short)" ) ).Output ) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" }
    )
    foreach( $backupRef in $backupRefs ) {
        $changedPaths = @(
            @( ( Invoke-External -FilePath "git" -Arguments @( "diff", "--name-only", "$backupRef..master" ) ).Output ) |
            ForEach-Object { $_.Trim() } |
            Where-Object { $_ -ne "" }
        )
        $meaningfulDiff = @(
            $changedPaths | Where-Object { $BackupDiffIgnorePaths -notcontains $_ }
        )
        if( $meaningfulDiff.Count -eq 0 ) {
            $reusedBackup = $backupRef
            break
        }
    }
    if( -not [string]::IsNullOrWhiteSpace( $reusedBackup ) ) {
        Write-Host "[porting] backup branch reused: $reusedBackup"
    } else {
        Write-Step "Creating backup branch from master"
        $backupBase = "backup/master-pre-port-$runStamp"
        $backupBranch = Resolve-BackupBranchName -BaseName $backupBase
        Invoke-External -FilePath "git" -Arguments @( "branch", $backupBranch, "master" ) | Out-Null
        Write-Host "[porting] backup branch created: $backupBranch"
    }
}

if( $script:IsAuditMode ) {
    Write-Step "Audit mode (mode=$Mode, aol-source=$AolSourceRef, strategy=$MasterSyncMode)"
    $auditSummary = New-Object System.Collections.Generic.List[object]
    try {
        foreach( $target in $selectedTargets ) {
            Write-Step "Audit target: $($target.Name)"
            $exists = Test-RefExists -RefName "refs/heads/$($target.Branch)"
            if( -not $exists ) {
                [void]$auditSummary.Add( [PSCustomObject]@{
                        Target = $target.Name
                        Branch = $target.Branch
                        UpstreamConflicts = "-"
                        UpstreamRoute = "missing-branch"
                        AolNativeConflicts = "-"
                        AolDeltaQueue = "-"
                        AolPatchsetPending = "-"
                        AolQueue = "-"
                        AolQueueSource = "-"
                        PlannedRoute = "bootstrap-required"
                        Notes = "Target branch does not exist."
                    } )
                continue
            }

            Invoke-External -FilePath "git" -Arguments @( "checkout", $target.Branch ) | Out-Null
            Assert-CurrentBranch -Expected $target.Branch -Context "audit merge scan for $($target.Name)"

            $upstreamConflicts = "-"
            $upstreamRoute = "skipped"
            $upstreamNotes = ""
            $basisUsable = $true
            if( $script:RunUpstreamLane ) {
                $upstreamAudit = Invoke-NativeMergeAuditStep -SourceRef $target.UpstreamRef -StepLabel "upstream"
                $upstreamConflicts = $upstreamAudit.ConflictCount
                if( $upstreamAudit.Success ) {
                    $upstreamRoute = "native"
                } elseif( $upstreamAudit.ConflictCount -le $NativeMergeMaxConflicts ) {
                    $upstreamRoute = "native+codex"
                } else {
                    $upstreamRoute = "reset-required"
                }
                $upstreamNotes = $upstreamAudit.Notes
                $basisUsable = $upstreamAudit.Success -or ( $upstreamAudit.ConflictCount -le $NativeMergeMaxConflicts )
            }

            $aolNativeConflicts = "-"
            $aolDeltaQueue = "-"
            $aolPatchsetPending = "-"
            $aolQueue = "-"
            $aolQueueSource = "-"
            $plannedRoute = $upstreamRoute
            $aolNotes = ""
            if( $script:RunAolLane ) {
                $aolNativeAudit = Invoke-NativeMergeAuditStep -SourceRef $AolSourceRef -StepLabel "aol-native"
                $aolNativeConflicts = $aolNativeAudit.ConflictCount
                $aolQueuePlan = Select-AolQueuePlan -TargetName $target.Name -TargetBranch $target.Branch -SourceRef $AolSourceRef -PathFilter $MasterDeltaPathFilter -IgnorePathGlobs $MasterDeltaIgnorePathGlobs
                $aolDeltaQueue = if( $aolQueuePlan.DeltaCount -lt 0 ) { "SKIPPED" } else { $aolQueuePlan.DeltaCount }
                $aolPatchsetPending = $aolQueuePlan.PatchsetPendingCount
                $aolQueue = $aolQueuePlan.Queue.Count
                $aolQueueSource = $aolQueuePlan.Source
                if( -not $basisUsable -and $script:RunUpstreamLane ) {
                    $plannedRoute = "$upstreamRoute -> blocked"
                } elseif( $aolQueue -eq 0 ) {
                    $plannedRoute = if( $script:RunUpstreamLane ) { "upstream-only" } else { "aol-up-to-date" }
                } else {
                    if( $MasterSyncMode -eq "native-merge" -and $aolNativeAudit.Success ) {
                        $plannedRoute = if( $script:RunUpstreamLane ) { "upstream + aol-native" } else { "aol-native" }
                    } else {
                        $queueRouteTag = if( $aolQueuePlan.Source -eq "patchset" ) { "aol-patchset" } else { "aol-delta" }
                        $plannedRoute = if( $script:RunUpstreamLane ) { "upstream + $queueRouteTag" } else { $queueRouteTag }
                    }
                }
                $aolNotes = "$($aolNativeAudit.Notes) delta=$aolDeltaQueue patchsetPending=$aolPatchsetPending selected=$aolQueue source=$aolQueueSource reason=$($aolQueuePlan.SelectionReason) $($aolQueuePlan.Notes)".Trim()
            }

            [void]$auditSummary.Add( [PSCustomObject]@{
                    Target = $target.Name
                    Branch = $target.Branch
                    UpstreamConflicts = $upstreamConflicts
                    UpstreamRoute = $upstreamRoute
                    AolNativeConflicts = $aolNativeConflicts
                    AolDeltaQueue = $aolDeltaQueue
                    AolPatchsetPending = $aolPatchsetPending
                    AolQueue = $aolQueue
                    AolQueueSource = $aolQueueSource
                    PlannedRoute = $plannedRoute
                    Notes = "$upstreamNotes $aolNotes".Trim()
                } )
        }
    } finally {
        $branchNow = Get-CurrentBranch
        if( $branchNow -ne "master" ) {
            $restoreResult = Invoke-External -FilePath "git" -Arguments @( "checkout", "master" ) -IgnoreFailure
            if( $restoreResult.ExitCode -ne 0 ) {
                throw "Audit failed to restore branch to master from '$branchNow'."
            }
        }
    }
    Assert-CurrentBranch -Expected "master" -Context "audit return to master"
    Write-Step "Audit summary"
    $auditSummary | Format-Table -AutoSize
    Write-Host ""
    Write-Host "[porting] audit mode completed. No branch recreation or queue replay steps were executed."
    return
}

$summary = New-Object System.Collections.Generic.List[object]

foreach( $target in $selectedTargets ) {
    Write-Step "Target: $($target.Name)"
    $targetRunDir = Join-Path $runDir $target.Name
    if( -not $DryRun ) {
        New-Item -ItemType Directory -Path $targetRunDir -Force | Out-Null
    }

    $applyMode = "none"
    $nativeMergeApplied = $false
    $commitQueue = @()
    $queueLabel = "none"
    $appliedCount = 0
    $applyOk = $true
    $codexUsedForApply = $false
    $failedCommit = ""
    $failureNotes = ""
    $shouldApplyQueue = $false
    $existingPortBranch = Test-RefExists -RefName "refs/heads/$($target.Branch)"
    $upstreamReady = -not $script:RunUpstreamLane

    if( $existingPortBranch ) {
        Invoke-External -FilePath "git" -Arguments @( "checkout", $target.Branch ) | Out-Null
        Assert-CurrentBranch -Expected $target.Branch -Context "target entry for $($target.Name)"
        if( $script:RunUpstreamLane ) {
            $upstreamStep = Invoke-NativeMergeStep -TargetName $target.Name -TargetBranch $target.Branch -SourceRef $target.UpstreamRef -StepLabel "upstream" -TargetRunDir $targetRunDir
            $codexUsedForApply = $codexUsedForApply -or $upstreamStep.CodexUsed
            if( $upstreamStep.Success ) {
                $upstreamReady = $true
            } else {
                $failureNotes = "$($upstreamStep.Notes)"
            }
        }
    } else {
        if( $script:RunUpstreamLane ) {
            $upstreamReady = $false
            $failureNotes = "Target branch '$($target.Branch)' does not exist."
        } else {
            $applyOk = $false
            $failureNotes = "Target branch '$($target.Branch)' does not exist. CAOL-only mode requires existing target branch."
        }
    }

    if( $applyOk -and $upstreamReady -and -not $script:RunAolLane ) {
        $nativeMergeApplied = $true
        $applyMode = "upstream-only"
    }

    if( $applyOk -and $upstreamReady -and $script:RunAolLane ) {
        $aolNativeMerged = $false
        if( $MasterSyncMode -eq "native-merge" ) {
            $aolNativeStep = Invoke-NativeMergeStep -TargetName $target.Name -TargetBranch $target.Branch -SourceRef $AolSourceRef -StepLabel "aol-native" -TargetRunDir $targetRunDir
            $codexUsedForApply = $codexUsedForApply -or $aolNativeStep.CodexUsed
            if( $aolNativeStep.Success ) {
                $aolNativeMerged = $true
                $nativeMergeApplied = $true
                $applyMode = if( $script:RunUpstreamLane ) { "upstream+aol-native" } else { "aol-native" }
                Write-Host "[porting] AOL native merge completed for $($target.Name) from $AolSourceRef"
            } else {
                $failureNotes = $aolNativeStep.Notes
            }
        }

        if( -not $aolNativeMerged ) {
            $aolQueuePlan = Select-AolQueuePlan -TargetName $target.Name -TargetBranch $target.Branch -SourceRef $AolSourceRef -PathFilter $MasterDeltaPathFilter -IgnorePathGlobs $MasterDeltaIgnorePathGlobs
            $commitQueue = @( $aolQueuePlan.Queue )
            $queueLabel = $aolQueuePlan.QueueLabel
            $queueModeTag = if( $aolQueuePlan.Source -eq "patchset" ) { "aol-patchset" } else { "aol-delta" }
            $applyMode = if( $script:RunUpstreamLane ) { "upstream+$queueModeTag" } else { "$queueModeTag-only" }
            if( $commitQueue.Count -eq 0 ) {
                $nativeMergeApplied = $true
                Write-Host "[porting] AOL queue empty for $($target.Name): $($aolQueuePlan.Notes)"
            } else {
                $shouldApplyQueue = $true
                $deltaDisplay = if( $aolQueuePlan.DeltaCount -lt 0 ) { "SKIPPED" } else { [string]$aolQueuePlan.DeltaCount }
                Write-Host "[porting] AOL queue for $($target.Name): selected=$($commitQueue.Count) source=$($aolQueuePlan.Source) delta=$deltaDisplay patchsetPending=$($aolQueuePlan.PatchsetPendingCount) ($($aolQueuePlan.Notes))"
            }
        }
    }

    $needsDestructiveFallback = $applyOk -and $script:RunUpstreamLane -and -not $upstreamReady
    if( $needsDestructiveFallback ) {
        $canDestructiveFallback = ( -not $existingPortBranch ) -or $AllowDestructiveFallback
        if( -not $canDestructiveFallback ) {
            $applyOk = $false
            if( [string]::IsNullOrWhiteSpace( $failureNotes ) ) {
                $failureNotes = "Non-destructive path could not continue."
            }
            $failureNotes = "$failureNotes Destructive fallback disabled. Re-run with -AllowDestructiveFallback to recreate the branch."
        } else {
            if( $existingPortBranch ) {
                $confirmed = Confirm-CherryPickFallback -TargetName $target.Name -TargetBranch $target.Branch -Reason $failureNotes
                if( -not $confirmed ) {
                    throw "Aborted by user before destructive fallback for target '$($target.Name)'."
                }
            }

            # Reset failure reason; from this point failures should reflect fallback apply, not pre-fallback merge state.
            $failureNotes = ""
            Invoke-External -FilePath "git" -Arguments @( "checkout", "master" ) | Out-Null
            Assert-CurrentBranch -Expected "master" -Context "fallback reset prelude for $($target.Name)"

            if( Test-RefExists -RefName "refs/heads/$($target.Branch)" ) {
                Invoke-External -FilePath "git" -Arguments @( "branch", "-D", $target.Branch ) | Out-Null
            }

            Invoke-External -FilePath "git" -Arguments @( "checkout", "-B", $target.Branch, $target.UpstreamRef ) | Out-Null
            Assert-CurrentBranch -Expected $target.Branch -Context "fallback branch recreate for $($target.Name)"
            if( Sync-GitignoreFromMaster -CommitMessage "[porting] Sync .gitignore with master (baseline)" ) {
                Write-Host "[porting] synced .gitignore from master (baseline)"
            }
            $upstreamReady = $true

            if( $script:RunAolLane ) {
                $aolQueuePlan = Select-AolQueuePlan -TargetName $target.Name -TargetBranch $target.Branch -SourceRef $AolSourceRef -PathFilter $MasterDeltaPathFilter -IgnorePathGlobs $MasterDeltaIgnorePathGlobs
                $commitQueue = @( $aolQueuePlan.Queue )
                $queueLabel = $aolQueuePlan.QueueLabel
                $queueModeTag = if( $aolQueuePlan.Source -eq "patchset" ) { "aol-patchset" } else { "aol-delta" }
                $applyMode = "upstream-reset+$queueModeTag"
                if( $commitQueue.Count -eq 0 ) {
                    $nativeMergeApplied = $true
                    Write-Host "[porting] AOL queue empty after reset for $($target.Name): $($aolQueuePlan.Notes)"
                } else {
                    $shouldApplyQueue = $true
                    $deltaDisplay = if( $aolQueuePlan.DeltaCount -lt 0 ) { "SKIPPED" } else { [string]$aolQueuePlan.DeltaCount }
                    Write-Host "[porting] AOL queue after reset for $($target.Name): selected=$($commitQueue.Count) source=$($aolQueuePlan.Source) delta=$deltaDisplay patchsetPending=$($aolQueuePlan.PatchsetPendingCount) ($($aolQueuePlan.Notes))"
                }
            } else {
                $nativeMergeApplied = $true
                $applyMode = "upstream-reset"
            }
        }
    }

    if( $applyOk -and $shouldApplyQueue ) {
        $queueResult = Invoke-CherryPickQueue -TargetName $target.Name -TargetBranch $target.Branch -BaseRefForPrompt $target.UpstreamRef -CommitQueue $commitQueue -TargetRunDir $targetRunDir -QueueLabel $queueLabel
        $applyOk = $queueResult.Success
        $appliedCount = $queueResult.AppliedCount
        $codexUsedForApply = $codexUsedForApply -or $queueResult.CodexUsed
        $failedCommit = $queueResult.FailedCommit
        if( -not $queueResult.Success ) {
            if( [string]::IsNullOrWhiteSpace( $queueResult.FailureNotes ) ) {
                if( [string]::IsNullOrWhiteSpace( $failureNotes ) ) {
                    $failureNotes = "Queue apply failed."
                }
            } else {
                $failureNotes = $queueResult.FailureNotes
            }
        }
    }

    if( -not $applyOk ) {
        if( Sync-GitignoreFromMaster -CommitMessage "[porting] Sync .gitignore with master (post-apply-failure)" ) {
            Write-Host "[porting] synced .gitignore from master after apply failure"
        }
        if( Sync-OrchestratorFilesFromMaster -CommitMessage "[porting] Sync orchestrator files with master (post-apply-failure)" ) {
            Write-Host "[porting] synced orchestrator files from master after apply failure"
        }
        [void]$summary.Add( [PSCustomObject]@{
                Target = $target.Name
                Branch = $target.Branch
                Queue = $commitQueue.Count
                Applied = $appliedCount
                Apply = "FAILED"
                CodexApply = $( if( $codexUsedForApply ) { "USED" } else { "NO" } )
                Build = "SKIPPED"
                Notes = "$( $failureNotes ) Strategy: $applyMode. See $targetRunDir"
            } )
        continue
    }

    $windowsBuild = "SKIPPED"
    $linuxBuild = "SKIPPED"
    $buildNotes = ""
    if( Sync-GitignoreFromMaster -CommitMessage "[porting] Sync .gitignore with master (post-apply)" ) {
        Write-Host "[porting] synced .gitignore from master after apply"
    }
    if( Sync-OrchestratorFilesFromMaster -CommitMessage "[porting] Sync orchestrator files with master (post-apply)" ) {
        Write-Host "[porting] synced orchestrator files from master after apply"
    }

    if( -not $SkipBuild ) {
        $winLog = Join-Path $targetRunDir "build-windows.log"
        $linLog = Join-Path $targetRunDir "build-linux.log"

        $winExit = Invoke-CmdLogged -CommandLine "just_build.cmd --unclean" -LogFile $winLog -IgnoreFailure
        $windowsBuild = if( $winExit -eq 0 ) { "OK" } else { "FAIL" }
        if( $winExit -ne 0 -and $script:RunCodexEnabled ) {
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
        if( $linExit -ne 0 -and $script:RunCodexEnabled ) {
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

    $applyStatus = if( $DryRun ) { "DRYRUN" } else { "OK" }
    if( $DryRun ) {
        $windowsBuild = "DRYRUN"
        $linuxBuild = "DRYRUN"
        $buildNotes = if( [string]::IsNullOrWhiteSpace( $buildNotes ) ) {
            "Dry-run: merge/apply/build steps are simulated only."
        } else {
            "$buildNotes; Dry-run: merge/apply/build steps are simulated only."
        }
    }

    [void]$summary.Add( [PSCustomObject]@{
            Target = $target.Name
            Branch = $target.Branch
            Queue = $commitQueue.Count
            Applied = $appliedCount
            Apply = $applyStatus
            CodexApply = $( if( $codexUsedForApply ) { "USED" } else { "NO" } )
            Build = "$( $windowsBuild )/$( $linuxBuild )"
            Notes = if( [string]::IsNullOrWhiteSpace( $buildNotes ) ) { "Strategy: $applyMode" } else { "$buildNotes; Strategy: $applyMode" }
        } )
}

Invoke-External -FilePath "git" -Arguments @( "checkout", "master" ) | Out-Null
Assert-CurrentBranch -Expected "master" -Context "final return to master"

Write-Step "Summary"
$summary | Format-Table -AutoSize

$summaryFile = Join-Path $runDir "summary.txt"
if( -not $DryRun ) {
    $summary | Format-Table -AutoSize | Out-String | Out-File -FilePath $summaryFile -Encoding utf8
}

Write-Host ""
Write-Host "[porting] logs directory: $runDir"
Write-Host "[porting] next manual step: check out each port/* branch and package release artifacts."
