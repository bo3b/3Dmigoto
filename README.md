![image](https://cloud.githubusercontent.com/assets/6544511/22624161/934dba64-eb27-11e6-8f78-46c902e96e1b.png)
========

### Chiri's wrapper to enable fixing broken stereoscopic effects in DX11 games.
#### Now also a general purpose DX11 modding tool.

<br>

This includes the entire code base, and it will compile, link, and run in it's current state.

This is not the end-user version of the tool, this is for people developing the code by fixing
bugs, adding new features, or documenting how to use it.  You can find the latest releases https://github.com/bo3b/3Dmigoto/releases
<br>
<br>
The current project is updated to using Visual Studio 2022 Community, so anyone can do development for free.

To get started do:

1. Download VS2022 Community for Windows Desktop. https://visualstudio.microsoft.com/vs/community/
1. Install VS2022 and be sure to select:
   - "Programming Languages" -> "Visual C++"
   - "Windows 10 SDK (10.0.19041.0)"
   - "MSVC v143" (currently using C++14)
1. Run VS2022.
1. Git menu, Clone Repository.  Opens the page for cloning.
1. Use Clone menu, and enter the repository: 
https://github.com/bo3b/3Dmigoto.git
1. Change the source-code destination to where you prefer, and then click Clone.
1. Double click your new local repository to set it active (if you have others.)
1. At the home menu in Git Changes, double click StereovisionHacks.sln to open the solution.
1. Switch to Solution Explorer, and wait for it to parse all the files.
1. Hit F7 to build the full solution.
1. Output files are in .\builds\x64\Debug
   - d3d11.dll
   - nvapi64.dll
   - d3dx.ini
   - uninstall.bat
   - ShaderFixes folder
<br>

#### If you have any questions or problems don't hesitate to contact us or leave Issues.


Big, big, _impossibly_ big thanks to Chiri for open-sourcing 3Dmigoto.
