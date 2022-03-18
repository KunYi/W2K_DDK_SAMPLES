/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dpoint.c
*
*  Content:    Direct3D hw point rasterization code.
*
* Copyright (C) 1996-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"
#include "hw3d.h"

//*****************************************************************************
//
// void __HWRenderPoint
//
//*****************************************************************************
void __HWRenderPoint(S3_CONTEXT *pCtxt,
                     LPD3DTLVERTEX p0,
                     LPS3FVFOFFSETS lpS3FVFOff)
{
    int i0y;
    // necessary variable for _WRITE_REG_TRI_LONG to work
    char *_TRI_BASE = (char*)D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DTri;
    BYTE aS;
    DWORD dwColorOffs,dwSpecularOffs,dwTexOffs;

    __SetFVFOffsets(&dwColorOffs,&dwSpecularOffs,&dwTexOffs,lpS3FVFOff);

    // Verify the vertex screen coordinates are within
    // reasonable limits for the Virge Fifo issue.
    CHECK_VERTEX_VALUE(p0);

    i0y = MYFLINT(p0->sy);

    _WRITE_S3D_HEADER ( DATA_SIZE );

    // Setup texturing parameters for points which may require it
    if( pCtxt->dwRCode & S3TEXTURED )
    {
        _PRECISION u0;
        _PRECISION v0;
        _PRECISION w0, uBase, vBase;

        u0 = FVFTEX(p0,dwTexOffs)->tu;
        v0 = FVFTEX(p0,dwTexOffs)->tv;

        u0 *= pCtxt->fTextureWidth;
        v0 *= pCtxt->fTextureHeight;
        if( !(pCtxt->dwRCode & S3PERSPECTIVE)) {
            S3DFifoWait(pCtxt->ppdev, 5);
            _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_VS,
                FLOAT_TO_TEXPOINT(pCtxt,v0 + 0.501) );
            _WRITE_REG_TRI_LONG( TRI_3D_US,
                FLOAT_TO_TEXPOINT(pCtxt,u0 + 0.501) );
            _WRITE_REG_TRI_LONG(TRI_3D_bV, 0);
            _WRITE_REG_TRI_LONG(TRI_3D_bU,
                ( D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh ));
        } else {
            w0 = p0->rhw;
            uBase = u0;
            vBase = v0;
            while ( uBase < 0 ) {
                uBase += pCtxt->fTextureWidth;
            }
            while ( vBase < 0 ) {
                vBase += pCtxt->fTextureHeight;
            }
            S3DFifoWait(pCtxt->ppdev,  6);

            _WRITE_REG_TRI_LONG( TRI_3D_bV, FLOAT_TO_TEXPOINT(pCtxt,vBase) );
            _WRITE_REG_TRI_LONG( TRI_3D_bU,
                ( FLOAT_TO_TEXPOINT(pCtxt,uBase) |
                  D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh ) );
            _WRITE_REG_TRI_LONG( TRI_3D_WS, FLOAT_TO_1319(w0) );
            _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_VS, 0 );
            _WRITE_REG_TRI_LONG( TRI_3D_US, 0 );
        }
    } else if( pCtxt->bFogEnabled || pCtxt->bSpecular ) {
        // special case fogged without texture

        S3DFifoWait(pCtxt->ppdev, 5);
        _WRITE_REG_TRI_LONG( TRI_3D_DS, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_VS, 0 );
        _WRITE_REG_TRI_LONG( TRI_3D_US, 0 );
        _WRITE_REG_TRI_LONG(TRI_3D_bV, 0);
        _WRITE_REG_TRI_LONG(TRI_3D_bU,
            ( D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh ));
    }

    // Alpha value determination
    if( pCtxt->bSpecular )
        aS = 255 - D3D_GETSPECULAR(FVFSPEC(p0, dwSpecularOffs)->specular);
    else if( pCtxt->bFogEnabled )
        aS = D3D_GETALPHA(FVFSPEC(p0, dwSpecularOffs)->specular);
    else if( pCtxt->ShadeMode == D3DSHADE_GOURAUD )
        aS = D3D_GETALPHA(FVFCOLOR(p0, dwColorOffs)->color);
    else
        aS = 255;

    // Set RGBA values of the point
    S3DFifoWait(pCtxt->ppdev,  8);
    _WRITE_REG_TRI_LONG( TRI_3D_GS_BS,
        (COLOR_TO_87( D3D_GETGREEN(FVFCOLOR(p0, dwColorOffs)->color) ) << 16) |
         COLOR_TO_87( D3D_GETBLUE(FVFCOLOR(p0, dwColorOffs)->color) ) );
    _WRITE_REG_TRI_LONG( TRI_3D_AS_RS,
        (COLOR_TO_87( aS ) << 16) |
         COLOR_TO_87( D3D_GETRED(FVFCOLOR(p0, dwColorOffs)->color) )  );

    // Set Z buffer value if necessary
    if( pCtxt->bZEnabled )
        _WRITE_REG_TRI_LONG( TRI_3D_ZS02, MYFLINT31( p0->sz ) );

    // Setup X & Y Coordinates for point
    _WRITE_REG_TRI_LONG( TRI_3D_XEnd01,   FLOAT_TO_1220( p0->sx + 1.0 ) );
    _WRITE_REG_TRI_LONG( TRI_3D_XStart02, FLOAT_TO_1220( p0->sx ) );
    _WRITE_REG_TRI_LONG( TRI_3D_YStart,   i0y );
    _WRITE_REG_TRI_LONG( TRI_3D_Y01_Y12,  (1 << 16) | 0x80000000 );

    // Render it
    _ADVANCE_INDEX( DATA_SIZE );
    _WRITE_S3D_HEADER ( CMD_SIZE );
    _WRITE_REG_SET_LONG(pCtxt,
        D3DGLOBALPTR(pCtxt)->D3DGlobals.CMD_SET, pCtxt->rndCommand );
    _ADVANCE_INDEX( CMD_SIZE );
    _START_DMA;
}


