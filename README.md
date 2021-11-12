# DaedalusX64-vitaGL [![Build Status](https://dev.azure.com/rinnegatamante/Daedalus%20X64/_apis/build/status/Rinnegatamante.DaedalusX64-vitaGL?branchName=master)](https://dev.azure.com/rinnegatamante/Daedalus%20X64/_build/latest?definitionId=2&branchName=master)
 
DaedalusX64 is a Nintendo 64 emulator originally for Linux and PSP. This repository is the official one for the PSVITA/PSTV port using vitaGL as renderer backend.

## Build Instructions

You need to have vitaGL compiled with `NO_DEBUG=1 HAVE_UNFLIPPED_FBOS=1 NO_TEX_COMBINER=1 SHADER_COMPILER_SPEEDHACK=1 HAVE_CUSTOM_HEAP=1` in order to properly compile DaedalusX64.<br>
Also, before compiling it, run:
```
sed -i "s/float fog_dist = coords.z \/ coords.w;/float fog_dist = coords.z;/" source/shaders/ffp_f.h
sed -i "s/#define SHADER_CACHE_MAGIC /#define SHADER_CACHE_MAGIC 99/" source/ffp.c
```
This will apply an hack to the lib to make it handle better N64 fogging.<br><br>

You can compile DaedalusX64-vitaGL with:
```
mkdir daedbuild
cd daedbuild
cmake ../Source
make
```
 
## Compatibility List
 
A compatibility list can be found on [this GitHub repository](https://github.com/Rinnegatamante/DaedalusX64-vitaGL-Compatibility/issues). Contributions are very welcome. There's also an interactive website using this data available [on this link](https://daedalusx64.rinnegatamante.it/).

## Discord Server

You can head to Vita Nuova discord server to get help with DaedalusX64-vitaGL. We have a dedicated channel (#daedalus-x64) for discussing futur developments, suggesitons, help, etc.
 
Invite link: https://discord.gg/PyCaBx9

## HD Textures Pack Tutorial
In order to create an HD texture pack (or adapt an existing one to DaedalusX64). You'll need to follow these steps:
* Launch DaedalusX64 and enable **Textures Dumper** option under **Extra** menu.
* Launch the game you want to create an HD texture pack for.
* Play a bit the game in order to let Daedalus X64 dump all the textures the game loads in the meantime.
* Close DaedalusX64, if you'll now navigate in **ux0:/data/DaedalusX64/Textures** you'll find a new folder named as the Cartridge ID of the game you've run containing a lot of images.
* Create HD replacements for those images (or use existing ones from other textures pack) without changing filenames.

## Network Roms Tutorial
In order to play roms from your PC or from an online webserver, you can follow this guide made by Samilop Cimmerian iter:
https://samilops2.gitbook.io/vita-troubleshooting-guide/daedalus-x64/load-rom-through-a-web-server

## Custom Bubbles Tutorial
In order to create custom bubbles for the Livearea to launch a game directly with Daedalus X64, you can follow this guide made by Samilop Cimmerian iter:
https://samilops2.gitbook.io/vita-troubleshooting-guide/daedalus-x64/making-custom-bubbles

## Credits
 
- All the original DaedalusX64 developers
- xerpi for the original Vita port
- m4xw for the help sanitizing PIF code
- MasterFeizz for the ARM DynaRec
- TheFloW & cmf028 for their contributions to the DynaRec code
- Salvy & frangarcj for several improvements and bugfixes
- Inssame for some additions to the UI code
- That One Seong & TheIronUniverse for the LiveArea assets
- withLogic for the high-res preview assets
- Rob Scotcher for the DaedalusX64 logo image
