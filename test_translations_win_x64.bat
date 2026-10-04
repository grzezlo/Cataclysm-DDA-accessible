@echo off
setlocal

rem Check the translation layer scripts with fixture catalogs.
rem Usage: test_translations_win_x64.bat

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1
call "%CATA_ROOT%\lang\translation_env.bat"
if errorlevel 1 exit /b 1

pushd "%CATA_ROOT%"
"%CATA_BASH%" lang/test_access_po.sh %*
set "CATA_RESULT=%ERRORLEVEL%"
popd
exit /b %CATA_RESULT%
