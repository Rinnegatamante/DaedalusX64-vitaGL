#pragma once

constexpr int8_t F3DEX2_G_NOOP = OPCODE(0x00);
constexpr int8_t F3DEX2_G_RDPHALF_2 = OPCODE(0xf1);
constexpr int8_t F3DEX2_G_SETOTHERMODE_H = OPCODE(0xe3);
constexpr int8_t F3DEX2_G_SETOTHERMODE_L = OPCODE(0xe2);
constexpr int8_t F3DEX2_G_RDPHALF_1 = OPCODE(0xe1);
constexpr int8_t F3DEX2_G_SPNOOP = OPCODE(0xe0);
constexpr int8_t F3DEX2_G_ENDDL = OPCODE(0xdf);
constexpr int8_t F3DEX2_G_DL = OPCODE(0xde);
constexpr int8_t F3DEX2_G_LOAD_UCODE = OPCODE(0xdd);
constexpr int8_t F3DEX2_G_MOVEMEM = OPCODE(0xdc);
constexpr int8_t F3DEX2_G_MOVEWORD = OPCODE(0xdb);
constexpr int8_t F3DEX2_G_MTX = OPCODE(0xda);
constexpr int8_t F3DEX2_G_GEOMETRYMODE = OPCODE(0xd9);
constexpr int8_t F3DEX2_G_POPMTX = OPCODE(0xd8);
constexpr int8_t F3DEX2_G_TEXTURE = OPCODE(0xd7);
constexpr int8_t F3DEX2_G_DMA_IO = OPCODE(0xd6);
constexpr int8_t F3DEX2_G_SPECIAL_1 = OPCODE(0xd5);
constexpr int8_t F3DEX2_G_SPECIAL_2 = OPCODE(0xd4);
constexpr int8_t F3DEX2_G_SPECIAL_3 = OPCODE(0xd3);

constexpr int8_t F3DEX2_G_VTX = OPCODE(0x01);
constexpr int8_t F3DEX2_G_MODIFYVTX = OPCODE(0x02);
constexpr int8_t F3DEX2_G_CULLDL = OPCODE(0x03);
constexpr int8_t F3DEX2_G_BRANCH_Z = OPCODE(0x04);
constexpr int8_t F3DEX2_G_TRI1 = OPCODE(0x05);
constexpr int8_t F3DEX2_G_TRI2 = OPCODE(0x06);
constexpr int8_t F3DEX2_G_QUAD = OPCODE(0x07);
constexpr int8_t F3DEX2_G_LINE3D = OPCODE(0x08);

/*
 * G_MTX: parameter flags
 */
constexpr int8_t F3DEX2_G_MTX_MODELVIEW = OPCODE(0x00);
constexpr int8_t F3DEX2_G_MTX_PROJECTION = OPCODE(0x04);
constexpr int8_t F3DEX2_G_MTX_MUL = OPCODE(0x00);
constexpr int8_t F3DEX2_G_MTX_LOAD = OPCODE(0x02);
constexpr int8_t F3DEX2_G_MTX_NOPUSH = OPCODE(0x00);
constexpr int8_t F3DEX2_G_MTX_PUSH = OPCODE(0x01);

/*
 * flags for G_SETGEOMETRYMODE
 * (this rendering state is maintained in RSP)
 *
 * DO NOT USE THE LOW 8 BITS OF GEOMETRYMODE:
 * The weird bit-ordering is for the micro-code: the lower byte
 * can be OR'd in with G_TRI_SHADE (11001100) to construct
 * the triangle command directly. Don't break it...
 *
 * DO NOT USE THE HIGH 8 BITS OF GEOMETRYMODE:
 * The high byte is OR'd with 0x703 to form the clip code mask.
 * If it is set to 0x04, this will cause near clipping to occur.
 * If it is zero, near clipping will not occur.
 *
 * Further explanation:
 * G_SHADE is necessary in order to see the color that you passed
 * down with the vertex. If G_SHADE isn't set, you need to set the DP
 * appropriately and use primcolor to see anything.
 *
 * G_SHADING_SMOOTH enabled means use all 3 colors of the triangle.
 * If it is not set, then do 'flat shading', where only one vertex color
 * is used (and all 3 vertices are set to that same color by the ucode)
 * See the man page for gSP1Triangle().
 *
 */
constexpr uint32_t F3DEX2_G_CLIPPING = 0x00800000;
constexpr uint32_t F3DEX2_G_TEXTURE_ENABLE = 0x00000000;
constexpr uint32_t F3DEX2_G_SHADING_SMOOTH = 0x00200000;
constexpr uint32_t F3DEX2_G_CULL_FRONT = 0x00000200;
constexpr uint32_t F3DEX2_G_CULL_BACK = 0x00000400;
constexpr uint32_t F3DEX2_G_CULL_BOTH = 0x00000600;

/*
 * MOVEMEM indices
 *
 * Each of these indexes an entry in a dmem table
 * which points to a 1-4 word block of dmem in
 * which to store a 1-4 word DMA.
 *
 */
constexpr int8_t F3DEX2_G_MV_MMTX = OPCODE(2);
constexpr int8_t F3DEX2_G_MV_PMTX = OPCODE(6);
constexpr int8_t F3DEX2_G_MV_VIEWPORT = OPCODE(8);
constexpr int8_t F3DEX2_G_MV_LIGHT = OPCODE(10);
constexpr int8_t F3DEX2_G_MV_POINT = OPCODE(12);
constexpr int8_t F3DEX2_G_MV_MATRIX = OPCODE(14);
constexpr int8_t F3DEX2_G_MVO_LOOKATX = OPCODE(0 * 24);
constexpr int8_t F3DEX2_G_MVO_LOOKATY = OPCODE(1 * 24);
constexpr int8_t F3DEX2_G_MVO_L0 = OPCODE(2 * 24);
constexpr int8_t F3DEX2_G_MVO_L1 = OPCODE(3 * 24);
constexpr int8_t F3DEX2_G_MVO_L2 = OPCODE(4 * 24);
constexpr int8_t F3DEX2_G_MVO_L3 = OPCODE(5 * 24);
constexpr int8_t F3DEX2_G_MVO_L4 = OPCODE(6 * 24);
constexpr int8_t F3DEX2_G_MVO_L5 = OPCODE(7 * 24);
constexpr int8_t F3DEX2_G_MVO_L6 = OPCODE(8 * 24);
constexpr int8_t F3DEX2_G_MVO_L7 = OPCODE(9 * 24);

/*
 * MOVEWORD indices
 *
 * Each of these indexes an entry in a dmem table
 * which points to a word in dmem in dmem where
 * an immediate word will be stored.
 *
 */
constexpr int8_t F3DEX2_G_MW_FORCEMTX = 0x0c;

/*
 * MOVEWORD indices
 *
 * Each of these indexes an entry in a dmem table
 * which points to a word in dmem in dmem where
 * an immediate word will be stored.
 *
 * These are offsets from the address in the dmem table
 */
constexpr int8_t F3DEX2_G_MWO_aLIGHT_2 = OPCODE(0x18);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_2 = OPCODE(0x1c);
constexpr int8_t F3DEX2_G_MWO_aLIGHT_3 = OPCODE(0x30);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_3 = OPCODE(0x34);
constexpr int8_t F3DEX2_G_MWO_aLIGHT_4 = OPCODE(0x48);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_4 = OPCODE(0x4c);
constexpr int8_t F3DEX2_G_MWO_aLIGHT_5 = OPCODE(0x60);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_5 = OPCODE(0x64);
constexpr int8_t F3DEX2_G_MWO_aLIGHT_6 = OPCODE(0x78);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_6 = OPCODE(0x7c);
constexpr int8_t F3DEX2_G_MWO_aLIGHT_7 = OPCODE(0x90);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_7 = OPCODE(0x94);
constexpr int8_t F3DEX2_G_MWO_aLIGHT_8 = OPCODE(0xa8);
constexpr int8_t F3DEX2_G_MWO_bLIGHT_8 = OPCODE(0xac);

/*===========================================================================*
 *	GBI Commands for S2DEX microcode
 *===========================================================================*/
/* GBI Header */
constexpr int8_t F3DEX2_G_OBJ_RECTANGLE_R = OPCODE(0xda);
constexpr int8_t F3DEX2_G_OBJ_MOVEMEM = OPCODE(0xdc);
constexpr int8_t F3DEX2_G_RDPHALF_0 = OPCODE(0xe4);
constexpr int8_t F3DEX2_G_OBJ_RECTANGLE = OPCODE(0x01);
constexpr int8_t F3DEX2_G_OBJ_SPRITE = OPCODE(0x02);
constexpr int8_t F3DEX2_G_SELECT_DL = OPCODE(0x04);
constexpr int8_t F3DEX2_G_OBJ_LOADTXTR = OPCODE(0x05);
constexpr int8_t F3DEX2_G_OBJ_LDTX_SPRITE = OPCODE(0x06);
constexpr int8_t F3DEX2_G_OBJ_LDTX_RECT = OPCODE(0x07);
constexpr int8_t F3DEX2_G_OBJ_LDTX_RECT_R = OPCODE(0x08);
constexpr int8_t F3DEX2_G_BG_1CYC = OPCODE(0x09);
constexpr int8_t F3DEX2_G_BG_COPY = OPCODE(0x0a);
constexpr int8_t F3DEX2_G_OBJ_RENDERMODE = OPCODE(0x0b);