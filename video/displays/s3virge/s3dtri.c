/******************************Module*Header*******************************\
* Module Name: s3dtri.c
*
* Contains the low-level (rasterization) triangle-rendering routines for the
* S3 Virge MCD driver.
*
* Copyright (c) 1998 Microsoft Corporation
\**************************************************************************/

#undef HWSETUPDELTAS
#undef ADJUSTVERTEXALLPROPS
#undef HWDRAWCOMMAND
#undef MCDFILLSUBTRIANGLE
#undef HWTRIANGLECLIPPING

#ifdef S3D_DMA

    #define HWSETUPDELTAS           __HWSetupDeltasDMA
    #define ADJUSTVERTEXALLPROPS    __AdjustVertexAllPropsDMA
    #define HWDRAWCOMMAND           __HWDrawCommandDMA
    #define MCDFILLSUBTRIANGLE      __MCDFillSubTriangleDMA
    #define HWTRIANGLECLIPPING      HWTriangleSetupClippingDMA

#else

    #define HWSETUPDELTAS           __HWSetupDeltas
    #define ADJUSTVERTEXALLPROPS    __AdjustVertexAllProps
    #define HWDRAWCOMMAND           __HWDrawCommand
    #define MCDFILLSUBTRIANGLE      __MCDFillSubTriangle
    #define HWTRIANGLECLIPPING      HWTriangleSetupClipping

#endif

VOID FASTCALL __HWTexPerspPrepare(DEVRC *, MCDVERTEX *, MCDVERTEX *, MCDVERTEX *);
VOID FASTCALL __MCDCalcCoordDeltas(DEVRC *, MCDVERTEX *, MCDVERTEX *, MCDVERTEX *, BOOL *);
VOID FASTCALL __MCDCalcXDeltaProps(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                  MCDVERTEX *c);
VOID FASTCALL __MCDCalcYDeltaProps(DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                  MCDVERTEX *c);
VOID __MCDSetupTexFilter(DEVRC *pRc,MCDVERTEX *a, MCDVERTEX *b,MCDVERTEX *c,
                         BOOL bTrust);
MCDFLOAT FASTCALL __AdjustVertexPosition(MCDVERTEX *p, LONG iyVal,
                                         MCDFLOAT dxdy_slope);
VOID FASTCALL __MCDFlipXDeltaProps(DEVRC *pRc);

//**************************************************************************
//
// VOID FASTCALL __HWSetupDeltas
//
// Set all relevant Delta registers on the S3VIRGE
// if not enabled, the Delta registers should be set if
// needed in HW_INIT_PRIMITIVE_STATE
//
//**************************************************************************

VOID FASTCALL HWSETUPDELTAS(DEVRC *pRc)
{
    LONG idgbdx,idgbdy,idardx,idardy;
    int i;
    ULONG *pjTmp;
    PDEV *ppdev = pRc->ppdev;

    //
    // For each triangle command, we anticipate a write of up to 31 dwords,
    // starting from S3D_TRI_BASE_V(B504) to S3D_TRI_Y_COUNT(B57C). Add
    // extra 4 bytes to account for DMA header.
    //
    // NOTE: the DMA macros are null if S3D_DMA is not defined
    //

    CHECK_DMA_SLOT(ppdev, 31 * 4 + 4);
    CLEAR_DMA_SLOT(ppdev, 31 * 4 + 4);
    WRITE_DMA_HEADER(ppdev, S3D_TRI_BASE_V, 31 * 4);

#ifndef S3D_DMA

    // Clear all the 31 MMIO registers with 0s.
    pjTmp = (ULONG *)(ppdev->pjMmBase + S3D_TRI_BASE_V);
    i = 31;
    do {
        *(pjTmp+i) = 0L;
    } while (--i >= 0);

    S3DFifoWait(pRc->ppdev, 6); //Maximum fifo entries we may need (no texturing)

#endif


    if (!(pRc->privateEnables & __MCDENABLE_BLEND)) {
        pRc->fxdadyEdge = 0;
        pRc->fxdadx     = 0;
    }

    if ((pRc->privateEnables & __MCDENABLE_COLORED) &&
        (pRc->privateEnables & __MCDENABLE_SMOOTH)) {
        idgbdx   = (pRc->fxdgdx & 0xFFFF) << 16 | (pRc->fxdbdx & 0xFFFF);
        idardx   = (pRc->fxdadx & 0xFFFF) << 16 | (pRc->fxdrdx & 0xFFFF);
        idgbdy   = (pRc->fxdgdyEdge & 0xFFFF) << 16 |
                   (pRc->fxdbdyEdge & 0xFFFF);
        idardy   = (pRc->fxdadyEdge & 0xFFFF) << 16 |
                   (pRc->fxdrdyEdge & 0xFFFF);

        S3DWRITE(ppdev, TRI_GREEN_BLUE_X_DELTA, idgbdx);
        S3DWRITE(ppdev, TRI_ALPHA_RED_X_DELTA, idardx);
        S3DWRITE(ppdev, TRI_GREEN_BLUE_Y_DELTA, idgbdy);
        S3DWRITE(ppdev, TRI_ALPHA_RED_Y_DELTA, idardy);
    }


    if (pRc->privateEnables & __MCDENABLE_Z) {
        S3DWRITE(ppdev, TRI_Z_X_DELTA, pRc->fxdzdx);
        S3DWRITE(ppdev, TRI_Z_Y_DELTA, pRc->fxdzdyEdge);
    }

    if (pRc->privateEnables & __MCDENABLE_TEXTURED) {

#ifndef S3D_DMA

        //Maximum fifo entries we may need for texturing
        S3DFifoWait(pRc->ppdev, 8);

#endif

        S3DWRITE(ppdev, TRI_U_X_DELTA,  pRc->fxdudx);
        S3DWRITE(ppdev, TRI_U_Y_DELTA,  pRc->fxdudyEdge);
        S3DWRITE(ppdev, TRI_V_X_DELTA,  pRc->fxdvdx);
        S3DWRITE(ppdev, TRI_V_Y_DELTA,  pRc->fxdvdyEdge);

        if (pRc->privateEnables & __MCDENABLE_TEXPERSP) {
            S3DWRITE(ppdev, TRI_W_X_DELTA, pRc->fxdwdx);
            S3DWRITE(ppdev, TRI_W_Y_DELTA, pRc->fxdwdyEdge);
        }

        if (pRc->privateEnables & __MCDENABLE_TEXMIPMAP) {
            S3DWRITE(ppdev, TRI_D_X_DELTA, pRc->fxdddx);
            S3DWRITE(ppdev, TRI_D_Y_DELTA, pRc->fxdddyEdge);
        }

    }
}


//**************************************************************************
//
// MCDFLOAT FASTCALL __AdjustVertexAllProps
//
// Adjust initial vertex position+RGBA+(ZUVWD) as needed
// and setup in the S3VIRGE
//
//**************************************************************************

MCDFLOAT FASTCALL ADJUSTVERTEXALLPROPS(DEVRC *pRc,MCDVERTEX *p, LONG iyVal,
                                       MCDFLOAT dxdy_slope)
{
    PDEV * ppdev = pRc->ppdev;
    LONG  b0, a0, gbStart, arStart;
    MCDFLOAT vBaseHw;
    LONG  g0;
    MCDFLOAT uBaseHw;
    LONG  r0;
    MCDFLOAT vStart;
    LONG  d0;
    MCDFLOAT uStart;
    LONG  w0;
    MCDFLOAT xNew;
    LONG  v0;
    MCDFLOAT dy;
    LONG  u0;
    MCDFLOAT dx;

    // Compute new X and deltas for adjustments
    dy = iyVal - (p->windowCoord.y - __MCDHALF);
    dx = dy * dxdy_slope;
    xNew = p->windowCoord.x  - dx;

    // Adjust and write X for starting vertex
#ifndef S3D_DMA
    S3DFifoWait(ppdev, 4);   //Maximum fifo entries we may need wo texturing
#endif


    S3DWRITE( ppdev,
              TRI_X_START ,
              FLT_TO_FIX_SCALE( xNew + (LONG)pRc->xOffset -
                                __MCDHALF, S3_S20_SCALE ) );

    if (pRc->privateEnables & __MCDENABLE_COLORED) {

        // Adjust and write RGB(A)
        if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
            r0 = ICLAMP(FTOL((p->colors[0].r - (pRc->drdyEdge * dy)) * pRc->rScale));
            g0 = ICLAMP(FTOL((p->colors[0].g - (pRc->dgdyEdge * dy)) * pRc->gScale));
            b0 = ICLAMP(FTOL((p->colors[0].b - (pRc->dbdyEdge * dy)) * pRc->bScale));

            if (pRc->privateEnables & __MCDENABLE_BLEND)
            {
                a0 = ICLAMP(FTOL((p->colors[0].a
                                - (pRc->dadyEdge * dy))* pRc->aScale));
            }
            else if (pRc->hwBlendFunc == S3D_ALPHA_BLEND_SOURCE_ALPHA)
            {
                a0 = 0x7F80;
            }
            else
            {
                a0 = 0x7FFF;
            }
        } else {
            r0 = ICLAMP(FTOL((pRc->pvProvoking->colors[0].r * pRc->rScale)));
            g0 = ICLAMP(FTOL((pRc->pvProvoking->colors[0].g * pRc->gScale)));
            b0 = ICLAMP(FTOL((pRc->pvProvoking->colors[0].b * pRc->bScale)));
            if (pRc->privateEnables & __MCDENABLE_BLEND)
            {
                a0 = ICLAMP(FTOL((pRc->pvProvoking->colors[0].a * pRc->aScale)));
            }
            else if (pRc->hwBlendFunc == S3D_ALPHA_BLEND_SOURCE_ALPHA)
            {
                a0 = 0x7F80;
            }
            else
            {
                a0 = 0x7FFF;
            }
        }

        // Write RGB(A)
        gbStart= (  g0 & 0x7FFF) << 16 | (  b0 & 0x7FFF);
        arStart= (  a0 & 0x7FFF) << 16 | (  r0 & 0x7FFF);

        S3DWRITE(ppdev, TRI_GREEN_BLUE_START, gbStart);
        S3DWRITE(ppdev, TRI_ALPHA_RED_START , arStart);
    }

    // Adjust and write Z if needed
    if (pRc->privateEnables & __MCDENABLE_Z) {
        ULONG     z0;

        if (pRc->MCDState.enables & MCD_POLYGON_OFFSET_FILL_ENABLE) {
            MCDFLOAT zOffset;

            zOffset = __MCDGetZOffsetDelta(pRc) +
                    (pRc->MCDState.zOffsetUnits * pRc->zScale);

            z0 = FTOL( (p->windowCoord.z - (pRc->dzdyEdge * dy))
                          * pRc->zScale + zOffset);
        } else {
            z0 = FTOL((p->windowCoord.z  - (pRc->dzdyEdge * dy))
                         * pRc->zScale);
        }

        S3DWRITE(ppdev, TRI_Z_START, z0);
    }

    // Adjust and write texture related coordinates if needed

    if (pRc->privateEnables & __MCDENABLE_TEXTURED) {

        uStart = p->texCoord.x;
        vStart = p->texCoord.y;

        if (pRc->privateEnables & __MCDENABLE_TEXPERSP) {

            // Even when we should, we avoid sub pixel correcting the texture
            // coordinates and w value in this particular implementation
            // because otherwise we see bad distortions in vertexes too close
            // to the viewing plane. This doesn't hurt conformance tests.
            w0 = FTOL((p->windowCoord.w - dy * pRc->dwdyEdge) * pRc->wScale);
            u0 = FTOL(((uStart - pRc->uBase) * p->windowCoord.w  
                        - dy * pRc->dudyEdge) * pRc->uvScale);
            v0 = FTOL(((vStart - pRc->vBase) * p->windowCoord.w 
                        - dy * pRc->dvdyEdge) * pRc->uvScale);

#ifndef S3D_DMA

            //Maximum fifo entries we may need for perpective corrected texturing
            S3DFifoWait(pRc->ppdev, 6);

#endif

            S3DWRITE(ppdev, TRI_W_START, w0);
            S3DWRITE(ppdev, TRI_U_START, u0);
            S3DWRITE(ppdev, TRI_V_START, v0);

            uBaseHw = pRc->uBase + pRc->ufix;
            vBaseHw = pRc->vBase + pRc->vfix;
            while (uBaseHw < __MCDZERO) uBaseHw += __MCDONE;
            while (vBaseHw < __MCDZERO) vBaseHw += __MCDONE;

            // For Virge and Virge/VX.
            if (ppdev->ulChipID == SUBTYPE_325 || ppdev->ulChipID == SUBTYPE_988) {
                S3DWRITE( ppdev,
                          TRI_BASE_U ,
                          FTOL(uBaseHw * pRc->uBaseScale));
            }
            else {   // DX, GX, GX2, M5
                S3DWRITE( ppdev,
                          TRI_BASE_U ,
                          (FTOL(uBaseHw * pRc->uBaseScale) &
                           0x1FFFFFFF) | 0x80000000);
            }
            S3DWRITE(ppdev, TRI_BASE_V ,
                            FTOL(vBaseHw * pRc->vBaseScale));
        } else {

            uStart +=  - (pRc->dudyEdge * dy);
            vStart +=  - (pRc->dvdyEdge * dy);

            //S3Virge requires the s,t start coordinates to be >= 0.0
            if (uStart < __MCDZERO) uStart += __MCDONE;
            if (vStart < __MCDZERO) vStart += __MCDONE;

            // This offset fix is a neccesary hack in the S3Virge to
            // align correctly the u,v coordinates
            u0 = FTOL( uStart * pRc->uvScale + pRc->ufix);
            v0 = FTOL( vStart * pRc->uvScale + pRc->vfix);

#ifndef S3D_DMA

            //Maximum fifo entries we may need for normal texturing
            S3DFifoWait(pRc->ppdev, 3);

#endif

            S3DWRITE(ppdev, TRI_U_START, u0);
            S3DWRITE(ppdev, TRI_V_START, v0);
        }

        if (pRc->privateEnables & __MCDENABLE_TEXMIPMAP) {
            d0 = FTOL((p->texCoord.z - dy * pRc->dddyEdge) * pRc->dScale);
            S3DWRITE(ppdev, TRI_D_START, d0);
        }

    }

    return xNew;
}


//**************************************************************************
//
// void FASTCALL __HWDrawCommand
//
// Send command to draw triangle
//
//**************************************************************************

void FASTCALL HWDRAWCOMMAND( DEVRC *pRc, LONG yStart,
                             LONG ylenAB, LONG ylenBC, BOOL bCCW)
{
    LONG yCount;
    PDEV *ppdev = pRc->ppdev;

    yStart = (LONG)(yStart + (LONG)pRc->yOffset) & 0x000007FF;
    yCount = (LONG)(ylenBC) <<0   |
             (LONG)(ylenAB) << 16 | (bCCW? 0x00000000 : 0x80000000 );

    //
    // NOTE: DMA macros are null if S3D_DMA is not defined
    //

    if (!(yCount & 0x7FFFFFFF)) {

        SUBTRACT_DMA_INDEX(ppdev, 4);    // account for DMA header only.
        return;
    }

    S3DWRITE(ppdev, TRI_Y_START, yStart);
    S3DWRITE(ppdev, TRI_Y_COUNT, yCount);
    ADVANCE_DMA_INDEX(ppdev, 31 * 4);

    CHECK_DMA_SLOT(ppdev, 4 + 4);
    WRITE_DMA_HEADER(ppdev, S3D_TRI_COMMAND, 4);

#ifndef S3D_DMA

    S3DGPWait(pRc->ppdev);

#endif


    if (pRc->privateEnables & __MCDENABLE_TEXTURED)
    {
        // Must set Alpha blending to avoid S3d engine reported bug #A-31
        S3DWRITE(ppdev, TRI_COMMAND, pRc->hwVideoMode            |
                                     pRc->hwZFunc                |
                                     pRc->hwTexFunc              |
                                     pRc->hwTriClipFunc          |
                                     pRc->hwBlendFunc            |
                                     S3D_COMMAND_3D );
    } else {
        // Must set Alpha blending to avoid S3d engine reported bug #A-31
        S3DWRITE(ppdev, TRI_COMMAND, pRc->hwVideoMode            |
                                     pRc->hwZFunc                |
                                     S3D_GOURAUD_SHADED_TRIANGLE |
                                     pRc->hwTriClipFunc          |
                                     pRc->hwBlendFunc            |
                                     S3D_COMMAND_3D );
    }

    ADVANCE_DMA_INDEX(ppdev, 4);
    START_DMA(ppdev);

    ppdev->fTriangleFix = TRUE;
}

//*****************************************************************************
//
// VOID FASTCALL __MCDFillSubTriangle
//
// HW Render a triangle (no left window edge clipping nor tesselation necessary)
//
// Vertices are ordered from highest y-value (a) to smallest y-value (c)
//
// bTrust indicates if this  triangle was generated from our tesselation code or
// from the left edge window clipping code and therefore we can't trust the
// deltas and area calculated in previuos stages.
//
//*****************************************************************************

VOID FASTCALL MCDFILLSUBTRIANGLE( DEVRC *pRc, MCDVERTEX *a, MCDVERTEX *b,
                                  MCDVERTEX *c, BOOL bCCW, BOOL bTrust)
{
#if TEX_NON_SQR
    MCDCOORD aTexOld, bTexOld, cTexOld;
#endif
    MCDFLOAT fwScaleOld, fuvScaleOld;
    MCDFLOAT dxdyBC;
    PDEV *ppdev = pRc->ppdev;
    MCDFLOAT dxdyAB;
    MCDVERTEX *vtemp;
    MCDFLOAT dxdyAC;
    LONG cIY;
    MCDFLOAT xEndBC;
    LONG bIY;
    MCDFLOAT xEndAB;
    LONG aIY;
    MCDFLOAT xStart;

    // Verify the vertex screen coordinates are within
    // reasonable limits for the Virge Fifo issue.
    CHECK_MCD_VERTEX_VALUE(a);
    CHECK_MCD_VERTEX_VALUE(b);
    CHECK_MCD_VERTEX_VALUE(c);


    // If !bTrust we should not trust the y-order of the vertexes,
    // because this is a textured triangle which was tesselated
    // in order to not span the maximimum number of permitted texels
    // or a clipped triangle clipped against the left edge since HW
    // clipping does not work for x < 0.
    if (!bTrust) {

        if (__MCD_VERTEX_COMPARE(a->windowCoord.y, >=, b->windowCoord.y))
            if (__MCD_VERTEX_COMPARE(b->windowCoord.y, >=, c->windowCoord.y)) {
                // nothing to do, order is O.K. (a b c)
            } else
                if (__MCD_VERTEX_COMPARE(a->windowCoord.y, >=, c->windowCoord.y)) {
                // order is a c b (largest-to-smallest)
                    vtemp = b; b = c; c = vtemp;
                } else {
                // order is c a b (largest-to-smallest)
                    vtemp = a; a = c; c = b; b = vtemp;
                }
        else
            if (__MCD_VERTEX_COMPARE(a->windowCoord.y, >=, c->windowCoord.y)) {
                // order is  b a c (largest-to-smallest)
                    vtemp = a; a = b; b = vtemp;
            } else
                if (__MCD_VERTEX_COMPARE(b->windowCoord.y,>=, c->windowCoord.y)) {
                // order is  b c a (largest-to-smallest)
                    vtemp = a; a = b; b = c; c = vtemp;
                } else {
                // order is  c b a  (largest-to-smallest)
                    vtemp = a; a = c; c = vtemp;
                }
    }


    MCDBG_DUMPVERTEX(a);
    MCDBG_DUMPVERTEX(b);
    MCDBG_DUMPVERTEX(c);

#if DBG_TESSELATION
    if (!bTrust)
        DbgTessInit(pRc, a, b, c);
#endif

#if TEX_NON_SQR
    //Scale & save u,v coordinates if non-square texture maps are involved
    if ((pRc->privateEnables & __MCDENABLE_TEXTURED) &&
        (pRc->privateEnables & __MCDENABLE_TEX_NONSQR))
    {
        a->texCoord.x = (aTexOld.x = a->texCoord.x) * pRc->uFactor;
        b->texCoord.x = (bTexOld.x = b->texCoord.x) * pRc->uFactor;
        c->texCoord.x = (cTexOld.x = c->texCoord.x) * pRc->uFactor;
        a->texCoord.y = (aTexOld.y = a->texCoord.y) * pRc->vFactor;
        b->texCoord.y = (bTexOld.y = b->texCoord.y) * pRc->vFactor;
        c->texCoord.y = (cTexOld.y = c->texCoord.y) * pRc->vFactor;
    }
#endif

    // Get integer part of each y coordinate to snap them
    aIY = FTOL(a->windowCoord.y - __MCDHALF);
    bIY = FTOL(b->windowCoord.y - __MCDHALF);
    cIY = FTOL(c->windowCoord.y - __MCDHALF);

    // Preset all slopes to take care of horizontal lines
    dxdyAC = (MCDFLOAT)(0.0);
    dxdyAB = (MCDFLOAT)(0.0);
    dxdyBC = (MCDFLOAT)(0.0);

    // Compute base u and v if texture & perspective correction are enabled
    if (pRc->privateEnables & __MCDENABLE_TEXPERSP)
    {
        fwScaleOld  = pRc->wScale;
        fuvScaleOld = pRc->uvScale;
        __HWTexPerspPrepare(pRc, a, b, c);
    }

#ifdef P5TIME
    CLEARTPC(ppdev);
#endif  //P5TIME

    // If we are receiving a tesselated triangle we will need to recalculate
    // again the deltas among x and y coordinates and also we will need
    // to recalculate the triangle area and reconsider the order of its
    // vertices (cw or ccw)
    if (!bTrust)
        __MCDCalcCoordDeltas(pRc, a, b, c, &bCCW);

#ifdef P5TIME
    READTPC(ppdev);
    PRINTIME((0, "MCDCalcCoordDeltas: Delta Time = %lx, %lx",
                        HIDWORD(ppdev->Timer), LODWORD(ppdev->Timer));
#endif  //P5TIME


#if MOVE_XDER_BACK
    // Calculate x derivate values only if we are texturing and doing
    // perspective correction, otherwise they were calculated previously
    if (pRc->privateEnables & __MCDENABLE_TEXPERSP)
        __MCDCalcXDeltaProps(pRc, a, b, c);
    // check if sign changed to flip xdelta signs, this is a convention the
    // S3Virge follows
    else if (bCCW ^ pRc->xDerCCW) {
       __MCDFlipXDeltaProps(pRc);
       pRc->xDerCCW = bCCW;
    }

#else
    // Calculate delta values for unit changes in x or y:
    __MCDCalcXDeltaProps(pRc, a, b, c);
#endif

    // We need the inverse of dyAC to calculate the y derivates
    // of all needed properties along the AC Edge
    if (FABS(pRc->dyAC)>MCDDIVEPS)
        pRc->invdyAC = __MCDONE / pRc->dyAC;
    else
        pRc->invdyAC = __MCDZERO;

    // Calculate y derivates of required properties:
    __MCDCalcYDeltaProps(pRc, a, b, c);

    // If texturing, select the minification or magnification filter
    if (pRc->privateEnables & __MCDENABLE_TEXTURED)
        __MCDSetupTexFilter(pRc, a, b, c, bTrust);

    // Send Delta x and y property values to S3Virge:
    HWSETUPDELTAS(pRc);


    // Adjust starting vertex(0) coordinates and all neccesary properties
    dxdyAC = -(pRc->dxAC * pRc->invdyAC);
    xStart = ADJUSTVERTEXALLPROPS(pRc, a, aIY, dxdyAC);


    if (FABS(pRc->dyBC) > MCDDIVEPS)
        dxdyBC = -(pRc->dxBC / pRc->dyBC);

    if (bIY != cIY) {
        // Adjust coordinates of vertex for S3V
        xEndBC = __AdjustVertexPosition(b,bIY,dxdyBC);
    } else {
        xEndBC = b->windowCoord.x;
    }

#ifndef S3D_DMA
    // Maximum fifo entries we need to set the triangle x,y registers
    // and send the draw command
    S3DFifoWait(pRc->ppdev, 8);
#endif

    S3DWRITE( ppdev,
              TRI_X_Y_1_2_DELTA,
              FLT_TO_FIX_SCALE(dxdyBC,S3_S20_SCALE));

    S3DWRITE( ppdev,
              TRI_X_Y_0_2_DELTA,
              FLT_TO_FIX_SCALE(dxdyAC,S3_S20_SCALE));

    S3DWRITE( ppdev,
              TRI_X_1_2_END    ,
              FLT_TO_FIX_SCALE( xEndBC + (LONG)pRc->xOffset -
                                __MCDHALF, S3_S20_SCALE));


    if (FABS(pRc->dyAB) > MCDDIVEPS)
        dxdyAB = -(pRc->dxAB / pRc->dyAB);

    if (aIY != bIY) {
        // Adjust coordinates of vertex for S3V
        xEndAB = __AdjustVertexPosition(a, aIY, dxdyAB);
    } else {
        xEndAB = a->windowCoord.x;
    }

    S3DWRITE( ppdev,
              TRI_X_Y_0_1_DELTA,
              FLT_TO_FIX_SCALE(dxdyAB, S3_S20_SCALE));

    S3DWRITE( ppdev,
              TRI_X_0_1_END    ,
              FLT_TO_FIX_SCALE(xEndAB + (LONG)pRc->xOffset -
                               __MCDHALF, S3_S20_SCALE));

    // Send remaining parameters and draw
   HWDRAWCOMMAND( pRc, aIY, aIY - bIY , bIY - cIY , bCCW);

#if TEX_NON_SQR
    //Restore u,v coordinates if non-square texture maps are involved
    if ((pRc->privateEnables & __MCDENABLE_TEXTURED) &&
        (pRc->privateEnables & __MCDENABLE_TEX_NONSQR))
    {
        a->texCoord.x = aTexOld.x;
        b->texCoord.x = bTexOld.x;
        c->texCoord.x = cTexOld.x;
        a->texCoord.y = aTexOld.y;
        b->texCoord.y = bTexOld.y;
        c->texCoord.y = cTexOld.y;
    }
#endif

    if (pRc->privateEnables & __MCDENABLE_TEXPERSP)
    {
        pRc->uvScale = fuvScaleOld;
        pRc->wScale  = fwScaleOld;
    }

#if DBG_TESSELATION
    if (!bTrust)
        DbgTessEnd(pRc);
#endif
}

//**************************************************************************
//
// VOID FASTCALL HWTriangleSetupClipping
//
// Setup window clipping parameters for triangle rendering
//
//**************************************************************************
VOID FASTCALL HWTRIANGLECLIPPING(DEVRC *pRc, RECTL *pClip)
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

#ifdef S3D_DMA

    WAIT_DMA_IDLE( ppdev );

#endif

    CHECK_DMA_SLOT(ppdev, 8 + 4);

    WRITE_DMA_HEADER(ppdev, S3D_TRI_CLIP_LEFT_RIGHT, 8);

    S3DWRITE(ppdev, TRI_CLIP_LEFT_RIGHT, ulLeftRight);

    S3DWRITE(ppdev, TRI_CLIP_TOP_BOTTOM, ulTopBottom);

    ADVANCE_DMA_INDEX(ppdev, 8);

    START_DMA(ppdev);

    //
    // Indicate that clipping must be enabled when drawing triangles
    //

    pRc->hwTriClipFunc = S3D_HARDWARE_CLIPPING;
}

