#pragma once

#define G_IMMFIRST -65

constexpr int8_t F3DEX_G_SPNOOP = OPCODE(0x0);
constexpr int8_t F3DEX_G_MTX = OPCODE(0x1);
constexpr int8_t F3DEX_G_RESERVED0 = OPCODE(0x2);
constexpr int8_t F3DEX_G_MOVEMEM = OPCODE(0x3);
constexpr int8_t F3DEX_G_VTX = OPCODE(0x4);
constexpr int8_t F3DEX_G_RESERVED1 = OPCODE(0x5);
constexpr int8_t F3DEX_G_DL = OPCODE(0x6);
constexpr int8_t F3DEX_G_RESERVED2 = OPCODE(0x7);
constexpr int8_t F3DEX_G_RESERVED3 = OPCODE(0x8);
constexpr int8_t F3DEX_G_TRI1 = OPCODE(G_IMMFIRST - 0);
constexpr int8_t F3DEX_G_CULLDL = OPCODE(G_IMMFIRST - 1);
constexpr int8_t F3DEX_G_POPMTX = OPCODE(G_IMMFIRST - 2);
constexpr int8_t F3DEX_G_MOVEWORD = OPCODE(G_IMMFIRST - 3);
constexpr int8_t F3DEX_G_TEXTURE = OPCODE(G_IMMFIRST - 4);
constexpr int8_t F3DEX_G_SETOTHERMODE_H = OPCODE(G_IMMFIRST - 5);
constexpr int8_t F3DEX_G_SETOTHERMODE_L = OPCODE(G_IMMFIRST - 6);
constexpr int8_t F3DEX_G_ENDDL = OPCODE(G_IMMFIRST - 7);
constexpr int8_t F3DEX_G_SETGEOMETRYMODE = OPCODE(G_IMMFIRST - 8);
constexpr int8_t F3DEX_G_CLEARGEOMETRYMODE = OPCODE(G_IMMFIRST - 9);
constexpr int8_t F3DEX_G_LINE3D = OPCODE(G_IMMFIRST - 10);
constexpr int8_t F3DEX_G_RDPHALF_1 = OPCODE(G_IMMFIRST - 11);
constexpr int8_t F3DEX_G_RDPHALF_2 = OPCODE(G_IMMFIRST - 12);
constexpr int8_t F3DEX_G_MODIFYVTX = OPCODE(G_IMMFIRST - 13);
constexpr int8_t F3DEX_G_RDPHALF_CONT = OPCODE(G_IMMFIRST - 13);
constexpr int8_t F3DEX_G_TRI2 = OPCODE(G_IMMFIRST - 14);
constexpr int8_t F3DEX_G_BRANCH_Z = OPCODE(G_IMMFIRST - 15);
constexpr int8_t F3DEX_G_LOAD_UCODE = OPCODE(G_IMMFIRST - 16);
constexpr int8_t F3DEX_G_QUAD = OPCODE(G_IMMFIRST - 10);

// S2DEX
constexpr int8_t F3DEX_G_SPRITE2D_BASE = OPCODE(9);
constexpr int8_t F3DEX_G_SPRITE2D_SCALEFLIP = OPCODE(G_IMMFIRST - 1);
constexpr int8_t F3DEX_G_SPRITE2D_DRAW = OPCODE(G_IMMFIRST - 2);
constexpr int8_t F3DEX_G_NOOP = OPCODE(0xc0);

/*
 * G_MTX: parameter flags
 */
constexpr int8_t F3DEX_G_MTX_MODELVIEW = 0x00;
constexpr int8_t F3DEX_G_MTX_PROJECTION = 0x01;
constexpr int8_t F3DEX_G_MTX_MUL = 0x00;
constexpr int8_t F3DEX_G_MTX_LOAD = 0x02;
constexpr int8_t F3DEX_G_MTX_NOPUSH = 0x00;
constexpr int8_t F3DEX_G_MTX_PUSH = 0x04;

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

constexpr uint32_t F3DEX_G_CLIPPING = 0x00800000;
constexpr uint32_t F3DEX_G_TEXTURE_ENABLE = 0x00000002;
constexpr uint32_t F3DEX_G_SHADING_SMOOTH = 0x00000200;
constexpr uint32_t F3DEX_G_CULL_FRONT = 0x00001000;
constexpr uint32_t F3DEX_G_CULL_BACK = 0x00002000;
constexpr uint32_t F3DEX_G_CULL_BOTH = 0x00003000;

/*
 * MOVEMEM indices
 *
 * Each of these indexes an entry in a dmem table
 * which points to a 1-4 word block of dmem in
 * which to store a 1-4 word DMA.
 *
 */
constexpr uint8_t F3DEX_G_MV_VIEWPORT = 0x80;
constexpr uint8_t F3DEX_G_MV_LOOKATY = 0x82;
constexpr uint8_t F3DEX_G_MV_LOOKATX = 0x84;
constexpr uint8_t F3DEX_G_MV_L0 = 0x86;
constexpr uint8_t F3DEX_G_MV_L1 = 0x88;
constexpr uint8_t F3DEX_G_MV_L2 = 0x8a;
constexpr uint8_t F3DEX_G_MV_L3 = 0x8c;
constexpr uint8_t F3DEX_G_MV_L4 = 0x8e;
constexpr uint8_t F3DEX_G_MV_L5 = 0x90;
constexpr uint8_t F3DEX_G_MV_L6 = 0x92;
constexpr uint8_t F3DEX_G_MV_L7 = 0x94;
constexpr uint8_t F3DEX_G_MV_TXTATT = 0x96;
constexpr uint8_t F3DEX_G_MV_MATRIX_1 = 0x9e;
constexpr uint8_t F3DEX_G_MV_MATRIX_2 = 0x98;
constexpr uint8_t F3DEX_G_MV_MATRIX_3 = 0x9a;
constexpr uint8_t F3DEX_G_MV_MATRIX_4 = 0x9c;

/*
 * MOVEWORD indices
 *
 * Each of these indexes an entry in a dmem table
 * which points to a word in dmem in dmem where
 * an immediate word will be stored.
 *
 */
constexpr int8_t F3DEX_G_MW_POINTS = 0x0c;

/*
 * These are offsets from the address in the dmem table
 */

constexpr int8_t F3DEX_G_MWO_aLIGHT_2 = OPCODE(0x20);
constexpr int8_t F3DEX_G_MWO_bLIGHT_2 = OPCODE(0x24);
constexpr int8_t F3DEX_G_MWO_aLIGHT_3 = OPCODE(0x40);
constexpr int8_t F3DEX_G_MWO_bLIGHT_3 = OPCODE(0x44);
constexpr int8_t F3DEX_G_MWO_aLIGHT_4 = OPCODE(0x60);
constexpr int8_t F3DEX_G_MWO_bLIGHT_4 = OPCODE(0x64);
constexpr int8_t F3DEX_G_MWO_aLIGHT_5 = OPCODE(0x80);
constexpr int8_t F3DEX_G_MWO_bLIGHT_5 = OPCODE(0x84);
constexpr int8_t F3DEX_G_MWO_aLIGHT_6 = OPCODE(0xa0);
constexpr int8_t F3DEX_G_MWO_bLIGHT_6 = OPCODE(0xa4);
constexpr int8_t F3DEX_G_MWO_aLIGHT_7 = OPCODE(0xc0);
constexpr int8_t F3DEX_G_MWO_bLIGHT_7 = OPCODE(0xc4);
constexpr int8_t F3DEX_G_MWO_aLIGHT_8 = OPCODE(0xe0);
constexpr int8_t F3DEX_G_MWO_bLIGHT_8 = OPCODE(0xe4);

/*===========================================================================*
 *	GBI Commands for S2DEX microcode
 *===========================================================================*/
/* GBI Header */
constexpr int8_t F3DEX_G_BG_1CYC = OPCODE(0x01);
constexpr int8_t F3DEX_G_BG_COPY = OPCODE(0x02);
constexpr int8_t F3DEX_G_OBJ_RECTANGLE = OPCODE(0x03);
constexpr int8_t F3DEX_G_OBJ_SPRITE = OPCODE(0x04);
constexpr int8_t F3DEX_G_OBJ_MOVEMEM = OPCODE(0x05);
constexpr int8_t F3DEX_G_SELECT_DL = OPCODE(0xb0);
constexpr int8_t F3DEX_G_OBJ_RENDERMODE = OPCODE(0xb1);
constexpr int8_t F3DEX_G_OBJ_RECTANGLE_R = OPCODE(0xb2);
constexpr int8_t F3DEX_G_OBJ_LOADTXTR = OPCODE(0xc1);
constexpr int8_t F3DEX_G_OBJ_LDTX_SPRITE = OPCODE(0xc2);
constexpr int8_t F3DEX_G_OBJ_LDTX_RECT = OPCODE(0xc3);
constexpr int8_t F3DEX_G_OBJ_LDTX_RECT_R = OPCODE(0xc4);
constexpr int8_t F3DEX_G_RDPHALF_0 = OPCODE(0xe4);
