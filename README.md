![image](https://cloud.githubusercontent.com/assets/6544511/22624161/934dba64-eb27-11e6-8f78-46c902e96e1b.png)
========

#### Chiri's wrapper to enable fixing broken stereoscopic effects in DX11 games.
#### Now also a general purpose DX11 modding tool.

<br>

This includes the entire code base, and it will compile, link, and run in it's current state.

This is not the end-user version of the tool, this is for people developing the code by fixing
bugs, adding new features, or documenting how to use it.  You can find the latest releases https://github.com/bo3b/3Dmigoto/releases
<br>
<br>
The current project is updated to using Visual Studio 2026 Community, so anyone can do development for free.

To get started do:

1. Download VS2026 Community for Windows Desktop. https://visualstudio.microsoft.com/vs/community/
1. Install VS2026 and be sure to select:
   - "Programming Languages" -> "Visual C++"
   - "Windows 10 SDK (10.0.19041.0)"
   - "MSVC v143" (currently using C++17)
1. Run VS2026.
1. Git menu, Clone Repository.  Opens the page for cloning.
1. Use Clone menu, and enter the repository: 
https://github.com/bo3b/3Dmigoto.git
1. Change the source-code destination to where you prefer, and then click Clone.
1. Double click your new local repository to set it active (if you have others.)
1. At the home menu in Git Changes, double click StereovisionHacks.sln to open the solution.
1. Switch to Solution Explorer, and wait for it to parse all the files.
1. Hit Ctrl-Shift-B to build the full solution.
1. Output files are in .\builds\x64\Debug
   - d3d11.dll
   - nvapi64.dll
   - d3dx.ini
   - uninstall.bat
   - ShaderFixes folder

<br>

#### Versions

* 1.5.x

The latest version with a focus on code readability and cleanup. Unifying the code around the StyleGuide
improvements so that we have a consistent code style. Set up with .clang-format to auto-format on save,
so that it stays consistent. .clang-tidy recommendations will highlight preferred style.

This includes a lot of code improvements for best practices, like removing `using std namespace` and
updating to C++17. 

Code is not well tested, and can in principle have introduced bugs. Currently no obvious issues though.

Code has dramatic renaming and reformatting through the whole code base to unify around the common
style. This will make older diffs and blame harder to follow, although the git commit messages are accurate
and the AI will have no trouble following changes. To improve the diffs we add `.git-blame-ignore-revs`
for the changes so that regular history can be followed.

* 1.4.10

This version is the most clear descendant of the 1.3.16 version, with minimal code changes.
Platform toolset=v143 and SDK=10.0.19041.0 should work as far back as Win7.

Recent top of tree build with updates:
1. VS2022 as default editor
1. Updates to latest ThirdParty Libraries
1. Added D3D11On12CreateDevice required by Unity
1. Bug fixes for Assembler and Decompiler
1. Steam controller compatibility
1. Direct Github builds via Actions.

* 1.3.16

Most stable and tested version. 
Long term stability with primary release at tagged [1.3.16](https://github.com/bo3b/3Dmigoto/tree/1.3.16).
Platform toolset=v120 and Windows 8.0 Software Development Kit (SDK) worked as far back as Win7.
Hard to build now, but can be done if you use the older SDK and VS2017.


<br>

#### If you have any questions or problems don't hesitate to contact us or leave Issues.


Big, big, _impossibly_ big thanks to Chiri for open-sourcing 3Dmigoto.
