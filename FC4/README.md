Far Cry 4
=========

A fix by DarkStarSword and mike_ar69

Special thanks to Bo3b for the decompiler fixes and DHR for discovering the Max
Payne 3 profile!

Update 2016-03-09
-----------------
- Fixed the crosshair not working on the Steam version of the game.
- Fixed the simulated fur glitch in HLSL (Thanks to bo3b for the work on the decompiler)
- Fixed interior shadow mask clipping with MSAA enabled.
- Updated to 3DMigoto that supports SBS & TAB output modes. To enable, set the
  output mode to checkerboard in the nvidia control panel (don't worry -
  checkerboard won't be used), uncomment the 'run = CustomShader3DVision2SBS'
  line in the d3dx.ini, then press F9 in game to cycle modes.

Update 2016-02-23
-----------------
Removed the assembly shaders due to a falling out in the community. This means
the NVIDIA Hairworks simulated fur glitch at depth == 1.0 has returned, but
that is a very minor issue. Soft shadows are now fixed with 3DMigoto's
arbitrary resource copying support so they continue to work flawlessly. Be sure
to uninstall the existing fix with the provided uninstall.bat prior to updating
to this version.

Update 2015-12-21
-----------------
This is a major update to showcase new 3DMigoto features. For the original fix,
Bo3b and I had already spent a good deal of engineering effort adding a new
input infrastructure to 3DMigoto to add the aiming down sights convergence
preset that was necessary to make the game playable, but there were numerous
other wishlist items that were not possible to fix at the time, but not
important enough to delay releasing the fix.

I've since been working behind the scenes adding new features to 3DMigoto to
give our community the ability to fix many more advanced issues in DX11 games,
and since this is the main game I use to demo 3D Vision in public I decided
that it would be a good choice to showcase many of these new features.  
&nbsp;&nbsp;-DarkStarSword

- Automatically adjust the crosshair and weapon sight depth to rest on the
  target, and for the first time ever the depth buffer from the opposite eye is
  used to improve the accuracy of the auto crosshair (new features: texture &
  constant buffer copying, reverse stereo blit)

- Automatically adjust various floating icons in the HUD based on the depth
  buffer (new features: same as auto crosshair, access vertex buffers as
  structured buffers, screen size injection, texture filtering, texture
  injection)

- Automatically adjust the camera HUD depth, and leave vignette full screen
  (similar to above, but also used texture hash tracking and intra-frame
  texture detection)

- Disable aiming convergence preset while descending ropes (new features:
  conditional overrides, shader & texture detection)

- Fixed one frame sync issue on shadows when anti-aliasing was disabled or set
  to SMAA (This was a game bug and is present in 2D as well, but shows the
  flexibility of just what we can achieve using the arbitrary resource copying
  feature of 3DMigoto)

- Fixed artefacts on rocky surfaces when using NVIDIA HBAO+ (more info and
  screenshots [here][1]. New feature: constant buffer copying)

[1]: https://forums.geforce.com/default/topic/897529/3d-hbao-normal-map-artefact-fix

- Removed need for 2560x1440 users to edit the d3dx.ini (new feature: texture
  hashes independent of resolution)

- Aligned repair tool flame with repair tool, and aligned sparks with vehicle
  being repaired (new features: intra-frame texture detection, arbitrary
  resource copying)

- Fixed reflected glow of sun & moon (new feature: render target size
  filtering)

- Fixed NVIDIA Hairworks simulated fur glitch at depth == 1.0 (new features:
  ability to fix geometry shaders & use Flugan's assembler)

- Fixed box shaped artefact on NVIDIA soft shadows (new feature: <del>Flugan's
  assembler</del> arbitrary resource copying)

- Fixed interior shadow mask clipping on windows & doors (No new feature
  strictly required to fix it, but the new frame analysis features were crucial
  to identify the cause of this issue)

- Stretched vignette on world map to full 3D screen (new features: texture
  filtering & texture hash tracking. Note: on my screen I get a slight moire
  effect in the crosstalk that can make it look 2D on the white background
  beyond the edge of the map, but it looks fine on a non-white background)

- Fixed minor inverted emboss on patchy snow (Manually fixed. Bo3b has since
  improved 3DMigoto's handling of booleans in HLSL for these type of issues)

- Fixed environmental reflections when texture quality >= high

- Fixed reflections of several more objects

- Improved accuracy of specular highlight fix

- Re-enabled ripples and foam in river in prologue

- Fixed several shaders used in some community maps (just some of the more
  common ones I saw in a few maps - I'm not going to fix every map)

Fixed in original release
-------------------------
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
- UI/HUD depth adjusted to fixed depth.
- Weapon sight depth adjusted to fixed depth.
- "Dirty lens" bloom depth adjusted
- nVidia god rays ("ENHANCED" option in settings)
- Regular god rays (while looking at sun through trees)
- Underwater green fog volume
- Underwater caustics
- Halos around lights
- Decals
- Sun/moon glow
- Fog

Installation
------------
1. Use driver 358.87 (Newer drivers may work as well, but this the recommended
   driver for this game as it is known to work well for a lot of people,
   including Windows 10 users. It is no longer necessary to change the game
   profile with these newer drivers).

2. Unpack zip to Far Cry 4\bin directory

3. If game switches to windowed mode after launch, press alt+enter to switch
   back to full screen

4. Set ambient occlusion to HBAO+ or SSAO. HBAO+ looks significantly better
   than SSAO and is highly recommended, especially now I've fixed the artefacts
   it had on rocky surfaces. SSAO may not work on earlier versions of the game.

5. Disable motion blur, as it causes rendering artefacts.

6. If you are using SLI, set terrain to medium or lower to get good
   performance.

HUD Depth
---------
Most of the HUD is now automatically adjusted, but there are a few tweaks
available in the d3dx.ini in the [Constants] section:

- x sets the depth of any HUD element that is not automatically adjusted

- y sets a minimum depth for any automatically adjusted HUD element

- z sets the depth of the minimap. Use -1 to automatically adjust it, but I
  found that could be distracting given the gun can wave around in front of it
  while running.

Known Issues
------------
- Shangri-La missions are reportedly broken with SLI for some users (need to
  confirm, but it seems these may now be working with the latest drivers).
  The only known workarounds are to turn off either 3D or SLI for these
  missions.

- An outline of the current weapon may show offset to the right in certain
  rivers (most notably in the prologue) while using MSAA or TXAA. This is a
  game bug that occurs even in 2D without the fix installed. Most water does
  not suffer from this problem.

- The text showing the distance to a waypoint is not lined up with the floating
  waypoint icon. This would be tricky to fix as the text is drawn earlier in
  the frame than the icon and there's no easy way to distinguish it from any
  other text that we don't want to adjust.

Notes
-----
If you find any rendering issues be sure to mention where the broken effect was
in the game, and what settings you are using - particularly anti-aliasing,
shadows, water and vegetation.
