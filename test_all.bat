@echo off
setlocal

rem Usage: test_all.bat [Catch2 arguments]
rem Without arguments, run the full suite as the Windows CI does: lexical
rem order, a random seed, the compiled TEST_DATA catalog, and a fresh user
rem directory, so tests that depend on state left by other tests fail here
rem before they are pushed.  Arguments replace these defaults.

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1

call "%CATA_ROOT%\build_tests_win_x64.bat"
if errorlevel 1 exit /b 1

if not "%~1"=="" goto run_arguments

where msgfmt.exe >nul 2>nul
if errorlevel 1 if exist "C:\gettext\bin\msgfmt.exe" set "PATH=C:\gettext\bin;%PATH%"
where msgfmt.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: msgfmt.exe is not on PATH. Install gettext or add its bin directory to PATH. 1>&2
    exit /b 1
)
set "CATA_TEST_MO_DIR=%CATA_ROOT%\data\mods\TEST_DATA\lang\mo\ru\LC_MESSAGES"
if not exist "%CATA_TEST_MO_DIR%" mkdir "%CATA_TEST_MO_DIR%"
msgfmt.exe -f -o "%CATA_TEST_MO_DIR%\TEST_DATA.mo" "%CATA_ROOT%\data\mods\TEST_DATA\lang\po\ru.po"
if errorlevel 1 exit /b 1

set "CATA_TEST_USER_DIR=%CATA_ROOT%\out\test_user_dir_full"
if exist "%CATA_TEST_USER_DIR%" rmdir /s /q "%CATA_TEST_USER_DIR%"

echo Running all tests as the Windows CI does...
pushd "%CATA_ROOT%"
"%CATA_TEST_EXE%" --min-duration 20 --rng-seed time --order lex --user-dir="out/test_user_dir_full/"
set "CATA_RESULT=%ERRORLEVEL%"
popd
exit /b %CATA_RESULT%

:run_arguments
echo Running all tests...
"%CATA_TEST_EXE%" %*
exit /b %ERRORLEVEL%
