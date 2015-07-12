Far Cry 4
=========

A fix by DarkStarSword and mike_ar69

Special thanks to Bo3b for the decompiler fixes and DHR for discovering the Max
Payne 3 profile!

Fixed
-----
- Shadows
- Halos
- Water reflections
- Environment reflections
- Specular highlights
- Interior building masks
- Clipping on lights
- Low convergence preset activated when aiming via right mouse or left trigger
  (you can edit the d3dx.ini file to adjust the convergence amount or key
  binding under \[Key1\] and \[Key2\]).
- UI/HUD depth adjusted (depth may be customised via the X parameter in
  d3dx.ini under \[Constants\])
- Weapon sight depth adjusted (depth may be customised via the Y parameter in
  d3dx.ini under \[Constants\]).
- "Dirty lens" bloom depth adjusted
- nVidia god rays ("ENHANCED" option in settings)
- Regular god rays (while looking at sun through trees)
- Underwater green fog volume
- Underwater caustics
- Halos around lights
- Decals
- Sun/moon glow
- Fog
- Simulated fur

Installation
------------
1. Some newer drivers are causing issues in this game. Avoid versions 352.86
   (some instability) and 353.06 (3D does not work at all). 350.12 seems
   pretty stable and older drivers should also work.

2. Use nvidia inspector to assign the game to the Max Payne 3 profile. Refer to
   [this guide][1] for instructions on how to do this.

   [1]: http://helixmod.blogspot.com/2013/03/how-to-change-3d-vision-profile-and.html

3. Unpack zip to Far Cry 4\bin directory

4. If game switches to windowed mode after launch, press alt+enter to switch
   back to full screen

5. Set ambient occlusion to HBAO+ or SSAO. HBAO+ looks significantly better
   than SSAO, but has some minor artefacts on some surfaces. SSAO may not work
   on earlier versions of the game.

6. Set Anti-Aliasing to MSAA or TXAA if your machine can handle it. Off works,
   but shadows will have a 1 frame sync issue, and may appear to lift off the
   ground while walking. SMAA is not recommended.

7. Disable motion blur, as it causes rendering artefacts.

8. If you are using SLI, set terrain to medium or lower to get good
   performance.

Known Issues
------------
- Shangri-La missions are reportedly broken with SLI for some users. Currently
  the only known workarounds are to turn off either 3D or SLI for these
  missions.

- Repair tool flame is misaligned, and can't be fixed without breaking other
  flames.

- Windows & doors of buildings sometimes appear brighter than they should.

- Soft shadows create a box shaped artefact.

Notes
-----
If you find any rendering issues please post on [the forum thread][3], and be
sure to mention where the broken effect was in the game, and what settings you
are using - particularly anti-aliasing, shadows and vegetation.

[3]: https://forums.geforce.com/default/topic/789514/far-cry-4-3d-screenshots-
