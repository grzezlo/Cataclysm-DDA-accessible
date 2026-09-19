@echo off

rem Locate the repository, Visual Studio, MSBuild, and the bundled vcpkg without
rem relying on machine-specific installation paths.
for %%I in ("%~dp0.") do set "CATA_ROOT=%%~fI"

rem Some automation hosts provide both Path and PATH.  MSBuild treats those as
rem duplicate environment keys, so collapse them into one entry.
set "CATA_SAVED_PATH=%PATH%"
set "Path="
set "PATH="
set "PATH=%CATA_SAVED_PATH%"
set "CATA_SAVED_PATH="

set "CATA_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%CATA_VSWHERE%" (
    echo ERROR: vswhere.exe was not found. Install Visual Studio 2022 with 1>&2
    echo        Desktop development with C++ and the vcpkg component. 1>&2
    exit /b 1
)

set "CATA_VS="
for /f "usebackq delims=" %%I in (`"%CATA_VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.Vcpkg -property installationPath`) do set "CATA_VS=%%I"

if not defined CATA_VS (
    echo ERROR: A suitable Visual Studio installation was not found. 1>&2
    echo        Install MSBuild, the x64 C++ tools, and the vcpkg component. 1>&2
    exit /b 1
)

set "CATA_MSBUILD=%CATA_VS%\MSBuild\Current\Bin\MSBuild.exe"
set "VCPKG_ROOT=%CATA_VS%\VC\vcpkg"
set "CATA_VCPKG=%VCPKG_ROOT%\vcpkg.exe"
set "CATA_VCPKG_INSTALLED=%CATA_ROOT%\msvc-full-features\vcpkg_installed"
set "CATA_SOLUTION=%CATA_ROOT%\msvc-full-features\Cataclysm-vcpkg-static.sln"
set "CATA_TEST_EXE=%CATA_ROOT%\Cataclysm-test-vcpkg-static-Release-x64.exe"
set "CATA_GAME_EXE=%CATA_ROOT%\cataclysm-tiles.exe"

if not exist "%CATA_MSBUILD%" (
    echo ERROR: MSBuild was not found at "%CATA_MSBUILD%". 1>&2
    exit /b 1
)
if not exist "%CATA_VCPKG%" (
    echo ERROR: vcpkg was not found at "%CATA_VCPKG%". 1>&2
    exit /b 1
)
if not exist "%CATA_SOLUTION%" (
    echo ERROR: The Cataclysm solution was not found at "%CATA_SOLUTION%". 1>&2
    exit /b 1
)

exit /b 0
