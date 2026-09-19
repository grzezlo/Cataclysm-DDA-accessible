@echo off
setlocal

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1

call "%CATA_ROOT%\build_tests_win_x64.bat"
if errorlevel 1 exit /b 1

echo Running all tests...
"%CATA_TEST_EXE%" %*
exit /b %ERRORLEVEL%
