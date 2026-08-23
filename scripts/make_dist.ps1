<#
.SYNOPSIS
    Verifies the built output folder and copies it to dist/ as the install image.

.DESCRIPTION
    Since Phase 6 / E7b the BUILD already produces one complete folder: both shells share a
    single DESTDIR (build\bin\<config>, set in qmake\app_common.pri), and both the
    Qt/OpenCV/Pylon/ADS deployment and the RobotKinematics DLL/asset copy resolve their
    destination from it. So there is exactly one definition of what ships — the build's own
    deploy step — and this script no longer assembles anything.

    What it still does is the part a build step cannot: CHECK, then copy.

        1. both executables are present;
        2. they carry the same, non-placeholder file version — two exes in one folder from
           different builds is the failure the image exists to prevent;
        3. nothing that is a build intermediate leaks into the image;
        4. no DLL appears twice.

    The result is dist\, matching the field install layout exactly:

        dist/
          ncr_picking.exe          commissioning
          ncr_runtime.exe          operator runtime
          <Qt, OpenCV, Pylon, ADS, RobotKinematics DLLs>   one copy
          platforms/ sqldrivers/ imageformats/ ...          Qt plugins
          robot_assets/

    Both executables must be in one folder: they share ~40 runtime DLLs plus the plugin
    tree, and two folders means two copies that can drift to different Qt or Pylon
    versions — a mismatch that only shows up on the customer's machine. It is also what
    lets a future editor/runtime switch find its sibling via applicationDirPath().

    The file list this produces IS the installer manifest tracked in
    docs/backlog/later_todo_list.md #27. See docs/product/install_image.md.

.PARAMETER BinDir
    The build's shared output folder. Defaults to <repo>\build\bin\release, which is where
    qmake\app_common.pri points DESTDIR for a release build of either shell — whether it
    was built through ncr_picking_all.pro or on its own.

.PARAMETER DistDir
    Output directory. Defaults to <repo>\dist

.PARAMETER Clean
    Remove DistDir before staging. Off by default: this script never deletes without
    being asked.

.EXAMPLE
    # After any build of both shells.
    .\scripts\make_dist.ps1

.EXAMPLE
    # A debug image, or a build that used a different DESTDIR.
    .\scripts\make_dist.ps1 -BinDir build\bin\debug
#>
[CmdletBinding()]
param(
    [string] $BinDir,
    [string] $DistDir,
    [switch] $Clean
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BinDir)  { $BinDir  = Join-Path $repoRoot 'build\bin\release' }
if (-not $DistDir) { $DistDir = Join-Path $repoRoot 'dist' }

if (-not (Test-Path -PathType Container $BinDir)) {
    throw ("Build output folder not found: $BinDir`n" +
           "Build both shells first - see docs/rules/build_and_verification.md.")
}

$editorExe  = Join-Path $BinDir 'ncr_picking.exe'
$runtimeExe = Join-Path $BinDir 'ncr_runtime.exe'
foreach ($exe in @($editorExe, $runtimeExe)) {
    if (-not (Test-Path $exe)) {
        throw ("Executable not found: $exe`n" +
               "Both shells must be built before staging - a one-shell image is not an " +
               "install image. Build through ncr_picking_all.pro.")
    }
}

# Refuse to stage a mismatched pair. The version comes from qmake/version.pri, which feeds
# both the file resource read here and the string the application logs at startup.
$editorVersion  = (Get-Item $editorExe).VersionInfo.ProductVersion
$runtimeVersion = (Get-Item $runtimeExe).VersionInfo.ProductVersion

Write-Host "bin     : $BinDir"
Write-Host "editor  : ncr_picking.exe  ($editorVersion)"
Write-Host "runtime : ncr_runtime.exe  ($runtimeVersion)"

if ([string]::IsNullOrWhiteSpace($editorVersion) -or $editorVersion -eq '0.0.0.0') {
    throw ("Executables carry no file version. qmake/version.pri sets VERSION - if it " +
           "reads 0.0.0.0 the build predates it, so re-run qmake and rebuild. Staging " +
           "now would produce an image whose version pairing cannot be checked at all.")
}

if ($editorVersion -ne $runtimeVersion) {
    throw ("Version mismatch: editor $editorVersion vs runtime $runtimeVersion. " +
           "Both executables in one install folder must come from the same build - " +
           "see docs/product/install_image.md.")
}

if ($Clean -and (Test-Path $DistDir)) {
    Write-Host "Removing $DistDir (--Clean)"
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Force $DistDir | Out-Null

# Intermediates are excluded by extension rather than by listing what to keep, so a new
# DLL family lands in the image automatically instead of being silently absent — the
# failure mode of a keep-list is a missing file nobody notices until the customer's
# machine. bin/ should hold none of these; the filter is a guard, not a workaround, and it
# reports what it caught rather than dropping it quietly.
$excludedExtensions = @('.obj', '.cpp', '.h', '.moc', '.res', '.qrc', '.log', '.stash')

$copiedFiles = 0
$skipped = @()
Get-ChildItem -Path $BinDir -File | ForEach-Object {
    if (($excludedExtensions -contains $_.Extension.ToLower()) -or ($_.Name -like 'Makefile*')) {
        $skipped += $_.Name
        return
    }
    Copy-Item $_.FullName -Destination $DistDir -Force
    $copiedFiles++
}

# Plugin and asset folders (platforms/, sqldrivers/, robot_assets/, ...). "logs" is
# runtime output, not payload.
$excludedDirs = @('logs', '.qtc_clangd', 'debug', 'release')
$copiedDirs = 0
Get-ChildItem -Path $BinDir -Directory | ForEach-Object {
    if ($excludedDirs -contains $_.Name.ToLower()) { return }
    Copy-Item $_.FullName -Destination $DistDir -Recurse -Force
    $copiedDirs++
}

# A DLL reachable from two places in the image is the drift this whole layout exists to
# prevent, so it is asserted rather than assumed.
$dlls = Get-ChildItem -Path $DistDir -Recurse -Filter '*.dll' -File
$duplicates = $dlls | Group-Object Name | Where-Object { $_.Count -gt 1 }

Write-Host ""
Write-Host "Staged to $DistDir"
Write-Host "  files                   : $copiedFiles"
Write-Host "  folders                 : $copiedDirs"
Write-Host "  DLLs (incl. plugins)    : $($dlls.Count)"
Write-Host "  version (both)          : $editorVersion"

if ($skipped.Count -gt 0) {
    Write-Host ""
    Write-Warning ("Excluded $($skipped.Count) build intermediate(s) from $BinDir. " +
                   "bin/ is a DESTDIR and should contain none: " + ($skipped -join ', '))
}

if ($duplicates) {
    Write-Host ""
    foreach ($d in $duplicates) {
        Write-Warning ("Duplicate DLL in image: $($d.Name) x$($d.Count) -> " +
                       (($d.Group | ForEach-Object { $_.FullName.Substring($DistDir.Length) }) -join ', '))
    }
    throw ("The image contains duplicated DLLs. One runtime set is the point of this " +
           "layout - see docs/product/install_image.md.")
}

Write-Host "  duplicate DLLs          : 0"
Write-Host ""
Write-Host "Verify by running BOTH executables from dist\ with the dependency"
Write-Host "directories removed from PATH. Neither may fall back to a developer machine's"
Write-Host "Qt/OpenCV/Pylon install - that is the whole point of the image."
