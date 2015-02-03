@echo off

REM Publish batch file to safely build a complete Zip file to be published.
REM This takes it out of being a manual operation, and will always build and
REM zip the current code.
REM
REM To use this, create an external tool in Tools menu, and set:
REM  Title:              Zip Release
REM  Command:            %systemroot%\system32\cmd.exe
REM  Arguments:          /k Publish.bat
REM  Initial Directory:  $(SolutionDir)
REM
REM TODO: change Win32 to x86, it's annoying
REM TODO: remove all invalid build targets like x32 for Mordor.


REM Activate the VsDevCmds so that we can do msbuild easily.

CALL "C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat"

echo(
echo(
echo === Deep Cleaning Output Directories ===
@echo on
RMDIR ".\Zip Release\" /S /Q
RMDIR ".\x64\Zip Release\" /S /Q
@echo off

echo(
echo(
echo === Building Win32 target ===
echo(
MSBUILD StereoVisionHacks.sln /p:Configuration="Zip Release" /p:Platform=Win32 /v:minimal
echo(
echo(
echo === Building x64 target ===
echo(
MSBUILD StereoVisionHacks.sln /p:Configuration="Zip Release" /p:Platform=x64 /v:minimal


REM Use 7zip command tool to create a full release that can be unzipped into a
REM game directory. This builds a Side-by-Side zip of x32/x64.
REM Includes d3dx.ini and uninstall.bat for x32/x64.

echo(
echo(
echo === Move builds to target zip directory ===
MKDIR ".\Zip Release\x32\"
MOVE ".\Zip Release\d3dx.ini"  ".\Zip Release\x32\"
MOVE ".\Zip Release\uninstall.bat"  ".\Zip Release\x32\"
MOVE ".\Zip Release\*.dll"  ".\Zip Release\x32\"

echo(
MKDIR ".\Zip Release\x64\"
MOVE ".\x64\Zip Release\d3dx.ini"  ".\Zip Release\x64\"
MOVE ".\x64\Zip Release\uninstall.bat"  ".\Zip Release\x64\"
MOVE ".\x64\Zip Release\*.dll"  ".\Zip Release\x64\"

echo(
echo(
echo === Create Zip release for x32 and x64  ===
7zip\7za a ".\Zip Release\3Dmigoto-0.99.xx.zip"   ".\Zip Release\x32\"
7zip\7za a ".\Zip Release\3Dmigoto-0.99.xx.zip"   ".\Zip Release\x64\"

PAUSE
EXIT
