/******************************Module*Header*******************************\
* Module Name: mcdutil.c
*
* Contains various utility routines for the S3 Virge MCD driver such as
* rendering-procedure picking functionality and buffer management.
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#ifdef MCD95
#include <stdio.h>
#endif
#include "hw3d.h"
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"
#ifdef MCD95
#include "mcd95ini.h"
#endif

#define MCD_ALLOC_TAG   'dDCM'

// Set this flag according to the messages/conditions you want to
// trace either from the debugger or in the source code. By default
// it will trace error conditions (DBG_ERR). See mcdutil.h for
// listing of valid values (can be or'ed)
LONG DBGFLAG = DBG_ERR;

#ifdef MCD95
#define BACKBUF_ALLOC_FLAGS     (FLOH_MAKE_PERMANENT | FLOH_BACKBUFFER)
#define ZBUF_ALLOC_FLAGS        (FLOH_MAKE_PERMANENT | FLOH_ZBUFFER)
#else
#define BACKBUF_ALLOC_FLAGS     (FLOH_MAKE_PERMANENT)
#define ZBUF_ALLOC_FLAGS        (FLOH_MAKE_PERMANENT)
#endif


//
// Make an IOCTL to miniport driver and check for Virge DX/GX capability.
//
VOID CheckVirgeCap(
    IN PDEV* ppdev
    )
{
    DWORD            ReturnedDataLength;
    DWORD            KeyNameLength;
    VIDEO_DMA_BUFFER DmaBuffer;
    DWORD            dwCommandDMAEnabled = 0;
#if DBG
    DWORD            dwRC;
#endif

    KeyNameLength = (wcslen(COMMAND_DMA_KEYNAME) + 1 ) * 2;

    EngDeviceIoControl( ppdev->hDriver,
                        IOCTL_VIDEO_S3_GET_REGISTRY_VALUE,
                        COMMAND_DMA_KEYNAME,
                        KeyNameLength,
                        &dwCommandDMAEnabled,
                        sizeof(ULONG),
                        &ReturnedDataLength );

    //
    // Ask the mini-port to allocate the DMA buffer if Virge DX/GX, Rev. B.
    //

#if DBG
    dwRC =
#endif

    EngDeviceIoControl( ppdev->hDriver,
                        IOCTL_VIDEO_S3_GET_DMA_BUFFER,
                        &dwCommandDMAEnabled,
                        sizeof(DWORD),
                        &DmaBuffer,
                        sizeof(VIDEO_DMA_BUFFER),
                        &ReturnedDataLength );

    ppdev->pjDmaBuffer = DmaBuffer.pjDmaBuffer;
    MCDBG_PRINT(DBG_MCD,"Linear address to Dma Buffer = %lx", ppdev->pjDmaBuffer);

    ppdev->ulChipID = DmaBuffer.ulChipID;
    MCDBG_PRINT(DBG_MCD,"Chip ID = %lx", ppdev->ulChipID);

#if DBG
    if ( dwRC )
    {
        MCDBG_PRINT(DBG_ERR,"Failed VIDEO_S3_GET_DMA_BUFFER");
    }
#endif

    // Maximum delta U and V range allowed.
    //      Virge and Virge/VX is 128.
    //      Virge DX, GX, M5 and GX-2 is 2048.
    // Number of bits to shift for U and V related registers.
    //      TdVdX(B51C), TdUdX(B520), TdVdY(B528), TdUdY(B52C),
    //      TVS(B534) and TUS(B538).
    //

    if (ppdev->ulChipID == SUBTYPE_325 || ppdev->ulChipID == SUBTYPE_988)
    {
        ppdev->uvMaxTexels = (MCDFLOAT)128.0;
    }
    else
    {
        ppdev->uvMaxTexels = (MCDFLOAT)2048.0;
    }

    //
    // Setup MMIO or DMA worker routines for various __MCD functions.
    //

    if (ppdev->pjDmaBuffer)
    {
        ppdev->mcdFillSubTriangle  = __MCDFillSubTriangleDMA;
        ppdev->mcdHWLine           = __MCDRenderHWLineDMA;
        ppdev->hwLineSetupClipRect = HWLineSetupClippingDMA;
        ppdev->hwTriSetupClipRect  = HWTriangleSetupClippingDMA;
        MCDBG_PRINT(DBG_MCD,"Physical address to Dma Buffer = %lx\n", 
                    DmaBuffer.ulDmaPhysical);

        // 23-4.
        // The DMA read pointer is set to the address of the next dword to
        // be read by the DMA. Once initalized, the read pointer will be
        // updated by Virge automatically. The write pointer needs to be
        // set to the NEXT dword address after the last dword written to
        // the system memory. This needs to be setup on each DMA batch.
        //
        S3writeHIU(ppdev, S3D_DMA_READ_POINTER, 0L);

        S3writeHIU(ppdev, S3D_DMA_WRITE_POINTER, 0L);

        // Virge DX/GX, M5 and GX-2 do 4K DMA buffer.
        S3writeHIU(ppdev, S3D_DMA_COMMAND_BASE,
                          DmaBuffer.ulDmaPhysical | S3D_DMA_4K_BUFFER);

        // Number of free slots to start with should be 4K.
        ppdev->ulDmaFreeSlots = 0x1000L;

        // Turn on DMA for now.
        S3writeHIU(ppdev, S3D_DMA_ENABLE_REG, S3D_DMA_ENABLE);
    }
    else
    {
        ppdev->mcdFillSubTriangle  = __MCDFillSubTriangle;
        ppdev->mcdHWLine           = __MCDRenderHWLine;
        ppdev->hwLineSetupClipRect = HWLineSetupClipping;
        ppdev->hwTriSetupClipRect  = HWTriangleSetupClipping;
    }
}


//
// Memory allocation, free and debugging versions
//
#if DBG

ULONG MCDrvAllocMemSize = 0;

UCHAR *MCDDbgMemAlloc(UINT size)
{
    UCHAR *pRet;

#ifdef MCD95
    if (pRet = (UCHAR *)LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT,
                                    size + sizeof(ULONG))) {
#else
    if (pRet = (UCHAR *)EngAllocMem(FL_ZERO_MEMORY, size + sizeof(ULONG),
                                    MCD_ALLOC_TAG)) {
#endif
        MCDrvAllocMemSize += size;
        *((ULONG *)pRet) = size;
        return (pRet + sizeof(ULONG));
    } else
        return (UCHAR *)NULL;
}

VOID MCDDbgMemFree(UCHAR *pMem)
{
    if (!pMem) {
        MCDBG_PRINT(DBG_ERR,"MCDDbgMemFree: Attempt to free NULL pointer.");
        return;
    }

    pMem -= sizeof(ULONG);

    MCDrvAllocMemSize -= *((ULONG *)pMem);

    MCDBG_PRINT(DBG_MCD,"MCDDbgMemFree: %x bytes in use.", MCDrvAllocMemSize);

#ifdef MCD95
    LocalFree((VOID *)pMem);
#else
    EngFreeMem((VOID *)pMem);
#endif
}

VOID MCDrvDebugPrint(ULONG debugset, char *pMessage, ...)
{
    va_list ap;
    va_start(ap, pMessage);

    if (DBGFLAG & debugset) {
#ifdef MCD95
        char buffer[256];

        OutputDebugStringA("["MCD_DLL_NAME"] ");
        vsprintf(buffer, pMessage, ap);
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
#else
        EngDebugPrint("[MCD DRIVER] ", pMessage, ap);
        EngDebugPrint("", "\n", ap);
#endif
    }

    va_end(ap);
}


#else /* DBG */


UCHAR *MCDMemAlloc(UINT size)
{
#ifdef MCD95
    return (UCHAR *)LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT, size);
#else
    return (UCHAR *)EngAllocMem(FL_ZERO_MEMORY, size, MCD_ALLOC_TAG);
#endif
}


VOID MCDMemFree(UCHAR *pMem)
{
#ifdef MCD95
    LocalFree((VOID *)pMem);
#else
    EngFreeMem((VOID *)pMem);
#endif
}


#endif /* DBG */


#ifndef MCD95
//**************************************************************************
//
// VOID DumpPohData
//
// Debugging function to check how memory blocks are being used
//
//**************************************************************************
VOID DumpPohData(ULONG dbgflag, PDEV * ppdev)
{
    OH* pohtemp;
    pohtemp = &(ppdev->heap.ohPermanent);
    do {
        pohtemp = pohtemp->pohNext;
        MCDBG_PRINT(dbgflag,"Permanent blk, x=%i y=%i, cx=%i, cy=%i",
            pohtemp->x,pohtemp->y,pohtemp->cx,pohtemp->cy);
    }while (&(ppdev->heap.ohPermanent) != pohtemp);

    pohtemp = &(ppdev->heap.ohFree);
    do {
        pohtemp = pohtemp->pohNext;
        MCDBG_PRINT(dbgflag,"Free blk, x=%i y=%i, cx=%i, cy=%i",
            pohtemp->x,pohtemp->y,pohtemp->cx,pohtemp->cy);
    }while (&(ppdev->heap.ohFree) != pohtemp);
}
#endif

//**************************************************************************
//
// VOID FASTCALL NullRenderPoint
//
// Empty function call to render a point when the state forces it to be culled
//
//**************************************************************************
VOID FASTCALL NullRenderPoint(DEVRC *pRc, MCDVERTEX *pv)
{
}

//**************************************************************************
//
// VOID FASTCALL NullRenderLine
//
// Empty function call to render a line when the state forces it to be culled
//
//**************************************************************************
VOID FASTCALL NullRenderLine(DEVRC *pRc, MCDVERTEX *pv1,
                             MCDVERTEX *pv2, BOOL bReset)
{
}

//**************************************************************************
//
// VOID FASTCALL NullRenderTri
//
// Empty function call to render a triangle when the state forces it to be
// culled
//
//**************************************************************************
VOID FASTCALL NullRenderTri(DEVRC *pRc, MCDVERTEX *pv1,
                            MCDVERTEX *pv2, MCDVERTEX *pv3)
{
}

//**************************************************************************
//
// VOID FASTCALL NullRenderTri
//
// Empty function call when a primitive can't be rendered. It returns the
// command pointer unchanged, forcing MCDrvDraw to kickback to sw rendering
//
//**************************************************************************
MCDCOMMAND * FASTCALL FailPrimDraw(DEVRC *pRc, MCDCOMMAND *pCmd)
{
    return pCmd;
}

//**************************************************************************
//
// BOOL PickPointFuncs
//
// Determines if rendering state can be succesfully hw rendered in the case
// of points and sets up appropriate rendering function pointers. Rejection
// cases are dependent on the hw capabilities.
//
//**************************************************************************
BOOL PickPointFuncs(DEVRC *pRc)
{
    ULONG enables = pRc->MCDState.enables;

    pRc->drawPoint = NULL;   // assume failure

    if (pRc->MCDState.enables & (MCD_ALPHA_TEST_ENABLE )) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Points can't be alpha tested");
        return FALSE;
    }


    if (enables & (MCD_POINT_SMOOTH_ENABLE)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Points can't be SMOOTH");
        return FALSE;
    }

    if ((enables & MCD_FOG_ENABLE) && (!pRc->bCheapFog)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Points can't be fogged unless "
                            "they're cheap fogged");
        return FALSE;
    }

    if (pRc->MCDState.pointSize != __MCDONE) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Points can't have size > 1");
        return FALSE;
    }

// First, get high-level rendering functions:

    if ((pRc->MCDState.enables & MCD_DITHER_ENABLE) ||
        (pRc->ppdev->iBitmapFormat != BMF_16BPP))
        pRc->renderPoint = __MCDRenderGenericPoint;
    else {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Points can't be non-dithered in 16bpp");
        return FALSE;
    }

    if ((pRc->bCheapFog) && (pRc->MCDState.shadeModel != GL_SMOOTH)) {
        pRc->renderPointX = pRc->renderPoint;
        pRc->renderPoint = __MCDRenderFogPoint;
    }

// Handle any lower-level rendering if needed:

    pRc->drawPoint = pRc->renderPoint;

    return TRUE;
}

//**************************************************************************
//
// BOOL PickLineFuncs
//
// Determines if rendering state can be succesfully hw rendered in the case
// of lines and sets up appropriate rendering function pointers. Rejection
// cases are dependent on the hw capabilities.
//
//**************************************************************************
BOOL PickLineFuncs(DEVRC *pRc)
{
    ULONG enables = pRc->MCDState.enables;

    pRc->drawLine = NULL;   // assume failure

    //
    // commented out to avoid compiler warnings
    //

    if (enables & (MCD_LINE_SMOOTH_ENABLE |
                   MCD_LINE_STIPPLE_ENABLE)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Lines be SMOOTH or STIPPLED");
        return FALSE;
    }

    if (pRc->MCDState.enables & (MCD_ALPHA_TEST_ENABLE )) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Lines can't be alpha tested");
        return FALSE;
    }

    if ((enables & MCD_FOG_ENABLE) && (!pRc->bCheapFog)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Lines can't be fogged unless "
                            "they're cheap fogged");
        return FALSE;
    }

    if (pRc->MCDState.lineWidth != __MCDONE) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Lines can't have size > 1");
        return FALSE;
    }

// First, get high-level rendering functions:

    if (pRc->MCDState.shadeModel == GL_SMOOTH)
        pRc->renderLine = __MCDRenderSmoothLine;
    else
        pRc->renderLine = __MCDRenderFlatLine;

    if ((pRc->bCheapFog) && (pRc->MCDState.shadeModel != GL_SMOOTH)) {
        pRc->renderLineX = __MCDRenderSmoothLine;
        pRc->renderLine = __MCDRenderFlatFogLine;
    }

// Handle any lower-level rendering if needed:

    pRc->drawLine = pRc->renderLine;

    return TRUE;
}

//**************************************************************************
// BOOL PickTriangleFuncs
//
// Determines if rendering state can be succesfully hw rendered in the case
// of triangles and sets up appropriate rendering function pointers. Rejection
// cases are dependent on the hw capabilities.
//
//**************************************************************************
BOOL PickTriangleFuncs(DEVRC *pRc)
{

    ULONG enables = pRc->MCDState.enables;

    // No polygon stippling can be hw accelerated in S3V
    if (enables & MCD_POLYGON_STIPPLE_ENABLE) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Triangles can't be STIPPLED");
        return FALSE;
    }


    if (enables & (MCD_POLYGON_SMOOTH_ENABLE |
                   MCD_COLOR_LOGIC_OP_ENABLE)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Triangles can't be SMOOTHED "
                            "or work with a LOGIC_OP");
        return FALSE;
    }

    if ((enables & MCD_FOG_ENABLE) && (!pRc->bCheapFog)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Triangles can't be fogged unless "
                            "they're cheap fogged");
        return FALSE;
    }

    // First, get high-level rendering functions.  If we're not GL_FILL'ing
    // both sides of our polygons, use the "generic" function.

    if ((pRc->MCDState.polygonModeFront == GL_FILL) &&
         (pRc->MCDState.polygonModeBack == GL_FILL)) {
        if ((pRc->MCDState.shadeModel == GL_SMOOTH) ||
            (pRc->bCheapFog))
            pRc->renderTri = __MCDRenderSmoothTriangle;
        else
            pRc->renderTri = __MCDRenderFlatTriangle;
    } else {
        pRc->renderTri = __MCDRenderGenTriangle;

        // In this case, we must handle the various fill modes.  We must
        // fail triangle drawing if we can't handle the types of primitives
        // that may have to be drawn.  This logic depends on the line and
        // point pick routines

        if (((pRc->MCDState.polygonModeFront == GL_POINT)
                                    && (!pRc->drawPoint)) ||
            ((pRc->MCDState.polygonModeFront == GL_LINE)
                                    && (!pRc->drawLine))) {
            MCDBG_PRINT(DBG_MCD,"FAIL: HW Triangles can't be drawn at "
                                "the FRONT as points or lines");
            return FALSE;
        }
        if (pRc->privateEnables & __MCDENABLE_TWOSIDED) {
            if (((pRc->MCDState.polygonModeBack == GL_POINT)
                                    && (!pRc->drawPoint)) ||
                ((pRc->MCDState.polygonModeBack == GL_LINE)
                                    && (!pRc->drawLine))) {
                MCDBG_PRINT(DBG_MCD,"FAIL: HW Triangles can't be TWOSIDED "
                                    "and at the other side be points or lines");
                return FALSE;
            }
        }
    }

    if ((pRc->bCheapFog) && (pRc->MCDState.shadeModel != GL_SMOOTH)) {
        pRc->renderTriX = pRc->renderTri;
        pRc->renderTri = __MCDRenderFlatFogTriangle;
    }

    // Handle lower-level triangle rendering:

    pRc->drawTri = __MCDFillTriangle;

    return TRUE;
}


VOID FASTCALL HWSetupClipping(DEVRC *pRc, RECTL *pClip);


//**************************************************************************
//
// VOID __MCDPickRenderingFuncs
//
// Pick the necessary rendering functions associated to the
// rendering context in terms of the current MCDState and
// determine if kick back to sw rendering is necessary
// Rejection cases are dependent on the hw capabilities.
//
//**************************************************************************
VOID __MCDPickRenderingFuncs(DEVRC *pRc, DEVWND *pDevWnd)
{
    BOOL bSupportedZFunc = TRUE;

    pRc->primFunc[GL_POINTS]         = __MCDPrimDrawPoints;
    pRc->primFunc[GL_LINES]          = __MCDPrimDrawLines;
    pRc->primFunc[GL_LINE_LOOP]      = __MCDPrimDrawLineLoop;
    pRc->primFunc[GL_LINE_STRIP]     = __MCDPrimDrawLineStrip;
    pRc->primFunc[GL_TRIANGLES]      = __MCDPrimDrawTriangles;
    pRc->primFunc[GL_TRIANGLE_STRIP] = __MCDPrimDrawTriangleStrip;
    pRc->primFunc[GL_TRIANGLE_FAN]   = __MCDPrimDrawTriangleFan;
    pRc->primFunc[GL_QUADS]          = __MCDPrimDrawQuads;
    pRc->primFunc[GL_QUAD_STRIP]     = __MCDPrimDrawQuadStrip;
    pRc->primFunc[GL_POLYGON]        = __MCDPrimDrawPolygon;

    //Clear flags for internal enabled states
    pRc->privateEnables = 0;

    //We clear all components which will makeup later the hw rendering command
    pRc->hwZFunc        = 0;
    pRc->hwTexFunc      = 0;

    //
    //  If we don't enable SOURCE_ALPHA all the time, we will fail
    //  HCT conformance tests for transformation normals (xformn.c).
    //  Partial fix to SPR 16684.
    //

    pRc->hwBlendFunc = S3D_ALPHA_BLEND_SOURCE_ALPHA;

    // Assume we are going to gouraud or flat shade triangles unless otherwise
    // determined by the texturing state
    pRc->privateEnables |= __MCDENABLE_COLORED;


    // If we're culling everything or not updating any of our buffers, just
    // return for all primitives:

    if (((pRc->MCDState.enables & MCD_CULL_FACE_ENABLE) &&
         (pRc->MCDState.cullFaceMode == GL_FRONT_AND_BACK)) ||
        ((pRc->MCDState.drawBuffer == GL_NONE) &&
         ((!pRc->MCDState.depthWritemask) || (!pDevWnd->bValidZBuffer)))
       ) {
        pRc->renderPoint = NullRenderPoint;
        pRc->renderLine  = NullRenderLine;
        pRc->renderTri   = NullRenderTri;
        pRc->allPrimFail = FALSE;
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitives not actrivated if CULLING "
                            "everything or not updating buffers");
        return;
    }

    /////////////////////////////////////////////////////////////////
    // Determine if we can satisfy the current OpenGL rendering state
    // with our hardware
    /////////////////////////////////////////////////////////////////

    pRc->allPrimFail = TRUE;     // Assume that we fail everything


    // Even though we're set up to handle this in the primitive pick
    // functions, we'll exit early here since we don't actually handle
    // this in the primitive routines themselves:
    if (pRc->MCDState.drawBuffer == GL_FRONT_AND_BACK) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitives dont draw simultaneously"
                            " to FRONT and BACK buffer");
        return;
    }

    if ((pRc->MCDState.twoSided) &&
        (pRc->MCDState.enables & MCD_LIGHTING_ENABLE))
        pRc->privateEnables |= __MCDENABLE_TWOSIDED;

    if (pRc->MCDState.shadeModel == GL_SMOOTH)
        pRc->privateEnables |= __MCDENABLE_SMOOTH;

    // Setup if possible the Z buffering options, else fail

    switch (pRc->MCDState.depthTestFunc) {
        default:
        case GL_NEVER:
            pRc->hwZFunc = S3D_Z_COMP_NEVER ;
            break;
        case GL_LESS:
            pRc->hwZFunc = S3D_Z_COMP_S_LT_B;
            break;
        case GL_EQUAL:
            pRc->hwZFunc = S3D_Z_COMP_S_EQ_B;
            break;
        case GL_LEQUAL:
            pRc->hwZFunc = S3D_Z_COMP_S_LE_B;
            break;
        case GL_GREATER:
            pRc->hwZFunc = S3D_Z_COMP_S_GT_B;
            break;
        case GL_NOTEQUAL:
            pRc->hwZFunc = S3D_Z_COMP_S_NE_B;
            break;
        case GL_GEQUAL:
            pRc->hwZFunc = S3D_Z_COMP_S_GE_B;
            break;
        case GL_ALWAYS:
            pRc->hwZFunc = S3D_Z_COMP_ALWAYS;
            break;
    }

    if (pDevWnd->bValidZBuffer &&
        (pRc->MCDState.enables & MCD_DEPTH_TEST_ENABLE))
        pRc->privateEnables |= __MCDENABLE_Z;

    if (pRc->privateEnables & __MCDENABLE_Z) {
        // We support all depth tests on the S3Virge
        // but still test this to keep it generic
        if (!bSupportedZFunc) {
            pRc->allPrimFail = TRUE;
            MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitive unsupported "
                                "with requested Z function");
            return;
        }

        if ((pRc->MCDState.depthWritemask) && (pRc->zBufEnabled))
            pRc->hwZFunc |=  S3D_Z_BUFFER_NORMAL | S3D_Z_BUFFER_UPDATE;

    } else {
        pRc->hwZFunc = S3D_Z_COMP_ALWAYS | S3D_NO_Z_BUFFERING;
    }

    // The S3Virge cannot mask the color planes
    // Virge does not support destination alpha plane.
    // Therefore, we only check for R,G and B planes here.
    if (!(pRc->MCDState.colorWritemask[0] &&
          pRc->MCDState.colorWritemask[1] &&
          pRc->MCDState.colorWritemask[2])) {
            MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitve cannot use color mask planes");
            return;
    }

    // The S3V does not support color-index mode logical operations
    if ((pRc->MCDState.enables & MCD_COLOR_LOGIC_OP_ENABLE) &&
        !(pRc->MCDState.logicOpMode == GL_COPY)) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitive cant use LOGIC_OP "
                            "unless it is GL_COPY");
        return;
    }


    // Check if texturing is enabled and if we can handle it or have to
    // kick back to software. Later we can check the details of each
    // texture map passed in HWUpdateTexState and kick back to software
    // there too if neccesary.

#if DBG_FAIL_TEX_DRAW
    if (pRc->MCDState.textureEnabled) //To test without texturing
        return;
#endif


    if (pRc->MCDState.textureEnabled)
    {
        pRc->privateEnables |= __MCDENABLE_TEXTURED;

        switch(pRc->MCDState.perspectiveCorrectionHint) {
            case GL_NICEST:
            case GL_DONT_CARE:
                pRc->privateEnables |= __MCDENABLE_TEXPERSP;
                break;
            case GL_FASTEST:
                //do nothing, just normal texturing
                break;
            default:
                MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitive uses unknown "
                                    "perspective correction hint");
                pRc->allPrimFail = TRUE;
                return; // unknown perspective correction hint
        }

        //  __MCDENABLES__TEXMIPMAP is set in HWUpdateTexState when the
        // texture is actually examined and it is verified that a mipmap
        // structure exists

    }


    // No stenciling possible on the S3V
    if (pRc->MCDState.enables & MCD_STENCIL_TEST_ENABLE) {
        MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitive Stecil request not fulfilled");
        return;
    }


    // No Alpha testing or stenciling on the S3V , unless ...
    if (pRc->MCDState.enables & MCD_ALPHA_TEST_ENABLE)
#if TEXALPHA_FUNC
        if ((pRc->MCDState.alphaTestFunc == GL_GREATER) &&
            (pRc->MCDState.alphaTestRef  < __MCDONE)    &&
            (pRc->MCDState.alphaTestRef  > __MCDZERO)   &&
            (pRc->MCDState.textureEnabled))
            pRc->privateEnables |= __MCDENABLE_TEXALPHAFUNC_MODE;
        else
#endif
        {
            MCDBG_PRINT(DBG_MCD,"FAIL: HW Primitive Alpha Func "
                                "request not fulfilled");
            return;
        }


    // We do blending on the S3Virge only if
    //      sfactor = GL_ONE       and dfactor = GL_ZERO
    //   or sfactor = GL_SRC_ALPHA and dfactor = GL_ONE_MINUS_SRC_ALPHA
    if (pRc->MCDState.enables & MCD_BLEND_ENABLE)
    {
        if ((pRc->MCDState.blendSrc == GL_ONE) &&
            (pRc->MCDState.blendDst == GL_ZERO))
        {
            NULL; //this is equivalent to no blending done!
        }
        else
        {
            if ((pRc->MCDState.blendSrc == GL_SRC_ALPHA) &&
                (pRc->MCDState.blendDst == GL_ONE_MINUS_SRC_ALPHA))
            {
                pRc->privateEnables |= __MCDENABLE_BLEND;
                pRc->hwBlendFunc = S3D_ALPHA_BLEND_SOURCE_ALPHA;
            }
#if TEXLUMSPEC_BLEND_DKN
            // Special case in which luminance texture & blending
            // can be accomplished
            else if ((pRc->MCDState.blendSrc == GL_ZERO) &&
                     (pRc->MCDState.blendDst == GL_ONE_MINUS_SRC_COLOR) &&
                     (pRc->MCDState.textureEnabled))
            {
                pRc->privateEnables |= __MCDENABLE_TEXLUMSPEC_BLEND_MODE_DKN;
            }
#endif
            else
            {
                MCDBG_PRINT(DBG_MCD,
                            "FAIL: HW Primitives can't use blending mode (%i and %i)",
                            pRc->MCDState.blendSrc,pRc->MCDState.blendDst);
                return;
            }
        }
    }


    //////////////////////////////////////////////////////////////////////
    // Setup necessary data structures and state that will be needed later
    //////////////////////////////////////////////////////////////////////

    // Determine if we have "cheap" fog:
    pRc->bCheapFog = FALSE;

    if (pRc->MCDState.enables & MCD_FOG_ENABLE) {
        if (!(pRc->MCDState.textureEnabled) &&
             (pRc->MCDState.fogHint != GL_NICEST)) {
            pRc->bCheapFog = TRUE;
            pRc->privateEnables |= __MCDENABLE_SMOOTH;
            if ((pRc->MCDState.fogColor.r == pRc->MCDState.fogColor.g) &&
                (pRc->MCDState.fogColor.r == pRc->MCDState.fogColor.b))
                pRc->privateEnables |= __MCDENABLE_GRAY_FOG;
        }
    }


    // Build lookup table for face direction

    switch (pRc->MCDState.frontFace) {
        case GL_CW:
            pRc->polygonFace[__MCD_CW]  = __MCD_BACKFACE;
            pRc->polygonFace[__MCD_CCW] = __MCD_FRONTFACE;
            break;
        case GL_CCW:
            pRc->polygonFace[__MCD_CW]  = __MCD_FRONTFACE;
            pRc->polygonFace[__MCD_CCW] = __MCD_BACKFACE;
            break;
    }

    // Build lookup table for face filling modes:

    pRc->polygonMode[__MCD_FRONTFACE] = pRc->MCDState.polygonModeFront;
    pRc->polygonMode[__MCD_BACKFACE] = pRc->MCDState.polygonModeBack;

    if (pRc->MCDState.enables & MCD_CULL_FACE_ENABLE)
        pRc->cullFlag =
            (pRc->MCDState.cullFaceMode == GL_FRONT ?
                                __MCD_FRONTFACE : __MCD_BACKFACE);
    else
        pRc->cullFlag = __MCD_NOFACE;


    ////////////////////////////////////////////
    // Pickup rendering functions for primitives
    ////////////////////////////////////////////

    // Get rendering functions for points:

    if (!PickPointFuncs(pRc)) {
        pRc->primFunc[GL_POINTS] = FailPrimDraw;
    } else
        pRc->allPrimFail = FALSE;

    // Get rendering functions for lines:

    if (!PickLineFuncs(pRc)) {
        pRc->primFunc[GL_LINES]      =
        pRc->primFunc[GL_LINE_LOOP]  =
        pRc->primFunc[GL_LINE_STRIP] = FailPrimDraw;
    } else
        pRc->allPrimFail = FALSE;

    // Get rendering functions for triangles:

    if (!PickTriangleFuncs(pRc)) {
        pRc->primFunc[GL_TRIANGLES]      =
        pRc->primFunc[GL_TRIANGLE_STRIP] =
        pRc->primFunc[GL_TRIANGLE_FAN]   =
        pRc->primFunc[GL_QUADS]          =
        pRc->primFunc[GL_QUAD_STRIP]     =
        pRc->primFunc[GL_POLYGON]        = FailPrimDraw;
    } else
        pRc->allPrimFail = FALSE;
}

//**************************************************************************
//                    HARDWARE SPECIFIC FUNCTIONS
//**************************************************************************

//**************************************************************************
//
// VOID FASTCALL HWSetupClipping
//
// Setup window clipping parameters for triangle and line rendering
// We're calling this because we don't know beforehand what are we going to
// render.
//
//**************************************************************************
VOID FASTCALL HWSetupClipping(DEVRC *pRc, RECTL *pClip)
{
    HWLineSetupClipping(pRc, pClip);
    HWTriangleSetupClipping(pRc, pClip);
}


#if DBG_BUFALLOC

#undef pohAllocate
#undef pohFree
#define pohAllocate
#define pohFree

#endif

BOOL HWAllocResources( MCDWINDOW    *pMCDWnd,
                       SURFOBJ      *pso,
                       BOOL         zBufferEnabled,
                       BOOL         backBufferEnabled )
{
    DEVWND  *pDevWnd        = (DEVWND *)pMCDWnd->pvUser;
    PDEV    *ppdev          = (PDEV *)pso->dhpdev;
    OH*     pohBackBuffer   = NULL;
    OH*     pohZBuffer      = NULL;
    ULONG   w;
    ULONG   width;
    ULONG   height;
    ULONG   zHeight;
    ULONG   bufferExtra;
    ULONG   wPow2;
    ULONG   zPitch;
    LONG    cxMax           = ppdev->cxMemory;
    LONG    xDim            = pMCDWnd->clientRect.right - pMCDWnd->clientRect.left;

#if DEBUG_OFFSCREEN
    static OH fakeOh[2];

    pohBackBuffer    = &fakeOh[0];
    pohZBuffer       = &fakeOh[1];

    pohBackBuffer->y = 512;
    pohZBuffer->y    = 256;
#endif

    MCDBG_PRINT(DBG_MCD,"HWAllocResources");

#if DEBUG_OFFSCREEN
    width  = ppdev->cxMemory;
    height = 256;
#else
    width  = ppdev->cxMemory;
    height = min(pMCDWnd->clientRect.bottom - pMCDWnd->clientRect.top,
                 ppdev->cyScreen);
#endif

    if (width==0 || height == 0 || xDim == 0)
    {
        return FALSE;
    }

    //
    // Assume failure:
    //

    pDevWnd->allocatedBufferHeight = 0;
    pDevWnd->bValidBackBuffer      =
    pDevWnd->bValidZBuffer         = FALSE;
    pDevWnd->pohBackBuffer         =
    pDevWnd->pohZBuffer            = NULL;

    zPitch                         = ppdev->cxMemory * 2;

    //
    // We have to be able to keep our buffers 8-byte aligned, so calculate
    // extra scan lines needed to aligned scan on a 8-byte boundary
    //

    for ( wPow2 = 1, w = zPitch;
          (w) && !(w & 1);
          w >>= 1, wPow2 *= 2 );

    //
    // z buffer granularity is 8 bytes...
    //

    bufferExtra = 8 / wPow2;

    //
    // Now adjust the number of extra scan lines needed for the pixel format
    // we're using, since the z and color stride may be different...
    //

    switch (ppdev->iBitmapFormat)
    {
    case BMF_16BPP:
        zHeight     = height     + bufferExtra;
        break;

    case BMF_24BPP:
        zHeight     = (height     + bufferExtra) * 2 / 3;

        if ( ((height + bufferExtra) *2) % 3 != 0)
        {
            zHeight++;
        }

        break;

    case BMF_8BPP:
    case BMF_32BPP:
    default:
        return FALSE;
    }

    pDevWnd->numPadScans = bufferExtra;

    //
    // Add extra scans for alignment:
    //

    height += bufferExtra;

    //
    // Before we begin, boot all the discardable stuff from offscreen
    // memory:
    //

    bMoveAllDfbsFromOffscreenToDibs(ppdev);

    //
    // Now, try to allocate per-window resources:
    //

    width = (xDim + HEAP_X_ALIGNMENT - 1) & ~(HEAP_X_ALIGNMENT - 1);

    if (backBufferEnabled)
    {
        height = (height * width + cxMax - 1)/cxMax;

        MCDBG_PRINT(DBG_MCD,"BackBuffer: (R,L)=%d,%d, (W,H)=%d,%d",
                 pMCDWnd->clientRect.right, pMCDWnd->clientRect.left,
                 cxMax, height);

#if !DBG_BUFALLOC
        pohBackBuffer = pohAllocate(ppdev, cxMax,
                                    height, BACKBUF_ALLOC_FLAGS);
#else
        pohBackBuffer = &fakeOh[0];
#endif

        if (!pohBackBuffer)
        {
            return FALSE;
        }

        pDevWnd->ulBackBufferStride  =
        pohBackBuffer->lDelta        =
        ppdev->ulBackBufferStride    = width * ppdev->cjPelSize;
        ppdev->cDoubleBufferRef      = 0;
    }

    if (zBufferEnabled)
    {
        zHeight = (zHeight * width + cxMax - 1)/cxMax;

        MCDBG_PRINT(DBG_MCD,"BackBuffer: (R,L)=%d,%d, (W,H)=%d,%d",
                 pMCDWnd->clientRect.right, pMCDWnd->clientRect.left,
                 cxMax, zHeight);

#if !DBG_BUFALLOC
        pohZBuffer = pohAllocate(ppdev, cxMax, zHeight,
                                 ZBUF_ALLOC_FLAGS);
#else
        pohZBuffer = &fakeOh[1];
#endif

        if (!pohZBuffer)
        {
            if (pohBackBuffer)
            {
                pohFree(ppdev, pohBackBuffer);
            }
            return FALSE;
        }

        pDevWnd->zPitch           =
        pohZBuffer->lDelta        =
        ppdev->ulZBufferStride    = width * 2;
        ppdev->cZBufferRef        = 0;
    }

    pDevWnd->allocatedBufferWidth  = xDim;
    pDevWnd->allocatedBufferHeight =
        min(pMCDWnd->clientRect.bottom - pMCDWnd->clientRect.top,
            ppdev->cyScreen);

#if DBG
    if (zBufferEnabled)
    {
        MCDBG_PRINT(DBG_MCD,"HWAllocResources: Allocated window-sized z buffer");
    }

    if (backBufferEnabled)
    {
        MCDBG_PRINT(DBG_MCD,"HWAllocResources: Allocated window-sized back buffer");
    }
#endif

    pDevWnd->pohBackBuffer = pohBackBuffer;
    pDevWnd->pohZBuffer    = pohZBuffer;


#ifdef MCD95

    pDevWnd->bValidBackBuffer = FALSE;
    pDevWnd->bValidZBuffer = FALSE;

#else

    // On MCD for Win95, computations that rely on poh->x and/or poh->y
    // cannot be done until after pohLock is called.  Therefore, these
    // must be moved to HWLockResources.

    S3DFifoWait(ppdev, 5);
    S3writeHIU(ppdev,S3D_BLT_MONO_PATTERN_0,0xFFFF);
    S3writeHIU(ppdev,S3D_BLT_MONO_PATTERN_1,0xFFFF);

    S3writeHIU( ppdev,
                S3D_BLT_DESTINATION_X_Y,
                0 );

    S3writeHIU( ppdev,
                S3D_BLT_PATTERN_FOREGROUND_COLOR,
                0 );

    S3writeHIU( ppdev,
                S3D_BLT_DESTINATION_SOURCE_STRIDE,
                ppdev->lDelta << 16 | ppdev->lDelta );

    //
    // Calculate back buffer variables:
    //

    if (backBufferEnabled)
    {
        pDevWnd->backBufferOffset   = pohBackBuffer->ulBase;
        pDevWnd->bValidBackBuffer   = TRUE;

        ASSERTDD(pohBackBuffer->x == 0,
                 "Back buffer should be 0-aligned");

        // Set up base position, etc.


        S3DFifoWait(ppdev, 2);

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_BASE,
                    pohBackBuffer->ulBase );

        S3writeHIU( ppdev,
                    S3D_BLT_WIDTH_HEIGHT,
                    ( pohBackBuffer->cy       & 0x07FF) |
                    ( (pohBackBuffer->cx - 1) & 0x07FF) << 16 );

        S3DGPWait(ppdev);

        S3writeHIU( ppdev,
                    S3D_BLT_COMMAND,
                    ppdev->ulCommandBase          |
                    (PAT_COPY << S3D_ROP_SHIFT)   |
                    S3D_MONOCHROME_PATTERN        |
                    S3D_RECTANGLE                 |
                    S3D_DRAW_ENABLE               |
                    S3D_X_POSITIVE_BLT            |
                    S3D_Y_POSITIVE_BLT );
    }

    if (zBufferEnabled)
    {
        pDevWnd->zBufferBase    =
        pDevWnd->zBufferOffset  = pohZBuffer->ulBase;
        pDevWnd->bValidZBuffer  = TRUE;

        ASSERTDD(pohZBuffer->x == 0,
                 "Z buffer should be 0-aligned");

        //
        // NAC - added this, no need for hwupdatebufferpos, base address is
        //       always qword aligned.
        //


        S3DFifoWait(ppdev, 2);

        S3writeHIU( ppdev,
                    S3D_BLT_DESTINATION_BASE,
                    pohZBuffer->ulBase );

        S3writeHIU( ppdev,
                    S3D_BLT_WIDTH_HEIGHT,
                    ( pohZBuffer->cy       & 0x07FF) |
                    ( (pohZBuffer->cx - 1) & 0x07FF) << 16);

        S3DGPWait(ppdev);

        S3writeHIU( ppdev,
                    S3D_BLT_COMMAND,
                    PAT_COPY << S3D_ROP_SHIFT     |
                    S3D_DEST_16BPP_1555           |
                    S3D_MONOCHROME_PATTERN        |
                    S3D_RECTANGLE                 |
                    S3D_DRAW_ENABLE               |
                    S3D_X_POSITIVE_BLT            |
                    S3D_Y_POSITIVE_BLT );
    }

#endif

    MCDBG_PRINT(DBG_MCD,"HWAllocResources OK");

    return TRUE;
}

#ifdef MCD95
//**************************************************************************
//
// VOID HWValidateResources
//
// Validate back and depth buffers
//
//**************************************************************************
VOID HWValidateResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso)
{
    DEVWND *pDevWnd;
    PDEV *ppdev = (PDEV *)pso->dhpdev;

    ASSERTDD(ppdev, "HWValidateResources: invalid ppdev");

    // Is there a pDevWnd?

    if (pMCDWnd && (pDevWnd = (DEVWND *) pMCDWnd->pvUser))
    {
        // Validate back buffer.

        if (pDevWnd->pohBackBuffer) {

            // Is back buffer surface info.

            if (pohValid(ppdev, pDevWnd->pohBackBuffer) &&
                pDevWnd->pohBackBuffer->pvScan0) {
                ASSERTDD(pDevWnd->pohBackBuffer->x == 0,
                         "HWValidateResources: Back buffer should be 0-aligned");

                // Set up base position, etc.

                pDevWnd->backBufferOffset = pDevWnd->pohBackBuffer->ulBase;
                pDevWnd->bValidBackBuffer = TRUE;
            } else {
                pDevWnd->bValidBackBuffer = FALSE;
            }
        } else {
            pDevWnd->bValidBackBuffer = FALSE;
        }

        // Validate zbuffer if enabled.

        if (pDevWnd->pohZBuffer) {

            // Lock poh into memory.

            if (pohValid(ppdev, pDevWnd->pohZBuffer) &&
                pDevWnd->pohZBuffer->pvScan0) {
                ASSERTDD(pDevWnd->pohZBuffer->x == 0,
                         "HWValidateResources: Z buffer should be 0-aligned");

                pDevWnd->zBufferBase = pDevWnd->pohZBuffer->ulBase;
                pDevWnd->zPitch = ppdev->ulZBufferStride;
                pDevWnd->bValidZBuffer = TRUE;
            } else {
                pDevWnd->bValidZBuffer = FALSE;
            }
        } else {
            pDevWnd->bValidZBuffer = FALSE;
        }
    }
}
#endif

//**************************************************************************
//
// VOID HWFreeResources
//
// Deallocate the zbuffer and/or backbuffer allocated to a window
// Do it for full screen buffers only if there are no more references
// to use them
//
//**************************************************************************
VOID HWFreeResources(MCDWINDOW *pMCDWnd, SURFOBJ *pso)
{
    DEVWND *pDevWnd = (DEVWND *)pMCDWnd->pvUser;
    PDEV *ppdev = (PDEV *)pso->dhpdev;


    // Free Z Buffer
    if (pDevWnd->pohZBuffer) {
        if (ppdev->cZBufferRef) {  // full screen z buffer
            if (!--ppdev->cZBufferRef) { // is also the last RC ref to this
                                         // full screen Z buffer
                MCDBG_PRINT(DBG_MCD,"HWFreeResources: Free global z buffer");
                pohFree(ppdev, ppdev->pohZBuffer);
                ppdev->pohZBuffer = NULL;
            }
        } else { // per-window z buffer
            MCDBG_PRINT(DBG_MCD,"HWFreeResources: Free local z buffer");
            pohFree(ppdev, pDevWnd->pohZBuffer);
            pDevWnd->pohZBuffer = NULL;
        }
    }

    // Free Back Buffer
    if (pDevWnd->pohBackBuffer) {

        if (ppdev->cDoubleBufferRef) {
            //
            // full-screen back buffer
            //
            if (!--ppdev->cDoubleBufferRef) { // is also the last RC ref to this
                                              // full screen back buffer
                MCDBG_PRINT(DBG_MCD,"HWFreeResources: Free global color buffer");
                pohFree(ppdev, ppdev->pohBackBuffer);
                ppdev->pohBackBuffer = NULL;
            }
        } else { // per-window back buffer
            MCDBG_PRINT(DBG_MCD,"HWFreeResources: Free local color buffer");
            pohFree(ppdev, pDevWnd->pohBackBuffer);
            pDevWnd->pohBackBuffer = NULL;
        }
    }

    // Free Texture Memory
    HWFreeAllTextureMemory(ppdev);
}

#define INTERP(newObj,orgObj,clipObj,t,Prop)  \
        newObj->Prop = orgObj->Prop + t * (clipObj->Prop - orgObj->Prop)

//**************************************************************************
//
// VOID FASTCALL __MCDClip2Vert
//
// Calculate the properties of a new vertex when a line is clipped in XY space
//
//**************************************************************************
VOID FASTCALL __MCDClip2Vert(DEVRC *pRc,MCDVERTEX *newV,
                             MCDVERTEX *orgV,MCDVERTEX *clippedV)
{
    MCDFLOAT t;

    t = (orgV->windowCoord.x + (long)pRc->xOffset - __MCDHALF) /
        ( orgV->windowCoord.x - clippedV->windowCoord.x);

    newV->windowCoord.x = - (long)pRc->xOffset + __MCDHALF;

    INTERP(newV,orgV,clippedV,t,windowCoord.y);

    // Adjust R,G,B,A if needed
    if (pRc->privateEnables & __MCDENABLE_COLORED) {

        if (pRc->privateEnables & __MCDENABLE_SMOOTH) {
            INTERP(newV, orgV, clippedV, t, colors[0].r);
            INTERP(newV, orgV, clippedV, t, colors[0].g);
            INTERP(newV, orgV, clippedV, t, colors[0].b);
            }

        if (pRc->privateEnables & __MCDENABLE_BLEND) {
            INTERP(newV, orgV, clippedV, t, colors[0].a);
        }
    }

    // Adjust Z coordinate if needed
    if (pRc->privateEnables & __MCDENABLE_Z) {
        INTERP(newV, orgV, clippedV, t, windowCoord.z);
    }

    // Adjust texture related coordinates if needed
    if (pRc->privateEnables & __MCDENABLE_TEXTURED) {

        INTERP(newV, orgV, clippedV, t, texCoord.x);
        INTERP(newV, orgV, clippedV, t, texCoord.y);

        if (pRc->privateEnables & __MCDENABLE_TEXPERSP) {
            INTERP(newV, orgV, clippedV, t, windowCoord.w);
        }
    }
}


//**************************************************************************
//
// VOID __MCDCalcFogColor
//
// Calculate the final vertex colors when cheap fog is in use
//
//**************************************************************************
VOID __MCDCalcFogColor(DEVRC *pRc, MCDVERTEX *a, MCDCOLOR *pResult,
                       MCDCOLOR *pColor)
{
    MCDFLOAT oneMinusFog;
    MCDCOLOR *pFogColor;

    pFogColor = (MCDCOLOR *)&pRc->MCDState.fogColor;
    oneMinusFog = (MCDFLOAT)1.0 - a->fog;

    if (pRc->privateEnables & __MCDENABLE_GRAY_FOG) {
        MCDFLOAT delta = oneMinusFog * pFogColor->r;

        pResult->r = a->fog * pColor->r + delta;
        pResult->g = a->fog * pColor->g + delta;
        pResult->b = a->fog * pColor->b + delta;
    } else {
       pResult->r = (a->fog * pColor->r) + (oneMinusFog * pFogColor->r);
       pResult->g = (a->fog * pColor->g) + (oneMinusFog * pFogColor->g);
       pResult->b = (a->fog * pColor->b) + (oneMinusFog * pFogColor->b);
    }
}

//**************************************************************************
//
// BOOL __MCDCreateDevWindow
//
// Allocate the per-window DEVWND structure for maintaining per-window info
// such as front/back/z buffer resources:
//
//**************************************************************************
BOOL __MCDCreateDevWindow(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc)
{
    DEVWND *pDevWnd = (DEVWND *)(pMCDSurface->pWnd->pvUser);
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    pDevWnd = pMCDSurface->pWnd->pvUser = (DEVWND *)MCDMemAlloc(sizeof(DEVWND));
    if (!pDevWnd) {
        MCDBG_PRINT(DBG_MCD,"MCDrvBindContext: couldn't allocate DEVWND");
        return FALSE;
    }
    pDevWnd->createFlags  = pMCDRc->createFlags;
    pDevWnd->iPixelFormat = pMCDRc->iPixelFormat;
    pDevWnd->dispUnique   = GetDisplayUniqueness(ppdev);
#ifdef MCD95
    pDevWnd->cColorBits   = ppdev->pDibEngine->deBitsPixel;
#endif

    return TRUE;
}


//**************************************************************************
//
// BOOL __MCDMatchDevWindowPixFmt
//
// Do a sanity-check on the pixel format for this context
//
//**************************************************************************
BOOL __MCDMatchDevWindowPixFmt(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc)
{
    DEVWND *pDevWnd = (DEVWND *)(pMCDSurface->pWnd->pvUser);
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    if (pDevWnd)
        if (pMCDRc->iPixelFormat == pDevWnd->iPixelFormat)
            return TRUE;
        else
            MCDBG_PRINT(DBG_MCD,
                "__MCDMatchDevWindowPixFmt: mismatched pixel formats"
                ", window = %d, context = %d",
                pDevWnd->iPixelFormat, pMCDRc->iPixelFormat);
    else
        MCDBG_PRINT(DBG_MCD,"Null Dev Wnd in __MCDMatchDevWindowPixFmt");

    return FALSE;
}

#endif


