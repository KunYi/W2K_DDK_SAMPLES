/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dline.c
*
*  Content:    Direct3D line rasterization code.
*
* Copyright (C) 1995-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"
#include "hw3d.h"

//*****************************************************************************
//
// void _HWRenderLine
//
//*****************************************************************************
void __HWRenderLine( S3_CONTEXT *pCtxt, LPD3DTLVERTEX    p0,
                                        LPD3DTLVERTEX    p1,
                                        LPD3DTLVERTEX    pFlat,
                                        LPS3FVFOFFSETS   lpS3FVFOff)
{
    int iDy01;
    int i1y, i0y;
    _PRECISION fDxr, fDx;
    double fDzY;
    double fRdY, fGdY, fBdY, fAdY;
    BYTE aS;
    D3DCOLOR Color1, Color0;
    _PRECISION fDyCC;
    _PRECISION fDy01, fDx01;
    _PRECISION fDxDy01, fDy01r;
    BOOL bXMajor;
    char *_TRI_BASE=(char*)D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DTri;
    DWORD dwColorOffs, dwSpecOffs, dwTexOffs;

    // Handle textured lines as a special case of triangles
    if (pCtxt->dwRCode & S3TEXTURED)
    {
        FVFVERTEX TLv2;
        __CpyFVFVertexes( (LPD3DTLVERTEX)&TLv2, p0 , lpS3FVFOff);
        TLv2.TLvtx.sx += 1.0;
        TLv2.TLvtx.sy += 1.0;
        pRenderTriangle[pCtxt->dwRCode](pCtxt, p0, p1, (LPD3DTLVERTEX)&TLv2,
                                                                lpS3FVFOff);

        __CpyFVFVertexes( (LPD3DTLVERTEX)&TLv2, p1 , lpS3FVFOff);
        TLv2.TLvtx.sx += 1.0;
        TLv2.TLvtx.sy += 1.0;
        pRenderTriangle[pCtxt->dwRCode](pCtxt, p0, p1, (LPD3DTLVERTEX)&TLv2,
                                                                lpS3FVFOff);

        return;
    }

    // order the vertices
    if( p1->sy > p0->sy )
        PTRSWAP((ULONG_PTR)p1, (ULONG_PTR)p0 );

    __SetFVFOffsets(&dwColorOffs,&dwSpecOffs,&dwTexOffs,lpS3FVFOff);

    // Verify the vertex screen coordinates are within
    // reasonable limits for the Virge Fifo issue.
    CHECK_VERTEX_VALUE(p0);
    CHECK_VERTEX_VALUE(p1);

    // calculate integer y deltas
    i1y = MYFLINT(p1->sy);
    iDy01 = (i0y = MYFLINT(p0->sy)) - i1y;
    fDy01 = p0->sy - p1->sy;

    if( iDy01 == 0 ) {
        iDy01 = 1;
        fDy01 = 1.0f;
    }

    fDyCC = p0->sy - (float)i0y;
    fDxDy01 = (p1->sx - p0->sx)/(float)iDy01;
    fDx01 = fDxDy01;
    fDx = p1->sx - p0->sx;

    if( fDx < -1/524188. ) {
        if( -fDx < fDy01 )
            fDx01 = -1.0f;
        fDxr = (_PRECISION)-1.0 / fDx;
    } else if (fDx > 1/524188.) {
        if( fDx < fDy01 )
            fDx01 = 1.0f;
        fDxr = (_PRECISION)1.0 / fDx;
    } else {
        //fix missing vertical lines
        fDx = fDxr = 1.0f;
        if( 1.0 < fDy01 )
            fDx01 = 1.0f;
    }

    if (fDxDy01>1.0) {
        fDy01r = (_PRECISION)1.0/(float)iDy01;
        bXMajor=FALSE;
    } else {
        fDy01r = fDxr;
        bXMajor=TRUE;
    }

    _WRITE_S3D_HEADER ( DATA_SIZE );

    if( (pCtxt->dwTexture && !(pCtxt->dwRCode & S3PERSPECTIVE)) ||
        ((pCtxt->bFogEnabled || pCtxt->bSpecular) && !pCtxt->dwTexture) )
    {
        S3DFifoWait(  pCtxt->ppdev,  2 );
        _WRITE_REG_TRI_LONG( TRI_3D_bV, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_bU,
                         ( D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh ) );
    }

    if( pCtxt->bFogEnabled || pCtxt->bSpecular) {
    // special case fogged without texture
        S3DFifoWait(  pCtxt->ppdev,  8 );
        _WRITE_REG_TRI_LONG( TRI_3D_dVdX, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dUdX, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dDdY, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dVdY, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dUdY, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_VS, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_US, 0 );
    }

    // Color setup
    if( (pCtxt->ShadeMode == D3DSHADE_GOURAUD) ) {
        if(pCtxt->Alpha_workaround) {
           Color1 = FVFCOLOR(p1, dwColorOffs)->color | 0xff000000;
           Color0 = FVFCOLOR(p0, dwColorOffs)->color | 0xff000000;
        } else {
           Color1 = FVFCOLOR(p1, dwColorOffs)->color;
           Color0 = FVFCOLOR(p0, dwColorOffs)->color;
        }


        if (pCtxt->bSpecular) {
             fAdY = -((double)(D3D_GETSPECULAR(FVFSPEC(p1, dwSpecOffs)->specular) -
                    D3D_GETSPECULAR(FVFSPEC(p0, dwSpecOffs)->specular)) * fDy01r);
             aS = (BYTE)255 - D3D_GETSPECULAR(FVFSPEC(p0, dwSpecOffs)->specular);
        } else if( pCtxt->bFogEnabled ) {
            fAdY = (double)(D3D_GETALPHA(FVFSPEC(p1, dwSpecOffs)->specular) -
                    D3D_GETALPHA(FVFSPEC(p0, dwSpecOffs)->specular)) * fDy01r;
            aS = D3D_GETALPHA(FVFSPEC(p0, dwSpecOffs)->specular);
        } else if(pCtxt->rndCommand & cmdALP_BLD_CTL_SrcAlph){
            fAdY = (double)(D3D_GETALPHA(Color1) -
                    D3D_GETALPHA(Color0)) * fDy01r;
            aS = D3D_GETALPHA(Color0);
        } else {
            fAdY = 0.0;
            aS = (BYTE)255;
        }


        fGdY = (double)(D3D_GETGREEN(Color1) - D3D_GETGREEN(Color0)) * fDy01r;
        fBdY = (double)(D3D_GETBLUE(Color1)  - D3D_GETBLUE(Color0)) * fDy01r;
        fRdY = (double)(D3D_GETRED(Color1)   - D3D_GETRED(Color0)) * fDy01r;

        S3DFifoWait(  pCtxt->ppdev,  6 );
        if (bXMajor) {
            _WRITE_REG_TRI_LONG( TRI_3D_dGdY_dBdY, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_dAdY_dRdY, 0);
            _WRITE_REG_TRI_LONG( TRI_3D_dGdX_dBdX, (FLOAT_TO_87( fGdY ) << 16) |
                                  (FLOAT_TO_87( fBdY ) & 0xFFFF) );
            _WRITE_REG_TRI_LONG( TRI_3D_dAdX_dRdX, (FLOAT_TO_87( fAdY ) << 16) |
                                  (FLOAT_TO_87( fRdY ) & 0xFFFF) );
         } else {
            _WRITE_REG_TRI_LONG( TRI_3D_dGdX_dBdX, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_dAdX_dRdX, 0);
            _WRITE_REG_TRI_LONG( TRI_3D_dGdY_dBdY, (FLOAT_TO_87( fGdY ) << 16) |
                                  (FLOAT_TO_87( fBdY ) & 0xFFFF) );
            _WRITE_REG_TRI_LONG( TRI_3D_dAdY_dRdY, (FLOAT_TO_87( fAdY ) << 16) |
                                  (FLOAT_TO_87( fRdY ) & 0xFFFF) );
         }
    } else {
       if(pCtxt->Alpha_workaround)
           Color0 = FVFCOLOR(pFlat, dwColorOffs)->color | 0xff000000;
       else
           Color0 = FVFCOLOR(pFlat, dwColorOffs)->color;

        if (pCtxt->bSpecular)
             aS = (BYTE)255 - D3D_GETSPECULAR(FVFSPEC(p0, dwSpecOffs)->specular);
        else if( pCtxt->bFogEnabled )
             aS = D3D_GETALPHA(FVFSPEC(pFlat, dwSpecOffs)->specular);
        else if(pCtxt->rndCommand & cmdALP_BLD_CTL_SrcAlph)
             aS = D3D_GETALPHA(Color0);
        else
             aS = 255;

        S3DFifoWait(  pCtxt->ppdev,  6 );
        _WRITE_REG_TRI_LONG( TRI_3D_dGdX_dBdX, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dAdX_dRdX, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dGdY_dBdY, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_dAdY_dRdY, 0 );
    }

    _WRITE_REG_TRI_LONG( TRI_3D_GS_BS,
        (COLOR_TO_87( D3D_GETGREEN(Color0) ) << 16) |
         COLOR_TO_87( D3D_GETBLUE(Color0) ) );
    _WRITE_REG_TRI_LONG( TRI_3D_AS_RS,
        (COLOR_TO_87( aS ) << 16) |
         COLOR_TO_87( D3D_GETRED(Color0) )  );

    // Z setup
    if( pCtxt->bZEnabled ) {
         S3DFifoWait(  pCtxt->ppdev,  3 );
         fDzY = (p1->sz - p0->sz) * fDy01r;
         if (bXMajor) {
            _WRITE_REG_TRI_LONG( TRI_3D_dZdY, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_dZdX, MYFLINT31( fDzY ) );
         } else {
            _WRITE_REG_TRI_LONG( TRI_3D_dZdX, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_dZdY, MYFLINT31( fDzY ) );
         }
         _WRITE_REG_TRI_LONG( TRI_3D_ZS02, MYFLINT31( p0->sz + (fDyCC * fDzY)) );
    }


    // Draw the line

    S3DFifoWait(  pCtxt->ppdev,  10 );
    _WRITE_REG_TRI_LONG( TRI_3D_dXdY12, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_XEnd12, 0 );
    _WRITE_REG_TRI_LONG( TRI_3D_dXdY01, FLOAT_TO_1220(fDxDy01) );
    _WRITE_REG_TRI_LONG( TRI_3D_XEnd01, FLOAT_TO_1220( p0->sx + fDx01 ) );
    _WRITE_REG_TRI_LONG( TRI_3D_dXdY02, FLOAT_TO_1220( fDxDy01 ) );
    _WRITE_REG_TRI_LONG( TRI_3D_XStart02, FLOAT_TO_1220( p0->sx ) );
    _WRITE_REG_TRI_LONG( TRI_3D_YStart, i0y );
    _WRITE_REG_TRI_LONG( TRI_3D_Y01_Y12,
    (iDy01 << 16) | ((fDx > 0) ? 0x80000000 : 0) );

    _ADVANCE_INDEX( DATA_SIZE );
    _WRITE_S3D_HEADER ( CMD_SIZE );

    _WRITE_REG_SET_LONG(pCtxt, D3DGLOBALPTR(pCtxt)->D3DGlobals.CMD_SET,
                                                         pCtxt->rndCommand );

    _ADVANCE_INDEX( CMD_SIZE );
    _START_DMA;
}



