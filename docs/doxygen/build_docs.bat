@echo off
setlocal enabledelayedexpansion

rem ---------------------------------------------------------------------
rem Builds the Doxygen developer reference for ncr_picking:
rem   1. Renders uml/*.puml to SVG (PlantUML + Graphviz).
rem   2. Runs Doxygen over src/, app/, and components/RobotKinematics.
rem
rem All tool locations are environment variables so this works across
rem machines (see docs/rules/documentation_build.md). Defaults below match
rem the copies found under C:\build_packages on this machine; override any
rem of them before calling this script if your machine differs.
rem ---------------------------------------------------------------------

if not defined NCR_PICKING_ROOT (
    for %%I in ("%~dp0..\..") do set "NCR_PICKING_ROOT=%%~fI"
)

if not defined DOXYGEN_EXE set "DOXYGEN_EXE=C:\build_packages\doxygen-1.17.0-win64\doxygen.exe"
if not defined GRAPHVIZ_DOT_DIR set "GRAPHVIZ_DOT_DIR=C:\build_packages\Graphviz-15.1.0-win64\bin"
rem plantuml-1.2026.6.jar in the same folder needs Java 11+; this repo's
rem local machine only has Java 8, so default to the Java-8-compatible jar.
if not defined PLANTUML_JAR set "PLANTUML_JAR=C:\build_packages\plantuml\plantuml-java8-SNAPSHOT.jar"
if not defined JAVA_EXE set "JAVA_EXE=java"

if not exist "%DOXYGEN_EXE%" (
    echo [build_docs] DOXYGEN_EXE not found: %DOXYGEN_EXE%
    exit /b 1
)
if not exist "%GRAPHVIZ_DOT_DIR%\dot.exe" (
    echo [build_docs] dot.exe not found under GRAPHVIZ_DOT_DIR: %GRAPHVIZ_DOT_DIR%
    exit /b 1
)
if not exist "%PLANTUML_JAR%" (
    echo [build_docs] PLANTUML_JAR not found: %PLANTUML_JAR%
    exit /b 1
)

set "DOXY_DIR=%NCR_PICKING_ROOT%\docs\doxygen"
set "GEN_DIR=%NCR_PICKING_ROOT%\docs\generated\doxygen"
set "UML_SVG_DIR=%GEN_DIR%\uml_svg"
set "UML_SRC_DIR=%NCR_PICKING_ROOT%\uml"

if not exist "%GEN_DIR%" mkdir "%GEN_DIR%"
if not exist "%UML_SVG_DIR%" mkdir "%UML_SVG_DIR%"

echo [build_docs] Rendering uml\*.puml to SVG via PlantUML...
set "GRAPHVIZ_DOT=%GRAPHVIZ_DOT_DIR%\dot.exe"
"%JAVA_EXE%" -jar "%PLANTUML_JAR%" -tsvg -o "%UML_SVG_DIR%" "%UML_SRC_DIR%\*.puml"
if errorlevel 1 (
    echo [build_docs] PlantUML render failed.
    exit /b 1
)

echo [build_docs] Running Doxygen...
cd /d "%DOXY_DIR%" || exit /b 1
"%DOXYGEN_EXE%" Doxyfile
if errorlevel 1 (
    echo [build_docs] Doxygen failed.
    exit /b 1
)

echo [build_docs] Copying rendered UML SVGs into the HTML output...
rem Doxygen's HTML_EXTRA_FILES does not recursively copy a directory's
rem contents in this Doxygen version, so the rendered diagrams (referenced
rem by plain filename from architecture_diagrams.dox) are copied here
rem instead. Re-running this script always refreshes them.
copy /Y "%UML_SVG_DIR%\*.svg" "%GEN_DIR%\html\" >nul
if errorlevel 1 (
    echo [build_docs] Copying UML SVGs failed.
    exit /b 1
)

echo [build_docs] Done. HTML output: %GEN_DIR%\html\index.html
echo [build_docs] Undocumented-symbol warnings: %GEN_DIR%\warnings.log
