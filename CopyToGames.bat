@echo off
if "%username%" NEQ "bo3b" exit

if %2=="amd64" GOTO Copyx64

:Copyx86
@echo on
echo 
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Assassin's Creed 3\"  /F /Y
xcopy "%1*.*" "T:\Bootleg\Assassin's Creed IV - Black Flag\"  /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\BioShock Infinite\Binaries\Win32\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Saints Row IV\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Saints Row the Third\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\F.E.A.R. 3\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Alien Isolation\" /F /Y
xcopy "%1*.*" "T:\Bootleg\Ori and the Blind Forest\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\DefenseGrid2\" /F /Y
xcopy "%1*.*" "T:\Bootleg\DiRT Rally\" /F /Y
xcopy "%1*.*" "T:\Games\Crysis 3\Bin32\" /F /Y

exit


:Copyx64
@echo on
echo 
xcopy "%1*.*" "T:\Games\Watch_Dogs\bin"  /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Call of Duty Ghosts\"  /F /Y
xcopy "%1*.*" "T:\Bootleg\Project CARS\" /F /Y
xcopy "%1*.*" "T:\Bootleg\Dying Light\" /F /Y
xcopy "%1*.*" "T:\Games\The Witcher 3 Wild Hunt\bin\x64\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Metal Gear Solid Ground Zeroes\" /F /Y
xcopy "%1*.*" "T:\SteamLibrary\SteamApps\common\Batman Arkham Knight\Binaries\Win64\" /F /Y
xcopy "%1*.*" "T:\Games\Far Cry 4\bin\" /F /Y
