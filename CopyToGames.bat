@echo off
if "%username%" NEQ "Bo3b" exit

@echo on
echo 
xcopy "C:\Users\Bo3b\Documents\Code\3DMigoto\Debug\*.*" "T:\SteamLibrary\SteamApps\common\Assassin's Creed 3\"  /F /Y
echo 
xcopy "C:\Users\Bo3b\Documents\Code\3DMigoto\Debug\*.*" "T:\Bootleg\Assassin's Creed IV - Black Flag\"  /F /Y
echo 
xcopy "C:\Users\Bo3b\Documents\Code\3DMigoto\Debug\*.*" "T:\SteamLibrary\SteamApps\common\BioShock Infinite\Binaries\Win32\" /F /Y
