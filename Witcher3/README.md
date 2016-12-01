The Witcher 3: Wild Hunt
========================
A fix by mike_ar69 & DarkStarSword
Assembler and alternate wrapper by Flugan
3DMigoto improvements by Bo3b and DarkStarSword
Profile and 1.10 update by Helifax
Special thanks to everyone on the forum who helped out with testing!

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
1. Use driver 358.87 (Newer drivers may work as well, but this the recommended
   driver for this game as it is known to work well for a lot of people,
   including Windows 10 users).

2. Install Helifax' custom profile for The Witcher 3. Note that you will have
   to repeat this any time you reinstall the nvidia drivers (instructions
   summarised from [here][1]):

[1]: https://forums.geforce.com/default/topic/841696/3d-vision/the-witcher-3-correct-3d-vision-nvidia-profile-cm-mode-included-/post/4562951/#4562951

    1. Download Geforce 3D Profile Manager.exe attached to [this page][2]

[2]: http://nvidia.custhelp.com/app/answers/detail/a_id/2625/kw/Profile

    2. In the "C:\ProgramData\NVIDIA Corporation\Drs" folder delete the
       nvdrssel.bin file (It will get automatically generated on profile
       import).

    3. In Geforce 3D Profile Manager, choose "Export SLI Profiles" and save the
       NVIDIA Profiles.txt somewhere.

    4. Open NVIDIA Profiles.txt in notepad (other editors may corrupt the
       file's non-standard encoding) and search for "Witcher 3". Remove the
       existing profile and replace it with this one:

            Profile "The Witcher 3"
                ShowOn GeForce
                ProfileType Application
                Executable "witcher3.exe"
                Executable "witcher3release.exe"
                Setting ID_0x00a06946 = 0x780020F5
                Setting ID_0x1033cec2 = 0x00000002
                Setting ID_0x1033dcd3 = 0x00000004
                Setting ID_0x70092d4a = 0xb19c3533 InternalSettingFlag=V0
                Setting ID_0x701eb457 = 0x2241ab21 InternalSettingFlag=V0
                Setting ID_0x702442fc = 0x1c22fe24 InternalSettingFlag=V0
                SettingString ID_0x7049c7ec = "웪ꑌ" InternalSettingFlag=V0
                SettingString ID_0x7051e5f5 = "籪鸙" InternalSettingFlag=V0
                Setting ID_0x708db8c5 = 0x5c3300b3 InternalSettingFlag=V0
                Setting ID_0x708db8c5 = 0x3FF13DD9 UserSpecified=true
                Setting ID_0x709a1ddf = 0x4b1cd968 InternalSettingFlag=V0
                SettingString ID_0x70b5603f = "榛鳈⏙ꢗ" InternalSettingFlag=V0
                Setting ID_0x70edb381 = 0x24208b6c InternalSettingFlag=V0
                Setting ID_0x70f8e408 = 0x80b671f3 InternalSettingFlag=V0
                Setting ID_0x709a1ddf = 0x4b1cd968 InternalSettingFlag=V0
                Setting ID_0x709adada = 0x37f58357 InternalSettingFlag=V0
            EndProfile

    5. In the "C:\ProgramData\NVIDIA Corporation\Drs" folder delete the
       nvdrssel.bin file. (if it got generated).

    6. In Geforce 3D Profile Manager, choose "Import SLI Profiles" and select
       the modified NVIDIA Profiles.txt

3. Extract [3Dfix-Witcher3-1.10.zip][3] into the game directory. If done
   correctly, the d3d11.dll file should be in the same directory as
   witcher3.exe.

[3]: https://s3.amazonaws.com/DarkStarSword/3Dfix-Witcher3-1.10.zip

4. Make sure that "Enable advanced in-game settings" is enabled in the NVIDIA
   control panel under Set Up Stereoscopic 3D -> Set keyboard shortcuts.

5. Launch the game. If 3D does not kick in change the video settings to
   exclusive full screen mode (This setting has a tendency to reset from time
   to time, so this step may need to be repeated on occasion).

6. Disable compatibility mode with Ctrl+Alt+F11 (check the green text to
   confirm that it is disabled - you should only need to do this once).

Keys and Configuration
----------------------
~: Cycles UI depth between several presets. You may customise the default depth
   by editing the d3dx.ini and adjusting x under [Constants]. Negative numbers
   go into the screen, positive numbers will pop out. To customise the presets
   on the ~ key, find the [KeyHUD] section and adjust the list as desired.

F1: Toggle an experimental auto HUD depth adjustment. This will make NPC names
    and some icons hover above the NPC, but will break other parts of the UI
    and in some cases (when Geralt is standing in the way) they may appear too
    close.

When raining, raindrops will land on the camera. If you find this distracting
you can disable it by editing the d3dx.ini file and setting x1 under
[Constants] to 1.

Known Issues
------------
- Sometimes after dying and loading a previous save the game may crash, or the
  3D may glitch out. If this happens, restart the game.

- On SLI systems, water in certain areas may display an incorrect reflection
  (e.g. reflecting clouds from the sky while underground). These false
  reflections can be disabled by pressing F3.
