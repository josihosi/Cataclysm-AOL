[CmdletBinding()]
param(
    [string[]]$Targets = @( "cdda-master", "cdda-0.H", "cdda-0.I", "ctlg-master" ),
    [string]$PatchsetFile = "tools/porting/patchsets/common.txt",
    [switch]$NoTargetPatchsets,
    [string]$CommitRange = "",
    [string[]]$PathFilter = @(),
    [switch]$Fetch,
    [switch]$KeepWorktrees
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step {
    param( [string]$Message )
    Write-Host ""
    Write-Host "[patchset-sim] $Message"
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Arguments,
        [string]$WorkDir = "",
        [switch]$IgnoreFailure
    )
    $previousErrorAction = $ErrorActionPreference
    if( -not [string]::IsNullOrWhiteSpace( $WorkDir ) ) {
        Push-Location $WorkDir
    }
    try {
        $ErrorActionPreference = "Continue"
        $output = & git @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
        if( -not [string]::IsNullOrWhiteSpace( $WorkDir ) ) {
            Pop-Location
        }
    }
    if( $exitCode -ne 0 -and -not $IgnoreFailure ) {
        $rendered = [string]::Join( [Environment]::NewLine, [string[]]$output )
        throw "git failed ($exitCode): git $($Arguments -join ' ')`n$rendered"
    }
    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output = if( $null -eq $output ) { @() } else { [string[]]$output }
    }
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
        @( ( Invoke-Git -Arguments $args.ToArray() ).Output ) | Where-Object { $_ -ne "" }
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

$repoRoot = @( ( Invoke-Git -Arguments @( "rev-parse", "--show-toplevel" ) ).Output )[-1].Trim()
if( [string]::IsNullOrWhiteSpace( $repoRoot ) ) {
    throw "Could not resolve repository root."
}
Set-Location $repoRoot

$targetMap = @{
    "cdda-master" = [PSCustomObject]@{
        Name = "cdda-master"
        UpstreamRef = "upstream/master"
    }
    "cdda-0.H" = [PSCustomObject]@{
        Name = "cdda-0.H"
        UpstreamRef = "upstream/0.H-branch"
    }
    "cdda-0.I" = [PSCustomObject]@{
        Name = "cdda-0.I"
        UpstreamRef = "upstream/0.I-branch"
    }
    "ctlg-master" = [PSCustomObject]@{
        Name = "ctlg-master"
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

if( $Fetch ) {
    Write-Step "Fetching remotes"
    Invoke-Git -Arguments @( "fetch", "upstream", "--prune" ) | Out-Null
    Invoke-Git -Arguments @( "fetch", "upstream-ctlg", "--prune" ) | Out-Null
}

foreach( $target in $selectedTargets ) {
    $verify = Invoke-Git -Arguments @( "rev-parse", "--verify", $target.UpstreamRef ) -IgnoreFailure
    if( $verify.ExitCode -ne 0 ) {
        throw "Missing upstream ref for target '$($target.Name)': $($target.UpstreamRef)"
    }
}

$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runDir = Join-Path $repoRoot ( Join-Path "tools/porting/logs" $runStamp )
$simDir = Join-Path $runDir "patchset-sim"
New-Item -ItemType Directory -Path $simDir -Force | Out-Null

$tempRoot = Join-Path $env:TEMP ( "aol-patchset-sim-" + $runStamp )
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

Write-Step "Building commit queue"
$baseQueue = @()
if( -not [string]::IsNullOrWhiteSpace( $CommitRange ) ) {
    $baseQueue = @( Resolve-CommitRangeList -RangeSpec $CommitRange -Paths $PathFilter )
    Write-Host "[patchset-sim] source: commit range $CommitRange ($($baseQueue.Count) commits)"
} else {
    $basePatchsetPath = Join-Path $repoRoot $PatchsetFile
    $baseQueue = @( Read-CommitListFile -FilePath $basePatchsetPath )
    Write-Host "[patchset-sim] source: patchset file $PatchsetFile ($($baseQueue.Count) commits)"
}

if( $baseQueue.Count -eq 0 ) {
    Write-Host "[patchset-sim] WARNING: base commit queue is empty."
}

$summary = New-Object System.Collections.Generic.List[object]

foreach( $target in $selectedTargets ) {
    Write-Step "Target: $($target.Name)"
    $targetDir = Join-Path $simDir $target.Name
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    $targetQueue = New-Object System.Collections.Generic.List[string]
    foreach( $sha in $baseQueue ) {
        [void]$targetQueue.Add( $sha )
    }
    if( -not $NoTargetPatchsets ) {
        $targetPatchset = Select-TargetPatchsetFile -RepoRoot $repoRoot -TargetName $target.Name
        if( -not [string]::IsNullOrWhiteSpace( $targetPatchset ) ) {
            $extra = @( Read-CommitListFile -FilePath $targetPatchset )
            foreach( $sha in $extra ) {
                [void]$targetQueue.Add( $sha )
            }
        }
    }
    $commitQueue = @( Get-UniqueOrdered -Items $targetQueue.ToArray() )

    foreach( $sha in $commitQueue ) {
        $exists = Invoke-Git -Arguments @( "cat-file", "-e", "$sha^{commit}" ) -IgnoreFailure
        if( $exists.ExitCode -ne 0 ) {
            throw "Invalid commit in queue for $($target.Name): $sha"
        }
    }

    $worktreePath = Join-Path $tempRoot $target.Name
    Invoke-Git -Arguments @( "worktree", "add", "--detach", $worktreePath, $target.UpstreamRef ) | Out-Null

    $appliedCount = 0
    $failedCommit = ""
    $conflictFiles = @()
    $status = "OK"

    foreach( $sha in $commitQueue ) {
        $pick = Invoke-Git -Arguments @( "cherry-pick", "--allow-empty", "-x", $sha ) -WorkDir $worktreePath -IgnoreFailure
        if( $pick.ExitCode -ne 0 ) {
            $failedCommit = $sha
            $status = "CONFLICT"
            $conflictFiles = @( ( Invoke-Git -Arguments @( "diff", "--name-only", "--diff-filter=U" ) -WorkDir $worktreePath ).Output )
            Invoke-Git -Arguments @( "cherry-pick", "--abort" ) -WorkDir $worktreePath -IgnoreFailure | Out-Null
            break
        }
        $appliedCount++
    }

    if( $commitQueue.Count -eq 0 ) {
        $status = "NO_COMMITS"
    }

    $detailFile = Join-Path $targetDir "result.txt"
    @(
        "target=$($target.Name)"
        "upstream_ref=$($target.UpstreamRef)"
        "status=$status"
        "queue_commits=$($commitQueue.Count)"
        "applied_before_failure=$appliedCount"
        "failed_commit=$failedCommit"
        "conflict_files=$($conflictFiles.Count)"
        ""
        "conflict_file_list:"
    ) | Out-File -FilePath $detailFile -Encoding utf8
    if( $conflictFiles.Count -gt 0 ) {
        $conflictFiles | Out-File -FilePath $detailFile -Append -Encoding utf8
    }

    [void]$summary.Add( [PSCustomObject]@{
            Target = $target.Name
            Upstream = $target.UpstreamRef
            Queue = $commitQueue.Count
            Applied = $appliedCount
            Status = $status
            ConflictFiles = $conflictFiles.Count
            FailedCommit = $failedCommit
        } )

    if( -not $KeepWorktrees ) {
        Invoke-Git -Arguments @( "worktree", "remove", "--force", $worktreePath ) -IgnoreFailure | Out-Null
    }
}

if( -not $KeepWorktrees ) {
    Remove-Item -Recurse -Force $tempRoot
}

$summaryFile = Join-Path $simDir "summary.txt"
$summary | Format-Table -AutoSize | Out-String | Out-File -FilePath $summaryFile -Encoding utf8

Write-Step "Summary"
$summary | Format-Table -AutoSize
Write-Host ""
Write-Host "[patchset-sim] logs directory: $simDir"
