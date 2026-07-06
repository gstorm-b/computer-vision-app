@echo off
setlocal EnableExtensions

if "%NCR_PICKING_ROOT%"=="" set "NCR_PICKING_ROOT=%~dp0.."
for %%I in ("%NCR_PICKING_ROOT%\.") do set "NCR_PICKING_ROOT=%%~fI"

if "%QT_MSVC_DIR%"=="" (
    echo [ERROR] QT_MSVC_DIR must point to the Qt MSVC kit root.
    exit /b 1
)
if "%VCVARS%"=="" (
    echo [ERROR] VCVARS must point to vcvars64.bat.
    exit /b 1
)
if "%OPENCV_BIN%"=="" (
    echo [ERROR] OPENCV_BIN must point to the OpenCV runtime bin directory.
    exit /b 1
)
if "%PYLON_RUNTIME_DIR%"=="" (
    echo [ERROR] PYLON_RUNTIME_DIR must point to the Basler Pylon runtime directory.
    exit /b 1
)

set "TEST_DIR=%NCR_PICKING_ROOT%\tests\architecture_contract_test"
set "BUILD=%TEST_DIR%\build\msvc_debug"

call "%VCVARS%"
if not defined VCToolsInstallDir (
    echo [ERROR] Visual Studio environment was not initialized.
    exit /b 1
)

set "PATH=%QT_MSVC_DIR%\bin;%BUILD%\debug;%OPENCV_BIN%;%PYLON_RUNTIME_DIR%;%PATH%"

if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%" || exit /b 1

echo === Running qmake ===
qmake -o Makefile "%TEST_DIR%\architecture_contract_test.pro" -spec win32-msvc CONFIG+=debug
if errorlevel 1 (
    echo [ERROR] qmake failed.
    exit /b 1
)

echo === Running moc target ===
nmake /nologo -f Makefile.Debug compiler_moc_source_make_all
if errorlevel 1 (
    echo [ERROR] moc target failed.
    exit /b 1
)

echo === Running nmake ===
nmake /nologo
if errorlevel 1 (
    echo [ERROR] nmake failed.
    exit /b 1
)

echo === BUILD SUCCESS ===
