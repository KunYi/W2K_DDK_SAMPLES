/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dtess.c
*
*  Content:    Direct3D triangle tesselation code.
*
* Copyright (C) 1996-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"

#define LIMIT_UV_HI(uv)     { if ((uv) > 1.0e6f)  uv = 127.9f; }
#define LIMIT_UV_LO(uv)     { if ((uv) < -1.0e6f) uv = -128.0f;}

#define UVCLIP_U        1
#define UVCLIP_V        2
#define EPS             (1.0e-5)

//************************************************************************
// The S3Virge cannot rasterize a textured, perspective corrected triangle
// which exceeds a maximum limit of texels per triangle side. This limit
// is 128 texels on the original Virge and Virge VX and 2048 on the
// Virge DX. When we find a triangle which exceeds this constraint we need
// to tesselate it into smaller triangles which don't exceed the constraint.
// We then send these triangles to the triangle hw rendering function to get
// them rasterized.
//
// The algorithm we use to achieve this goes as follows:
//
// We must visualize for the purpouse of this procedure triangles in UV
// space (vs XYZ space), since that is our space of interest(where we have
// constraints). We first detect in which direction (U or V) is our
// constraint being mostly exceeded. For purpouses of the explanation lets
// assume it is the U direction. The algorithm for the V direction will
// follow naturally from the former. Taking as an example the following
// triangle, we take our (illustrative) texel limit to be 5:
//
//            **
//          **  ***
//        **       ****
//      **             ***
//    **                  ****
//  **                        ***
//  *****************************
//
//
// which is being exceeded in all directions, but mostly in U. We proceed
// to partition the triangle in vertical trapezoidal stripes, which do not
// exceed for any reason our limit (5). We take special care at the vertex
// transition in order not to generate a complex polygon which wouldn't be
// trapezoid:
//
//            *|
//          *| |***
//        ** | |   *|**
//      |*   | |    |  **|
//    **|    | |    |    |****
//  **  |    | |    |    |    |**
//  ****|****|*|****|****|****|**
//    1    2  3   4    5   6    7
//
// We see that in this example we generated 7 trapezoidal stripes. We take
// stripe # 4 ans now perform a similar tesselation in the V direction,
// and generate as many triangles as needed in each polygon created:
//
//             |           |                               *
//             |***        |***                            * *
//             |   *|      |---*|   *          *****       *****
//             |    |      |    |   **          *  *
//             |    |  ->  |    | = * *    +     * *    +
//             |    |      |    |   *  *          **
//             |****|      |****|   *****          *
//
//
// This is the basic algorithm . We have to take care of course of some
// situations on both steps, which are explained directly in the code below.
//************************************************************************

//************************************************************************
//
// void CalculateNewVertexProps
//
// Calulate a new vertexes (pr)  properties , thsi new vertex lies on
// the edge p0-p1 in texture coordinate space such that (in vector notation)
// pr = p0 + ratio * ( p0 - p1 ). FlatColor and FlatSpecular help us to
// avoid recalculating colors if no gouraud shading is required.
//************************************************************************
void CalcNewVertexProps(S3_CONTEXT *pCtxt,
                        LPD3DTLVERTEX pr,
                        LPD3DTLVERTEX p0,
                        LPD3DTLVERTEX p1,
                        _PRECISION ratio,
                        D3DCOLOR FlatColor,
                        D3DCOLOR FlatSpecular,
                        LPS3FVFOFFSETS lpS3FVFOff)
{
    _PRECISION sUV, eUV, mUV, prpInXYSpace;
    _PRECISION diffU, diffV, diffUV;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;
    D3DCOLOR Color0, Color1;

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs, lpS3FVFOff);

    // trivial fast case solutions
    if (ratio == 0.0f) {
        __CpyFVFVertexes( pr , p0, lpS3FVFOff );
        return;
    }

    if (ratio == 1.0f) {
        __CpyFVFVertexes( pr , p1, lpS3FVFOff );
        return;
    }

    DPF_DBG("CalcNewVertexProps (lengthy calc) performed , ratio=%d",
            (int)(ratio*1000));

    diffU = (_PRECISION)FVFTEX(p1, dwTexOffs)->tu - (_PRECISION)FVFTEX(p0, dwTexOffs)->tu;
    diffV = (_PRECISION)FVFTEX(p1, dwTexOffs)->tv - (_PRECISION)FVFTEX(p0, dwTexOffs)->tv;

    FVFTEX(pr, dwTexOffs)->tu = (D3DVALUE)((_PRECISION)FVFTEX(p0, dwTexOffs)->tu + diffU * ratio);
    FVFTEX(pr, dwTexOffs)->tv = (D3DVALUE)((_PRECISION)FVFTEX(p0, dwTexOffs)->tv + diffV * ratio);

    pr->rhw=(D3DVALUE)(1.0 / (1.0 / (_PRECISION)p0->rhw +
              (1.0 / (_PRECISION)p1->rhw - 1.0/(_PRECISION)p0->rhw) * ratio));

    if(pCtxt->dwRCode & S3PERSPECTIVE) {
        if (fabs(diffU) > fabs(diffV)) {
            sUV = FVFTEX(p0, dwTexOffs)->tu * p0->rhw;
            eUV = FVFTEX(p1, dwTexOffs)->tu * p1->rhw;
            if (( diffUV = eUV - sUV ) != 0.0) {
                mUV = FVFTEX(pr, dwTexOffs)->tu * pr->rhw;
                prpInXYSpace = ( mUV - sUV ) / diffUV;
            } else {
                prpInXYSpace = 0.0f;
            }
        } else {
            sUV = FVFTEX(p0, dwTexOffs)->tv * p0->rhw;
            eUV = FVFTEX(p1, dwTexOffs)->tv * p1->rhw;
            if (( diffUV = eUV - sUV ) != 0.0) {
                mUV = FVFTEX(pr, dwTexOffs)->tv * pr->rhw;
                prpInXYSpace = ( mUV - sUV ) / diffUV;
            } else {
                prpInXYSpace = 0.0f;
            }
        }
    } else {
        prpInXYSpace = ratio;
    }

    pr->sx = (D3DVALUE)((_PRECISION)p0->sx +
               ((_PRECISION)p1->sx - (_PRECISION)p0->sx) * prpInXYSpace);
    pr->sy = (D3DVALUE)((_PRECISION)p0->sy +
               ((_PRECISION)p1->sy - (_PRECISION)p0->sy) * prpInXYSpace);
    pr->sz = (D3DVALUE)((_PRECISION)p0->sz +
               ((_PRECISION)p1->sz - (_PRECISION)p0->sz) * prpInXYSpace);

    if(pCtxt->ShadeMode == D3DSHADE_GOURAUD) {
        BYTE r,g,b,a;

        // If there is a color component in the FVF vertex compute it
        if (dwColorOffs) {
            Color0 = FVFCOLOR(p0, dwColorOffs)->color;
            Color1 = FVFCOLOR(p1, dwColorOffs)->color;

            r = INTERPCOLOR(Color0, Color1, RGBA_GETRED  , prpInXYSpace);
            g = INTERPCOLOR(Color0, Color1, RGBA_GETGREEN, prpInXYSpace);
            b = INTERPCOLOR(Color0, Color1, RGBA_GETBLUE , prpInXYSpace);
            a = INTERPCOLOR(Color0, Color1, RGBA_GETALPHA, prpInXYSpace);

            FVFCOLOR(pr, dwColorOffs)->color = RGBA_MAKE( r, g, b, a );
        }

        // If there is a specular component in the FVF vertex compute it
        if (dwSpecOffs) {
            Color0 = FVFSPEC(p0, dwSpecOffs)->specular;
            Color1 = FVFSPEC(p1, dwSpecOffs)->specular;

            r = INTERPCOLOR(Color0, Color1, RGBA_GETRED  ,prpInXYSpace);
            g = INTERPCOLOR(Color0, Color1, RGBA_GETGREEN,prpInXYSpace);
            b = INTERPCOLOR(Color0, Color1, RGBA_GETBLUE ,prpInXYSpace);
            a = INTERPCOLOR(Color0, Color1, RGBA_GETALPHA,prpInXYSpace);

            FVFSPEC(pr, dwSpecOffs)->specular = RGBA_MAKE( r, g, b, a );
        }

    } else {
        if (dwColorOffs) {
            FVFCOLOR(pr, dwColorOffs)->color = FlatColor;
        }

        if (dwSpecOffs) {
            FVFSPEC(pr, dwSpecOffs)->specular = FlatSpecular;
        }
    }
}

//************************************************************************
//
// int OutOfUVRange
//
//
// Returns
//     0         if triangle doesn't exceed uv constraints
//     UVCLIP_U  if triangle exceeds constraints (predominantly) in U
//     UVCLIP_V  if triangle exceeds constraints (predominantly) in V
//                 in this two last cases , piMaxInd is set to the
//
//************************************************************************

int OutOfUVRange( LPD3DTLVERTEX v[ 3 ], double tex_w, double tex_h,
                  int *piMaxInd,S3_CONTEXT*pCtxt, LPS3FVFOFFSETS lpS3FVFOff)
{
    _PRECISION delt_max_U = 0.0f, delt_max_V = 0.0f;
    int i, j, k;
    int uMax = 0, vMax = 0;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);


    for ( i = 0; i<3; i++ )
    {
        j = (i+1)%3;
        k = (i+2)%3;
        if (FVFTEX(v[i], dwTexOffs)->tu == (D3DVALUE)((int)FVFTEX(v[i], dwTexOffs)->tu)) {
            if (FVFTEX(v[i], dwTexOffs)->tu >= FVFTEX(v[j], dwTexOffs)->tu &&
                FVFTEX(v[i], dwTexOffs)->tu >= FVFTEX(v[k], dwTexOffs)->tu)
                FVFTEX(v[i], dwTexOffs)->tu -= (D3DVALUE)(TEXTURE_FACTOR / tex_w);
        }

        if (FVFTEX(v[i], dwTexOffs)->tv == (D3DVALUE)((int)FVFTEX(v[i], dwTexOffs)->tv)) {
            if (FVFTEX(v[i], dwTexOffs)->tv >= FVFTEX(v[j], dwTexOffs)->tv &&
                FVFTEX(v[i], dwTexOffs)->tv >= FVFTEX(v[k], dwTexOffs)->tv)
                FVFTEX(v[i], dwTexOffs)->tv -= (D3DVALUE)(TEXTURE_FACTOR / tex_h);
        }
    }

    k = 1;
    for( i = 0; i < 3 ; i++ )
    {
        _PRECISION delt;

        delt = (_PRECISION)fabs((FVFTEX(v[i], dwTexOffs)->tu -
                                 FVFTEX(v[k], dwTexOffs)->tu) * tex_w);

        if ( delt > delt_max_U ) {
            delt_max_U = delt;
            uMax = i;
        }

        delt = (_PRECISION)fabs((FVFTEX(v[i], dwTexOffs)->tv -
                                 FVFTEX(v[k], dwTexOffs)->tv) * tex_h);

        if ( delt > delt_max_V ) {
            delt_max_V = delt;
            vMax = i;
        }

        if (++k == 3)
            k = 0;
    }

    if ( delt_max_U > D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE ||
         delt_max_V > D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE ) {

        if ( delt_max_U > delt_max_V ) {
            *piMaxInd = uMax;
            return(UVCLIP_U);
        } else {
            *piMaxInd = vMax;
            return(UVCLIP_V);
        }
    }

    return ( 0 );
}

//************************************************************************
//
// int ValidTriangle
//
// Checks the triangle is not degenerate, i.e., that its vertexes are not
// that close enough to be just a point. We use this to avoid rendering
// invalid triangles generated by the algorithm.
//
//************************************************************************
int ValidTriangle(LPD3DTLVERTEX pV0,
                  LPD3DTLVERTEX pV1,
                  LPD3DTLVERTEX pV2,
                  LPS3FVFOFFSETS lpS3FVFOff)
{
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);

    if ((fabs(FVFTEX(pV0, dwTexOffs)->tu - FVFTEX(pV1, dwTexOffs)->tu) < EPS) &&
        (fabs(FVFTEX(pV0, dwTexOffs)->tv - FVFTEX(pV1, dwTexOffs)->tv) < EPS))
        return FALSE;

    if ((fabs(FVFTEX(pV0, dwTexOffs)->tu - FVFTEX(pV2, dwTexOffs)->tu) < EPS) &&
        (fabs(FVFTEX(pV0, dwTexOffs)->tv - FVFTEX(pV2, dwTexOffs)->tv) < EPS))
        return FALSE;

    if ((fabs(FVFTEX(pV2, dwTexOffs)->tu - FVFTEX(pV1, dwTexOffs)->tu) < EPS) &&
        (fabs(FVFTEX(pV2, dwTexOffs)->tv - FVFTEX(pV1, dwTexOffs)->tv) < EPS))
        return FALSE;

    return TRUE;
}

//************************************************************************
//
// void StepCalc
//
// Calculate the stepratio necessary to traverse an edge defined by
// pSmall and pBig (the relevant texture coordinates will be smaller
// in the former). If the relevant coordinates (determined by g_Flag)
// are singular (the edge is a point or is perpendicular to the relevant
// direction) we handle this as a special case by avoiding traversing it.
//
//************************************************************************
void StepCalc(S3_CONTEXT *pCtxt, int g_Flag,
              LPD3DTLVERTEX pBig, LPD3DTLVERTEX pSmall,
              _PRECISION *pStepRatio, _PRECISION *pActRatio,
              LPS3FVFOFFSETS lpS3FVFOff)
{
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;
    _PRECISION fTexDif;

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);

    if (g_Flag == UVCLIP_U) {
        if ((FVFTEX(pBig, dwTexOffs)->tu - FVFTEX(pSmall, dwTexOffs)->tu) > EPS) {
            fTexDif = FVFTEX(pBig, dwTexOffs)->tu - FVFTEX(pSmall, dwTexOffs)->tu;
            *pStepRatio = (_PRECISION)D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE /
                           (fTexDif*pCtxt->fTextureWidth );
            *pActRatio = 0.0f;
        } else {
            *pStepRatio = 1.0f;
            *pActRatio = 1.0f;
        }
    } else {
        if ((FVFTEX(pBig, dwTexOffs)->tv - FVFTEX(pSmall, dwTexOffs)->tv) > EPS) {
            fTexDif = FVFTEX(pBig, dwTexOffs)->tv - FVFTEX(pSmall, dwTexOffs)->tv;
            *pStepRatio = (_PRECISION)D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE /
                            (fTexDif*pCtxt->fTextureWidth );
            *pActRatio = 0.0f;
        } else {
            *pStepRatio = 1.0f;
            *pActRatio = 1.0f;
        }
    }
}

//************************************************************************
//
// void AdvanceRatios
//
// Here we advance each ratio associated to an edge of the triangle,
// allowing to divide it in perfectly vertical (or horizontal) stripes
// which then become trapezoids which are processed separately.
//
// Each ratio indicates how much have we advanced along an edge, so its
// value at the starting vertex is 0.0 and at the ending vertex is 1.0.
// We have to advance the ratios for both (current) edges so that the
// stripes are not slanted in any way (i.e., they are either 0 degress or
// 90 degrees each).
//
//************************************************************************
void AdvanceRatios(_PRECISION *pactratioSM,
                   _PRECISION stepratioSM,
                   _PRECISION *pactratioLG,
                   _PRECISION stepratioLG)
{
    // advance ratio for both edges
    *pactratioSM += stepratioSM;
    *pactratioLG += stepratioLG;

    if ((*pactratioSM >= 1.0f) && (*pactratioLG >= 1.0f)) {
        // both edges are overflowed, we are done
        *pactratioLG = 1.0f;
        *pactratioSM = 1.0f;
    } else if (*pactratioSM > 1.0f) {  // check overflow of the smaller edge
        // if so, adjust the largest edge to advance
        // as much as the smallest edge did
        *pactratioLG -= stepratioLG * (*pactratioSM - 1.0f) / stepratioSM;
        // advance smallest edge ratio to its maximum valid value
        *pactratioSM = 1.0f;
    } else if (*pactratioLG > 1.0f)
        *pactratioLG = 1.0f; // verify we haven't overflowed largest edge
}

//************************************************************************
//
// void ProcessTrapezoid
//
// We process a trapezoid stripe into triangles. The edge pVa0-pVa1 is
// parallel to the edge pVb0-pVb1. The distance from the edge pVa0-pVa1
// to the edge pVb0-pVb1 must be equal or less to the allowable UV range.
//
// We must beware in that the trapezoid might be degenerate: One of the
// following situations may arise when we are next to an (original) triangle
// vertex :
//
//         1) Va0 == Vb0
//         2) Va1 == Vb1
//         3) Va0 == Va1
//         4) Vb0 == Vb1
//
// When this function is called, however, we are guaranteed that any
// degeneration is only severe enough to create a triangle and not a line
// or a point
//
// We make use of code defined in macros (PROCESS_TRAPEZOID) in order that we
// don't repeat code for the UVCLIP_U and UVCLIP_V cases , which differ only
// which texturing coordinates (tu or tv) we must use to traverse the
// trapezoid
//************************************************************************

#define TRAPEZOID_CLIP_EDGE(pV0, pV1, tCoord)                               \
{                                                                           \
    /* To have the generated vertexes well ordered, we shall */             \
    /* proceed from pV0 (or any vertex generated by clipping */             \
    /* it, and then pV1. If pV1 falls exactly in the startUV */             \
    /* or stopUV border we will not place it and leave it to */             \
    /* the next edge to include it (then as pV0)             */             \
                                                                            \
    _PRECISION tpV0, tpV1;                                                  \
                                                                            \
    tpV0 = FVFTEX(pV0, dwTexOffs)->tCoord;                                  \
    tpV1 = FVFTEX(pV1, dwTexOffs)->tCoord;                                  \
                                                                            \
    b0Left  = (tpV0 < startUV);                                             \
    b0Right = (tpV0 > stopUV);                                              \
    b1Left  = (tpV1 < startUV);                                             \
    b1Right = (tpV1 > stopUV);                                              \
                                                                            \
    if ( b0Left && b1Left ) {                                               \
        /*edge is invisible , do not insert nothing into ClipVtx*/          \
    } else  if ( b0Right && b1Right ) {                                     \
        /*edge is invisible , do not insert nothing into ClipVtx*/          \
    } else  {                                                               \
                                                                            \
        if (b0Left) {                                                       \
            /*intersect pV0 with startUV unless its on the startUV border*/ \
            if ((tpV1 - startUV) > EPS) {                                   \
                ratio = (startUV - tpV0) / (tpV1 - tpV0);                   \
                CalcNewVertexProps(pCtxt, &ClipVtx[nVtxs].TLvtx, pV0, pV1,  \
                                ratio ,FlatColor, FlatSpecular, lpS3FVFOff);\
                nVtxs++;                                                    \
            }                                                               \
        } else if (b0Right) {                                               \
            /*intersect pV0 with stopUV unless its on the stopUV border*/   \
            if ((stopUV - tpV1) > EPS) {                                    \
                ratio = (tpV0 - stopUV) / (tpV0 - tpV1);                    \
                CalcNewVertexProps(pCtxt, &ClipVtx[nVtxs].TLvtx, pV0, pV1,  \
                                ratio ,FlatColor, FlatSpecular, lpS3FVFOff);\
                nVtxs++;                                                    \
            }                                                               \
        } else {                                                            \
            /* no clipping, just include the vertex */                      \
            __CpyFVFVertexes( &ClipVtx[nVtxs++].TLvtx , pV0, lpS3FVFOff );  \
        }                                                                   \
                                                                            \
        if (b1Left) {                                                       \
            /*intersect pV1 with startUV */                                 \
            ratio = (tpV0 - startUV) / (tpV0 - tpV1);                       \
            CalcNewVertexProps(pCtxt, &ClipVtx[nVtxs].TLvtx, pV0, pV1,      \
                               ratio ,FlatColor, FlatSpecular, lpS3FVFOff); \
            nVtxs++;                                                        \
        } else if (b1Right) {                                               \
            /*intersect pV1 with stopUV */                                  \
            ratio = (stopUV - tpV0) / (tpV1 - tpV0);                        \
            CalcNewVertexProps(pCtxt, &ClipVtx[nVtxs].TLvtx, pV0, pV1,      \
                                ratio ,FlatColor, FlatSpecular, lpS3FVFOff);\
            nVtxs++;                                                        \
        } else {                                                            \
            /* no clipping , and therefore we don't include it leaving */   \
            /* it to be processed in the next edge                     */   \
        }                                                                   \
    }                                                                       \
}

#define PROCESS_TRAPEZOID(tCoord)                                             \
{                                                                             \
    /* Get its base coord */                                                  \
    minUV = min4(FVFTEX(pVa0, dwTexOffs)->tCoord,                             \
                 FVFTEX(pVa1, dwTexOffs)->tCoord,                             \
                 FVFTEX(pVb0, dwTexOffs)->tCoord,                             \
                 FVFTEX(pVb1, dwTexOffs)->tCoord);                            \
                                                                              \
    maxUV = max4(FVFTEX(pVa0, dwTexOffs)->tCoord,                             \
                 FVFTEX(pVa1, dwTexOffs)->tCoord,                             \
                 FVFTEX(pVb0, dwTexOffs)->tCoord,                             \
                 FVFTEX(pVb1, dwTexOffs)->tCoord);                            \
                                                                              \
    /* we make sure we are correctly directed , we check for both */          \
    /* cases to avoid problems from degenerate cases , but we     */          \
    /* assume both segments are directed in the same way          */          \
    if ((FVFTEX(pVa0, dwTexOffs)->tCoord > FVFTEX(pVa1, dwTexOffs)->tCoord) ||\
        (FVFTEX(pVb0, dwTexOffs)->tCoord > FVFTEX(pVb1, dwTexOffs)->tCoord)) {\
        /* we invert both segments, they have both the same orienation */     \
        PTRSWAP((ULONG_PTR)pVa0,(ULONG_PTR)pVa1);                                \
        PTRSWAP((ULONG_PTR)pVb0,(ULONG_PTR)pVb1);                                \
    }                                                                         \
                                                                              \
    startUV = minUV;                                                          \
                                                                              \
    /* with just one loop we will traverse the whole trapezoid */             \
    do{                                                                       \
        /* advance window edge to process trapezoidal section */              \
        stopUV = startUV + stepvalue;                                         \
                                                                              \
        /* Now we are going to clip the trapezoid vs the      */              \
        /* valid texture window defined by startUV and stopUV */              \
        nVtxs = 0;                                                            \
                                                                              \
        TRAPEZOID_CLIP_EDGE(pVa0, pVa1, tCoord);                              \
        TRAPEZOID_CLIP_EDGE(pVa1, pVb1, tCoord);                              \
        TRAPEZOID_CLIP_EDGE(pVb1, pVb0, tCoord);                              \
        TRAPEZOID_CLIP_EDGE(pVb0, pVa0, tCoord);                              \
                                                                              \
        /* Render the resulting edge using HW triangles */                    \
        for (i = 1; i < nVtxs - 1 ; i++) {                                    \
            if (ValidTriangle(&ClipVtx[0].TLvtx,                              \
                              &ClipVtx[i].TLvtx,                              \
                              &ClipVtx[i+1].TLvtx,                            \
                              lpS3FVFOff))                                    \
                pRenderTriangle[pCtxt->dwRCode](pCtxt,&ClipVtx[0].TLvtx,      \
                                                      &ClipVtx[i].TLvtx,      \
                                                      &ClipVtx[i+1].TLvtx,    \
                                                      lpS3FVFOff);            \
        }                                                                     \
                                                                              \
        /* prepare for next iteration */                                      \
        startUV = stopUV;                                                     \
    } while(stopUV < maxUV);                                                  \
}

// Implementors note: Notice that we are using a structure capable of containing
//  a D3DTLVERTEX plus any additional texture coordinates valid in an FVF vertex
// (ClipVtx) to store any vertexes generated in the clipping phase of the 
// algorithm. This is because even when the Virge will not do multitexturing, we
// need to provide for any possible FVF case when copying!

void ProcessTrapezoid(S3_CONTEXT *pCtxt,
                      LPD3DTLVERTEX pVa0,
                      LPD3DTLVERTEX pVa1,
                      LPD3DTLVERTEX pVb0,
                      LPD3DTLVERTEX pVb1,
                      D3DCOLOR FlatColor,
                      D3DCOLOR FlatSpecular,
                      int nUVDirection,
                      LPS3FVFOFFSETS lpS3FVFOff)
{
    _PRECISION minUV, maxUV, stepvalue, startUV, stopUV, ratio;
    int nVtxs, i;
    int b0Left, b0Right, b1Left, b1Right;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;
    FVFVERTEX ClipVtx[8]; // in a trapezoid we won;t exceed 8 vertexes 

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);

    // The algorithm advances in the nUVDirection , creating trapezoidal
    // stripes. Here we must advance in the orthogonal direction to that
    if (nUVDirection == UVCLIP_U) {
        // Here the trapezoid is a slab in the V direction
        stepvalue = (_PRECISION)D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE /
                            pCtxt->fTextureHeight;
        PROCESS_TRAPEZOID(tv);
    } else {
        // Here the trapezoid is a slab in the U direction
        stepvalue = (_PRECISION)D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE /
                            pCtxt->fTextureWidth;
        PROCESS_TRAPEZOID(tu);    }
}

//************************************************************************
//
// BOOL TesselateInUVSpace
//
// We tesselate a triangle into subtriangles which don't exceed our
// required texel rendering constraints. Be aware that we only do this
// for our first texture coordinate pair, since the S3 Virge does not
// support multitexturing. If IHV hardware can do multitexturing and
// has texel rasterization limits, then the triangles that are the
// output of this stage could be processed again in the same fashion
// for the next set of texture coordinates, instead of being injected
// into the triangle hw renedering routine.
//
//************************************************************************
BOOL TesselateInUVSpace (S3_CONTEXT *pCtxt,
                         LPD3DTLVERTEX p0,
                         LPD3DTLVERTEX p1,
                         LPD3DTLVERTEX p2,
                         LPS3FVFOFFSETS   lpS3FVFOff)
{
    LPD3DTLVERTEX g_s[3];
    LPD3DTLVERTEX pV0,pV1,pV2;
    LPD3DTLVERTEX pVa0,pVa1,pVb0,pVb1;

    FVFVERTEX tempvtx[4];
    FVFVERTEX tvertex[4];

    D3DCOLOR FlatColor, FlatSpecular;
    int g_Flag, g_MaxInd, i;
    BOOL PerspectiveChange;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;

    _PRECISION actratio01, actratio02, actratio12;
    _PRECISION stepratio01, stepratio02, stepratio12;

    BOOL Alpha_workaround = pCtxt->Alpha_workaround;
    D3DTEXTUREADDRESS TextureAddress = pCtxt->TextureAddress;

    DPF_DBG("In TesselateInUVSpace");

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);

    PerspectiveChange = FALSE;

    // Copy the vertexes to temporary storage so that we may munge any data
    __CpyFVFVertexes( &tempvtx[0].TLvtx , p0, lpS3FVFOff);
    __CpyFVFVertexes( &tempvtx[1].TLvtx , p1, lpS3FVFOff);
    __CpyFVFVertexes( &tempvtx[2].TLvtx , p2, lpS3FVFOff);

    // Get pointers to current triangle data
    g_s[0] = &(tempvtx[0].TLvtx);
    g_s[1] = &(tempvtx[1].TLvtx);
    g_s[2] = &(tempvtx[2].TLvtx);

    // ID4 demo hang and MotoRacer demo hang without these checks.
    // One of the u or v value that passed in has a huge value which
    // causes the following tesselation algorithm to stay in an infinite loop.
    LIMIT_UV_HI(FVFTEX(g_s[0], dwTexOffs)->tu);
    LIMIT_UV_HI(FVFTEX(g_s[1], dwTexOffs)->tu);
    LIMIT_UV_HI(FVFTEX(g_s[2], dwTexOffs)->tu);
    LIMIT_UV_HI(FVFTEX(g_s[0], dwTexOffs)->tv);
    LIMIT_UV_HI(FVFTEX(g_s[1], dwTexOffs)->tv);
    LIMIT_UV_HI(FVFTEX(g_s[2], dwTexOffs)->tv);

    LIMIT_UV_LO(FVFTEX(g_s[0], dwTexOffs)->tu);
    LIMIT_UV_LO(FVFTEX(g_s[1], dwTexOffs)->tu);
    LIMIT_UV_LO(FVFTEX(g_s[2], dwTexOffs)->tu);
    LIMIT_UV_LO(FVFTEX(g_s[0], dwTexOffs)->tv);
    LIMIT_UV_LO(FVFTEX(g_s[1], dwTexOffs)->tv);
    LIMIT_UV_LO(FVFTEX(g_s[2], dwTexOffs)->tv);

    // if we are to do perspective correction, but the w values make us avoid
    // it we avoid it and skip even the triangle tesselation!
    if( (pCtxt->dwRCode & S3PERSPECTIVE) &&
        ( g_s[0]->rhw == g_s[1]->rhw) &&
        ( g_s[1]->rhw == g_s[2]->rhw )) {
        pCtxt->dwRCode &= ~S3PERSPECTIVE;
        pCtxt->rndCommand &= ~(cmdCMD_TYPE_UnlitTexPersp - cmdCMD_TYPE_UnlitTex);
        pCtxt->dTexValtoInt = dTexValtoInt[pCtxt->rsfMaxMipMapLevel];
        PerspectiveChange = TRUE;
    }

    // If we are just flat shading, we will avoid calculating the
    // intermediate vertex colors, so we store beforehand the flat colors
    if(!(pCtxt->ShadeMode == D3DSHADE_GOURAUD)) {
        if (dwColorOffs) {
            FlatColor = FVFCOLOR(g_s[2], dwColorOffs)->color =
                        FVFCOLOR(g_s[1], dwColorOffs)->color =
                        FVFCOLOR(g_s[0], dwColorOffs)->color;
        } else {
            FlatColor = 0;
        }

        if (dwSpecOffs) {
            FlatSpecular = FVFSPEC(g_s[2], dwSpecOffs)->specular =
                           FVFSPEC(g_s[1], dwSpecOffs)->specular =
                           FVFSPEC(g_s[0], dwSpecOffs)->specular;
        } else {
            FlatSpecular = 0;
        }

    }

    if(Alpha_workaround) {
        if (dwColorOffs) {
            FVFCOLOR(g_s[0], dwColorOffs)->color |= 0xff000000;
            FVFCOLOR(g_s[1], dwColorOffs)->color |= 0xff000000;
            FVFCOLOR(g_s[2], dwColorOffs)->color |= 0xff000000;
        }
        pCtxt->Alpha_workaround=FALSE;
    }

    // We will do any wrapping/clamping before sending the tesselated vertexes
    // down the pipeline.
    pCtxt->TextureAddress = 0;

    if( TextureAddress == D3DTADDRESS_WRAP ) {
        // address wrap
        if ( pCtxt->bWrapU )
            MYWRAP( FVFTEX(g_s[0], dwTexOffs)->tu,
                    FVFTEX(g_s[1], dwTexOffs)->tu,
                    FVFTEX(g_s[2], dwTexOffs)->tu);

        if ( pCtxt->bWrapV )
            MYWRAP( FVFTEX(g_s[0], dwTexOffs)->tv,
                    FVFTEX(g_s[1], dwTexOffs)->tv,
                    FVFTEX(g_s[2], dwTexOffs)->tv);

    } else if ( TextureAddress == D3DTADDRESS_CLAMP ) {
        for ( i = 0; i < 3; i++ )
        {
            if ( FVFTEX(g_s[i], dwTexOffs)->tu > (float)1.0f ) {
                FVFTEX(g_s[i], dwTexOffs)->tu = (float)1.0f ;
            } else if ( FVFTEX(g_s[i], dwTexOffs)->tu < (float)0.0f ) {
                FVFTEX(g_s[i], dwTexOffs)->tu = (float)0.0f ;
            }

            if ( FVFTEX(g_s[i], dwTexOffs)->tv > (float)1.0f ) {
                FVFTEX(g_s[i], dwTexOffs)->tv = (float)1.0f ;
            } else if ( FVFTEX(g_s[i], dwTexOffs)->tv < (float)0.0f ) {
                FVFTEX(g_s[i], dwTexOffs)->tv = (float)0.0f ;
            }
        }
    }

    // If the triangle doesnt exceed the texel limit we will avoid
    // tesselating it
    if( (g_Flag = OutOfUVRange( g_s, pCtxt->fTextureWidth,
        pCtxt->fTextureHeight, &g_MaxInd,pCtxt, lpS3FVFOff )) == 0) {
        //Culling is done already
        pRenderTriangle[pCtxt->dwRCode](pCtxt,g_s[0], g_s[1], g_s[2], lpS3FVFOff);
        goto ContextRestore;
    }

    // Flag to indicate the triangle hw rasterization function that the triangle was
    // previously tesselated, i.e., it comes from this process.
    pCtxt->bChopped = TRUE;

    // we make sure that the longest edge in UV space is 02 and that
    // the vertexes are ordered from smallest to largest uv value
    // from 0 to 2.
    if (g_MaxInd == 0) {
        PTRSWAP((ULONG_PTR)g_s[1], (ULONG_PTR)g_s[2]); // max edge was 01, so swap
                                                    // pointers to vtxs 1&2
    } else if (g_MaxInd == 1) {
        PTRSWAP((ULONG_PTR)g_s[0], (ULONG_PTR)g_s[1]); // max edge was 12, so swap
                                                    // pointers to vtxs 0&1
    } else if (g_MaxInd == 2) {
        ;                        //max edge was 20 , so don't swap anything
    }

    if (g_Flag == UVCLIP_U) {
        if (FVFTEX(g_s[0], dwTexOffs)->tu > FVFTEX(g_s[2], dwTexOffs)->tu)
            //if vtx 0 is larger than vtx 2 ,swap
            PTRSWAP((ULONG_PTR)g_s[0], (ULONG_PTR)g_s[2]);
    } else {
        if (FVFTEX(g_s[0], dwTexOffs)->tv > FVFTEX(g_s[2], dwTexOffs)->tv)
            //if vtx 0 is larger than vtx 2 ,swap
            PTRSWAP((ULONG_PTR)g_s[0], (ULONG_PTR)g_s[2]);
    }

    // Calculate the stepratio for each edge so that we advance in a
    // proportional way along all edges creating horizontal or vertical
    // trapezoids. At the very least, stepratio02 must be < 1.0 .
    StepCalc(pCtxt, g_Flag, g_s[1], g_s[0], &stepratio01, &actratio01, lpS3FVFOff);
    StepCalc(pCtxt, g_Flag, g_s[2], g_s[0], &stepratio02, &actratio02, lpS3FVFOff);
    StepCalc(pCtxt, g_Flag, g_s[2], g_s[1], &stepratio12, &actratio12, lpS3FVFOff);

    // Set pV0, pV1, pV2 to the vertexes ordered from smallest to largest u|v
    pV0 = g_s[0];
    pV1 = g_s[1];
    pV2 = g_s[2];

    // Prepare working vertex pointers for trapezoids
    pVa0 = &(tvertex[0].TLvtx);
    pVa1 = &(tvertex[1].TLvtx);
    pVb0 = &(tvertex[2].TLvtx);
    pVb1 = &(tvertex[3].TLvtx);

    // Our first vertexes will be equal to pV0
    __CpyFVFVertexes( pVa0, g_s[0], lpS3FVFOff);
    __CpyFVFVertexes( pVa1, g_s[0], lpS3FVFOff);

    // Process first half of the triangle from vertex 0 to vertex 1
    DPF_DBG("Processing 1st half");
    while (actratio01 < 1.0f) {
        // calculate ratios associated to next trapezoidal stripe (horz|vert)
        AdvanceRatios(&actratio01, stepratio01, &actratio02, stepratio02);
        // calculate new associated vertexes & their properties
        CalcNewVertexProps(pCtxt, pVb1, pV0, pV1, actratio01,
                            FlatColor, FlatSpecular, lpS3FVFOff);
        CalcNewVertexProps(pCtxt, pVb0, pV0, pV2, actratio02,
                            FlatColor, FlatSpecular, lpS3FVFOff);
        // subprocess trapezoidal stripe into triangles
        ProcessTrapezoid(pCtxt, pVa0, pVa1, pVb0, pVb1, FlatColor,
                         FlatSpecular, g_Flag, lpS3FVFOff);
        // prepare for next iteration
        PTRSWAP((ULONG_PTR)pVa1,(ULONG_PTR)pVb1);
        PTRSWAP((ULONG_PTR)pVa0,(ULONG_PTR)pVb0);
    }

    // Now we have to update pVa1 with our vertex 1 (specially for triangles
    // which start of with a vertical edge
    __CpyFVFVertexes( pVa1, g_s[1], lpS3FVFOff);

    // Process second half of the triangle from vertex 1 to vertex 2
    DPF_DBG("Processing 2nd half");
    while (actratio02 < 1.0f) {
        // calculate ratios associated to next trapezoidal stripe (horz|vert)
        AdvanceRatios(&actratio12, stepratio12, &actratio02, stepratio02);
        // calculate new associated vertexes & their properties
        CalcNewVertexProps(pCtxt, pVb1, pV1, pV2, actratio12,
                            FlatColor, FlatSpecular, lpS3FVFOff);
        CalcNewVertexProps(pCtxt, pVb0, pV0, pV2, actratio02,
                            FlatColor, FlatSpecular, lpS3FVFOff);
        // subprocess trapezoidal stripe into triangles
        ProcessTrapezoid(pCtxt, pVa0, pVa1, pVb0, pVb1, FlatColor,
                         FlatSpecular, g_Flag, lpS3FVFOff);
        // prepare for next iteration
        PTRSWAP((ULONG_PTR)pVa1,(ULONG_PTR)pVb1);
        PTRSWAP((ULONG_PTR)pVa0,(ULONG_PTR)pVb0);
    }

ContextRestore:
    if ( PerspectiveChange ) {
        pCtxt->dwRCode |= S3PERSPECTIVE;
        pCtxt->dTexValtoInt = dTexValtoIntPerspective[pCtxt->rsfMaxMipMapLevel];
        pCtxt->rndCommand  |= (cmdCMD_TYPE_UnlitTexPersp - cmdCMD_TYPE_UnlitTex);
    }

    // Restore pre-tesselation values
    pCtxt->bChopped = FALSE;
    pCtxt->TextureAddress = TextureAddress;
    pCtxt->Alpha_workaround = Alpha_workaround;
    return FALSE;  //not Culled
}

