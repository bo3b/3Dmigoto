@echo off
SetLocal EnableDelayedExpansion
Pushd "%~dp0"
PATH=%PATH%;C:\Program Files (x86)\Git\bin\

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
REM TODO: remove all invalid build targets like x32 for Mordor.


REM -----------------------------------------------------------------------------
REM Since we are going to autoincrement and Publish a new version, we want to
REM check-in that new version.h file with the latest revision.
REM The users git home may be full of junk though, and we don't want to build
REM stuff they might have half-done.  We'll use "git stash" to temporarily get
REM a clean check-out for building.

echo(
echo(
echo === Git Stash Uncommitted Changes ===

git stash


REM -----------------------------------------------------------------------------
REM Before doing a build, let's bump the version number of the tool. 
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


REM -----------------------------------------------------------------------------
echo(
echo(
echo === Deep Cleaning Output Directories ===
@echo on
RMDIR ".\x32\Zip Release\" /S /Q
RMDIR ".\x64\Zip Release\" /S /Q
RMDIR ".\Zip Release\" /S /Q
@echo off


REM -----------------------------------------------------------------------------
REM Activate the VsDevCmds so that we can do MSBUILD easily.

CALL "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"

REM -----------------------------------------------------------------------------
echo(
echo(
echo === Building Win32 target ===
echo(
MSBUILD StereoVisionHacks.sln /p:Configuration="Zip Release" /p:Platform=Win32 /v:minimal /target:rebuild
echo(
echo(
echo === Building x64 target ===
echo(
MSBUILD StereoVisionHacks.sln /p:Configuration="Zip Release" /p:Platform=x64 /v:minimal /target:rebuild


REM -----------------------------------------------------------------------------
REM Assuming the build completed, check-in the change to the version.h file as
REM the only thing changed in the working directory.

git commit --all --message="Incremental Publish build: !Major!.!Minor!.!NewRev!"


REM -----------------------------------------------------------------------------
REM With the build complete, and the version.h committed as latest change, we need
REM to restore the user's Git environment to what it was using "Git Stash Pop".

echo(
echo(
echo === Git Stash Pop ===

git stash pop


REM -----------------------------------------------------------------------------
REM Use 7zip command tool to create a full release that can be unzipped into a
REM game directory. This builds a Side-by-Side zip of x32/x64.
REM Includes d3dx.ini and uninstall.bat for x32/x64.

echo(
echo(
echo(
echo(
echo === Move builds to target zip directory ===
echo(
MKDIR ".\Zip Release\x32\"
MKDIR ".\Zip Release\x32\ShaderFixes\"
MOVE ".\x32\Zip Release\d3dx.ini"  ".\Zip Release\x32\"
MOVE ".\x32\Zip Release\uninstall.bat"  ".\Zip Release\x32\"
MOVE ".\x32\Zip Release\*.dll"  ".\Zip Release\x32\"
MOVE ".\x32\Zip Release\ShaderFixes\*.*"  ".\Zip Release\x32\ShaderFixes\"

echo(
MKDIR ".\Zip Release\x64\"
MKDIR ".\Zip Release\x64\ShaderFixes\"
MOVE ".\x64\Zip Release\d3dx.ini"  ".\Zip Release\x64\"
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

echo(
echo(
echo === Create Zip release for x32 and x64  ===
7zip\7za a ".\Zip Release\3Dmigoto-!Major!.!Minor!.!NewRev!.zip"   ".\Zip Release\x32\"
7zip\7za a ".\Zip Release\3Dmigoto-!Major!.!Minor!.!NewRev!.zip"   ".\Zip Release\x64\"
7zip\7za a ".\Zip Release\3Dmigoto-!Major!.!Minor!.!NewRev!.zip"   ".\Zip Release\loader\"
7zip\7za a ".\Zip Release\cmd_Decompiler-!Major!.!Minor!.!NewRev!.zip"   ".\Zip Release\cmd_Decompiler\*"

PAUSE
EXIT
