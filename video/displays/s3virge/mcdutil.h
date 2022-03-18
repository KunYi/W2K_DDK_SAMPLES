/******************************Module*Header*******************************\
* Module Name: mcdutil.h
*
* Include file which indirects all of the hardware-dependent functionality
* in the MCD driver code.
*
* Copyright (c) 1996 Microsoft Corporation
\**************************************************************************/

#ifndef _MCDUTIL_H
#define _MCDUTIL_H

//////////////////////////////////////////////////////////////////////
// Constants which define the level or errors/messages reported by the
// driver. 'Or' the desired flags and set up in DBGFLAG variable
//////////////////////////////////////////////////////////////////////
#define DBG_ERR   0x00000001  //Debug fatal errors
#define DBG_WRN   0x00000002  //Debug non-fatal warning conditions
#define DBG_MCD   0x00000004  //Debug High level MCD entry functions
#define DBG_TESS  0x00000008  //Debug triangle tesselation related code
#define DBG_VERT  0x00000010  //Debug vertex properties for each primitive
#define DBG_TRI   0x00000020  //Debug triangle hw rendering code
#define DBG_LINE  0x00000040  //Debug line hw rendering code
#define DBG_PNT   0x00000080  //Debug point hw rendering code
#define DBG_TEX   0x00000100  //Debug texture loading and state handling code
#define DBG_S3REG 0x00000200  //Debug all S3 register reads/writes
#define DBG_PRIM  0x00000400  //Debug primitive preprocessing
#define DBG_PDEV  0x00000800  //Debug PDEV properties (Win95 code)

//
// Copied from hwtr.h.
//
#define S_FifoThresh_Shift  5
#define S_FifoThresh_Mask   (31L << S_FifoThresh_Shift)

#define P_FifoThresh_Shift  10
#define P_FifoThresh_Mask   (31L << P_FifoThresh_Shift)

#if DBG
UCHAR *MCDDbgMemAlloc(UINT);
VOID MCDDbgMemFree(UCHAR *);


#define MCDMemAlloc   MCDDbgMemAlloc
#define MCDMemFree    MCDDbgMemFree

VOID MCDrvDebugPrint(ULONG debugset, char *, ...);

#ifdef MCDBG_PRINT
#undef MCDBG_PRINT
#endif

#define MCDBG_PRINT  MCDrvDebugPrint

#else

UCHAR *MCDMemAlloc(UINT);
VOID MCDMemFree(UCHAR *);
#define MCDBG_PRINT

#endif

#define INTERSECTRECT(RectInter, pRect, Rect)\
{\
    RectInter.left   = max(pRect->left, Rect.left);\
    RectInter.right  = min(pRect->right, Rect.right);\
    RectInter.top    = max(pRect->top, Rect.top);\
    RectInter.bottom = min(pRect->bottom, Rect.bottom);\
}

// Check that some structure member is within a given
// valid (or known) size of the structure. This is used
// to avoid writing to members of structures which are
// used for communication between OPENGL32.DLL and the
// MCD driver
#define MEM_VALID(pStart,pEnd,count)     \
    (((ULONG_PTR)(pEnd) - (ULONG_PTR)(pStart)) < (count))

// Check that we don't overwrite a structure with a field it
// doesn't have because of version mismatch among OPENGL32.DLL
// and the driver.
#define SET_ENTRY(dstPfn,srcPfn,pDstStruct,dstStructSize) \
    if (MEM_VALID(pDstStruct,&(dstPfn),dstStructSize))    \
        dstPfn = srcPfn

#define MCD_CHECK_RC(pRc)\
    if (pRc == NULL) {\
        MCDBG_PRINT(DBG_ERR, "NULL device RC");\
        return FALSE;\
    }

#define MCD_CHECK_BUFFERS_VALID(pMCDSurface, pRc, resChangedRet)\
{\
    DEVWND *pDevWnd = (DEVWND *)pMCDSurface->pWnd->pvUser;\
\
    if (!pDevWnd) {\
        MCDBG_PRINT(DBG_ERR, "HW_CHECK_BUFFERS_VALID: NULL buffers");\
        return FALSE;\
    }\
\
    if ((pRc->backBufEnabled) &&\
        (!pDevWnd->bValidBackBuffer)) {\
        MCDBG_PRINT(DBG_ERR, "HW_CHECK_BUFFERS_VALID: back buffer invalid");\
        return FALSE;\
    }\
\
    if ((pRc->zBufEnabled) &&\
        (!pDevWnd->bValidZBuffer)) {\
        MCDBG_PRINT(DBG_ERR, "HW_CHECK_BUFFERS_VALID: z buffer invalid");\
        return FALSE;\
    }\
\
    if (pDevWnd->dispUnique !=                                          \
                GetDisplayUniqueness((PDEV *)pMCDSurface->pso->dhpdev)) {\
        MCDBG_PRINT(DBG_ERR, "HW_CHECK_BUFFERS_VALID: resolution changed but not updated");\
        return resChangedRet;\
    }\
}

#ifdef MCD95
//
// For use with MCD95 in MCDrvGetBuffers only.
//
#define MCD95_CHECK_GETBUFFERS_VALID(pMCDSurface)\
{\
    DEVWND *pDevWnd = (DEVWND *)pMCDSurface->pWnd->pvUser;\
\
    if (!pDevWnd) {\
        MCDBG_PRINT(DBG_ERR,"HW_CHECK_BUFFERS_VALID: NULL buffers");\
        return FALSE;\
    }\
}
#endif

#define CHK_TEX_KEY(pTex,textureKey);                                   \
        if(pTex == NULL) {                                              \
                MCDBG_PRINT(DBG_ERR, "Attempted to update a null texture");      \
                return FALSE;                                           \
        }                                                               \
                                                                        \
        if(pTex->textureKey == 0) {                                     \
                MCDBG_PRINT(DBG_ERR, "Attempted to update a null device texture");\
                return FALSE;                                           \
        }

#if DBG

extern LONG DBGFLAG;

#ifndef S3D_DMA

    #define S3DWRITE( ppdev, Offset, Value )                                \
                                                                            \
        MCDBG_PRINT( DBG_S3REG,                                             \
                     "S3DWrite %4x   %8x ( S3D_"#Offset "=" #Value ")",     \
                     S3D_##Offset,                                          \
                     Value);                                                \
                                                                            \
        S3writeHIU( ppdev, S3D_##Offset, Value );

#else

    #define S3DWRITE( ppdev, Offset, Value )                                \
                                                                            \
        MCDBG_PRINT( DBG_S3REG,                                             \
                     "DMA_Offset %4x = %8x ( DMA_"#Offset "=" #Value ")",   \
                     DMA_##Offset,                                          \
                     Value);                                                \
                                                                            \
        WRITE_REGISTER_ULONG( (BYTE *)                                      \
                              ( ppdev->pjDmaBuffer +                        \
                                ((ppdev->ulDmaIndex + DMA_##Offset) & 0xFFF)),\
                              Value );                                      \

#endif


#define S3DREAD( ppdev, MMIO_Offset, Var ) \
    Var = S3readHIU( ppdev, MMIO_Offset ); \
    MCDBG_PRINT(DBG_S3REG,"S3Read %4x   %8x (" #MMIO_Offset "=" #Var ")",MMIO_Offset,Var)

#define MCDBG_DUMPVERTEX(v) \
    MCDBG_PRINT(DBG_VERT,"Vertex x=%i y=%i z=%i w=%i r=%i g=%i b=%i a=%i u=%i v=%i", \
               (long)(v->windowCoord.x),(long)(v->windowCoord.y),(long)(v->windowCoord.z),                    \
               (long)(v->windowCoord.w*1000.0),\
               (long)(v->colors[0].r),(long)(v->colors[0].g),(long)(v->colors[0].b),(long)(v->colors[0].a),   \
               (long)(v->texCoord.x * 1000.0),(long)(v->texCoord.y * 1000.0))

#else

#ifndef S3D_DMA

    #define S3DWRITE( ppdev, Offset, Value )                                  \
        S3writeHIU( ppdev, S3D_##Offset, Value );

#else

    #define S3DWRITE( ppdev, Offset, Value )                                  \
        WRITE_REGISTER_ULONG( (BYTE *)                                        \
                              ( ppdev->pjDmaBuffer +                          \
                                ((ppdev->ulDmaIndex + DMA_##Offset) & 0xFFF)),\
                              Value )

#endif

#define S3DREAD( ppdev, MMIO_Offset, Var ) \
    Var = S3readHIU( ppdev, MMIO_Offset );

#define MCDBG_DUMPVERTEX(v)

#endif



//
// Some DMA worker macros.
//
#if DBG

#define DMAWRITE(ppdev, DMA_Offset, Value )                                 \
    MCDBG_PRINT( DBG_S3REG,                                                 \
                 "DMA_Offset %4x = %8x (" #DMA_Offset "=" #Value ")",       \
                 DMA_Offset,                                                \
                 Value);                                                    \
                                                                            \
    WRITE_REGISTER_ULONG( (BYTE *)                                          \
                          ( ppdev->pjDmaBuffer +                            \
                            ((ppdev->ulDmaIndex + DMA_Offset) & 0xFFF)),    \
                          Value )

#else

#define DMAWRITE(ppdev, DMA_Offset, Value )                                 \
    WRITE_REGISTER_ULONG( (BYTE *)                                          \
                          ( ppdev->pjDmaBuffer +                            \
                            ((ppdev->ulDmaIndex + DMA_Offset) & 0xFFF)),    \
                          Value )

#endif


#ifdef S3D_DMA

    //
    // Check to see if enough bytes are available in ulDmaFreeSlots, else read
    // the DMA read pointer to find out the next address to be read by the DMA
    // hardware and spin until more slots are freed up. By keeping track of the
    // number of free slots in ulDmaFreeSlots, we avoid unnecessary PCI read.
    //

    #define CHECK_DMA_SLOT(ppdev, slots) {                                  \
        ULONG dmaReadPtr;                                                   \
                                                                            \
        while (ppdev->ulDmaFreeSlots <= (slots))                            \
        {                                                                   \
            S3DREAD( ppdev, S3D_DMA_READ_POINTER, dmaReadPtr);              \
            if (dmaReadPtr <= ppdev->ulDmaIndex)                            \
                dmaReadPtr += 0x1000;                                       \
            ppdev->ulDmaFreeSlots = (dmaReadPtr - ppdev->ulDmaIndex);       \
        }                                                                   \
    }

    //
    // Clear up the DMA command buffer with 0s. The Virge hardware is parculiar
    // about the unused slots. They need to be cleared with 0s. Therefore, we
    // clear up all the slots we need per triangle or line commands, instead of
    // filling them up one at a time.
    //

    #define CLEAR_DMA_SLOT(ppdev, slots) {                                  \
        ULONG index;                                                        \
        LONG bytesleft;                                                     \
        PUCHAR pj;                                                          \
        LONG i;                                                             \
                                                                            \
        index = ppdev->ulDmaIndex;                                          \
        if ((bytesleft = (index+slots) - 0x1000) <= 0)                      \
        {                                                                   \
            pj = (BYTE *)(ppdev->pjDmaBuffer + index);                      \
            for (i = 0; i < (slots)/4; i++)                                 \
            {                                                               \
                *((ULONG *)pj) = 0L;                                        \
                pj += 4;                                                    \
            }                                                               \
        }                                                                   \
        else                                                                \
        {                                                                   \
            pj = (BYTE *)(ppdev->pjDmaBuffer + index);                      \
            for (i=0; i<(slots-bytesleft)/4; i++)                           \
            {                                                               \
                *((ULONG *)pj) = 0L;                                        \
                pj += 4;                                                    \
            }                                                               \
            pj = (BYTE *)(ppdev->pjDmaBuffer);                              \
            for (i=0; i<bytesleft/4; i++)                                   \
            {                                                               \
                *((ULONG *)pj) = 0L;                                        \
                pj += 4;                                                    \
            }                                                               \
        }                                                                   \
    }

    //
    // DMA header is formated as.
    // Bits 29---------14, 13---------0
    //      Address      , Dwords to write
    //

    #define WRITE_DMA_HEADER(ppdev, value, slot) {                          \
                                                                            \
        DMAWRITE(ppdev, 0, ((value) << 14) | ((slot) >> 2));                \
                                                                            \
        ppdev->ulDmaIndex =                                                 \
            (ppdev->ulDmaIndex + 4) & 0xFFF;                                \
        ppdev->ulDmaFreeSlots -= 4;                                         \
    }

    //
    // Macro for updating some pointers.
    //      ulDmaIndex - last address written to the DMA buffer.
    //      ulDmaFreeSlots - number of free slots available in the DMA buffer.
    //
    #define ADVANCE_DMA_INDEX(ppdev, slots) {                               \
        ppdev->ulDmaIndex =                                                 \
            (ppdev->ulDmaIndex + slots) & 0xFFF;                            \
        ppdev->ulDmaFreeSlots -= slots;                                     \
    }

    //
    // Macro to undo the DMA index in case we decide that we will not
    // handle this DMA operation.
    //
    #define SUBTRACT_DMA_INDEX(ppdev, slots) {                              \
        ppdev->ulDmaIndex =                                                 \
            (ppdev->ulDmaIndex + 0x1000 - slots) & 0xFFF;                   \
        ppdev->ulDmaFreeSlots += slots;                                     \
    }


    //
    // Macro to kick off the DMA operation.
    //
    #define START_DMA(ppdev) {                                              \
        S3writeHIU( ppdev, S3D_DMA_WRITE_POINTER,                           \
            ppdev->ulDmaIndex | S3D_DMA_WRITE_PTR_UPD);                     \
        ppdev->b3DDMAHwUsed = TRUE;                                         \
    }

#else

    #define CHECK_DMA_SLOT( ppdev, slots )
    #define CLEAR_DMA_SLOT( ppdev, slots )
    #define WRITE_DMA_HEADER( ppdev, value, slot )
    #define ADVANCE_DMA_INDEX( ppdev, slots )
    #define SUBTRACT_DMA_INDEX( ppdev, slots )
    #define START_DMA( ppdev )

#endif


__inline LONG ICLAMP(LONG l)
{
    if (l < 0L)
        return 0L;
    else if (l > 0x7FFF)
        return 0x7FFF;
    else
        return l;
}

__inline ULONG S3ModeMask(ULONG mode)
{
    switch (mode) {
    case BMF_8BPP:
        return S3D_DEST_8BPP_INDX;
        break;
    case BMF_16BPP:
        return S3D_DEST_16BPP_1555;
        break;
    case BMF_24BPP:
        return S3D_DEST_24BPP_888;
        break;
    case BMF_32BPP:
    default:
        return 0;
        break;
    }

}

__inline void HW_WAIT_DRAW_TEX(PDEV *ppdev)
{
    // If the last item sent down the pipeline involved
    // a textured primitive, we should wait for it to
    // be completed before setting new values in the
    // texture registers
    if (ppdev->lastRenderedTexKey){
        S3DGPWait(ppdev);
        // we may consider the last item was non-textured
        // since we don't need to wait for hw to de done anymore
        ppdev->lastRenderedTexKey = 0;
    }

}


__inline void HW_WAIT_FOR_VBLANK(MCDSURFACE *pMCDSurface)
{
    PDEV *ppdev = (PDEV *) pMCDSurface->pso->dhpdev;
    BYTE *pjIoBase = ppdev->pjIoBase;

    WAIT_FOR_VBLANK(ppdev);
}

__inline void HW_INIT_DRAWING_STATE(MCDSURFACE *pMCDSurface, MCDWINDOW *pMCDWnd,
                                       DEVRC *pRc)
{

    DEVWND *pDevWnd = (DEVWND *)pMCDWnd->pvUser;
    PDEV   *ppdev   = (PDEV *)pMCDSurface->pso->dhpdev;
    LONG    BufOrg;
    ULONG   zStride;
    ULONG   ulColorBufferStride;

    pRc->hwLineClipFunc          = 0;
    pRc->hwTriClipFunc           = 0;
    pRc->hwVideoMode             = 0;

    WAIT_DMA_IDLE(ppdev); 

    // Note:  zBufferOffset is maintained in MCDrvTrackWindow

    // Z Buffer Setup
    if (pDevWnd->pohZBuffer)
    {
        LONG zDiff;

        ppdev->ulZBufferStride =
        zStride                = pDevWnd->zPitch;

        if (ppdev->cZBufferRef)
        {
            zDiff = pDevWnd->zBufferOffset;
        }
        else
        {
            //
            // align on quadword and pixel boundary
            //

            zDiff = pMCDWnd->clipBoundsRect.left;

            zDiff = pDevWnd->zBufferOffset          -
                    (zDiff - (zDiff & 7))       * 2 -
                    pMCDWnd->clipBoundsRect.top * pDevWnd->zPitch;
        }

        ASSERTDD((zDiff & 0x7) == 0,
                "Z buffer is not aligned to 8-byte boundary.");

        S3DFifoWait(ppdev, 4);
        S3writeHIU(ppdev,S3D_TRI_Z_BASE,zDiff);
        S3writeHIU(ppdev,S3D_3D_LINE_Z_BASE,zDiff);
        S3writeHIU(ppdev,S3D_TRI_Z_STRIDE,zStride);
        S3writeHIU(ppdev,S3D_3D_LINE_Z_STRIDE,zStride);

    }


    // Color Buffer Setup
    if (pRc->MCDState.drawBuffer == GL_FRONT)
    {

        BufOrg                 = 0;
        ulColorBufferStride    = ppdev->lDelta << 16;
    }
    else if (pRc->MCDState.drawBuffer == GL_BACK)
    {
        ppdev->ulBackBufferStride = pDevWnd->ulBackBufferStride;
        ulColorBufferStride       = pDevWnd->ulBackBufferStride << 16;

        if (ppdev->cDoubleBufferRef)
        {
            //
            // Full screen backbuffer
            //

            BufOrg = pDevWnd->backBufferOffset;
        }
        else
        {
            //
            // Per window backbuffer
            //
            // align on a quadword boundary and a pixel boundary,
            //

            BufOrg = pMCDWnd->clipBoundsRect.left;

            BufOrg = pDevWnd->backBufferOffset                      -
                     (BufOrg - (BufOrg & 7))     * ppdev->cjPelSize -
                     pMCDWnd->clipBoundsRect.top * pDevWnd->ulBackBufferStride ;
        }
    }

    S3DFifoWait(ppdev, 4);
    S3writeHIU(ppdev,S3D_TRI_DESTINATION_BASE,BufOrg);
    S3writeHIU(ppdev,S3D_TRI_DESTINATION_SOURCE_STRIDE, ulColorBufferStride);
    S3writeHIU(ppdev,S3D_3D_LINE_DESTINATION_BASE,BufOrg);
    S3writeHIU(ppdev,S3D_3D_LINE_DESTINATION_SOURCE_STRIDE,ulColorBufferStride);


    pRc->hwVideoMode = S3ModeMask(ppdev->iBitmapFormat);

}


__inline void HW_INIT_PRIMITIVE_STATE(MCDSURFACE *pMCDSurface, DEVRC *pRc)
{
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    if (!(pRc->privateEnables & __MCDENABLE_SMOOTH)) {

        // We will be using the interpolation mode of the hardware, but we'll
        // only be interpolating Z, so set the color deltas to 0.

        S3DFifoWait(ppdev,6);

        S3writeHIU(ppdev,S3D_TRI_GREEN_BLUE_X_DELTA, 0x0L);
        S3writeHIU(ppdev,S3D_TRI_ALPHA_RED_X_DELTA , 0x0L);
        S3writeHIU(ppdev,S3D_TRI_GREEN_BLUE_Y_DELTA, 0x0L);
        S3writeHIU(ppdev,S3D_TRI_ALPHA_RED_Y_DELTA , 0x0L);
        S3writeHIU(ppdev,S3D_3D_LINE_GREEN_BLUE_DELTA, 0x0L);
        S3writeHIU(ppdev,S3D_3D_LINE_ALPHA_RED_DELTA , 0x0L);
    }

    if (!(pRc->privateEnables & __MCDENABLE_Z)){

        S3DFifoWait(pRc->ppdev,3);
        S3writeHIU(ppdev, S3D_TRI_Z_X_DELTA         , 0x0L);
        S3writeHIU(ppdev, S3D_TRI_Z_Y_DELTA         , 0x0L);
        S3writeHIU(ppdev, S3D_3D_LINE_Z_DELTA       , 0x0L);
    }
}


__inline void HW_START_FILL_RECT(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc,
                                DEVRC *pRc, ULONG buffers)
{
    PDEV   *ppdev   = (PDEV    *)pMCDSurface->pso->dhpdev;
    DEVWND *pDevWnd = (DEVWND  *)pMCDSurface->pWnd->pvUser;
    BOOL   bFillC   = (buffers & GL_COLOR_BUFFER_BIT) != 0;
    BOOL   bFillZ   = (buffers & GL_DEPTH_BUFFER_BIT) &&
                      pRc->MCDState.depthWritemask;
    ULONG  rBits,gBits,bBits;


    pRc->bNeedFillColorBuffer = bFillC;
    pRc->bNeedFillZBuffer     = bFillZ;

    S3DFifoWait(ppdev, 2);
    S3writeHIU(ppdev,S3D_BLT_MONO_PATTERN_0,0xFFFF);
    S3writeHIU(ppdev,S3D_BLT_MONO_PATTERN_1,0xFFFF);

    if (bFillC){
        rBits = ICLAMP(FTOL(pRc->MCDState.colorClearValue.r*pRc->rScale));
        gBits = ICLAMP(FTOL(pRc->MCDState.colorClearValue.g*pRc->gScale));
        bBits = ICLAMP(FTOL(pRc->MCDState.colorClearValue.b*pRc->bScale));

        switch (ppdev->iBitmapFormat) {
            case BMF_16BPP:
                pRc->hwFillColor =
                    (rBits & 0x7C00)      |
                    (gBits & 0x7C00) >>  5|
                    (bBits & 0x7C00) >> 10;
                break;
            case BMF_24BPP:
                pRc->hwFillColor =
                    (rBits & 0x7F80) <<  9 |
                    (gBits & 0x7F80) <<  1 |
                    (bBits & 0x7F80) >>  7;
                break;
            case BMF_8BPP:
            case BMF_32BPP:
            default:
            MCDBG_PRINT(DBG_ERR, "S3Virge invalid 3D bitmap format");
                return;
                break;
        }

    }

    if (bFillZ){
        pRc->hwFillZValue = (ULONG)(pRc->MCDState.depthClearValue);
    }
}

__inline void HW_FILL_RECT( MCDSURFACE *pMCDSurface,
                            DEVRC      *pRc,
                            RECTL      *pRecl )
{
    PDEV            *ppdev      = (PDEV *)pMCDSurface->pso->dhpdev;
    DEVWND          *pDevWnd    = (DEVWND *)(pMCDSurface->pWnd->pvUser);
    ULONG           ulDstXY;
    ULONG           ulDstSrcStride;
    ULONG           ulBase;
    MCDWINDOW      *pWnd        = pMCDSurface->pWnd;

    //
    // Back buffer check
    //

    if (pRc->MCDState.drawBuffer == GL_BACK)
    {
        ulDstSrcStride  = pDevWnd->ulBackBufferStride << 16;
        ulBase          = pDevWnd->pohBackBuffer->ulBase;
    }
    else
    {
        ulDstSrcStride  = ppdev->lDelta << 16;
        ulBase          = 0;
    }

    if (ppdev->cDoubleBufferRef || pRc->MCDState.drawBuffer != GL_BACK)
    {
        ulDstXY         = (( pRecl->left & 0x07FF ) << 16 ) |
                           ( pRecl->top  & 0x07FF );
    }
    else
    {
        ulDstXY         = ( (pWnd->clipBoundsRect.left & 7) +
                            pRecl->left - pWnd->clipBoundsRect.left) << 16 |
                          (pRecl->top - pWnd->clipBoundsRect.top);
    }

    S3DGPWait( ppdev );

    S3writeHIU( ppdev,
                S3D_BLT_DESTINATION_X_Y,
                ulDstXY );

    S3writeHIU( ppdev,
                S3D_BLT_WIDTH_HEIGHT,
                (((pRecl->right - pRecl->left - 1) & 0x07FF) << 16) |
                ((pRecl->bottom - pRecl->top    ) & 0x07FF) );

    //
    // Clear the Color (Frame) Buffer
    //

    if (pRc->bNeedFillColorBuffer)
    {
        //
        // full-screen Back Buffer is almost the same as a front buffer
        // the assumpion is that x will always be 0 for full-screen buffers
        //

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    ulDstSrcStride );

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_BASE,
                    ulBase );

        S3writeHIU( ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, pRc->hwFillColor);


        S3writeHIU( ppdev,
                    S3D_BLT_COMMAND,
                    ppdev->ulCommandBase      |
                    PAT_COPY << S3D_ROP_SHIFT |
                    S3D_MONOCHROME_PATTERN    |
                    S3D_RECTANGLE             |
                    S3D_DRAW_ENABLE           |
                    S3D_X_POSITIVE_BLT        |
                    S3D_Y_POSITIVE_BLT );

        S3DGPWait( ppdev );
    }

    //
    // Clear the Z (Depth) Buffer
    //

    if (pRc->bNeedFillZBuffer)
    {
        if ( ulDstXY != 0 && !ppdev->cZBufferRef)
        {
            S3writeHIU( ppdev,
                        S3D_BLT_DESTINATION_X_Y,
                        ( (pWnd->clipBoundsRect.left & 7) +
                           pRecl->left - pWnd->clipBoundsRect.left) << 16 |
                          (pRecl->top - pWnd->clipBoundsRect.top) );
        }

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_BASE,
                    pDevWnd->zBufferOffset );

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_SOURCE_STRIDE,
                    pDevWnd->zPitch << 16);

        S3writeHIU( ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, pRc->hwFillZValue);


        S3writeHIU( ppdev,
                    S3D_BLT_COMMAND,
                    PAT_COPY << S3D_ROP_SHIFT |
                    S3D_DEST_16BPP_1555       |
                    S3D_MONOCHROME_PATTERN    |
                    S3D_RECTANGLE             |
                    S3D_DRAW_ENABLE           |
                    S3D_X_POSITIVE_BLT        |
                    S3D_Y_POSITIVE_BLT );

        S3DGPWait( ppdev );
    }
}

// Valid pixel formats for 3D and their properties

static DRVPIXELFORMAT drvFormats[] = { {16,  5, 5, 5, 0,   10, 5, 0, 0},
                                       {24,  8, 8, 8, 0,   16, 8, 0, 0}
                                     };

__inline DRVPIXELFORMAT *__HWGetPixelFormat(PDEV *ppdev)
{
    switch (ppdev->cBitsPerPel)
    {
    case 16:  
        return( &drvFormats[0] );
    case 24:
        //
        // we support drawing in 24bpp mode on this driver for
        // illustration purposes, though due to some hw features,
        // it is NOT possible to strictly pass conformance in this mode.
        // (but renderings will look usually fine).
        //
        // If we have a hardware fix, enable 24bpp support!!!
        //

        if (ppdev->ulCaps2 & CAPS2_HW_3DALPHA_FIX)
            return( &drvFormats[1] );
        else
            return( NULL );
    default:
        MCDBG_PRINT(DBG_ERR,"HWGetPixelFormat: device doesn't "
                            "support this bitmap format");
        return( NULL );
    }
}

// External declarations

BOOL __MCDCreateDevWindow(MCDSURFACE *pMCDSurface,MCDRC *pMCDRc);
BOOL __MCDMatchDevWindowPixFmt(MCDSURFACE *pMCDSurface,MCDRC *pMCDRc);

BOOL HWAllocResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso, BOOL zEnabled,
                      BOOL backBufferEnabled);
VOID HWFreeResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso);

VOID FASTCALL __MCDClip2Vert(DEVRC *pRc,MCDVERTEX *newV,
                             MCDVERTEX *orgV,MCDVERTEX *clippedV);

LONG HWUpdateTexState(DEVRC *pRc,ULONG textureKey);
LONG HWAllocTexMemory(PDEV  *ppdev, DEVRC *pRc, DEVMIPMAPLEVEL *newlevel, MCDMIPMAPLEVEL *McdLevel);
LONG HWLoadTexRegion(PDEV *ppdev, DEVRC *pRc, DEVTEXTURE *pDevTexture, ULONG lod, RECTL *pRect);
LONG HWFreeTexMemory(PDEV *ppdev, DEVTEXTURE *pDevTexture);
LONG HWResidentTex(PDEV *ppdev, DEVTEXTURE *pDevTexture);
VOID HWFreeAllTextureMemory(PDEV *ppdev);

#ifdef MCD95
BOOL HWLockResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso,
                     BOOL zBufferEnabled,
                     BOOL backBufferEnabled);
BOOL HWUnlockResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso);
VOID HWValidateResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso);
LONG HWUnlockTexKey(DEVRC *pRc,ULONG textureKey);
#endif

#ifndef MCD95
VOID DumpPohData(ULONG dbgflag, PDEV * ppdev);
#endif


//*****************************************************************************
// MACROS
//
// Calculate deltas on any property varying inside the triangle
// (r,g,b,a,z,u,v,d,w). If PropVarName is "dp", then DEVRC needs
// to have members named dpAC,dpAB,dpdx,dpdy (MCDFLOAT) and fxdpdx,
// fxdpdy (MCDFIXED). Notice also that the way to compute dpdx and
// dpdy is different, S3Virge needs these values to be computed THIS way.
//
//*****************************************************************************


#define START_CALC_PROP_DELTA_W_BASE_AB(PropVarName, Prop , PropW, Base)      \
    pRc->PropVarName##AB =                                                    \
            (b->Prop - Base)*b->PropW - (a->Prop - Base)*a->PropW

#define START_CALC_PROP_DELTA_W_BASE_AC(PropVarName, Prop , PropW, Base)      \
    pRc->PropVarName##AC =                                                    \
            (c->Prop - Base)*c->PropW - (a->Prop - Base)*a->PropW

#define START_CALC_PROP_DELTA_AB(PropVarName, Prop)                           \
    pRc->PropVarName##AB = b->Prop - a->Prop

#define START_CALC_PROP_DELTA_AC(PropVarName, Prop)                           \
    pRc->PropVarName##AC = c->Prop - a->Prop

#define END_CALC_PROP_DELTA_X(PropVarName,ScaleName)                          \
    pRc->PropVarName##dx =                                                    \
        (pRc->PropVarName##AC * pRc->dyAB - pRc->PropVarName##AB * pRc->dyAC) \
        *oneOverArea;                                                         \
    pRc->fx##PropVarName##dx = FTOL(pRc->PropVarName##dx * ScaleName);        \

#define END_CALC_PROP_DELTA_Y(PropVarName,ScaleName)                          \
    pRc->PropVarName##dyEdge = -(pRc->PropVarName##AC * pRc->invdyAC);        \
    pRc->fx##PropVarName##dyEdge = FTOL(pRc->PropVarName##dyEdge * ScaleName)


#define START_CALC_PROP_DELTA_W_BASE(PropVarName, Prop , PropW, Base)         \
        START_CALC_PROP_DELTA_W_BASE_AB(PropVarName, Prop , PropW, Base);     \
        START_CALC_PROP_DELTA_W_BASE_AC(PropVarName, Prop , PropW, Base)

#define X_CALC_PROP_DELTA_W_BASE(PropVarName, Prop , PropW, Base, Scale)      \
    START_CALC_PROP_DELTA_W_BASE_AB(PropVarName, Prop , PropW, Base);         \
    START_CALC_PROP_DELTA_W_BASE_AC(PropVarName, Prop , PropW, Base);         \
    END_CALC_PROP_DELTA_X(PropVarName, Scale)

#ifdef _X86_
#define X_CALC_PROP_DELTA_ASM(PropVarName, Prop, Scale )                      \
    {                                                                         \
    MCDFLOAT temp = Scale;                                                    \
    __asm mov    esi , pRc                                                    \
    __asm mov    eax , a                                                      \
    __asm mov    ebx , b                                                      \
    __asm mov    ecx , c                                                      \
    __asm fld    DWORD PTR [OFFSET (MCDVERTEX.Prop)][ecx]                     \
    __asm fld    DWORD PTR [OFFSET (MCDVERTEX.Prop)][ebx]                     \
    __asm fld    DWORD PTR [OFFSET (MCDVERTEX.Prop)][eax]                     \
    __asm fsub   ST(1) , ST(0)                                                \
    __asm fsubp  ST(2) , ST(0)                                                \
    __asm fst    DWORD PTR [OFFSET (DEVRC.PropVarName##AB)][esi]              \
    __asm fxch                                                                \
    __asm fst    DWORD PTR [OFFSET (DEVRC.PropVarName##AC)][esi]              \
    __asm fmul   DWORD PTR [OFFSET (DEVRC.dyAB)][esi]                         \
    __asm fxch                                                                \
    __asm fmul   DWORD PTR [OFFSET (DEVRC.dyAC)][esi]                         \
    __asm fsubp  ST(1) , ST(0)                                                \
    __asm fmul   oneOverArea                                                  \
    __asm fst    DWORD PTR [OFFSET (DEVRC.PropVarName##dx)][esi]              \
    __asm fmul   temp                                                         \
    __asm fistp  DWORD PTR [OFFSET (DEVRC.fx##PropVarName##dx)][esi]          \
    }

#else
#define X_CALC_PROP_DELTA_ASM(PropVarName, Prop, Scale )                      \
    START_CALC_PROP_DELTA_AB(PropVarName, Prop);                              \
    START_CALC_PROP_DELTA_AC(PropVarName, Prop);                              \
    END_CALC_PROP_DELTA_X(PropVarName, Scale)
#endif

#define X_CALC_PROP_DELTA(PropVarName, Prop, Scale )                          \
    START_CALC_PROP_DELTA_AB(PropVarName, Prop);                              \
    START_CALC_PROP_DELTA_AC(PropVarName, Prop);                              \
    END_CALC_PROP_DELTA_X(PropVarName, Scale)


#define Y_CALC_PROP_DELTA_W_BASE(PropVarName, Prop , PropW, Base, Scale)      \
    START_CALC_PROP_DELTA_W_BASE_AC(PropVarName, Prop , PropW, Base);         \
    END_CALC_PROP_DELTA_Y(PropVarName, Scale)

#define Y_CALC_PROP_DELTA(PropVarName, Prop, Scale )                          \
    START_CALC_PROP_DELTA_AC(PropVarName, Prop);                              \
    END_CALC_PROP_DELTA_Y(PropVarName, Scale)

#endif /* _MCDUTIL_H */


