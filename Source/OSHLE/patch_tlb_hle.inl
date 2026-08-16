//*****************************************************************************
//
//*****************************************************************************
// Todo: Implement me
u32 Patch_osMapTLB()
{
	//osMapTLB(s32, OSPageMask, void *, u32, u32, s32)
	return PATCH_RET_NOT_PROCESSED;
}

//*****************************************************************************
//
//*****************************************************************************
// ENTRYHI left untouched after call
u32 Patch___osProbeTLB()
{
	u32 addr = gGPR[REG_a0]._u32_0;
	//DBGConsole_Msg(0, "Probe: 0x%08x -> 0x%08x", VAddr, dwPAddr);

	gGPR[REG_v0]._s64 = (s64)OS_HLE___osProbeTLB( addr );

	return PATCH_RET_JR_RA;
}

//*****************************************************************************
//
//*****************************************************************************
u32 Patch_osVirtualToPhysical_Mario()
{
	u32 addr = gGPR[REG_a0]._u32_0;
	//DBGConsole_Msg(0, "osVirtualToPhysical(0x%08x)", addr);

	gGPR[REG_v0]._s64 = (s64)ConvertToPhysics( addr );

	return PATCH_RET_JR_RA;
}

//*****************************************************************************
//
//*****************************************************************************
// Identical - just optimised
u32 Patch_osVirtualToPhysical_Rugrats()
{
	u32 addr = gGPR[REG_a0]._u32_0;
	//DBGConsole_Msg(0, "osVirtualToPhysical(0x%08x)", (u32)gGPR[REG_a0]);

	gGPR[REG_v0]._s64 = (s64)ConvertToPhysics( addr );

	return PATCH_RET_JR_RA;
}

//*****************************************************************************
//
//*****************************************************************************
// Todo: Implement me
u32 Patch_osMapTLBRdb()
{
	return PATCH_RET_NOT_PROCESSED;
}

//*****************************************************************************
//
//*****************************************************************************
// Todo: Implement me
u32 Patch_osUnmapTLBAll_Mario()
{
	return PATCH_RET_NOT_PROCESSED;
}

//*****************************************************************************
//
//*****************************************************************************
// Identical to mario, different loop structure
u32 Patch_osUnmapTLBAll_Rugrats()
{
	return PATCH_RET_NOT_PROCESSED;
}
