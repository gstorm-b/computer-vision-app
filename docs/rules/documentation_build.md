# Documentation Build Guide

**Last updated:** 2026-08-23 (Phase 7 close-out rebuild)

## Overview

`ncr_picking` has two documentation surfaces:

- **Hand-written docs** under `docs/` (architecture notes, domain docs, rules, backlog). These
  remain the source of truth for *why* and for anything a generator cannot infer.
- **Generated Doxygen API reference** under `docs/generated/doxygen/`, built from the `///`
  Doxygen comments in `src/`, `app/`, `runtime_app/`, and `components/RobotKinematics/` (see
  [design_rules.md](design_rules.md) §18 for the comment style), plus the hand-authored UML
  under `uml/` rendered to SVG and embedded with pan/zoom.

> ⚠️ **A new top-level source root does not appear in the reference by itself.** `runtime_app/`
> was added in Phase 6 and stayed invisible in the generated output until Phase 7 close-out,
> because the Doxyfile's `INPUT` lists roots explicitly (`RECURSIVE` only descends into the roots
> already listed). Add the root to `INPUT` in the same change that creates it.

The generated reference complements, not replaces, `docs/generated/architecture_docs/` (manually
curated Markdown, may drift — see `AGENT.md`).

## Tools

Three external tools are required, none of them vendored in the repository:

| Tool | Purpose | Found on this machine under |
|---|---|---|
| Doxygen | Parses `///` comments, generates the HTML reference | `C:\build_packages\doxygen-1.17.0-win64\doxygen.exe` |
| Graphviz (`dot`) | Renders Doxygen's class/collaboration/include graphs | `C:\build_packages\Graphviz-15.1.0-win64\bin\dot.exe` |
| PlantUML (`.jar`, needs a JVM) | Renders `uml/*.puml` to SVG | `C:\build_packages\plantuml\plantuml-java8-SNAPSHOT.jar` |

> ⚠️ **`build_docs.bat`'s built-in defaults currently point at `C:\BAO\...`, which does not exist
> on this machine** (changed in commit `51469ecf`). Until the script and the machine agree, the
> build only runs with the three environment variables set explicitly — see below. The script
> fails loudly (`[build_docs] DOXYGEN_EXE not found: ...`, exit 1) rather than producing a partial
> reference, so this cannot pass unnoticed.

**Java note.** The local machine only has Java 8 (`java -version` → `1.8.0_501`). The newer
`plantuml-1.2026.6.jar` in the same folder requires Java 11+ (it fails with
`UnsupportedClassVersionError`) — use `plantuml-java8-SNAPSHOT.jar` instead. If this machine (or
a future one) gets a Java 11+ runtime, either jar will work; prefer the newer one for its bug
fixes and set `PLANTUML_JAR` accordingly (see below).

## Environment Variables

Per the project convention (see [build_and_verification.md](build_and_verification.md)), tool
locations are environment variables, never hard-coded in the Doxyfile or scripts. All of them
have a default in `docs/doxygen/build_docs.bat` matching the paths above — override any of them
before calling the script if your machine differs:

- `NCR_PICKING_ROOT` — repository root (already used elsewhere in the project).
- `DOXYGEN_EXE` — full path to `doxygen.exe`.
- `GRAPHVIZ_DOT_DIR` — directory containing `dot.exe` (not the exe path itself).
- `PLANTUML_JAR` — full path to the PlantUML `.jar` to run.
- `JAVA_EXE` — the `java` command/path used to run the PlantUML jar (default: `java`, i.e.
  whatever resolves on `PATH`).

The Doxyfile itself reads `$(NCR_PICKING_ROOT)`, `$(GRAPHVIZ_DOT_DIR)`, and `$(PLANTUML_JAR)`
directly (Doxygen config files support `$(ENV_VAR)` substitution) — set these three before
running `doxygen` by hand instead of through the wrapper script.

## Building The Docs

```bat
docs\doxygen\build_docs.bat
```

On this machine the defaults do not resolve (see the warning above), so set the three tool
variables first:

```bat
set "DOXYGEN_EXE=C:\build_packages\doxygen-1.17.0-win64\doxygen.exe" && set "GRAPHVIZ_DOT_DIR=C:\build_packages\Graphviz-15.1.0-win64\bin" && set "PLANTUML_JAR=C:\build_packages\plantuml\plantuml-java8-SNAPSHOT.jar" && docs\doxygen\build_docs.bat
```

This:
1. Renders every `uml/*.puml` to SVG (`docs/generated/doxygen/uml_svg/`) via PlantUML +
   Graphviz.
2. Patches each rendered SVG for pan/zoom (see below).
3. Runs Doxygen over `src/`, `app/`, `runtime_app/`, and
   `components/RobotKinematics/{include,src}` (see `docs/doxygen/Doxyfile`), plus the custom pages
   in `docs/doxygen/pages/`.
4. Copies the rendered UML SVGs into the HTML output so
   [architecture_diagrams.dox](../doxygen/pages/architecture_diagrams.dox) can reference them by
   plain filename (Doxygen's `HTML_EXTRA_FILES` does not recursively copy a directory's contents
   in this Doxygen version, so this is done as an explicit post-copy step instead).

Output: `docs/generated/doxygen/html/index.html`. Undocumented/mismatched-declaration warnings:
`docs/generated/doxygen/warnings.log`.

`docs/generated/doxygen/` is a build artifact — regenerate it any time; do not hand-edit anything
under it.

## Architecture Diagrams (Pan/Zoom)

The rendered UML uses **Doxygen's own `svg.min.js`** — the same `INTERACTIVE_SVG` mechanism its
generated class and collaboration graphs use — rather than a custom viewer.
`docs/doxygen/scripts/patch_uml_svg.ps1` gives each PlantUML-rendered SVG the structure that
script expects (`id="main"` + `onload="init(evt)"` on the root, the `viewWidth`/`viewHeight`/
`sectionId` globals, `id="viewport"` on the content `<g>`, and the on-canvas zoom/pan/reset
overlay). `architecture_diagrams.dox` then embeds each diagram in an `<iframe>`.

**This replaced a hand-rolled `<img>` + CSS-transform viewer** (`assets/uml_pan_zoom.{css,js}`,
now deleted) for two concrete reasons, both recorded in the script's header comment: an SVG
embedded as `<img>` is rasterized once at layout size, so CSS `scale()` zoom blurs it; and a pan
handler that never calls `preventDefault()` gets hijacked by the browser's native text-selection
drag after the first drag. The Doxygen script zooms via a native SVG `transform="matrix(...)"`, so
it stays vector-crisp, and calls `preventDefault()` throughout.

The patch script is safe to re-run: a file already carrying `id="main"` is skipped. It also
refuses rather than guesses — an SVG whose width/height it cannot read, or that does not have
exactly one top-level content `<g>`, is warned about and left alone.

**Adding a new diagram:** after adding `uml/NN_new_diagram.puml`, add a matching `@section` +
`@htmlonly`/`<iframe>` block to `architecture_diagrams.dox` (copy an existing block; the `src="..."`
must match the SVG's `@startuml <name>` diagram name, **not** the `.puml` source filename — check
`docs/generated/doxygen/uml_svg/` after rendering to confirm the output name).

> Rendering a `.puml` is not the same as publishing it. `11_runtime_shell.puml` rendered correctly
> from the day it was written and was still absent from the reference, because nothing on
> `architecture_diagrams.dox` pointed at it. The two steps are separate; do both.

## Auditing Doc Coverage

The Doxyfile ships with `EXTRACT_ALL = YES` (also `EXTRACT_PRIVATE`/`EXTRACT_STATIC`/etc.), which
is what makes every class/method — including private ones — show up in the output. This also
makes `WARN_IF_UNDOCUMENTED` a no-op: with `EXTRACT_ALL = YES`, Doxygen treats every entity as
documented regardless of whether a `///` comment exists. To audit real coverage gaps, run a
throwaway pass with it flipped off:

```bat
"%DOXYGEN_EXE%" -x Doxyfile
```

(`-x` prints config-recommended potential problems) or temporarily set `EXTRACT_ALL = NO` in a
copy of the Doxyfile and inspect `warnings.log` for `warning: Member ... is not documented`
entries, then discard that copy — never commit a Doxyfile with `EXTRACT_ALL = NO`.

## Comment Style

New and modified code must use the Doxygen `///` style described in
[design_rules.md](design_rules.md) §18, not because *this* file requires it but because that is
now the project's documented comment convention going forward.
