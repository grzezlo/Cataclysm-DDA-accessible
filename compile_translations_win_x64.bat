@echo off
setlocal

rem Compile lang\mo from lang\po, the access fork catalogs in lang\access, and
rem the machine translation layer in lang\machine.
rem Usage: compile_translations_win_x64.bat [--no-machine] [all or language...]

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1
call "%CATA_ROOT%\lang\translation_env.bat"
if errorlevel 1 exit /b 1

pushd "%CATA_ROOT%"
"%CATA_BASH%" lang/compile_access_mo.sh %*
set "CATA_RESULT=%ERRORLEVEL%"
popd
exit /b %CATA_RESULT%
