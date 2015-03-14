Far Cry 4
=========

A fix by DarkStarSword and mike_ar69

**NOTE: This is an early alpha release for wider testing. If you find any
rendering issues please report back:**

- What is broken (a 3D screenshot showing the broken effect is fine)
- Where in the game (somewhere in the open world, or a particular mission)
- What anti-aliasing setting you are using **(VERY IMPORTANT!!!)**
- What other settings you are using
- Which driver profile you are using
- If you using SLI or a single GPU

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
- UI/HUD depth adjusted (depth may be customised via the X parameter in
  d3dx.ini under \[Constants\])
- Weapon sight depth adjusted (press tilde to toggle between lining up with the
  UI depth, or the weapon depth).
- "Dirty lens" bloom depth adjusted
- nVidia god rays ("ENHANCED" option in settings)
- Regular god rays (while looking at sun through trees)
- Underwater green fog volume
- Underwater caustics
- Halos around lights
- Decals
- Moon glow
- Fog

Installation
------------
1. Use nvidia inspector to assign the game to the Max Payne 3 profile(1)(2)

2. Unpack zip to Far Cry 4\bin directory

3. Enable advanced keys in nVidia control panel

4. If game switches to windowed mode after launch, press alt+enter to switch
   back to full screen

5. If using a recent driver version >= 347.09, press ctrl+alt+F11 in game to
   disable compatibility mode

7. Set ambient occlusion to HBAO+ or SSAO. HBAO+ looks significantly better
   than SSAO, but has some minor artefacts on some surfaces. SSAO may not work
   on earlier versions of the game.

8. Set Anti-Aliasing to MSAA or TXAA if your machine can handle it. Off works,
   but shadows will have a 1 frame sync issue, and may appear to lift off the
   ground while walking. SMAA is not recommended. (3)

9. Disable motion blur, as it causes rendering artefacts.


Known Issues
------------
- Repair tool flame is misaligned, and can't be fixed without breaking other
  flames.

- Windows & doors of buildings sometimes appear brighter than they should.

- Soft shadows have some artefacts.

Notes
-----
(1) I've actually been using the Far Cry 4 profile with StereoFlagsDX10 set to
    0x00004000 instead of the Max Payne 3 profile, but they should be roughly
    equivelent.

(2) SLI users can alternatively use a null profile with the SLI compatibility
    bits copied from the Far Cry 3 profile. Plese report back with what works
    best for performance and without graphics glitches.

(3) Many effects use different shaders depending on the AA settings (off/SMAA,
    MSAA2/TXAA2, MSAA4/TXAA4, MSAA8). I've tried to fix all variants, but I may
    have missed some - please let me know if you find something broken and
    which AA setting you were using.
