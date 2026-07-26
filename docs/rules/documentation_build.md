# Documentation Build Guide

**Last updated:** 2026-07-26

## Overview

`ncr_picking` has two documentation surfaces:

- **Hand-written docs** under `docs/` (architecture notes, domain docs, rules, backlog). These
  remain the source of truth for *why* and for anything a generator cannot infer.
- **Generated Doxygen API reference** under `docs/generated/doxygen/`, built from the `///`
  Doxygen comments in `src/`, `app/`, and `components/RobotKinematics/` (see
  [design_rules.md](design_rules.md) §18 for the comment style), plus the hand-authored UML
  under `uml/` rendered to SVG and embedded with pan/zoom.

The generated reference complements, not replaces, `docs/generated/architecture_docs/` (manually
curated Markdown, may drift — see `AGENT.md`).

## Tools

Three external tools are required, none of them vendored in the repository:

| Tool | Purpose | Found on this machine under |
|---|---|---|
| Doxygen | Parses `///` comments, generates the HTML reference | `C:\build_packages\doxygen-1.17.0-win64\doxygen.exe` |
| Graphviz (`dot`) | Renders Doxygen's class/collaboration/include graphs | `C:\build_packages\Graphviz-15.1.0-win64\bin\dot.exe` |
| PlantUML (`.jar`, needs a JVM) | Renders `uml/*.puml` to SVG | `C:\build_packages\plantuml\plantuml-java8-SNAPSHOT.jar` |

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

This:
1. Renders every `uml/*.puml` to SVG (`docs/generated/doxygen/uml_svg/`) via PlantUML +
   Graphviz.
2. Runs Doxygen over `src/`, `app/`, and `components/RobotKinematics/{include,src}` (see
   `docs/doxygen/Doxyfile`), plus the custom pages in `docs/doxygen/pages/`.
3. Copies the rendered UML SVGs into the HTML output so
   [architecture_diagrams.dox](../doxygen/pages/architecture_diagrams.dox) can reference them by
   plain filename (Doxygen's `HTML_EXTRA_FILES` does not recursively copy a directory's contents
   in this Doxygen version, so this is done as an explicit post-copy step instead).

Output: `docs/generated/doxygen/html/index.html`. Undocumented/mismatched-declaration warnings:
`docs/generated/doxygen/warnings.log`.

`docs/generated/doxygen/` is a build artifact — regenerate it any time; do not hand-edit anything
under it.

## Architecture Diagrams (Pan/Zoom)

`docs/doxygen/pages/architecture_diagrams.dox` embeds each rendered `uml/*.puml` SVG in a small
self-contained pan/zoom viewer (`docs/doxygen/assets/uml_pan_zoom.{css,js}` — vanilla JS, no
external dependency, wheel-to-zoom / drag-to-pan / reset button, one `<img>` per diagram inside a
`.uml-viewer` container so the JS never needs to reach into the SVG's own DOM).

**Adding a new diagram:** after adding `uml/NN_new_diagram.puml`, add a matching
`@section`/viewer block to `architecture_diagrams.dox` (copy an existing block; the `<img
src="...">` must match the SVG's `@startuml <name>` diagram name, not the `.puml` source
filename — check `docs/generated/doxygen/uml_svg/` after rendering to confirm the output name).

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
