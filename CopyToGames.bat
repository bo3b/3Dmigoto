@echo off
if "%username%" NEQ "Bo3b" exit

if %2=="amd64" GOTO Copyx64

:Copyx86
@echo on
echo 
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Assassin's Creed 3\"  /F /Y
echo 
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\BioShock Infinite\Binaries\Win32\" /F /Y
echo 
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Saints Row the Third\" /F /Y
echo 
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\F.E.A.R. 3\" /F /Y

exit


:Copyx64
@echo on
