#pragma once

typedef int Mtx_t[4][4];
typedef union {
    Mtx_t m;
    struct {
        unsigned short int intPart[4][4];
        unsigned short int fracPart[4][4];
    };
    long long int forc_structure_alignment;
} MtxS;

typedef float MtxF_t[4][4];
typedef union {
    MtxF_t mf;
    struct {
        float xx, yx, zx, wx, xy, yy, zy, wy, xz, yz, zz, wz, xw, yw, zw, ww;
    };
} MtxF;

#ifndef GBI_FLOATS
typedef MtxS Mtx;
#else
typedef MtxF Mtx;
#endif
