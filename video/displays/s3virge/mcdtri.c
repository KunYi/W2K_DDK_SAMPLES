/******************************Module*Header*******************************\
* Module Name: mcdtri.c
*
* Contains the low-level (rasterization) triangle-rendering routines for the
* S3 Virge MCD driver.
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#include "hw3d.h"
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"

#define WSCALE  ((MCDFLOAT)2048.0)      // normalize w's to within this range to
                                        // provide maximum precision in h/w

#define MCDDIVEPS    ((MCDFLOAT)(1.0e-4))
#define MCDUVEPSILON ((MCDFLOAT)(1.0e-10))

//replaced with ppdev->uvMaxTexels to accomondate Virge DX, GX, MX and GX-2.
//#define UVMAXTEXELS  ((MCDFLOAT)(128.0))

#define MOVE_XDER_BACK 1  // Set to 1 to calculate x derivates before any triangle
                          // splitting, since it is invariant across the whole
                          // triangle. Unfortunately, the y derivates in this
                          // implementation for the S3 Virge require to be along
                          // the longest edge, so we cannot do the same for them.
                          // Also, perspective-corrected textured triangles
                          // require a particular derivate calculation so it is
                          // not used in such a case.

//**************************************************************************
//
// VOID FASTCALL __MCDCalcXDeltaProps
//
// Compute all relevant deltas (r,g,b,a,z,u,v,w,d) according to the state
//
//**************************************************************************
VOID FASTCALL __MCDCalcXDeltaProps(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                  MCDVERTEX *c)
{
    MCDFLOAT oneOverArea;

    // We check if we need to calculate the inverse of the area for any
    // x derivate
    if (pRc->privateEnables & (__MCDENABLE_SMOOTH  |
                               __MCDENABLE_Z       |
                               __MCDENABLE_TEXTURED|
                               __MCDENABLE_BLEND)) {
        // Pre-compute one over polygon half-area
        pRc->invHalfArea = __MCDONE / FABS(pRc->halfArea);
        oneOverArea = pRc->invHalfArea;
    }

    if (pRc->privateEnables & __MCDENABLE_COLORED) {

        if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
            X_CALC_PROP_DELTA(dr, colors[0].r, pRc->rScale);
            X_CALC_PROP_DELTA(dg, colors[0].g, pRc->gScale);
            X_CALC_PROP_DELTA(db, colors[0].b, pRc->bScale);
        }

        if (pRc->privateEnables & __MCDENABLE_BLEND) {
            X_CALC_PROP_DELTA(da, colors[0].a, pRc->aScale);
        }
    }

    if (pRc->privateEnables & __MCDENABLE_Z) {
        X_CALC_PROP_DELTA_ASM(dz, windowCoord.z, pRc->zScale);
    }

    if (pRc->privateEnables & __MCDENABLE_TEXTURED) {

        if (pRc->privateEnables & __MCDENABLE_TEXPERSP) {
            X_CALC_PROP_DELTA_ASM(dw, windowCoord.w, pRc->wScale);
            X_CALC_PROP_DELTA_W_BASE(du,texCoord.x,windowCoord.w,pRc->uBase, pRc->uvScale);
            X_CALC_PROP_DELTA_W_BASE(dv,texCoord.y,windowCoord.w,pRc->vBase, pRc->uvScale);
        } else {
            // no perspective correction
            X_CALC_PROP_DELTA_ASM(du, texCoord.x, pRc->uvScale);
            X_CALC_PROP_DELTA_ASM(dv, texCoord.y, pRc->uvScale);
            // for mipmapping (without perspective correction)
            if (pRc->privateEnables & __MCDENABLE_TEXMIPMAP) {
                X_CALC_PROP_DELTA_ASM(dw, windowCoord.w, __MCDONE);
            }
        }
    }
}

//**************************************************************************
//
// VOID FASTCALL __MCDCalcYDeltaProps
//
// Compute all relevant deltas (r,g,b,a,z,u,v,w,d) according to the state
//
//**************************************************************************
VOID FASTCALL __MCDCalcYDeltaProps(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                  MCDVERTEX *c)
{
    if (pRc->privateEnables & __MCDENABLE_COLORED) {

        if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
            Y_CALC_PROP_DELTA(dr, colors[0].r, pRc->rScale);
            Y_CALC_PROP_DELTA(dg, colors[0].g, pRc->gScale);
            Y_CALC_PROP_DELTA(db, colors[0].b, pRc->bScale);
        }

        if (pRc->privateEnables & __MCDENABLE_BLEND) {
            Y_CALC_PROP_DELTA(da, colors[0].a, pRc->aScale);
        }
    }

    if (pRc->privateEnables & __MCDENABLE_Z) {
        Y_CALC_PROP_DELTA(dz, windowCoord.z, pRc->zScale);
    }

    if (pRc->privateEnables & __MCDENABLE_TEXTURED) {

        if (pRc->privateEnables & __MCDENABLE_TEXPERSP) {
            Y_CALC_PROP_DELTA(dw, windowCoord.w, pRc->wScale);
            Y_CALC_PROP_DELTA_W_BASE(du,texCoord.x,windowCoord.w,pRc->uBase, pRc->uvScale);
            Y_CALC_PROP_DELTA_W_BASE(dv,texCoord.y,windowCoord.w,pRc->vBase, pRc->uvScale);
        } else {
            // no perspective correction
            Y_CALC_PROP_DELTA(du, texCoord.x, pRc->uvScale);
            Y_CALC_PROP_DELTA(dv, texCoord.y, pRc->uvScale);
            // for mipmapping (without perspective correction)
            if (pRc->privateEnables & __MCDENABLE_TEXMIPMAP) {
                Y_CALC_PROP_DELTA(dw, windowCoord.w, __MCDONE);
            }
        }
    }
}

#define X_DELTA_FLIP(Prop)                       \
          pRc->Prop##dx     = - pRc->Prop##dx;   \
          pRc->fx##Prop##dx = - pRc->fx##Prop##dx

//**************************************************************************
//
// VOID FASTCALL __MCDFlipXDeltaProps
//
// Change signs of all relevant deltas (r,g,b,a,z,u,v,w,d) according to the state
//
//**************************************************************************
VOID FASTCALL __MCDFlipXDeltaProps(DEVRC *pRc)
{
    if (pRc->privateEnables & __MCDENABLE_COLORED) {

        if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
            X_DELTA_FLIP(dr);
            X_DELTA_FLIP(dg);
            X_DELTA_FLIP(db);
        }

        if (pRc->privateEnables & __MCDENABLE_BLEND) {
            X_DELTA_FLIP(da);
        }
    }

    if (pRc->privateEnables & __MCDENABLE_Z) {
        X_DELTA_FLIP(dz);
    }
}


//**************************************************************************
//
// MCDFLOAT FASTCALL __AdjustVertexPosition
//
// Sub-pixel adjust vertex XY position only
//
//**************************************************************************
MCDFLOAT FASTCALL __AdjustVertexPosition(MCDVERTEX *p, LONG iyVal,
                                         MCDFLOAT dxdy_slope)
{
    MCDFLOAT dx, dy, xNew;

    dy = iyVal - (p->windowCoord.y - __MCDHALF);
    dx = dy * dxdy_slope;
    xNew = p->windowCoord.x -  dx;
    return xNew;
}


//**************************************************************************
//
// VOID HWSelectTexFilter
//
// Setup the appropriate texturing filter
//
//**************************************************************************
VOID HWSelectTexFilter(DEVRC *pRc,ULONG filter, BOOL bMipMappingOn)
{
    // First clear the bits so that any previous filtering
    // setting will not interfere with it

    pRc->hwTexFunc &= ~S3D_TEXTURE_FILTER_RESERVED;

    if (bMipMappingOn) {
        switch (filter) {
            case GL_NEAREST_MIPMAP_NEAREST:
                    pRc->hwTexFunc |= S3D_TEXTURE_FILTER_MIP_NEAREST;
                    break ;
            case GL_LINEAR_MIPMAP_NEAREST:
                    pRc->hwTexFunc |= S3D_TEXTURE_FILTER_MIP_LINEAR;
                    break ;
            case GL_NEAREST_MIPMAP_LINEAR:
                    pRc->hwTexFunc |= S3D_TEXTURE_FILTER_LINEAR_MIP_NEAREST;
                    break ;
            case GL_LINEAR_MIPMAP_LINEAR:
                    pRc->hwTexFunc |= S3D_TEXTURE_FILTER_LINEAR_MIP_LINEAR;
                    break ;
            default:
                    break;
        }
    } else {
        switch (filter) {
            case GL_NEAREST:
                    pRc->hwTexFunc |= S3D_TEXTURE_FILTER_NEAREST;
                    break;
            case GL_LINEAR:
                    pRc->hwTexFunc |= S3D_TEXTURE_FILTER_LINEAR;
                    break ;
            default:
                    break;
            }
    }
}

//**************************************************************************
//
// VOID FASTCALL __HWTexPerspPrepare
//
// Finds out the necessary u,v base values for perspective corrected texturing
//
//**************************************************************************
VOID FASTCALL __HWTexPerspPrepare(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                    MCDVERTEX *c)
{
    MCDFLOAT uMin, vMin, wMax, wScale;

    uMin = a->texCoord.x;
    if (uMin > b->texCoord.x) uMin = b->texCoord.x;
    if (uMin > c->texCoord.x) uMin = c->texCoord.x;

    vMin = a->texCoord.y;
    if (vMin > b->texCoord.y) vMin = b->texCoord.y;
    if (vMin > c->texCoord.y) vMin = c->texCoord.y;

    wMax = a->windowCoord.w;
    if (wMax < b->windowCoord.w) wMax = b->windowCoord.w;
    if (wMax < c->windowCoord.w) wMax = c->windowCoord.w;

    // u and v base register can only take care of integer part of u and
    // v value. 
    if (uMin < __MCDZERO)
        pRc->uBase = (MCDFLOAT)FTOL(uMin * pRc->texwidth - __MCDONE) / 
                     pRc->texwidth;
    else
        pRc->uBase = (MCDFLOAT)FTOL(uMin * pRc->texwidth) / pRc->texwidth;
    if (vMin < __MCDZERO)
        pRc->vBase = (MCDFLOAT)FTOL(vMin * pRc->texwidth - __MCDONE) / 
                     pRc->texwidth;
    else
        pRc->vBase = (MCDFLOAT)FTOL(vMin * pRc->texwidth) / pRc->texwidth;


    wScale = WSCALE / wMax;
    pRc->uvScale    *= wScale;
    pRc->wScale     *= wScale;
}

//**************************************************************************
//
// Data Tables for fast logarithm function
//
// Limit = POW2(i)*POW2(i) = 2^(2n)
//
//**************************************************************************
MCDFLOAT powersOfFour[10] = {(MCDFLOAT)1.0,
                             (MCDFLOAT)4.0,
                             (MCDFLOAT)16.0,
                             (MCDFLOAT)64.0,
                             (MCDFLOAT)256.0,
                             (MCDFLOAT)1024.0,
                             (MCDFLOAT)4096.0,
                             (MCDFLOAT)16384.0,
                             (MCDFLOAT)65536.0,
                             (MCDFLOAT)262144.0 };

//**************************************************************************
//
// Data Tables for fast logarithm function
//
// 2^(-n)
//
//**************************************************************************
MCDFLOAT negPowerOfTwo[32] ={(MCDFLOAT) 1.0,
                             (MCDFLOAT) 0.5,
                             (MCDFLOAT) 0.25,
                             (MCDFLOAT) 0.125,
                             (MCDFLOAT) 0.0625,
                             (MCDFLOAT) 0.03125,
                             (MCDFLOAT) 0.015625 ,
                             (MCDFLOAT) 0.0078125 ,
                             (MCDFLOAT) 0.00390625,
                             (MCDFLOAT) 0.001953125,
                             (MCDFLOAT) 0.0009765625,
                             (MCDFLOAT) 0.00048828125,
                             (MCDFLOAT) 0.000244140625,
                             (MCDFLOAT) 0.0001220703125,
                             (MCDFLOAT) 0.00006103515625,
                             (MCDFLOAT) 0.000030517578125,
                             (MCDFLOAT) 1.52587890625e-5,
                             (MCDFLOAT) 7.62939453125e-6,
                             (MCDFLOAT) 3.814697265625e-6,
                             (MCDFLOAT) 1.907348632813e-6,
                             (MCDFLOAT) 9.536743164063e-7,
                             (MCDFLOAT) 4.768371582031e-7,
                             (MCDFLOAT) 2.384185791016e-7,
                             (MCDFLOAT) 1.192092895508e-7,
                             (MCDFLOAT) 5.960464477539e-8,
                             (MCDFLOAT) 2.98023223877e-8,
                             (MCDFLOAT) 1.490116119385e-8,
                             (MCDFLOAT) 7.450580596924e-9,
                             (MCDFLOAT) 3.725290298462e-9,
                             (MCDFLOAT) 1.862645149231e-9,
                             (MCDFLOAT) 9.313225746155e-10,
                             (MCDFLOAT) 4.656612873077e-10};

//**************************************************************************
//
// MCDFLOAT __MCDCalcMipMapLevel
//
// Calculate the appropriate mip map level for a vertex
//
//**************************************************************************
MCDFLOAT __MCDCalcMipMapLevel(DEVRC *pRc, MCDVERTEX *p, MCDFLOAT cval,
                              MCDFLOAT _dwdx, MCDFLOAT _dwdy)
{
    MCDFLOAT dx, dy, _dsdx, _dsdy, _dtdx, _dtdy, invW, rho;
    LONG irho, lev;

    // Calculate local rho for this particular vertex
    // its square root is the appropriate mip map level to select
    // all this derivatives (as _dsdx) are only multiplied by
    // pRc->uvScale if no perspective correction is in force

    if (FABS(p->windowCoord.w) > MCDDIVEPS)
        invW = __MCDONE / p->windowCoord.w;
    else
        invW = __MCDONE / MCDDIVEPS;

    _dsdx = invW * (pRc->dsdx - p->texCoord.x * _dwdx);
    _dsdy = invW * (pRc->dsdy - p->texCoord.x * _dwdy);
    _dtdx = invW * (pRc->dtdx - p->texCoord.y * _dwdx);
    _dtdy = invW * (pRc->dtdy - p->texCoord.y * _dwdy);

    dx = _dsdx * _dsdx + _dtdx * _dtdx;
    dy = _dsdy * _dsdy + _dtdy * _dtdy;
    rho = max (dx,dy) * pRc->texwidth * pRc->texwidth;

    if (rho > powersOfFour[pRc->texwidthLog2])
        p->texCoord.z = (MCDFLOAT)pRc->texwidthLog2;
    else {
        //fast log2 (square root(rho))
        irho = FTOL(rho);
        lev  = 0;
        while (irho >>= 1) lev++;
        p->texCoord.z =
            (lev + ( rho * negPowerOfTwo[lev] - __MCDONE ) ) *__MCDHALF;
    }

    if (p->texCoord.z < cval) p->texCoord.z = cval;

    MCDBG_PRINT(DBG_TRI,"rho = %i _dsdx = %i _dsdy = %i _dtdx=%i _dtdy=%i",
               (long)(rho * 10),(long)(_dsdx * 1000),(long)(_dsdy * 1000),
               (long)(_dtdx * 1000),(long)(_dtdy * 1000));

    return (rho);
}

//**************************************************************************
//
// VOID __MCDSetupTexFilter
//
// Calculate the appropriate mip map level for each vertex of a triangle
// Even if mip mapping is not enabled, we need this to select between the
// minification and the magnification filters
//
//**************************************************************************
VOID __MCDSetupTexFilter(DEVRC *pRc,MCDVERTEX *a, MCDVERTEX *b,MCDVERTEX *c,
                         BOOL bTrust)
{
    MCDFLOAT ro_sqr, c_value, aRho, bRho, cRho, _dwdx, _dwdy;
    BOOL     bChooseMagFilter;
    MCDFLOAT oneOverArea, filterLimit;

    // Fast path: no mipmapping and both filters are equal then just
    // set the filter and get out
    if (!(pRc->privateEnables & __MCDENABLE_TEXMIPMAP) &&
         (pRc->texMagFilter == pRc->texMinFilter)) {
        HWSelectTexFilter(pRc, pRc->texMinFilter, FALSE);

        return;
    }

    // Note that the OpenGL 1.1 spec says that you should
    // compare lambda  against c , where lambda = log2(ro)
    // Notice that lambda = ln(ro)/(ln 2) = ln(ro*ro)/(2ln2)
    // (ro*ro == ro_sqr).
    //
    // c should be chosen as
    //
    //  c == 0.5 (if the MAG_FILTER == LINEAR &&
    //                    (MIN_FILTER == NEAREST_MIPMAP_NEAREST ||
    //                     MIN_FILTER == LINEAR_MIPMAP_NEAREST)   )
    //
    //      so that the comparison which needs to be made is
    //             lambda > c
    //      =>     1/(2.0*ln 2.0) * ln(ro_sqr) > 0.5
    //      =>     ln(ro_sqr) > ln 2.0
    //      =>     ro_sqr > 2.0
    //
    //      the last being valid because ro_sqr is ALWAYS >= 0.0
    //          due to its definition and e > 1.0. The other case
    //          of c is
    //
    //  c == 0.0 (otherwise)
    //
    //             lambda > c
    //      =>     1/(2.0 *ln 2.0) * ln(ro_sqr) > 0.0
    //      =>     ln(ro_sqr) > 0.0
    //      =>     ro_sqr     > 1.0

    // We recalculate dsdx,dsdy,dtdx,dtdy,dwdx,dwdy because the method necessary
    // to setup the S3Virge registers is different than what is mathematically correct!
    START_CALC_PROP_DELTA_W_BASE(ds, texCoord.x , windowCoord.w, __MCDZERO);
    START_CALC_PROP_DELTA_W_BASE(dt, texCoord.y , windowCoord.w, __MCDZERO);

#if MOVE_XDER_BACK
    // This is a tesselated triangle, we need therefore to
    // recalculate the inverse of its area!
    if (!bTrust)
        pRc->invHalfArea = __MCDONE / FABS(pRc->halfArea);
#endif

    pRc->dsdx = (pRc->dsAC * pRc->dyAB - pRc->dsAB * pRc->dyAC) * pRc->invHalfArea;
    pRc->dtdx = (pRc->dtAC * pRc->dyAB - pRc->dtAB * pRc->dyAC) * pRc->invHalfArea;
    pRc->dsdy = (pRc->dsAB * pRc->dxAC - pRc->dsAC * pRc->dxAB) * pRc->invHalfArea;
    pRc->dtdy = (pRc->dtAB * pRc->dxAC - pRc->dtAC * pRc->dxAB) * pRc->invHalfArea;
    _dwdx = (pRc->dwAC * pRc->dyAB - pRc->dwAB * pRc->dyAC) * pRc->invHalfArea;
    _dwdy = (pRc->dwAB * pRc->dxAC - pRc->dwAC * pRc->dxAB) * pRc->invHalfArea;


    MCDBG_PRINT(DBG_TRI,"dsdx = %i dsdy = %i dtdx=%i dtdy=%i dwdx=%i dwdy=%i",
               (long)(pRc->dsdx*1000),(long)(pRc->dsdy*1000),
               (long)(pRc->dtdx*1000),(long)(pRc->dtdy*1000),
               (long)(pRc->dwdx*1000),(long)(pRc->dwdyEdge*1000));

    // Determine filtering limit to choose minification or magnification

    // DRIVER WRITERS:
    // OpenGL requires us to be able to choose the appropriate
    // filter according to the below computed rho factor. However in the S3Virge
    // we can only set one filter for the whole triangle, so we strive to choose
    // the minification filter (which will probably be of better quality) by
    // looking for the maximum rho of each vertex. This choice enables us to pass
    // OpenGL conformance.

    if ((pRc->texMagFilter == GL_LINEAR) &&
        ((pRc->texMinFilter == GL_NEAREST_MIPMAP_NEAREST) ||
         (pRc->texMinFilter == GL_LINEAR_MIPMAP_NEAREST))) {
        filterLimit = (MCDFLOAT)2.0;
        c_value = __MCDHALF;
    } else {
        filterLimit = (MCDFLOAT)1.0;
        c_value = __MCDZERO;
    }

    //Set the texCoord.z of the vertexes equal
    //to the calculated mip map levels for them
    //and get rho to determine minification/magnification choice
    aRho = __MCDCalcMipMapLevel(pRc,a,c_value,_dwdx,_dwdy);
    bRho = __MCDCalcMipMapLevel(pRc,b,c_value,_dwdx,_dwdy);
    cRho = __MCDCalcMipMapLevel(pRc,c,c_value,_dwdx,_dwdy);

    MCDBG_PRINT(DBG_VERT,"a->texCoord.z=%i",FTOL(a->texCoord.z * (MCDFLOAT)1000.0));
    MCDBG_PRINT(DBG_VERT,"b->texCoord.z=%i",FTOL(b->texCoord.z * (MCDFLOAT)1000.0));
    MCDBG_PRINT(DBG_VERT,"c->texCoord.z=%i",FTOL(c->texCoord.z * (MCDFLOAT)1000.0));

    // Get max(rho) since we cannot select the filter on a
    // per-pixel basis, only on a per-triangle basis.
    // Usually we would rather want to be able to specify
    // separate filters for magnification & minification
    // SIMULTANEOUSLY
    ro_sqr = aRho;
    if (bRho > ro_sqr) ro_sqr = bRho;
    if (cRho > ro_sqr) ro_sqr = cRho;

    bChooseMagFilter = ( ro_sqr <= filterLimit);

    if (bChooseMagFilter) {
        HWSelectTexFilter(pRc, pRc->texMagFilter, FALSE);
        pRc->fxdddx     = 0; // No mip mapping
        pRc->fxdddyEdge = 0; // No mip mapping
    } else {

        if ((pRc->privateEnables & __MCDENABLE_TEXMIPMAP)) {
            HWSelectTexFilter(pRc, pRc->texMinFilter, TRUE);
            oneOverArea = pRc->invHalfArea;
            X_CALC_PROP_DELTA(dd, texCoord.z, pRc->dScale);
            Y_CALC_PROP_DELTA(dd, texCoord.z, pRc->dScale);
        } else
            HWSelectTexFilter(pRc, pRc->texMinFilter, FALSE);
    }
}

//**************************************************************************
//
// VOID FASTCALL __MCDCalcCoordDeltas
//
// Recalculate all deltas between vertexes and the halfArea
// This is necessary when after tesselation or left edge window clipping
// we can't trust the values calculated before for these quantities
//
//**************************************************************************
VOID FASTCALL __MCDCalcCoordDeltas(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                    MCDVERTEX *c, BOOL *bCCW)
{
    //   Later the inverse of dyAC will be needed so start computing it
    pRc->dyAC = c->windowCoord.y - a->windowCoord.y;
    pRc->dxAC = c->windowCoord.x - a->windowCoord.x;
    pRc->dxBC = c->windowCoord.x - b->windowCoord.x;
    pRc->dxAB = b->windowCoord.x - a->windowCoord.x;
    pRc->dyBC = c->windowCoord.y - b->windowCoord.y;
    pRc->dyAB = b->windowCoord.y - a->windowCoord.y;

    // Should not trust the bCCW and the pRc->halfArea
    // field because this is a textured triangle which was tesselated
    // in order to not span the maximimum number of permitted texels

    pRc->halfArea = pRc->dxAC * pRc->dyBC - pRc->dxBC * pRc->dyAC;
    *bCCW = !__MCD_FLOAT_LTZ(pRc->halfArea);
}


//**************************************************************************
//
// VOID DbgTessInit
//
// Textured triangle tesselation debugging code. Each time we get a
// triangle for which bTrust is FALSE (an indication that it may be
// a tesselated triangle) we modify the state and the triangle to render
// just a gouraud shaded triangle in one of the primary(RGB) or
// secondary(CMY) colors. This code is not correct in all cases and
// states and is used ONLY as a visual debugging aid. iTessCount is
// a variable only used for debugging and it helps us to change the
// color among succesive triangles for easy visual identification.
//
//**************************************************************************
#if DBG_TESSELATION

VOID DbgTessInit(DEVRC *pRc,MCDVERTEX *a,MCDVERTEX *b, MCDVERTEX *c)
{
    if (iTessCount >= 0) {
        // For debugging purpouses show tesselated triangles as flat shaded
        // with varying colors
        static MCDFLOAT Tess_r [] = {(MCDFLOAT)255.0,(MCDFLOAT)  0.0,
                                     (MCDFLOAT)  0.0,(MCDFLOAT)255.0,
                                     (MCDFLOAT)255.0,(MCDFLOAT)  0.0,
                                                     (MCDFLOAT)255.0};
        static MCDFLOAT Tess_g [] = {(MCDFLOAT)  0.0,(MCDFLOAT)255.0,
                                     (MCDFLOAT)  0.0,(MCDFLOAT)255.0,
                                     (MCDFLOAT)  0.0,(MCDFLOAT)255.0,
                                                     (MCDFLOAT)255.0};
        static MCDFLOAT Tess_b [] = {(MCDFLOAT)  0.0,(MCDFLOAT)  0.0,
                                     (MCDFLOAT)255.0,(MCDFLOAT)  0.0,
                                     (MCDFLOAT)255.0,(MCDFLOAT)255.0,
                                                     (MCDFLOAT)255.0};

        // This is only for testing - beware it munges the vertexes colors!
        pRc->privateEnables &= ~__MCDENABLE_TEXTURED;
        pRc->privateEnables &= ~__MCDENABLE_TEXPERSP;
        pRc->privateEnables |= __MCDENABLE_COLORED;
        pRc->privateEnables |= __MCDENABLE_SMOOTH;

        a->colors[0].r = b->colors[0].r = c->colors[0].r = Tess_r[iTessCount];
        a->colors[0].g = b->colors[0].g = c->colors[0].g = Tess_g[iTessCount];
        a->colors[0].b = b->colors[0].b = c->colors[0].b = Tess_b[iTessCount];

        iTessCount++;
        iTessCount %= 7;

        a->colors[0].a = b->colors[0].a = c->colors[0].a = 255.0;
    }
}
#endif

//**************************************************************************
//
// VOID DbgTessEnd
//
//**************************************************************************
#if DBG_TESSELATION

VOID DbgTessEnd(DEVRC *pRc)
{
    // Restore the rendering context in case we are debugging the textured
    // triangle tesselation code.
    pRc->privateEnables |= __MCDENABLE_TEXTURED;
    pRc->privateEnables |= __MCDENABLE_TEXPERSP;
    pRc->privateEnables &= ~__MCDENABLE_COLORED;
    pRc->privateEnables &= ~__MCDENABLE_SMOOTH;
}

#endif

// Calculate the new internal properties (x,y,z,w,a,r,g,b) of a vertex
// created in some place along the edge v0-v1 in UV space

#define NEWPROP(VN,V0,V1,PXY,PROP)                  \
    VN->PROP = V0->PROP + PXY*(V1->PROP - V0->PROP)

//**************************************************************************
// VOID FASTCALL __MCDNew_UV_VertexProps
//
// Calculate the properties of a new vertex based on UV subdivision
//
//**************************************************************************
VOID FASTCALL __MCDNew_UV_VertexProps(DEVRC *pRc,MCDVERTEX *vNew,
                                      MCDVERTEX *v0,MCDVERTEX *v1,BOOL uGTv)
{
    MCDFLOAT perspUVSpace, perspXYSpace, s0, s1, sn;

    // calculate new w coordinate
    if (uGTv) {
        s0 = v0->texCoord.x;
        s1 = v1->texCoord.x;
        sn = vNew->texCoord.x;
    } else {
        s0 = v0->texCoord.y;
        s1 = v1->texCoord.y;
        sn = vNew->texCoord.y;
    }

    if ( FABS(s1-s0) > MCDUVEPSILON )
        perspUVSpace = (sn - s0) / (s1 - s0);
    else
        perspUVSpace = __MCDZERO;

    vNew->windowCoord.w = (v1->windowCoord.w * v0->windowCoord.w) /
                           ( perspUVSpace * v0->windowCoord.w +
                            (__MCDONE - perspUVSpace) * v1->windowCoord.w );

    // calculate perspective correction factor in XY space
    if (uGTv) {
        s0 = v0->texCoord.x * v0->windowCoord.w;
        s1 = v1->texCoord.x * v1->windowCoord.w;
        sn = vNew->texCoord.x * vNew->windowCoord.w;
    } else {
        s0 = v0->texCoord.y * v0->windowCoord.w;
        s1 = v1->texCoord.y * v1->windowCoord.w;
        sn = vNew->texCoord.y * vNew->windowCoord.w;
    }

    if ( FABS(s1 - s0) > MCDUVEPSILON )
        perspXYSpace = (sn - s0) / (s1 - s0);
    else
        perspXYSpace = __MCDZERO;

    // calculate new spatial coordinates
    NEWPROP(vNew, v0, v1, perspXYSpace, windowCoord.x);
    NEWPROP(vNew, v0, v1, perspXYSpace, windowCoord.y);
    NEWPROP(vNew, v0, v1, perspXYSpace, windowCoord.z);

    // calculate new color coordinates, if neccesary
    if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
        NEWPROP(vNew, v0, v1, perspXYSpace, colors[0].r);
        NEWPROP(vNew, v0, v1, perspXYSpace, colors[0].g);
        NEWPROP(vNew, v0, v1, perspXYSpace, colors[0].b);
        NEWPROP(vNew, v0, v1, perspXYSpace, colors[0].a);
    } else {
        vNew->colors[0] = v0->colors[0];
    }
}

//**************************************************************************
// MCDFLOAT FASTCALL __MCDUVSlope
//
// Return appropriate slope to use if we are going to travel mainly along
// U or V for a specific edge v0-v1
//
//**************************************************************************
MCDFLOAT FASTCALL __MCDUVSlope(MCDVERTEX *v0, MCDVERTEX *v1,BOOL uGTv)
{
    if (uGTv)
        // dy/dx
        return (v0->texCoord.y - v1->texCoord.y) /
               (v0->texCoord.x - v1->texCoord.x);
    else
        // dx/dy
        return (v0->texCoord.x - v1->texCoord.x) /
               (v0->texCoord.y - v1->texCoord.y);
}

//**************************************************************************
// BOOL FASTCALL __MCD_SplitUVEdge
//
// Calculates a new vertex (vNew) along the edge between v0 and v1 and the next
// in sequence according to vNew's old value , calculate the new properties of
// this point and updates the basevertex for the next calculations!
// Returns TRUE if a new vertex was created,FALSE if v1 has already been reached
//
//**************************************************************************
BOOL FASTCALL __MCD_SplitUVEdge(DEVRC *pRc,MCDVERTEX *vNew,MCDVERTEX *voldnew,
                                MCDVERTEX *v0,MCDVERTEX *v1,
                                BOOL uGTv ,MCDFLOAT derivate,
                                MCDFLOAT uvIncr,MCDVERTEX *basevertex)
{
    MCDFLOAT delta;

    MCDBG_PRINT(DBG_TESS,"TEX PERSP PART:MCDUVSplitUVEdge");

    if (uGTv) { // if delta_u >> delta_v

        if (FABS(voldnew->texCoord.x - v1->texCoord.x) < MCDUVEPSILON) {
            *vNew = *v1;
            return FALSE; // nothing to do, there is no new vertex to create!
        }

        if (v0->texCoord.x < v1->texCoord.x) {
            if ( (vNew->texCoord.x = basevertex->texCoord.x + uvIncr) >
                                                                v1->texCoord.x)
                *vNew = *v1;
            else {
                delta = uvIncr - (voldnew->texCoord.x - basevertex->texCoord.x);
                vNew->texCoord.y = voldnew->texCoord.y + delta * derivate;
                // calculate new properties (w,r,g,b,a,x,y,z)
                __MCDNew_UV_VertexProps(pRc, vNew, v0, v1, uGTv);
            }

        } else {
            if ( (vNew->texCoord.x = basevertex->texCoord.x - uvIncr) <
                                                                v1->texCoord.x)
                *vNew = *v1;
            else {
                delta =-uvIncr + (basevertex->texCoord.x - voldnew->texCoord.x);
                vNew->texCoord.y = voldnew->texCoord.y + delta * derivate;
                // calculate new properties (w,r,g,b,a,x,y,z)
                __MCDNew_UV_VertexProps(pRc, vNew, v0, v1, uGTv);
            }
        }


    } else { // if delta_v > delta_u

        if (FABS(voldnew->texCoord.y - v1->texCoord.y) < MCDUVEPSILON) {
            *vNew = *v1;
            return FALSE; // nothing to do, there is no new vertex to create!
        }

        if (v0->texCoord.y < v1->texCoord.y) {
            if ( (vNew->texCoord.y = basevertex->texCoord.y + uvIncr) >
                                                                v1->texCoord.y)
                *vNew = *v1;
            else {
                delta = uvIncr - (voldnew->texCoord.y - basevertex->texCoord.y);
                vNew->texCoord.x = voldnew->texCoord.x + delta * derivate;
                // calculate new properties (w,r,g,b,a,x,y,z)
                __MCDNew_UV_VertexProps(pRc, vNew, v0, v1, uGTv);
            }

        } else {

            if ( (vNew->texCoord.y = basevertex->texCoord.y - uvIncr) <
                                                                v1->texCoord.y)
                *vNew = *v1;
            else {
                delta = -uvIncr + (basevertex->texCoord.y - voldnew->texCoord.y);
                vNew->texCoord.x = voldnew->texCoord.x + delta * derivate;
                // calculate new properties (w,r,g,b,a,x,y,z)
                __MCDNew_UV_VertexProps(pRc, vNew, v0, v1, uGTv);
            }
        }
    }
    return TRUE; // a new vertex was created
}


//**************************************************************************
// VOID FASTCALL __MCDUVSplitTrapezoid
//
// Split the trapezoid formed by the base edge b and the top edge a with start
// at the v?0 vertexes and ending at the v?f vertexes. Care must be given if
// the trapezoid is degenerated (vA0 == vAf in values or vB0 == vBf in values)
//
//**************************************************************************
VOID FASTCALL __MCDUVSplitTrapezoid(DEVRC *pRc,
                                    MCDVERTEX *vA0,MCDVERTEX *vAf,
                                    MCDVERTEX *vB0,MCDVERTEX *vBf,
                                    MCDFLOAT uvMaxLen, MCDFLOAT uvIncr,
                                    MCDFLOAT uvIncrInv)
{
    MCDFLOAT  derEdgeA, derEdgeB;
    MCDVERTEX vBaseA, vBaseB, vNewTestA, vNewTestB;
    BOOL      edgeXLargeA, edgeXLargeB, incrA, incrB, lastSplitEdgeWasA;
    LONG      piecesEdgeA, piecesEdgeB, totalPieces, iTriCnt;
    PDEV      *ppdev = pRc->ppdev;   

    MCDBG_PRINT(DBG_TESS,"TEX PERSP PART:MCDUVSplitTrapezoid");

    // Edge a properties
    if ( FABS(vAf->texCoord.x - vA0->texCoord.x) >
        FABS(vAf->texCoord.y - vA0->texCoord.y)) {
        // trapezoid basis is u-large
        edgeXLargeA = TRUE;
        piecesEdgeA =
            __MCD_CEIL (FABS(vAf->texCoord.x - vA0->texCoord.x) * uvIncrInv);
    } else {
        // trapezoid basis is v-large
        edgeXLargeA = FALSE;
        piecesEdgeA =
            __MCD_CEIL (FABS(vAf->texCoord.y - vA0->texCoord.y) * uvIncrInv);
    }
    //beware if degenerate
    if (piecesEdgeA) derEdgeA = __MCDUVSlope(vAf, vA0, edgeXLargeA);


    // Edge b properties
    if ( FABS(vBf->texCoord.x - vB0->texCoord.x) >
        FABS(vBf->texCoord.y - vB0->texCoord.y)) {
        // trapezoid basis is u-large
        edgeXLargeB = TRUE;
        piecesEdgeB =
            __MCD_CEIL (FABS(vBf->texCoord.x - vB0->texCoord.x) * uvIncrInv);
    } else {
        // trapezoid basis is v-large
        edgeXLargeB = FALSE;
        piecesEdgeB =
            __MCD_CEIL (FABS(vBf->texCoord.y - vB0->texCoord.y) * uvIncrInv);
    }
    //beware if degenerate
    if (piecesEdgeB) derEdgeB = __MCDUVSlope(vBf, vB0, edgeXLargeB);


    // establish base vertexes to start stripe tesselation
    vBaseA =  *vA0;
    vBaseB =  *vB0;
    // we set the limit of pieces created beforehand
    totalPieces = piecesEdgeA + piecesEdgeB;

    lastSplitEdgeWasA = FALSE;

    for (iTriCnt=0;iTriCnt < totalPieces ;iTriCnt++) {

        if (piecesEdgeA) { // we can still create new pieces along edge a
            __MCD_SplitUVEdge(pRc, &vNewTestA, &vBaseA, vA0, vAf,
                                   edgeXLargeA, derEdgeA, uvIncr, &vBaseA);
            //check if new diagonal meets constraints
            incrA =(FABS(vNewTestA.texCoord.x - vBaseB.texCoord.x) <= uvMaxLen)&&
                   (FABS(vNewTestA.texCoord.y - vBaseB.texCoord.y) <= uvMaxLen);
        } else
            incrA = FALSE;


        if (piecesEdgeB) { // we can still create new pieces along edge b
            __MCD_SplitUVEdge(pRc, &vNewTestB, &vBaseB, vB0, vBf,
                                   edgeXLargeB, derEdgeB, uvIncr, &vBaseB);
            //check if new diagonal meets constraints
            incrB =(FABS(vNewTestB.texCoord.x - vBaseA.texCoord.x) <= uvMaxLen)&&
                   (FABS(vNewTestB.texCoord.y - vBaseA.texCoord.y) <= uvMaxLen);
        } else
            incrB = FALSE;

        if (incrA && (!incrB || !lastSplitEdgeWasA)) {
            (*ppdev->mcdFillSubTriangle)(pRc, &vNewTestA, &vBaseA, &vBaseB, FALSE, FALSE);
            lastSplitEdgeWasA = TRUE;
            vBaseA = vNewTestA;
            piecesEdgeA--;
        } else if (incrB && (!incrA || lastSplitEdgeWasA)) {
            (*ppdev->mcdFillSubTriangle)(pRc, &vNewTestB, &vBaseB, &vBaseA, FALSE, FALSE);
            lastSplitEdgeWasA = FALSE;
            vBaseB = vNewTestB;
            piecesEdgeB--;
        } else {
            // This sould not happen but just in case we handle it.
            MCDBG_PRINT(DBG_TESS,"Could not continue processing Trapezoid stripe!");
        }
    }
}

//**************************************************************************
// VOID FASTCALL __MCDUVSplitTriangle
//
// Checks if a triangle requires tesselation in order not to span the
// maximum of texels the S3Virge allows us for a single textured triangle.
// If it does require tesselation, it goes on to do it according to the
// number of edges which violate the constraint.
//
//**************************************************************************
VOID FASTCALL __MCDUVSplitTriangle(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                    MCDVERTEX *c, BOOL bCCW, BOOL bTrust)
{
    MCDFLOAT uvMaxLen, uvIncr, uvIncrInv, uLen[3], vLen[3], derEdge1, derEdge2;
    MCDVERTEX *v[3], vNew, vNew1, vNew2, vBase;
    MCDVERTEX vLast1, vLast2, vNewApex1, vNewApex2;
    BOOL     uGTv[3], vGTu[3];
    BOOL     edgeXLarge1, edgeXLarge2, bvNew1, bvNew2;
    LONG     vLargeSideI, vLargeSideF, vSmallSideI, vSmallSideF;
    LONG     vOppositeLarge, vOppositeSmall;
    LONG     bigSidesCount, vInd, vInd1, vInd2;
    LONG     bLeftToRight, bBottomToTop, bHorizontal, bVertical, bMixed;
    PDEV     *ppdev = pRc->ppdev;   


    MCDBG_PRINT(DBG_TESS,"Entry to MCDUVSplit Triangle");
    MCDBG_PRINT(DBG_TESS,"Vertex 1 u=%i  v=%i", (long)(a->texCoord.x * 1000.0),
                                        (long)(a->texCoord.y*1000.0));
    MCDBG_PRINT(DBG_TESS,"Vertex 2 u=%i  v=%i", (long)(b->texCoord.x * 1000.0),
                                        (long)(b->texCoord.y*1000.0));
    MCDBG_PRINT(DBG_TESS,"Vertex 3 u=%i  v=%i", (long)(c->texCoord.x * 1000.0),
                                        (long)(c->texCoord.y*1000.0));


    // For mip mapping we should change this somehow
    uvMaxLen = ppdev->uvMaxTexels / pRc->texwidth;  

    bigSidesCount = 0;
    v[0] = a;
    v[1] = b;
    v[2] = c;

    for(vInd=0; vInd < 3; vInd++) {
        vInd1 = (vInd + 1) % 3;
        vInd2 = (vInd + 2) % 3;
        uLen[vInd] = FABS(v[vInd1]->texCoord.x - v[vInd2]->texCoord.x);
        vLen[vInd] = FABS(v[vInd1]->texCoord.y - v[vInd2]->texCoord.y);
        if ((uLen[vInd] > uvMaxLen) || (vLen[vInd] > uvMaxLen)) {
            bigSidesCount++;       // This branch is taken only once for
            vLargeSideI = vInd1;   // triangles which have 1 edge
            vLargeSideF = vInd2;   // exceeding the UV constraint
            vOppositeLarge  = vInd;
            uGTv[vInd] = uLen[vInd] > vLen[vInd];
            vGTu[vInd] = vLen[vInd] > uLen[vInd];
        } else {
            vSmallSideI     = vInd1;   // This branch is taken only once for
            vSmallSideF     = vInd2;   // triangles which have 2 edges
            vOppositeSmall  = vInd;    // exceeding the UV constraint
        }
    }

    switch(bigSidesCount) {
        case 0:// Trivial case, the triangle doesnt exceed UV space constraints
                MCDBG_PRINT(DBG_TESS,"TEX PERSP PART(0):No splitting");
                (*ppdev->mcdFillSubTriangle)(pRc, a, b, c, bCCW, bTrust);
                break;
        case 1:// Only one side exceeds constraints, it will be midpoint
               // subdivided and two new triangles created which are guaranteed
               // to satisfy constraints
                MCDBG_PRINT(DBG_TESS,"TEX PERSP PART(1):Start edge splitting");

                vNew.texCoord.x = __MCDHALF * (v[vLargeSideI]->texCoord.x +
                                             v[vLargeSideF]->texCoord.x);
                vNew.texCoord.y = __MCDHALF * (v[vLargeSideI]->texCoord.y +
                                             v[vLargeSideF]->texCoord.y);
                __MCDNew_UV_VertexProps(pRc, &vNew, v[vLargeSideI],
                                        v[vLargeSideF],uGTv[vOppositeLarge]);
                // Must recalculate area & bCCW for new two subtriangles
                (*ppdev->mcdFillSubTriangle)(pRc, v[vLargeSideI], &vNew,
                                     v[vOppositeLarge], FALSE, FALSE);
                (*ppdev->mcdFillSubTriangle)(pRc, &vNew, v[vLargeSideF],
                                     v[vOppositeLarge], FALSE, FALSE);
                break;
        case 2:// Two edges exceed constraints, we will first cut a triangle out
               // of the apex (vOppositeSmall) and then create pairs of triangles
               // until we get to the opposite edge to the apex vertex.

                MCDBG_PRINT(DBG_TESS,"TEX PERSP PART(2):Start edge splitting");

                // Establish first the base vertex of our virtual square equal
                // to the apex vertex
                vLast1 = vLast2 = vBase = *(v[vOppositeSmall]);
                edgeXLarge1 = uGTv[vSmallSideF];
                edgeXLarge2 = uGTv[vSmallSideI];

                derEdge1 =
                    __MCDUVSlope(v[vOppositeSmall], v[vSmallSideI], edgeXLarge1);
                derEdge2 =
                    __MCDUVSlope(v[vOppositeSmall], v[vSmallSideF], edgeXLarge2);

                // Determine if both edges which exceed the constraint run
                // horizontal or vertically. If they run in mixed mode then
                // we need to handle the base vertex to build valid subtriangles
                // which don't exceed the constraints. Only one of bHorizontal,
                // bVertical and bMixed is going to be TRUE.

                bHorizontal = edgeXLarge1 && edgeXLarge2;
                bVertical   = (!edgeXLarge1) && (!edgeXLarge2);
                bMixed      = (!bHorizontal) && (!bVertical);

                if (bHorizontal)
                    bLeftToRight =
                        (v[vOppositeSmall]->texCoord.x < v[vSmallSideI]->texCoord.x);

                if (bVertical)
                    bBottomToTop =
                        (v[vOppositeSmall]->texCoord.y < v[vSmallSideI]->texCoord.y);

                // Do the first splitting of the edges creating an apex subtriangle

                __MCD_SplitUVEdge(pRc, &vNew1, &vLast1,
                                  v[vOppositeSmall], v[vSmallSideI],
                                  edgeXLarge1, derEdge1, uvMaxLen, &vBase);
                __MCD_SplitUVEdge(pRc, &vNew2, &vLast2,
                                  v[vOppositeSmall], v[vSmallSideF],
                                  edgeXLarge2, derEdge2, uvMaxLen, &vBase);

                // Must recalculate area & bCCW for our first triangle
                (*ppdev->mcdFillSubTriangle)(pRc, v[vOppositeSmall], &vNew1, &vNew2,
                                     FALSE, FALSE);

                // Now we partition the remaining  trapezoid until we reach the
                // small edge. This partitioning must be done at least once,
                // otherwise we would have not fallen in this particular case
                // the trapezoid will be tesselated in stripes, each stripe
                // containing 2 or less triangles

                do {
                    // update working vertexes
                    vLast1 = vNew1;
                    vLast2 = vNew2;

                    // Update the base vertex from which we are going to build up
                    // the next virtual square to contain our sub triangles

                    if (bHorizontal)
                        if ( bLeftToRight )
                            vBase.texCoord.x =
                                max(vNew1.texCoord.x, vNew2.texCoord.x);
                        else
                            vBase.texCoord.x =
                                min(vNew1.texCoord.x, vNew2.texCoord.x);

                    if (bVertical)
                        if ( bBottomToTop )
                            vBase.texCoord.y =
                                max(vNew1.texCoord.y, vNew2.texCoord.y);
                        else
                            vBase.texCoord.y =
                                min(vNew1.texCoord.y, vNew2.texCoord.y);

                    if (bMixed)
                        if (edgeXLarge2) {
                                vBase.texCoord.x = vNew1.texCoord.x;
                                vBase.texCoord.y = vNew2.texCoord.y;
                        } else {
                                vBase.texCoord.x = vNew2.texCoord.x;
                                vBase.texCoord.y = vNew1.texCoord.y;
                        }

                    // Calculate new vertexes!
                    bvNew1 = __MCD_SplitUVEdge(pRc, &vNew1, &vLast1,
                                               v[vOppositeSmall], v[vSmallSideI],
                                               edgeXLarge1, derEdge1, uvMaxLen,
                                               &vBase);
                    bvNew2 = __MCD_SplitUVEdge(pRc, &vNew2, &vLast2,
                                               v[vOppositeSmall], v[vSmallSideF],
                                               edgeXLarge2, derEdge2, uvMaxLen,
                                               &vBase);

                    // If vNew1 is indeed new then create a subtriangle
                    // with vLast1,vNew1 and vLast2 and send it to hw rendering
                    if (bvNew1)
                        (*ppdev->mcdFillSubTriangle)(pRc, &vLast1, &vNew1, &vLast2,
                                             FALSE, FALSE);

                    // if vNew2 is indeed new and vNew1 is also new then create a
                    // subtriangle with vLast2,vNew2 and vNew1 , otherwise do it
                    // with vLast2,vNew2 and vLast1. Send it to hw rendering
                    if (bvNew2)
                        if (bvNew1)
                            (*ppdev->mcdFillSubTriangle)(pRc, &vLast2, &vNew2, &vNew1,
                                                 FALSE, FALSE);
                        else
                            (*ppdev->mcdFillSubTriangle)(pRc, &vLast2, &vNew2, &vLast1,
                                                 FALSE, FALSE);

                } while (bvNew1 || bvNew2);

                break;
        case 3:// All three edges exceed the constraints. This is a generalized
               // case of the above case, in which in each stripe we create not
               // 2 but maybe more subtriangles, according to the length of each
               // stripes side.

                // Choose first a starting vertex, choose the one opposite to
                // the smallest side using x2+y2 metric (instead of max(x,y))

                vOppositeSmall = 0;

                if (( uLen[vOppositeSmall] * uLen[vOppositeSmall] +
                      vLen[vOppositeSmall] * vLen[vOppositeSmall] ) >
                    ( uLen[1] * uLen[1] + vLen[1] * vLen[1]) )
                    vOppositeSmall = 1;

                if (( uLen[vOppositeSmall] * uLen[vOppositeSmall] +
                      vLen[vOppositeSmall] * vLen[vOppositeSmall] ) >
                    ( uLen[2] * uLen[2] + vLen[2] * vLen[2]) )
                    vOppositeSmall = 2;

                vSmallSideI = (vOppositeSmall + 1) % 3;
                vSmallSideF = (vOppositeSmall + 2) % 3;


                // The base vertex of each edge is going to be equal to new vertex
                vLast1 = vLast2 = *(v[vOppositeSmall]);
                edgeXLarge1 = uGTv[vSmallSideF];
                edgeXLarge2 = uGTv[vSmallSideI];

                derEdge1 = __MCDUVSlope(v[vOppositeSmall], v[vSmallSideI],
                                        edgeXLarge1);
                derEdge2 = __MCDUVSlope(v[vOppositeSmall], v[vSmallSideF],
                                        edgeXLarge2);

                if ((edgeXLarge1 ^ edgeXLarge2) &&
                   (vGTu[vSmallSideF] ^ vGTu[vSmallSideI]))
                    // large in opposite directions
                    uvIncr = uvMaxLen * __MCDHALF;
                else
                    // both large in same direction
                    uvIncr = uvMaxLen;

                uvIncrInv = __MCDONE / uvIncr;

                // Calculate new vertexes!
                bvNew1 = __MCD_SplitUVEdge(pRc, &vNew1, &vLast1,
                                           v[vOppositeSmall], v[vSmallSideI],
                                           edgeXLarge1, derEdge1, uvIncr,
                                           &vLast1);
                bvNew2 = __MCD_SplitUVEdge(pRc, &vNew2, &vLast2,
                                           v[vOppositeSmall], v[vSmallSideF],
                                           edgeXLarge2, derEdge2, uvIncr,
                                           &vLast2);

                // We create triangle stripes until we hit the end of at least
                // one the edges
                MCDBG_PRINT(DBG_TESS,"TEX PERSP PART(3):Start first edge splitting");
                while ( bvNew1 && bvNew2) {

                    // Now we have to partition the trapezoid formed by vLast1,
                    // vLast2, vNew1 and vNew2 into triangles
                    __MCDUVSplitTrapezoid(pRc, &vNew1, &vNew2, &vLast1, &vLast2,
                                          uvMaxLen, uvIncr, uvIncrInv);

                    // update working vertexes
                    vLast1 = vNew1;
                    vLast2 = vNew2;

                    // Calculate new vertexes!
                    bvNew1 = __MCD_SplitUVEdge(pRc, &vNew1, &vLast1,
                                               v[vOppositeSmall], v[vSmallSideI],
                                               edgeXLarge1, derEdge1, uvIncr,
                                               &vLast1);
                    bvNew2 = __MCD_SplitUVEdge(pRc, &vNew2, &vLast2,
                                               v[vOppositeSmall], v[vSmallSideF],
                                               edgeXLarge2, derEdge2, uvIncr,
                                               &vLast2);
                }

                // Now we must see if we are done or if we need to switch
                // to our last edge

                if ((!bvNew1) && (!bvNew2))
                    return; //we are done


                vNewApex1 = vNewApex2 = *v[vOppositeSmall];

                // Update the finished edge (1) with the last remaining edge
                if (!bvNew1) {

                    vLast1 = vNewApex1 = *v[vSmallSideI];
                    vSmallSideI = vSmallSideF; //Don't miss this

                    // The base vertex of each edge is going
                    // to be equal to the new vertex
                    edgeXLarge1 =  ( FABS(vNewApex1.texCoord.x -
                                           v[vSmallSideI]->texCoord.x) >
                                      FABS(vNewApex1.texCoord.y -
                                           v[vSmallSideI]->texCoord.y));

                    derEdge1 = __MCDUVSlope(&vNewApex1, v[vSmallSideF],
                                            edgeXLarge1);

                    // Calculate new vertex!
                    bvNew1 = __MCD_SplitUVEdge(pRc, &vNew1, &vLast1,
                                               &vNewApex1, v[vSmallSideI],
                                               edgeXLarge1, derEdge1, uvIncr,
                                               &vLast1);
                }

                // Update the finished edge (2) with the last remaining edge
                if (!bvNew2) {
                    vLast2 = vNewApex2 = *v[vSmallSideF];
                    vSmallSideF = vSmallSideI; // Don't miss this

                    // The base vertex of each edge is going
                    // to be equal to the new vertex
                    edgeXLarge2 =  ( FABS(vNewApex2.texCoord.x -
                                           v[vSmallSideF]->texCoord.x) >
                                      FABS(vNewApex2.texCoord.y -
                                           v[vSmallSideF]->texCoord.y));


                    derEdge2 = __MCDUVSlope(&vNewApex2, v[vSmallSideI],
                                            edgeXLarge2);

                    // Calculate new vertex!
                    bvNew2 = __MCD_SplitUVEdge(pRc, &vNew2, &vLast2,
                                               &vNewApex2, v[vSmallSideF],
                                               edgeXLarge2, derEdge2, uvIncr,
                                               &vLast2);
                }

                // We create the second set of triangle stripes until we hit the
                // end of at least one the edges (which really is going to be
                // the end of both!)
                MCDBG_PRINT(DBG_TESS,"TEX PERSP PART(3):Starting second "
                                     "edge splitting");

                while ( bvNew1 || bvNew2) {

                    // Now we have to partition the trapezoid formed by vLast1,
                    // vLast2, vNew1 and vNew2 into triangles
                    __MCDUVSplitTrapezoid(pRc, &vNew1, &vNew2,
                                          &vLast1, &vLast2,
                                          uvMaxLen, uvIncr, uvIncrInv);

                    // Calculate new vertexes!
                    if (bvNew1) {
                        vLast1 = vNew1;
                        bvNew1 = __MCD_SplitUVEdge(pRc, &vNew1, &vLast1,
                                                   &vNewApex1, v[vSmallSideI],
                                                   edgeXLarge1, derEdge1, uvIncr,
                                                   &vLast1);
                    }

                    if (bvNew2) {
                        vLast2 = vNew2;
                        bvNew2 = __MCD_SplitUVEdge(pRc, &vNew2, &vLast2,
                                                   &vNewApex2, v[vSmallSideF],
                                                   edgeXLarge2, derEdge2, uvIncr,
                                                   &vLast2);
                    }
                }

                break;
        default:
                MCDBG_PRINT(DBG_TESS,"TEX PERSP PART:Cannot have more than 3 "
                                     "sides on a triangle for UV space "
                                     "partitioning!");
                break;
    }

    MCDBG_PRINT(DBG_TESS,"TEX PERSP PART: SPLITTING END");
}

//**************************************************************************
//
// VOID FASTCALL __MCDFillTexOrSmoothTriangle
//
// Render a triangle (no left window edge clipping necessary)
//
// Determines if a triangle can be HW rendered as it is, or if it is
// necessary to perform tesselation to adhere to S3Virge rule's of
// maximum of texels spanned in a single triangle.
//
//**************************************************************************
VOID FASTCALL __MCDFillTexOrSmoothTriangle(DEVRC *pRc, MCDVERTEX *a,
                                           MCDVERTEX *b, MCDVERTEX *c,
                                           BOOL bCCW, BOOL bTrust)
{
    PDEV *ppdev = pRc->ppdev;   


    if (pRc->privateEnables & __MCDENABLE_TEXPERSP)
        // Since we are going to texture it and do perspective correction we
        // need to perform subdivision if necessary for the S3 Virge
        // since it fails when the texel coordinate range exceeds 128 texels.
        // even on textures of smaller size but with repeating coordinates
        // This situation is particular to the S3 Virge.
            __MCDUVSplitTriangle(pRc, a, b, c, bCCW, bTrust);
    else
        // last param indicates we can trust ordering of
        // vertexes from higher valued y to lower valued y,
        // bCCW and pRc->halfArea, otherwise, we will have to
        // figure them out again.
        (*ppdev->mcdFillSubTriangle)(pRc, a, b, c, bCCW, bTrust);

}


//**************************************************************************
//
// VOID FASTCALL __MCDFillTriangle
//
// Main entry point to render a triangle
//
// Determines if a triangle can be rendered as it is, or if it is
// necessary to clip it against the window's left edge when the triangles
// x coordinates become negative (which is not appropriately handled by hw)
//
//**************************************************************************
VOID FASTCALL __MCDFillTriangle(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                MCDVERTEX *c, BOOL bCCW)
{
    BOOL      aClip, bClip, cClip;
    MCDVERTEX v0, v1;


#if MOVE_XDER_BACK
    //////////////////////////////////////////////////////////
    // PRECALCULATE ANY QUANTITIES WHICH WILL REMAIN CONSTANT
    // REGARDLESS OF ANY MODIFICATIONS TO THE TRIANGLE
    //////////////////////////////////////////////////////////

    if (!(pRc->privateEnables & __MCDENABLE_TEXPERSP)) {

        // We can calculate the area x derivates before any potential triangle
        // splitting since they must remain constant over the whole triangle
        // surface no matter how it is splitted!
        __MCDCalcXDeltaProps(pRc, a, b, c);
        pRc->xDerCCW = bCCW;
    }
#endif

    //////////////////////////////////////////////////
    // FIX A PARTICULAR CLIPPING ISSUE OF THE S3 VIRGE
    //////////////////////////////////////////////////

    // Need to clip by sw if x < 0 since hw clipping on the left edge
    // window when x < 0 is not working appropriately

    // No extra clipping performed if the window is not crossing the
    // left screen edge
    if (pRc->pMCDSurface->pWnd->clientRect.left >= 0) {
        __MCDFillTexOrSmoothTriangle(pRc, a, b, c, bCCW, TRUE);
        return;
    }

    // Otherwise, check coordinates of triangle to see if they cross the
    // left edge
    aClip = a->windowCoord.x + (long)pRc->xOffset < 0;
    bClip = b->windowCoord.x + (long)pRc->xOffset < 0;
    cClip = c->windowCoord.x + (long)pRc->xOffset < 0;

    // no clipping needed
    if (!(aClip || bClip ||cClip)) {
        __MCDFillTexOrSmoothTriangle(pRc, a, b, c, bCCW, TRUE);
        return;
    }

     // triangle completly rejected
    if (aClip && bClip && cClip)
        return;

    // Need create two new vertexes and send 1 triangle down the pipeline
    if (aClip && bClip) {
        __MCDClip2Vert(pRc, &v0, c, a);
        __MCDClip2Vert(pRc, &v1, c, b);
        __MCDFillTexOrSmoothTriangle(pRc, &v0, &v1, c, bCCW, FALSE);
        return;
    }

    if (bClip && cClip) {
        __MCDClip2Vert(pRc, &v0, a, b);
        __MCDClip2Vert(pRc, &v1, a, c);
        __MCDFillTexOrSmoothTriangle(pRc, &v0, &v1, a, bCCW, FALSE);
        return;
    }

    if (aClip && cClip) {
        __MCDClip2Vert(pRc, &v0, b, a);
        __MCDClip2Vert(pRc, &v1, b, c);
        __MCDFillTexOrSmoothTriangle(pRc, &v0, &v1, b, bCCW, FALSE);
        return;
    }

    //Need create two new vertexes and send 2 triangles down the pipeline
    if (aClip) {
        __MCDClip2Vert(pRc, &v0, b, a);
        __MCDClip2Vert(pRc, &v1, c, a);
        __MCDFillTexOrSmoothTriangle(pRc, &v0, &v1, b, bCCW, FALSE);
        __MCDFillTexOrSmoothTriangle(pRc, &v1, b, c, bCCW, FALSE);
        return;
    }

    if (bClip) {
        __MCDClip2Vert(pRc, &v0, a, b);
        __MCDClip2Vert(pRc, &v1, c, b);
        __MCDFillTexOrSmoothTriangle(pRc, &v0, &v1, a, bCCW, FALSE);
        __MCDFillTexOrSmoothTriangle(pRc, &v1, a, c, bCCW, FALSE);
        return;
    }

    if (cClip) {
        __MCDClip2Vert(pRc, &v0, a, c);
        __MCDClip2Vert(pRc, &v1, b, c);
        __MCDFillTexOrSmoothTriangle(pRc, &v0, &v1, a, bCCW, FALSE);
        __MCDFillTexOrSmoothTriangle(pRc, &v1, a, b, bCCW, FALSE);
        return;
    }

    MCDBG_PRINT(DBG_ERR,"MCDFillTriangle - ERROR: We should but did not clip");
    return;
}

#endif

