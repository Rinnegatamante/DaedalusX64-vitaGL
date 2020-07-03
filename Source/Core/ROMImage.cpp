/*
Copyright (C) 2007 StrmnNrmn

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

#include "stdafx.h"
#include "ROMImage.h"
#include "SysVita/UI/Menu.h"

// Find out the CIC type
ECicType ROM_GenerateCICType( const u8 * p_rom_base )
{
	u64	cic = 0;

	for(u32 i = 0x40; i < 0x1000; i+=4)
	{
		cic += *(u32*)(p_rom_base + i);
	}

	switch( cic )
	{
	case 0x000000D0027FDF31:	return CIC_6101;
	case 0x000000CFFB631223:	return CIC_6101;
	case 0x000000D057C85244:	return CIC_6102;
	case 0x000000D6497E414B:	return CIC_6103;
	case 0x0000011A49F60E96:	return CIC_6105;
	case 0x000000D6D5BE5580:	return CIC_6106;
	case 0x000001053BC19870:	return CIC_5167; //64DD CONVERSION CIC
	case 0x000000D2E53EF008:	return CIC_8303; //64DD IPL
	case 0x000000D2E53EF39F:	return CIC_DDTL; //64DD IPL TOOL
	case 0x000000D2E53E5DDA:	return CIC_DDUS; //64DD IPL US (different CIC)
	default:
//		DAEDALUS_ERROR("Unknown CIC Code");
		return CIC_UNKNOWN;
	}
}

const char * ROM_GetCicName( ECicType cic_type )
{
	switch(cic_type)
	{
	case CIC_6101:	return "CIC-6101";
	case CIC_6102:	return "CIC-6102";
	case CIC_6103:	return "CIC-6103";
	case CIC_6105:	return "CIC-6105";
	case CIC_6106:	return "CIC-6106";
	case CIC_5167:	return "CIC-5167";
	case CIC_8303:	return "CIC-8303";
	case CIC_DDUS:	return "CIC-DDUS";
	case CIC_DDTL:	return "CIC-DDTL";
	default:		return lang_strings[STR_UNKNOWN];
	}
}
