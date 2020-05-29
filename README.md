# DaedalusX64-vitaGL [![Build Status](https://dev.azure.com/rinnegatamante/Daedalus%20X64/_apis/build/status/Rinnegatamante.DaedalusX64-vitaGL?branchName=master)](https://dev.azure.com/rinnegatamante/Daedalus%20X64/_build/latest?definitionId=2&branchName=master)
 
Daedalus X64 is a Nintendo 64 emulator originally for Linux and PSP. This repository is the official one for the PSVITA/PSTV port using vitaGL as renderer.

## Build Instructions

In order to compile this, you'll first need to install [libmathneon](https://github.com/Rinnegatamante/math-neon), [vitaGL](https://github.com/Rinnegatamante/vitaGL) and [dear ImGui](https://github.com/Rinnegatamante/imgui-vita).

When installing vitaGL, be sure to use this command when compiling it: `make HAVE_SBRK=1 NO_DEBUG=1 install`.

Once you've satisfied those dependencies requirements, you can compile DaedalusX64-vitaGL with:
```
mkdir daedbuild
cd daedbuild
cmake -DVITA_RELEASE=1 ../Source -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make
```
 
## Compatibility List
 
A compatibility list can be found on [this GitHub repository](https://github.com/Rinnegatamante/DaedalusX64-vitaGL-Compatibility/issues). Contributions are very welcome. There's also an interactive website using this data available [on this link](https://daedalusx64.rinnegatamante.it/).

## Discord Server

You can head to Vita Nuova discord server to get help with DaedalusX64-vitaGL. We have a dedicated channel (#daedalus-x64) for discussing futur developments, suggesitons, help, etc.
 
Invite link: https://discord.gg/PyCaBx9
 
## Credits
 
- All the original Daedalus X64 developers
- xerpi for the original Vita port
- m4xw for the help sanitizing PIF code
- MasterFeizz for the ARM DynaRec
- TheFloW for his contributions to the DynaRec code
- Salvy & frangarcj for several improvements and bugfixes
- Inssame for some additions to the UI code
- That One Seong & TheIronUniverse for the LiveArea assets
- withLogic for the high-res preview assets
