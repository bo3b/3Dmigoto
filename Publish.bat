@echo off
SetLocal EnableDelayedExpansion
Pushd "%~dp0"

REM -----------------------------------------------------------------------------
REM Updated 6-1-25 to add an input parameter to determine whether to bump
REM the version number or not. For regular builds (zip_release.yml) we bump the 
REM build and git commit that new version.h. 
REM For tagged Releases (release_on_tag.yml), we want to keep the version as tagged.

REM Updated 8-25-24 for Github Actions server environment.
REM Removes Git Stash because environment will always be clean.
REM Remove MSBuild and Git setup as they are done in the Action script.
REM Remove the git push of new version, now found in Action script.
REM Removes the 7zip process, because artifact downloads are auto-zipped.

REM Updated 7-2-22 to add an input parameter to decide SpatialLabs 
REM build or not. This is passed in from Github Action as %1=SPATIAL_LABS

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
REM TODO: change Win32 to x32, it's annoying


REM -----------------------------------------------------------------------------
REM Before doing a build, let's bump the version number of the project. 
REM This is stored in the version.h file at the project root, and is used 
REM during resource file compiles to build the proper output in the DLLs.
REM
REM This awesome batch sequence to auto-increment VERSION_REVISION is courtesy of
REM   *** TsaebehT ***
REM -----------------------------------------------------------------------------
REM For regular builds, we'll bump the Build number here. 
REM For Release builds, we calculate this but skip saving to keep the tagged build version.

For /F "tokens=1,2 delims=[]" %%? in ('Type Version.h ^| Find /V /N ""') do (
	Set "Line=%%@"
	if "!Line:~0,16!" == "#define VERSION_" (
		For /F "tokens=3,4 delims=_ " %%? in ("%%@") do (
			if "%%?" == "MAJOR" Set "Major=%%@"
			if "%%?" == "MINOR" Set "Minor=%%@"
			if "%%?" == "REVISION" (
				Set /A "NewRev=%%@+1","OldRev=%%@"
				Call Set "Line=%%Line:!OldRev!=!NewRev!%%"
			)
		)
	)
	Set "Line%%?=!Line!"&&Set "Count=%%?"
)

REM If it's a Release build we won't change the Version.h file, so the
REM build numbers are whatever the last zip_release calculated.
REM
REM This sequence rewrites the whole file using the Line array.
if [%1] NEQ [RELEASE] (
	> Version.h (
		For /L %%? in (1,1,!Count!) do (
			Echo/!Line%%?!
		)
	)
) else (
	echo(
	echo(
	echo === Creating Release Build ===
)

echo(
echo(
echo === Latest Version Info ===
echo(
echo   !Major!.!Minor!.!NewRev!

echo(
echo(
echo === Building Win32 target ===
echo(
MSBUILD /M StereoVisionHacks.sln  /p:Configuration="Zip Release" /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.19041.0 /v:minimal /target:rebuild
IF %ERRORLEVEL% NEQ 0 (
	Echo *** x32 BUILD FAIL ***  
	Echo   Result: %ERRORLEVEL% 
	EXIT 1)

echo(
echo(
echo === Building x64 target ===
echo(
MSBUILD /M StereoVisionHacks.sln  /p:Configuration="Zip Release" /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.19041.0 /v:minimal /target:rebuild
IF %ERRORLEVEL% NEQ 0 (
	Echo *** x64 BUILD FAIL ***  
	Echo   Result: %ERRORLEVEL% 
	EXIT 1)


REM -----------------------------------------------------------------------------
REM Use 7zip command tool to create a full release that can be dropped into a
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
MOVE ".\builds\x32\Zip Release\*.ini"  ".\Zip Release\x32\"
MOVE ".\builds\x32\Zip Release\uninstall.bat"  ".\Zip Release\x32\"
MOVE ".\builds\x32\Zip Release\*.dll"  ".\Zip Release\x32\"
MOVE ".\builds\x32\Zip Release\ShaderFixes\*.*"  ".\Zip Release\x32\ShaderFixes\"

echo(
MKDIR ".\Zip Release\x64\"
MKDIR ".\Zip Release\x64\ShaderFixes\"
MOVE ".\builds\x64\Zip Release\*.ini"  ".\Zip Release\x64\"
MOVE ".\builds\x64\Zip Release\uninstall.bat"  ".\Zip Release\x64\"
MOVE ".\builds\x64\Zip Release\*.dll"  ".\Zip Release\x64\"
MOVE ".\builds\x64\Zip Release\ShaderFixes\*.*"  ".\Zip Release\x64\ShaderFixes\"

echo(
MKDIR ".\Zip Release\loader\"
MKDIR ".\Zip Release\loader\x32\"
MKDIR ".\Zip Release\loader\x64\"
MOVE ".\builds\x32\Zip Release\3DMigoto Loader.exe"  ".\Zip Release\loader\x32\"
MOVE ".\builds\x64\Zip Release\3DMigoto Loader.exe"  ".\Zip Release\loader\x64\"

echo(
MKDIR ".\Zip Release\cmd_Decompiler\"
MOVE ".\builds\x32\Zip Release\cmd_Decompiler.exe"  ".\Zip Release\cmd_Decompiler\"
COPY ".\builds\Zip Release\x32\d3dcompiler_47.dll"  ".\Zip Release\cmd_Decompiler\"

REM -----------------------------------------------------------------------------
REM Write new version to a file that we can use in Action script too.

echo !Major!.!Minor!.!NewRev! > ".\Zip Release\Version-!Major!.!Minor!.!NewRev!.txt"

Dir /s ".\Zip Release\"
