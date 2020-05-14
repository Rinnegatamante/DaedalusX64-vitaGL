#include "stdafx.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Math.h"
#include "Math/MathUtil.h" // Swap

#ifdef DAEDALUS_PSP
#include <pspvfpu.h>
#endif

#ifdef DAEDALUS_VITA
extern "C" {
#include <math_neon.h>
};
#endif

// http://forums.ps2dev.org/viewtopic.php?t=5557
// http://bradburn.net/mr.mr/vfpu.html

// Many of these mtx funcs should be inline since they are simple enough and called frequently - Salvy

#ifdef DAEDALUS_PSP_USE_VFPU
void MatrixMultiplyUnaligned(Matrix4x4 * m_out, const Matrix4x4 *mat_a, const Matrix4x4 *mat_b)
{
	__asm__ volatile (

		"ulv.q   R000, 0  + %1\n"
		"ulv.q   R001, 16 + %1\n"
		"ulv.q   R002, 32 + %1\n"
		"ulv.q   R003, 48 + %1\n"

		"ulv.q   R100, 0  + %2\n"
		"ulv.q   R101, 16 + %2\n"
		"ulv.q   R102, 32 + %2\n"
		"ulv.q   R103, 48 + %2\n"

		"vmmul.q   M200, M000, M100\n"

		"usv.q   R200, 0  + %0\n"
		"usv.q   R201, 16 + %0\n"
		"usv.q   R202, 32 + %0\n"
		"usv.q   R203, 48 + %0\n"

		: "=m" (*m_out) : "m" (*mat_a) ,"m" (*mat_b) : "memory" );
}

void MatrixMultiplyAligned(Matrix4x4 * m_out, const Matrix4x4 *mat_a, const Matrix4x4 *mat_b)
{
	__asm__ volatile (

		"lv.q   R000, 0  + %1\n"
		"lv.q   R001, 16 + %1\n"
		"lv.q   R002, 32 + %1\n"
		"lv.q   R003, 48 + %1\n"

		"lv.q   R100, 0  + %2\n"
		"lv.q   R101, 16 + %2\n"
		"lv.q   R102, 32 + %2\n"
		"lv.q   R103, 48 + %2\n"

		"vmmul.q   M200, M000, M100\n"

		"sv.q   R200, 0  + %0\n"
		"sv.q   R201, 16 + %0\n"
		"sv.q   R202, 32 + %0\n"
		"sv.q   R203, 48 + %0\n"

		: "=m" (*m_out) : "m" (*mat_a) ,"m" (*mat_b) : "memory" );
}

#else // DAEDALUS_PSP_USE_VFPU


void MatrixMultiplyUnaligned(Matrix4x4 * m_out, const Matrix4x4 *mat_a, const Matrix4x4 *mat_b)
{
#ifdef DAEDALUS_VITA
	matmul4_neon((float*)mat_b, (float*)mat_a, (float*)m_out);
#else
	*m_out = *mat_a * *mat_b;
#endif
}

void MatrixMultiplyAligned(Matrix4x4 * m_out, const Matrix4x4 *mat_a, const Matrix4x4 *mat_b)
{
#ifdef DAEDALUS_VITA
	matmul4_neon((float*)mat_b, (float*)mat_a, (float*)m_out);
#else
	*m_out = *mat_a * *mat_b;
#endif
}

#endif // DAEDALUS_PSP_USE_VFPU

v3 Matrix4x4::TransformNormal( const v3 & vec ) const
{
#ifdef DAEDALUS_VITA
	v4 r;
	matvec4_neon((float*)this, (float*)v4(vec.x, vec.y, vec.z, 0.0f).f, r.f);
	return v3(r.x, r.y, r.z);
#else
	return v3( vec.x * m11 + vec.y * m21 + vec.z * m31,
			   vec.x * m12 + vec.y * m22 + vec.z * m32,
			   vec.x * m13 + vec.y * m23 + vec.z * m33 );
#endif
}

v4 Matrix4x4::Transform( const v4 & vec ) const
{
#ifdef DAEDALUS_VITA
	v4 r;
	matvec4_neon((float*)this, (float*)vec.f, r.f);
	return r;
#else
	return v4( vec.x * m11 + vec.y * m21 + vec.z * m31 + vec.w * m41,
			   vec.x * m12 + vec.y * m22 + vec.z * m32 + vec.w * m42,
			   vec.x * m13 + vec.y * m23 + vec.z * m33 + vec.w * m43,
			   vec.x * m14 + vec.y * m24 + vec.z * m34 + vec.w * m44 );
#endif
}

Matrix4x4 Matrix4x4::operator*( const Matrix4x4 & rhs ) const
{
	Matrix4x4 r;

//VFPU
#if defined(DAEDALUS_PSP) || defined(DAEDALUS_VITA)
	MatrixMultiplyUnaligned( &r, this, &rhs );
//CPU
#else
	for ( u32 i = 0; i < 4; ++i )
	{
		for ( u32 j = 0; j < 4; ++j )
		{
			r.m[ i ][ j ] = m[ i ][ 0 ] * rhs.m[ 0 ][ j ] +
							m[ i ][ 1 ] * rhs.m[ 1 ][ j ] +
							m[ i ][ 2 ] * rhs.m[ 2 ][ j ] +
							m[ i ][ 3 ] * rhs.m[ 3 ][ j ];
		}
	}
#endif

	return r;
}

const Matrix4x4	gMatrixIdentity(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f );
