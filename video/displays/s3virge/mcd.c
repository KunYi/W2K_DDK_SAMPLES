/******************************Module*Header*******************************\
* Module Name: mcd.c
*
* Main file for the S3 Virge OpenGL MCD driver.  This file contains
* the entry points needed for an MCD driver.
*
* Copyright (c) 1996,1997 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#if SUPPORT_MCD

#include "hw3d.h"

#include <excpt.h>
#include "mcdhw.h"
#include "mcdmath.h"
#include "mcdutil.h"
#ifdef MCD95
#include "mcd95ini.h"
#endif


// Depth/backbuffer capabilities of supported pixel formats

#define TOTAL_PIXEL_FORMATS (2 * 2)


static BOOL zEnabledFormats[TOTAL_PIXEL_FORMATS]
          = { FALSE, FALSE, TRUE, TRUE };

static BOOL doubleBufferEnabledFormats[TOTAL_PIXEL_FORMATS]
          = { FALSE, TRUE, FALSE, TRUE };

/*****************************Public*Routine********************************\
* BOOL MCDrvInfo
*
* The MCDrvInfo function is used to return MCD-specific driver information
* to the caller.  This is typically done once when the MCD interface is
* initialized.
*
* Parameters    pMCDSurface
*                   A pointer to an MCDSURFACE structure.  Only the pso field
*                   of this structure will be valid for this function.
*               pMCDDriverInfo
*                   A pointer to an MCDDRIVERINFO structure which the driver
*                   must fill.
*
* Return Value      The function returns TRUE if the driver returns driver
*                   information, FALSE otherwise.
*
* Notes  Used to get basic driver info
*        -- Version, Driver ID, drawing batch size, DMA capability reporting
\***************************************************************************/

BOOL WINAPI
MCDrvInfo(MCDSURFACE *pMCDSurface, MCDDRIVERINFO *pMCDDriverInfo)
{
    PDEV    *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;
    DWORD   ReturnedDataLength;
    ULONG   ulMCDBatchSize  = 256000;
    DWORD   KeyNameLength;

    MCDBG_PRINT(DBG_MCD,"MCDrvInfo");

    pMCDDriverInfo->verMajor = MCD_VER_MAJOR;
    pMCDDriverInfo->verMinor = MCD_VER_MINOR;
    pMCDDriverInfo->verDriver = 0x10000;
    strcpy(pMCDDriverInfo->idStr, "S3d Virge");
    pMCDDriverInfo->drvMemFlags = 0;

    //
    // DRIVER WRITERS:
    // MCD's can request any size buffer to be used for MCDCOMMAND storage.
    // Larger buffers may improve performance due to more commands being sent
    // through per MCDrvDraw command but they also incur more paging overhead.
    // You should test it size vs. benchmarks in order to find smallest
    // possible buffer to use without downgrading performance
    //

    KeyNameLength = (wcslen(MCD_BATCH_KEYNAME) + 1 ) * 2;

    EngDeviceIoControl( ppdev->hDriver,
                        IOCTL_VIDEO_S3_GET_REGISTRY_VALUE,
                        MCD_BATCH_KEYNAME,
                        KeyNameLength,
                        &ulMCDBatchSize,
                        sizeof(ULONG),
                        &ReturnedDataLength );

    pMCDDriverInfo->drvBatchMemSizeMax = ulMCDBatchSize;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvInfo");
    return TRUE;
}


/*****************************Public*Routine*********************************\
* LONG MCDrvDescribePixelFormat
*
* This function is used to enumerate the pixel formats supported by an MCD
* implementation.  The pixel formats returned are analogous to those returned
* by the DescribePixelFormat API funnction.
*
* Parameters    pMCDSurface
*                    A pointer to an MCDSURFACE structure.  Only the pso field
*                    of this structure will be valid for this function.
*               iPixelFormat
*                    The pixel format ID for which the driver should return
*                    pixel format information.
*               nBytes
*                    The number of bytes available for the MCDPIXELFORMAT
*                    information given by pMCDPixelFmt. This is for information
*                    only, since the structure size has already been validated
*                    by the caller.
*               pMCDPixelFormat
*                    Pointer to the MCDPIXELFORMAT structure which the driver
*                    fills in, or NULL.  When NULL, the driver should return
*                    the total number of pixel formats supported.
*               flags
*                    This is currently defined to be 0.
*
* Return Value       The function returns the total number of pixel formats
*                    supported if the function succeeds, and 0 otherwise if
*                    pMCDPixelFormat is non-NULL. When pMCDPixelFormat is NULL,
*                    the function should return the total number of pixel
*                    formats supported.  In the later case, returning 0 does
*                    not signal failure.
* Notes
*       iPixelFormat starts at =1
*       To support simulations:
*       -- color bits        (cColorBits)       must be: 8,16,24 or 32
*       -- depth buffer bits (cDepthBufferBits) must be: 16 or 32
*       -- doesn't mean you can't have different physical buffer configurations
\****************************************************************************/

LONG WINAPI
MCDrvDescribePixelFormat(MCDSURFACE *pMCDSurface, LONG iPixelFormat,
                              ULONG nBytes, MCDPIXELFORMAT *pMCDPixelFormat,
                              ULONG flags)
{
    BOOL zEnabled;
    BOOL doubleBufferEnabled;
    DRVPIXELFORMAT *pDrvPixelFormat;
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;


    if (!pMCDPixelFormat) {
        LONG lNum = 0;

        MCDBG_PRINT(DBG_MCD,"MCDrvDescribePixelFormat: num request");

        if (! __HWGetPixelFormat(ppdev))
            lNum = 0; // bitmap format not supported
        else
            lNum = TOTAL_PIXEL_FORMATS;

        MCDBG_PRINT(DBG_MCD,"Exit MCDrvDescribePixelFormat (%ld formats)",
                    lNum);

        return lNum;

    }

    MCDBG_PRINT(DBG_MCD,"MCDrvDescribePixelFormat: iPixelFormat = %u",
                iPixelFormat);

    if (nBytes < sizeof(MCDPIXELFORMAT)) {
        MCDBG_PRINT(DBG_ERR,"MCDrvDescribePixelFormat: pMCDPixelFormat "
                            "buffer too small");
        return 0;
    }

    if (iPixelFormat > TOTAL_PIXEL_FORMATS) {
        MCDBG_PRINT(DBG_ERR,"MCDrvDescribePixelFormat: iPixelFormat = %ld "
                    "out of range", iPixelFormat);
        return 0;
    }

    iPixelFormat--;

    zEnabled            =  zEnabledFormats[iPixelFormat];
    doubleBufferEnabled =  doubleBufferEnabledFormats[iPixelFormat];

    pMCDPixelFormat->nSize = sizeof(MCDPIXELFORMAT);
    pMCDPixelFormat->dwFlags = PFD_SWAP_COPY;
    if (doubleBufferEnabled)
        pMCDPixelFormat->dwFlags |= PFD_DOUBLEBUFFER;
    pMCDPixelFormat->iPixelType = PFD_TYPE_RGBA;

    if (!(pDrvPixelFormat = __HWGetPixelFormat(ppdev)))
        return 0; // bitmap format not supported

    pMCDPixelFormat->cColorBits  = pDrvPixelFormat->cColorBits;
    pMCDPixelFormat->cRedBits    = pDrvPixelFormat->rBits;
    pMCDPixelFormat->cGreenBits  = pDrvPixelFormat->gBits;
    pMCDPixelFormat->cBlueBits   = pDrvPixelFormat->bBits;
    pMCDPixelFormat->cAlphaBits  = pDrvPixelFormat->aBits;
    pMCDPixelFormat->cRedShift   = pDrvPixelFormat->rShift;
    pMCDPixelFormat->cGreenShift = pDrvPixelFormat->gShift;
    pMCDPixelFormat->cBlueShift  = pDrvPixelFormat->bShift;
    pMCDPixelFormat->cAlphaShift = pDrvPixelFormat->aShift;


    if (zEnabled)
    {
        pMCDPixelFormat->cDepthBits       = 16;
        pMCDPixelFormat->cDepthBufferBits = 16;
        pMCDPixelFormat->cDepthShift      = 16;
    }
    else
    {
        pMCDPixelFormat->cDepthBits       = 0;
        pMCDPixelFormat->cDepthBufferBits = 0;
        pMCDPixelFormat->cDepthShift      = 0;
    }


    // S3V does not support stencil; generic will supply a software
    // implementation as necessary.

    pMCDPixelFormat->cStencilBits = 0;


    pMCDPixelFormat->cOverlayPlanes = 0;
    pMCDPixelFormat->cUnderlayPlanes = 0;
    pMCDPixelFormat->dwTransparentColor = 0;


    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDescribePixelFormat");
    return TOTAL_PIXEL_FORMATS;
}

/*****************************Public*Routine********************************\
* BOOL MCDrvDescribeLayerPlane
*
* This function is used to enumerate the layer planes of a particular pixel
* format supported by an MCD implementation.  The layer plane formats returned
* are analogous to those returned by the DescribeLayerPlane API funnction.
*
* The function should return failure if the specified layer plane is not
* supported.
*
* Parameters    pMCDSurface
*                   A pointer to an MCDSURFACE structure.  Only the pso field
*                   of this structure will be valid for this function.
*               iPixelFormat
*                   Specifies the pixel format which supports the layer plane
*                   of interest.
*               iLayerPlane
*                   Specifies the layer plane of interest. The value 0
*                   identifies the main plane.  The value 1 identifies the first
*                   overlay plane over the main plane, 2 the second overlay
*                   plane over the first overlay plane and the main plane, and
*                   so on. The value -1 identifies the first underlay plane
*                   under the main plane, -2 the second underlay plane under the
*                   first underlay plane and the main plane, and so on.
*               nBytes
*                   The number of bytes available for the MCDLAYERPLANE
*                   information pointed to by pMCDLayerPlane.  This is for
*                   information only, since the structure size has already been
*                   validated by the caller.
*               pMCDLayerPlane
*                   Pointer to the MCDLAYERPLANE structure which the driver
*                   fills in.
*
* Return Value      The function returns TRUE if the MCDLAYERPLANE structure was
*                   successfully filled in.  The function will return FALSE if
*                   the return buffer is NULL or the specified layer plane is
*                   not supported.
\***************************************************************************/
BOOL WINAPI
MCDrvDescribeLayerPlane(MCDSURFACE *pMCDSurface,
                             LONG iPixelFormat, LONG iLayerPlane,
                             ULONG nBytes, MCDLAYERPLANE *pMCDLayerPlane,
                             ULONG flags)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvDescribeLayerPlane");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDescribeLayerPlane");
    return FALSE;
}

/*****************************Public*Routine********************************\
* LONG MCDrvSetLayerPalette
*
* This function is used to set the physical palette entries for a specified
* layer plane.  The function should return failure if the specified layer
* plane is not supported.
*
* Parameters    pMCDSurface
*                   A pointer to an MCDSURFACE structure.  Only the pso field
*                   of this structure will be valid for this function.
*               iLayerPlane
*                   Specifies the layer plane of interest. The value 0
*                   identifies the main plane and is invalid for this function.
*                   The value 1 identifies the first overlay plane over the
*                   main plane, 2 the second overlay plane over the first
*                   overlay plane and the main plane, and so on.  The value -1
*                   identifies the first underlay plane under the main plane,
*                   -2 the second underlay plane under the first underlay plane
*                   and the main plane, and so on.
*
*                   If the layer plane pixel type is LPD_TYPE_COLORINDEX, the
*                   palette entries are passed via pPaletteEntries.  If the
*                   layer plane pixel type is LPD_TYPE_RGBA, the palette is not
*                   settable so pPaletteEntries can be ignored (the palette
*                   entries are implied by cRedBits,cRedShift,cGreenBits,etc.).
*               bRealize
*                   Indicates whether the palette entries passed in should be
*                   realized to the physical palette.  If bRealize is TRUE, the
*                   palette entries are mapped one-to-one into the physical
*                   palette. If bRealize is FALSE, the palette entries for the
*                   layer plane are no longer needed and may be released for
*                   use by another surface.
*              cEntries
*                   Specifies the number of palette entries in the buffer
*                   pointed to by pPaletteEntries.
*              pPaletteEntries
*                   Pointer to the buffer containing the palette entries to be
*                   set.
*
* Return Value      If bRealize is TRUE, the function returns the number of
*                   palette entries set in the physical palette.  If bRealize
*                   is FALSE, the function returns non-zero if the palette
*                   is successfully unrealized.  The function returns 0 if
*                   an error occurs.
*
* Comments          When a window's layer palette is realized, its palette
*                   entries are always mapped one-to-one into the physical
*                   palette.  Unlike GDI logical palettes, the system makes
*                   no attempt to map other window's layer palettes to the
*                   current physical palette.
*
*                   This function essentially combines the functionality of
*                   the wglSetLayerPaletteEntries and wglRealizeLayerPalette
*                   APIs, but without the need for the driver to maintain a
*                   logical palette for each layer plane.  Assuming that well
*                   behaved applications are using the layer planes,
*                   wglRealizeLayerPalette (and therefore MCDrvSetLayerPalette)
*                   will be called whenever a window becomes the foreground
*                   window.
\***************************************************************************/
LONG WINAPI
MCDrvSetLayerPalette(MCDSURFACE *pMCDSurface, LONG iLayerPlane,
                          BOOL bRealize, LONG cEntries, COLORREF *pcr)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvSetLayerPalette");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvSetLayerPalette");
    return 0;
}

/*****************************Public*Routine********************************\
* HDEV  MCDrvGetHdev
*
* This function is used to query for the HDEV available for the specified
* surface.  The MCDrvGetHdev  function is used with Windows NT only.
*
* Parameters    pMCDSurface
*                   A pointer to an MCDSURFACE structure.  Only the pso field
*                   of this structure will be valid for this function.
*
* Return Value      The functions returns the HDEV for the specified surface.
* Comments      Used by MCD object management functions
\***************************************************************************/
HDEV WINAPI
MCDrvGetHdev(MCDSURFACE *pMCDSurface)
{
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    MCDBG_PRINT(DBG_MCD,"MCDrvGetHdev");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvGetHdev");
    return ppdev->hdevEng;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvCreateContext
*
* This function is called to create a new rendering context.  The driver should
* allocate its per-context data structures, and bind the new rendering context
* to the supplied surface.  If the surface is uninitialized (which is the case
* when no other context has yet been bound to the surface)  the driver should
* allocate its per-surface data for the specified surface.  Previously
* uninitialized surfaces will be bound to the pixel format of the rendering
* context.  The driver should fail context creation if the pixel format of a
* previously initialized surface does not match the pixel format of the
* rendering context. The driver should also fail context creation if the
* iLayerPlane in pMCDRc specifies an unsupported overlay or underlay plane.
*
* The driver may also perform any initialization needed.  However, it should
* not allocate any per-surface resources (such as a back buffer or depth buffer)
* since this is handled by other functions.
*
* Parameters   pMCDSurface
*                  A pointer to an MCDSURFACE structure.  For windowed surfaces
*                  (specified by MCDSURFACE_HWND in the surfaceFlags of
*                  pMCDSurface), the driver can use information cached via the
*                  pvUser field of pMCDSurface.pWnd to determine whether or not
*                  the window corresponding to pMCDSurface has already been
*                  bound to a rendering context with a previous
*                  MCDrvCreateContext or MCDrvBindContext call.  If the window
*                  has been previously bound, then the pixel format of the
*                  previous binding and the pixel format specified in this
*                  function (specified in iPixelFormat of pMCDRc) must match.
*                  Otherwise, the driver must fail context creation.
*              pMCDRc
*                  Points to an MCDRC which specifies the type of context to
*                  create.
*              pRcInfo
*                  Points to an MCDRCINFO which supplies information about the
*                  context (such as color and z scaling) to be created.  This
*                  information may be modified by the driver to allow driver-
*                  specified values to be used instead of the ones supplied to
*                  the function.
*
* Return Value     The function returns TRUE if  a context was created, FALSE
*                  otherwise.
*
* Notes
*      Allows driver to manage rendering context
*      -- allocates driver-specific space for context
*      -- Store driver's context (rendering state, etc.) in MCDRC
*      Will create driver-specific data for the surface
*      -- allocate per surface data
*      -- bind pixel format in MCDRC to surface
*      -- if surface already exists, just use it
*      Must have a context for drawing
*               (drawing function does not get a copy of current state)
*      MCDRCINFO: used to get scaling info, etc.
*      Driver can also set scaling (care must be taken for simulations!)
\***************************************************************************/
ULONG WINAPI
MCDrvCreateContext(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc,
                         MCDRCINFO *pRcInfo)
{
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;
    MCDWINDOW *pMCDWnd = pMCDSurface->pWnd;
    DEVRC *pRc, *pOldDevRC;
    DRVPIXELFORMAT *pDrvPixelFormat;

    MCDBG_PRINT(DBG_MCD,"MCDrvCreateContext");

    // We only support window surfaces:

    if (!(pMCDSurface->surfaceFlags & MCDSURFACE_HWND))
        return FALSE;

    MCDBG_PRINT(DBG_MCD,"pMCDRC->iPixelFormat: %u",pMCDRc->iPixelFormat);

    if ((pMCDRc->iPixelFormat > TOTAL_PIXEL_FORMATS) ||
        (pMCDRc->iPixelFormat < 1)) {
        MCDBG_PRINT(DBG_MCD,"MCDrvCreateContext: bad pixel format");
        return FALSE;
    }

    // We don't support overlay planes:

    if (pMCDRc->iLayerPlane) {
        MCDBG_PRINT(DBG_ERR,"MCDrvCreateContext: layer planes not supported");
        return FALSE;
    }


    // Allocate 4 bytes extra, since there is no guarantee that we will
    // get a Qword-aligned pointer to DEVRC.
    //
    pOldDevRC = (DEVRC *)MCDMemAlloc(sizeof(DEVRC)+4);

    if (!pOldDevRC) {
        MCDBG_PRINT(DBG_ERR,"MCDrvCreateContext: couldn't allocate DEVRC");
        return FALSE;
    }

    // Align pointer to qword boundary.
    pRc = (DEVRC *)((((ULONG_PTR)pOldDevRC) + 7) & ~0x7);

    pMCDRc->pvUser = pRc;
    MCDBG_PRINT(DBG_MCD,"Creating/assigning pvUser = %x",pMCDRc->pvUser); 
    pRc->pOldDevRC = pOldDevRC;
    pRc->ppdev     = ppdev;

    // Same as in MCDrvDescribePixelFormat
    pRc->zBufEnabled    = zEnabledFormats[pMCDRc->iPixelFormat - 1];
    pRc->backBufEnabled = doubleBufferEnabledFormats[pMCDRc->iPixelFormat - 1];

    if (!(pDrvPixelFormat = __HWGetPixelFormat(ppdev)))
        goto FailCntxt; // bitmap format not supported

    // On this hw, 16bpp requires 2 bytes and 24bpp requires 3 bytes of storage
    pRc->hwBpp = pDrvPixelFormat->cColorBits >> 3;

    pRc->pixelFormat = *pDrvPixelFormat;

    // If we're not yet tracking this window, allocate the per-window DEVWND
    // structure for maintaining per-window info such as front/back/z buffer
    // resources:

    if (!pMCDWnd->pvUser) {
        if (!__MCDCreateDevWindow(pMCDSurface, pMCDRc))
            goto FailCntxt;
    } else {
        // We already have a per-window DEVWND structure tracking this window.
        // In this case, do a sanity-check on the pixel format for this
        // context, since a window's pixel format can not change once it has
        // set (by the first context bound to the window).  So, if the pixel
        // format for the incoming context doesn't match the current pixel
        // format for the window, we have to fail context creation:

        if (!__MCDMatchDevWindowPixFmt(pMCDSurface, pMCDRc))
            goto FailCntxt;
    }

    pRc->pEnumClip = pMCDSurface->pWnd->pClip;

    // Set up our color scale values so that color components are
    // normalized to 0..7fff

    // We also need to make sure we don't fault due to bad FL data as well...

    try {

    if (pRcInfo->redScale != (MCDFLOAT)0.0)
        pRc->rScale = (MCDFLOAT)(0x8000) / pRcInfo->redScale;
    else
        pRc->rScale = (MCDFLOAT)0.0;

    if (pRcInfo->greenScale != (MCDFLOAT)0.0)
        pRc->gScale = (MCDFLOAT)(0x8000) / pRcInfo->greenScale;
    else
        pRc->gScale = (MCDFLOAT)0.0;

    if (pRcInfo->blueScale != (MCDFLOAT)0.0)
        pRc->bScale = (MCDFLOAT)(0x8000) / pRcInfo->blueScale;
    else
        pRc->bScale = (MCDFLOAT)0.0;

    // Normalize alpha to 0..7fff

    if (pRcInfo->alphaScale != (MCDFLOAT)0.0)
        pRc->aScale = (MCDFLOAT)(0x8000) / pRcInfo->alphaScale;
    else
        pRc->aScale = (MCDFLOAT)0.0;

    } except (EXCEPTION_EXECUTE_HANDLER) {

        MCDBG_PRINT(DBG_ERR,"!!Exception in MCDrvCreateContext!!");
        goto FailCntxt;
    }

    pRc->zScale = (MCDFLOAT)32767.0;

    pRc->xScale = (MCDFLOAT)(0x1FFFF);

    pRc->pickNeeded = TRUE;         // We'll definitely need to re-pick
                                    // our rendering functions
    pRc->bRGBMode = TRUE;           // We only support RGB mode
                                    // (used to pick clipping functions)

    // Initialize the pColor pointer in the clip buffer:

    {
        MCDVERTEX *pv;
        ULONG i, maxVi;

        for (i = 0, pv = &pRc->clipTemp[0],
            maxVi = sizeof(pRc->clipTemp) / sizeof(MCDVERTEX);
            i < maxVi; i++, pv++) {
                pv->pColor = &pv->colors[__MCD_FRONTFACE];
        }
    }

    // Set up those rendering functions which are state-invariant:

    pRc->clipLine          = __MCDClipLine;
    pRc->clipTri           = __MCDClipTriangle;
    pRc->clipPoly          = __MCDClipPolygon;
    pRc->doClippedPoly     = __MCDDoClippedPolygon;

    pRc->beginPointDrawing = __MCDPointBegin;

    pRc->beginLineDrawing  = __MCDLineBegin;
    pRc->endLineDrawing    = __MCDLineEnd;

    // Set up in order to later calculate appropriate coordinate offsets

    pRc->viewportXAdjust   = pRcInfo->viewportXAdjust;
    pRc->viewportYAdjust   = pRcInfo->viewportYAdjust;

#if TEST_REQ_FLAGS
    // Request to change the range in which actual property values
    // are scaled and offset before being sent to the MCD driver
    pRcInfo->requestFlags = MCDRCINFO_NOVIEWPORTADJUST |
                            MCDRCINFO_Y_LOWER_LEFT |
                            MCDRCINFO_DEVCOLORSCALE |
                            MCDRCINFO_DEVZSCALE;

    pRcInfo->redScale   = (MCDFLOAT)1.0;
    pRcInfo->greenScale = (MCDFLOAT)1.0;
    pRcInfo->blueScale  = (MCDFLOAT)1.0;
    pRcInfo->alphaScale = (MCDFLOAT)1.0;

    pRcInfo->zScale     = 0.99991;
#endif


    MCDBG_PRINT(DBG_MCD,"Exit MCDrvCreateContext");
    return TRUE;

FailCntxt:
    MCDBG_PRINT(DBG_MCD,"Freeing pvUser = %x",pMCDRc->pvUser); 
    MCDMemFree((VOID *)pOldDevRC); 
    pMCDRc->pvUser = NULL;
    return FALSE;
}


/*****************************Public*Routine********************************\
* ULONG MCDrvBindContext
*
* This function will bind the specified context to an MCD surface.
*
* If the specified surface has no previous rendering context binding, the
* driver should create new driver-specific surface data.  The driver may use
* the  pvUser field in pMCDSurface->pWnd to store the driver's surface
* information. The new surface will inherit the rendering context's pixel
* format specified in pMCDRc, and should also attempt to allocate the buffers
* required for the pixel format. The driver may return successfully from this
* function even in the case where the buffers required to support the
* specified pixel format could not be allocated.
*
* If the specified surface has a previous binding to a rendering context, the
* driver should verify that the rendering context and surface pixel formats
* match, and fail the call otherwise.
*
* Parameters  pMCDSurface
*                 Points to an MCDSURFACE structure specifying the surface to
*                 which the rendering context specified by pMCDRc will be bound.
*             pMCDRc
*                 Points to an MCDRC structure specifying the rendering context
*                 to bind to the surface specified by pMCDSurface.
*
* Return Value    The function returns TRUE if the bind operation succeeded,
*                 FALSE otherwise.
* Comments    Used to draw with the same context on different window(or surface)
\***************************************************************************/
ULONG WINAPI
MCDrvBindContext(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc)
{
    DEVWND *pDevWnd = (DEVWND *)(pMCDSurface->pWnd->pvUser);
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    MCDBG_PRINT(DBG_MCD,"MCDrvBindContext");

    // If this is a new binding create the per-window structure and
    // set the pixel format:

    if (!pDevWnd) {
        if (!__MCDCreateDevWindow(pMCDSurface,pMCDRc))
            return FALSE;
    } else {
        if (!__MCDMatchDevWindowPixFmt(pMCDSurface,pMCDRc))
            return FALSE;
    }

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvBindContext");
    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvDeleteContext
*
* The context-deletion function is called to allow the driver to perform any
* required cleanup such as freeing memory allocated for the specified context.
*
* Parameters    pMCDRc
*                    Pointer to an MCDCONTEXT structure specifying the context
*                    to be deleted.
*               dhpdev
*                    Specifies the device corresponding to the context.
*
* Return Value       The function returns TRUE if  the context was deleted,
*                    FALSE otherwise.
* Comments      Driver must not free surface resources (drawing buffers,etc.)
*               -- resources freed when window is destroyed
\***************************************************************************/
ULONG WINAPI
MCDrvDeleteContext(MCDRC *pMCDRc, DHPDEV dhpdev)
{
    PDEV *ppdev;
    DEVRC *pOldDevRC;

#ifdef MCD95
    //
    // On Win95, dhpdev is bogus.  Replace with gppdev.
    //

    dhpdev = (DHPDEV) gppdev;
#endif

    MCDBG_PRINT(DBG_MCD,"MCDrvDeleteContext");

    if (pMCDRc->pvUser)
    {
        pOldDevRC = ((DEVRC*)(pMCDRc->pvUser))->pOldDevRC;
        MCDMemFree((UCHAR *)(pOldDevRC)); 
        MCDBG_PRINT(DBG_MCD,"Freeing pvUser = %x",pMCDRc->pvUser); 
        pMCDRc->pvUser   = NULL;
    }
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDeleteContext");
    return (ULONG)TRUE;
}


/*****************************Public*Routine********************************\
* ULONG MCDrvAllocBuffers
*
* This function is called to allocate the buffers required for the specified
* surface. All of the buffers required for the surface's pixel format must be
* allocated for the function to succeed.  This function may be called with
* surfaces which already have buffers allocated, so care must be taken to
* avoid redundant allocation of device resources.
*
* Parameters   pMCDSurface
*                  Points to an MCDSURFACE structure specifying the surface
*                  for which buffer allocation is being requested.
*              pRc
*                  Points to an MCDRC structure specifying the rendering
*                  context for which buffer allocation is being requested.
*
* Return Value     The function returns TRUE if the allocation succeeded or
*                  if the buffers for the surface had previously been allocated,
*                  FALSE otherwise.
* Comments      Driver must allocate drawing buffers once for the window
*               -- This call is also used as a query for existence
\***************************************************************************/
ULONG WINAPI
MCDrvAllocBuffers(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc)
{
    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;
    MCDWINDOW *pMCDWnd = pMCDSurface->pWnd;
    DEVWND *pDevWnd  = (DEVWND *)(pMCDSurface->pWnd->pvUser);
    BOOL bHaveDepth  = (pDevWnd->pohZBuffer != NULL);
    BOOL bHaveBack   = (pDevWnd->pohBackBuffer != NULL);
#ifdef MCD95
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;
#endif

    MCDBG_PRINT(DBG_MCD,"MCDrvAllocBuffers");

    // Reject the call if we've already done an allocation for this window:

    if ((bHaveDepth || bHaveBack) &&
        (((DEVWND *)pMCDWnd->pvUser)->dispUnique ==
        GetDisplayUniqueness((PDEV *)pMCDSurface->pso->dhpdev))) {

#ifdef MCD95
        BOOL bFreedRes = FALSE;

        // If either surface is lost, free the resources.

        if (bHaveDepth) {
            if (!pohValid(ppdev, pDevWnd->pohZBuffer)) {
                HWFreeResources(pMCDWnd, pMCDSurface->pso);
                bFreedRes = TRUE;
            }
        }

        if ((!bFreedRes) && bHaveBack) {
            if (!pohValid(ppdev, pDevWnd->pohBackBuffer)) {
                HWFreeResources(pMCDWnd, pMCDSurface->pso);
                bFreedRes = TRUE;
            }
        }

        // If resources not freed, buffers still valid.  Return TRUE
        // if the required buffers exist.  Otherwise, fall into the
        // allocation case below.

        if (!bFreedRes) {
            return  ((bHaveDepth == pRc->zBufEnabled) &&
                     (bHaveBack == pRc->backBufEnabled));
        }
#else
        // Return TRUE if the right buffers are already allocated
        return  ((bHaveDepth ==  pRc->zBufEnabled) &&
                 (bHaveBack  == pRc->backBufEnabled));
#endif
    }

    // Update the display resolution uniqueness for this window:

    ((DEVWND *)pMCDWnd->pvUser)->dispUnique =
            GetDisplayUniqueness((PDEV *)pMCDSurface->pso->dhpdev);

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvAllocBuffers");

#ifdef MCD95
    // Need to ensure PDEV is current when doing allocation
    // because mode changes (which change PDEV) cause reallocation.

    if (!S3FinishPdevInit(NULL, ppdev)) {
        MCDBG_PRINT(DBG_ERR, "MCDrvAllocBuffers: S3FinishPdevInit failed");
        return FALSE;
    }
#endif

    // Allocate the buffers
    return (ULONG)HWAllocResources(pMCDSurface->pWnd, pMCDSurface->pso,
                                   pRc->zBufEnabled, pRc->backBufEnabled);
}

/*****************************Public*Routine********************************\
* ULONG MCDrvGetBuffers
*
* This function returns information about the device's front, back, and depth
* buffers.  This information is used to gain access to these buffers to perform
* software simulation of those drawing functions which the driver does not
* support.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the surface for
*                   which buffer information is being requested.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context for the specified surface.
*               pMCDBuffers
*                   Points to an MCDBUFFERS structure that the driver fills with
*                   information about the state and accessibility of the front,
*                   back, and depth buffers.
*
* Return Value      Returns TRUE if the driver returns valid buffer information
*                   in pMCDBuffers, FALSE otherwise.
* Comments      Used to return information about the drawing buffers
*               -- (frame buffer accesability, info for simulations)
*               If drawing buffers don't exist, return FALSE
\***************************************************************************/

ULONG WINAPI
MCDrvGetBuffers(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc,
                      MCDBUFFERS *pMCDBuffers)
{
#ifndef MCD95
    DEVRC     *pRc     = (DEVRC             *)pMCDRc->pvUser;
#endif
    MCDWINDOW *pMCDWnd = pMCDSurface->pWnd;
    DEVWND    *pDevWnd = (DEVWND            *)(pMCDSurface->pWnd->pvUser);
    PDEV      *ppdev   = (PDEV              *)pMCDSurface->pso->dhpdev;
    LONG       lOffset = pMCDWnd->clipBoundsRect.left & 7;

    MCDBG_PRINT(DBG_MCD,"MCDrvGetBuffers");

#ifdef MCD95
    MCD95_CHECK_GETBUFFERS_VALID(pMCDSurface);
#else
    MCD_CHECK_BUFFERS_VALID(pMCDSurface, pRc, FALSE);
#endif

#ifdef MCD95
    //
    // Pixel format check requested.
    //

    if (pMCDBuffers->mcdRequestFlags & MCDBUF_REQ_PIXELFORMATCHECK)
    {
        // WARNING TO IHVs:
        // This comparison is checking the physical bits
        // in the pdev against the pixelformat color bits.
        // This comparison works for S3 ViRGE, but may not
        // work on your implementation (for example, if you
        // supply both 16bpp 555 and 565 modes, this check
        // is an insufficient test for pixelformat
        // compatibility).  Modify as appropriate.

        if (pDevWnd->cColorBits != ppdev->pDibEngine->deBitsPixel)
        {
            return FALSE;
        }
    }

    //
    // DirectDraw information requested.
    //

    if (pMCDBuffers->mcdRequestFlags & MCDBUF_REQ_DDRAWINFO)
    {
        pMCDBuffers->mcdFrontBuf.pMcdDD = (MCD_DDRAWINFO *) NULL;

        if (pDevWnd->pohBackBuffer)
        {
            pMCDBuffers->mcdBackBuf.pMcdDD = &pDevWnd->pohBackBuffer->mcdDD;
        }
        else
        {
            pMCDBuffers->mcdBackBuf.pMcdDD = (MCD_DDRAWINFO *) NULL;
        }

        if (pDevWnd->pohZBuffer)
        {
            pMCDBuffers->mcdDepthBuf.pMcdDD = &pDevWnd->pohZBuffer->mcdDD;
        }
        else
        {
            pMCDBuffers->mcdDepthBuf.pMcdDD = (MCD_DDRAWINFO *) NULL;
        }
    }

    //
    // Buffer information requested.
    //

    if (pMCDBuffers->mcdRequestFlags & MCDBUF_REQ_MCDBUFINFO)
#endif
    {
#ifdef MCD95
        HWValidateResources(pMCDWnd, pMCDSurface->pso);
#endif

    #ifdef MCD95
        // For MCD on Win95, return pointer to screen:

        pMCDBuffers->pvFrameBuf = (VOID *) ppdev->pjScreen;
    #endif


        pMCDBuffers->mcdFrontBuf.bufFlags  = MCDBUF_ENABLED;
        pMCDBuffers->mcdFrontBuf.bufStride = ppdev->lDelta;
        pMCDBuffers->mcdFrontBuf.bufOffset =
                (pMCDWnd->clientRect.top  * ppdev->lDelta) +
                (pMCDWnd->clientRect.left * ppdev->cjPelSize);

        if (pDevWnd->bValidBackBuffer)
        {
            pMCDBuffers->mcdBackBuf.bufFlags = MCDBUF_ENABLED;
            if ((ppdev->cDoubleBufferRef == 1) || (pMCDWnd->pClip->c == 1))
            {
                pMCDBuffers->mcdBackBuf.bufFlags |= MCDBUF_NOCLIP;
            }

            if ( ppdev->cDoubleBufferRef )
            {
                pMCDBuffers->mcdBackBuf.bufOffset =
                    (pMCDWnd->clientRect.top * pDevWnd->ulBackBufferStride) +
                    (pMCDWnd->clientRect.left * ppdev->cjPelSize) +
                    pDevWnd->backBufferOffset;
            }
            else
            {
                pMCDBuffers->mcdBackBuf.bufOffset = pDevWnd->backBufferOffset +
                                                    lOffset * ppdev->cjPelSize;
            }

            pMCDBuffers->mcdBackBuf.bufStride = pDevWnd->ulBackBufferStride;
        }
        else
        {
            pMCDBuffers->mcdBackBuf.bufFlags = 0;
        }

        if (pDevWnd->bValidZBuffer)
        {
            pMCDBuffers->mcdDepthBuf.bufFlags = MCDBUF_ENABLED;

            // There is only one rectangle to draw into or our full-screen
            // z-buffer allocations worked, or the z-buffer existed already
            if ((ppdev->cZBufferRef == 1) || (pMCDWnd->pClip->c == 1))
            {
                pMCDBuffers->mcdDepthBuf.bufFlags |= MCDBUF_NOCLIP;
            }

            if (ppdev->cZBufferRef)
            {
                //
                // for a full screen zbuffer
                //

                pMCDBuffers->mcdDepthBuf.bufOffset =
                    ((pMCDWnd->clientRect.top * ppdev->cxMemory) +
                    pMCDWnd->clientRect.left) * 2 +
                    pDevWnd->zBufferOffset;
            }
            else
            {
                //
                // for a per window zbuffer
                //

                  pMCDBuffers->mcdDepthBuf.bufOffset = pDevWnd->zBufferOffset +
                                                       lOffset * 2;
            }

            pMCDBuffers->mcdDepthBuf.bufStride = pDevWnd->zPitch;
        }
        else
        {
            pMCDBuffers->mcdDepthBuf.bufFlags = 0;
        }
    }

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvGetBuffers");
    return (ULONG)TRUE;

}

/*****************************Public*Routine********************************\
* ULONG MCDrvCreateMem
*
* This function allows the driver to allocate its own memory for use by the MCD.
* Since a memory area is provided by default, the driver does not need to take
* any action with this function unless the driver needs to allocate special
* memory regions to support DMA, or other device-specific features.
*
* Parameters    pMCDSurface
*                    A pointer to an MCDSURFACE structure.  Only the pso field
*                    of this structure will be valid for this function.
*               pMCDMem
*                    A pointer to an MCDMEM structure.  The driver may replace
*                    this field to allow the driver to specify a memory region
*                    different from the one specified by default.  In this case,
*                    the default memory region will be freed, and the one
*                    specified by the driver will be used.  This allows the
*                    driver to support special types of memory required to
*                    support DMA.
*
* Return Value       The function returns TRUE if the driver allocates its own
*                    memory region successfully or choses to use the default
*                    memory area, FALSE otherwise.
* Notes
*   Allows driver to specify its own memory blocks for data transfer
*   High-end DMA (makes sense only of hw that can process drawing buffer
*     directly)
\***************************************************************************/

ULONG WINAPI
MCDrvCreateMem(MCDSURFACE *pMCDSurface, MCDMEM *pMCDMem)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvCreateMem");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvCreateMem");
    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvDeleteMem
*
* This function allows the driver to free any memory allocated with
* MCDrvCreateMem.
*
* Parameters    pMCDMem
*                   A pointer to an MCDMEM structure.
*               dhpdev
*                   Specifies the handle to the display device.
*
* Return Value      The function returns FALSE if the driver failed to delete
*                   the specified memory region which the driver had previously
*                   allocated with MCDrvCreateMem, TRUE otherwise.
* Notes
*   Allows driver to specify its own memory blocks for data transfer
*   High-end DMA (makes sense only of hw that can process drawing buffer
*      directly)
\***************************************************************************/

ULONG WINAPI
MCDrvDeleteMem(MCDMEM *pMCDMem, DHPDEV dhpdev)
{
#ifdef MCD95
    //
    // On Win95, dhpdev is bogus.  Replace with gppdev.
    //

    dhpdev = (DHPDEV) gppdev;
#endif

    MCDBG_PRINT(DBG_MCD,"MCDrvDeleteMem");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDeleteMem");
    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG_PTR MCDrvDraw
*
* The MCDrvDraw function is called to draw all of the OpenGL primitives
* available through the MCD interface.  The primitives are specified as a
* chain of MCDCOMMAND structures separated by MCDVERTEX structures which
* specify the vertices comprising the primitives.  The driver must process
* the drawing commands sequentially until either the entire buffer has been
* processed, or the driver choses to allow the rest of the buffer to be
* processed via software simulation. In the later case, the driver will not
* be called to process any of the remaining primitives in the command buffer.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the surface on
*                   which drawing will be done.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context to be used for drawing.
*               pMCDMem
*                   Points to an MCDMEM structure specifying the memory region
*                   containing the sequence of drawing commands to process.
*               pStart
*                   Specifies the location of the first MCDCOMMAND structure to
*                   process.
*               pEnd
*                   Specifies the location of the end of the last drawing
*                   command to process.
*
* Return Value      The function returns 0 if the entire batch of drawing
*                   commands was processed by the driver.  Otherwise, the
*                   function should return the pointer value to the first
*                   primitive (the first MCDCOMMAND pointer) in the drawing
*                   batch which the driver could not process.
\***************************************************************************/

ULONG_PTR WINAPI
MCDrvDraw(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc, MCDMEM *prxExecMem,
                UCHAR *pStart, UCHAR *pEnd)
{

    MCDCOMMAND *pCmd            = (MCDCOMMAND *)pStart;
    MCDCOMMAND *pCmdNext;
    DEVRC      *pRc             = (DEVRC *)pMCDRc->pvUser;
    DEVWND     *pDevWnd         = (DEVWND *)(pMCDSurface->pWnd->pvUser);
    ULONG       lastTextureKey  = (ULONG)0;

    CHOP_ROUND_ON();

    MCDBG_PRINT(DBG_MCD,"MCDrvDraw:Drawing a batch");

#if DBG_3D_NO_DRAW
    CHOP_ROUND_OFF();

    return (ULONG)0;
#endif


    // Make sure we have both a valid RC and window structure:

    if (!pRc || !pDevWnd)
    {
       MCDBG_PRINT(DBG_ERR,"MCDrvDraw: invalid RC or window structure ptrs");
       goto DrawExit;
    }

    pRc->ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

#if DBG_FAIL_ALL_DRAWING
    MCDBG_PRINT(DBG_ERR,"MCDrvDraw is compiled to fail all drawing");
    goto DrawExit;
#endif

    //
    // If the resolution has changed and we have not yet updated our
    // buffers, fail the call gracefully since the client won't be
    // able to perform any software simulations at this point either.
    // This applies to any of the other drawing functions as well (such
    // as spans and clears).
    //

    if (pDevWnd->dispUnique != GetDisplayUniqueness(pRc->ppdev) ||
        !__HWGetPixelFormat( pRc->ppdev ))
    {
        MCDBG_PRINT(DBG_MCD,"MCDrvDraw: invalid (changed) resolution");

        CHOP_ROUND_OFF();
        return 0;
    }

    if ((pRc->zBufEnabled && !pDevWnd->bValidZBuffer) ||
        (pRc->backBufEnabled && !pDevWnd->bValidBackBuffer)) {

        MCDBG_PRINT(DBG_ERR,"MCDrvDraw: invalid buffers");
        goto DrawExit;
    }

    // re-pick the rendering functions if we've have a state change:

    if (pRc->pickNeeded) {
        __MCDPickRenderingFuncs(pRc, pDevWnd);
        __MCDPickClipFuncs(pRc);
        pRc->pickNeeded = FALSE;
    }

    // If we're completely clipped, return success:

    pRc->pEnumClip = pMCDSurface->pWnd->pClip;

    if (!pRc->pEnumClip->c) {
        CHOP_ROUND_OFF();
        MCDBG_PRINT(DBG_MCD,"MCDrvDraw completely clipped out");
        return 0;
    }

    // return here if we can't draw any primitives:

    if (pRc->allPrimFail) {
        MCDBG_PRINT(DBG_WRN,"MCDrvDraw cannot draw any primitives "
                            "(incompatible rendering state)");
        goto DrawExit;
    }

    // Set these up in the device's RC so we can just pass a single pointer
    // to do everything:

    pRc->pMCDSurface = pMCDSurface;
    pRc->pMCDRc = pMCDRc;

    pRc->xOffset = pMCDSurface->pWnd->clientRect.left -
                   pRc->viewportXAdjust;

    pRc->yOffset = pMCDSurface->pWnd->clientRect.top -
                   pRc->viewportYAdjust;

    pRc->pMemMin = pStart;
    pRc->pvProvoking = (MCDVERTEX *)pStart;     // bulletproofing
    pRc->pMemMax = pEnd - sizeof(MCDVERTEX);

    // warm up the hardware for drawing primitives:


    HW_INIT_DRAWING_STATE(pMCDSurface, pMCDSurface->pWnd, pRc);
    HW_INIT_PRIMITIVE_STATE(pMCDSurface, pRc);

    // If we have a single clipping rectangle, set it up in the hardware once
    // for this batch:

    if (pRc->pEnumClip->c == 1) {
        (*((pRc->ppdev)->hwLineSetupClipRect))(pRc, &pRc->pEnumClip->arcl[0]);
        (*((pRc->ppdev)->hwTriSetupClipRect))(pRc, &pRc->pEnumClip->arcl[0]);
    }

    // Now, loop through the commands and process the batch:


    try {
        while (pCmd && (UCHAR *)pCmd < pEnd) {

            volatile ULONG command = pCmd->command;

            // Make sure we can read at least the command header:

            if ((pEnd - (UCHAR *)pCmd) < sizeof(MCDCOMMAND)) {
                MCDBG_PRINT(DBG_ERR,"MCDrvDraw: command buffer overrun");
                goto DrawExit;
            }

            if (command <= GL_POLYGON) {

                if (pRc->MCDState.textureEnabled &&
                    (lastTextureKey != pCmd->textureKey)) {
#ifdef MCD95
                    if (lastTextureKey) {
                        // Must finish drawing before unlocking texture.

                        HWUnlockTexKey(pRc,lastTextureKey);
                        lastTextureKey = 0;
                    }
#endif

                    // MCD95 Note: HWUpdateTexState will lock the textureKey.

                    if (!HWUpdateTexState(pRc,pCmd->textureKey)) {
                        // texture state mix cannot be handled by HW
                        MCDBG_PRINT(DBG_ERR,"MCDrvDraw: HWUpdateTexState failed");
                        goto DrawExit;
                    }
                    lastTextureKey = pCmd->textureKey;
                }

                if (pCmd->flags & MCDCOMMAND_RENDER_PRIMITIVE)
                    pCmdNext = (*pRc->primFunc[command])(pRc, pCmd);
                else
                    pCmdNext = pCmd->pNextCmd;

                if (pRc->MCDState.textureEnabled)
                    pRc->ppdev->lastRenderedTexKey = lastTextureKey;
                else
                    pRc->ppdev->lastRenderedTexKey = 0;

                if (pCmdNext == pCmd) {
                    MCDBG_PRINT(DBG_WRN,"Command (%i) failed",(long)command);
                    goto DrawExit;           // primitive failed
                }
                if (!(pCmd = pCmdNext)) {    // we're done with the batch
                    CHOP_ROUND_OFF();
#ifdef MCD95
                    if (lastTextureKey) {
                        HWUnlockTexKey(pRc, lastTextureKey);
                        lastTextureKey = 0;
                    }
#endif
                    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDraw: succesful");
                    return 0;
                }
            }
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {

        CHOP_ROUND_OFF();
#ifdef MCD95
        if (lastTextureKey) {
            HWUnlockTexKey(pRc,lastTextureKey);
            lastTextureKey = 0;
        }
#endif
        MCDBG_PRINT(DBG_ERR,"Failing MCDrvDraw batch due to an exception");
        return (ULONG_PTR)pCmd;
    }

DrawExit:
    CHOP_ROUND_OFF();

#ifdef MCD95
        if (lastTextureKey) {
            HWUnlockTexKey(pRc, lastTextureKey);
            lastTextureKey = 0;
        }
#endif
    MCDBG_PRINT(DBG_WRN,"..Failing MCDrvDraw");
    return (ULONG_PTR)pCmd;    // some sort of overrun has occurred
}

/*****************************Public*Routine********************************\
* ULONG MCDrvClear
*
* The MCDrvClear function will clear the requested buffers in the specified
* rectangle.  If the driver fails this function, the clear operation will be
* performed using software simulation on supported buffers.  Overlay planes
* and stencil buffers sizes other than 8 bits are not supported through
* software simulation.  The clear rectangle is specified relative to the entire
* buffer region given by pMCDSurface.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the surface
*                   containing the buffers to be cleared.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context containing the the state required for the buffer
*                   clear operation.
*               pRect
*                   Points to a RECTL structure specifying the rectanglar area
*                   in the buffer to clear.  This rectangle is relative to the
*                   region given by pMCDSurface.
*               buffers
*                   Specified the buffers to be cleared.  This value is a
*                   combination of GL_COLOR_BUFFER_BIT, GL_STENCIL_BUFFER_BIT,
*                   and GL_DEPTH_BUFFER_BIT.
*
* Return Value      The function returns TRUE if all of the specified buffers
*                   were cleared, FALSE otherwise.
* Notes
*   Clears color, depth, stencil buffers
*   Uses current clear values in context
*   Clears specified rectangle
\***************************************************************************/

ULONG WINAPI
MCDrvClear(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc, ULONG buffers)
{

    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;
    MCDWINDOW *pWnd;
    ULONG cClip;
    RECTL *pClip;

    MCDBG_PRINT(DBG_MCD,"MCDrvClear");

    MCD_CHECK_RC(pRc);

    pWnd = pMCDSurface->pWnd;

    MCD_CHECK_BUFFERS_VALID(pMCDSurface, pRc, TRUE);

    pRc->ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

#if DBG_FAIL_ALL_DRAWING
    return FALSE;
#endif

    if (buffers & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                    GL_ACCUM_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
        MCDBG_PRINT(DBG_ERR,"MCDrvClear: unknown buffer requested.");
        return FALSE;
    }

    if ((buffers & GL_DEPTH_BUFFER_BIT) && (!pRc->zBufEnabled))
    {
        MCDBG_PRINT(DBG_ERR,"MCDrvClear: clear z requested with z-buffer disabled.");
        return FALSE;
    }

    if (buffers & (GL_ACCUM_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
        MCDBG_PRINT(DBG_ERR,"MCDrvClear: no accum or stencil support");
        return FALSE;
    }

    // Return if we have nothing to clear:

    if (!(cClip = pWnd->pClip->c))
        return TRUE;

    // Initialize hardware state for filling operation:

    HW_INIT_DRAWING_STATE(pMCDSurface, pMCDSurface->pWnd, pRc);

    // We have to protect against bad clear colors since this can
    // potentially cause an FP exception:

    try {

        HW_START_FILL_RECT(pMCDSurface, pMCDRc, pRc, buffers);

    } except (EXCEPTION_EXECUTE_HANDLER) {

        MCDBG_PRINT(DBG_ERR,"!!Exception in MCDrvClear!!");
        return FALSE;
    }


    for (pClip = &pWnd->pClip->arcl[0]; cClip; cClip--,
         pClip++)
    {
        // Do the fill:

        HW_FILL_RECT(pMCDSurface, pRc, pClip);
    }

#if DBG_TESSELATION
    // Textured triangle tesselation debugging code. Each time we clear the
    // color buffer we set iTessCount so that alternating frames will show
    // the triangles properly textured or gouraud shaded with a contrasting
    // color to check how are they being subdivided.
    if (buffers & GL_COLOR_BUFFER_BIT) {
        if (iTessCount >=0)
            iTessCount = -1;
        else
            iTessCount = 0;
    }
#endif


    MCDBG_PRINT(DBG_MCD,"Exit MCDrvClear");
    return (ULONG)TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvSwap
*
* This function displays the content of the back buffer on the front buffer.
* The mechanics of the swap must adhere to the pixel format capabilities
* established earlier through the pixel format and context creation/binding
* mechanisms.  For example, if a buffer swap is requested on a surface with
* a pixel format that supports PFD_SWAP_COPY, the driver must copy the contents
* of the back buffer to the front buffer.
*
* The driver should honor the MCDCONTEXT_SWAPSYNC flag specified by createFlags
* in MCDRC if the driver supports smooth, tear-free double-buffering.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the surface
*                   containing the buffers to be swapped.
*               flags
*                   Specifies the buffers to be swapped.  This may be a
*                   combination of the following:
*
*                   Value                 Meaning
*
*                   MCDSWAP_MAIN_PLANE   Specifies that the main color buffers
*                                        are to be swapped.
*                   MCDSWAP_OVERLAYn     Specifies that overlay plane  n is to
*                                        be swapped.  n is between 1 and 15,
*                                        inclusive.  MCDSWAP_OVERLAY1 identifies
*                                        the first layer plane over the main
*                                        plane, and so on.
*                   MCDSWAP_UNDERLAYn    Specifies that underlay plane n is to
*                                        be swapped.  n is between 1 and 15,
*                                        inclusive. MCDSWAP_UNDERLAY1 identifies
*                                        the first layer plane under the main
*                                        plane, and so on.
*
* Return Value      The function returns TRUE if the buffer swap was completed
*                   successfully, FALSE otherwise.
*
* Comments          Software simulation will not be provided if the driver fails
*                   this function.
* Notes
*   Swaps buffers with specified surface (Use MCDCONTEXT_SWAP_SYNC)
*   Implementation up to the driver
*   Not supported in simulations
\***************************************************************************/

ULONG WINAPI
MCDrvSwap(MCDSURFACE *pMCDSurface, ULONG flags)
{

    MCDWINDOW  *pWnd;
    ULONG       cClip;
    RECTL      *pClip;
    POINTL      ptSrc;
    PDEV       *ppdev   = (PDEV *)pMCDSurface->pso->dhpdev;
    DEVWND     *pDevWnd;
    OH          ohDst;

    MCDBG_PRINT(DBG_MCD,"MCDrvSwap");

    pWnd = pMCDSurface->pWnd;

#if DBG_ONLY_FRONTBUFFER
    return TRUE; // Rendering is forced to the front buffer, so don't swap!
#endif

    // If we're not tracking this window, just return...

    if (!pWnd) {
        MCDBG_PRINT(DBG_ERR,"MCDrvSwap: trying to swap an untracked window");\
        return FALSE;
    }

    pDevWnd = (DEVWND *)(pMCDSurface->pWnd->pvUser);

    if (!pDevWnd) {
        MCDBG_PRINT(DBG_ERR,"MCDrvSwap: NULL buffers.");\
        return FALSE;
    }

    if (!pDevWnd->bValidBackBuffer) {
        MCDBG_PRINT(DBG_ERR,"MCDrvSwap: back buffer invalid");
        return FALSE;
    }

    if (pDevWnd->dispUnique != GetDisplayUniqueness(ppdev)) {
        MCDBG_PRINT(DBG_ERR,"MCDrvSwap: resolution changed but not updated");
        return FALSE;
    }

    // Just return if we have nothing to swap:
    //
    //      - no visible rectangle
    //      - per-plane swap, but none of the specified planes
    //        are supported by driver

    if (!(cClip = pWnd->pClipUnscissored->c) ||
        (flags && !(flags & MCDSWAP_MAIN_PLANE)))
        return TRUE;

    // Wait for sync if we can do a fast blt:

    HW_WAIT_FOR_VBLANK(pMCDSurface);

    pClip = &pWnd->pClipUnscissored->arcl[0];

    //
    // handle windowed buffer case
    //

    if (ppdev->cDoubleBufferRef)
    {
        ptSrc.x = pWnd->clipBoundsRect.left;
        ptSrc.y = pWnd->clipBoundsRect.top;
    }
    else
    {
        ptSrc.x = pWnd->clipBoundsRect.left & 7;
        ptSrc.y = 0;
    }

    //
    // Swap all of the clip rectangles in the backbuffer to the front:
    //

    ohDst.lDelta = ppdev->lDelta;
    ohDst.ulBase =
    ohDst.x      =
    ohDst.y      = 0;

    WAIT_DMA_IDLE( ppdev );
    TRIANGLE_WORKAROUND( ppdev );

    vMmCopyBlt3D (ppdev,
                  &ohDst,
                  pDevWnd->pohBackBuffer,
                  cClip,
                  pClip,
                  0xCC,
                  &ptSrc,
                  &pWnd->clipBoundsRect);

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvSwap");
    return (ULONG)TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvState
*
* The MCDrvState function is used to modify states or sets of states associated
* with a rendering context. The pStart parameter specifies a pointer to the
* start of the state data in the memory block specified by pMCDMem.  This
* pointer specifies a buffer of states or state commands, with each state
* command accessible as an MCDSTATE structure.  Although the stateValue field of
* MCDSTATE is specified as a ULONG, the size and format of the state data depends
* on the state field in each MCDSTATE specified. The driver is required to
* process the entire state buffer.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the device
*                   surface.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context to receive state changes.
*               pMCDMem
*                   Points to an MCDMEM structure specifying the memory block
*                   which contains the state buffer.
*               length
*                   Specifies the length in bytes of state data to process.
*               numStates
*                   Specifies the total number of states to process.
*
* Return Value      The function returns TRUE if the specified states were
*                   processed successfully, FALSE otherwise.  If the driver
*                   returns FALSE, it must fail all drawing until the driver
*                   has received a full set of state information successfully.
*                   Othewise, the driver may operate with a faulty or
*                   incomplete set of state information.
* Notes:
*       MCDRENDERSTATE
*          Contains all rasterization-level OpenGL information
*          -- enable flags, culling, stipples, fog, hints, stencil, alpha, etc
*          -- user clip planes
*          -- include 1.1 polygon offset support
*       MCDPIXELSTATE
*
\***************************************************************************/

ULONG WINAPI
MCDrvState(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc, MCDMEM *pMCDMem,
                 UCHAR *pStart, LONG length, ULONG numStates)
{

    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;
    MCDSTATE *pState = (MCDSTATE *)pStart;
    MCDSTATE *pStateEnd = (MCDSTATE *)(pStart + length);

    MCDBG_PRINT(DBG_MCD,"MCDrvState");

    MCD_CHECK_RC(pRc);

    while (pState < pStateEnd) {

        if (((UCHAR *)pStateEnd - (UCHAR *)pState) < sizeof(MCDSTATE)) {
            MCDBG_PRINT(DBG_ERR,"MCDrvState: buffer too small");
            return FALSE;
        }

        switch (pState->state) {
            case MCD_RENDER_STATE:
                if (((UCHAR *)pState + sizeof(MCDRENDERSTATE)) >
                    (UCHAR *)pStateEnd) {
                    MCDBG_PRINT(DBG_ERR,"MCDrvState: MCD_RENDER_STATE, "
                                        "buffer too small");
                    return FALSE;
                }

                memcpy(&pRc->MCDState, &pState->stateValue,
                   sizeof(MCDRENDERSTATE));

                // Flag the fact that we need to re-pick the
                // rendering functions since the state has changed:

                pRc->pickNeeded = TRUE;

                pState = (MCDSTATE *)((UCHAR *)pState +
                                    sizeof(MCDSTATE_RENDER));
                break;

            case MCD_PIXEL_STATE:
                // Not accelerated in this driver, so we can ignore this state
                // (which implies that we do not need to set the pick flag).

                pState = (MCDSTATE *)((UCHAR *)pState +
                                    sizeof(MCDSTATE_PIXEL));
                break;

            case MCD_SCISSOR_RECT_STATE:
                // Not needed in this driver, so we can ignore this state
                // (which implies that we do not need to set the pick flag).

                pState = (MCDSTATE *)((UCHAR *)pState +
                                    sizeof(MCDSTATE_SCISSOR_RECT));
                break;

            case MCD_TEXENV_STATE:
                // Needed to pass modulating/blending modes and color
                if (((UCHAR *)pState + sizeof(MCDTEXENVSTATE)) >
                    (UCHAR *)pStateEnd) {
                    MCDBG_PRINT(DBG_ERR,"MCDrvState: MCD_TEXENV_STATE,"
                                        " buffer too small");
                    return FALSE;
                }

                memcpy(&pRc->MCDTexEnvState, &pState->stateValue,
                       sizeof(MCDTEXENVSTATE));

                // Flag the fact that we need to re-pick the
                // rendering functions since the state has changed:
                pRc->pickNeeded = TRUE;

                pState = (MCDSTATE *)((UCHAR *)pState +
                                    sizeof(MCDSTATE_TEXENV));
                break;

            default:
                MCDBG_PRINT(DBG_ERR,"MCDrvState: Unrecognized state %d.",
                                    pState->state);
                return FALSE;
        }
    }

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvState");
    return (ULONG)TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvViewport
*
* This function is used to give the driver the information required to perform
* primitive clipping.  Window coordinates (X, Y, Z, W) can be generated from
* object coordinates (x, y, z, w) as follows:
*
*    W = 1 / w
*    X = ((x * xScale) * W) + xCenter
*    Y = ((y * yScale) * W) + yCenter
*    Z = ((z * zScale) * W) + zCenter
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the device
*                   surface.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context with which the specified viewport will be used.
*               pMCDViewport
*                   Points to an MCDVIEWPORT structure specifying the new
*                   viewport.
*
* Return Value      The function returns TRUE if the viewport was successfully
*                   applied to the the rendering context, FALSE otherwise.
* Notes
*     Specifies scaling and offset values needed for (3D) clipping
*     Driver should clip
\***************************************************************************/

ULONG WINAPI
MCDrvViewport(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc,
                    MCDVIEWPORT *pMCDViewport)
{
    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;

    MCDBG_PRINT(DBG_MCD,"MCDrvViewport");

    MCD_CHECK_RC(pRc);
    pRc->MCDViewport = *pMCDViewport;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvViewport");
    return (ULONG)TRUE;
}


/*****************************Public*Routine********************************\
* ULONG MCDrvSpan
*
* The MCDrvSpan function is used to transfer pixel data during software
* drawing simulations.   Although the simulations will use direct framebuffer
* access whenever possible, this function will be used in those cases where
* direct framebuffer access is available but the format is incompatible with
* the simulation functions, or when DCI/DirectDraw has been disabled or is not
* available.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the device
*                   surface.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context to be used for span drawing.
*               pMCDMem
*                   Points to an MCDMEM structure specifying the memory
*                   block used to transfer the required pixel data.
*               pMCDSpan
*                   Points to an MCDSPAN structure specifying the type,
*                   location, and amount of pixel data to transfer.
*               bRead
*                   If TRUE, the pixel data is to be read from the buffer
*                   requested.  If  FALSE, the specified pixel data is written
*                   to the specified buffer.
*
* Return Value      The function returns TRUE is the specified span was
*                   successfully read or written, FALSE othersise.
* Comments
*             Part of interface for completness, not speed
\***************************************************************************/

ULONG WINAPI
MCDrvSpan(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc, MCDMEM *pMCDMem,
                MCDSPAN *pMCDSpan, BOOL bRead)
{
    DEVRC       *pRc        = (DEVRC  *)pMCDRc->pvUser;
    PDEV        *ppdev      = (PDEV   *)pMCDSurface->pso->dhpdev;
    UCHAR       *pScreen;
    UCHAR       *pPixels;
    MCDWINDOW   *pWnd;
    DEVWND      *pDevWnd;
    LONG        xLeftOrg, xLeft, xRight, y;
    ULONG       bytesNeeded;
    ULONG       cjPelSize;

    MCDBG_PRINT( DBG_MCD,
                 "MCDrvSpan: read %d, (%d, %d) type %d",
                 bRead,
                 pMCDSpan->x,
                 pMCDSpan->y,
                 pMCDSpan->type );

    MCD_CHECK_RC(pRc);

    pRc->ppdev = ppdev;
    pWnd       = pMCDSurface->pWnd;

    //
    // Return if we have nothing to clip:
    //

    if (!pWnd->pClip->c)
    {
        return TRUE;
    }

    //
    // Fail if number of pixels is negative or range is null
    //

    if (pMCDSpan->numPixels <= 0)
    {
        MCDBG_PRINT(DBG_ERR,"MCDrvSpan: numPixels <=0 ");
        return FALSE;
    }

    MCD_CHECK_BUFFERS_VALID(pMCDSurface, pRc, TRUE);

    pDevWnd  = (DEVWND *) pWnd->pvUser;

    xLeft    =
    xLeftOrg = (pMCDSpan->x + pWnd->clientRect.left);
    xRight   = (xLeft       + pMCDSpan->numPixels);
    y        = pMCDSpan->y  + pWnd->clientRect.top;

    //
    // Early-out spans which are not visible:
    //

    if ( (y <  pWnd->clipBoundsRect.top   ) ||
         (y >= pWnd->clipBoundsRect.bottom) )
    {
        return TRUE;
    }

    xLeft  = max(xLeft,  pWnd->clipBoundsRect.left);
    xRight = min(xRight, pWnd->clipBoundsRect.right);

    //
    // Return if empty:
    //

    if (xLeft >= xRight)
    {
        return TRUE;
    }

    switch (pMCDSpan->type)
    {
    case MCDSPAN_FRONT:
        cjPelSize   =  ppdev->cjPelSize;
        bytesNeeded =  pMCDSpan->numPixels * cjPelSize;
        pScreen     =  ppdev->pjScreen                      +
                       (y                  * ppdev->lDelta) +
                       (xLeft              *  cjPelSize);
        break;

    case MCDSPAN_BACK:
        cjPelSize   = ppdev->cjPelSize;
        bytesNeeded = pMCDSpan->numPixels * cjPelSize;

        if (ppdev->cDoubleBufferRef)
        {
            //
            // Full screen back buffer
            //

            pScreen = ppdev->pjScreen           +
                      pDevWnd->backBufferOffset +
                      y     * ppdev->lDelta     +
                      xLeft * cjPelSize;
        }
        else
        {
            //
            // Per window back buffer (not full screen)
            //

            pScreen = ppdev->pjScreen                                             +
                pDevWnd->backBufferOffset                                         +
                pMCDSpan->y                         * pDevWnd->ulBackBufferStride +
                (pWnd->clipBoundsRect.left & 7)     * cjPelSize                   +
                (xLeft - pWnd->clipBoundsRect.left) * cjPelSize;
        }

        break;

    case MCDSPAN_DEPTH:
        //
        // Z is always 16bpp on ViRGE
        //
        cjPelSize   = 2;
        bytesNeeded = pMCDSpan->numPixels *  cjPelSize;

        if (ppdev->cZBufferRef)
        {
            //
            // Full screen Z Buffer
            //

            pScreen = ppdev->pjScreen                              +
                      (((y * ppdev->cxMemory + xLeft) * cjPelSize) +
                        pDevWnd->zBufferOffset);
        }
        else
        {
            //
            // Per window Z Buffer
            //

            pScreen =  ppdev->pjScreen                                       +
                       pDevWnd->zBufferOffset                                +
                       pMCDSpan->y                         * pDevWnd->zPitch +
                       (pWnd->clipBoundsRect.left & 7)     * cjPelSize       +
                       (xLeft - pWnd->clipBoundsRect.left) * cjPelSize;
        }

        break;
    default:
        MCDBG_PRINT(DBG_ERR,"MCDrvReadSpan: Unrecognized buffer %d",
                            pMCDSpan->type);
        return FALSE;
    }

    //
    // Make sure we don't read past the end of the buffer:
    //

    if (((char *)pMCDSpan->pPixels + bytesNeeded) >
        ((char *)pMCDMem->pMemBase + pMCDMem->memSize))
    {
        MCDBG_PRINT(DBG_ERR,"MCDrvSpan: Buffer too small");
        return FALSE;
    }

    pPixels = pMCDSpan->pPixels;

    if (bRead)
    {
        if (xLeftOrg != xLeft)
        {
            pPixels = (UCHAR *) pMCDSpan->pPixels +
                                    ((xLeft - xLeftOrg) * cjPelSize);
        }

        RtlCopyMemory(pPixels, pScreen, (xRight - xLeft) * cjPelSize);
    }
    else
    {
        LONG    xLeftClip, xRightClip;
        RECTL  *pClip;
        ULONG   cClip;

        for (pClip = &pWnd->pClip->arcl[0], cClip = pWnd->pClip->c;
             cClip;
             cClip--, pClip++)
        {
            UCHAR *pScreenClip;

            //
            // Test for trivial cases:
            //

            if (y < pClip->top)
            {
                break;
            }

            //
            // Determine trivial rejection for just this span
            //

            if ((xLeft  >= pClip->right)    ||
                (y      >= pClip->bottom)   ||
                (xRight <= pClip->left))
            {
                continue;
            }

            //
            // Intersect current clip rect with the span:
            //

            xLeftClip  = max(xLeft,  pClip->left);
            xRightClip = min(xRight, pClip->right);

            if (xLeftClip >= xRightClip)
            {
                continue;
            }

            if (xLeftOrg != xLeftClip)
            {
                pPixels = (UCHAR *)pMCDSpan->pPixels +
                                    ((xLeftClip - xLeftOrg) * cjPelSize);
            }

            pScreenClip = pScreen + ((xLeftClip - xLeft) * cjPelSize);

            //
            // Write the span:
            //

            RtlCopyMemory(pScreenClip,
                          pPixels,
                          (xRightClip - xLeftClip) * cjPelSize);
        }
    }

    MCDBG_PRINT( DBG_MCD, "Exit MCDrvSpan" );
    return (ULONG)TRUE;
}

/*****************************Public*Routine********************************\
* VOID MCDrvTrackWindow
*
* The MCDrvTrackWindow function is called to notify the driver of surface
* and window-region changes.  The driver may use this function to perform
* updates of any device-specific buffer data such as window ID planes.  This
* function should be used to free resources tied to a window when the window
* is deleted.  The driver should also use this function to resize or reallocate
* any device buffers or resources required due to a window resize event.
*
* Parameters   pWndObj
*                  Points to the WNDOBJ structure specifying the window and
*                  surface changes.
*              pMCDWnd
*                  Points an the MCDWINDOW structure corresponding the tracked
*                  window.  This parameter will be NULL for WOC_RGN_SURFACE
*                  and WOC_RGN_SURFACE_DELTA.
*              flags
*                  Flags for the region notification.  These flags are the same
*                  as those specified by the WNDOBJCHANGEPROC callback function.
*                  Any per-window resources tied to pMCDWnd should be freed in
*                  response to WOC_DELETE.  For WOC_RGN_CLIENT, the driver
*                  should reallocate the buffer resources required for the
*                  surface if the device resolution has changed.  If the
*                  resolution has not changed, the driver should attempt to
*                  reallocate any resources dependant on window size.
*
* Return Value     None.
*
* Notes
*    Used to track window clipping, surface region changes
*    Driver gets full WNDOBJ info
*    Driver must free resources used for a window surface at window deletion
*    Driver must also track resizes for reallocating drawing buffers
*
\***************************************************************************/

VOID WINAPI
MCDrvTrackWindow(WNDOBJ *pWndObj, MCDWINDOW *pMCDWnd, ULONG flags)
{
#ifdef MCD95
    SURFOBJ bogusSurf;
    SURFOBJ *pso = &bogusSurf;

    bogusSurf.dhpdev = gppdev;
#else
    SURFOBJ *pso = pWndObj->psoOwner;
#endif

    MCDBG_PRINT(DBG_MCD,"MCDrvTrackWindow");

    //
    // Note: pMCDWnd is NULL for surface notifications, so if needed
    // they should be handled before this check:
    //

    if (!pMCDWnd)
        return;

    if (!pMCDWnd->pvUser) {
        MCDBG_PRINT(DBG_MCD,"MCDrvTrackWindow: NULL pDevWnd");
        return;
    }

    switch (flags) {
        case WOC_DELETE:

            MCDBG_PRINT(DBG_MCD,"MCDrvTrackWindow: WOC_DELETE");

            // If the display resoultion has changed, the resources we had
            // bound to the tracked window are gone, so don't try to delete
            // the back- and z-buffer resources which are no longer present:

            if (((DEVWND *)pMCDWnd->pvUser)->dispUnique ==
                GetDisplayUniqueness((PDEV *)pso->dhpdev))
                HWFreeResources(pMCDWnd, pso);

            MCDMemFree((VOID *)pMCDWnd->pvUser);
            pMCDWnd->pvUser = NULL;

            break;

        case WOC_RGN_CLIENT:

            // The resources we had  bound to the tracked window have moved,
            // so update them:
            {
                DEVWND *pWnd        = (DEVWND *)pMCDWnd->pvUser;
                BOOL    bHaveDepth  = (pWnd->pohZBuffer != NULL);
                BOOL    bHaveBack   = (pWnd->pohBackBuffer != NULL);
                PDEV   *ppdev       = (PDEV *)pso->dhpdev;
                ULONG   height      = pMCDWnd->clientRect.bottom -
                                      pMCDWnd->clientRect.top;
                ULONG   width       = pMCDWnd->clientRect.right -
                                      pMCDWnd->clientRect.left;
                BOOL    bWindowBuffer =
                            ((bHaveDepth && !ppdev->cZBufferRef) ||
                             (bHaveBack  && !ppdev->cDoubleBufferRef));

                // If the window is using a window-sized back/z resource,
                // we need to reallocate it if there has been a size change:

                if (pWnd->dispUnique == GetDisplayUniqueness(ppdev))
                {
                    if ( (height != pWnd->allocatedBufferHeight ||
                          width  != pWnd->allocatedBufferWidth ) &&
                         bWindowBuffer )
                    {
                           HWFreeResources(pMCDWnd, pso);
                           HWAllocResources(pMCDWnd, pso,bHaveDepth,bHaveBack);
                    }
                }
                else
                {
                    // In this case, the display has been re-initialized due
                    // to some event such as a resolution change, so we need
                    // to create our buffers from scratch:

                    pWnd->dispUnique =GetDisplayUniqueness((PDEV *)pso->dhpdev);
                    HWAllocResources(pMCDWnd, pso, bHaveDepth, bHaveBack);
                }

            }
            break;

        default:
            break;
    }

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvTrackWindow");

    return;
}


/*****************************Public*Routine********************************\
* ULONG MCDrvSync
*
* This function is used to synchronize hardware drawing.  The driver should
* finish any remaining processing of the drawing batch, and wait until the
* drawing engine becomes idle before returning.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the device
*                   surface.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context for the specified surface.
*
* Return Value      Returns TRUE when the drawing engine becomes idle.
* Comments
*           Driver responsible for synchronization on kickbacks to simulations
*           Exception: pixel functions
\***************************************************************************/

ULONG WINAPI
MCDrvSync(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc)
{

    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;

    MCDBG_PRINT(DBG_MCD,"MCDrvSync");
    pRc->ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    WAIT_DMA_IDLE( pRc->ppdev );

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvSync");
    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvCreateTexture
*
* This function is called during texture creation.  If the driver can support
* the specified texure, it must allocate room for the texture, and load the
* texture data into device memory using the image supplied in the texture
* function. Alternately, the driver may chose to delay allocation of device
* memory for the texture. In this case, the driver must cache enough
* information about the texture to allow realizing the texture at a later time.
* The MCDTEXTUREDATA structure will be stable thoughout the life of the texture,
* so the driver can cache a pointer to this structure to retain access to the
* client-managed texture information.
*
* Parameters    pMCDSurface
*                    Points to an MCDSURFACE structure specifying the surface
*                    for which texture is being created.
*               pMCDRc
*                    Points to an MCDRC structure specifying the rendering
*                    context for which the texture is being created.  Textures
*                    may be shared among any number of contexts with matching
*                    pixel formats.
*               pMCDTexture
*                    Points to an MCDTEXTURE structure which contains
*                    information about the texture being created.  If the
*                    driver successfully creates the texture, it must place
*                    a unique driver-specific identifier in the textureKey
*                    field.  This value will be used to identify the desired
*                    texture to apply when drawing primitives.
*
* Return Value       The function will return TRUE if the driver could create
*                    and load the texture, FALSE otherwise
* Comments
*       Used to create and load the texture
*       Driver must fill in non-zero textureKey
\***************************************************************************/

ULONG WINAPI
MCDrvCreateTexture(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc, MCDTEXTURE *pMCDTexture)
{

    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;
    PDEV *ppdev = (PDEV *)pMCDSurface->pso->dhpdev;
    DEVTEXTURE *newTexture;
    DEVMIPMAPLEVEL *newLevels;
    MCDTEXTUREDATA *pMCDTextureData;
    LONG i,totLevels;

    MCDBG_PRINT(DBG_MCD,"MCDrvCreateTexture");

    if (!pMCDTexture) {
        MCDBG_PRINT(DBG_ERR,"MCDrvCreateTexture:Texture structure "
                            "pointer is null");
        return FALSE;
    }

    pMCDTextureData = pMCDTexture->pMCDTextureData;

    if (!pMCDTextureData) {
        MCDBG_PRINT(DBG_ERR,"MCDrvCreateTexture:Texture Data "
                            "structure pointer is null");
        return FALSE;
    }

    // First create a DEVTEXTURE structure for this texture
    newTexture = (DEVTEXTURE *)MCDMemAlloc(sizeof(DEVTEXTURE));
    if (newTexture == NULL) {
        MCDBG_PRINT(DBG_ERR,"MCDrvCreateTexture:Failed to allocate "
                            "DEVTEXTURE structure");
        return FALSE;
    }

    // Store the texture data which is generic to all mip-map levels
    newTexture->sWrapMode   = pMCDTextureData->textureState.sWrapMode;
    newTexture->tWrapMode   = pMCDTextureData->textureState.tWrapMode;
    newTexture->minFilter   = pMCDTextureData->textureState.minFilter;
    newTexture->magFilter   = pMCDTextureData->textureState.magFilter;
    newTexture->borderColor = pMCDTextureData->textureState.borderColor;

    newTexture->name        = pMCDTextureData->textureObjState.name;
    newTexture->priority    = pMCDTextureData->textureObjState.priority;

    newTexture->textureDimension  = pMCDTextureData->textureDimension;
    newTexture->paletteSize       = pMCDTextureData->paletteSize;
    newTexture->paletteData       = pMCDTextureData->paletteData;
    newTexture->paletteBaseFormat = pMCDTextureData->paletteBaseFormat;
    newTexture->paletteRequestedFormat =pMCDTextureData->paletteRequestedFormat;
    newTexture->bMipMaps          = FALSE;
    newTexture->validState        = DEV_TEXTURE_NOT_VALIDATED;

    // Now create as many mip-map levels as necessary

    // check if this is a non-mipmapped texture
    if (pMCDTexture->pMCDTextureData->level[1].width == 0)
        totLevels = 1;
    else {
        totLevels = 1;
        while ((pMCDTexture->pMCDTextureData->level[totLevels].width > 0) &&
                (pMCDTexture->pMCDTextureData->level[totLevels].width <=
                pMCDTexture->pMCDTextureData->level[totLevels-1].width))
            totLevels++;
    }

    newTexture->totLevels = totLevels;

    newLevels =
        (DEVMIPMAPLEVEL *)MCDMemAlloc(sizeof(DEVMIPMAPLEVEL)*(totLevels));

    if (newLevels == NULL) {
        MCDBG_PRINT(DBG_ERR,"MCDrvCreateTexture:Failed to allocate "
                            "DEVMIPMAPLEVEL structure!");
        MCDMemFree((UCHAR *)newTexture);
        return FALSE;
    }

    for(i=0; i<totLevels; i++) {
        // Setup the information static to this mip-map level
        newLevels[i].parent = newTexture; //set up pointer back to main texture
        if ( !HWAllocTexMemory(ppdev, pRc, &(newLevels[i]),
                                &(pMCDTexture->pMCDTextureData->level[i]))) {
                MCDBG_PRINT(DBG_ERR,"MCDrvCreateTexture:Failed to allocate "
                                    "Texture Memory!");
                MCDMemFree((UCHAR *)newLevels);
                MCDMemFree((UCHAR *)newTexture);
                return FALSE;
        }
    }

    newTexture->MipMapLevels = newLevels;
    pMCDTexture->textureKey = (ULONG_PTR)newTexture;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvCreateTexture");

    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvUpdateSubTexture
*
* This function is used to update rectangular regions of textures which have
* already been created.  Only one level of detail is updated per call.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the device
*                   surface.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context with which the texture image is being updated.
*                   Although this function is invoked with a single valid
*                   rendering context, the texture object is a global resource
*                   which may be used by multiple rendering contexts, each or
*                   which is able to modify the texture image data.
*               pMCDTexture
*                   Points to an MCDTEXTURE structure which contains the
*                   updated texture data.
*               lod
*                   Specifies the level of detail for the texture image being
*                   updated.  The first and largest texture image in a sequence
*                   of mipmap levels is 0.  The driver should use this value
*                   to access the correct MCDMIPMAPLEVEL structure in
*                   pMCDTexture in order to update the device's copy of the
*                   texture image data.
*               pRect
*                   Specifies the rectangle within the texture image which
*                   requires updating.  The rectangle is relative to the image
*                   specified by the level of detail parameter lod, and is
*                   given in pixel (or texel) units.
*
* Return Value      The function will return TRUE if the driver could update
*                   the specified texture, FALSE otherwise.
\***************************************************************************/

ULONG WINAPI
MCDrvUpdateSubTexture(MCDSURFACE *pMCDSurface, MCDRC *pMCDRc,
                            MCDTEXTURE *pMCDTexture, ULONG lod, RECTL *pRect)
{
    DEVTEXTURE *pDevTexture;
    DEVRC *pRc = (DEVRC *)pMCDRc->pvUser;
    PDEV * ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    MCDBG_PRINT(DBG_MCD,"MCDrvUpdateSubTexture");

    CHK_TEX_KEY(pMCDTexture, textureKey);

    pDevTexture = (DEVTEXTURE *)pMCDTexture->textureKey;

    return HWLoadTexRegion(ppdev, pRc, pDevTexture, lod, pRect);
}

/*****************************Public*Routine********************************\
* ULONG MCDrvUpdateTexturePalette
*
* This function requests the driver to update the texture palette for the
* specified texture.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the device
*                   surface.
*               pMCDRc
*                   Points to an MCDRC structure specifying the rendering
*                   context with which the texture palette is being updated.
*                   Although this function is invoked with a single valid
*                   rendering context, the texture object is a global resource
*                   which may be used by multiple rendering contexts, each or
*                   which is able to modify the texture palette.
*               pMCDTexture
*                   Points to an MCDTEXTURE structure which contains the
*                   updated texture palette data.
*               start
*                   Specifies the first index of palette data that requires
*                   updating.
*               numEntries
*                   Specifies the total number of palette entries that require
*                   updating.
*
* Return Value      The function will return TRUE if the driver could update
*                   the palette for the specified texture, FALSE otherwise.
* Comments
*       Updates a range of entries in the texture palette
*       Can be used for palette animation
\***************************************************************************/

ULONG WINAPI
MCDrvUpdateTexturePalette(MCDSURFACE *pMCDSurface, MCDRC *pRc,
                                MCDTEXTURE *pMCDTexture, ULONG start,
                                ULONG numEntries)
{
    DEVTEXTURE *pDevTexture;
#if TEX_PALETTED_SUPPORT
    ULONG      i;
#endif

    MCDBG_PRINT(DBG_MCD,"MCDrvUpdateTexturePalette");

    CHK_TEX_KEY(pMCDTexture, textureKey);

    pDevTexture = (DEVTEXTURE *)pMCDTexture->textureKey;

#if TEX_PALETTED_SUPPORT
    // DRIVER WRITERS:
    // The S3Virge doesn't handle paletted textures in the way
    // OpenGL expects, so we have this code here disabled.

    // Copy across the new palette entries
    for(i=start; i<start+numEntries; i++) {
        pDevTexture->paletteData[i] =
                    pMCDTexture->pMCDTextureData->paletteData[i];
    }
#endif

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvUpdateTexturePalette");
    return FALSE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvUpdateTexturePriority
*
* This function assigns or reassigns the priority of a specified texture to
* allow more efficient driver-managed texture caching.  If the driver fails
* this call, the client will attempt to maintain an efficient texture cache
* and efficient heap by recreating the currently active textures in priority
* order.
*
* Parameters   pMCDSurface
*                  Points to an MCDSURFACE structure specifying the device
*                  surface.
*              pMCDRc
*                  Points to an MCDRC structure specifying the rendering
*                  context through which the texture priority is being updated.
*                  Although this function is invoked with a single valid
*                  rendering context, the texture object is a global resource
*                  which may be used by multiple rendering contexts, each or
*                  which is able to modify the texture priority.
*              pMCDTexture
*                  Points to an MCDTEXTURE structure which contains the updated
*                  texture priority in
*                  pMCDTexture->pMCDTextureData->textureObjState.priority.
*
* Return Value     The function will return TRUE if the driver successfully
*                  updates the priority of the specified texture, FALSE
*                  otherwise.
\***************************************************************************/

ULONG WINAPI
MCDrvUpdateTexturePriority(MCDSURFACE *pMCDSurface, MCDRC *pRc,
                                 MCDTEXTURE *pMCDTexture)
{
    DEVTEXTURE *pDevTexture;

    MCDBG_PRINT(DBG_MCD,"MCDrvUpdateTexturePriority");

    CHK_TEX_KEY(pMCDTexture, textureKey);

    pDevTexture = (DEVTEXTURE *)pMCDTexture->textureKey;

    // Update the stored priority
    pDevTexture->priority =
            pMCDTexture->pMCDTextureData->textureObjState.priority;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvUpdateTexturePriority");
    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvUpdateTextureState
*
* The MCDrvUpdateTextureState function is called to update those texture
* states which are bound to a texture object.  The set of states that may be
* updated are contained in the MCDTEXTURESTATE structure.
*
* Parameters   pMCDSurface
*                  Points to an MCDSURFACE structure specifying the device
*                  surface.
*              pMCDRc
*                  Points to an MCDRC structure specifying the rendering context
*                  for which texture state is being updated.  Although this
*                  function is invoked with a single valid rendering context,
*                  the texture object is a global resource which may be used by
*                  multiple rendering contexts, each or which is able to modify
*                  the texture state.
*              pMCDTexture
*                  Points to an MCDTEXTURE structure which contains the updated
*                  texture state information in
*                  pMCDTexture->pMCDTextureData->textureState.
*
* Return Value     The function will return TRUE if the driver successfully
*                  updates the texture state of the specified texture, FALSE
*                  otherwise.
* Comments
*       Updates portion of the texture state not bound to context
*       (wrap mode, filter, border color)
\***************************************************************************/

ULONG WINAPI
MCDrvUpdateTextureState(MCDSURFACE *pMCDSurface, MCDRC *pRc,
                              MCDTEXTURE *pMCDTexture)
{
    DEVTEXTURE *pDevTexture;
    MCDTEXTUREDATA *pMCDTextureData;

    MCDBG_PRINT(DBG_MCD,"MCDrvUpdateTextureState");

    CHK_TEX_KEY(pMCDTexture, textureKey);

    pDevTexture = (DEVTEXTURE *)pMCDTexture->textureKey;
    pMCDTextureData = pMCDTexture->pMCDTextureData;

    // Update the stored texture state
    pDevTexture->sWrapMode  = pMCDTextureData->textureState.sWrapMode;
    pDevTexture->tWrapMode  = pMCDTextureData->textureState.tWrapMode;
    pDevTexture->minFilter  = pMCDTextureData->textureState.minFilter;
    pDevTexture->magFilter  = pMCDTextureData->textureState.magFilter;
    pDevTexture->borderColor= pMCDTextureData->textureState.borderColor;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvUpdateTextureState");
    return TRUE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvTextureStatus
*
* The MCDrvTextureStatus function is used to retrieve information about the
* specified texture.
*
* Parameters   pMCDSurface
*                  Points to an MCDSURFACE structure specifying the device
*                  surface.
*              pMCDRc
*                  Points to an MCDRC structure specifying the rendering context
*                  for which texture status is being requested.  Although this
*                  function is invoked with a single valid rendering context,
*                  the texture object is a global resource which may be used by
*                  multiple rendering contexts, each or which is able to modify
*                  the texture, or retrieve the current status of the texture.
*              pMCDTexture
*                  Points to an MCDTEXTURE structure corresponding to the
*                  texture for which status information is returned.
*
* Return Value     The function may return the following value, or 0:
*
*                  Value                   Meaning
*
*                  MCDRV_TEXTURE_RESIDENT  The specified texture is currently
*                                          contained loaded in device memory.
*                                          The driver should not set this flag
*                                          if the texture is cached in host
*                                          memory.
\***************************************************************************/

ULONG WINAPI
MCDrvTextureStatus(MCDSURFACE *pMCDSurface, MCDRC *pRc,
                         MCDTEXTURE *pMCDTexture)
{
    DEVTEXTURE *pDevTexture;
    PDEV * ppdev = (PDEV *)pMCDSurface->pso->dhpdev;

    MCDBG_PRINT(DBG_MCD,"MCDrvTextureStatus");

    CHK_TEX_KEY(pMCDTexture, textureKey);

    pDevTexture = (DEVTEXTURE *)pMCDTexture->textureKey;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvTextureStatus");

    return HWResidentTex(ppdev, pDevTexture);
}

/*****************************Public*Routine********************************\
* ULONG MCDrvDeleteTexture
*
* The MCDrvDeleteTexture function is called when textures are destroyed.  The
* driver must delete any resources used by the specified texture
*
* Parameters    pMCDTexture
*                   Points to an MCDTEXTURE structure which contains
*                   information about the texture being deleted.
*               dhpdev
*                   Specifies the device for which the texture was created.
*
* Return Value      The function will return TRUE if the driver could delete
*                   the specified texture, FALSE otherwise.
\***************************************************************************/

ULONG WINAPI
MCDrvDeleteTexture(MCDTEXTURE *pMCDTexture, DHPDEV dhpdev)
{
    DEVTEXTURE *pDevTexture;

#ifdef MCD95
    //
    // On Win95, dhpdev is bogus.  Replace with gppdev.
    //

    dhpdev = (DHPDEV) gppdev;
#endif

    MCDBG_PRINT(DBG_MCD,"MCDrvDeleteTexture");

    CHK_TEX_KEY(pMCDTexture, textureKey);

    pDevTexture = (DEVTEXTURE *)pMCDTexture->textureKey;

    // Need to chuck out allocated memory.
    if (!HWFreeTexMemory((PDEV *)dhpdev, pDevTexture)) {
            MCDBG_PRINT(DBG_ERR,"MCDrvDeleteTexture:Failed to free S3 memory");
            return FALSE;
    }

    if (pDevTexture->MipMapLevels == NULL) {
        MCDBG_PRINT(DBG_ERR,"MCDrvDeleteTexture:Attempted to delete null "
                            "mip-map levels");
        return FALSE;
    }

    // Guard against future invalid usage of the texture key
    pDevTexture->validState = DEV_TEXTURE_NOT_VALIDATED;

    // Delete all mip-map levels in this texture.
    MCDMemFree((UCHAR *)pDevTexture->MipMapLevels);
    pDevTexture->MipMapLevels = NULL;

    // Delete the main texture structure.
    MCDMemFree((UCHAR *)pDevTexture);
    pDevTexture = NULL;

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDeleteTexture");
    return TRUE;
}
/*****************************Public*Routine********************************\
* ULONG MCDrvDrawPixels
*
* This function allows the driver to hook the glDrawPixels API.  It writes a
* block of pixels to the specified buffer relative to the current raster
* position. With the exception of the packed flag, the parameters and
* description of this function are identical to glDrawPixels (please refer
* to the OpenGL specification for a complete description).
*
* If the driver cannot support the operation with the current state (refer to
* the MCDPIXELSTATE structure description), the function may return FALSE and
* the generic implementation will complete the operation.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the target
*                   surface.
*               pRc
*                   A pointer to an MCDRC structure specifying the rendering
*                   context for which this pixel transfer is being performed.
*               width
*                   Specifies the width of the pixel rectangle in the buffer
*                   that will be written to.
*               height
*                   Specifies the height of the pixel rectangle in the buffer
*                   that will be written to.
*               format
*                   Specifies the format of the pixel data array pointed to by
*                   pPixels.  Can be one of the following: GL_COLOR_INDEX,
*                   GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_RGBA, GL_RED,
*                   GL_GREEN, GL_BLUE, GL_ALPHA, GL_RGB, GL_LUMINANCE,
*                   GL_LUMINANCE_ALPHA, GL_BGRA_EXT, and GL_BGR_EXT.
*               type
*                   Specifies the data type for the pixels data array pointed
*                   to by pPixels.  Can be on of the following types:
*                   GL_UNSIGNED_BYTE, GL_BYTE, GL_BITMAP, GL_UNSIGNED_SHORT,
*                   GL_SHORT, GL_UNSIGNED_INT, GL_INT, and GL_FLOAT.
*               pPixels
*                   Points to the pixel data array to be copied to the buffer.
*               packed
*                   If TRUE, the current MCDPIXELUNPACK state is superseded by
*                   an implicit packed format as follows:
*
*                   MCDPIXELUNPACK Member         Value if (packed == TRUE)
*
*                        swapEndian                       FALSE
*                        lsbFirst                         FALSE
*                        lineLength               width (from parameter list)
*                        skipLines                          0
*                        skipPixels                         0
*                        alignment                          1
*
* Return Value      The function returns TRUE if the operation was completed
*                   successfully. If the driver returns FALSE, the OpenGL
*                   generic implementation will supply a simulation and
*                   complete the operation.
*
* Comments          The rasterPos member of the MCDPIXELSTATE structure
*                   specifies the location of the lower left corner of the
*                   pixel rectangle written to the buffer.
\***************************************************************************/

ULONG WINAPI
MCDrvDrawPixels(MCDSURFACE *pMcdSurface, MCDRC *pRc,
                      ULONG width, ULONG height, ULONG format,
                      ULONG type, VOID *pPixels, BOOL packed)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvDrawPixels");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvDrawPixels");
    return FALSE;
}
/*****************************Public*Routine********************************\
* ULONG MCDrvReadPixels
*
* This function is allows the driver to hook the glReadPixels API.  It reads
* a rectangular block of pixels (specified by the location of the lower left
* corner and size of the rectangle) from the specified buffer. The parameters
* and description of this function are identical to glReadPixels (please refer
* to the OpenGL specification for a complete description).
*
* If the driver cannot support the operation with the current state (refer to
* the MCDPIXELSTATE structure description), the function may return FALSE and
* the generic implementation will complete the operation.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the target
*                   surface.
*               pRc
*                   A pointer to an MCDRC structure specifying the rendering
*                   context for which this pixel transfer is being performed.
*               x
*                   Together with y, specifies the OpenGL window coordinate of
*                   the lower left corner of the rectangular block of pixels
*                   that will be read.
*               y
*                   Together with x, specifies the OpenGL window coordinate
*                   of the lower left corner of the rectangular block of pixels
*                   that will be read.
*               width
*                   Specifies the width of the pixel rectangle in the buffer
*                   that will be read.
*               height
*                   Specifies the height of the pixel rectangle in the buffer
*                   that will be read.
*               format
*                   Specifies the format of the pixel data array pointed to by
*                   pPixels.  Can be one of the following: GL_COLOR_INDEX,
*                   GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_RGBA, GL_RED,
*                   GL_GREEN, GL_BLUE, GL_ALPHA, GL_RGB, GL_LUMINANCE,
*                   GL_LUMINANCE_ALPHA, GL_BGRA_EXT, and GL_BGR_EXT.
*               type
*                   Specifies the data type for the pixels data array pointed
*                   to by pPixels.  Can be on of the following types:
*                   GL_UNSIGNED_BYTE, GL_BYTE, GL_BITMAP, GL_UNSIGNED_SHORT,
*                   GL_SHORT, GL_UNSIGNED_INT, GL_INT, and GL_FLOAT.
*               pPixels
*                   Points to the pixel data array to be that returns the data
*                   read.
*
* Return Value      The function returns TRUE if the operation was completed
*                   successfully.  If the driver returns FALSE, the OpenGL
*                   generic implementation will supply a software simulation
*                   and complete the operation.
\***************************************************************************/

ULONG WINAPI
MCDrvReadPixels(MCDSURFACE *pMcdSurface, MCDRC *pRc,
                      LONG x, LONG y, ULONG width, ULONG height,
                      ULONG format, ULONG type, VOID *pPixels)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvReadPixels");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvReadPixels");
    return FALSE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvCopyPixels
*
* This function allows the driver to hook the glCopyPixels API.  It copies
* a rectangular block of pixels (specified by the location of the lower left
* corner and size of the rectangle) to the current raster position.  The
* parameters and description of this function are identical to glReadPixels
* (please refer to the OpenGL specification for a complete description).
*
* If the driver cannot support the operation with the current state (refer to
* the MCDPIXELSTATE structure description), the function may return FALSE and
* OpenGL implementation will complete the operation through software simulation.
*
* Parameters   pMCDSurface
*                  Points to an MCDSURFACE structure specifying the target
*                  surface.
*              pRc
*                  A pointer to an MCDRC structure specifying the rendering
*                  context for which this pixel transfer is being performed.
*              x
*                  Together with y, specifies the OpenGL window coordinate of
*                  the lower left corner of the rectangular block of pixels
*                  that will be the source of the copy.
*              y
*                  Together with x, specifies the OpenGL window coordinate
*                  of the lower left corner of the rectangular block of pixels
*                  that will be the source of the copy.
*              width
*                  Specifies the width of the pixel rectangle.
*              height
*                  Specifies the height of the pixel rectangle.
*              type
*                  Specifies whether color values, depth values, or stencil
*                  values are to be copied.  Can be one of the following:
*                  GL_COLOR, GL_DEPTH, and GL_STENCIL.
*
* Return Value     The function returns TRUE if the operation was completed
*                  successfully.  If the driver returns FALSE, the OpenGL
*                  generic implementation will supply a simulation and
*                  complete the operation.
*
* Comments
*     The rasterPos member of the MCDPIXELSTATE structure specifies the
*     location of the lower left corner of the destination pixel rectangle.
\***************************************************************************/

ULONG WINAPI
MCDrvCopyPixels(MCDSURFACE *pMcdSurface, MCDRC *pRc,
                      LONG x, LONG y, ULONG width, ULONG height, ULONG type)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvCopyPixels");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvCopyPixels");
    return FALSE;
}

/*****************************Public*Routine********************************\
* ULONG MCDrvPixelMap
*
* This function sets up the translation tables, or maps,  used by
* MCDrvDrawPixels, MCDrvReadPixels, and MCDrvCopyPixels.  It is very similar
* to the glPixelMap API, but unlike the API call it does not have a variety
* of entry points for different data types (i.e., glPixelMapfx, glPixelMapuiv,
* etc.).  Instead, the data type is determined by the mapType (see below).
*
* If the driver cannot support this operation, the function may return FALSE.
*
* Parameters    pMCDSurface
*                   Points to an MCDSURFACE structure specifying the target
*                   surface.
*               pRc
*                   A pointer to an MCDRC structure specifying the rendering
*                   context for which the map is being set.
*               mapType
*                   Specifies the translation table or map to be set.  Can
*                   be one of the following:
*
*                Map                 Description
*
*                GL_PIXEL_MAP_I_TO_I Maps color indices to color indices.
*                GL_PIXEL_MAP_S_TO_S Maps stencil indices to stencil indices.
*                GL_PIXEL_MAP_I_TO_R Maps color indices to red components.
*                GL_PIXEL_MAP_I_TO_G Maps color indices to green components.
*                GL_PIXEL_MAP_I_TO_B Maps color indices to blue components.
*                GL_PIXEL_MAP_I_TO_A Maps color indices to alpha components.
*                GL_PIXEL_MAP_R_TO_R Maps red components to red components.
*                GL_PIXEL_MAP_G_TO_G Maps green components to green components.
*                GL_PIXEL_MAP_B_TO_B Maps blue components to blue components.
*                GL_PIXEL_MAP_A_TO_A Maps alpha components to alpha components.
*
*               mapSize
*                   Specifies the size of the map being defined.
*               pMap
*                   Pointer to an array of mapSize data values.  The type of the
*                   data is LONG for the GL_PIXEL_MAP_I_TO_I and
*                   GL_PIXEL_MAP_S_TO_S maps, MCDFLOAT for all others.
*
* Return Value      The function returns TRUE if the map was successfully set,
*                   FALSE if not set or not supported.
\***************************************************************************/

ULONG WINAPI
MCDrvPixelMap(MCDSURFACE *pMcdSurface, MCDRC *pRc,
                    ULONG mapType, ULONG mapSize, VOID *pMap)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvPixelMap");
    MCDBG_PRINT(DBG_MCD,"Exit MCDrvPixelMap");
    return FALSE;
}

/*****************************Public*Routine********************************\
* BOOL MCDrvGetEntryPoints
*
* This function is called to obtain the function pointers (or addresses) of
* the MCD functions which an MCD implementation supports.  The only MCD
* function not exported in the MCDDRIVER table is the MCDrvGetEntryPoints
* function itself, which is exported when the MCD driver is initialized.
* Unsupported functions are designated by a NULL function address.
*
* Parameters    pMCDSurface
*                   A pointer to an MCDSURFACE structure.  Only the pso field
*                   of this structure will be valid for this function.
*               pMCDDriver
*                   A pointer to an MCDDRIVER structure.  The driver should
*                   fill the appropriate fields of this structure with the
*                   function pointers for supported MCD functions.
*
* Return Value  The function returns TRUE if the function table was
*               successfully filled, FALSE otherwise.
* Notes
*   Fills in supported MCD functions in MCDDRIVER structure
*   -- Unsupported functions will not be called
*   Only a handful of functions are required to draw primitives
\***************************************************************************/

BOOL WINAPI
MCDrvGetEntryPoints(MCDSURFACE *pMCDSurface, MCDDRIVER *pMCDDriver)
{
    MCDBG_PRINT(DBG_MCD,"MCDrvGetEntryPoints");

    // In order to allow newer drivers to run in older systems
    // we fill the MCDDRIVER structure as far as we can according to
    // pMCDDriver->ulSize . We leave it to OPENGL32.DLL to decide if it
    // wants to run MCD or not through the verMajor and verMinor reported
    // in MCDrvInfo

    SET_ENTRY(pMCDDriver->pMCDrvInfo,                 MCDrvInfo,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDescribePixelFormat,  MCDrvDescribePixelFormat,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDescribeLayerPlane,   MCDrvDescribeLayerPlane,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvSetLayerPalette,      MCDrvSetLayerPalette,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvCreateContext,        MCDrvCreateContext,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDeleteContext,        MCDrvDeleteContext,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvCreateMem,            MCDrvCreateMem,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDeleteMem,            MCDrvDeleteMem,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDraw,                 MCDrvDraw,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvClear,                MCDrvClear,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvSwap,                 MCDrvSwap,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvState,                MCDrvState,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvViewport,             MCDrvViewport,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvGetHdev,              MCDrvGetHdev,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvSpan,                 MCDrvSpan,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvTrackWindow,          MCDrvTrackWindow,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvAllocBuffers,         MCDrvAllocBuffers,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvGetBuffers,           MCDrvGetBuffers,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvBindContext,          MCDrvBindContext,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvSync,                 MCDrvSync,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvCreateTexture,        MCDrvCreateTexture,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDeleteTexture,        MCDrvDeleteTexture,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvUpdateSubTexture,     MCDrvUpdateSubTexture,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvUpdateTexturePalette, MCDrvUpdateTexturePalette,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvUpdateTexturePriority,MCDrvUpdateTexturePriority,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvUpdateTextureState,   MCDrvUpdateTextureState,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvTextureStatus,        MCDrvTextureStatus,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvDrawPixels,           MCDrvDrawPixels,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvReadPixels,           MCDrvReadPixels,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvCopyPixels,           MCDrvCopyPixels,
                                                pMCDDriver,pMCDDriver->ulSize);

    SET_ENTRY(pMCDDriver->pMCDrvPixelMap,              MCDrvPixelMap,
                                                pMCDDriver,pMCDDriver->ulSize);

    MCDBG_PRINT(DBG_MCD,"Exit MCDrvGetEntryPoints");

    return TRUE;
}

#endif

