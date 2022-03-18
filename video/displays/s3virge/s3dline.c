/******************************Module*Header*******************************\
* Module Name: s3dline.c
*
* Contains all of the line-rendering routines for the S3 Virge MCD driver.
*
* Copyright (c) 1996 Microsoft Corporation
\**************************************************************************/

#undef RENDERHWLINE
#undef HWLINECLIPPING

#ifdef S3D_DMA

    #define RENDERHWLINE    __MCDRenderHWLineDMA
    #define HWLINECLIPPING  HWLineSetupClippingDMA

#else

    #define RENDERHWLINE    __MCDRenderHWLine
    #define HWLINECLIPPING  HWLineSetupClipping

#endif

//**************************************************************************
//
// VOID FASTCALL __MCDRenderHWLine
//
// HW Render a single line
//
//**************************************************************************

VOID FASTCALL RENDERHWLINE ( DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                             BOOL resetLine)
{
    PDEV *ppdev = pRc->ppdev;
    ULONG clipNum;
    RECTL *pClip;
    MCDFLOAT invDx, invDy, fdzdy, dy;
    MCDVERTEX *vtemp;

    LONG aIX, aIY, bIX, bIY;
    LONG xStart, xEndAB, yStart, yCount, zStart;
    LONG idxAB, idyAB, dxAB, dyAB, dxdyAB, dzdy ;
    LONG a0, r0, b0, g0, gbStart, arStart;
    LONG dgdy, dbdy, dady, drdy, dgbdy, dardy;
    BOOL bLineAtoB;

    bLineAtoB = TRUE;
    // must process lines draw from top to bottom
    if (b->windowCoord.y > a->windowCoord.y) {
        vtemp = a;
        a     = b;
        b     = vtemp;
        bLineAtoB = FALSE;
    }

    // Verify the vertex screen coordinates are within
    // reasonable limits for the Virge Fifo issue.
    CHECK_MCD_VERTEX_VALUE(a);
    CHECK_MCD_VERTEX_VALUE(b);

    if ((clipNum = pRc->pEnumClip->c) > 1) {
        pClip = &pRc->pEnumClip->arcl[0];
        (*pRc->HWSetupClipRect)(pRc, pClip++);
    }

    MCDBG_DUMPVERTEX(a);
    MCDBG_DUMPVERTEX(b);

    aIY = FTOL(a->windowCoord.y);
    bIY = FTOL(b->windowCoord.y);

    // Compute X related values:
    xStart = FLT_TO_FIX_SCALE(a->windowCoord.x +
                (LONG)pRc->xOffset - __MCDHALF,S3_S20_SCALE);

    dxAB = (long)(b->windowCoord.x - a->windowCoord.x);

    dyAB = (long)(aIY - bIY); // always >=0 since we force a.y >= b.y

    // Compute appropriate inverse of dy:
    if (dyAB  > 1) {
        invDy  = __MCDONE/(a->windowCoord.y - b->windowCoord.y);
        dxdyAB = FLT_TO_FIX_SCALE((b->windowCoord.x - a->windowCoord.x)*invDy,
                                                                S3_S20_SCALE);
    } else if (dyAB > 0) {
        invDy  = __MCDONE;
        dxdyAB = FLT_TO_FIX_SCALE((b->windowCoord.x - a->windowCoord.x)*invDy,
                                                                S3_S20_SCALE);
    } else {
        invDy  = __MCDZERO;
        dxdyAB = 0;
    }

    aIX = FTOL(a->windowCoord.x+(LONG)pRc->xOffset - __MCDHALF + S3DELTA);
    bIX = FTOL(b->windowCoord.x+(LONG)pRc->xOffset - __MCDHALF + S3DELTA);

    idxAB = IABS(dxAB);
    idyAB = IABS(dyAB);

    //////////////////////////////////////////////////////////////
    // Fix line coordinates in order to pass OpenGL conformance
    //
    // The S3Virge follows some line rendering rules very different
    // from the OpenGL diamond rule, with the last-pixel-turned-off
    // exit condition. We are adjusting here as well as we can to
    // get an "acceptable" line rendered which passes conformance
    //////////////////////////////////////////////////////////////

    // Since the S3Virge draws all pixels of a line, we have to
    // adjust in order to not draw the last pixel as OpenGL requires

    if ((dyAB != 0) && (idyAB >= idxAB)) { // if y-major line and its
        if (bLineAtoB)                     // directed from A to B then
            bIY++;                         // increment bIY to avoid drawing
    }                                      // the last pixel

    if ((dxAB != 0) ) {        // if it is not a vertical line
        if (aIX < bIX) {
            if (bLineAtoB)     // and goes from lower left to upper right
                bIX--;         // => decr ending X to avoid drawing last pixel
            else               // and goes from upper left to lower right
                aIX++;         // => incr starting X to avoid drawing last pixel
        } else {
            if (bLineAtoB)     // and goes from lower right to upper left
                bIX++;         // => incr ending X to avoid drawing last pixel
            else               // and goes from upper right to lower left
                aIX--;         // => decr starting X to avoid drawing last pixel
        }
    }

    if (dxAB > 0) {                // X1 < X2 , drawing from left to right
        if (idyAB >= idxAB) {
            // Y-major line
            // Add 0.5 to xStart and adjust for hw artifact
            xStart += ((1 <<20)/2);
            xStart--;
        } else {
            // X-major line
            // Add 0.5 of the slope to xStart and adjust for hw artifact
            xStart += (dxdyAB/2);
            xStart--;
        }
    } else {                       // X1 > X2 , drawing from right to left
        if (idyAB >= idxAB) {
            // Y-major line
            // Add 0.5 to xStart and adjust by almost 1 pixel to the right
            xStart += ((1 <<20)/2) - 513;

        } else {
            // X-major line
            // Adjust xStart by 1 pixel to the right due to the Floor(xStart)
            // function used by the hardware.
            xStart += (dxdyAB/2) + (1 <<20);
        }
    }

    // Calculate necessary y register values to feed into S3Virge:
    yStart = (long int)(aIY  + (LONG)pRc->yOffset) & 0x000007FF;
    yCount = (long int)(aIY - bIY + 1) <<0  |
                       ((dxAB < 0) ? 0x00000000:0x80000000);
    xEndAB = (bIX & 0x7FF) | ((aIX & 0x7FF) << 16);

    // Compute necessary RGBA values :
    r0 = ICLAMP(FTOL(a->colors[0].r * pRc->rScale));
    g0 = ICLAMP(FTOL(a->colors[0].g * pRc->gScale));
    b0 = ICLAMP(FTOL(a->colors[0].b * pRc->bScale));

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

    if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
        if ((dyAB != 0) && (idyAB >= idxAB)) {
            drdy = FTOL((b->colors[0].r - a->colors[0].r) * invDy * pRc->rScale);
            dgdy = FTOL((b->colors[0].g - a->colors[0].g) * invDy * pRc->gScale);
            dbdy = FTOL((b->colors[0].b - a->colors[0].b) * invDy * pRc->bScale);
            if (pRc->privateEnables & __MCDENABLE_BLEND)
                dady = FTOL((b->colors[0].a - a->colors[0].a) * invDy * pRc->aScale);
            else
                dady = 0;
        } else if ((dxAB != 0) && (idxAB >= idyAB)) {
            invDx = __MCDONE / idxAB;
            drdy = FTOL((b->colors[0].r - a->colors[0].r) * invDx * pRc->rScale);
            dgdy = FTOL((b->colors[0].g - a->colors[0].g) * invDx * pRc->gScale);
            dbdy = FTOL((b->colors[0].b - a->colors[0].b) * invDx * pRc->bScale);
            if (pRc->privateEnables & __MCDENABLE_BLEND)
                dady = FTOL((b->colors[0].a - a->colors[0].a) * invDx * pRc->aScale);
            else
                dady = 0;
        } else {
            drdy = 0;
            dgdy = 0;
            dbdy = 0;
            dady = 0;
        }

        dgbdy   = (dgdy & 0xFFFF) << 16 | (dbdy &0xFFFF);
        dardy   = (dady & 0xFFFF) << 16 | (drdy &0xFFFF);
    } else {
        dgbdy   = 0;
        dardy   = 0;
    }

    gbStart= (  g0 & 0x7FFF) << 16 | (  b0 &0x7FFF);
    arStart= (  a0 & 0x7FFF) << 16 | (  r0 &0x7FFF);

    if (pRc->privateEnables & __MCDENABLE_Z) {

        // Compute necessary Z values
        if ((dyAB != 0) && (idyAB >= idxAB)) {
            fdzdy   = (b->windowCoord.z - a->windowCoord.z) * invDy;
        } else if ((dxAB != 0) && (idxAB >= idyAB)) {
            invDx = __MCDONE / idxAB;
            fdzdy   = (b->windowCoord.z - a->windowCoord.z) * invDx;
        } else {
            fdzdy   = __MCDZERO;
        }

        dzdy    = FLT_TO_FIX_SCALE(fdzdy,S3_S15_SCALE);
        // sub-pixel correct the starting z value
        dy     = (a->windowCoord.y) - aIY;
        zStart = FLT_TO_FIX_SCALE(a->windowCoord.z - (fdzdy * dy) ,S3_S15_SCALE);

        //
        // We need to account for 8+20+16+4 or 48 bytes of register writes,
        // plus 4 DMA header writes of 16 bytes.
        //
        // NOTE: the DMA macros are null if S3D_DMA is not defined
        //

        CHECK_DMA_SLOT(ppdev, 48 + 16);
        WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_Z_DELTA, 8);

#ifndef S3D_DMA

        S3DFifoWait(ppdev, 12);

#endif

        S3DWRITE(ppdev, 3D_LINE_Z_DELTA, dzdy);
        S3DWRITE(ppdev, 3D_LINE_Z_START, zStart);

        ADVANCE_DMA_INDEX(ppdev, 8);

    } else {

        //
        // We need to account for 20+16+4 or 40 bytes of register writes,
        // plus 4 DMA header writes of 16 bytes.
        //
        // NOTE: the DMA macros are null if S3D_DMA is not defined
        //

        CHECK_DMA_SLOT(ppdev, 40 + 16);

#ifndef S3D_DMA

        S3DFifoWait(ppdev, 10);

#endif

    }

    //
    // NOTE: the DMA macros are null if S3D_DMA is not defined
    //

    WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_X_END_0_1, 20);

    S3DWRITE(ppdev, 3D_LINE_X_END_0_1,xEndAB);
    S3DWRITE(ppdev, 3D_LINE_X_DELTA,  dxdyAB);
    S3DWRITE(ppdev, 3D_LINE_X_START,  xStart);
    S3DWRITE(ppdev, 3D_LINE_Y_START,  yStart);
    S3DWRITE(ppdev, 3D_LINE_Y_COUNT,  yCount);

    ADVANCE_DMA_INDEX(ppdev, 20);

    WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_GREEN_BLUE_DELTA, 16);

    S3DWRITE(ppdev, 3D_LINE_GREEN_BLUE_DELTA, dgbdy);
    S3DWRITE(ppdev, 3D_LINE_ALPHA_RED_DELTA , dardy);
    S3DWRITE(ppdev, 3D_LINE_GREEN_BLUE_START, gbStart);
    S3DWRITE(ppdev, 3D_LINE_ALPHA_RED_START,  arStart);

    ADVANCE_DMA_INDEX(ppdev, 16);

    //
    // Must set Alpha blending to avoid S3d engine reported bug #A-31
    //
    // NOTE: the DMA macros are null if S3D_DMA is not defined
    //

    WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_COMMAND, 4);

#ifndef S3D_DMA

    S3DGPBusy( ppdev );

#endif

    S3DWRITE( ppdev,
              3D_LINE_COMMAND,
              pRc->hwVideoMode      |
              S3D_LINE_3D           |
              S3D_COMMAND_3D        |
              pRc->hwZFunc          |
              pRc->hwLineClipFunc   |
              pRc->hwBlendFunc);

    ADVANCE_DMA_INDEX(ppdev, 4);
    START_DMA(ppdev);

    // flag that we have issued a 3D command, take care of proper
    // transition back to 2D commands on HW_WAIT_DRAWING_DONE
    //ppdev->b3DHwUsed = TRUE;

    while (--clipNum) {
        (*pRc->HWSetupClipRect)(pRc, pClip++);

        // We rewrite the registers because the S3V munges them
        if (pRc->privateEnables & __MCDENABLE_Z) {

            CHECK_DMA_SLOT(ppdev, 8 + 4);
            WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_Z_DELTA, 8);

#ifndef S3D_DMA

            S3DFifoWait(ppdev, 12);

#endif

            S3DWRITE(ppdev, 3D_LINE_Z_DELTA,   dzdy);
            S3DWRITE(ppdev, 3D_LINE_Z_START, zStart);

            ADVANCE_DMA_INDEX(ppdev, 8);

        } else
        {

#ifndef S3D_DMA

            S3DFifoWait(ppdev, 10);

#endif

        }

        //
        // We need to account for a total of 20+16+4 or 40 bytes,
        // plus 3 DMA headers or 12 bytes.
        //
        // NOTE: the DMA macros are null if S3D_DMA is not defined
        //

        CHECK_DMA_SLOT(ppdev, 40 + 12);
        WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_X_END_0_1, 20);

        S3DWRITE(ppdev, 3D_LINE_X_END_0_1,xEndAB);
        S3DWRITE(ppdev, 3D_LINE_X_DELTA,  dxdyAB);
        S3DWRITE(ppdev, 3D_LINE_X_START,  xStart);
        S3DWRITE(ppdev, 3D_LINE_Y_START,  yStart);
        S3DWRITE(ppdev, 3D_LINE_Y_COUNT,  yCount);

        ADVANCE_DMA_INDEX(ppdev, 20);

        WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_GREEN_BLUE_DELTA, 16);

        S3DWRITE(ppdev, 3D_LINE_GREEN_BLUE_DELTA, dgbdy);
        S3DWRITE(ppdev, 3D_LINE_ALPHA_RED_DELTA , dardy);
        S3DWRITE(ppdev, 3D_LINE_GREEN_BLUE_START, gbStart);
        S3DWRITE(ppdev, 3D_LINE_ALPHA_RED_START,  arStart);

        ADVANCE_DMA_INDEX(ppdev, 16);

        WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_COMMAND, 4);

#ifndef S3D_DMA

        S3DGPWait( ppdev );

#endif

        S3DWRITE( ppdev,
                  3D_LINE_COMMAND,
                  pRc->hwVideoMode   |
                  S3D_LINE_3D        |
                  S3D_COMMAND_3D     |
                  pRc->hwZFunc       |
                  pRc->hwLineClipFunc|
                  pRc->hwBlendFunc);

        ADVANCE_DMA_INDEX(ppdev, 4);
        START_DMA(ppdev);
    }
}

//**************************************************************************
//
// VOID FASTCALL HWLineSetupClipping
//
// Setup window clipping parameters for line rendering
//
//**************************************************************************
VOID FASTCALL HWLINECLIPPING(DEVRC *pRc, RECTL *pClip)
{
    ULONG  ulLeftRight,ulTopBottom;
    PDEV *ppdev = pRc->ppdev;

    //
    // Back buffer is also handled here since we change the
    // S3D_3D_LINE_DESTINATION_BASE in HW_INIT_DRAWING_STATE
    //

    ulLeftRight = (pClip->left << 16) | (pClip->right  - 1);
    ulTopBottom = (pClip->top ) << 16 | (pClip->bottom - 1);

    //
    // Check to see if there is enough slots in the DMA buffer.
    // we need 8 bytes to accomondate 2 dword writes and 4 bytes
    // for the DMA header.
    //

    CHECK_DMA_SLOT(ppdev, 8 + 4);

    WRITE_DMA_HEADER(ppdev, S3D_3D_LINE_CLIP_LEFT_RIGHT, 8);

    S3DWRITE(ppdev, 3D_LINE_CLIP_LEFT_RIGHT, ulLeftRight);

    S3DWRITE(ppdev, 3D_LINE_CLIP_TOP_BOTTOM, ulTopBottom);

    ADVANCE_DMA_INDEX(ppdev, 8);

    START_DMA(ppdev);   // kick off DMA.

    //
    // Indicate that clipping must be enabled when drawing lines
    //

    pRc->hwLineClipFunc = S3D_HARDWARE_CLIPPING;

}

