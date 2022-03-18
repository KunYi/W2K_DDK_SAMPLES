/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dgntri.c
*
*  Content:    Generate a triangle function
*
* Copyright (C) 1995-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

// assume no aliasing and don't generate frame pointers on the stack
#pragma optimize( "a", on )
#pragma optimize( "y", off )


// We check and set up necessary symbols to build this particular instance
// of the hardware triangle rasterization function

#ifndef GOURAUD
#define GOURAUD 0
#else
#undef GOURAUD
#define GOURAUD 1
#endif

#undef  DMASUPPORT
#define DMASUPPORT 0

#ifndef ZBUFFER
#define ZBUFFER 0
#else
#undef ZBUFFER
#define ZBUFFER 1
#endif

#ifndef TEXTURED
#define TEXTURED 0
#else
#undef TEXTURED
#define TEXTURED 1
#endif

#ifndef PERSPECTIVE
#define PERSPECTIVE 0
#else
#undef PERSPECTIVE
#if TEXTURED    //No Perspective if no texture
    #define PERSPECTIVE 1
#else   //TEXTURED
  #error  !ERROR: TEXTRED needs to be defined when PERSPECTIVE
#endif  //TEXTURED
#endif

#ifndef FOGGED
#define FOGGED 0
#else
#undef FOGGED
#define FOGGED 1
#endif

#ifndef WRAPCODE
#define WRAPCODE 1
#endif

// We redefine our hardware access functions in case this is a DMA
// version of our function
#if DMASUPPORT

#undef  CMD_SET
#undef  WaitFifo
#undef  WaitFifoEmpty
#undef  _WRITE_REG_TRI_LONG
#undef  _WRITE_REG_SET_LONG
#undef  _INDEX_ADVANCE
#undef  _ADVANCE_INDEX
#undef  _WRITE_HEADER
#undef  _UNDO_DMA_HDR
#undef  _START_DMA
#define CMD_SET 0
#define WaitFifo(pCtxt, x)
#define WaitFifoEmpty()
#define _WRITE_REG_TRI_LONG( Offset, Value )                                \
    { *( ULONG * )( ( char * )D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DTri +    \
        ((D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex+Offset)&_DMAFILTER))   \
            =  Value;}
#define _WRITE_REG_SET_LONG( Offset, Value )                                \
    { *( ULONG * )( ( char * )D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DStp +    \
        ((D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex+Offset)&_DMAFILTER))   \
            =  Value;}
#define _INDEX_ADVANCE  4
#define _WRITE_HEADER( SLOTS_NEEDED, DMAVALUE ) {                           \
    ULONG tmp;                                                              \
    do {                                                                    \
        tmp = D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaReadPtr;                 \
        if (D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaReadPtr <=                 \
            D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex)                     \
            tmp += _DMAFILTER + 1;                                          \
        if ( (tmp - D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex) <=          \
             (SLOTS_NEEDED) ) { /*need to re-read read ptr*/                \
            tmp = D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaReadPtr =            \
                *(D3DGLOBALPTR(pCtxt)->D3DGlobals.g_lpReadPtrReg) & 0xFFC;  \
            if (tmp <= D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex)          \
                tmp += _DMAFILTER + 1;                                      \
        }                                                                   \
    } while ((tmp - D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex) <=          \
             (SLOTS_NEEDED) );                                              \
    _WRITE_REG_SET_LONG( 0, DMAVALUE );                                     \
    D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex += _INDEX_ADVANCE;           \
}

#define _ADVANCE_INDEX( ADVANCE ) {                                         \
    D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex += ADVANCE;                  \
    D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex &= _DMAFILTER;               \
}

#define _START_DMA {                                                        \
    *(D3DGLOBALPTR(pCtxt)->D3DGlobals.g_lpWritePtrReg) =                    \
        0x00010000 | D3DGLOBALPTR(pCtxt)->D3DGlobals.g_DmaIndex;            \
}
#undef _UBASEHIGH
#define _UBASEHIGH  0x80000000

#else   //DMASUPPORT

#define CMD_SET 0x002C
#undef _UBASEHIGH
#define _UBASEHIGH      (D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh)

#endif  //DMASUPPORT


#ifndef CONSTDEFD
#define CONSTDEFD
const float fDxConstNeg = -1.0f/524188.0f;
const float fDxConstPos =  1.0f/524188.0f;
const float magic = 256.f;
// This is a Virge-specific offset needed to render textures according
// to the  D3D rasterization rules
const float fUVFixOffs[] = { 0.5f/  1.0f,
                             0.5f/  2.0f,
                             0.5f/  4.0f,
                             0.5f/  8.0f,
                             0.5f/ 16.0f,
                             0.5f/ 32.0f,
                             0.5f/ 64.0f,
                             0.5f/128.0f,
                             0.5f/256.0f,
                             0.5f/512.0f };
#endif
//-------------------------------------

//*****************************************************************************
//
// BOOL FNAME
//
// The strange way this function is written allows the compiler to produce
// fully optimized code for a given case, with source level debugging
// possible.
// Another advantage is that a mixed code and assembler file produced
// by the compiler contains real code lines, whereas using the
// preprocessor all you get is on code line for the whole routine.
// Comments about the code..
// 1. As far as possible, float multiplies are used instead of integer.
//    Although instinct would tend to suggest otherwise, the Pentium
//    float multiply is 3 times faster than integer, and is twice as
//    fast as an FIMUL.
// 2. Conversion from float to integer is costly, and is only done once
//    where possible for each value.
// 3. Float comparison is bad.. avoid if possible.
//
//*****************************************************************************

void    FNAME ( S3_CONTEXT *pCtxt,
                LPD3DTLVERTEX p0,
                LPD3DTLVERTEX p1,
                LPD3DTLVERTEX p2,
                LPS3FVFOFFSETS   lpS3FVFOff)
{

    #pragma pack(8)

    struct  _TRILOCALS
    {
        D3DCOLOR Color0;
        int iCount;
        int iDy12, iDy02, iDy01;
        int i2y, i1y, i0y;
        _PRECISION fDxr, fDx;
        _PRECISION fDyCC;
        _PRECISION fDy01, fDy12;
        double fDxDy01, fDxDy12, fDxDy02;
        _PRECISION fDy02r;

    #if GOURAUD
        D3DCOLOR Color2, Color1;
    #else
        LPD3DTLVERTEX pFlat;
    #endif

    #if WRAPCODE
        _PRECISION lp, rp, mp;
    #endif

    #if TEXTURED
        _PRECISION u2, u1, u0;
        _PRECISION v2, v1, v0;
        _PRECISION fDvDx, fDuDy, fDvDy, fDuDx;
    #endif

    #if PERSPECTIVE
        int   i, j;
        BOOL PerspectiveChange;
        _PRECISION w2, w1, w0, uBase, vBase, tmp, wMax;
        _PRECISION fDwY,fDwX;
        _PRECISION Magic;
        _PRECISION a[3][3];
        _PRECISION det;
        _PRECISION Du[3],Dv[3],Dw[3];
        _PRECISION tmpW[3], tmpU[3], tmpV[3], tmpD[3];
        _PRECISION diff[2][2];
        _PRECISION diff_2[2][2];
        _PRECISION tr, dt;
        _PRECISION lt;
        _PRECISION d0, ddx, ddy;
    #endif

    #if GOURAUD //put these 2 condition in will slow polythrough from 178 to 145
        _PRECISION fRdX, fRdY, fGdX, fGdY, fBdX, fBdY, fAdX, fAdY;
    #endif
    #if ZBUFFER
        _PRECISION fDzX, fDzY;
    #endif
    #if TEXTURED
        int texsizelog2;
    #endif
        BYTE As;
    } tl ;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;

    char*       _TRI_BASE = ( char * )D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DTri;

    DPF_DBG("Triangle Rendering - Mode %x", pCtxt->dwRCode);

#if !GOURAUD
    // in flat shading, the following must be done before we order vertices
    tl.pFlat = p0;
#endif

    // Order the vertices
    if( p2->sy > p1->sy )
        PTRSWAP( (ULONG_PTR)p2, (ULONG_PTR)p1 );
    if( p2->sy > p0->sy )
        PTRSWAP( (ULONG_PTR)p2, (ULONG_PTR)p0 );
    if( p1->sy > p0->sy )
        PTRSWAP( (ULONG_PTR)p1, (ULONG_PTR)p0 );

    __SetFVFOffsets(&dwColorOffs, &dwSpecOffs, &dwTexOffs,lpS3FVFOff);

    // Verify the vertex screen coordinates are within
    // reasonable limits for the Virge Fifo issue.
    CHECK_VERTEX_VALUE(p0);
    CHECK_VERTEX_VALUE(p1);
    CHECK_VERTEX_VALUE(p2);


    // Print out debugging info
    DPF_DBG("   vertices = (%d,%d)-(%d,%d)-(%d,%d)",
            (long)p0->sx, (long)p0->sy,
            (long)p1->sx, (long)p1->sy,
            (long)p2->sx, (long)p2->sy);

#if TEXTURED
    DPF_DBG("   tex coords = (%d,%d)-(%d,%d)-(%d,%d)",
            (long)(FVFTEX(p0, dwTexOffs)->tu*1000.0), (long)(FVFTEX(p0, dwTexOffs)->tv*1000.0),
            (long)(FVFTEX(p1, dwTexOffs)->tu*1000.0), (long)(FVFTEX(p1, dwTexOffs)->tv*1000.0),
            (long)(FVFTEX(p2, dwTexOffs)->tu*1000.0), (long)(FVFTEX(p2, dwTexOffs)->tv*1000.0));
#endif

    // calculate integer y deltas
    tl.i2y = MYFLINT(p2->sy);
    tl.iDy02 = (tl.i0y = MYFLINT(p0->sy)) - tl.i2y;

    // if width of triangle is zero then don't render it
    if( tl.iDy02 == 0 && ((float)tl.i2y < p2->sy))
        return;

    tl.iDy12 = (tl.i1y = MYFLINT(p1->sy)) - tl.i2y;
    tl.fDy02r = 1.0f/(p0->sy - p2->sy);
    tl.fDy01 = p0->sy - p1->sy;
    tl.fDy12 = p1->sy - p2->sy;
    tl.iDy01 = tl.i0y - tl.i1y;
    tl.fDyCC = p0->sy - (float)tl.i0y;

    if (tl.fDyCC == 0.0f && tl.iDy01 == 0.0f) {
        tl.i0y --;
        tl.i1y --;
        tl.fDyCC = 1.0f;
    }

    tl.fDxDy12 = tl.fDy12 ? (p2->sx - p1->sx)/tl.fDy12 : 0.0f;
    tl.fDxDy01 = tl.fDy01 ? (p1->sx - p0->sx)/tl.fDy01 : 0.0f;
    tl.fDxDy02 = (p2->sx - p0->sx)*tl.fDy02r;
    // tl.fDx = p1->sx - (tl.fDy01 * tl.fDxDy02 + p0->sx);
    tl.fDx = (float)(p1->sx - (tl.fDy01 * tl.fDxDy02 + p0->sx));

    if( tl.fDx < fDxConstNeg )
        tl.fDxr = -1.0f / tl.fDx;
    else if (tl.fDx > fDxConstPos)
        tl.fDxr = 1.0f / tl.fDx;
    else
        tl.fDx = tl.fDxr = 0.0f;

    _WRITE_S3D_HEADER ( DATA_SIZE );

    /****************************************************************/
    /*                TEXTURING PARAMETERS SETUP                    */
    /****************************************************************/
#if TEXTURED

    // Attempt to make virge texturing pass conformance. Will keep for
    // a short while only to remind us that we already tried this ...
#if 0
    tl.texsizelog2 = IntLog2((unsigned int)pCtxt->fTextureHeight);

    tl.u2 = FVFTEX(p2, dwTexOffs)->tu - fUVFixOffs[tl.texsizelog2];
    tl.u1 = FVFTEX(p1, dwTexOffs)->tu - fUVFixOffs[tl.texsizelog2];
    tl.u0 = FVFTEX(p0, dwTexOffs)->tu - fUVFixOffs[tl.texsizelog2];
    tl.v2 = FVFTEX(p2, dwTexOffs)->tv - fUVFixOffs[tl.texsizelog2];
    tl.v1 = FVFTEX(p1, dwTexOffs)->tv - fUVFixOffs[tl.texsizelog2];
    tl.v0 = FVFTEX(p0, dwTexOffs)->tv - fUVFixOffs[tl.texsizelog2];
#endif

    tl.u2 = FVFTEX(p2, dwTexOffs)->tu;
    tl.u1 = FVFTEX(p1, dwTexOffs)->tu;
    tl.u0 = FVFTEX(p0, dwTexOffs)->tu;
    tl.v2 = FVFTEX(p2, dwTexOffs)->tv;
    tl.v1 = FVFTEX(p1, dwTexOffs)->tv;
    tl.v0 = FVFTEX(p0, dwTexOffs)->tv;

    if( pCtxt->TextureAddress == D3DTADDRESS_WRAP ) {

        if ( pCtxt->bWrapU )
            MYWRAP( tl.u0, tl.u1, tl.u2);

        if ( pCtxt->bWrapV )
            MYWRAP( tl.v0, tl.v1, tl.v2);

    } else if ( pCtxt->TextureAddress == D3DTADDRESS_CLAMP ) {
        LIMIT_HI_TO_ONE(tl.u0);
        LIMIT_LO_TO_ZRO(tl.u0);
        LIMIT_HI_TO_ONE(tl.u1);
        LIMIT_LO_TO_ZRO(tl.u1);
        LIMIT_HI_TO_ONE(tl.u2);
        LIMIT_LO_TO_ZRO(tl.u2);

        LIMIT_HI_TO_ONE(tl.v0);
        LIMIT_LO_TO_ZRO(tl.v0);
        LIMIT_HI_TO_ONE(tl.v1);
        LIMIT_LO_TO_ZRO(tl.v1);
        LIMIT_HI_TO_ONE(tl.v2);
        LIMIT_LO_TO_ZRO(tl.v2);
    }

    // smart filter
    if( (pCtxt->TextureMode == D3DFILTER_LINEAR)
#if !DMASUPPORT
              &&  ( (D3DGLOBALPTR(pCtxt)->wDeviceId == D_S3VIRGEDXGX) ||
                    (D3DGLOBALPTR(pCtxt)->wDeviceId == D_S3VIRGEGX2)  ||
                    (D3DGLOBALPTR(pCtxt)->wDeviceId == D_S3M5 )     )
#endif  //!DMASUPPORT
                   ) {
       DXGXSmartFilter(p0,p1,p2, &(pCtxt->rndCommand), lpS3FVFOff );
    }

    /************************************************************/
    /*    TEXTURING PARAMETERS SETUP - PERSPECTIVE CORRECTION   */
    /************************************************************/
#if PERSPECTIVE
    if( (p2->rhw != p1->rhw) || (p0->rhw != p1->rhw) )    /* Perspective */
    {
        float uMin, vMin;

        tl.PerspectiveChange = FALSE;

        tl.u2 *= pCtxt->fTextureWidth;
        tl.u1 *= pCtxt->fTextureWidth;
        tl.u0 *= pCtxt->fTextureWidth;
        tl.v2 *= pCtxt->fTextureHeight;
        tl.v1 *= pCtxt->fTextureHeight;
        tl.v0 *= pCtxt->fTextureHeight;

        uMin = min3( tl.u0, tl.u1, tl.u2);
        vMin = min3( tl.v0, tl.v1, tl.v2);

       if (uMin < 0.0f)
                tl.uBase = DblRound(uMin - 1.0f);
        else
             tl.uBase = DblRound(uMin);

       if (vMin < 0.0f)
                tl.vBase = DblRound(vMin - 1.0f);
       else
                tl.vBase = DblRound(vMin);


        tl.Magic = (_PRECISION)magic;
        tl.w2 = p2->rhw;
        tl.w1 = p1->rhw;
        tl.w0 = p0->rhw;

        WaitFifo(pCtxt, 3);

        // filtering mode does not imply mip mapping
        _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dDdX, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dDdY, 0 );

        //find max{w[i]} and scale all w with magic/wMAX
        tl.wMax = max3( tl.w0, tl.w1, tl.w2 );
        tl.tmp = tl.Magic / tl.wMax;
        tl.w0 = tl.w0 * tl.tmp;
        tl.w1 = tl.w1 * tl.tmp;
        tl.w2 = tl.w2 * tl.tmp;

        tl.u0 = ( tl.u0 - tl.uBase ) * tl.w0;
        tl.u1 = ( tl.u1 - tl.uBase ) * tl.w1;
        tl.u2 = ( tl.u2 - tl.uBase ) * tl.w2;
        tl.v0 = ( tl.v0 - tl.vBase ) * tl.w0;
        tl.v1 = ( tl.v1 - tl.vBase ) * tl.w1;
        tl.v2 = ( tl.v2 - tl.vBase ) * tl.w2;

        tl.uBase -= (float)D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj;
        tl.vBase -= (float)D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj;

        if (pCtxt->fTextureWidth > 0.0f)
        while ( tl.uBase < 0 ) {
            tl.uBase += pCtxt->fTextureWidth;
        }


        if (pCtxt->fTextureHeight > 0.0f)
        while ( tl.vBase < 0 ) {
            tl.vBase += pCtxt->fTextureHeight;
        }

        tl.fDwY = (tl.w2-tl.w0)*tl.fDy02r;
        tl.fDwX = ( tl.w1 - (tl.fDwY*tl.fDy01 + tl.w0) ) * tl.fDxr;
        tl.fDvDy = (tl.v2-tl.v0)*tl.fDy02r;
        tl.fDvDx = ( tl.v1 - (tl.fDvDy*tl.fDy01 + tl.v0) ) * tl.fDxr;
        tl.fDuDy = (tl.u2-tl.u0)*tl.fDy02r;
        tl.fDuDx = ( tl.u1 - (tl.fDuDy*tl.fDy01 + tl.u0) ) * tl.fDxr;

        WaitFifo(pCtxt, 11 );

        _WRITE_REG_TRI_LONG( TRI_3D_bV, FLOAT_TO_TEXPOINT(pCtxt,tl.vBase) );
        _WRITE_REG_TRI_LONG( TRI_3D_bU,
            ( FLOAT_TO_TEXPOINT(pCtxt,tl.uBase) | _UBASEHIGH ) );

        _WRITE_REG_TRI_LONG( TRI_3D_dWdX, FLOAT_TO_1319(tl.fDwX) );
        _WRITE_REG_TRI_LONG( TRI_3D_dWdY, FLOAT_TO_1319(tl.fDwY) );
        _WRITE_REG_TRI_LONG( TRI_3D_WS,
            FLOAT_TO_1319(tl.w0 + tl.fDwY*tl.fDyCC) );

        _WRITE_REG_TRI_LONG( TRI_3D_dVdX,
            FLOAT_TO_2012(tl.fDvDx)>>D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX);
        _WRITE_REG_TRI_LONG( TRI_3D_dUdX,
            FLOAT_TO_2012(tl.fDuDx)>>D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX);

        _WRITE_REG_TRI_LONG( TRI_3D_dVdY,
            FLOAT_TO_2012(tl.fDvDy)>>D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX);
        _WRITE_REG_TRI_LONG( TRI_3D_dUdY,
            FLOAT_TO_2012(tl.fDuDy)>>D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX);

        _WRITE_REG_TRI_LONG( TRI_3D_VS,
            FLOAT_TO_2012(tl.v0 + tl.fDvDy*tl.fDyCC)>>D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX);
        _WRITE_REG_TRI_LONG( TRI_3D_US,
            FLOAT_TO_2012(tl.u0 + tl.fDuDy*tl.fDyCC)>>D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX);

#if DBG
        DPF_DBG("Pespective(%d,%d,%d,%d,%d,%d),(%d,%d,%d,%d,%d,%d),(%d,%d,%d,%d,%d,%d)    (uBase=%d,vBase=%d) (%d,%d,%d,%d,%d,%d,%d)",
            MYFLINT(p0->sx),tl.i0y,MYFLINT(tl.u0*100),MYFLINT(tl.v0*100),MYFLINT(tl.w0*100),MYFLINT(p0->sz*100),
            MYFLINT(p1->sx),tl.i1y,MYFLINT(tl.u1*100),MYFLINT(tl.v1*100),MYFLINT(tl.w1*100),MYFLINT(p1->sz*100),
            MYFLINT(p2->sx),tl.i2y,MYFLINT(tl.u2*100),MYFLINT(tl.v2*100),MYFLINT(tl.w2*100),MYFLINT(p2->sz*100),
            MYFLINT(tl.uBase*100),MYFLINT(tl.vBase*100),
            MYFLINT(tl.fDuDx*100),MYFLINT(tl.fDuDy*100),MYFLINT(tl.fDvDx*100),MYFLINT(tl.fDvDy*100),
            MYFLINT((tl.u0 + tl.fDuDy*tl.fDyCC)*100),
            MYFLINT((tl.v0 + tl.fDvDy*tl.fDyCC)*100),MYFLINT(tl.fDyCC*100));
#endif  //0
    } else  {
        //  No perspective correction needed
        tl.PerspectiveChange = TRUE;
        pCtxt->rndCommand &= ~(cmdCMD_TYPE_UnlitTexPersp - cmdCMD_TYPE_UnlitTex);
        pCtxt->dTexValtoInt = dTexValtoInt[pCtxt->rsfMaxMipMapLevel];
        WaitFifo(pCtxt, 11 );
        _WRITE_REG_TRI_LONG( TRI_3D_bU, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_bV, 0 );

#else   //PERSPECTIVE
    {
        // filtering mode does not imply mip mapping
        WaitFifo(pCtxt, 3 );
        _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dDdX, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dDdY, 0 );

#endif  //PERSPECTIVE

        /********************************************************/
        /*   TEXTURING PARAMETERS SETUP - TEXTURE COORDINATES   */
        /********************************************************/
        tl.u2 *= pCtxt->fTextureWidth;
        tl.u1 *= pCtxt->fTextureWidth;
        tl.u0 *= pCtxt->fTextureWidth;
        tl.v2 *= pCtxt->fTextureHeight;
        tl.v1 *= pCtxt->fTextureHeight;
        tl.v0 *= pCtxt->fTextureHeight;

        tl.fDvDy = (tl.v2-tl.v0)*tl.fDy02r;
        tl.fDvDx = ( tl.v1 - (tl.fDvDy*tl.fDy01 + tl.v0) ) * tl.fDxr;
        tl.fDuDy = (tl.u2-tl.u0)*tl.fDy02r;
        tl.fDuDx = ( tl.u1 - (tl.fDuDy*tl.fDy01 + tl.u0) ) * tl.fDxr;

#if !PERSPECTIVE
        WaitFifo(pCtxt, 6 );
#endif  //!PERSPECTIVE

        _WRITE_REG_TRI_LONG( TRI_3D_dVdX, FLOAT_TO_TEXPOINT(pCtxt,tl.fDvDx) );
        _WRITE_REG_TRI_LONG( TRI_3D_dUdX, FLOAT_TO_TEXPOINT(pCtxt,tl.fDuDx) );
        _WRITE_REG_TRI_LONG( TRI_3D_dVdY, FLOAT_TO_TEXPOINT(pCtxt,tl.fDvDy) );
        _WRITE_REG_TRI_LONG( TRI_3D_dUdY, FLOAT_TO_TEXPOINT(pCtxt,tl.fDuDy) );
        _WRITE_REG_TRI_LONG( TRI_3D_VS, FLOAT_TO_TEXPOINT(pCtxt,tl.v0 + tl.fDvDy*tl.fDyCC - D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj) );
        _WRITE_REG_TRI_LONG( TRI_3D_US, FLOAT_TO_TEXPOINT(pCtxt,tl.u0 + tl.fDuDy*tl.fDyCC - D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj) );

    }
#else   //TEXTURED

#if FOGGED

    // special case fogged without texture

    WaitFifo(pCtxt, 8 );
    _WRITE_REG_TRI_LONG( TRI_3D_dVdX, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dUdX, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dDdY, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dVdY, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dUdY, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_VS, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_US, 0 );
#endif  //FOGGED
#endif  //TEXTURED

    /****************************************************************/
    /*                   COLOR AND ALPHA SETUP                      */
    /****************************************************************/


#if ZBUFFER
    // Wait for enough fifo slots for color parameters
    WaitFifo(pCtxt, 6 );
#else   //ZBUFFER
    // Wait for enough fifo slots for color & coord parameters
    WaitFifo(pCtxt, 15 );
#endif  //ZBUFFER

#if GOURAUD
    if(pCtxt->Alpha_workaround) {
        tl.Color2 = FVFCOLOR(p2, dwColorOffs)->color | 0xff000000;
        tl.Color1 = FVFCOLOR(p1, dwColorOffs)->color | 0xff000000;
        tl.Color0 = FVFCOLOR(p0, dwColorOffs)->color | 0xff000000;
    } else {
        tl.Color2 = FVFCOLOR(p2, dwColorOffs)->color;
        tl.Color1 = FVFCOLOR(p1, dwColorOffs)->color;
        tl.Color0 = FVFCOLOR(p0, dwColorOffs)->color;
    }
#if FOGGED
    // get the right alpha parameters based on fog & specular highlights flags
    if (pCtxt->bSpecular) {
        tl.fAdY =  ((float)(D3D_GETSPECULAR(FVFSPEC(p2, dwSpecOffs)->specular) -
            D3D_GETSPECULAR(FVFSPEC(p0, dwSpecOffs)->specular)) * tl.fDy02r);
        tl.fAdX =  - (((float)(D3D_GETSPECULAR(FVFSPEC(p1, dwSpecOffs)->specular) -
            D3D_GETSPECULAR(FVFSPEC(p0, dwSpecOffs)->specular)) - tl.fAdY*tl.fDy01) * tl.fDxr);
        tl.fAdY = - tl.fAdY;
        tl.As = 255 - D3D_GETSPECULAR(FVFSPEC(p0, dwSpecOffs)->specular);
    } else {
        tl.fAdY = (float)(D3D_GETALPHA(FVFSPEC(p2, dwSpecOffs)->specular) -
            D3D_GETALPHA(FVFSPEC(p0, dwSpecOffs)->specular)) * tl.fDy02r;
        tl.fAdX = ((float)(D3D_GETALPHA(FVFSPEC(p1, dwSpecOffs)->specular) -
            D3D_GETALPHA(FVFSPEC(p0, dwSpecOffs)->specular)) - tl.fAdY*tl.fDy01) * tl.fDxr;
        tl.As = D3D_GETALPHA(FVFSPEC(p0, dwSpecOffs)->specular);
    }
#else   //FOGGED
    if(pCtxt->rndCommand & cmdALP_BLD_CTL_SrcAlph) {
        tl.fAdY = (float)(D3D_GETALPHA(tl.Color2) -
            D3D_GETALPHA(tl.Color0)) * tl.fDy02r;
        tl.fAdX = ((float)(D3D_GETALPHA(tl.Color1) -
            D3D_GETALPHA(tl.Color0)) - tl.fAdY*tl.fDy01) * tl.fDxr;
        tl.As = D3D_GETALPHA(tl.Color0);
    } else {
        tl.fAdY = 0.0f;
        tl.fAdX = 0.0f;
        tl.As = 255;
    }
#endif  //FOGGED

    tl.fGdY = (float)(D3D_GETGREEN(tl.Color2) - D3D_GETGREEN(tl.Color0)) * tl.fDy02r;
    tl.fGdX = ((float)(D3D_GETGREEN(tl.Color1) - D3D_GETGREEN(tl.Color0)) - tl.fGdY*tl.fDy01) * tl.fDxr;
    tl.fBdY = (float)(D3D_GETBLUE(tl.Color2) - D3D_GETBLUE(tl.Color0)) * tl.fDy02r;
    tl.fBdX = ((float)(D3D_GETBLUE(tl.Color1) - D3D_GETBLUE(tl.Color0)) - tl.fBdY*tl.fDy01) * tl.fDxr;
    tl.fRdY = (float)(D3D_GETRED(tl.Color2) - D3D_GETRED(tl.Color0)) * tl.fDy02r;
    tl.fRdX = ((float)(D3D_GETRED(tl.Color1) - D3D_GETRED(tl.Color0)) - tl.fRdY*tl.fDy01) * tl.fDxr;

    // Write out color interpolation parameters for gouraud shading
    _WRITE_REG_TRI_LONG( TRI_3D_dGdX_dBdX, (FLOAT_TO_87( tl.fGdX ) << 16) |
                                          (FLOAT_TO_87( tl.fBdX ) & 0xFFFF) );
    _WRITE_REG_TRI_LONG( TRI_3D_dAdX_dRdX, (FLOAT_TO_87( tl.fAdX ) << 16) |
                                          (FLOAT_TO_87( tl.fRdX ) & 0xFFFF) );
    _WRITE_REG_TRI_LONG( TRI_3D_dGdY_dBdY, (FLOAT_TO_87( tl.fGdY ) << 16) |
                                          (FLOAT_TO_87( tl.fBdY ) & 0xFFFF) );
    _WRITE_REG_TRI_LONG( TRI_3D_dAdY_dRdY, (FLOAT_TO_87( tl.fAdY ) << 16) |
                                          (FLOAT_TO_87( tl.fRdY ) & 0xFFFF) );
    _WRITE_REG_TRI_LONG( TRI_3D_GS_BS,
        (COLOR_TO_87( D3D_GETGREEN(tl.Color0) ) << 16) |
         COLOR_TO_87( D3D_GETBLUE(tl.Color0) ) );
    _WRITE_REG_TRI_LONG( TRI_3D_AS_RS,
        (COLOR_TO_87( tl.As ) << 16) |
         COLOR_TO_87( D3D_GETRED(tl.Color0) )  );
#else  //GOURAUD

    if(pCtxt->Alpha_workaround)
        tl.Color0 = FVFCOLOR(tl.pFlat, dwColorOffs)->color | 0xff000000;
    else
        tl.Color0 = FVFCOLOR(tl.pFlat, dwColorOffs)->color;

#if  FOGGED
    if (pCtxt->bSpecular)
        tl.As = 255 - D3D_GETSPECULAR(FVFSPEC(tl.pFlat, dwSpecOffs)->specular);
    else
        tl.As = D3D_GETALPHA(FVFSPEC(tl.pFlat, dwSpecOffs)->specular);
#else //FOGGED
    if(pCtxt->rndCommand & cmdALP_BLD_CTL_SrcAlph)
        tl.As = D3D_GETALPHA(tl.Color0);
    else
        tl.As = 255;
#endif //FOGGED

    // Write out color parameters for flat shading
    _WRITE_REG_TRI_LONG( TRI_3D_dGdX_dBdX, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dAdX_dRdX, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dGdY_dBdY, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dAdY_dRdY, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_GS_BS,
        (COLOR_TO_87( D3D_GETGREEN(tl.Color0) ) << 16) |
         COLOR_TO_87( D3D_GETBLUE(tl.Color0) ) );
    _WRITE_REG_TRI_LONG( TRI_3D_AS_RS,
        (COLOR_TO_87( tl.As ) << 16) |
         COLOR_TO_87( D3D_GETRED(tl.Color0) ) );
#endif  //GOURAUD


    /****************************************************************/
    /*                        Z BUFFER SETUP                        */
    /****************************************************************/

#if ZBUFFER
    // Z setup
    tl.fDzY = (p2->sz - p0->sz) * tl.fDy02r;
    tl.fDzX = (p1->sz - (tl.fDzY*tl.fDy01 + p0->sz)) * tl.fDxr;
    // Wait for enough fifo slots for coord & z parameters
    WaitFifo(pCtxt, 12 );
    _WRITE_REG_TRI_LONG( TRI_3D_dZdX, MYFLINT31( tl.fDzX ) );
    _WRITE_REG_TRI_LONG( TRI_3D_dZdY, MYFLINT31( tl.fDzY ) );
    _WRITE_REG_TRI_LONG( TRI_3D_ZS02, MYFLINT31( p0->sz + (tl.fDyCC * tl.fDzY)) );
#endif  //ZBUFFER

    /****************************************************************/
    /*                    XY COORDINATE SETUP                       */
    /****************************************************************/

    // Draw the triangle

    _WRITE_REG_TRI_LONG( TRI_3D_dXdY12, FLOAT_TO_1220(tl.fDxDy12) );
    _WRITE_REG_TRI_LONG( TRI_3D_XEnd12,
        FLOAT_TO_1220( p1->sx  + (tl.fDxDy12 * (p1->sy - (float)tl.i1y)) ) );
    _WRITE_REG_TRI_LONG( TRI_3D_dXdY01, FLOAT_TO_1220(tl.fDxDy01) );
    _WRITE_REG_TRI_LONG( TRI_3D_XEnd01,
        FLOAT_TO_1220( p0->sx + (tl.fDxDy01 * tl.fDyCC)) );
    _WRITE_REG_TRI_LONG( TRI_3D_dXdY02, FLOAT_TO_1220( tl.fDxDy02 ) );
    _WRITE_REG_TRI_LONG( TRI_3D_XStart02,
        FLOAT_TO_1220( (tl.fDxDy02 * tl.fDyCC) + p0->sx ) );
    _WRITE_REG_TRI_LONG( TRI_3D_YStart, tl.i0y );
    _WRITE_REG_TRI_LONG( TRI_3D_Y01_Y12,
        (tl.iDy01 << 16) | (tl.iDy12 + (p2->sy == (float)tl.i2y) ) |
        ((tl.fDx > 0) ? 0x80000000 : 0) );

    /****************************************************************/
    /*                    HW RENDER TRIANGLE                        */
    /****************************************************************/

    _ADVANCE_INDEX( DATA_SIZE );
    _WRITE_S3D_HEADER ( CMD_SIZE );
    _WRITE_REG_SET_LONG(pCtxt, CMD_SET, pCtxt->rndCommand );
    _ADVANCE_INDEX( CMD_SIZE );

    _START_DMA;

#if PERSPECTIVE
    if(tl.PerspectiveChange) {
        tl.PerspectiveChange  = FALSE;
        pCtxt->dTexValtoInt = dTexValtoIntPerspective[pCtxt->rsfMaxMipMapLevel];
        pCtxt->rndCommand  |= (cmdCMD_TYPE_UnlitTexPersp - cmdCMD_TYPE_UnlitTex);
    }
#endif //PERSPECTIVE

    return;
}

#undef GOURAUD
#undef DMASUPPORT
#undef ZBUFFER
#undef TEXTURED
#undef PERSPECTIVE
#undef FOGGED
#undef FNAME
#undef WRAPCODE

#pragma optimize( "a", off )

