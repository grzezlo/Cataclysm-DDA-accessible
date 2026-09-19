@echo off
setlocal

call "%~dp0setenv.bat"
if errorlevel 1 exit /b 1

echo Building the Windows x64 tests...
"%CATA_MSBUILD%" "%CATA_SOLUTION%" /m /verbosity:minimal /p:Configuration=Release /p:Platform=x64 "/p:VcpkgInstalledDir=%CATA_VCPKG_INSTALLED%" /t:Cataclysm-test-vcpkg-static
if errorlevel 1 exit /b 1

echo Built "%CATA_TEST_EXE%".
exit /b 0
