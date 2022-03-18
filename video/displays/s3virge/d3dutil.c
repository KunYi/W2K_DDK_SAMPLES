/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3dutil.c
*
*  Content:    D3D Driver utility functions
*
* Copyright (C) 1996-1998. Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"
#include "hw3d.h"    // Needed for ReadCrReg under VC 4.2

// Definition of auxiliary conversion tables for floating to fixed point
double dTexValtoInt[] = {
                          
    TWOPOW(26)+TWOPOW(25),                   //   6.26     // 0      1x1
    TWOPOW(27)+TWOPOW(26),                   //   7.25     // 1      2x2
    TWOPOW(28)+TWOPOW(27),                   //   8.24     // 2      4x4
    TWOPOW(29)+TWOPOW(28),                   //   9.23     // 3      8x8
    TWOPOW(30)+TWOPOW(29),                   //  10.22     // 4     16x16
    TWOPOW(31)+TWOPOW(30),                   //  11.21     // 5     32x32
    TWOPOW(32)+TWOPOW(31),                   //  12.20     // 6     64x64
    TWOPOW(33)+TWOPOW(32),                   //  13.19     // 7    128x128
    TWOPOW(34)+TWOPOW(33),                   //  14.18     // 8    256x256
    TWOPOW(35)+TWOPOW(34)                    //  15.17     // 9    512x512
};


double dTexValtoIntPerspective[] = {
                          
    TWOPOW(37)+TWOPOW(36),                   //  17.15    // 0      1x1
    TWOPOW(38)+TWOPOW(37),                   //  18.14    // 1      2x2
    TWOPOW(39)+TWOPOW(38),                   //  19.13    // 2      4x4
    TWOPOW(40)+TWOPOW(39),                   //  20.12    // 3      8x8
    TWOPOW(41)+TWOPOW(40),                   //  21.11    // 4     16x16
    TWOPOW(42)+TWOPOW(41),                   //  22.10    // 5     32x32
    TWOPOW(43)+TWOPOW(42),                   //  23.9     // 6     64x64
    TWOPOW(44)+TWOPOW(43),                   //  24.8     // 7    128x128
    TWOPOW(45)+TWOPOW(44),                   //  25.7     // 8    256x256
    TWOPOW(46)+TWOPOW(45),                   //  24.6     // 9    512x512
    TWOPOW(47)+TWOPOW(46),
};



//******************************************************************
//
// BOOL GetChromaValue
//
// Checks the surfaces defined current chroma key value.This is a 
// texture based transparency and is enabled using the 
// render state D3DRENDERSTATE_ALPHABLENDENABLE. The chroma value is 
// defined by the source color key on the surface representing 
// the texture, if the R, G, and B components lie within (inclusive) 
// the range of the color key low and high values, then that pixel 
// is transparent. 
//
//******************************************************************
BOOL GetChromaValue(LPDDRAWI_DDRAWSURFACE_LCL lpLcl, 
                    LPDWORD lpdwChromaLow, LPDWORD lpdwChromaHigh)
{
    if (!(lpLcl ->dwFlags & DDRAWISURF_HASCKEYSRCBLT)){
        return FALSE;
    }

    *lpdwChromaLow = lpLcl->ddckCKSrcBlt.dwColorSpaceLowValue;
    *lpdwChromaHigh = lpLcl->ddckCKSrcBlt.dwColorSpaceHighValue;
    return TRUE;
}

//*****************************************************************************
//
// BOOL GenericWireFrameTriangle
//
// Draw ONE wireframe triangle 
//
//*****************************************************************************
void GenericWireFrameTriangle (S3_CONTEXT     *pCtxt,
                               LPD3DTLVERTEX  p0,
                               LPD3DTLVERTEX  p1,
                               LPD3DTLVERTEX  p2,
                               LPS3FVFOFFSETS   lpS3FVFOff)
{
    __HWRenderLine( pCtxt, p0, p1, p0, lpS3FVFOff );
    __HWRenderLine( pCtxt, p1, p2, p0, lpS3FVFOff );
    __HWRenderLine( pCtxt, p2, p0, p0, lpS3FVFOff );
    return;
}

//*****************************************************************************
//
// BOOL GenericPointTriangle
//
// Draw ONE triangles vertex points
//
//*****************************************************************************
void GenericPointTriangle (S3_CONTEXT     *pCtxt,
                               LPD3DTLVERTEX  p0,
                               LPD3DTLVERTEX  p1,
                               LPD3DTLVERTEX  p2,
                               LPS3FVFOFFSETS   lpS3FVFOff)
{
    __HWRenderPoint( pCtxt, p0, lpS3FVFOff );
    __HWRenderPoint( pCtxt, p1, lpS3FVFOff );
    __HWRenderPoint( pCtxt, p2, lpS3FVFOff );
    return;
}


#define __UVBASEDEL          0

//*****************************************************************************
//
// HRESULT S3GetDeviceId
//
// Finds out what specific chip & revision were running in.
//*****************************************************************************
HRESULT S3GetDeviceRevision(PPDEV ppdev,WORD *pId ,ULONG *pRevision)
{
    ULONG revision;
    WORD id;
    int i;

    ULONG IDs[ ] = {
        D_S3VIRGE,
        D_S3VIRGEVX,
        D_S3VIRGEDXGX,
        D_S3VIRGEGX2,
        D_S3M3,
        D_S3M5 
    };

    // test for type of 3D chip 

    ACQUIRE_CRTC_CRITICAL_SECTION( ppdev );
    VGAOUTPW( ppdev, SEQ_INDEX,  SEQ_REG_UNLOCK );
    revision = ReadCrReg( ppdev, 0x2F);

    id      = (WORD)(ReadCrReg( ppdev, 0x2E) << 8) |
                    (ReadCrReg( ppdev, 0x2D));
    VGAOUTPW( ppdev, SEQ_INDEX,  SEQ_REG_LOCK );
    RELEASE_CRTC_CRITICAL_SECTION( ppdev );

    for( i = 0; i < sizeof(IDs)/sizeof(ULONG); i++ ) {
        if ( IDs[ i ] == id ) {
            break;
        }
    }
    if ( i >= sizeof(IDs)/sizeof(ULONG) ) {
        return DDERR_CURRENTLYNOTAVAIL;
    }

    *pId = id;
    *pRevision = revision;
    return DD_OK;

}

//*****************************************************************************
//
// HRESULT S3VirgeInit
//
// This implementation specific initialization routine determines the chip 
// type being used, whether DMA is possible, unlocks the chip, gets the 
// physical address of the video buffer and returns DD_OK if all this is done 
// successfully. 
//*****************************************************************************
HRESULT S3VirgeInit(S3_CONTEXT *pCtxt)
{
    ULONG_PTR VideoBufferLinearAddr;
    ULONG revision;
    WORD id;
    int i, dma_possible;

    if (S3GetDeviceRevision(D3DGLOBALPTR(pCtxt),&id,&revision) == DD_OK)
    {
        D3DGLOBALPTR(pCtxt)->wDeviceId = id;
    }
    else
    {
        return DDERR_CURRENTLYNOTAVAIL;
    }


    // Determine what chips are dma capable, and set some 
    // pecific constants needed depending on the chipset
    D3DGLOBALPTR(pCtxt)->D3DGlobals.dma_possible    = FALSE;

    if ( (D3DGLOBALPTR(pCtxt)->wDeviceId !=  D_S3VIRGE) && 
         (D3DGLOBALPTR(pCtxt)->wDeviceId !=  D_S3VIRGEVX) ) {
        if (D3DGLOBALPTR(pCtxt)->wDeviceId == D_S3VIRGEDXGX) {
            if ( revision == 1 ) { 
            } else if ( revision == 2 ) { 
                // Rev C is 2
                dma_possible = TRUE;
            } else { 
                // Rev A is 0
                dma_possible = FALSE;
            }
        } else {
            dma_possible    = TRUE;
        }

        D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX      = 4;            //FLINT24:8

        //ViRGE DX/GX without D change
        D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh = 0x80000000;   
        if (D3DGLOBALPTR(pCtxt)->wDeviceId == D_S3VIRGEDXGX)
            D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj= -TEXTURE_FACTOR*0.99f;
        else
            D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj= 0.000f;
        D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE = ( 2048. - __UVBASEDEL ) ;
    } else {
        // Virge & Virge VX
        D3DGLOBALPTR(pCtxt)->D3DGlobals.DXGX      = 0;             //FLINT20:12
        D3DGLOBALPTR(pCtxt)->D3DGlobals.uBaseHigh = 0;
        //fixed wrong offset problems
        D3DGLOBALPTR(pCtxt)->D3DGlobals.coord_adj= 0.499f-0.7f*TEXTURE_FACTOR;  
        D3DGLOBALPTR(pCtxt)->D3DGlobals.__UVRANGE = ( 128.  - __UVBASEDEL ) ;
    }

    // Right now we are not allowing DMA under 
    // WinNT for this version of the driver
    D3DGLOBALPTR(pCtxt)->D3DGlobals.dma_possible    = FALSE;

    VideoBufferLinearAddr = D3DGLOBALPTR((ULONG_PTR)pCtxt)->pjScreen;

    D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DStp   = 
        ( vi13D_SETUP * )   ( D3DGLOBALPTR(pCtxt)->pjMmBase + 0xB4D4L );
    D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DTri   = 
        ( vi13D_TRIANGLE * )( D3DGLOBALPTR(pCtxt)->pjMmBase + 0xB504L );

    D3DGLOBALPTR(pCtxt)->D3DGlobals.CMD_SET    = 0x2CL;

    *(DWORD*)(VideoBufferLinearAddr+ D3DGLOBALPTR(pCtxt)->D3DGlobals.dwWhiteTexture) = 0xFFFFFFFF;

    return DD_OK;
}



