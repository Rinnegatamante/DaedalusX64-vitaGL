#pragma once
#include <stdint.h>
namespace Fast {
#define OPCODE(x) (int8_t)(x)

#include "f3dex.h"
#include "f3dex2.h"

/* RDP commands: */
constexpr int8_t RDP_G_SETCIMG = OPCODE(0xff);         /*  -1 */
constexpr int8_t RDP_G_SETZIMG = OPCODE(0xfe);         /*  -2 */
constexpr int8_t RDP_G_SETTIMG = OPCODE(0xfd);         /*  -3 */
constexpr int8_t RDP_G_SETCOMBINE = OPCODE(0xfc);      /*  -4 */
constexpr int8_t RDP_G_SETENVCOLOR = OPCODE(0xfb);     /*  -5 */
constexpr int8_t RDP_G_SETPRIMCOLOR = OPCODE(0xfa);    /*  -6 */
constexpr int8_t RDP_G_SETBLENDCOLOR = OPCODE(0xf9);   /*  -7 */
constexpr int8_t RDP_G_SETFOGCOLOR = OPCODE(0xf8);     /*  -8 */
constexpr int8_t RDP_G_SETFILLCOLOR = OPCODE(0xf7);    /*  -9 */
constexpr int8_t RDP_G_FILLRECT = OPCODE(0xf6);        /* -10 */
constexpr int8_t RDP_G_SETTILE = OPCODE(0xf5);         /* -11 */
constexpr int8_t RDP_G_LOADTILE = OPCODE(0xf4);        /* -12 */
constexpr int8_t RDP_G_LOADBLOCK = OPCODE(0xf3);       /* -13 */
constexpr int8_t RDP_G_SETTILESIZE = OPCODE(0xf2);     /* -14 */
constexpr int8_t RDP_G_LOADTLUT = OPCODE(0xf0);        /* -16 */
constexpr int8_t RDP_G_RDPSETOTHERMODE = OPCODE(0xef); /* -17 */
constexpr int8_t RDP_G_SETPRIMDEPTH = OPCODE(0xee);    /* -18 */
constexpr int8_t RDP_G_SETSCISSOR = OPCODE(0xed);      /* -19 */
constexpr int8_t RDP_G_SETCONVERT = OPCODE(0xec);      /* -20 */
constexpr int8_t RDP_G_SETKEYR = OPCODE(0xeb);         /* -21 */
constexpr int8_t RDP_G_SETKEYGB = OPCODE(0xea);        /* -22 */
constexpr int8_t RDP_G_RDPFULLSYNC = OPCODE(0xe9);     /* -23 */
constexpr int8_t RDP_G_RDPTILESYNC = OPCODE(0xe8);     /* -24 */
constexpr int8_t RDP_G_RDPPIPESYNC = OPCODE(0xe7);     /* -25 */
constexpr int8_t RDP_G_RDPLOADSYNC = OPCODE(0xe6);     /* -26 */
constexpr int8_t RDP_G_TEXRECTFLIP = OPCODE(0xe5);     /* -27 */
constexpr int8_t RDP_G_TEXRECT = OPCODE(0xe4);         /* -28 */

// CUSTOM OTR COMMANDS
constexpr int8_t OTR_G_SETTIMG_OTR_HASH = OPCODE(0x20);
constexpr int8_t OTR_G_SETFB = OPCODE(0x21);
constexpr int8_t OTR_G_RESETFB = OPCODE(0x22);
constexpr int8_t OTR_G_SETTIMG_FB = OPCODE(0x23);
constexpr int8_t OTR_G_VTX_OTR_FILEPATH = OPCODE(0x24);
constexpr int8_t OTR_G_SETTIMG_OTR_FILEPATH = OPCODE(0x25);
constexpr int8_t OTR_G_TRI1_OTR = OPCODE(0x26);
constexpr int8_t OTR_G_DL_OTR_FILEPATH = OPCODE(0x27);
constexpr int8_t OTR_G_PUSHCD = OPCODE(0x28);
constexpr int8_t OTR_G_MTX_OTR_FILEPATH = OPCODE(0x29);
constexpr int8_t OTR_G_DL_OTR_HASH = OPCODE(0x31);
constexpr int8_t OTR_G_VTX_OTR_HASH = OPCODE(0x32);
constexpr int8_t OTR_G_MARKER = OPCODE(0x33);
constexpr int8_t OTR_G_INVALTEXCACHE = OPCODE(0x34);
constexpr int8_t OTR_G_BRANCH_Z_OTR = OPCODE(0x35);
constexpr int8_t OTR_G_MTX_OTR = OPCODE(0x36);
constexpr int8_t OTR_G_TEXRECT_WIDE = OPCODE(0x37);
constexpr int8_t OTR_G_FILLWIDERECT = OPCODE(0x38);

/* GFX Effects */

// RDP Cmd
constexpr int8_t OTR_G_SETGRAYSCALE = OPCODE(0x39);
constexpr int8_t OTR_G_EXTRAGEOMETRYMODE = OPCODE(0x3a);
constexpr int8_t OTR_G_COPYFB = OPCODE(0x3b);
constexpr int8_t OTR_G_IMAGERECT = OPCODE(0x3c);
constexpr int8_t OTR_G_DL_INDEX = OPCODE(0x3d);
constexpr int8_t OTR_G_READFB = OPCODE(0x3e);
constexpr int8_t OTR_G_REGBLENDEDTEX = OPCODE(0x3f);
constexpr int8_t OTR_G_SETINTENSITY = OPCODE(0x40);
constexpr int8_t OTR_G_MOVEMEM_HASH = OPCODE(0x42);
constexpr int8_t OTR_G_PUSH_SHADER = OPCODE(0x43);
constexpr int8_t OTR_G_POP_SHADER = OPCODE(0x44);
constexpr int8_t RDP_G_SETTILESIZE_INTERP = OPCODE(0x45);
constexpr int8_t RDP_G_SETTARGETINTERPINDEX = OPCODE(0x46);
constexpr int8_t RDP_G_LOADBLOCK_WIDE = OPCODE(0x47);
constexpr int8_t RDP_G_VTX_WIDE = OPCODE(0x48);
constexpr int8_t RDP_G_TRI1_WIDE = OPCODE(0x49);
constexpr int8_t RDP_G_SETTILESIZE_LERP = OPCODE(0x4a);

/*
 * The following commands are the "generated" RDP commands; the user
 * never sees them, the RSP microcode generates them.
 *
 * The layout of the bits is magical, to save work in the ucode.
 * These id's are -56, -52, -54, -50, -55, -51, -53, -49, ...
 *                                 edge, shade, texture, zbuff bits:  estz
 */
#define G_TRI_FILL 0xc8             /* fill triangle:            11001000 */
#define G_TRI_SHADE 0xcc            /* shade triangle:           11001100 */
#define G_TRI_TXTR 0xca             /* texture triangle:         11001010 */
#define G_TRI_SHADE_TXTR 0xce       /* shade, texture triangle:  11001110 */
#define G_TRI_FILL_ZBUFF 0xc9       /* fill, zbuff triangle:     11001001 */
#define G_TRI_SHADE_ZBUFF 0xcd      /* shade, zbuff triangle:    11001101 */
#define G_TRI_TXTR_ZBUFF 0xcb       /* texture, zbuff triangle:  11001011 */
#define G_TRI_SHADE_TXTR_ZBUFF 0xcf /* shade, txtr, zbuff trngl: 11001111 */

/*
 * A TRI_FILL triangle is just the edges. You need to set the DP
 * to use primcolor, in order to see anything. (it is NOT a triangle
 * that gets rendered in 'fill mode'. Triangles can't be rendered
 * in 'fill mode')
 *
 * A TRI_SHADE is a gouraud triangle that has colors interpolated.
 * Flat-shaded triangles (from the software) are still gouraud shaded,
 * it's just the colors are all the same and the deltas are 0.
 *
 * Other triangle types, and combinations are more obvious.
 */

/* masks to build RDP triangle commands: */
#define G_RDP_TRI_FILL_MASK 0x08
#define G_RDP_TRI_SHADE_MASK 0x04
#define G_RDP_TRI_TXTR_MASK 0x02
#define G_RDP_TRI_ZBUFF_MASK 0x01

/*
 * HACK:
 * This is a dreadful hack. For version 1.0 hardware, there are still
 * some 'bowtie' hangs. This parameter can be increased to avoid
 * the hangs. Every increase of 4 chops one scanline off of every
 * triangle. Values of 4,8,12 should be sufficient to avoid any
 * bowtie hang.
 *
 * Change this value, then recompile ALL of your program (including static
 * display lists!)
 *
 * THIS WILL BE REMOVED FOR HARDWARE VERSION 2.0!
 */
#define BOWTIE_VAL 0

/* gets added to RDP command, in order to test for addres fixup: */
#define G_RDP_ADDR_FIXUP 3 /* |RDP cmds| <= this, do addr fixup */
#ifdef _LANGUAGE_ASSEMBLY
#define G_RDP_TEXRECT_CHECK ((-1 * G_TEXRECTFLIP) & 0xff)
#endif

/* macros for command parsing: */
#define GDMACMD(x) (x)
#ifdef GIMMCMD
#undef GIMMCMD
#endif
#define GIMMCMD(x) = OPCODE(G_IMMFIRST - (x))
#define GRDPCMD(x) (0xff - (x))

#define G_DMACMDSIZ 128
#define G_IMMCMDSIZ 64
#define G_RDPCMDSIZ 64

/*
 * Coordinate shift values, number of bits of fraction
 */
#define G_TEXTURE_IMAGE_FRAC 2
#define G_TEXTURE_SCALE_FRAC 16
#define G_SCALE_FRAC 8
#define G_ROTATE_FRAC 16

/*
 * Parameters to graphics commands
 */

/*
 * Data packing macros
 */

/*
 * Maximum z-buffer value, used to initialize the z-buffer.
 * Note : this number is NOT the viewport z-scale constant.
 * See the comment next to G_MAXZ for more info.
 */
#define G_MAXFBZ 0x3fff /* 3b exp, 11b mantissa */

#define GPACK_RGBA5551(r, g, b, a) ((((r) << 8) & 0xf800) | (((g) << 3) & 0x7c0) | (((b) >> 2) & 0x3e) | ((a)&0x1))
#define GPACK_ZDZ(z, dz) ((z) << 2 | (dz))

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

#define G_ZBUFFER 0x00000001
#define G_SHADE 0x00000004
#define G_FOG 0x00010000
#define G_LIGHTING 0x00020000
#define G_TEXTURE_GEN 0x00040000
#define G_TEXTURE_GEN_LINEAR 0x00080000
#define G_LOD 0x00100000
#define G_LIGHTING_POSITIONAL 0x00400000

/*
 * G_EXTRAGEOMETRY flags: set extra custom geometry modes
 */
#define G_EX_INVERT_CULLING 0x00000001
#define G_EX_ALWAYS_EXECUTE_BRANCH 0x00000002

/*
 * G_SETIMG fmt: set image formats
 */
#define G_IM_FMT_RGBA 0
#define G_IM_FMT_YUV 1
#define G_IM_FMT_CI 2
#define G_IM_FMT_IA 3
#define G_IM_FMT_I 4

/*
 * G_SETIMG siz: set image pixel size
 */
#define G_IM_SIZ_4b 0
#define G_IM_SIZ_8b 1
#define G_IM_SIZ_16b 2
#define G_IM_SIZ_32b 3
#define G_IM_SIZ_DD 5

#define G_IM_SIZ_4b_BYTES 0
#define G_IM_SIZ_4b_TILE_BYTES G_IM_SIZ_4b_BYTES
#define G_IM_SIZ_4b_LINE_BYTES G_IM_SIZ_4b_BYTES

#define G_IM_SIZ_8b_BYTES 1
#define G_IM_SIZ_8b_TILE_BYTES G_IM_SIZ_8b_BYTES
#define G_IM_SIZ_8b_LINE_BYTES G_IM_SIZ_8b_BYTES

#define G_IM_SIZ_16b_BYTES 2
#define G_IM_SIZ_16b_TILE_BYTES G_IM_SIZ_16b_BYTES
#define G_IM_SIZ_16b_LINE_BYTES G_IM_SIZ_16b_BYTES

#define G_IM_SIZ_32b_BYTES 4
#define G_IM_SIZ_32b_TILE_BYTES 2
#define G_IM_SIZ_32b_LINE_BYTES 2

#define G_IM_SIZ_4b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_8b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_16b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_32b_LOAD_BLOCK G_IM_SIZ_32b

#define G_IM_SIZ_4b_SHIFT 2
#define G_IM_SIZ_8b_SHIFT 1
#define G_IM_SIZ_16b_SHIFT 0
#define G_IM_SIZ_32b_SHIFT 0

#define G_IM_SIZ_4b_INCR 3
#define G_IM_SIZ_8b_INCR 1
#define G_IM_SIZ_16b_INCR 0
#define G_IM_SIZ_32b_INCR 0

/*
 * Texturing macros
 */

/* These are also defined defined above for Sprite Microcode */

#define G_TX_LOADTILE 7
#define G_TX_RENDERTILE 0

#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0

/*
 * G_SETCOMBINE: color combine modes
 */
/* Color combiner constants: */
#define G_CCMUX_COMBINED 0
#define G_CCMUX_TEXEL0 1
#define G_CCMUX_TEXEL1 2
#define G_CCMUX_PRIMITIVE 3
#define G_CCMUX_SHADE 4
#define G_CCMUX_ENVIRONMENT 5
#define G_CCMUX_CENTER 6
#define G_CCMUX_SCALE 6
#define G_CCMUX_COMBINED_ALPHA 7
#define G_CCMUX_TEXEL0_ALPHA 8
#define G_CCMUX_TEXEL1_ALPHA 9
#define G_CCMUX_PRIMITIVE_ALPHA 10
#define G_CCMUX_SHADE_ALPHA 11
#define G_CCMUX_ENV_ALPHA 12
#define G_CCMUX_LOD_FRACTION 13
#define G_CCMUX_PRIM_LOD_FRAC 14
#define G_CCMUX_NOISE 7
#define G_CCMUX_K4 7
#define G_CCMUX_K5 15
#define G_CCMUX_1 6
#define G_CCMUX_0 31
/* Chroma-key center/scale and YUV-convert K4/K5 combiner inputs. */
#define G_CCMUX_KEY_CENTER 20
#define G_CCMUX_KEY_SCALE 21
#define G_CCMUX_CONVERT_K4 22
#define G_CCMUX_CONVERT_K5 23

/* Alpha combiner constants: */
#define G_ACMUX_COMBINED 0
#define G_ACMUX_TEXEL0 1
#define G_ACMUX_TEXEL1 2
#define G_ACMUX_PRIMITIVE 3
#define G_ACMUX_SHADE 4
#define G_ACMUX_ENVIRONMENT 5
#define G_ACMUX_LOD_FRACTION 0
#define G_ACMUX_PRIM_LOD_FRAC 6
#define G_ACMUX_1 6
#define G_ACMUX_0 7

/* typical CC cycle 1 modes */
#define G_CC_PRIMITIVE 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE
#define G_CC_SHADE 0, 0, 0, SHADE, 0, 0, 0, SHADE
#define G_CC_MODULATEI TEXEL0, 0, SHADE, 0, 0, 0, 0, SHADE
#define G_CC_MODULATEIA TEXEL0, 0, SHADE, 0, TEXEL0, 0, SHADE, 0
#define G_CC_MODULATEIDECALA TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0
#define G_CC_MODULATERGB G_CC_MODULATEI
#define G_CC_MODULATERGBA G_CC_MODULATEIA
#define G_CC_MODULATERGBDECALA G_CC_MODULATEIDECALA
#define G_CC_MODULATEI_PRIM TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE
#define G_CC_MODULATEIA_PRIM TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0
#define G_CC_MODULATEIDECALA_PRIM TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0
#define G_CC_MODULATERGB_PRIM G_CC_MODULATEI_PRIM
#define G_CC_MODULATERGBA_PRIM G_CC_MODULATEIA_PRIM
#define G_CC_MODULATERGBDECALA_PRIM G_CC_MODULATEIDECALA_PRIM
#define G_CC_DECALRGB 0, 0, 0, TEXEL0, 0, 0, 0, SHADE
#define G_CC_DECALRGBA 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0
#define G_CC_BLENDI ENVIRONMENT, SHADE, TEXEL0, SHADE, 0, 0, 0, SHADE
#define G_CC_BLENDIA ENVIRONMENT, SHADE, TEXEL0, SHADE, TEXEL0, 0, SHADE, 0
#define G_CC_BLENDIDECALA ENVIRONMENT, SHADE, TEXEL0, SHADE, 0, 0, 0, TEXEL0
#define G_CC_BLENDRGBA TEXEL0, SHADE, TEXEL0_ALPHA, SHADE, 0, 0, 0, SHADE
#define G_CC_BLENDRGBDECALA TEXEL0, SHADE, TEXEL0_ALPHA, SHADE, 0, 0, 0, TEXEL0
#define G_CC_ADDRGB 1, 0, TEXEL0, SHADE, 0, 0, 0, SHADE
#define G_CC_ADDRGBDECALA 1, 0, TEXEL0, SHADE, 0, 0, 0, TEXEL0
#define G_CC_REFLECTRGB ENVIRONMENT, 0, TEXEL0, SHADE, 0, 0, 0, SHADE
#define G_CC_REFLECTRGBDECALA ENVIRONMENT, 0, TEXEL0, SHADE, 0, 0, 0, TEXEL0
#define G_CC_HILITERGB PRIMITIVE, SHADE, TEXEL0, SHADE, 0, 0, 0, SHADE
#define G_CC_HILITERGBA PRIMITIVE, SHADE, TEXEL0, SHADE, PRIMITIVE, SHADE, TEXEL0, SHADE
#define G_CC_HILITERGBDECALA PRIMITIVE, SHADE, TEXEL0, SHADE, 0, 0, 0, TEXEL0
#define G_CC_SHADEDECALA 0, 0, 0, SHADE, 0, 0, 0, TEXEL0
#define G_CC_BLENDPE PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, SHADE, 0
#define G_CC_BLENDPEDECALA PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, TEXEL0

/* oddball modes */
#define _G_CC_BLENDPE ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, TEXEL0, 0, SHADE, 0
#define _G_CC_BLENDPEDECALA ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, 0, 0, 0, TEXEL0
#define _G_CC_TWOCOLORTEX PRIMITIVE, SHADE, TEXEL0, SHADE, 0, 0, 0, SHADE
/* used for 1-cycle sparse mip-maps, primitive color has color of lowest LOD */
#define _G_CC_SPARSEST PRIMITIVE, TEXEL0, LOD_FRACTION, TEXEL0, PRIMITIVE, TEXEL0, LOD_FRACTION, TEXEL0
#define G_CC_TEMPLERP TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0, TEXEL1, TEXEL0, PRIM_LOD_FRAC, TEXEL0

/* typical CC cycle 1 modes, usually followed by other cycle 2 modes */
#define G_CC_TRILERP TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0, TEXEL1, TEXEL0, LOD_FRACTION, TEXEL0
#define G_CC_INTERFERENCE TEXEL0, 0, TEXEL1, 0, TEXEL0, 0, TEXEL1, 0

/*
 *  One-cycle color convert operation
 */
#define G_CC_1CYUV2RGB TEXEL0, K4, K5, TEXEL0, 0, 0, 0, SHADE

/*
 *  NOTE: YUV2RGB expects TF step1 color conversion to occur in 2nd clock.
 * Therefore, CC looks for step1 results in TEXEL1
 */
#define G_CC_YUV2RGB TEXEL1, K4, K5, TEXEL1, 0, 0, 0, 0

/* typical CC cycle 2 modes */
#define G_CC_PASS2 0, 0, 0, COMBINED, 0, 0, 0, COMBINED
#define G_CC_MODULATEI2 COMBINED, 0, SHADE, 0, 0, 0, 0, SHADE
#define G_CC_MODULATEIA2 COMBINED, 0, SHADE, 0, COMBINED, 0, SHADE, 0
#define G_CC_MODULATERGB2 G_CC_MODULATEI2
#define G_CC_MODULATERGBA2 G_CC_MODULATEIA2
#define G_CC_MODULATEI_PRIM2 COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE
#define G_CC_MODULATEIA_PRIM2 COMBINED, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0
#define G_CC_MODULATERGB_PRIM2 G_CC_MODULATEI_PRIM2
#define G_CC_MODULATERGBA_PRIM2 G_CC_MODULATEIA_PRIM2
#define G_CC_DECALRGB2 0, 0, 0, COMBINED, 0, 0, 0, SHADE
/*
 * ?
#define G_CC_DECALRGBA2     COMBINED, SHADE, COMBINED_ALPHA, SHADE, 0, 0, 0, SHADE
*/
#define G_CC_BLENDI2 ENVIRONMENT, SHADE, COMBINED, SHADE, 0, 0, 0, SHADE
#define G_CC_BLENDIA2 ENVIRONMENT, SHADE, COMBINED, SHADE, COMBINED, 0, SHADE, 0
#define G_CC_CHROMA_KEY2 TEXEL0, CENTER, SCALE, 0, 0, 0, 0, 0
#define G_CC_HILITERGB2 ENVIRONMENT, COMBINED, TEXEL0, COMBINED, 0, 0, 0, SHADE
#define G_CC_HILITERGBA2 ENVIRONMENT, COMBINED, TEXEL0, COMBINED, ENVIRONMENT, COMBINED, TEXEL0, COMBINED
#define G_CC_HILITERGBDECALA2 ENVIRONMENT, COMBINED, TEXEL0, COMBINED, 0, 0, 0, TEXEL0
#define G_CC_HILITERGBPASSA2 ENVIRONMENT, COMBINED, TEXEL0, COMBINED, 0, 0, 0, COMBINED

/*
 * G_SETOTHERMODE_L sft: shift count
 */
#define G_MDSFT_ALPHACOMPARE 0
#define G_MDSFT_ZSRCSEL 2
#define G_MDSFT_RENDERMODE 3
#define G_MDSFT_BLENDER 16

/*
 * G_SETOTHERMODE_H sft: shift count
 */
#define G_MDSFT_BLENDMASK 0 /* unsupported */
#define G_MDSFT_ALPHADITHER 4
#define G_MDSFT_RGBDITHER 6

#define G_MDSFT_COMBKEY 8
#define G_MDSFT_TEXTCONV 9
#define G_MDSFT_TEXTFILT 12
#define G_MDSFT_TEXTLUT 14
#define G_MDSFT_TEXTLOD 16
#define G_MDSFT_TEXTDETAIL 17
#define G_MDSFT_TEXTPERSP 19
#define G_MDSFT_CYCLETYPE 20
#define G_MDSFT_COLORDITHER 22 /* unsupported in HW 2.0 */
#define G_MDSFT_PIPELINE 23

/* G_SETOTHERMODE_H gPipelineMode */
#define G_PM_1PRIMITIVE (1 << G_MDSFT_PIPELINE)
#define G_PM_NPRIMITIVE (0 << G_MDSFT_PIPELINE)

/* G_SETOTHERMODE_H gSetCycleType */
#define G_CYC_1CYCLE (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE (1 << G_MDSFT_CYCLETYPE)
#define G_CYC_COPY (2 << G_MDSFT_CYCLETYPE)
#define G_CYC_FILL (3 << G_MDSFT_CYCLETYPE)

/* G_SETOTHERMODE_H gSetTexturePersp */
#define G_TP_NONE (0 << G_MDSFT_TEXTPERSP)
#define G_TP_PERSP (1 << G_MDSFT_TEXTPERSP)

/* G_SETOTHERMODE_H gSetTextureDetail */
#define G_TD_CLAMP (0 << G_MDSFT_TEXTDETAIL)
#define G_TD_SHARPEN (1 << G_MDSFT_TEXTDETAIL)
#define G_TD_DETAIL (2 << G_MDSFT_TEXTDETAIL)

/* G_SETOTHERMODE_H gSetTextureLOD */
#define G_TL_TILE (0 << G_MDSFT_TEXTLOD)
#define G_TL_LOD (1 << G_MDSFT_TEXTLOD)

/* G_SETOTHERMODE_H gSetTextureLUT */
#define G_TT_NONE (0 << G_MDSFT_TEXTLUT)
#define G_TT_RGBA16 (2 << G_MDSFT_TEXTLUT)
#define G_TT_IA16 (3 << G_MDSFT_TEXTLUT)

/* G_SETOTHERMODE_H gSetTextureFilter */
#define G_TF_POINT (0 << G_MDSFT_TEXTFILT)
#define G_TF_AVERAGE (3 << G_MDSFT_TEXTFILT)
#define G_TF_BILERP (2 << G_MDSFT_TEXTFILT)

/* G_SETOTHERMODE_H gSetTextureConvert */
#define G_TC_CONV (0 << G_MDSFT_TEXTCONV)
#define G_TC_FILTCONV (5 << G_MDSFT_TEXTCONV)
#define G_TC_FILT (6 << G_MDSFT_TEXTCONV)

/* G_SETOTHERMODE_H gSetCombineKey */
#define G_CK_NONE (0 << G_MDSFT_COMBKEY)
#define G_CK_KEY (1 << G_MDSFT_COMBKEY)

/* G_SETOTHERMODE_H gSetColorDither */
#define G_CD_MAGICSQ (0 << G_MDSFT_RGBDITHER)
#define G_CD_BAYER (1 << G_MDSFT_RGBDITHER)
#define G_CD_NOISE (2 << G_MDSFT_RGBDITHER)

#ifndef _HW_VERSION_1
#define G_CD_DISABLE (3 << G_MDSFT_RGBDITHER)
#define G_CD_ENABLE G_CD_NOISE /* HW 1.0 compatibility mode */
#else
#define G_CD_ENABLE (1 << G_MDSFT_COLORDITHER)
#define G_CD_DISABLE (0 << G_MDSFT_COLORDITHER)
#endif

/* G_SETOTHERMODE_H gSetAlphaDither */
#define G_AD_PATTERN (0 << G_MDSFT_ALPHADITHER)
#define G_AD_NOTPATTERN (1 << G_MDSFT_ALPHADITHER)
#define G_AD_NOISE (2 << G_MDSFT_ALPHADITHER)
#define G_AD_DISABLE (3 << G_MDSFT_ALPHADITHER)

/* G_SETOTHERMODE_L gSetAlphaCompare */
#define G_AC_NONE (0 << G_MDSFT_ALPHACOMPARE)
#define G_AC_THRESHOLD (1 << G_MDSFT_ALPHACOMPARE)
#define G_AC_DITHER (3 << G_MDSFT_ALPHACOMPARE)

/* G_SETOTHERMODE_L gSetDepthSource */
#define G_ZS_PIXEL (0 << G_MDSFT_ZSRCSEL)
#define G_ZS_PRIM (1 << G_MDSFT_ZSRCSEL)

/* G_SETOTHERMODE_L gSetRenderMode */
#define AA_EN 0x8
#define Z_CMP 0x10
#define Z_UPD 0x20
#define IM_RD 0x40
#define CLR_ON_CVG 0x80
#define CVG_DST_CLAMP 0
#define CVG_DST_WRAP 0x100
#define CVG_DST_FULL 0x200
#define CVG_DST_SAVE 0x300
#define ZMODE_OPA 0
#define ZMODE_INTER 0x400
#define ZMODE_XLU 0x800
#define ZMODE_DEC 0xc00
#define CVG_X_ALPHA 0x1000
#define ALPHA_CVG_SEL 0x2000
#define FORCE_BL 0x4000
#define TEX_EDGE 0x0000 /* used to be 0x8000 */

#define G_BL_CLR_IN 0
#define G_BL_CLR_MEM 1
#define G_BL_CLR_BL 2
#define G_BL_CLR_FOG 3
#define G_BL_1MA 0
#define G_BL_A_MEM 1
#define G_BL_A_IN 0
#define G_BL_A_FOG 1
#define G_BL_A_SHADE 2
#define G_BL_1 2
#define G_BL_0 3

#define GBL_c1(m1a, m1b, m2a, m2b) (uint32_t)(m1a) << 30 | (m1b) << 26 | (m2a) << 22 | (m2b) << 18
#define GBL_c2(m1a, m1b, m2a, m2b) (m1a) << 28 | (m1b) << 24 | (m2a) << 20 | (m2b) << 16

#define RM_AA_ZB_OPA_SURF(clk)                                                  \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_RA_ZB_OPA_SURF(clk)                                          \
    AA_EN | Z_CMP | Z_UPD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_ZB_XLU_SURF(clk)                                                 \
    AA_EN | Z_CMP | IM_RD | CVG_DST_WRAP | CLR_ON_CVG | FORCE_BL | ZMODE_XLU | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_OPA_DECAL(clk)                                        \
    AA_EN | Z_CMP | IM_RD | CVG_DST_WRAP | ALPHA_CVG_SEL | ZMODE_DEC | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_RA_ZB_OPA_DECAL(clk)                                \
    AA_EN | Z_CMP | CVG_DST_WRAP | ALPHA_CVG_SEL | ZMODE_DEC | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_ZB_XLU_DECAL(clk)                                                \
    AA_EN | Z_CMP | IM_RD | CVG_DST_WRAP | CLR_ON_CVG | FORCE_BL | ZMODE_DEC | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_OPA_INTER(clk)                                                   \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ALPHA_CVG_SEL | ZMODE_INTER | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_RA_ZB_OPA_INTER(clk)                                           \
    AA_EN | Z_CMP | Z_UPD | CVG_DST_CLAMP | ALPHA_CVG_SEL | ZMODE_INTER | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_ZB_XLU_INTER(clk)                                                  \
    AA_EN | Z_CMP | IM_RD | CVG_DST_WRAP | CLR_ON_CVG | FORCE_BL | ZMODE_INTER | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_XLU_LINE(clk)                                                                   \
    AA_EN | Z_CMP | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | ZMODE_XLU | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_DEC_LINE(clk)                                                                  \
    AA_EN | Z_CMP | IM_RD | CVG_DST_SAVE | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | ZMODE_DEC | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_TEX_EDGE(clk)                                                                           \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_OPA | TEX_EDGE | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_ZB_TEX_INTER(clk)                                                                            \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_INTER | TEX_EDGE | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_ZB_SUB_SURF(clk)                                                 \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_ZB_PCL_SURF(clk)                                                \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | G_AC_DITHER | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_OPA_TERR(clk)                                                  \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_TEX_TERR(clk)                                                                           \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_OPA | TEX_EDGE | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_ZB_SUB_TERR(clk)                                                 \
    AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_OPA_SURF(clk)                                     \
    AA_EN | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_RA_OPA_SURF(clk) \
    AA_EN | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_XLU_SURF(clk)                                            \
    AA_EN | IM_RD | CVG_DST_WRAP | CLR_ON_CVG | FORCE_BL | ZMODE_OPA | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_XLU_LINE(clk)                                                              \
    AA_EN | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | ZMODE_OPA | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_DEC_LINE(clk)                                                             \
    AA_EN | IM_RD | CVG_DST_FULL | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | ZMODE_OPA | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_TEX_EDGE(clk)                                                              \
    AA_EN | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_OPA | TEX_EDGE | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_SUB_SURF(clk)                                    \
    AA_EN | IM_RD | CVG_DST_FULL | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_AA_PCL_SURF(clk) \
    AA_EN | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | G_AC_DITHER | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_OPA_TERR(clk)                                     \
    AA_EN | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_TEX_TERR(clk)                                                              \
    AA_EN | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | ZMODE_OPA | TEX_EDGE | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_AA_SUB_TERR(clk)                                    \
    AA_EN | IM_RD | CVG_DST_FULL | ZMODE_OPA | ALPHA_CVG_SEL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_ZB_OPA_SURF(clk)                                    \
    Z_CMP | Z_UPD | CVG_DST_FULL | ALPHA_CVG_SEL | ZMODE_OPA | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_ZB_XLU_SURF(clk) \
    Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_XLU | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_ZB_OPA_DECAL(clk) \
    Z_CMP | CVG_DST_FULL | ALPHA_CVG_SEL | ZMODE_DEC | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)

#define RM_ZB_XLU_DECAL(clk) \
    Z_CMP | IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_DEC | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_ZB_CLD_SURF(clk) \
    Z_CMP | IM_RD | CVG_DST_SAVE | FORCE_BL | ZMODE_XLU | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_ZB_OVL_SURF(clk) \
    Z_CMP | IM_RD | CVG_DST_SAVE | FORCE_BL | ZMODE_DEC | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_ZB_PCL_SURF(clk) \
    Z_CMP | Z_UPD | CVG_DST_FULL | ZMODE_OPA | G_AC_DITHER | GBL_c##clk(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

#define RM_OPA_SURF(clk) CVG_DST_CLAMP | FORCE_BL | ZMODE_OPA | GBL_c##clk(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

#define RM_XLU_SURF(clk) \
    IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_OPA | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_TEX_EDGE(clk)                                                                    \
    CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | ZMODE_OPA | TEX_EDGE | AA_EN | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

#define RM_CLD_SURF(clk) \
    IM_RD | CVG_DST_SAVE | FORCE_BL | ZMODE_OPA | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)

#define RM_PCL_SURF(clk) \
    CVG_DST_FULL | FORCE_BL | ZMODE_OPA | G_AC_DITHER | GBL_c##clk(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

#define RM_ADD(clk) \
    IM_RD | CVG_DST_SAVE | FORCE_BL | ZMODE_OPA | GBL_c##clk(G_BL_CLR_IN, G_BL_A_FOG, G_BL_CLR_MEM, G_BL_1)

#define RM_NOOP(clk) GBL_c##clk(0, 0, 0, 0)

#define RM_VISCVG(clk) IM_RD | FORCE_BL | GBL_c##clk(G_BL_CLR_IN, G_BL_0, G_BL_CLR_BL, G_BL_A_MEM)

/* for rendering to an 8-bit framebuffer */
#define RM_OPA_CI(clk) CVG_DST_CLAMP | ZMODE_OPA | GBL_c##clk(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

#define G_RM_AA_ZB_OPA_SURF RM_AA_ZB_OPA_SURF(1)
#define G_RM_AA_ZB_OPA_SURF2 RM_AA_ZB_OPA_SURF(2)
#define G_RM_AA_ZB_XLU_SURF RM_AA_ZB_XLU_SURF(1)
#define G_RM_AA_ZB_XLU_SURF2 RM_AA_ZB_XLU_SURF(2)
#define G_RM_AA_ZB_OPA_DECAL RM_AA_ZB_OPA_DECAL(1)
#define G_RM_AA_ZB_OPA_DECAL2 RM_AA_ZB_OPA_DECAL(2)
#define G_RM_AA_ZB_XLU_DECAL RM_AA_ZB_XLU_DECAL(1)
#define G_RM_AA_ZB_XLU_DECAL2 RM_AA_ZB_XLU_DECAL(2)
#define G_RM_AA_ZB_OPA_INTER RM_AA_ZB_OPA_INTER(1)
#define G_RM_AA_ZB_OPA_INTER2 RM_AA_ZB_OPA_INTER(2)
#define G_RM_AA_ZB_XLU_INTER RM_AA_ZB_XLU_INTER(1)
#define G_RM_AA_ZB_XLU_INTER2 RM_AA_ZB_XLU_INTER(2)
#define G_RM_AA_ZB_XLU_LINE RM_AA_ZB_XLU_LINE(1)
#define G_RM_AA_ZB_XLU_LINE2 RM_AA_ZB_XLU_LINE(2)
#define G_RM_AA_ZB_DEC_LINE RM_AA_ZB_DEC_LINE(1)
#define G_RM_AA_ZB_DEC_LINE2 RM_AA_ZB_DEC_LINE(2)
#define G_RM_AA_ZB_TEX_EDGE RM_AA_ZB_TEX_EDGE(1)
#define G_RM_AA_ZB_TEX_EDGE2 RM_AA_ZB_TEX_EDGE(2)
#define G_RM_AA_ZB_TEX_INTER RM_AA_ZB_TEX_INTER(1)
#define G_RM_AA_ZB_TEX_INTER2 RM_AA_ZB_TEX_INTER(2)
#define G_RM_AA_ZB_SUB_SURF RM_AA_ZB_SUB_SURF(1)
#define G_RM_AA_ZB_SUB_SURF2 RM_AA_ZB_SUB_SURF(2)
#define G_RM_AA_ZB_PCL_SURF RM_AA_ZB_PCL_SURF(1)
#define G_RM_AA_ZB_PCL_SURF2 RM_AA_ZB_PCL_SURF(2)
#define G_RM_AA_ZB_OPA_TERR RM_AA_ZB_OPA_TERR(1)
#define G_RM_AA_ZB_OPA_TERR2 RM_AA_ZB_OPA_TERR(2)
#define G_RM_AA_ZB_TEX_TERR RM_AA_ZB_TEX_TERR(1)
#define G_RM_AA_ZB_TEX_TERR2 RM_AA_ZB_TEX_TERR(2)
#define G_RM_AA_ZB_SUB_TERR RM_AA_ZB_SUB_TERR(1)
#define G_RM_AA_ZB_SUB_TERR2 RM_AA_ZB_SUB_TERR(2)

#define G_RM_RA_ZB_OPA_SURF RM_RA_ZB_OPA_SURF(1)
#define G_RM_RA_ZB_OPA_SURF2 RM_RA_ZB_OPA_SURF(2)
#define G_RM_RA_ZB_OPA_DECAL RM_RA_ZB_OPA_DECAL(1)
#define G_RM_RA_ZB_OPA_DECAL2 RM_RA_ZB_OPA_DECAL(2)
#define G_RM_RA_ZB_OPA_INTER RM_RA_ZB_OPA_INTER(1)
#define G_RM_RA_ZB_OPA_INTER2 RM_RA_ZB_OPA_INTER(2)

#define G_RM_AA_OPA_SURF RM_AA_OPA_SURF(1)
#define G_RM_AA_OPA_SURF2 RM_AA_OPA_SURF(2)
#define G_RM_AA_XLU_SURF RM_AA_XLU_SURF(1)
#define G_RM_AA_XLU_SURF2 RM_AA_XLU_SURF(2)
#define G_RM_AA_XLU_LINE RM_AA_XLU_LINE(1)
#define G_RM_AA_XLU_LINE2 RM_AA_XLU_LINE(2)
#define G_RM_AA_DEC_LINE RM_AA_DEC_LINE(1)
#define G_RM_AA_DEC_LINE2 RM_AA_DEC_LINE(2)
#define G_RM_AA_TEX_EDGE RM_AA_TEX_EDGE(1)
#define G_RM_AA_TEX_EDGE2 RM_AA_TEX_EDGE(2)
#define G_RM_AA_SUB_SURF RM_AA_SUB_SURF(1)
#define G_RM_AA_SUB_SURF2 RM_AA_SUB_SURF(2)
#define G_RM_AA_PCL_SURF RM_AA_PCL_SURF(1)
#define G_RM_AA_PCL_SURF2 RM_AA_PCL_SURF(2)
#define G_RM_AA_OPA_TERR RM_AA_OPA_TERR(1)
#define G_RM_AA_OPA_TERR2 RM_AA_OPA_TERR(2)
#define G_RM_AA_TEX_TERR RM_AA_TEX_TERR(1)
#define G_RM_AA_TEX_TERR2 RM_AA_TEX_TERR(2)
#define G_RM_AA_SUB_TERR RM_AA_SUB_TERR(1)
#define G_RM_AA_SUB_TERR2 RM_AA_SUB_TERR(2)

#define G_RM_RA_OPA_SURF RM_RA_OPA_SURF(1)
#define G_RM_RA_OPA_SURF2 RM_RA_OPA_SURF(2)

#define G_RM_ZB_OPA_SURF RM_ZB_OPA_SURF(1)
#define G_RM_ZB_OPA_SURF2 RM_ZB_OPA_SURF(2)
#define G_RM_ZB_XLU_SURF RM_ZB_XLU_SURF(1)
#define G_RM_ZB_XLU_SURF2 RM_ZB_XLU_SURF(2)
#define G_RM_ZB_OPA_DECAL RM_ZB_OPA_DECAL(1)
#define G_RM_ZB_OPA_DECAL2 RM_ZB_OPA_DECAL(2)
#define G_RM_ZB_XLU_DECAL RM_ZB_XLU_DECAL(1)
#define G_RM_ZB_XLU_DECAL2 RM_ZB_XLU_DECAL(2)
#define G_RM_ZB_CLD_SURF RM_ZB_CLD_SURF(1)
#define G_RM_ZB_CLD_SURF2 RM_ZB_CLD_SURF(2)
#define G_RM_ZB_OVL_SURF RM_ZB_OVL_SURF(1)
#define G_RM_ZB_OVL_SURF2 RM_ZB_OVL_SURF(2)
#define G_RM_ZB_PCL_SURF RM_ZB_PCL_SURF(1)
#define G_RM_ZB_PCL_SURF2 RM_ZB_PCL_SURF(2)

#define G_RM_OPA_SURF RM_OPA_SURF(1)
#define G_RM_OPA_SURF2 RM_OPA_SURF(2)
#define G_RM_XLU_SURF RM_XLU_SURF(1)
#define G_RM_XLU_SURF2 RM_XLU_SURF(2)
#define G_RM_CLD_SURF RM_CLD_SURF(1)
#define G_RM_CLD_SURF2 RM_CLD_SURF(2)
#define G_RM_TEX_EDGE RM_TEX_EDGE(1)
#define G_RM_TEX_EDGE2 RM_TEX_EDGE(2)
#define G_RM_PCL_SURF RM_PCL_SURF(1)
#define G_RM_PCL_SURF2 RM_PCL_SURF(2)
#define G_RM_ADD RM_ADD(1)
#define G_RM_ADD2 RM_ADD(2)
#define G_RM_NOOP RM_NOOP(1)
#define G_RM_NOOP2 RM_NOOP(2)
#define G_RM_VISCVG RM_VISCVG(1)
#define G_RM_VISCVG2 RM_VISCVG(2)
#define G_RM_OPA_CI RM_OPA_CI(1)
#define G_RM_OPA_CI2 RM_OPA_CI(2)

#define G_RM_FOG_SHADE_A GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA)
#define G_RM_FOG_PRIM_A GBL_c1(G_BL_CLR_FOG, G_BL_A_FOG, G_BL_CLR_IN, G_BL_1MA)
#define G_RM_PASS GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)

/*
 * G_SETCONVERT: K0-5
 */
#define G_CV_K0 175
#define G_CV_K1 -43
#define G_CV_K2 -89
#define G_CV_K3 222
#define G_CV_K4 114
#define G_CV_K5 42

/*
 * G_SETSCISSOR: interlace mode
 */
#define G_SC_NON_INTERLACE 0
#define G_SC_ODD_INTERLACE 3
#define G_SC_EVEN_INTERLACE 2

/* flags to inhibit pushing of the display list (on branch) */
#define G_DL_PUSH 0x00
#define G_DL_NOPUSH 0x01

#if defined(_MSC_VER) || defined(__GNUC__)
#define _LANGUAGE_C
#endif

/*
 * BEGIN C-specific section: (typedef's)
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*
 * Data Structures
 *
 * NOTE:
 * The DMA transfer hardware requires 64-bit aligned, 64-bit multiple-
 * sized transfers. This important hardware optimization is unfortunately
 * reflected in the programming interface, with some structures
 * padded and alignment enforced.
 *
 * Since structures are aligned to the boundary of the "worst-case"
 * element, we can't depend on the C compiler to align things
 * properly.
 *
 * 64-bit structure alignment is enforced by wrapping structures with
 * unions that contain a dummy "long long int".  Why this works is
 * explained in the ANSI C Spec, or on page 186 of the second edition
 * of K&R, "The C Programming Language".
 *
 * The price we pay for this is a little awkwardness referencing the
 * structures through the union. There is no memory penalty, since
 * all the structures are at least 64-bits the dummy alignment field
 * does not increase the size of the union.
 *
 * Static initialization of these union structures works because
 * the ANSI C spec states that static initialization for unions
 * works by using the first union element. We put the dummy alignment
 * field last for this reason.
 *
 * (it's possible a newer 64-bit compiler from MIPS might make this
 * easier with a flag, but we can't wait for it...)
 *
 */

/*
 * Vertex (set up for use with colors)
 */
typedef struct {
#ifndef GBI_FLOATS
    short ob[3]; /* x, y, z */
#else
    float ob[3]; /* x, y, z */
#endif
    unsigned short flag;
    short tc[2];         /* texture coord */
    unsigned char cn[4]; /* color & alpha */
} F3DVtx_t;

/*
 * Vertex (set up for use with normals)
 */
typedef struct {
#ifndef GBI_FLOATS
    short ob[3]; /* x, y, z */
#else
    float ob[3]; /* x, y, z */
#endif
    unsigned short flag;
    short tc[2];      /* texture coord */
    signed char n[3]; /* normal */
    unsigned char a;  /* alpha  */
} F3DVtx_tn;

typedef union F3DVtx {
    F3DVtx_t v;  /* Use this one for colors  */
    F3DVtx_tn n; /* Use this one for normals */
    long long int force_structure_alignment;
} F3DVtx;

/*
 * Sprite structure
 */

typedef struct F3DuSprite_t {
    void* SourceImagePointer;
    void* TlutPointer;
    short Stride;
    short SubImageWidth;
    short SubImageHeight;
    char SourceImageType;
    char SourceImageBitSize;
    short SourceImageOffsetS;
    short SourceImageOffsetT;
    /* 20 bytes for above */

    /* padding to bring structure size to 64 bit allignment */
    char dummy[4];

} F3DuSprite_t;

typedef union {
    F3DuSprite_t s;

    /* Need to make sure this is 64 bit aligned */
    long long int force_structure_allignment[3];
} F3DuSprite;

/*
 * Triangle face
 */
typedef struct {
    unsigned char flag;
    unsigned char v[3];
} F3DTri;

/*
 * Viewport
 */

/*
 *
 * This magic value is the maximum INTEGER z-range of the hardware
 * (there are also 16-bits of fraction, which are introduced during
 * any transformations). This is not just a good idea, it's the law.
 * Feeding the hardware eventual z-coordinates (after any transforms
 * or scaling) bigger than this, will not work.
 *
 * This number is DIFFERENT than G_MAXFBZ, which is the maximum value
 * you want to use to initialize the z-buffer.
 *
 * The reason these are different is mildly interesting, but too long
 * to explain here. It is basically the result of optimizations in the
 * hardware. A more generic API might hide this detail from the users,
 * but we don't have the ucode to do that...
 *
 */
#define G_MAXZ 0x03ff /* 10 bits of integer screen-Z precision */

/*
 * The viewport structure elements have 2 bits of fraction, necessary
 * to accomodate the sub-pixel positioning scaling for the hardware.
 * This can also be exploited to handle odd-sized viewports.
 *
 * Accounting for these fractional bits, using the default projection
 * and viewing matrices, the viewport structure is initialized thusly:
 *
 *      (SCREEN_WD/2)*4, (SCREEN_HT/2)*4, G_MAXZ, 0,
 *      (SCREEN_WD/2)*4, (SCREEN_HT/2)*4, 0, 0,
 */
typedef struct {
    short vscale[4]; /* scale, 2 bits fraction */
    short vtrans[4]; /* translate, 2 bits fraction */
    /* both the above arrays are padded to 64-bit boundary */
} F3DVp_t;

typedef union {
    F3DVp_t vp;
    long long int force_structure_alignment;
} F3DVp;

/*
 * MOVEWORD indices
 *
 * Each of these indexes an entry in a dmem table
 * which points to a word in dmem in dmem where
 * an immediate word will be stored.
 *
 */
#define G_MW_MATRIX 0x00 /* NOTE: also used by movemem */
#define G_MW_NUMLIGHT 0x02
#define G_MW_CLIP 0x04
#define G_MW_SEGMENT 0x06
#define G_MW_SEGMENT_INTERP 0x07
#define G_MW_FOG 0x08
#define G_MW_LIGHTCOL 0x0a
#define G_MW_PERSPNORM 0x0e

/*
 * These are offsets from the address in the dmem table
 */
#define G_MWO_NUMLIGHT 0x00
#define G_MWO_CLIP_RNX 0x04
#define G_MWO_CLIP_RNY 0x0c
#define G_MWO_CLIP_RPX 0x14
#define G_MWO_CLIP_RPY 0x1c
#define G_MWO_SEGMENT_0 0x00
#define G_MWO_SEGMENT_1 0x01
#define G_MWO_SEGMENT_2 0x02
#define G_MWO_SEGMENT_3 0x03
#define G_MWO_SEGMENT_4 0x04
#define G_MWO_SEGMENT_5 0x05
#define G_MWO_SEGMENT_6 0x06
#define G_MWO_SEGMENT_7 0x07
#define G_MWO_SEGMENT_8 0x08
#define G_MWO_SEGMENT_9 0x09
#define G_MWO_SEGMENT_A 0x0a
#define G_MWO_SEGMENT_B 0x0b
#define G_MWO_SEGMENT_C 0x0c
#define G_MWO_SEGMENT_D 0x0d
#define G_MWO_SEGMENT_E 0x0e
#define G_MWO_SEGMENT_F 0x0f
#define G_MWO_FOG 0x00
#define G_MWO_aLIGHT_1 0x00
#define G_MWO_bLIGHT_1 0x04
#define G_MWO_MATRIX_XX_XY_I 0x00
#define G_MWO_MATRIX_XZ_XW_I 0x04
#define G_MWO_MATRIX_YX_YY_I 0x08
#define G_MWO_MATRIX_YZ_YW_I 0x0c
#define G_MWO_MATRIX_ZX_ZY_I 0x10
#define G_MWO_MATRIX_ZZ_ZW_I 0x14
#define G_MWO_MATRIX_WX_WY_I 0x18
#define G_MWO_MATRIX_WZ_WW_I 0x1c
#define G_MWO_MATRIX_XX_XY_F 0x20
#define G_MWO_MATRIX_XZ_XW_F 0x24
#define G_MWO_MATRIX_YX_YY_F 0x28
#define G_MWO_MATRIX_YZ_YW_F 0x2c
#define G_MWO_MATRIX_ZX_ZY_F 0x30
#define G_MWO_MATRIX_ZZ_ZW_F 0x34
#define G_MWO_MATRIX_WX_WY_F 0x38
#define G_MWO_MATRIX_WZ_WW_F 0x3c
#define G_MWO_POINT_RGBA 0x10
#define G_MWO_POINT_ST 0x14
#define G_MWO_POINT_XYSCREEN 0x18
#define G_MWO_POINT_ZSCREEN 0x1c

/*
 * Light structure.
 *
 * Note: only directional (infinite) lights are currently supported.
 *
 * Note: the weird order is for the DMEM alignment benefit of
 * the microcode.
 *
 */

typedef struct {
    unsigned char col[3]; /* diffuse light value (rgba) */
    char pad1;
    unsigned char colc[3]; /* copy of diffuse light value (rgba) */
    char pad2;
    signed char dir[3]; /* direction of light (normalized) */
    char pad3;
} F3DLight_t;

typedef struct {
    unsigned char col[3];
    unsigned char unk3;
    unsigned char colc[3];
    unsigned char unk7;
    short pos[3];
    unsigned char unkE;
} F3DPointLight_t;

typedef struct {
    unsigned char col[3]; /* ambient light value (rgba) */
    char pad1;
    unsigned char colc[3]; /* copy of ambient light value (rgba) */
    char pad2;
} F3DAmbient_t;

typedef struct {
    int x1, y1, x2, y2; /* texture offsets for highlight 1/2 */
} F3DHilite_t;

typedef union {
    F3DLight_t l;
    F3DPointLight_t p;
    long long int force_structure_alignment[2];
} F3DLight;

typedef union {
    F3DAmbient_t l;
    long long int force_structure_alignment[1];
} F3DAmbient;

typedef struct {
    F3DAmbient a;
    F3DLight l[7];
} F3DLightsn;

typedef struct {
    F3DAmbient a;
    F3DLight l[1];
} F3DLights0;

typedef struct {
    F3DAmbient a;
    F3DLight l[1];
} F3DLights1;

typedef struct {
    F3DAmbient a;
    F3DLight l[2];
} F3DLights2;

typedef struct {
    F3DAmbient a;
    F3DLight l[3];
} F3DLights3;

typedef struct {
    F3DAmbient a;
    F3DLight l[4];
} F3DLights4;

typedef struct {
    F3DAmbient a;
    F3DLight l[5];
} F3DLights5;

typedef struct {
    F3DAmbient a;
    F3DLight l[6];
} F3DLights6;

typedef struct {
    F3DAmbient a;
    F3DLight l[7];
} F3DLights7;

typedef struct {
    F3DLight l[2];
} F3DLookAt;

typedef union {
    F3DHilite_t h;
    long int force_structure_alignment[4];
} F3DHilite;

/*
 * Generic Gfx Packet
 */
typedef struct {
    uintptr_t w0;
    uintptr_t w1;
} F3DGwords;

#ifdef __cplusplus
static_assert(sizeof(F3DGwords) == 2 * sizeof(void*), "Display list size is bad");
#endif

/*
 * This union is the fundamental type of the display list.
 * It is, by law, exactly 64 bits in size.
 */
typedef union F3DGfx {
    F3DGwords words;
    long long int force_structure_alignment;
} F3DGfx;

/*===========================================================================*
 *	GBI Commands for S2DEX microcode
 *===========================================================================*/
/* GBI Header */
#define G_BGLT_LOADBLOCK 0x0033
#define G_BGLT_LOADTILE 0xfff4

#define G_BG_FLAG_FLIPS 0x01
#define G_BG_FLAG_FLIPT 0x10

/* Non scalable background plane */
typedef struct {
    unsigned short imageX; /* x-coordinate of upper-left position of texture (u10.5) */
    unsigned short imageW; /* width of the texture (u10.2) */
    short frameX;          /* upper-left position of transferred frame (s10.2) */
    unsigned short frameW; /* width of transferred frame (u10.2) */

    unsigned short imageY; /* y-coordinate of upper-left position of texture (u10.5) */
    unsigned short imageH; /* height of the texture (u10.2) */
    short frameY;          /* upper-left position of transferred frame (s10.2) */
    unsigned short frameH; /* height of transferred frame (u10.2) */

    unsigned long long int* imagePtr; /* texture source address on DRAM */
    unsigned short imageLoad;         /* which to use, LoadBlock or  LoadTile */
    unsigned char imageFmt;           /* format of texel - G_IM_FMT_*  */
    unsigned char imageSiz;           /* size of texel - G_IM_SIZ_*   */
    unsigned short imagePal;          /* pallet number  */
    unsigned short imageFlip;         /* right & left image inversion (Inverted by G_BG_FLAG_FLIPS) */

    /* The following is set in the initialization routine guS2DInitBg(). There is no need for the user to set it. */
    unsigned short tmemW;      /* TMEM width and Word size of frame 1 line.
                       At LoadBlock, GS_PIX2TMEM(imageW/4,imageSiz)
                       At LoadTile  GS_PIX2TMEM(frameW/4,imageSiz)+1 */
    unsigned short tmemH;      /* height of TMEM loadable at a time (s13.2) 4 times value
                       When the normal texture, 512/tmemW*4
                       When the CI texture, 256/tmemW*4 */
    unsigned short tmemLoadSH; /* SH value
                       At LoadBlock, tmemSize/2-1
                       At LoadTile, tmemW*16-1 */
    unsigned short tmemLoadTH; /* TH value or Stride value
                       At LoadBlock, GS_CALC_DXT(tmemW)
                       At LoadTile, tmemH-1 */
    unsigned short tmemSizeW;  /* skip value of imagePtr for image 1-line
                       At LoadBlock, tmemW*2
                       At LoadTile, GS_PIX2TMEM(imageW/4,imageSiz)*2 */
    unsigned short tmemSize;   /* skip value of imagePtr for 1-loading
                       = tmemSizeW*tmemH                          */
} F3DuObjBg_t;                 /* 40 bytes */

/* Scalable background plane */
typedef struct {
    unsigned short imageX; /* x-coordinate of upper-left position of texture (u10.5) */
    unsigned short imageW; /* width of texture (u10.2) */
    short frameX;          /* upper-left position of transferred frame (s10.2) */
    unsigned short frameW; /* width of transferred frame (u10.2) */

    unsigned short imageY; /* y-coordinate of upper-left position of texture (u10.5) */
    unsigned short imageH; /* height of texture (u10.2) */
    short frameY;          /* upper-left position of transferred frame (s10.2) */
    unsigned short frameH; /* height of transferred frame (u10.2) */

    unsigned long long int* imagePtr; /* texture source address on DRAM */
    unsigned short imageLoad;         /* Which to use, LoadBlock or LoadTile? */
    unsigned char imageFmt;           /* format of texel - G_IM_FMT_*  */
    unsigned char imageSiz;           /* size of texel - G_IM_SIZ_*  */
    unsigned short imagePal;          /* pallet number */
    unsigned short imageFlip;         /* right & left image inversion (Inverted by G_BG_FLAG_FLIPS) */

    unsigned short scaleW; /* scale value of X-direction (u5.10) */
    unsigned short scaleH; /* scale value of Y-direction (u5.10) */
    int imageYorig;        /* start point of drawing on image (s20.5) */

    unsigned char padding[4];

} F3DuObjScaleBg_t; /* 40 bytes */

typedef union {
    F3DuObjBg_t b;
    F3DuObjScaleBg_t s;
    long long int force_structure_alignment;
} F3DuObjBg;

/*---------------------------------------------------------------------------*
 *	2D Objects
 *---------------------------------------------------------------------------*/
#define G_OBJ_FLAG_FLIPS 1 << 0 /* inversion to S-direction */
#define G_OBJ_FLAG_FLIPT 1 << 4 /* nversion to T-direction */

typedef struct {
    short objX;                 /* s10.2 OBJ x-coordinate of upper-left end */
    unsigned short scaleW;      /* u5.10 Scaling of u5.10 width direction   */
    unsigned short imageW;      /* u10.5 width of u10.5 texture (length of S-direction) */
    unsigned short paddingX;    /* Unused - Always 0 */
    short objY;                 /* s10.2 OBJ y-coordinate of s10.2 OBJ upper-left end */
    unsigned short scaleH;      /* u5.10 Scaling of u5.10 height direction */
    unsigned short imageH;      /* u10.5 height of u10.5 texture (length of T-direction) */
    unsigned short paddingY;    /* Unused - Always 0 */
    unsigned short imageStride; /* folding width of texel (In units of 64bit word) */
    unsigned short imageAdrs;   /* texture header position in TMEM (In units of 64bit word) */
    unsigned char imageFmt;     /* format of texel - G_IM_FMT_* */
    unsigned char imageSiz;     /* size of texel - G_IM_SIZ_* */
    unsigned char imagePal;     /* pallet number (0-7) */
    unsigned char imageFlags;   /* The display flag - G_OBJ_FLAG_FLIP* */
} F3DuObjSprite_t;              /* 24 bytes */

typedef union {
    F3DuObjSprite_t s;
    long long int force_structure_alignment;
} F3DuObjSprite;

/*---------------------------------------------------------------------------*
 *	2D Matrix
 *---------------------------------------------------------------------------*/
typedef struct {
    int A, B, C, D;            /* s15.16 */
    short X, Y;                /* s10.2  */
    unsigned short BaseScaleX; /* u5.10  */
    unsigned short BaseScaleY; /* u5.10  */
} F3DuObjMtx_t;                /* 24 bytes */

typedef union {
    F3DuObjMtx_t m;
    long long int force_structure_alignment;
} F3DuObjMtx;

typedef struct {
    short X, Y;                /* s10.2  */
    unsigned short BaseScaleX; /* u5.10  */
    unsigned short BaseScaleY; /* u5.10  */
} F3DuObjSubMtx_t;             /* 8 bytes */

typedef union {
    F3DuObjSubMtx_t m;
    long long int force_structure_alignment;
} F3DuObjSubMtx;

/*---------------------------------------------------------------------------*
 *	Loading into TMEM
 *---------------------------------------------------------------------------*/
#define G_OBJLT_TXTRBLOCK 0x00001033
#define G_OBJLT_TXTRTILE 0x00fc1034
#define G_OBJLT_TLUT 0x00000030

#define GS_TB_TSIZE(pix, siz) (GS_PIX2TMEM((pix), (siz)) - 1)
#define GS_TB_TLINE(pix, siz) (GS_CALC_DXT(GS_PIX2TMEM((pix), (siz))))

typedef struct {
    unsigned int type;             /* G_OBJLT_TXTRBLOCK divided into types */
    unsigned long long int* image; /* texture source address on DRAM */
    unsigned short tmem;           /* loaded TMEM word address (8byteWORD) */
    unsigned short tsize;          /* Texture size, Specified by macro GS_TB_TSIZE() */
    unsigned short tline;          /* width of Texture 1-line, Specified by macro GS_TB_TLINE() */
    unsigned short sid;            /* STATE ID Multipled by 4 (Either one of  0, 4, 8 and 12) */
    unsigned int flag;             /* STATE flag  */
    unsigned int mask;             /* STATE mask  */
} F3DuObjTxtrBlock_t;              /* 24 bytes */

#define GS_TT_TWIDTH(pix, siz) ((GS_PIX2TMEM((pix), (siz)) << 2) - 1)
#define GS_TT_THEIGHT(pix, siz) (((pix) << 2) - 1)

typedef struct {
    unsigned int type;             /* G_OBJLT_TXTRTILE divided into types */
    unsigned long long int* image; /* texture source address on DRAM */
    unsigned short tmem;           /* loaded TMEM word address (8byteWORD)*/
    unsigned short twidth;         /* width of Texture (Specified by macro GS_TT_TWIDTH()) */
    unsigned short theight;        /* height of Texture (Specified by macro GS_TT_THEIGHT()) */
    unsigned short sid;            /* STATE ID Multipled by 4 (Either one of  0, 4, 8 and 12) */
    unsigned int flag;             /* STATE flag  */
    unsigned int mask;             /* STATE mask  */
} F3DuObjTxtrTile_t;               /* 24 bytes */

#define GS_PAL_HEAD(head) ((head) + 256)
#define GS_PAL_NUM(num) ((num)-1)

typedef struct {
    unsigned int type;             /* G_OBJLT_TLUT divided into types */
    unsigned long long int* image; /* texture source address on DRAM */
    unsigned short phead;          /* pallet number of load header (Between 256 and 511) */
    unsigned short pnum;           /* loading pallet number -1 */
    unsigned short zero;           /* Assign 0 all the time */
    unsigned short sid;            /* STATE ID Multipled by 4 (Either one of  0, 4, 8 and 12)*/
    unsigned int flag;             /* STATE flag  */
    unsigned int mask;             /* STATE mask  */
} F3DuObjTxtrTLUT_t;               /* 24 bytes */

typedef union {
    F3DuObjTxtrBlock_t block;
    F3DuObjTxtrTile_t tile;
    F3DuObjTxtrTLUT_t tlut;
    long long int force_structure_alignment;
} F3DuObjTxtr;

/*---------------------------------------------------------------------------*
 *	Loading into TMEM & 2D Objects
 *---------------------------------------------------------------------------*/
typedef struct {
    F3DuObjTxtr txtr;
    F3DuObjSprite sprite;
} F3DuObjTxSprite; /* 48 bytes */
}
#endif
