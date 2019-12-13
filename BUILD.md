# Building liblsl

This file includes quick start recipes. To see general principles, look [here](https://github.com/labstreaminglayer/labstreaminglayer/blob/master/doc/BUILD.md).


## Windows - CMake - Visual Studio 2017

Starting with Visual Studio 2017, Microsoft began to support much closer integration with CMake. Generally, this uses the Visual Studio GUI as a wrapper around the CMake build files, so you should not expect to see most of the Visual Studio Project configuration options that you are familiar with, and CMake projects cannot be directly blended with non-CMake Visual Studio projects. There are also some weird gotchas, described below.

You will need to download and install:<BR/>
 * [The full LabStreamingLayer meta project](https://github.com/sccn/labstreaminglayer) -> Clone (include --recursive flag) or download  
 * [Visual Studio Community 2017](https://imagine.microsoft.com/en-us/Catalog/Product/530)
 * [CMake 3.12.1](https://cmake.org/files/v3.12/)


From Visual Studio:<BR/> 
 * File -> Open -> CMake -> labstreaminglayer/CMakeLists.txt
 * Wait while CMake configures automatically and until CMake menu turns black
 * Select x86-Release
 * CMake menu -> Change CMake Settings -> LabStreamingLayer

```
{
  "name": "x86-Release",
  "generator": "Ninja",
  "configurationType": "RelWithDebInfo",
  "inheritEnvironments": [ "msvc_x86" ],
  "buildRoot": "${env.USERPROFILE}\\CMakeBuilds\\${workspaceHash}\\build\\${name}",
  "installRoot": "${env.USERPROFILE}\\CMakeBuilds\\${workspaceHash}\\install\\${name}",
  "cmakeCommandArgs": "",
  "buildCommandArgs": "-v",
  "ctestCommandArgs": ""
}
```

 * Consider changing the build and install roots, as the default path is obscure. When you save this file, CMake should reconfigure, and show the output in the output window (including any CMake configuration errors).

 * Cmake -> Build All

The Library will be in buildRoot (defined in the configuration above) /LSL/liblsl

Generally you only need liblsl32.dll.

Note that if you make significant changes, it may be necessary to do a full rebuild before CMake correctly notices the change. CMake -> Clean All is not sufficient to force this.

Just deleting the build folder causes an unfortunate error (build folder not found) on rebuild. To do a full rebuild, it is necessary to change the build and install folder paths in CMake -> Change CMake Settings -> LabStreamingLayer, build, then delete the old folder, change the path back, and build again.


## Windows - CMake - Legacy

This procedure also works in Visual Studio 2017, if desired. Generally, I'd recommend sticking with the newer procedure and Visual Studio 2017.

This procedure generates a Visual Studio type project from the CMake files, which can then be opened in Visual Studio.

You will need to download and install:<BR/>
 * The full [LabStreamingLayer meta project](https://github.com/labstreaminglayer/labstreaminglayer) -> Clone (include --recursive flag) or download  
 * Desired Visual Studio Version (the example uses 2015).
 * [CMake 3.12.1](https://cmake.org/files/v3.12/)

From the command line, from the labstreaminglayer folder:<BR/>
 * labstreaminglayer\build>cmake .. -G "Visual Studio 14 2015"
 * labstreaminglayer\build>cmake --build . --config Release --target install

To see a list of possible generators, run the command with garbage in the -G option. 

The library is in labstreaminglayer\build\LSL\Release.

Generally you only need liblsl32.dll.

You can open the Visual Studio Project with labstreaminglayer\build\LabStreamingLayer.sln.

If any significant changes are made to the project (such as changing Visual Studio version) it is recommended that you delete or rename the build folder and start over. Various partial cleaning processes do not work well.


## Linux

To be written

## OS X

To be written