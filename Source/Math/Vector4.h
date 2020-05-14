#ifndef MATH_VECTOR4_H_
#define MATH_VECTOR4_H_

#ifdef DAEDALUS_VITA
extern "C" {
#include <math_neon.h>
};
#endif

#include <string.h>

#include "Vector3.h"

#include "Math/Math.h"	// VFPU Math
#include "Utility/Alignment.h"
#include "Utility/DaedalusTypes.h"

ALIGNED_TYPE(class, v4, 16)
{
public:
	v4() {}
	v4( float _x, float _y, float _z, float _w ) : x( _x ), y( _y ), z( _z ), w( _w ) {}
	v4( float n[4] ) {memcpy(f, n, sizeof(float)*4);}
	v4( v3 v ) : w(1.0f) {memcpy(f, v.f, sizeof(float)*3);}
	
	void Normalise()
	{
#ifdef DAEDALUS_VITA
		normalize4_neon(f, f);
#else
		float	len_sq( LengthSq() );
		if(len_sq > 0.0001f)
		{
#ifdef DAEDALUS_PSP
			float r( vfpu_invSqrt( len_sq ) );
#else
			float r( InvSqrt( len_sq ) );
#endif
			x *= r;
			y *= r;
			z *= r;
			w *= r;
		}
#endif
	}

	v4 operator+( const v4 & v ) const
	{
		return v4( x + v.x, y + v.y, z + v.z, w + v.w );
	}
	v4 operator-( const v4 & v ) const
	{
		return v4( x - v.x, y - v.y, z - v.z, w - v.w );
	}

	v4 operator*( float s ) const
	{
		return v4( x * s, y * s, z * s, w * s );
	}

	float Length() const
	{
		return sqrtf( (x*x)+(y*y)+(z*z)+(w*w) );
	}

	float LengthSq() const
	{
		return (x*x)+(y*y)+(z*z)+(w*w);
	}

	float Dot( const v4 & rhs ) const
	{
		return (x*rhs.x) + (y*rhs.y) + (z*rhs.z) + (w*rhs.w);
	}
	
	union {
		struct { float x, y, z, w; };
		float f[4];
	};
};


#endif // MATH_VECTOR4_H_
