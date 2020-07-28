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

v3 Matrix4x4::TransformNormal( const v3 & vec ) const
{
#ifdef DAEDALUS_VITA
	v3 r;
	matvec4_neon((float*)this, (float*)vec.f, r.f);
	return r;
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

	MatrixMultiplyUnaligned( &r, this, &rhs );

	return r;
}

const Matrix4x4	gMatrixIdentity(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f );
