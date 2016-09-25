@echo off
if "%username%" NEQ "bo3b" exit

if %2=="amd64" GOTO Copyx64

:Copyx86
@echo on

xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Assassin's Creed 3\"  /F /Y
xcopy /C "%1*.*" "W:\Bootleg\Assassin's Creed IV - Black Flag\"  /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Saints Row IV\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Saints Row the Third\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\F.E.A.R. 3\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Alien Isolation\" /F /Y
xcopy /C "%1*.*" "W:\Bootleg\Ori and the Blind Forest\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\DefenseGrid2\" /F /Y
xcopy /C "%1*.*" "W:\Bootleg\DiRT Rally\" /F /Y

exit


:Copyx64
@echo on
echo 
xcopy /C "%1*.*" "W:\Games\Watch_Dogs\bin"  /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Call of Duty Ghosts\"  /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\ShadowOfMordor\x64\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Project CARS\" /F /Y
xcopy /C "%1*.*" "W:\Games\The Witcher 3 Wild Hunt\bin\x64\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Metal Gear Solid Ground Zeroes\" /F /Y
xcopy /C "%1*.*" "W:\Games\Far Cry 4\bin\" /F /Y
xcopy /C "%1*.*" "W:\SteamLibrary\SteamApps\common\Just Cause 3\" /F /Y
xcopy /C "%1*.*" "W:\Games\The Crew (Worldwide)\" /F /Y
