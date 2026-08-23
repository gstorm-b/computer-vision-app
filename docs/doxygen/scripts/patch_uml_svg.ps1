<#
Patches PlantUML-rendered UML SVGs (docs/generated/doxygen/uml_svg/*.svg) so they use
Doxygen's own vendored svg.min.js (the INTERACTIVE_SVG pan/zoom mechanism, an
SVGPan-derived script) instead of a custom <img>+CSS-transform viewer.

Why: an <img>-embedded SVG gets rasterized once at layout size, so CSS `transform:
scale()` zoom blurs/pixelates it; and a hand-rolled pointerdown/pointermove pan handler
without preventDefault() can be hijacked by the browser's native text-selection drag
after the first drag. Doxygen's own class/inheritance graphs use this same svg.min.js
already, embedded via <iframe>, and don't have either problem: zoom is a native SVG
transform="matrix(...)" on a <g id="viewport">, so it stays vector-crisp, and the
library's mouse handlers call preventDefault() throughout.

This script gives each rendered SVG the minimal structure svg.min.js expects:
  - id="main" onload="init(evt)" on the root <svg>
  - viewWidth/viewHeight/sectionId globals + a <script xlink:href="svg.min.js"/> include
  - id="viewport" on the single top-level content <g> (what gets panned/zoomed)
  - the on-canvas zoom/pan/reset button overlay (copied from Doxygen's own generated
    graphs), backed by a matching <defs> of button icon templates

Run after PlantUML renders the SVGs, before they are copied into the HTML output (see
build_docs.bat). Idempotent: a file that already has id="main" is left untouched, so
re-running the whole build is safe.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$SvgDir
)

$ErrorActionPreference = "Stop"

$defsTemplate = @'
<defs>
<circle id="rim" cx="0" cy="0" r="7"/>
<circle id="rim2" cx="0" cy="0" r="3.5"/>
<g id="zoomPlus">
<use xlink:href="#rim" fill="#404040"><set attributeName="fill" to="#808080" begin="zoomplus.mouseover" end="zoomplus.mouseout"/></use>
<path d="M-4,0h8M0,-4v8" fill="none" stroke="white" stroke-width="1.5" pointer-events="none"/>
</g>
<g id="zoomMin">
<use xlink:href="#rim" fill="#404040"><set attributeName="fill" to="#808080" begin="zoomminus.mouseover" end="zoomminus.mouseout"/></use>
<path d="M-4,0h8" fill="none" stroke="white" stroke-width="1.5" pointer-events="none"/>
</g>
<g id="arrowUp" transform="translate(30 24)">
<use xlink:href="#rim"/>
<path pointer-events="none" fill="none" stroke="white" stroke-width="1.5" d="M0,-3.0v7 M-2.5,-0.5L0,-3.0L2.5,-0.5"/>
</g>
<g id="arrowRight" transform="rotate(90) translate(36 -43)">
<use xlink:href="#rim"/>
<path pointer-events="none" fill="none" stroke="white" stroke-width="1.5" d="M0,-3.0v7 M-2.5,-0.5L0,-3.0L2.5,-0.5"/>
</g>
<g id="arrowDown" transform="rotate(180) translate(-30 -48)">
<use xlink:href="#rim"/>
<path pointer-events="none" fill="none" stroke="white" stroke-width="1.5" d="M0,-3.0v7 M-2.5,-0.5L0,-3.0L2.5,-0.5"/>
</g>
<g id="arrowLeft" transform="rotate(270) translate(-36 17)">
<use xlink:href="#rim"/>
<path pointer-events="none" fill="none" stroke="white" stroke-width="1.5" d="M0,-3.0v7 M-2.5,-0.5L0,-3.0L2.5,-0.5"/>
</g>
<g id="resetDef">
<use xlink:href="#rim2" fill="#404040"><set attributeName="fill" to="#808080" begin="reset.mouseover" end="reset.mouseout"/></use>
</g>
</defs>
'@

$navigatorTemplate = @'
<g id="navigator" transform="translate(0 0)" fill="#404254">
<rect fill="#f2f5e9" fill-opacity="0.5" stroke="#606060" stroke-width=".5" x="0" y="0" width="60" height="60"/>
<use id="zoomplus" xlink:href="#zoomPlus" x="17" y="9" onmousedown="handleZoom(evt,'in')"/>
<use id="zoomminus" xlink:href="#zoomMin" x="42" y="9" onmousedown="handleZoom(evt,'out')"/>
<use id="reset" xlink:href="#resetDef" x="30" y="36" onmousedown="handleReset()"/>
<use id="arrowup" xlink:href="#arrowUp" x="0" y="0" onmousedown="handlePan(0,-1)"/>
<use id="arrowright" xlink:href="#arrowRight" x="0" y="0" onmousedown="handlePan(1,0)"/>
<use id="arrowdown" xlink:href="#arrowDown" x="0" y="0" onmousedown="handlePan(0,1)"/>
<use id="arrowleft" xlink:href="#arrowLeft" x="0" y="0" onmousedown="handlePan(-1,0)"/>
</g>
'@

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

$svgFiles = Get-ChildItem -Path $SvgDir -Filter "*.svg"
if (-not $svgFiles) {
    Write-Host "[patch_uml_svg] No .svg files found under $SvgDir"
    exit 0
}

foreach ($file in $svgFiles) {
    $content = [System.IO.File]::ReadAllText($file.FullName)

    if ($content -match 'id="main"') {
        Write-Host "[patch_uml_svg] $($file.Name): already patched, skipping"
        continue
    }

    $rootMatch = [regex]::Match($content, '^<svg ([^>]*)>')
    if (-not $rootMatch.Success) {
        Write-Warning "[patch_uml_svg] $($file.Name): could not find root <svg> tag, skipping"
        continue
    }

    $widthMatch = [regex]::Match($content, 'width="(\d+)px"')
    $heightMatch = [regex]::Match($content, 'height="(\d+)px"')
    if (-not $widthMatch.Success -or -not $heightMatch.Success) {
        Write-Warning "[patch_uml_svg] $($file.Name): could not determine width/height, skipping"
        continue
    }
    $viewWidth = $widthMatch.Groups[1].Value
    $viewHeight = $heightMatch.Groups[1].Value
    $sectionId = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)

    $groupMatches = [regex]::Matches($content, '<g font-family="sans-serif"')
    if ($groupMatches.Count -ne 1) {
        Write-Warning "[patch_uml_svg] $($file.Name): expected exactly one top-level content <g>, found $($groupMatches.Count), skipping"
        continue
    }

    # 1. Root <svg> tag: add id="main" onload="init(evt)".
    $newRootTag = '<svg ' + $rootMatch.Groups[1].Value + ' id="main" onload="init(evt)">'
    $content = $content.Substring(0, $rootMatch.Index) + $newRootTag + $content.Substring($rootMatch.Index + $rootMatch.Length)

    # 2. Right after the root tag: viewWidth/viewHeight/sectionId globals svg.min.js reads,
    #    then the script include itself (vendored copy of Doxygen's INTERACTIVE_SVG script).
    $scriptBlock = @"
<script type="application/ecmascript">
var viewWidth = $viewWidth;
var viewHeight = $viewHeight;
var sectionId = '$sectionId';
</script>
<script type="application/ecmascript" xlink:href="svg.min.js"/>
"@
    $insertAt = $rootMatch.Index + $newRootTag.Length
    $content = $content.Substring(0, $insertAt) + $scriptBlock + $content.Substring($insertAt)

    # 3. Merge the navigator's button-icon templates into PlantUML's empty <defs/>.
    $content = $content -replace '<defs/>', $defsTemplate

    # 4. Retarget the single top-level content <g> to id="viewport" - the element
    #    svg.min.js pans/zooms via a native SVG transform attribute (stays vector-crisp
    #    at any zoom level, unlike a CSS transform on a rasterized <img>).
    $content = $content -replace '<g font-family="sans-serif"', '<g id="viewport" font-family="sans-serif"'

    # 5. On-canvas zoom/pan/reset overlay, as a sibling right before the final closing
    #    </svg> so it stays fixed on screen, unaffected by the viewport's own transform.
    $lastSvgClose = $content.LastIndexOf('</svg>')
    if ($lastSvgClose -lt 0) {
        Write-Warning "[patch_uml_svg] $($file.Name): could not find closing </svg>, skipping navigator overlay"
    } else {
        $content = $content.Substring(0, $lastSvgClose) + $navigatorTemplate + $content.Substring($lastSvgClose)
    }

    [System.IO.File]::WriteAllText($file.FullName, $content, $utf8NoBom)
    Write-Host "[patch_uml_svg] $($file.Name): patched (viewWidth=$viewWidth viewHeight=$viewHeight)"
}
