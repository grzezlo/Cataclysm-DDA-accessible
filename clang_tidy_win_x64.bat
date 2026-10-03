@echo off
setlocal

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1

set "CATA_CMAKE=%CATA_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CATA_NINJA=%CATA_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "CATA_TIDY=%CATA_VS%\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
set "CATA_VCVARS=%CATA_VS%\VC\Auxiliary\Build\vcvars64.bat"
if not defined CATA_TIDY_BUILD set "CATA_TIDY_BUILD=%CATA_ROOT%\out\build\clang-tidy-win-x64"

for %%F in ("%CATA_CMAKE%" "%CATA_NINJA%" "%CATA_TIDY%" "%CATA_VCVARS%") do (
    if not exist %%F (
        echo ERROR: Missing tool %%F. Install the Visual Studio C++ and LLVM components. 1>&2
        exit /b 1
    )
)

where msgfmt.exe >nul 2>nul
if errorlevel 1 if exist "C:\gettext\bin\msgfmt.exe" set "PATH=C:\gettext\bin;%PATH%"
where msgfmt.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: msgfmt.exe is not on PATH. Install gettext or add its bin directory to PATH. 1>&2
    exit /b 1
)

call "%CATA_VCVARS%" >nul
if errorlevel 1 exit /b 1

rem CMake's MSVC toolchain forces /TP even on its C compiler probe. The MSVC
rem compiler is verified by the regular Windows build, so bypass that probe.
rem Do not let this analysis-only configure change the canonical vcpkg tree.
rem The current static SDL2_mixer package lacks a CMake target, so analyze the
rem tiles configuration without sound and verify the full game with MSBuild.
"%CATA_CMAKE%" -S "%CATA_ROOT%" -B "%CATA_TIDY_BUILD%" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM="%CATA_NINJA%" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER_WORKS=ON ^
    -DCMAKE_TOOLCHAIN_FILE="%CATA_ROOT%\build-scripts\MSVC.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
    -DVCPKG_HOST_TRIPLET=x64-windows-static ^
    -DVCPKG_INSTALLED_DIR="%CATA_VCPKG_INSTALLED%" ^
    -DVCPKG_MANIFEST_INSTALL=OFF -DDYNAMIC_LINKING=OFF ^
    -DTILES=ON -DCURSES=OFF -DSOUND=OFF -DLOCALIZE=ON -DTESTS=ON
if errorlevel 1 exit /b 1

set "CATA_TIDY_FAILED=0"
pushd "%CATA_ROOT%"
if not "%~1"=="" goto explicit_files

for %%F in (src\accessibility.cpp src\accessibility_format.cpp ^
    src\accessibility_gameplay.cpp src\accessibility_document_view.cpp ^
    src\accessibility_nvda.cpp tests\accessibility_service_test.cpp ^
    tests\accessibility_format_test.cpp tests\accessibility_gameplay_test.cpp ^
    tests\accessibility_ui_test.cpp) do (
    call :check_file "%%F"
    if errorlevel 1 set "CATA_TIDY_FAILED=1"
)
goto done

:explicit_files
if "%~1"=="" goto done
call :check_file "%~1"
if errorlevel 1 set "CATA_TIDY_FAILED=1"
shift
goto explicit_files

:check_file
if not exist "%~1" (
    echo ERROR: File not found: %~1 1>&2
    exit /b 1
)
echo Checking %~1...
rem LLVM 19 adds a false positive for the public backend factory. The upstream
rem project checks run on LLVM 18 with a separate cata-* plugin in Linux CI.
"%CATA_TIDY%" -p "%CATA_TIDY_BUILD%" -quiet -header-filter="^$" ^
    -checks=-misc-use-internal-linkage ^
    --extra-arg=-Wno-overriding-option ^
    --extra-arg=-Wno-unused-command-line-argument "%~f1"
exit /b %errorlevel%

:done
popd
exit /b %CATA_TIDY_FAILED%
