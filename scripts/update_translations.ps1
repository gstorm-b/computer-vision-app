<#
.SYNOPSIS
    Updates the product's translation file from every source in the repository.

.DESCRIPTION
    Runs lupdate against translations\ncr_translations.pro — the one project whose source set
    is the WHOLE product (src/, both shells, and the vendored property browser).

    Do not point lupdate at a shell .pro instead. Since Phase 6 / E7a the shells compile only
    a handful of files each, because all of src/ lives in the ncr_shared static library, so an
    update driven from a shell sees a few dozen strings out of ~974 and marks the rest
    "vanished". That is what happened to this file once already: 861 of 902 messages, with no
    error and no warning. The symptom is a Japanese UI quietly reverting to English.

    Qt Creator's *Update Translations* on ncr_picking_all.pro does the same job, because the
    umbrella lists the translations project. This script exists so the update is reproducible
    without the IDE.

    NEVER add -no-obsolete. A "vanished" entry still carries its Japanese; the flag deletes
    those entries outright. Vanished is recoverable, deleted is not. This script takes a
    timestamped backup before writing for the same reason.

.PARAMETER QtBinDir
    Directory containing lupdate.exe. Defaults to %QT_MSVC_DIR%\bin.

.PARAMETER NoBackup
    Skip the timestamped backup. Off by default.

.EXAMPLE
    .\scripts\update_translations.ps1
#>
[CmdletBinding()]
param(
    [string] $QtBinDir,
    [switch] $NoBackup
)

$ErrorActionPreference = 'Stop'

$repoRoot   = Split-Path -Parent $PSScriptRoot
$projectPro = Join-Path $repoRoot 'translations\ncr_translations.pro'
$tsFile     = Join-Path $repoRoot 'components\app\translations\ncr_picking_ja_JP.ts'

if (-not $QtBinDir) {
    if (-not $env:QT_MSVC_DIR) {
        throw ("QT_MSVC_DIR is not set and -QtBinDir was not given. Machine-local paths come " +
               "from environment variables - see docs\rules\build_and_verification.md.")
    }
    $QtBinDir = Join-Path $env:QT_MSVC_DIR 'bin'
}

$lupdate = Join-Path $QtBinDir 'lupdate.exe'
if (-not (Test-Path $lupdate)) { throw "lupdate not found: $lupdate" }
foreach ($p in @($projectPro, $tsFile)) {
    if (-not (Test-Path $p)) { throw "Not found: $p" }
}

# Counts the three states separately. Reporting one total hides the distinction that matters:
# "unfinished" is new text waiting for a translator, "vanished" is text no longer in the code.
function Get-TsCounts([string] $path) {
    $text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    $finished = 0; $unfinished = 0; $vanished = 0
    foreach ($m in [regex]::Matches($text, '(?s)<message[^>]*>.*?</message>')) {
        if     ($m.Value -match 'type="vanished"')   { $vanished++ }
        elseif ($m.Value -match 'type="unfinished"') { $unfinished++ }
        else                                         { $finished++ }
    }
    [pscustomobject]@{ Finished = $finished; Unfinished = $unfinished; Vanished = $vanished }
}

$before = Get-TsCounts $tsFile

if (-not $NoBackup) {
    $stamp  = Get-Date -Format 'yyyyMMdd_HHmmss'
    $backup = "$tsFile.$stamp.bak"
    Copy-Item $tsFile $backup -Force
    Write-Host "backup   : $backup"
}

Write-Host "project  : $projectPro"
Write-Host "lupdate  : $lupdate"
Write-Host ""

# lupdate writes progress and warnings to stderr even on a completely successful run - the
# "Cannot run compiler 'cl'" line appears whenever this runs outside a Visual Studio
# environment, because qmake evaluates the mkspec. It is not a translation problem: lupdate
# compiles nothing and the scan set is unaffected.
#
# Windows PowerShell turns a native command's stderr into a terminating error under
# $ErrorActionPreference = 'Stop', so a successful lupdate would abort this script after it
# had already rewritten the file. Relax the preference around the call and judge the run by
# its exit code, which is what actually reports failure.
$previousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    & $lupdate $projectPro
} finally {
    $ErrorActionPreference = $previousPreference
}
if ($LASTEXITCODE -ne 0) { throw "lupdate failed with exit code $LASTEXITCODE" }

$after = Get-TsCounts $tsFile

Write-Host ""
Write-Host "$(Split-Path $tsFile -Leaf)"
Write-Host ("  finished   : {0,5}  ->{1,5}" -f $before.Finished,   $after.Finished)
Write-Host ("  unfinished : {0,5}  ->{1,5}" -f $before.Unfinished, $after.Unfinished)
Write-Host ("  vanished   : {0,5}  ->{1,5}" -f $before.Vanished,   $after.Vanished)
Write-Host ""
Write-Host "'unfinished' is new text needing a translator. 'vanished' is text no longer found"
Write-Host "in any source - it keeps its translation and comes back if the string returns."
Write-Host ""
Write-Host "A .qm proves the file compiled, not that the UI reads it. Rebuild and launch BOTH"
Write-Host "shells in Japanese to confirm."
