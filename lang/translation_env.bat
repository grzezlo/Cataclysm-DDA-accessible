@echo off

rem Locate the Git for Windows shell and gettext for the translation scripts.
rem Call this after setenv.bat; it sets CATA_BASH and may extend PATH.

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
exit /b 0
