u32 Patch_osInvalICache_Mario()
{
#ifdef DAEDALUS_ENABLE_DYNAREC
	u32 p = gGPR[REG_a0]._u32_0;
	u32 len = gGPR[REG_a1]._u32_0;

	CPU_InvalidateICacheRange(p, len);
#endif

	return PATCH_RET_JR_RA;
}

u32 Patch_osInvalICache_Rugrats()
{
	return Patch_osInvalICache_Mario();
}


u32 Patch_osInvalDCache_Mario()
{
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osInvalDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}
u32 Patch_osInvalDCache_Rugrats()
{
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osInvalDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}


u32 Patch_osWritebackDCache_Mario()
{
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osWritebackDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}
u32 Patch_osWritebackDCache_Rugrats()
{
	//u32 p = gGPR[REG_a0]._u32_0;
	//u32 len = gGPR[REG_a1]._u32_0;

	//DBGConsole_Msg(0, "osWritebackDCache(0x%08x, %d)", p, len);

	return PATCH_RET_JR_RA;
}


u32 Patch_osWritebackDCacheAll()
{
	//DBGConsole_Msg(0, "osWritebackDCacheAll()");

	return PATCH_RET_JR_RA;
}
