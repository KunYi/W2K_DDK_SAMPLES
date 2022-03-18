//-----------------------------------------------------------------------------
// MODULE:      TEXTOUT.C
//
// DESCRIPTION: On every TextOut, GDI provides an array of 'GLYPHPOS'
//              structures for every glyph to be drawn.  Each GLYPHPOS
//              structure contains a glyph handle and a pointer to a
//              monochrome bitmap that describes the glyph.  (Note that
//              unlike Windows 3.1, which provides a column-major glyph
//              bitmap, Windows NT always provides a row-major glyph bitmap.)
//              As such, there are three basic methods for drawing text
//              with hardware acceleration:
//
//              1) Glyph caching -- Glyph bitmaps are cached by the accelerator
//                  (probably in off-screen memory), and text is drawn by
//                  referring the hardware to the cached glyph locations.
//
//              2) Glyph expansion -- Each individual glyph is color-expanded
//                  directly to the screen from the monochrome glyph bitmap
//                  supplied by GDI.
//
//              3) Buffer expansion -- The CPU is used to draw all the glyphs
//                  into a 1bpp monochrome bitmap, and the hardware is then
//                  used to color-expand the result.
//
//              The fastest method depends on a number of variables, such as
//              the color expansion speed, bus speed, CPU speed, average
//              glyph size, and average string length.
//
//              For the S3 with normal sized glyphs, I've found that caching
//              the glyphs in off-screen memory is typically the slowest
//              method.  Buffer expansion is typically fastest on the slow
//              ISA bus (or when memory-mapped I/O isn't available on the
//              x86), and glyph expansion is best on fast buses such as VL
//              and PCI.
//
//              Glyph expansion is typically faster than buffer expansion
//              for very large glyphs, even on the ISA bus, because less
//              copying by the CPU needs to be done.  Unfortunately, large
//              glyphs are pretty rare.
//
//              An advantange of the buffer expansion method is that opaque
//              text will never flash -- the other two methods typically
//              need to draw the opaquing rectangle before laying down the
//              glyphs, which may cause a flash if the raster is caught at
//              the wrong time.
//
//              This driver implements glyph expansion -- method 2).
//
//              Copyright (c) 1992-1994  Microsoft Corporation
//              Copyright (c) 1997       S3 Incorporated
//-----------------------------------------------------------------------------

// INCLUDE FILES:
//

#include "precomp.h"
#include "hw3d.h"

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES:
//
VOID vClipSolid3D           (PDEV*, OH*, LONG, RECTL*, ULONG, CLIPOBJ*);
VOID vS3DClipGlyphExpansion (PDEV*, STROBJ*, CLIPOBJ*, ULONG);
VOID vS3DGlyphExpansion     (PDEV*, STROBJ*, ULONG);


//-----------------------------------------------------------------------------
// PROCEDURE:   BOOL DrvTextOut:
//
// DESCRIPTION: Outputs text using the 'glyph expansion' method.  Each
//              individual glyph is color-expanded directly to the screen/
//              bitmap from the monochrome glyph bitmap supplied by GDI.
//
// ASSUMPTIONS: None
//
// CALLS:       EngTextOut()
//              (ppdev->pfnFillSolid)()
//              vClipSolid3D()
//              vS3DClipGlyphExpansion()
//              vS3DGlyphExpansion()
//
// PARAMETERS:  SURFOBJ*  pso           pointer to display surface object
//              STROBJ*   pstro         pointer to string object
//              FONTOBJ*  pfo           pointer to font object
//              CLIPOBJ*  pco           pointer to clip object
//              RECTL*    prclExtra
//              RECTL*    prclOpaque    pointer to opaquing rectanble
//              BRUSHOBJ* pboFore       pointer to foreground brush object
//              BRUSHOBJ* pboOpaque     pointer to opaquing rect brush object
//              POINTL*   pptlBrush     pointer to brush origin point
//              MIX       mix
//
// RETURN:      BOOL: Text drawn status (TRUE if text drawn, FALSE if not drawn)
//
// NOTES:       If we had set GCAPS_HORIZSTRIKE, we would have to fill the
//              extra rectangles in prclExtra (it is used largely for
//              underlines).  It's not a big performance win (GDI will call
//              our DrvBitBlt to draw the extra rectangles).
//-----------------------------------------------------------------------------
BOOL DrvTextOut(
SURFOBJ*  pso,          // Destination surface ptr
STROBJ*   pstro,        // String object ptr
FONTOBJ*  pfo,          // Font object ptr
CLIPOBJ*  pco,          // Clipping object ptr
RECTL*    prclExtra,    // Underlining/overstrike rect array ptr (if != NULL)
RECTL*    prclOpaque,   // Opaque background rectangle ptr (if != NULL)
BRUSHOBJ* pboFore,      // Solid color foreground brush object
BRUSHOBJ* pboOpaque,    // Solid color background brush object
POINTL*   pptlBrush,    // Brush origin point ptr
MIX       mix)          // Mix mode (R2_COPYPEN)
{
    BOOL    bTextPerfectFit;
    BYTE    iDComplexity;
    PDEV*   ppdev;
    ULONG   ulCommand;

    DSURF*  pdsurf;
    OH*     poh;

    pdsurf = (DSURF*) pso->dhsurf;
    ppdev  = (PDEV*) pso->dhpdev;

    if (pdsurf->dt == DT_DIB)
    {
      // We're drawing to a DFB we've converted to a DIB, so just call GDI
      // to handle it:

        return (EngTextOut(pdsurf->pso, pstro, pfo, pco, prclExtra, prclOpaque,
                           pboFore, pboOpaque, pptlBrush, mix));
    }

    WAIT_DMA_IDLE(ppdev);
    TRIANGLE_WORKAROUND( ppdev );

//
//  get a pointer to the heap object for the screen/bitmap
//

    poh = pdsurf->poh;

    ASSERTDD(mix == 0x0d0d, "GDI should only give us a copy mix");

    ulCommand = ppdev->ulCommandBase        |
                S3D_BITBLT                  |
                (SRC_COPY << S3D_ROP_SHIFT) |
                S3D_BYTE_ALIGNED            |
                S3D_HARDWARE_CLIPPING       |
                S3D_MONOCHROME_SOURCE       |
                S3D_CPU_DATA                |
                S3D_X_POSITIVE_BLT          |
                S3D_Y_POSITIVE_BLT          |
                S3D_DRAW_ENABLE;

    iDComplexity = (BYTE) ((pco == NULL) ? DC_TRIVIAL : pco->iDComplexity);

    if (prclOpaque != NULL)
    {
    ////////////////////////////////////////////////////////////
    // Opaque Initialization
    ////////////////////////////////////////////////////////////

    // If we paint the glyphs in 'opaque' mode, we may not actually
    // have to draw the opaquing rectangle up-front -- the process
    // of laying down all the glyphs will automatically cover all
    // of the pixels in the opaquing rectangle.
    //
    // The condition that must be satisfied is that the text must
    // fit 'perfectly' such that the entire background rectangle is
    // covered, and none of the glyphs overlap (if the glyphs
    // overlap, such as for italics, they have to be drawn in
    // transparent mode after the opaquing rectangle is cleared).

        bTextPerfectFit = (pstro->flAccel & (SO_ZERO_BEARINGS |
          SO_FLAG_DEFAULT_PLACEMENT | SO_MAXEXT_EQUAL_BM_SIDE |
          SO_CHAR_INC_EQUAL_BM_BASE)) ==
          (SO_ZERO_BEARINGS | SO_FLAG_DEFAULT_PLACEMENT |
          SO_MAXEXT_EQUAL_BM_SIDE | SO_CHAR_INC_EQUAL_BM_BASE);

        if (!(bTextPerfectFit)                                  ||
             (pstro->rclBkGround.top    > prclOpaque->top)      ||
             (pstro->rclBkGround.left   > prclOpaque->left)     ||
             (pstro->rclBkGround.right  < prclOpaque->right)    ||
             (pstro->rclBkGround.bottom < prclOpaque->bottom))
        {
            if (iDComplexity == DC_TRIVIAL)
            {
                RBRUSH_COLOR rbc;

                rbc.iSolidColor = pboOpaque->iSolidColor;

                (ppdev->pfnFillSolid)(ppdev, poh, 1, prclOpaque, 0xF0L,
                                      0xF0L, rbc, NULL);
            }
            else
            {
                vClipSolid3D (ppdev, poh, 1, prclOpaque,
                              pboOpaque->iSolidColor, pco);
            }

            S3DGPWait (ppdev);
        //
        //  This was added to fix a problem where the bottom right
        //  corner of the opaque rectangle was being trashed -- I was
        //  unable to determine the exact sequence which caused the problem.
        //
        }

        if (bTextPerfectFit)
        {
        //
        // If we have already drawn the opaquing rectangle (because
        // it is larger than the text rectangle), we could lay down
        // the glyphs in 'transparent' mode.  But I've found the S3
        // to be a bit faster drawing in opaque mode, so we'll stick
        // with that.  Set the destination base address and background
        // color.
        //

            //
            // fix SPR 16855, change FIFO wait to a GPWait, mono pattern
            // bitblt is not done yet.  The problem will show itself as
            // a white/black strip in the bottom right corner of the
            // rectangle.
            //

            S3DGPWait( ppdev );

            S3writeHIU (ppdev, S3D_BLT_DESTINATION_BASE,        poh->ulBase);
            S3writeHIU (ppdev, S3D_BLT_SOURCE_BACKGROUND_COLOR,
                                                       pboOpaque->iSolidColor);

            goto SkipTransparentInitialization;
        }
    }

////////////////////////////////////////////////////////////
// Transparent Initialization
////////////////////////////////////////////////////////////
//
// Initialize the hardware for transparent text:
//  The following code was added to fix a text corruption problem when using
//  transparent text -- real bad in 24bpp. The FIX_ViRGE call expects the
//  source & destination base registers to be the same. -- bvr
//
    ulCommand |= S3D_TRANSPARENT;

    S3DFifoWait (ppdev, 2);

    S3writeHIU (ppdev, S3D_BLT_DESTINATION_BASE, poh->ulBase);
    S3writeHIU (ppdev, S3D_BLT_SOURCE_BASE,      poh->ulBase);

#ifdef  VIRGE_PATCH17 //----------------

    FIX_ViRGE  (ppdev);

#endif // VIRGE_PATCH17 //--------------

SkipTransparentInitialization:

//
// Set the source/destination stride and the top/bottom clipping
//
    S3DFifoWait (ppdev, 3);

    S3writeHIU (ppdev, S3D_BLT_SOURCE_FOREGROUND_COLOR,   pboFore->iSolidColor);
    S3writeHIU (ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
    S3writeHIU (ppdev, S3D_BLT_CLIP_TOP_BOTTOM,           0x000007FFL);

//
//  go draw the glyphs
//
    if (iDComplexity == DC_TRIVIAL)
    {
        vS3DGlyphExpansion (ppdev, pstro, ulCommand);
    }
    else
    {
        vS3DClipGlyphExpansion (ppdev, pstro, pco, ulCommand);
    }

    return(TRUE);

} // DrvTextOut ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vClipSolid3D:
//
// DESCRIPTION: Fills the specified rectangles with the specified color,
//              honoring the requested clipping.  No more than four
//              rectangles should be passed in.  Intended for drawing the
//              areas of the opaquing rectangle that extend beyond the text
//              box.  The rectangles must be in left to right, top to bottom
//              order.  Assumes there is at least one rectangle in the list.
//
// ASSUMPTIONS: None
//
// CALLS:       CLIPOBJ_bEnum()
//              CLIPOBJ_cEnumStart()
//              cIntersect()
//              (ppdev->pfnFillSolid)()
//
// PARAMETERS:  See below
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
VOID vClipSolid3D(
PDEV*       ppdev,      // Destination PDEV structure ptr
OH*         poh,        // Offscreen heap object ptr
LONG        crcl,       // Rectangle count
RECTL*      prcl,       // Rectangle list ptr
ULONG       iColor,     // Rectangle fill color
CLIPOBJ*    pco)        // Clipping object ptr
{
    BOOL            bMore;              // Flag for clip enumeration
    CLIPENUM        ce;                 // Clip enumeration object
    ULONG           i;
    ULONG           j;
    RECTL           arclTmp[4];
    ULONG           crclTmp;
    RECTL*          prclTmp;
    RECTL*          prclClipTmp;
    LONG            iLastBottom;
    RECTL*          prclClip;
    RBRUSH_COLOR    rbc;

    ASSERTDD((crcl > 0) && (crcl <= 4), "Expected 1 to 4 rectangles");
    ASSERTDD((pco != NULL) && (pco->iDComplexity != DC_TRIVIAL),
               "Expected a non-null clip object");

    rbc.iSolidColor = iColor;
    if (pco->iDComplexity == DC_RECT)
    {
        crcl = cIntersect(&pco->rclBounds, prcl, crcl);
        if (crcl != 0)
        {
            (ppdev->pfnFillSolid)(ppdev, poh, crcl, prcl, 0xF0L, 0xF0L,
                                    rbc, NULL);
        }
    }
    else // iDComplexity == DC_COMPLEX
    {
    // Bottom of last rectangle to fill

        iLastBottom = prcl[crcl - 1].bottom;

    // Initialize the clip rectangle enumeration to right-down so we can
    // take advantage of the rectangle list being right-down:

        CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_RIGHTDOWN, 0);

    // Scan through all the clip rectangles, looking for intersects
    // of fill areas with region rectangles:

        do
        {
        // Get a batch of region rectangles:

            bMore = CLIPOBJ_bEnum(pco, sizeof(ce), (VOID*)&ce);

        // Clip the rect list to each region rect:

            for (j = ce.c, prclClip = ce.arcl; j-- > 0; prclClip++)
            {
            // Since the rectangles and the region enumeration are both
            // right-down, we can zip through the region until we reach
            // the first fill rect, and are done when we've passed the
            // last fill rect.

                if (prclClip->top >= iLastBottom)
                {
                // Past last fill rectangle; nothing left to do:

                    return;
                }

            // Do intersection tests only if we've reached the top of
            // the first rectangle to fill:

                if (prclClip->bottom > prcl->top)
                {
                // We've reached the top Y scan of the first rect, so
                // it's worth bothering checking for intersection.

                // Generate a list of the rects clipped to this region
                // rect:

                    prclTmp     = prcl;
                    prclClipTmp = arclTmp;

                    for (i = crcl, crclTmp = 0; i-- != 0; prclTmp++)
                    {
                    // Intersect fill and clip rectangles

                        if (bIntersect(prclTmp, prclClip, prclClipTmp))
                        {
                        // Add to list if anything's left to draw:

                            crclTmp++;
                            prclClipTmp++;
                        }
                    }

                // Draw the clipped rects

                    if (crclTmp != 0)
                    {
                        (ppdev->pfnFillSolid)(ppdev, poh, crclTmp, &arclTmp[0],
                                            0xF0L, 0xF0L, rbc, NULL);
                    }
                }
            }
        } while (bMore);
    }

} // vClipSolid3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vS3DGlyphExpansion:
//
// DESCRIPTION: Outputs text using the 'glyph expansion' method.  Each
//              individual glyph is color-expanded directly to the screen/
//              bitmap from the monochrome glyph bitmap supplied by GDI.
//              No text boundary clipping rectangle is needed/used.
//
// ASSUMPTIONS: None
//
// CALLS:       STROBJ_bEnum()
//
// PARAMETERS:  PDEV*     ppdev         pointer to display's pdevice
//              STROBJ*   pstro         pointer to string object
//              ULONG     ulCommand     engine command
//
// RETURN:      none
//
// NOTES:       The code assumes that the text glyphs are byte aligned (if
//              the glyph is not 8 pixel width aligned, the glyph width is
//              padded out to an even 8 pixel (1 byte) boundary before the
//              next glyph scanline data begins.
//-----------------------------------------------------------------------------
VOID vS3DGlyphExpansion(
PDEV*     ppdev,
STROBJ*   pstro,
ULONG     ulCommand)
{
    BYTE*       pjMmBase;
    ULONG       cGlyph;
    BOOL        bMoreGlyphs;
    GLYPHPOS*   pgp;
    GLYPHBITS*  pgb;
    BYTE*       pjGlyph;
    LONG        ulCharInc;
    ULONG       yTop;
    ULONG       xLeft;
    ULONG       width;
    ULONG       height;
    ULONG       ulGlyphXferDwords;
    ULONG       ulDstGlyphXY;
    ULONG       ulDstGlyphWidHgt;

//
// Enable command autoexecute for glyph xfers (saves 1 FIFO slot within loop)
//  Disable hardware clipping as these glyphs are unclipped
//
    pjMmBase = ppdev->pjMmBase;
    ulCommand |= S3D_AUTOEXECUTE;
    ulCommand &= (~S3D_HARDWARE_CLIPPING);

    S3DFifoWait (ppdev, 1);

    S3writeHIU (ppdev, S3D_BLT_COMMAND, ulCommand); // enable cmd autoexecute

//
// Setup destination X,Y position, width/height, and transfer glyph(s)
//
    do // while (bMoreGlyphs);
    {
        if (pstro->pgp != NULL)     // glyph positions here, skip enumeration
        {
            pgp         = pstro->pgp;
            cGlyph      = pstro->cGlyphs;
            bMoreGlyphs = FALSE;
        }
        else // (pstro->pgp == NULL)
        {
            bMoreGlyphs = STROBJ_bEnum (pstro, &cGlyph, &pgp);
        }

        if (cGlyph == 0)
        {
            continue;       // no glyph xfer, continue do/while (bMoreGlyphs)
        }

//
// Presume Mono spacing/Fixed pitch font; load variables before entering loop
//
        ulCharInc = pstro->ulCharInc;
        pgb   = pgp->pgdf->pgb;
        xLeft = (pgb->ptlOrigin.x + pgp->ptl.x - ulCharInc);
        yTop  = (pgb->ptlOrigin.y + pgp->ptl.y);

        do // while (--cGlyphs);
        {
            pgb = pgp->pgdf->pgb;           // source glyph bitmap info ptr
            pjGlyph = pgb->aj;              // source glyph bitmap image ptr

            width  = pgb->sizlBitmap.cx;    // glyph width  in pixels
            height = pgb->sizlBitmap.cy;    // glyph height in pixels

            if (ulCharInc == 0)         // Proportional spacing/variable pitch
            {
                xLeft = (pgp->ptl.x + pgb->ptlOrigin.x);
                yTop  = (pgp->ptl.y + pgb->ptlOrigin.y);
            }
            else // (ulCharInc != 0)    // Mono spacing/fixed pitch
            {
                xLeft += ulCharInc;
            }

            ulGlyphXferDwords = (((((width + 7) >> 3) * height) + 3) >> 2);
            ulDstGlyphWidHgt  = (((width - 1) << 16) | height);
            ulDstGlyphXY      = ((xLeft << 16) | yTop);

            //
            // If drawing opaque text and the command registers/data will
            //  fit into the FIFO, only allocate the FIFO space necessary,
            //  else wait for idle.
            //
#ifdef  VIRGE_PATCH17 //----------------

            if ((ulCommand & S3D_TRANSPARENT) ||
                (ulGlyphXferDwords >= (MM_ALL_EMPTY_FIFO_COUNT - 2)))
            {
                S3DGPWait (ppdev); // wait for idle/all FIFO slots free
            }
            else // OPAQUE, (ulGlyphXferDwords < (MM_ALL_EMPTY_FIFO_COUNT - 2))
            {
                S3DFifoWait (ppdev, (ulGlyphXferDwords + 2));
            }

#else // !VIRGE_PATCH17 //--------------

            if (ulGlyphXferDwords < (MM_ALL_EMPTY_FIFO_COUNT - 2))
            {
                S3DFifoWait (ppdev, (ulGlyphXferDwords + 2));
            }
            else // (ulGlyphXferDwords >= (MM_ALL_EMPTY_FIFO_COUNT - 2))
            {
                S3DGPWait (ppdev); // wait for idle/all FIFO slots free
            }

#endif // VIRGE_PATCH17 //--------------

            S3writeHIU (ppdev, S3D_BLT_WIDTH_HEIGHT,    ulDstGlyphWidHgt);
            S3writeHIU (ppdev, S3D_BLT_DESTINATION_X_Y, ulDstGlyphXY);

            MM_TRANSFER_DWORD (ppdev, pjMmBase, pjGlyph, ulGlyphXferDwords, 10);

            pgp++;      // advance to next glyph in the array

        } while (--cGlyph);
    } while (bMoreGlyphs);

//
// Disable command autoexecute as glyph transfer(s) complete
//
//  ulCommand = (ppdev->ulCommandBase | S3D_NOP);   // NOP disables autoexecute
    ulCommand |= S3D_NOP;   // NOP disables autoexecute
    ulCommand &= ~S3D_AUTOEXECUTE;      //clear autoexecute bit
    S3DGPWait (ppdev); // wait for idle/FIFO empty

//  S3DFifoWait (ppdev, 1);

    S3writeHIU (ppdev, S3D_BLT_COMMAND, ulCommand); // disable cmd autoexecute

    return;

} // vS3DGlyphExpansion ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vS3DClipGlyphExpansion:
//
// DESCRIPTION: Handles any strings that need to be clipped, using the 'glyph
//              expansion' method.
//
// ASSUMPTIONS: The S3D_BLT_CLIP_TOP_BOTTOM register has been set by the caller.
//
// CALLS:       CLIPOBJ_bEnum()
//              CLIPOBJ_cEnumStart()
//              STROBJ_bEnum()
//
// PARAMETERS:  PDEV*     ppdev         pointer to display's pdevice
//              STROBJ*   pstro         pointer to string object
//              CLIPOBJ*  pco           pointer to clip object
//              ULONG     ulCommand     engine command
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
VOID vS3DClipGlyphExpansion(
PDEV*     ppdev,        // Destination surface PDEV ptr
STROBJ*   pstro,        // String object ptr
CLIPOBJ*  pco,          // Clip object ptr
ULONG     ulCommand)    // Virge mono pattern bitblt command
{
    BYTE*       pjMmBase;
    BOOL        bMoreGlyphs;
    ULONG       cGlyphOriginal;
    ULONG       cGlyph;
    GLYPHPOS*   pgpOriginal;
    GLYPHPOS*   pgp;
    GLYPHBITS*  pgb;
    BOOL        bMore;
    CLIPENUM    ce;
    RECTL*      prclClip;
    ULONG       ulCharInc;
    BYTE*       pjGlyph;
    LONG        width;
    LONG        height;
    LONG        lGlyphClipYAdj;         // Glyph software clip top coord adjust
    ULONG       ulDstClipRectLR;        // Destination clip rect left/right
    ULONG       ulDstGlyphXY;           // Destination glyph X,Y coord DWORD
    ULONG       ulDstGlyphWidHgt;       // Destination glyph width/height DWORD
    ULONG       ulGlyphWidthBytes;      // Glyph width in bytes
    ULONG       ulGlyphXferDwords;      // Glyph transfer size in DWORDs
    POINTL      ptlGlyphStringOrg;      // Glyph string top/left origin point
    RECTL       rclGlyphPos;            // Glyph dest position, right/bottom pts
    RECTL       rclClipRect;            // Clip rectangle (0-based right/bottom)
    BYTE*       pjGlyphTmp;
    ULONG       ulRemaining;

    ASSERTDD(pco != NULL, "Don't expect NULL clip objects here");

    pjMmBase = ppdev->pjMmBase;

//
// Enable glyph xfer cmd autoexecute (saves FIFO slot within glyph xfer loop)
//
    ulCommand |= (S3D_AUTOEXECUTE | S3D_HARDWARE_CLIPPING);

    S3DFifoWait (ppdev, 1);             // wait for FIFO slots available

    S3writeHIU (ppdev, S3D_BLT_COMMAND, ulCommand);

//
// Transfer all enumerated glyph strings
//
    do // while (bMoreGlyphs);
    {
        if (pstro->pgp != NULL)     // single string, skip string enumeration
        {
            pgpOriginal    = pstro->pgp;
            cGlyphOriginal = pstro->cGlyphs;
            bMoreGlyphs    = FALSE;
        }
        else // (pstro->pgp == NULL)
        {
            bMoreGlyphs = STROBJ_bEnum (pstro, &cGlyphOriginal, &pgpOriginal);
        }

        if (cGlyphOriginal == 0)
        {
            continue;       // no glyph xfer, continue do/while (bMoreGlyphs)
        }

        ulCharInc = pstro->ulCharInc;

//
// We could call 'cEnumStart' and 'bEnum' when the clipping is DC_RECT, but the
//  last time I checked, those two calls took more than 150 instructions to go
//  through GDI.  Since 'rclBounds' already contains the DC_RECT clip rectangle,
//  and since it's such a common case, we'll special case it:
//
        if (pco->iDComplexity == DC_RECT)
        {
            rclClipRect = pco->rclBounds;
            bMore = FALSE;
            ce.c  = 1;

            goto S3DCGE_SingleRectangle;
        }

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);

        do  // while (bMore);
        {
            bMore = CLIPOBJ_bEnum(pco, sizeof(ce), (ULONG*) &ce);

            for (prclClip = &ce.arcl[0]; ce.c != 0; ce.c--, prclClip++)
            {
                rclClipRect = *prclClip;    // copy clip rectangle to local var

            S3DCGE_SingleRectangle:

                pgp    = pgpOriginal;
                cGlyph = cGlyphOriginal;
                pgb    = pgp->pgdf->pgb;

                rclClipRect.right--;        // clip rect.right  last pixel drawn
                rclClipRect.bottom--;       // clip rect.bottom last pixel drawn

                ptlGlyphStringOrg.x = (pgp->ptl.x + pgb->ptlOrigin.x);
                ptlGlyphStringOrg.y = (pgp->ptl.y + pgb->ptlOrigin.y);

//
// Set the glyph string left/right clipping register (top/bottom clipping is
//  done in software to reduce the glyph data transfer requirements).
//
                ulDstClipRectLR = ((rclClipRect.left << 16) |
                                    rclClipRect.right);

                S3DFifoWait (ppdev, 1);     // wait for FIFO slots available

                S3writeHIU (ppdev, S3D_BLT_CLIP_LEFT_RIGHT, ulDstClipRectLR);

//-----------------------------------------------------------------------------
// Transfer the text glyphs located within the current clipping rectangle
// NOTE: This code handles both clipped and unclipped text glyphs.
//
                for (;;)    // loop until all glyphs transferred
                {
                    width  = pgb->sizlBitmap.cx;    // unclipped glyph width
                    height = pgb->sizlBitmap.cy;    // unclipped glyph height

                    rclGlyphPos.left   = ptlGlyphStringOrg.x;
                    rclGlyphPos.top    = ptlGlyphStringOrg.y;
                    rclGlyphPos.right  = (ptlGlyphStringOrg.x + width  - 1);
                    rclGlyphPos.bottom = (ptlGlyphStringOrg.y + height - 1);

                    if ((rclGlyphPos.left > rclClipRect.right) ||
                        (rclGlyphPos.right < rclClipRect.left))
                    {
                        goto S3DCGE_AdvanceToNextGlyph; // glyph fully clipped
                    }

                    pjGlyph           = pgb->aj;        // glyph image pointer
                    ulGlyphWidthBytes = ((width + 7) >> 3);

//
// If the glyph is clipped on top or bottom, adjust height and glyph bitmap ptr
//
                    if (rclGlyphPos.top < rclClipRect.top)
                    {
                        lGlyphClipYAdj = (rclClipRect.top - rclGlyphPos.top);
                        rclGlyphPos.top = rclClipRect.top;

                        height  -= lGlyphClipYAdj;
                        pjGlyph += (lGlyphClipYAdj * ulGlyphWidthBytes);
                    }

                    if (rclGlyphPos.bottom > rclClipRect.bottom)
                    {
                        height -= (rclGlyphPos.bottom - rclClipRect.bottom);
                    }

//
// The glyph is at least partially visible; setup and transfer it to the engine
//
                    if ((height > 0) && (width > 0))
                    {
                        ulDstGlyphWidHgt = (((width - 1) << 16) | height);
                        ulDstGlyphXY = ((rclGlyphPos.left << 16) |
                                         rclGlyphPos.top);
                        ulGlyphXferDwords =
                            (((ulGlyphWidthBytes * height) + 3) >> 2);

#ifdef  VIRGE_PATCH17 //----------------

                        //
                        // If drawing opaque text and the command regs/data
                        //  will fit into the FIFO, only allocate FIFO space
                        //  necessary, else wait for idle.
                        //
                        if ((ulCommand & S3D_TRANSPARENT) ||
                            (ulGlyphXferDwords >= (MM_ALL_EMPTY_FIFO_COUNT-2)))
                        {
                            S3DGPWait (ppdev); // wait for idle/all slots free
                        }
                        else // OPAQUE &&
                           // (ulGlyphXferDwords < (MM_ALL_EMPTY_FIFO_COUNT-2))
                        {
                            S3DFifoWait (ppdev, (ulGlyphXferDwords + 2));
                        }

#else // !VIRGE_PATCH17 //--------------

                        //
                        // If the command registers and data will fit into
                        //  the FIFO, only allocate the FIFO space necessary
                        //  (rather than wait for idle).
                        //
                        if (ulGlyphXferDwords < (MM_ALL_EMPTY_FIFO_COUNT - 2))
                        {
                            S3DFifoWait (ppdev, (ulGlyphXferDwords + 2));
                        }
                        else
                        {
                            S3DGPWait (ppdev); // wait for idle/FIFO empty
                        }

#endif // VIRGE_PATCH17 //--------------

                        S3writeHIU (ppdev, S3D_BLT_WIDTH_HEIGHT,
                                    ulDstGlyphWidHgt);
                        S3writeHIU (ppdev, S3D_BLT_DESTINATION_X_Y,
                                    ulDstGlyphXY);

                        ulRemaining = ulGlyphWidthBytes * height;

                        if (ulRemaining > 32768)
                        {
                            pjGlyphTmp = pjGlyph;

                            do
                            {
                                MM_TRANSFER_DWORD (ppdev, pjMmBase, pjGlyphTmp, 8192, 11 );
                                ulRemaining -= 32768;
                                pjGlyphTmp  += 32768;
                            }
                            while (ulRemaining > 32768);

                            //
                            // handle any left overs
                            //
                            if (ulRemaining > 0)
                            {
                                MM_TRANSFER (ppdev, pjMmBase, pjGlyphTmp,
                                             ulRemaining );
                            }
                        }
                        else
                        {
                            MM_TRANSFER (ppdev, pjMmBase, pjGlyph,
                                         ulGlyphWidthBytes * height );
                        }

                    } // end if ((height > 0) && (width > 0))

                S3DCGE_AdvanceToNextGlyph:

                    if (--cGlyph == 0)
                    {
                        break;
                    }

                    pgp++;                  // advance to next glyph in string
                    pgb = pgp->pgdf->pgb;

                    if (ulCharInc == 0)
                    {
                        ptlGlyphStringOrg.x = (pgp->ptl.x + pgb->ptlOrigin.x);
                        ptlGlyphStringOrg.y = (pgp->ptl.y + pgb->ptlOrigin.y);
                    }
                    else
                    {
                        ptlGlyphStringOrg.x += ulCharInc;
                    }
                } // end for (;;)
            } // end for (prclClip = &ce.arcl[0]; ce.c != 0; ce.c--, prclClip++)
        } while (bMore);
    } while (bMoreGlyphs);

//
// Disable command autoexecute as glyph transfer(s) complete
//
//  ulCommand = (ppdev->ulCommandBase | S3D_NOP);   // NOP to disab autoexecute
    ulCommand |= S3D_NOP;   // NOP disables autoexecute
    ulCommand &= ~S3D_AUTOEXECUTE;      //clear autoexecute bit
    S3DGPWait (ppdev); // wait for idle/FIFO empty

//  S3DFifoWait (ppdev, 1);

    S3writeHIU (ppdev, S3D_BLT_COMMAND, ulCommand); // disable cmd autoexecute

    return;

} // vS3DClipGlyphExpansion ()

