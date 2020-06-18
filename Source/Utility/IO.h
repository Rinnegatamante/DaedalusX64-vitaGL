/*
Copyright (C) 2006 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef UTILITY_IO_H_
#define UTILITY_IO_H_

#include "Utility/DaedalusTypes.h"

#if defined( DAEDALUS_PSP )
#include <pspiofilemgr.h>
#elif defined( DAEDALUS_VITA )
#include <psp2/io/stat.h>
#define DAEDALUS_VITA_MAIN_PATH_GENERIC "/data/DaedalusX64/"
#define DAEDALUS_VITA_MAIN_PATH "ux0:/data/DaedalusX64/"
#define DAEDALUS_VITA_PATH(p) DAEDALUS_VITA_MAIN_PATH p
#define DAEDALUS_VITA_PATH_EXT(p1, p2) p1 DAEDALUS_VITA_MAIN_PATH_GENERIC p2
#endif

#include <string.h>

namespace IO
{
	namespace File
	{
		bool		Move( const char * p_existing, const char * p_new );
		bool		Delete( const char * p_file );
		bool		Exists( const char * p_path );
#if defined( DAEDALUS_PSP ) || defined( DAEDALUS_VITA )
		int			Stat( const char *p_file, SceIoStat *stat );
#endif

	}
	namespace Directory
	{
		bool		Create( const char * p_path );
		bool		EnsureExists( const char * p_path );
		bool		IsDirectory( const char * p_path );
	}

	namespace Path
	{
		const u32	kMaxPathLen = 260;

		inline void Assign( char * p_dest, const char * p_dir )
		{
			strncpy(p_dest, p_dir, kMaxPathLen);
			p_dest[kMaxPathLen-1] = '\0';
		}

		char *				Combine( char * p_dest, const char * p_dir, const char * p_file );
		bool				Append( char * p_path, const char * p_more );
		const char *		FindExtension( const char * p_path );
		const char *		FindFileName( const char * p_path );
		char *				RemoveBackslash( char * p_path );
		bool				RemoveFileSpec( char * p_path );
		void				RemoveExtension( char * p_path );
		void				AddExtension( char * p_path, const char * p_ext );
#if defined( DAEDALUS_PSP ) || defined( DAEDALUS_VITA )
		int					DeleteRecursive(const char* p_path, const char * p_extension);
#endif

		inline void SetExtension( char * p_path, const char * p_extension)
		{
			RemoveExtension(p_path);
			AddExtension(p_path, p_extension);
		}
	}

	typedef char Filename[IO::Path::kMaxPathLen+1];

	struct FindDataT
	{
		Filename	Name;
	};

#if defined( DAEDALUS_PSP ) || defined( DAEDALUS_VITA )
	typedef SceUID FindHandleT;
#elif defined( DAEDALUS_W32 )
	typedef intptr_t FindHandleT;
#elif defined( DAEDALUS_OSX ) || defined( DAEDALUS_LINUX )
	typedef void * FindHandleT;
#else
#error Need to define FindHandleT for this platform
#endif

	bool	FindFileOpen( const char * path, FindHandleT * handle, FindDataT & data );
	bool	FindFileNext( FindHandleT handle, FindDataT & data );
	bool	FindFileClose( FindHandleT handle );
}

#endif // UTILITY_IO_H_
