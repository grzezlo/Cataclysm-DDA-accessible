@echo off
setlocal

rem Update the access fork catalogs in lang\access from the current sources
rem and, with --machine, the machine translation layer in lang\machine.
rem Usage: update_translations_win_x64.bat [--base revision] [--machine] [language...]

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1
call "%CATA_ROOT%\lang\translation_env.bat"
if errorlevel 1 exit /b 1

pushd "%CATA_ROOT%"
"%CATA_BASH%" lang/update_access_po.sh %*
set "CATA_RESULT=%ERRORLEVEL%"
popd
exit /b %CATA_RESULT%
