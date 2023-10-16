 #CMake File for DaedalusX64

#Build instructions
# Create a directory in the DaedalusX64 root directory and cd into it 'eg: mkdir tmp ~/DaedalusX64'
# ensure you have cmake installed
# For PSP:
# cmake -DCMAKE_TOOLCHAIN_FILE=../Tools/psptoolchain.cmake ../Source/ -DPSP_OPTION

#Options
# PSP_RELEASE - Builds PSP Release

cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 14)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project (DaedalusX64)

#Default Files for build
set (CONFIG_FILES Config/ConfigOptions.cpp)
set (CORE_FILES Core/HvqmTask.cpp Core/RE2Task.cpp Core/RDRam.cpp Core/Cheats.cpp Core/CPU.cpp Core/DMA.cpp Core/Dynamo.cpp Core/FlashMem.cpp Core/Interpret.cpp Core/Interrupts.cpp Core/JpegTask.cpp Core/Memory.cpp Core/PIF.cpp Core/R4300.cpp Core/ROM.cpp Core/ROMBuffer.cpp Core/ROMImage.cpp Core/RomSettings.cpp Core/RSP_HLE.cpp Core/Save.cpp Core/SaveState.cpp Core/TLB.cpp)
set (DEBUG_FILES Debug/DebugConsoleImpl.cpp Debug/DebugLog.cpp Debug/Dump.cpp)
set (DYNAREC_FILES DynaRec/BranchType.cpp DynaRec/DynaRecProfile.cpp DynaRec/Fragment.cpp DynaRec/FragmentCache.cpp DynaRec/IndirectExitMap.cpp DynaRec/StaticAnalysis.cpp DynaRec/TraceRecorder.cpp)
set (GRAPHICS_FILES Graphics/ColourValue.cpp Graphics/PngUtil.cpp Graphics/TextureTransform.cpp)
set (HLEAUDIO_FILES HLEAudio/Musyx.cpp HLEAudio/AudioHLEProcessor.cpp HLEAudio/ABI.cpp HLEAudio/NEAD.cpp HLEAudio/NAudio.cpp HLEAudio/ABI3mp3.cpp HLEAudio/AudioBuffer.cpp HLEAudio/HLEMain.cpp HLEAudio/ABI_ADPCM.cpp HLEAudio/ABI_Buffers.cpp HLEAudio/ABI_Filters.cpp HLEAudio/ABI_MixerInterleave.cpp HLEAudio/ENV_Mixer.cpp HLEAudio/ABI_Resample.cpp)
set (HLEGRAPHICS_FILES HLEGraphics/BaseRenderer.cpp HLEGraphics/BaseRenderer.h HLEGraphics/CachedTexture.cpp HLEGraphics/ConvertImage.cpp HLEGraphics/ConvertTile.cpp HLEGraphics/DLDebug.cpp HLEGraphics/DLParser.cpp HLEGraphics/Microcode.cpp HLEGraphics/RDPStateManager.cpp HLEGraphics/TextureCache.cpp HLEGraphics/TextureInfo.cpp HLEGraphics/uCodes/Ucode.cpp)
set (INTERFACE_FILES Interface/RomDB.cpp)
set (MATH_FILES Math/Matrix4x4.cpp)
set (OSHLE_FILES OSHLE/OS.cpp OSHLE/patch.cpp)
set (PLUGIN_FILES Plugins/GraphicsPlugin.cpp)
set (SYSTEM_FILES System/Paths.cpp System/System.cpp)
set (TEST_FILES Test/BatchTest.cpp)
set (UTILITY_FILES Utility/CRC.cpp Utility/DataSink.cpp Utility/FastMemcpy.cpp  Utility/FramerateLimiter.cpp Utility/Hash.cpp Utility/IniFile.cpp Utility/MemoryHeap.cpp Utility/Preferences.cpp Utility/PrintOpCode.cpp Utility/Profiler.cpp Utility/ROMFile.cpp Utility/ROMFileCache.cpp Utility/ROMFileCompressed.cpp Utility/ROMFileMemory.cpp Utility/ROMFileUncompressed.cpp Utility/ROMFileNetwork.cpp Utility/Stream.cpp Utility/StringUtil.cpp Utility/Synchroniser.cpp Utility/Timer.cpp Utility/ZLibWrapper.cpp)
set (UNKNOWN_FILES Utility/FastMemcpy_test.cpp)
set (DEBUG_ONLY Core/Registers.cpp)
set (BUILD ${CONFIG_FILES} ${CORE_FILES} ${DEBUG_FILES} ${DYNAREC_FILES} ${GRAPHICS_FILES} ${HLEAUDIO_FILES} ${HLEGRAPHICS_FILES} ${INTERFACE_FILES} ${MATH_FILES} ${OSHLE_FILES} ${PLUGIN_FILES} ${SYSTEM_FILES} ${TEST_FILES} ${UTILITY_FILES})

# Vita
set (VITA_MAIN_FILES SysVita/main.cpp)
set (VITA_DEBUG_FILES SysVita/Debug/DaedalusAssertVita.cpp SysVita/Debug/DBGConsoleVita.cpp )
set (VITA_DYNAREC_FILES SysVita/DynaRec/DynarecStubARM.S SysVita/DynaRec/CodeGeneratorARM.cpp SysVita/DynaRec/CodeBufferManagerARM.cpp SysVita/DynaRec/AssemblyWriterARM.cpp SysVita/DynaRec/AssemblyUtilsARM.cpp SysVita/DynaRec/N64RegisterCacheARM.cpp)
set (VITA_GRAPHICS_FILES SysVita/Graphics/GraphicsContextVita.cpp SysVita/Graphics/NativeTextureVita.cpp)
set (VITA_HLEGRAPHICS_FILES SysVita/HLEGraphics/GraphicsPluginVita.cpp SysVita/HLEGraphics/RendererLegacy.cpp SysVita/HLEGraphics/RendererModern.cpp SysVita/HLEGraphics/ColourAdjuster.cpp SysVita/HLEGraphics/BlendModes.cpp SysVita/HLEGraphics/Combiner/BlendConstant.cpp SysVita/HLEGraphics/Combiner/CombinerExpression.cpp SysVita/HLEGraphics/Combiner/CombinerTree.cpp SysVita/HLEGraphics/Combiner/RenderSettings.cpp)
set (VITA_HLEAUDIO_FILES SysVita/HLEAudio/AudioPluginVita.cpp SysVita/HLEAudio/AudioOutput.cpp)
set (VITA_INPUT_FILES SysVita/Input/InputManagerVita.cpp)
set (VITA_UI_FILES SysVita/UI/TranslationStrings.cpp SysVita/UI/MainMenuScreen.cpp SysVita/UI/InGameMenuScreen.cpp SysVita/UI/MenuBarScreen.cpp SysVita/UI/DownloaderScreen.cpp)
set (VITA_UTILITY_FILES SysVita/Utility/ThreadVita.cpp SysVita/Utility/IOVita.cpp SysVita/Utility/TimingVita.cpp minizip/ioapi.c minizip/zip.c minizip/unzip.c)
set (VITA_BUILD ${VITA_DEBUG_FILES} ${VITA_DYNAREC_FILES} ${VITA_GRAPHICS_FILES} ${VITA_HLEAUDIO_FILES} ${VITA_HLEGRAPHICS_FILES} ${VITA_INPUT_FILES} ${VITA_UI_FILES} ${VITA_UTILITY_FILES})

#Handle .S files - Probably a better way to do this
set_property(SOURCE SysVita/DynaRec/DynarecStubARM.S PROPERTY LANGUAGE C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

message("PSVita Release Build..")
	
execute_process(COMMAND git describe --abbrev=6 --dirty --always
	OUTPUT_VARIABLE GIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
	
set(VITA_APP_NAME "${PROJECT_NAME}")
set(VITA_TITLEID  "DEDALOX64")
set(VITA_VERSION  "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGIT_VERSION=\"${GIT_HASH}\" -g -O3 -mfpu=neon -fno-optimize-sibling-calls -fsingle-precision-constant -march=armv7-a -mtune=cortex-a9 -mfloat-abi=hard -ffast-math -Wno-write-strings")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGIT_VERSION=\"${GIT_HASH}\" -g -O3 -mfpu=neon -fno-optimize-sibling-calls -fsingle-precision-constant -march=armv7-a -mtune=cortex-a9 -mfloat-abi=hard -ffast-math -fpermissive -Wno-write-strings")

add_definitions("-DDAEDALUS_VITA")

# Build Daedalus Lib
add_library(daedalus STATIC
	${BUILD}
	${DEBUG_ONLY}
	${VITA_BUILD}
)

target_include_directories(daedalus PRIVATE
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/SysVita/Include
	${PROJECT_SOURCE_DIR}/Config/Dev
)

target_link_libraries(daedalus
	z
)

# Build and Link Executable
add_executable(${PROJECT_NAME}.elf
	${VITA_MAIN_FILES}
	${VITA_INPUTMANAGER_FILES}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/SysVita/Include
	${PROJECT_SOURCE_DIR}/Config/Dev
)

target_link_libraries(${PROJECT_NAME}.elf
	-Wl,--whole-archive daedalus -Wl,--no-whole-archive
	imgui
	vitaGL
	vitashark
	SceShaccCgExt
	mathneon
	curl
	ssl
	crypto
	speexdsp
	soloud
	pthread
	taihen_stub
	SceDisplay_stub
	SceGxm_stub
	SceCommonDialog_stub
	SceKernelThreadMgr_stub
	SceCtrl_stub
	SceKernelDmacMgr_stub
	-Wl,--whole-archive SceNetCtl_stub -Wl,--no-whole-archive
	-Wl,--whole-archive SceSysmodule_stub -Wl,--no-whole-archive
	SceTouch_stub
	SceAudio_stub
	ScePower_stub
	SceAppMgr_stub
	SceAppUtil_stub
	SceShaccCg_stub
	SceVshBridge_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME}.elf UNSAFE)
	
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
	VERSION ${VITA_VERSION}
	NAME ${VITA_APP_NAME}
	FILE ${PROJECT_SOURCE_DIR}/Roboto_compact.ttf Roboto_compact.ttf
	FILE ${PROJECT_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
	FILE ${PROJECT_SOURCE_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
	FILE ${PROJECT_SOURCE_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
	FILE ${PROJECT_SOURCE_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)

add_custom_target(launch
	COMMAND curl -T ${PROJECT_NAME}.self ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/eboot.bin
	COMMAND echo "launch ${VITA_TITLEID}" | nc $ENV{PSVITAIP} 1338
	DEPENDS ${PROJECT_NAME}.self
)

add_custom_target(send
	COMMAND curl -T ${PROJECT_NAME}.self ftp://$(PSVITAIP):1337/ux0:/app/${VITA_TITLEID}/eboot.bin
	DEPENDS ${PROJECT_NAME}.self
)

add_custom_target(vpksend
	COMMAND curl -T ${PROJECT_NAME}.vpk ftp://$(PSVITAIP):1337/ux0:/
	DEPENDS ${PROJECT_NAME}.vpk
)
