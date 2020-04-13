# Copyright (c) 2016, Yifan Lu
# Based off of Android toolchain file
# Copyright (c) 2010-2011, Ethan Rublee
# Copyright (c) 2011-2014, Andrey Kamaev
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1.  Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
# 2.  Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
# 3.  Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from this
#     software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required( VERSION 2.6.3 )

if( DEFINED CMAKE_CROSSCOMPILING )
  # subsequent toolchain loading is not really needed
  return()
endif()

if( CMAKE_TOOLCHAIN_FILE )
  # touch toolchain variable to suppress "unused variable" warning
endif()

set( CMAKE_SYSTEM_NAME Generic )
set( CMAKE_SYSTEM_VERSION 1 )

# search for Vita SDK path 1) where this toolchain file is, 2) in environment var, 3) manually defined
if( NOT DEFINED ENV{VITASDK} )
  get_filename_component(__vitasdk_path ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
  if( EXISTS "${__vitasdk_path}" )
    set( VITASDK "${__vitasdk_path}" )
    set( ENV{VITASDK} "${__vitasdk_path}" )
  endif()
else()
  set( VITASDK "$ENV{VITASDK}" )
endif()
set( VITASDK "${VITASDK}" CACHE PATH "Path to Vita SDK root" )

if( NOT EXISTS "${VITASDK}" )
  message( FATAL_ERROR "Cannot find Vita SDK at ${VITASDK}" )
endif()

set( TOOL_OS_SUFFIX "" )
if( CMAKE_HOST_WIN32 )
 set( TOOL_OS_SUFFIX ".exe" )
endif()

set( CMAKE_SYSTEM_PROCESSOR "armv7-a" )
set( CMAKE_C_COMPILER   "${VITASDK}/bin/arm-vita-eabi-gcc${TOOL_OS_SUFFIX}"     CACHE PATH "C compiler" )
set( CMAKE_CXX_COMPILER "${VITASDK}/bin/arm-vita-eabi-g++${TOOL_OS_SUFFIX}"     CACHE PATH "C++ compiler" )
set( CMAKE_ASM_COMPILER "${VITASDK}/bin/arm-vita-eabi-gcc${TOOL_OS_SUFFIX}"     CACHE PATH "assembler" )
set( CMAKE_STRIP        "${VITASDK}/bin/arm-vita-eabi-strip${TOOL_OS_SUFFIX}"   CACHE PATH "strip" )
set( CMAKE_AR           "${VITASDK}/bin/arm-vita-eabi-gcc-ar${TOOL_OS_SUFFIX}"  CACHE PATH "archive" )
set( CMAKE_LINKER       "${VITASDK}/bin/arm-vita-eabi-ld${TOOL_OS_SUFFIX}"      CACHE PATH "linker" )
set( CMAKE_NM           "${VITASDK}/bin/arm-vita-eabi-nm${TOOL_OS_SUFFIX}"      CACHE PATH "nm" )
set( CMAKE_OBJCOPY      "${VITASDK}/bin/arm-vita-eabi-objcopy${TOOL_OS_SUFFIX}" CACHE PATH "objcopy" )
set( CMAKE_OBJDUMP      "${VITASDK}/bin/arm-vita-eabi-objdump${TOOL_OS_SUFFIX}" CACHE PATH "objdump" )
set( CMAKE_RANLIB       "${VITASDK}/bin/arm-vita-eabi-gcc-ranlib${TOOL_OS_SUFFIX}"  CACHE PATH "ranlib" )

# Vita specific tools
set( VITA_ELF_CREATE    "${VITASDK}/bin/vita-elf-create${TOOL_OS_SUFFIX}"       CACHE PATH "vita-elf-create" )
set( VITA_ELF_EXPORT    "${VITASDK}/bin/vita-elf-export${TOOL_OS_SUFFIX}"       CACHE PATH "vita-elf-export" )
set( VITA_LIBS_GEN      "${VITASDK}/bin/vita-libs-gen${TOOL_OS_SUFFIX}"         CACHE PATH "vita-libs-gen" )
set( VITA_MAKE_FSELF    "${VITASDK}/bin/vita-make-fself${TOOL_OS_SUFFIX}"       CACHE PATH "vita-make-fself" )
set( VITA_MKSFOEX       "${VITASDK}/bin/vita-mksfoex${TOOL_OS_SUFFIX}"          CACHE PATH "vita-mksfoex" )
set( VITA_PACK_VPK      "${VITASDK}/bin/vita-pack-vpk${TOOL_OS_SUFFIX}"         CACHE PATH "vita-pack-vpk" )

# cache flags
set( CMAKE_CXX_FLAGS           ""                        CACHE STRING "c++ flags" )
set( CMAKE_C_FLAGS             ""                        CACHE STRING "c flags" )
set( CMAKE_CXX_FLAGS_RELEASE   "-O3 -DNDEBUG"            CACHE STRING "c++ Release flags" )
set( CMAKE_C_FLAGS_RELEASE     "-O3 -DNDEBUG"            CACHE STRING "c Release flags" )
set( CMAKE_CXX_FLAGS_DEBUG     "-O0 -g -DDEBUG -D_DEBUG" CACHE STRING "c++ Debug flags" )
set( CMAKE_C_FLAGS_DEBUG       "-O0 -g -DDEBUG -D_DEBUG" CACHE STRING "c Debug flags" )
set( CMAKE_SHARED_LINKER_FLAGS ""                        CACHE STRING "shared linker flags" )
set( CMAKE_MODULE_LINKER_FLAGS ""                        CACHE STRING "module linker flags" )
set( CMAKE_EXE_LINKER_FLAGS    "-Wl,-z,nocopyreloc"      CACHE STRING "executable linker flags" )

# we require the relocation table
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q" )

# set these global flags for cmake client scripts to change behavior
set( VITA True )
set( BUILD_VITA True )

# where is the target environment
set( CMAKE_FIND_ROOT_PATH "${VITASDK}/bin" "${VITASDK}/arm-vita-eabi" "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_PREFIX}/share" )
set( CMAKE_INSTALL_PREFIX "${VITASDK}/arm-vita-eabi" CACHE PATH "default install path" )

# only search for libraries and includes in vita toolchain
if( NOT CMAKE_FIND_ROOT_PATH_MODE_LIBRARY )
  set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
endif()
if( NOT CMAKE_FIND_ROOT_PATH_MODE_INCLUDE )
  set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
endif()
