/******************************Module*Header*******************************\
* Module Name: mcdpoint.c
*
* Contains the point-rendering routines for the S3 Virge MCD driver.
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#include "hw3d.h"
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"

//**************************************************************************
//
// VOID FASTCALL __MCDPointBegin
//
// Setup S3Virge registers before starting to draw a batch of points
//
//**************************************************************************
VOID FASTCALL __MCDPointBegin(DEVRC *pRc)
{
    PDEV *ppdev = (PDEV *)pRc->ppdev;

    WAIT_DMA_IDLE( ppdev );

    S3DFifoWait(ppdev, 4);
    S3writeHIU(ppdev, S3D_3D_LINE_GREEN_BLUE_DELTA, 0x0L);
    S3writeHIU(ppdev, S3D_3D_LINE_ALPHA_RED_DELTA , 0x0L);
    S3writeHIU(ppdev, S3D_3D_LINE_X_DELTA,          0x0L);
    S3writeHIU(ppdev, S3D_3D_LINE_Y_COUNT,          0x1L);
}


//**************************************************************************
//
// VOID FASTCALL __MCDRenderPoint
//
// HW render a sinle point
//
//**************************************************************************
VOID FASTCALL __MCDRenderGenericPoint(DEVRC *pRc, MCDVERTEX *a)
{
    PDEV *ppdev = pRc->ppdev;
    ULONG clipNum;
    RECTL *pClip;

    LONG aIY;
    LONG xStart, xEndAB, zStart, yStart;
    LONG a0, r0, b0, g0, gbStart, arStart;
    MCDFLOAT xCoor;

    // Verify the vertex screen coordinates are within
    // reasonable limits for the Virge Fifo issue.
    CHECK_MCD_VERTEX_VALUE(a);

    if ((clipNum = pRc->pEnumClip->c) > 1) {
        pClip = &pRc->pEnumClip->arcl[0];
        (*pRc->HWSetupClipRect)(pRc, pClip++);
    }

    MCDBG_DUMPVERTEX(a);

    // Compute x related values:
    xCoor  = a->windowCoord.x + (LONG)pRc->xOffset + S3DELTA;
    xStart = FLT_TO_FIX_SCALE(xCoor,S3_S20_SCALE);
    xEndAB = (FTOL(xCoor) & 0x7FF) | ((FTOL(xCoor) & 0x7FF) << 16);

    // Compute y realtex values:
    aIY = FTOL(a->windowCoord.y);
    // Use only 11 bits for the y start value
    yStart = (long int)(aIY + (LONG)pRc->yOffset) & 0x000007FF;


    // Compute RGBA values of the point
    if (pRc->privateEnables & __MCDENABLE_BLEND)
    {
        a0 = ICLAMP(FTOL(a->colors[0].a * pRc->aScale));
    }
    else if (pRc->hwBlendFunc == S3D_ALPHA_BLEND_SOURCE_ALPHA)
    {
        a0 = 0x7F80;
    }
    else
    {
        a0 = 0x7FFF;
    }

    // Adjust colors so they don't dither in 16bpp
    // this is necessary to pass OpenGL conformance
    if (ppdev->iBitmapFormat == BMF_16BPP) {

        r0 = FTOL(a->colors[0].r + __MCDHALF) << 10;
        g0 = FTOL(a->colors[0].g + __MCDHALF) << 10;
        b0 = FTOL(a->colors[0].b + __MCDHALF) << 10;

        gbStart= (  g0 & 0x7C00) << 16 | (  b0 &0x7C00);
        arStart = (  a0 & 0x7C00) << 16 | (  r0 &0x7C00);
    } else {
        // convert to fixed point format and clamp (24bpp)
        r0 = ICLAMP(FTOL(a->colors[0].r * pRc->rScale));
        g0 = ICLAMP(FTOL(a->colors[0].g * pRc->gScale));
        b0 = ICLAMP(FTOL(a->colors[0].b * pRc->bScale));

        gbStart= (  g0 & 0x7FFF) << 16 | (  b0 &0x7FFF);
        arStart= (  a0 & 0x7FFF) << 16 | (  r0 &0x7FFF);
    }

    // Compute Z buffering values if necessary:
    if (pRc->privateEnables & __MCDENABLE_Z) {
        zStart = FLT_TO_FIX_SCALE(a->windowCoord.z, S3_S15_SCALE);

        S3DFifoWait(ppdev,8);
        S3writeHIU(ppdev, S3D_3D_LINE_Z_DELTA,   0x0L);
        S3writeHIU(ppdev, S3D_3D_LINE_Z_START, zStart);
    } else {
        S3DFifoWait(ppdev,6);
    }

    S3writeHIU(ppdev, S3D_3D_LINE_X_END_0_1, xEndAB);
    S3writeHIU(ppdev, S3D_3D_LINE_X_START,   xStart);
    S3writeHIU(ppdev, S3D_3D_LINE_Y_START,   yStart);

    S3writeHIU(ppdev, S3D_3D_LINE_GREEN_BLUE_START, gbStart);
    S3writeHIU(ppdev, S3D_3D_LINE_ALPHA_RED_START,  arStart);

    S3DGPWait(ppdev);
    S3writeHIU(ppdev,S3D_3D_LINE_COMMAND, pRc->hwVideoMode   |
                                          S3D_LINE_3D        |
                                          S3D_COMMAND_3D     |
                                          pRc->hwZFunc       |
                                          pRc->hwLineClipFunc|
                                          pRc->hwBlendFunc);

    while (--clipNum) {
        (*pRc->HWSetupClipRect)(pRc, pClip++);

        // We must rewrite the registers because the S3V munges them
        S3DFifoWait(ppdev, 6);
        S3writeHIU(ppdev, S3D_3D_LINE_X_END_0_1, xEndAB);
        S3writeHIU(ppdev, S3D_3D_LINE_X_START,   xStart);
        S3writeHIU(ppdev, S3D_3D_LINE_Y_START,   yStart);
        S3writeHIU(ppdev, S3D_3D_LINE_GREEN_BLUE_START, gbStart);
        S3writeHIU(ppdev, S3D_3D_LINE_ALPHA_RED_START,  arStart);
        S3DGPWait(ppdev);
        S3writeHIU(ppdev,S3D_3D_LINE_COMMAND,pRc->hwVideoMode   |
                                              S3D_LINE_3D        |
                                              S3D_COMMAND_3D     |
                                              pRc->hwZFunc       |
                                              pRc->hwLineClipFunc|
                                              pRc->hwBlendFunc);
    }
}


//*************************************************************************
//
// VOID FASTCALL __MCDRenderFogPoint
//
// HW Render a single fogged point
//
//*************************************************************************
VOID FASTCALL __MCDRenderFogPoint(DEVRC *pRc, MCDVERTEX *pv)
{
    MCDCOLOR c;

    c = pv->colors[0];
    __MCDCalcFogColor(pRc, pv, &pv->colors[0], &c);
    (*pRc->renderPointX)(pRc, pv);
    pv->colors[0] = c;
}

#endif

