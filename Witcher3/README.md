The Witcher 3: Wild Hunt
========================
A fix by mike_ar69 & DarkStarSword
Profile and 1.10/1.21/1.31 updates by Helifax
3DMigoto improvements by Bo3b and DarkStarSword
Assembler by Flugan
Special thanks to everyone on the forum who helped out with testing!

Latest update 12-4-16, works with both Hearts of Stone and Blood and Wine expansions:

The Witcher 3 - Patch 1.31 Update (12-4-16)
-------------------------------------------
- 3DMigoto 1.2.50
- Profile is now part of the fix and is automatically deployed
- Dynamic-crosshair is back
- stereo2mono updated version so it doesn't kill the SLI bus in 3D Surround.
- Fixed HBAO+ normal map artefacts following DarkStarSword's pattern (thanks mx-2!)
- Fixes the Steam overlay and Steam controller in Windows 7 and Windows 8

Big thank you to DarkStarSword and Bo3b for all their work on this update of 3DMigoto!

The Witcher 3 - Patch 1.21 Update
---------------------------------
- **Decals (quite a lot of them)**
- **Rain splashes /drops**
- **Blood**
- **Hairworks shadows**
- **Fixed witcher senses bubbles & warp bubbles so they now are at proper depth.**
- **Crosshair is pushed to DEPTH**
- **All other UI is still at FIXED depth.**
- **Stars at night (thx SKAUT) for the savegame and pointing it out;)**
- I added presets for different convergence levels on Keys "Z" and
  "XBOX-RIGHT_THUMB_PRESS". - The Witcher Senses bubbles are also aligned for
  these 2 convergence values and might not work for others (haven't tested).
- I recommend using these presets to play the game (if you aren't already using
  something like this). One if for Cutscenes/Interior the other is for
  outside:)

Fixed
-----
- Lights
- Shadows
- Global illumination
- Specular highlights
- Environmental reflections
- Decals
- UI Depth adjustment added (cycle with ~ key)
- Hairworks MSAA one-eye transparency
- Hairworks glitch at specific depth from camera
- Hairworks shadows
- Light shafts
- Sun & moon depth
- Sun & moon reflection in water
- Approximate fix for water environment reflection probes (Note that an SLI bug
  affects these - see below)
- Approximate fix for direct reflections on water

Installation
------------
1. Extract the folder from the rar file into the game directory under "The
   Witcher 3 Wild Hunt\bin\x64". If done correctly, the d3d11.dll file should
   be in the same directory as witcher3.exe.

2. Launch the game. The first time you run it (and again after any driver
   update) you will get a UAC prompt for Rundll32 to install the driver
   profile - choose yes.

3. If 3D does not kick in change the video settings to exclusive full screen
   mode (This setting has a tendency to reset from time to time, so this step
   may need to be repeated on occasion).

Keys and Configuration
----------------------
~: Cycles UI depth between several presets. You may customise the default depth
   by editing the d3dx.ini and adjusting x under [Constants]. To customise the
   presets on the ~ key, find the [KeyHUD] section and adjust the list as
   desired.

F1: Toggle between two convergence presets.

F3: Disable false reflections caused by an SLI bug.

F11: Cycle SBS / TAB modes (see below to enable).

When raining, raindrops will land on the camera. If you find this distracting
you can disable it by editing the d3dx.ini file and setting x1 under
[Constants] to 1.

Side-by-Side / Top-and-Bottom Output Modes
------------------------------------------
This fix is bundled with the new SBS / TAB output mode support in 3DMigoto. To
enable it, edit the d3dx.ini, find the [Present] section and uncomment the
following line by removing the semicolon from the start:

    run = CustomShader3DVision2SBS

Then, in game press F11 to cycle output modes. If using 3D TV Play, set the
nvidia control panel to output checkerboard to remove the 720p limitation.

Known Issues
------------
- Sometimes after dying and loading a previous save the game may crash, or the
  3D may glitch out. If this happens, restart the game.

- On SLI systems, water in certain areas may display an incorrect reflection
  (e.g. reflecting clouds from the sky while underground). These false
  reflections can be disabled by pressing F3.

Download
--------
[The Witcher 3 v1.31 - 3DMigoto 1.2.49](https://s3.amazonaws.com/bo3b/3dsurroundgaming/Witcher_3_1.31.rar)

Outdated versions:  
[The Witcher 3 v1.22 - 3DMigoto 1.2.40](http://3dsurroundgaming.com/3DVision/Witcher_3_1.22_3DM_1.2.40.rar)  
[The Witcher 3 v1.21 - older 3DMigoto - Dynamic crosshair](http://3dsurroundgaming.com/3DVision/Witcher_3_1.21.rar)  
[The Witcher 3 v1.10](https://s3.amazonaws.com/DarkStarSword/3Dfix-Witcher3-1.10.zip)
