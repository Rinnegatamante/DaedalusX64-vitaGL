//Fixed point identity matrix
static const u32 s_IdentMatrixL[16] =
{
	0x00010000,	0x00000000,
	0x00000001,	0x00000000,
	0x00000000,	0x00010000,
	0x00000000,	0x00000001,
	0x00000000, 0x00000000,
	0x00000000,	0x00000000,
	0x00000000, 0x00000000,
	0x00000000,	0x00000000
};

//Floating point identity matrix
static const u32 s_IdentMatrixF[16] =
{
	0x3f800000,	0x00000000,
	0x00000000,	0x00000000,
	0x00000000,	0x3f800000,
	0x00000000,	0x00000000,
	0x00000000, 0x00000000,
	0x3f800000,	0x00000000,
	0x00000000, 0x00000000,
	0x00000000,	0x3f800000
};

u32 Patch_guMtxIdentF()
{
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 *pMtxBase = (u8*)ReadAddress(address);
	memcpy_neon(pMtxBase, s_IdentMatrixF, 0x40);

	return PATCH_RET_JR_RA;
}



u32 Patch_guMtxIdent()
{
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);
	memcpy_neon(pMtxBase, s_IdentMatrixL, 0x40);

	return PATCH_RET_JR_RA;
}

u32 Patch_guTranslateF()
{
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	memcpy_neon(pMtxBase, s_IdentMatrixF, 0x30);

	QuickWrite32Bits(pMtxBase, 0x30, gGPR[REG_a1]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x34, gGPR[REG_a2]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x38, gGPR[REG_a3]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);

	return PATCH_RET_JR_RA;
}

u32 Patch_guTranslate()
{
	const f32 fScale = 65536.0f;

	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	u32 x = (s32)(fx * fScale);
	u32 y = (s32)(fy * fScale);
	u32 z = (s32)(fz * fScale);

	u32 one = 65536;

	u32 xyhibits = (x & 0xFFFF0000) | (y >> 16);
	u32 xylobits = (x << 16) | (y & 0x0000FFFF);

	u32 z1hibits = (z & 0xFFFF0000) | (one >> 16);
	u32 z1lobits = (z << 16) | (one & 0x0000FFFF);
	
	memcpy_neon(pMtxBase, s_IdentMatrixL, 0x18);

	QuickWrite32Bits(pMtxBase, 0x18, xyhibits);	// xy
	QuickWrite32Bits(pMtxBase, 0x1c, z1hibits);	// z1
	
	memset(pMtxBase + 0x20, 0, 0x18);

	QuickWrite32Bits(pMtxBase, 0x38, xylobits);	// xy
	QuickWrite32Bits(pMtxBase, 0x3c, z1lobits);	// z1

	return PATCH_RET_JR_RA;
}

u32 Patch_guScaleF()
{
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	QuickWrite32Bits(pMtxBase, 0x00, gGPR[REG_a1]._u32_0);
	memset(pMtxBase + 0x04, 0, 0x10); 
	QuickWrite32Bits(pMtxBase, 0x14, gGPR[REG_a2]._u32_0);
	memset(pMtxBase + 0x18, 0, 0x10); 
	QuickWrite32Bits(pMtxBase, 0x28, gGPR[REG_a3]._u32_0);
	memcpy_neon(pMtxBase + 0x2C, &s_IdentMatrixF[11], 0x14);

	return PATCH_RET_JR_RA;
}

u32 Patch_guScale()
{
	const f32 fScale = 65536.0f;

	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	u32 x = (s32)(fx * fScale);
	u32 y = (s32)(fy * fScale);
	u32 z = (s32)(fz * fScale);

	u32 xzhibits = x & 0xFFFF0000;
	u32 xzlobits = x << 16;

	u32 zyhibits = y >> 16;
	u32 zylobits = y & 0x0000FFFF;

	u32 zzhibits = z & 0xFFFF0000;
	u32 zzlobits = z << 16;

	QuickWrite32Bits(pMtxBase, 0x00, xzhibits);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, zyhibits);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, zzhibits);
	QuickWrite32Bits(pMtxBase, 0x18, 0x00000000); // xy
	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000001); // z1

	QuickWrite32Bits(pMtxBase, 0x20, xzlobits);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, zylobits);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, zzlobits);
	QuickWrite32Bits(pMtxBase, 0x38, 0x00000000); // xy
	QuickWrite32Bits(pMtxBase, 0x3c, 0x00000000); // z1

	return PATCH_RET_JR_RA;
}

u32 Patch_guMtxF2L()
{
	const f32 fScale = 65536.0f;

	u8 *pMtxFBase = (u8*)ReadAddress(gGPR[REG_a0]._u32_0);
	u8 *pMtxBase  = (u8*)ReadAddress(gGPR[REG_a1]._u32_0);

	u8 *pMtxLBaseHiBits = (u8*)(pMtxBase + 0x00);
	u8 *pMtxLBaseLoBits = (u8*)(pMtxBase + 0x20);

	REG32 a, b;
	u32 tmp_a, tmp_b;
	u32 hibits;
	u32 lobits;
	u32 row;

	for (row = 0; row < 4; row++)
	{
		a._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0x0);
		b._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0x4);

		tmp_a = (s32)(a._f32 * fScale);
		tmp_b = (s32)(b._f32 * fScale);

		hibits = (tmp_a & 0xFFFF0000) | (tmp_b >> 16);
		QuickWrite32Bits(pMtxLBaseHiBits, (row << 3) , hibits);

		lobits = (tmp_a << 16) | (tmp_b & 0x0000FFFF);
		QuickWrite32Bits(pMtxLBaseLoBits, (row << 3) , lobits);

		/////
		a._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0x8);
		b._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0xc);

		tmp_a = (s32)(a._f32 * fScale);
		tmp_b = (s32)(b._f32 * fScale);

		hibits = (tmp_a & 0xFFFF0000) | (tmp_b >> 16);
		QuickWrite32Bits(pMtxLBaseHiBits, (row << 3) + 4, hibits);

		lobits = (tmp_a << 16) | (tmp_b & 0x0000FFFF);
		QuickWrite32Bits(pMtxLBaseLoBits, (row << 3) + 4, lobits);
	}

	return PATCH_RET_JR_RA;
}

u32 Patch_guNormalize_Mario()
{
	u8 *pXBase  = (u8*)ReadAddress(gGPR[REG_a0]._u32_0);
	u8 *pYBase  = (u8*)ReadAddress(gGPR[REG_a1]._u32_0);
	u8 *pZBase  = (u8*)ReadAddress(gGPR[REG_a2]._u32_0);

	REG32 r[4];
	r[0]._u32 = QuickRead32Bits(pXBase, 0x0);
	r[1]._u32 = QuickRead32Bits(pYBase, 0x0);
	r[2]._u32 = QuickRead32Bits(pZBase, 0x0);
	r[3]._f32 = 0.0f;
#ifdef DAEDALUS_VITA
	normalize4_neon((float*)r, (float*)r);
#else
	f32 fLenRecip = 1.0f / sqrtf((r[0]._f32 * r[0]._f32) + (r[1]._f32 * r[1]._f32) + (r[2]._f32 * r[2]._f32));

	r[0]._f32 *= fLenRecip;
 	r[1]._f32 *= fLenRecip;
 	r[2]._f32 *= fLenRecip;
#endif
	QuickWrite32Bits(pXBase, r[0]._u32);
	QuickWrite32Bits(pYBase, r[1]._u32);
	QuickWrite32Bits(pZBase, r[2]._u32);

	return PATCH_RET_JR_RA;
}

// NOT the same function as guNormalise_Mario
// This take one pointer, not 3
u32 Patch_guNormalize_Rugrats() //Using VFPU and no memcpy //Corn
{
	u8 *pBase  = (u8*)ReadAddress(gGPR[REG_a0]._u32_0);
	normalize3_neon((float*)pBase, (float*)pBase);

	return PATCH_RET_JR_RA;
}

u32 Patch_guOrthoF()
{
	REG32 l, r, b;
	REG32 tnfs[4];
	
	u8 * pMtxBase   = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);	// Base address
	u8 * pStackBase = g_pu8RamBase_8000 + gGPR[REG_sp]._u32_0;	//Base stack address, this is safe since stack is always in physical memory
	l._u32 = gGPR[REG_a1]._u32_0;	//Left
	r._u32 = gGPR[REG_a2]._u32_0;	//Right
	b._u32 = gGPR[REG_a3]._u32_0;	//Bottom
	
	memcpy_neon(tnfs, pStackBase + 0x10, 0x10);

	f32 fRmL = r._f32 - l._f32;
	f32 fTmB = tnfs[0]._f32 - b._f32;
	f32 fFmN = tnfs[2]._f32 - tnfs[1]._f32;
	f32 fRpL = r._f32 + l._f32;
	f32 fTpB = tnfs[0]._f32 + b._f32;
	f32 fFpN = tnfs[2]._f32 + tnfs[1]._f32;

	// Re-use unused old variables to store Matrix values
	f32 s2 = 2.0f * tnfs[3]._f32;
	l._f32 =  s2 / fRmL;
	r._f32 =  s2 / fTmB;
	b._f32 = -s2 / fFmN;

	tnfs[0]._f32 = -fRpL * tnfs[3]._f32 / fRmL;
	tnfs[1]._f32 = -fTpB * tnfs[3]._f32 / fTmB;
	tnfs[2]._f32 = -fFpN * tnfs[3]._f32 / fFmN;

	QuickWrite32Bits(pMtxBase, 0x00, l._u32);
	memset(pMtxBase + 0x04, 0, 0x10);
	QuickWrite32Bits(pMtxBase, 0x14, r._u32);
	memset(pMtxBase + 0x18, 0, 0x10);
	QuickWrite32Bits(pMtxBase, 0x28, b._u32);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);
	QuickWrite32Bits(pMtxBase, 0x30, tnfs[0]._u32);
	QuickWrite32Bits(pMtxBase, 0x34, tnfs[1]._u32);
	QuickWrite32Bits(pMtxBase, 0x38, tnfs[2]._u32);
	QuickWrite32Bits(pMtxBase, 0x3c, tnfs[3]._u32);

	return PATCH_RET_JR_RA;
}

//Do the float version on a temporary matrix and convert to fixed point in VFPU & CPU //Corn
u32 Patch_guOrtho()
{
	REG32 l, r, b;
	REG32 tnfs[4];

	u8 *pMtxBase   = (u8*)ReadAddress(gGPR[REG_a0]._u32_0); // Fixed point Base address
	u8 *pStackBase = g_pu8RamBase_8000 + gGPR[REG_sp]._u32_0; //Base stack address, this is safe since stack is always in physical memory
	l._u32 = gGPR[REG_a1]._u32_0; //Left
	r._u32 = gGPR[REG_a2]._u32_0; //Right
	b._u32 = gGPR[REG_a3]._u32_0; //Bottom
	memcpy_neon(tnfs, pStackBase + 0x10, 0x10);

	u8 *pMtxLBaseHiBits = (u8*)(pMtxBase + 0x00);
	u8 *pMtxLBaseLoBits = (u8*)(pMtxBase + 0x20);

	const f32 fScale = 65536.0f;

	f32 fRmL = r._f32 - l._f32;
	f32 fTmB = tnfs[0]._f32 - b._f32;
	f32 fFmN = tnfs[2]._f32 - tnfs[1]._f32;
	f32 fRpL = r._f32 + l._f32;
	f32 fTpB = tnfs[0]._f32 + b._f32;
	f32 fFpN = tnfs[2]._f32 + tnfs[1]._f32;
	f32 s2 = 2.0f * tnfs[3]._f32;
	
	l._f32 =  s2 / fRmL;
	r._f32 =  s2 / fTmB;
	b._f32 = -s2 / fFmN;

	tnfs[0]._f32 = -fRpL * tnfs[3]._f32 / fRmL;
	tnfs[1]._f32 = -fTpB * tnfs[3]._f32 / fTmB;
	tnfs[2]._f32 = -fFpN * tnfs[3]._f32 / fFmN;
	
	u32 l_u = (s32)(l._f32 * fScale); // 0
	u32 r_u = (s32)(r._f32 * fScale); // 5
	u32 b_u = (s32)(b._f32 * fScale); // 10
	u32 t_u = (s32)(tnfs[0]._f32 * fScale); // 12
	u32 n_u = (s32)(tnfs[1]._f32 * fScale); // 13
	u32 f_u = (s32)(tnfs[2]._f32 * fScale); // 14
	u32 s_u = (s32)(tnfs[3]._f32 * fScale); // 15
	
	// 0,1
	QuickWrite32Bits(pMtxLBaseHiBits, 0x00, l_u & 0xFFFF0000);
	QuickWrite32Bits(pMtxLBaseLoBits, 0x00, l_u << 16);
	// 2,3
	QuickWrite32Bits(pMtxLBaseHiBits, 0x04, 0);
	QuickWrite32Bits(pMtxLBaseLoBits, 0x04, 0);
	// 4,5
	QuickWrite32Bits(pMtxLBaseHiBits, 0x08, r_u >> 16);
	QuickWrite32Bits(pMtxLBaseLoBits, 0x08, r_u & 0x0000FFFF);
	// 6,7
	QuickWrite32Bits(pMtxLBaseHiBits, 0x0C, 0);
	QuickWrite32Bits(pMtxLBaseLoBits, 0x0C, 0);
	// 8,9
	QuickWrite32Bits(pMtxLBaseHiBits, 0x10, 0);
	QuickWrite32Bits(pMtxLBaseLoBits, 0x10, 0);
	// 10,11
	QuickWrite32Bits(pMtxLBaseHiBits, 0x14, b_u & 0xFFFF0000);
	QuickWrite32Bits(pMtxLBaseLoBits, 0x14, b_u << 16);
	// 12,13
	QuickWrite32Bits(pMtxLBaseHiBits, 0x18, (t_u & 0xFFFF0000) | (n_u >> 16));
	QuickWrite32Bits(pMtxLBaseLoBits, 0x18, (n_u & 0x0000FFFF) | (t_u << 16));
	// 14,15
	QuickWrite32Bits(pMtxLBaseHiBits, 0x1C, (f_u & 0xFFFF0000) | (s_u >> 16));
	QuickWrite32Bits(pMtxLBaseLoBits, 0x1C, (s_u & 0x0000FFFF) | (f_u << 16));

	return PATCH_RET_JR_RA;
}

//RotateF //Corn
u32 Patch_guRotateF()
{
	f32 s,c;
	REG32 a, r, x, y, z;

	u8 * pMtxBase = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);		//Matrix base address
	u8 * pStackBase = g_pu8RamBase_8000 + gGPR[REG_sp]._u32_0;	//Base stack address, this is safe since stack is always in physical memory

	a._u32 = gGPR[REG_a1]._u32_0;	//Angle in degrees + -> CCW
	x._u32 = gGPR[REG_a2]._u32_0;	//X
	y._u32 = gGPR[REG_a3]._u32_0;	//Y
	z._u32 = QuickRead32Bits(pStackBase, 0x10);	//Z

	sincosf(a._f32*(PI/180.0f), &s, &c);

	float xx = x._f32 * x._f32;
	float xy = x._f32 * y._f32;
	float yy = y._f32 * y._f32;
	float zz = z._f32 * z._f32;
	float xs = x._f32 * s;
	float yz = y._f32 * z._f32;
	float ys = y._f32 * s;
	float xz = x._f32 * z._f32;
	float c1 = 1.0f - c;
	float zs = z._f32 * s;
	
//Row #1
	r._f32 = xx + c * (1.0f - xx);
	QuickWrite32Bits(pMtxBase, 0x00, r._u32);

	r._f32 = xy * c1 + zs;
	QuickWrite32Bits(pMtxBase, 0x04, r._u32);

	r._f32 = xz * c1 - ys;
	QuickWrite32Bits(pMtxBase, 0x08, r._u32);

	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

//Row #2
	r._f32 = xy * c1 - zs;
	QuickWrite32Bits(pMtxBase, 0x10, r._u32);

	r._f32 = yy + c * (1.0f - yy);
	QuickWrite32Bits(pMtxBase, 0x14, r._u32);

	r._f32 = yz * c1 + xs;
	QuickWrite32Bits(pMtxBase, 0x18, r._u32);

	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000000);

//Row #3
	r._f32 = xz * c1 + ys;
	QuickWrite32Bits(pMtxBase, 0x20, r._u32);

	r._f32 = yz * c1 - xs;
	QuickWrite32Bits(pMtxBase, 0x24, r._u32);

	r._f32 = zz + c * (1.0f - zz);
	QuickWrite32Bits(pMtxBase, 0x28, r._u32);
	
	memcpy_neon(pMtxBase + 0x2c, &s_IdentMatrixF[11], 0x14);

	return PATCH_RET_JR_RA;
}