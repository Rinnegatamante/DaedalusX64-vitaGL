/*
 Copyright (C) 2010 StrmnNrmn

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "stdafx.h"

#include "Ucode.h"

// Only the official ucode tables are defined here, 5 in total
// Please do not add any new ucode tables here, doing so will just waste memory
// If you want to add a custom ucode table, just patch any of official ucode tables with GBIMicrocode_SetCustomArray

//*************************************************************************************
//
//*************************************************************************************
const MicroCodeInstruction gNormalInstruction[MAX_UCODE_TABLE][256] =
{
	// uCode 0 - RSP SW 2.0X
	// Games: Super Mario 64, Tetrisphere, Demos
	{
		DLParser_GBI1_SpNoop, DLParser_GBI1_Mtx, DLParser_GBI1_Reserved, DLParser_GBI1_MoveMem,
		DLParser_GBI0_Vtx, DLParser_GBI1_Reserved, DLParser_GBI1_DL, DLParser_GBI1_Reserved,
		DLParser_GBI1_Reserved, DLParser_GBI1_Sprite2DBase, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//10
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//20
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//30
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//40
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//50
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//60
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//70
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,

		//80
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//90
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//a0
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//b0
		DLParser_Nothing, DLParser_Nothing, DLParser_GBI1_RDPHalf_Cont, DLParser_GBI1_RDPHalf_2,
		DLParser_GBI1_RDPHalf_1, DLParser_GBI0_Line3D, DLParser_GBI1_GeometryMode, DLParser_GBI1_GeometryMode,
		DLParser_GBI1_EndDL, DLParser_GBI1_SetOtherModeL, DLParser_GBI1_SetOtherModeH, DLParser_GBI1_Texture,
		DLParser_GBI1_MoveWord, DLParser_GBI1_PopMtx, DLParser_GBI0_CullDL, DLParser_GBI0_Tri1,

		//c0
		DLParser_GBI1_Noop, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_TriRSP, DLParser_TriRSP, DLParser_TriRSP, DLParser_TriRSP,
		DLParser_TriRSP, DLParser_TriRSP, DLParser_TriRSP, DLParser_TriRSP,
		//d0
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//e0
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_TexRect, DLParser_TexRectFlip, DLParser_RDPLoadSync, DLParser_RDPPipeSync,
		DLParser_RDPTileSync, DLParser_RDPFullSync, DLParser_SetKeyGB, DLParser_SetKeyR,
		DLParser_SetConvert, DLParser_SetScissor, DLParser_SetPrimDepth, DLParser_RDPSetOtherMode,
		//f0
		DLParser_LoadTLut, DLParser_Nothing, DLParser_SetTileSize, DLParser_LoadBlock,
		DLParser_LoadTile, DLParser_SetTile, DLParser_FillRect, DLParser_SetFillColor,
		DLParser_SetFogColor, DLParser_SetBlendColor, DLParser_SetPrimColor, DLParser_SetEnvColor,
		DLParser_SetCombine, DLParser_SetTImg, DLParser_SetZImg, DLParser_SetCImg
	},
	// uCode 1 - F3DEX 1.XX
	// 00-3f
	// games: Mario Kart, Star Fox
	{
		DLParser_GBI1_SpNoop,	DLParser_GBI1_Mtx,			DLParser_GBI1_Reserved,		DLParser_GBI1_MoveMem,
		DLParser_GBI1_Vtx,		DLParser_GBI1_Reserved,		DLParser_GBI1_DL,			DLParser_GBI1_Reserved,
		DLParser_GBI1_Reserved, DLParser_GBI1_Sprite2DBase, DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//10
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//20
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//30
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//40
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//50
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//60
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//70
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//80
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//90
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//a0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_GBI1_LoadUCode,
		//b0
		DLParser_GBI1_BranchZ,	DLParser_GBI1_Tri2,			DLParser_GBI1_ModifyVtx,		DLParser_GBI1_RDPHalf_2,
		DLParser_GBI1_RDPHalf_1,DLParser_GBI1_Line3D,		DLParser_GBI1_GeometryMode,DLParser_GBI1_GeometryMode,
		DLParser_GBI1_EndDL,	DLParser_GBI1_SetOtherModeL,DLParser_GBI1_SetOtherModeH,	DLParser_GBI1_Texture,
		DLParser_GBI1_MoveWord, DLParser_GBI1_PopMtx,		DLParser_GBI1_CullDL,			DLParser_GBI1_Tri1,
		//c0
		DLParser_GBI1_Noop,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_TriRSP,		DLParser_TriRSP,			DLParser_TriRSP,			DLParser_TriRSP,
		DLParser_TriRSP,		DLParser_TriRSP,			DLParser_TriRSP,			DLParser_TriRSP,
		//d0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//e0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_TexRect,		DLParser_TexRectFlip,		DLParser_RDPLoadSync,		DLParser_RDPPipeSync,
		DLParser_RDPTileSync,	DLParser_RDPFullSync,		DLParser_SetKeyGB,			DLParser_SetKeyR,
		DLParser_SetConvert,	DLParser_SetScissor,		DLParser_SetPrimDepth,		DLParser_RDPSetOtherMode,
		//f0
		DLParser_LoadTLut,		DLParser_Nothing,			DLParser_SetTileSize,		DLParser_LoadBlock,
		DLParser_LoadTile,		DLParser_SetTile,			DLParser_FillRect,			DLParser_SetFillColor,
		DLParser_SetFogColor,	DLParser_SetBlendColor,		DLParser_SetPrimColor,		DLParser_SetEnvColor,
		DLParser_SetCombine,	DLParser_SetTImg,			DLParser_SetZImg,			DLParser_SetCImg
	},

	// Ucode:F3DEX_GBI_2
	// Zelda and new games
	{
		DLParser_GBI1_SpNoop,	DLParser_GBI2_Vtx,			DLParser_GBI1_ModifyVtx,	DLParser_GBI1_CullDL,
		DLParser_GBI1_BranchZ,  DLParser_GBI2_Tri1,			DLParser_GBI2_Tri2,         DLParser_GBI2_Quad,
		DLParser_GBI2_Line3D,   DLParser_S2DEX_Bg1cyc,		DLParser_S2DEX_BgCopy,		DLParser_S2DEX_ObjRendermode,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//10
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//20
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//30
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//40
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//50
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//60
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//70
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//80
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//90
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//a0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//b0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		//c0
		DLParser_GBI1_Noop,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_TriRSP,		DLParser_TriRSP,			DLParser_TriRSP,			DLParser_TriRSP,
		DLParser_TriRSP,		DLParser_TriRSP,			DLParser_TriRSP,			DLParser_TriRSP,
		//d0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_GBI2_DL_Count,		DLParser_GBI2_DMA_IO,		DLParser_GBI2_Texture,
		DLParser_GBI2_PopMtx,	DLParser_GBI2_GeometryMode, DLParser_GBI2_Mtx,			DLParser_GBI2_MoveWord,
		DLParser_GBI2_MoveMem,	DLParser_GBI1_LoadUCode,	DLParser_GBI1_DL,			DLParser_GBI1_EndDL,
		//e0
		DLParser_GBI1_SpNoop,	DLParser_GBI1_RDPHalf_1,	DLParser_GBI2_SetOtherModeL,DLParser_GBI2_SetOtherModeH,
		DLParser_TexRect,		DLParser_TexRectFlip,		DLParser_RDPLoadSync,		DLParser_RDPPipeSync,
		DLParser_RDPTileSync,	DLParser_RDPFullSync,		DLParser_SetKeyGB,			DLParser_SetKeyR,
		DLParser_SetConvert,	DLParser_SetScissor,		DLParser_SetPrimDepth,		DLParser_RDPSetOtherMode,
		//f0
		DLParser_LoadTLut,		DLParser_GBI1_RDPHalf_2,	DLParser_SetTileSize,		DLParser_LoadBlock,
		DLParser_LoadTile,		DLParser_SetTile,			DLParser_FillRect,			DLParser_SetFillColor,
		DLParser_SetFogColor,	DLParser_SetBlendColor,		DLParser_SetPrimColor,		DLParser_SetEnvColor,
		DLParser_SetCombine,	DLParser_SetTImg,			DLParser_SetZImg,			DLParser_SetCImg
	},

	// Ucode: S2DEX 1.--
	// Games: Yoshi's Story
	{
		DLParser_GBI1_SpNoop,		DLParser_S2DEX_Bg1cyc,		DLParser_S2DEX_BgCopy,	DLParser_S2DEX_ObjRectangle,
		DLParser_S2DEX_ObjSprite,	DLParser_S2DEX_ObjMoveMem,		DLParser_GBI1_DL,		DLParser_GBI1_Reserved,
		DLParser_GBI1_Reserved,		DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//10
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//20
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//30
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//40
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//50
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//60
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//70
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//80
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//90
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//a0
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_GBI1_LoadUCode,
		//b0
		DLParser_S2DEX_SelectDl,	DLParser_S2DEX_ObjRendermode,	DLParser_S2DEX_ObjRectangleR,	DLParser_GBI1_RDPHalf_2,
		DLParser_GBI1_RDPHalf_1,	DLParser_GBI1_Line3D,			DLParser_GBI1_GeometryMode,DLParser_GBI1_GeometryMode,
		DLParser_GBI1_EndDL,		DLParser_GBI1_SetOtherModeL,	DLParser_GBI1_SetOtherModeH,	DLParser_GBI1_Texture,
		DLParser_GBI1_MoveWord,		DLParser_GBI1_PopMtx,			DLParser_GBI1_CullDL,			DLParser_GBI1_Tri1,
		//c0
		DLParser_GBI1_Noop,			DLParser_S2DEX_ObjLoadTxtr,		DLParser_S2DEX_ObjLdtxSprite,	DLParser_S2DEX_ObjLdtxRect,
		DLParser_S2DEX_ObjLdtxRectR,DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_TriRSP,			DLParser_TriRSP,				DLParser_TriRSP,		DLParser_TriRSP,
		DLParser_TriRSP,			DLParser_TriRSP,				DLParser_TriRSP,		DLParser_TriRSP,
		//d0
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		//e0
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,		DLParser_Nothing,
		DLParser_S2DEX_RDPHalf_0,	DLParser_TexRectFlip,			DLParser_RDPLoadSync,	DLParser_RDPPipeSync,
		DLParser_RDPTileSync,		DLParser_RDPFullSync,			DLParser_SetKeyGB,		DLParser_SetKeyR,
		DLParser_SetConvert,		DLParser_SetScissor,			DLParser_SetPrimDepth,	DLParser_RDPSetOtherMode,
		//f0
		DLParser_LoadTLut,			DLParser_Nothing,				DLParser_SetTileSize,	DLParser_LoadBlock,
		DLParser_LoadTile,			DLParser_SetTile,				DLParser_FillRect,		DLParser_SetFillColor,
		DLParser_SetFogColor,		DLParser_SetBlendColor,			DLParser_SetPrimColor,	DLParser_SetEnvColor,
		DLParser_SetCombine,		DLParser_SetTImg,				DLParser_SetZImg,		DLParser_SetCImg
	},

	// Ucode: S2DEX 2.--
	// Games: Neon Evangelion, Kirby
	{

		DLParser_GBI1_Noop,			DLParser_S2DEX_ObjRectangle,	DLParser_S2DEX_ObjSprite,		DLParser_GBI1_CullDL,
		DLParser_S2DEX_SelectDl,	DLParser_S2DEX_ObjLoadTxtr,		DLParser_S2DEX_ObjLdtxSprite,	DLParser_S2DEX_ObjLdtxRect,
		DLParser_S2DEX_ObjLdtxRectR,DLParser_S2DEX_Bg1cyc,			DLParser_S2DEX_BgCopy,			DLParser_S2DEX_ObjRendermode,
		DLParser_Nothing,			DLParser_Nothing,				DLParser_Nothing,				DLParser_Nothing,

		//10
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//20
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//30
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//40
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//50
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//60
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//70
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//80
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//90
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//a0
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//b0
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		DLParser_Nothing, DLParser_Nothing, DLParser_Nothing, DLParser_Nothing,
		//c0
		DLParser_GBI1_Noop,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_TriRSP,		DLParser_TriRSP,			DLParser_TriRSP,			DLParser_TriRSP,
		DLParser_TriRSP,		DLParser_TriRSP,			DLParser_TriRSP,			DLParser_TriRSP,
		//d0
		DLParser_Nothing,		DLParser_Nothing,			DLParser_Nothing,			DLParser_Nothing,
		DLParser_Nothing,		DLParser_GBI2_DL_Count,		DLParser_GBI2_DMA_IO,		DLParser_GBI2_Texture,
		DLParser_GBI2_PopMtx,	DLParser_GBI2_GeometryMode, DLParser_S2DEX_ObjRectangleR,DLParser_GBI2_MoveWord,
		DLParser_GBI2_MoveMem,	DLParser_GBI1_LoadUCode,	DLParser_GBI1_DL,			DLParser_GBI1_EndDL,
		//e0
		DLParser_GBI1_SpNoop,	DLParser_GBI1_RDPHalf_1,	DLParser_GBI2_SetOtherModeL,DLParser_GBI2_SetOtherModeH,
		DLParser_TexRect,		DLParser_TexRectFlip,		DLParser_RDPLoadSync,		DLParser_RDPPipeSync,
		DLParser_RDPTileSync,	DLParser_RDPFullSync,		DLParser_SetKeyGB,			DLParser_SetKeyR,
		DLParser_SetConvert,	DLParser_SetScissor,		DLParser_SetPrimDepth,		DLParser_RDPSetOtherMode,
		//f0
		DLParser_LoadTLut,		DLParser_GBI1_RDPHalf_2,	DLParser_SetTileSize,		DLParser_LoadBlock,
		DLParser_LoadTile,		DLParser_SetTile,			DLParser_FillRect,			DLParser_SetFillColor,
		DLParser_SetFogColor,	DLParser_SetBlendColor,		DLParser_SetPrimColor,		DLParser_SetEnvColor,
		DLParser_SetCombine,	DLParser_SetTImg,			DLParser_SetZImg,			DLParser_SetCImg
	},
};
