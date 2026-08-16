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

#include "stdafx.h"
#include "ColourValue.h"

#include "Math/MathUtil.h"
#include "Math/Vector4.h"

// Around 463,000 ticks/million
inline u32 Vector2ColourClampedCPU( const v4 * col_in )
{
	u8 r = u8( Clamp<s32>( s32(col_in->x * 255.0f), 0, 255 ) );
	u8 g = u8( Clamp<s32>( s32(col_in->y * 255.0f), 0, 255 ) );
	u8 b = u8( Clamp<s32>( s32(col_in->z * 255.0f), 0, 255 ) );
	u8 a = u8( Clamp<s32>( s32(col_in->w * 255.0f), 0, 255 ) );

	return c32::Make( r, g, b, a );
}

inline u32 Vector2ColourClamped( const v4 & colour )
{
	return Vector2ColourClampedCPU( &colour );
}

inline u8 AddComponent( u8 a, u8 b )
{
	return u8( Clamp< s32 >( s32( a ) + s32( b ), 0, 255 ) );
}

inline u8 SubComponent( u8 a, u8 b )
{
	return u8( Clamp< s32 >( s32( a ) - s32( b ), 0, 255 ) );
}

inline u8 ModulateComponent( u8 a, u8 b )
{
	return u8( ( u32( a ) * u32( b ) ) >> 8 );		// >> 8 to return to 0..255
}

inline u8 InterpolateComponent( u8 a, u8 b, float factor )
{
	return u8(float(a) + (float(b) - float(a)) * factor);
}


const c32 c32::White( 255,255,255, 255 );
const c32 c32::Black( 0,0,0, 255 );
const c32 c32::Red( 255,0,0, 255 );
const c32 c32::Green( 0,255,0, 255 );
const c32 c32::Blue( 0,0,255, 255 );
const c32 c32::Magenta( 255,0,255, 255 );
const c32 c32::Gold( 255, 255, 24, 255 );
const c32 c32::Turquoise( 64,224,208,255 );
const c32 c32::Orange( 255,165,0,255 );
const c32 c32::Purple( 160,32,240,255 );
const c32 c32::Grey( 190,190,190, 255 );

c32::c32( const v4 & colour )
:	mColour( Vector2ColourClamped( colour ) )
{
}

v4	c32::GetColourV4() const
{
	return v4( GetR() / 255.0f, GetG() / 255.0f, GetB() / 255.0f, GetA() / 255.0f );
}

c32	c32::Add( c32 colour ) const
{
	u8 r = AddComponent( GetR(), colour.GetR() );
	u8 g = AddComponent( GetG(), colour.GetG() );
	u8 b = AddComponent( GetB(), colour.GetB() );
	u8 a = AddComponent( GetA(), colour.GetA() );

	return c32( r, g, b, a );
}

c32	c32::AddRGB( c32 colour ) const
{
	u8 r = AddComponent( GetR(), colour.GetR() );
	u8 g = AddComponent( GetG(), colour.GetG() );
	u8 b = AddComponent( GetB(), colour.GetB() );
	u8 a = GetA();

	return c32( r, g, b, a );
}

c32	c32::AddA( c32 colour ) const
{
	u8 r = GetR();
	u8 g = GetG();
	u8 b = GetB();
	u8 a = AddComponent( GetA(), colour.GetA() );

	return c32( r, g, b, a );
}

c32	c32::Sub( c32 colour ) const
{
	u8 r = SubComponent( GetR(), colour.GetR() );
	u8 g = SubComponent( GetG(), colour.GetG() );
	u8 b = SubComponent( GetB(), colour.GetB() );
	u8 a = SubComponent( GetA(), colour.GetA() );

	return c32( r, g, b, a );
}

c32	c32::SubRGB( c32 colour ) const
{
	u8 r = SubComponent( GetR(), colour.GetR() );
	u8 g = SubComponent( GetG(), colour.GetG() );
	u8 b = SubComponent( GetB(), colour.GetB() );
	u8 a = GetA();

	return c32( r, g, b, a );
}

c32	c32::SubA( c32 colour ) const
{
	u8 r = GetR();
	u8 g = GetG();
	u8 b = GetB();
	u8 a = SubComponent( GetA(), colour.GetA() );

	return c32( r, g, b, a );
}

c32	c32::Modulate( c32 colour ) const
{
	u8 r = ModulateComponent( GetR(), colour.GetR() );
	u8 g = ModulateComponent( GetG(), colour.GetG() );
	u8 b = ModulateComponent( GetB(), colour.GetB() );
	u8 a = ModulateComponent( GetA(), colour.GetA() );

	return c32( r, g, b, a );
}

c32	c32::ModulateRGB( c32 colour ) const
{
	u8 r = ModulateComponent( GetR(), colour.GetR() );
	u8 g = ModulateComponent( GetG(), colour.GetG() );
	u8 b = ModulateComponent( GetB(), colour.GetB() );
	u8 a = GetA();

	return c32( r, g, b, a );
}

c32	c32::ModulateA( c32 colour ) const
{
	u8 r = GetR();
	u8 g = GetG();
	u8 b = GetB();
	u8 a = ModulateComponent( GetA(), colour.GetA() );

	return c32( r, g, b, a );
}

c32	c32::Interpolate( c32 colour, float factor ) const
{
	u8 r = InterpolateComponent( GetR(), colour.GetR(), factor );
	u8 g = InterpolateComponent( GetG(), colour.GetG(), factor );
	u8 b = InterpolateComponent( GetB(), colour.GetB(), factor );
	u8 a = InterpolateComponent( GetA(), colour.GetA(), factor );

	return c32( r, g, b, a );
}

c32	c32::Interpolate( c32 colour, c32 factor ) const
{
	float	factor_r( factor.GetR() / 255.0f );
	float	factor_g( factor.GetG() / 255.0f );
	float	factor_b( factor.GetB() / 255.0f );
	float	factor_a( factor.GetA() / 255.0f );

	u8 r {InterpolateComponent( GetR(), colour.GetR(), factor_r )};
	u8 g {InterpolateComponent( GetG(), colour.GetG(), factor_g )};
	u8 b {InterpolateComponent( GetB(), colour.GetB(), factor_b )};
	u8 a {InterpolateComponent( GetA(), colour.GetA(), factor_a )};

	return c32( r, g, b, a );
}

c32 c32::ReplicateAlpha() const
{
	u8 a {GetA()};

	return c32( a, a, a, a );
}
