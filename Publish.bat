@echo off
SetLocal EnableDelayedExpansion
Pushd "%~dp0"

REM -----------------------------------------------------------------------------
REM Updated 8-25-24 for Github Actions server environment.
REM Removes Git Stash because environment will always be clean.
REM Remove MSBuild and Git setup as they are done in the Action script.
REM Remove the git push of new version, now found in Action script.
REM Removes the 7zip process, because artifact downloads are auto-zipped.

REM -----------------------------------------------------------------------------
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


REM -----------------------------------------------------------------------------
REM Before doing a build, let's bump the version number of the project. 
REM This is stored in the version.h file at the project root, and is used 
REM during resource file compiles to build the proper output in the DLLs.
REM
REM This awesome batch sequence to auto-increment VERSION_REVISION is courtesy of
REM   *** TsaebehT ***

For /F "tokens=1,2 delims=[]" %%? in ('Type Version.h ^| Find /V /N ""') do (
Set "Line=%%@"
if "!Line:~0,16!" == "#define VERSION_" (
For /F "tokens=3,4 delims=_ " %%? in ("%%@") do (
if "%%?" == "MAJOR" Set "Major=%%@"
if "%%?" == "MINOR" Set "Minor=%%@"
if "%%?" == "REVISION" (
Set /A "NewRev=%%@+1","OldRev=%%@"
Call Set "Line=%%Line:!OldRev!=!NewRev!%%"
)))
Set "Line%%?=!Line!"&&Set "Count=%%?"
)

> Version.h (
For /L %%? in (1,1,!Count!) do (
Echo/!Line%%?!
))

echo(
echo(
echo === Latest Version After Increment ===
echo(
echo   !Major!.!Minor!.!NewRev!

echo(
echo(
echo === Building Win32 target ===
echo(
MSBUILD StereoVisionHacks.sln /p:Configuration="Zip Release" /p:Platform=Win32 /v:minimal /target:rebuild
IF %ERRORLEVEL% NEQ 0 (
	Echo *** x32 BUILD FAIL ***  
	EXIT 1)

echo(
echo(
echo === Building x64 target ===
echo(
MSBUILD StereoVisionHacks.sln /p:Configuration="Zip Release" /p:Platform=x64 /v:minimal /target:rebuild
IF %ERRORLEVEL% NEQ 0 (
	Echo *** x64 BUILD FAIL ***  
	EXIT 1)


REM -----------------------------------------------------------------------------
REM Use 7zip command tool to create a full release that can be unzipped into a
REM game directory. This builds a Side-by-Side zip of x32/x64.
REM Includes d3dx.ini, d3dxdm.ini, and uninstall.bat for x32/x64.

echo(
echo(
echo(
echo(
echo === Move builds to target zip directory ===
echo(
MKDIR ".\Zip Release\x32\"
MKDIR ".\Zip Release\x32\ShaderFixes\"
MOVE ".\x32\Zip Release\*.ini"  ".\Zip Release\x32\"
MOVE ".\x32\Zip Release\uninstall.bat"  ".\Zip Release\x32\"
MOVE ".\x32\Zip Release\*.dll"  ".\Zip Release\x32\"
MOVE ".\x32\Zip Release\ShaderFixes\*.*"  ".\Zip Release\x32\ShaderFixes\"

echo(
MKDIR ".\Zip Release\x64\"
MKDIR ".\Zip Release\x64\ShaderFixes\"
MOVE ".\x64\Zip Release\*.ini"  ".\Zip Release\x64\"
MOVE ".\x64\Zip Release\uninstall.bat"  ".\Zip Release\x64\"
MOVE ".\x64\Zip Release\*.dll"  ".\Zip Release\x64\"
MOVE ".\x64\Zip Release\ShaderFixes\*.*"  ".\Zip Release\x64\ShaderFixes\"

echo(
MKDIR ".\Zip Release\loader\"
MKDIR ".\Zip Release\loader\x32\"
MKDIR ".\Zip Release\loader\x64\"
MOVE ".\x32\Zip Release\3DMigoto Loader.exe"  ".\Zip Release\loader\x32\"
MOVE ".\x64\Zip Release\3DMigoto Loader.exe"  ".\Zip Release\loader\x64\"

echo(
MKDIR ".\Zip Release\cmd_Decompiler\"
MOVE ".\x32\Zip Release\cmd_Decompiler.exe"  ".\Zip Release\cmd_Decompiler\"
COPY ".\Zip Release\x32\d3dcompiler_47.dll"  ".\Zip Release\cmd_Decompiler\"

REM -----------------------------------------------------------------------------
REM Write new version to a file that we can use in Action script too.

echo !Major!.!Minor!.!NewRev! > ".\Zip Release\Version-!Major!.!Minor!.!NewRev!.txt"

Dir /s ".\Zip Release\"
