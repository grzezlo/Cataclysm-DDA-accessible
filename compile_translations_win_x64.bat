@echo off
setlocal

rem Compile lang\mo from lang\po and the access fork catalogs in lang\access.
rem Usage: compile_translations_win_x64.bat [all or language...]

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1

rem The translation scripts run in the Git for Windows shell.
set "CATA_BASH="
for /f "delims=" %%G in ('where git.exe 2^>nul') do (
    if not defined CATA_BASH if exist "%%~dpG..\bin\bash.exe" set "CATA_BASH=%%~dpG..\bin\bash.exe"
    if not defined CATA_BASH if exist "%%~dpG..\..\bin\bash.exe" set "CATA_BASH=%%~dpG..\..\bin\bash.exe"
)
if not defined CATA_BASH (
    echo ERROR: Git for Windows bash.exe was not found. Install Git for Windows. 1>&2
    exit /b 1
)

where msgfmt.exe >nul 2>nul
if errorlevel 1 if exist "C:\gettext\bin\msgfmt.exe" set "PATH=C:\gettext\bin;%PATH%"
where msgfmt.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: msgfmt.exe is not on PATH. Install gettext or add its bin directory to PATH. 1>&2
    exit /b 1
)

pushd "%CATA_ROOT%"
"%CATA_BASH%" lang/compile_access_mo.sh %*
set "CATA_RESULT=%ERRORLEVEL%"
popd
exit /b %CATA_RESULT%
