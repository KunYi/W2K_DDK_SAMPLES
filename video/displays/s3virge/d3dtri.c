/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dtri.c
*
*  Content:    Direct3D triangle rasterization code.
*
* Copyright (C) 1996-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"
#include "hw3d.h"

// limit the max. and min. of u and v

#define LIMIT_HI_TO_ONE(uv) { if ((uv) > 0.99f)   uv = 0.99f;  }
#define LIMIT_LO_TO_ZRO(uv) { if ((uv) < 0.01f)   uv = 0.01f;  }


BOOL DXGXSmartFilter( LPD3DTLVERTEX p0,
                      LPD3DTLVERTEX p1,
                      LPD3DTLVERTEX p2,
                      DWORD *rndCommand,
                      LPS3FVFOFFSETS   lpS3FVFOff)
{
    float s1, s2;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);

    s1 = (p0->sx - p2->sx) * (p1->sy - p2->sy) -
         (p1->sx - p2->sx) * (p0->sy - p2->sy);

    if ( s1 < 0.0 )
        s1 = -s1;

    s2 = (FVFTEX(p0, dwTexOffs)->tu - FVFTEX(p2, dwTexOffs)->tu) *
         (FVFTEX(p1, dwTexOffs)->tv - FVFTEX(p2, dwTexOffs)->tv) -
         (FVFTEX(p1, dwTexOffs)->tu - FVFTEX(p2, dwTexOffs)->tu) *
         (FVFTEX(p0, dwTexOffs)->tv - FVFTEX(p2, dwTexOffs)->tv);

    if ( s2 < 0.0 )
        s2 = -s2;

    if (s1 <= s2 * 4.0) {
        *rndCommand &= cmdTEX_FILT_MODE_MASK;
        *rndCommand |= cmdTEX_FILT_MODE_2VTpp;
        return TRUE;
    } else {
       *rndCommand &= cmdTEX_FILT_MODE_MASK;
       *rndCommand |= cmdTEX_FILT_MODE_4Tpp;
       return FALSE;
    }

    return FALSE;
}

#undef GOURAUD
#undef DMASUPPORT
#undef ZBUFFER
#undef TEXTURED
#undef PERSPECTIVE
#undef FOGGED
#undef FNAME
#undef WRAPCODE

#define FNAME       ________Triangle
#include "d3dgntri.c"

#define GOURAUD
#define FNAME       G_______Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define FNAME       _S______Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define FNAME       GS______Triangle
#include "d3dgntri.c"

#define ZBUFFER
#define FNAME       __Z_____Triangle
#include "d3dgntri.c"

#define GOURAUD
#define ZBUFFER
#define FNAME       G_Z_____Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define ZBUFFER
#define FNAME       _SZ_____Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define ZBUFFER
#define FNAME       GSZ_____Triangle
#include "d3dgntri.c"

#define TEXTURED
#define FNAME       ___T____Triangle
#include "d3dgntri.c"

#define GOURAUD
#define TEXTURED
#define FNAME       G__T____Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define TEXTURED
#define FNAME       _S_T____Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define TEXTURED
#define FNAME       GS_T____Triangle
#include "d3dgntri.c"

#define ZBUFFER
#define TEXTURED
#define FNAME       __ZT____Triangle
#include "d3dgntri.c"

#define GOURAUD
#define ZBUFFER
#define TEXTURED
#define FNAME       G_ZT____Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define FNAME       _SZT____Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define FNAME       GSZT____Triangle
#include "d3dgntri.c"

#define TEXTURED
#define PERSPECTIVE
#define FNAME       ___TP___Triangle
#include "d3dgntri.c"

#define GOURAUD
#define TEXTURED
#define PERSPECTIVE
#define FNAME       G__TP___Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define TEXTURED
#define PERSPECTIVE
#define FNAME       _S_TP___Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define TEXTURED
#define PERSPECTIVE
#define FNAME       GS_TP___Triangle
#include "d3dgntri.c"

#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FNAME       __ZTP___Triangle
#include "d3dgntri.c"

#define GOURAUD
#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FNAME       G_ZTP___Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FNAME       _SZTP___Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FNAME       GSZTP___Triangle
#include "d3dgntri.c"

#define FOGGED
#define FNAME       _____F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define FOGGED
#define FNAME       G____F__Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define FOGGED
#define FNAME       _S___F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define FOGGED
#define FNAME       GS___F__Triangle
#include "d3dgntri.c"

#define ZBUFFER
#define FOGGED
#define FNAME       __Z__F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define ZBUFFER
#define FOGGED
#define FNAME       G_Z__F__Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define ZBUFFER
#define FOGGED
#define FNAME       _SZ__F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define ZBUFFER
#define FOGGED
#define FNAME       GSZ__F__Triangle
#include "d3dgntri.c"

#define TEXTURED
#define FOGGED
#define FNAME       ___T_F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define TEXTURED
#define FOGGED
#define FNAME       G__T_F__Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define TEXTURED
#define FOGGED
#define FNAME       _S_T_F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define TEXTURED
#define FOGGED
#define FNAME       GS_T_F__Triangle
#include "d3dgntri.c"

#define ZBUFFER
#define TEXTURED
#define FOGGED
#define FNAME       __ZT_F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define ZBUFFER
#define TEXTURED
#define FOGGED
#define FNAME       G_ZT_F__Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define FOGGED
#define FNAME       _SZT_F__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define FOGGED
#define FNAME       GSZT_F__Triangle
#include "d3dgntri.c"

#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       ___TPF__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       G__TPF__Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       _S_TPF__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       GS_TPF__Triangle
#include "d3dgntri.c"

#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       __ZTPF__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       G_ZTPF__Triangle
#include "d3dgntri.c"

#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       _SZTPF__Triangle
#include "d3dgntri.c"

#define GOURAUD
#define DMASUPPORT
#define ZBUFFER
#define TEXTURED
#define PERSPECTIVE
#define FOGGED
#define FNAME       GSZTPF__Triangle
#include "d3dgntri.c"


LPRENDERTRIANGLE pRenderTriangle[] =    //refer to RStyle definition
{
    ________Triangle,       G_______Triangle,       _S______Triangle,       GS______Triangle,
    __Z_____Triangle,       G_Z_____Triangle,       _SZ_____Triangle,       GSZ_____Triangle,
    ___T____Triangle,       G__T____Triangle,       _S_T____Triangle,       GS_T____Triangle,
    __ZT____Triangle,       G_ZT____Triangle,       _SZT____Triangle,       GSZT____Triangle,
    ________Triangle,       G_______Triangle,       _S______Triangle,       GS______Triangle,
    __Z_____Triangle,       G_Z_____Triangle,       _SZ_____Triangle,       GSZ_____Triangle,
    ___TP___Triangle,       G__TP___Triangle,       _S_TP___Triangle,       GS_TP___Triangle,
    __ZTP___Triangle,       G_ZTP___Triangle,       _SZTP___Triangle,       GSZTP___Triangle,
    _____F__Triangle,       G____F__Triangle,       _S___F__Triangle,       GS___F__Triangle,
    __Z__F__Triangle,       G_Z__F__Triangle,       _SZ__F__Triangle,       GSZ__F__Triangle,
    ___T_F__Triangle,       G__T_F__Triangle,       _S_T_F__Triangle,       GS_T_F__Triangle,
    __ZT_F__Triangle,       G_ZT_F__Triangle,       _SZT_F__Triangle,       GSZT_F__Triangle,
    _____F__Triangle,       G____F__Triangle,       _S___F__Triangle,       GS___F__Triangle,
    __Z__F__Triangle,       G_Z__F__Triangle,       _SZ__F__Triangle,       GSZ__F__Triangle,
    ___TPF__Triangle,       G__TPF__Triangle,       _S_TPF__Triangle,       GS_TPF__Triangle,
    __ZTPF__Triangle,       G_ZTPF__Triangle,       _SZTPF__Triangle,       GSZTPF__Triangle,
};

