![image](https://cloud.githubusercontent.com/assets/6544511/22624161/934dba64-eb27-11e6-8f78-46c902e96e1b.png)
========

####Chiri's wrapper to enable fixing broken stereoscopic effects in DX11 games.

This includes the entire code base, and it will compile, link, and run in it's current state.

This is not the end-user version of the tool, this is for people developing the code by fixing
bugs, adding new features, or documenting how to use it.
<br>
<br>
The current project is set up using Visual Studio 2013 Express, so anyone can do development for free.

To get started do:

1. Install IE 10 or 11.  VS2013 apparently requires this, but might have been fixed recently.
1. Download the Windows 8 SDK. VS2013 only includes the 8.1 SDK and we need 8.0.  These are only
header files and libraries, no toolsets are included.*
http://msdn.microsoft.com/en-us/windows/desktop/hh852363.aspx
1. Download VS2013 Express for Windows Desktop.
http://www.visualstudio.com/en-us/downloads#d-express-windows-desktop
1. Run VS2013 and it should automatically find Update 1 to install.
1. TEAM menu, Connect.  Opens the Connect page for cloning.
1. Use Clone menu, and enter the repository: 
https://github.com/bo3b/3Dmigoto.git
1. Change the source-code destination to where you prefer, and then click Clone.
1. Double click your new local repository to set it active (if you have others.)
1. At the home menu in Team Explorer, double click StereovisionHacks.sln to open the solution.
1. Switch to Solution Explorer, and wait for it to parse all the files.
1. Hit F7 to build the full solution.
1. Output files are in .\x64\Debug (3 dll and 1 .ini)
<br>
<br>
*After some research, it's clear that using the 8.1 SDK (or greater) will not work, because the Decompiler is heavily tied to the format of the assembler output from the 8.0 SDK.  
VS2015 has been tested and works, but still requires 8.0 SDK.
<br>
<br>

#####If you have any questions or problems don't hesitate to contact me.


Big, big, _impossibly_ big thanks to Chiri for open-sourcing 3Dmigoto.
