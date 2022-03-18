//-----------------------------------------------------------------------------
// MODULE:      BLTMM3D.C
//
// DESCRIPTION: Contains the low-level memory-mapped IO blt functions.
//
// NOTES:       In the following, 'relative' coordinates refers to coordinates
//              that haven't yet had the offscreen bitmap (DFB) offset applied.
//              'Absolute' coordinates have had the offset applied.  For
//              example, we may be told to blt to (1, 1) of the bitmap, but
//              the bitmap may be sitting in offscreen memory starting at
//              coordinate (0, 768) -- (1, 1) would be the 'relative' start
//              coordinate, and (1, 769) would be the 'absolute' start
//              coordinate.
//
// Copyright (c) 1992-1994  Microsoft Corporation
// Copyright (c) 1997       S3 Incorporated
//-----------------------------------------------------------------------------
//

#include "precomp.h"
#include "hw3d.h"

//-----------------------------------------------------------------------------
// LOCAL DEFINES:
//
#define COLOR_BLACK         0x00000000L     // Black color (at any pixel depth)

#define ROP3_BLACKNESS      0x00            // Blackness ROP3 code
#define ROP3_WHITENESS      0xFF            // Whiteness ROP3 code

#define S3D_CMD_ROP_MASK    (0xFF << S3D_ROP_SHIFT) // Command reg ROP mask

#define AVEC_NOT            0x01
#define AVEC_D              0x02
#define AVEC_S              0x04
#define AVEC_P              0x08
#define AVEC_DS             0x10
#define AVEC_DP             0x20
#define AVEC_SP             0x40
#define AVEC_DSP            0x80
#define AVEC_NEED_SOURCE    (AVEC_S | AVEC_DS | AVEC_SP | AVEC_DSP)
#define AVEC_NEED_PATTERN   (AVEC_P | AVEC_DP | AVEC_SP | AVEC_DSP)
#define AVEC_NEED_DEST      (AVEC_D | AVEC_DS | AVEC_DP | AVEC_DSP)

extern BYTE gajRop3[];

//-----------------------------------------------------------------------------
// PROCEDURE:   VOID FIX_ViRGE:
//
// DESCRIPTION: This routine is called to fix problem A-13 on the ViRGE chip
//              where commands using a mono pattern or source follows a line
//              draw command, the system will hang (see SI017-G).  It does a
//              one pixel screen to screen blt.
//
// ASSUMPTIONS: The Blt Source and Destination Base Address registers are set
//              to the same value!!!!!!!!!
//
// CALLS:       none
//
// PARAMETERS:  PDEV*   ppdev   pointer to the display's pdevice
//
// RETURN:      none
//
// NOTES:       none
//
VOID FIX_ViRGE(
PDEV* ppdev)                            // Destination screen/offscreen PDEV
{
    ULONG   ulEngDrawCmd;

    ulEngDrawCmd = (ppdev->ulCommandBase |      // screen to screen SRCCOPY cmd
                    S3D_BITBLT           |
                    S3D_DRAW_ENABLE      |
                    S3D_X_POSITIVE_BLT   |
                    S3D_Y_POSITIVE_BLT   |
                    (SRC_COPY << S3D_ROP_SHIFT));

    S3DFifoWait (ppdev, 4);             // wait for FIFO slots available

    S3writeHIU (ppdev, S3D_BLT_DESTINATION_X_Y, 0L);
    S3writeHIU (ppdev, S3D_BLT_WIDTH_HEIGHT,    0x00000001L);
    S3writeHIU (ppdev, S3D_BLT_SOURCE_X_Y,      0L);
    S3writeHIU (ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd);

    return;

} // end FIX_ViRGE ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID S3DImageTransferMm32:
//
// DESCRIPTION: Low-level routine for transferring a bitmap image via the
//              data transfer register using 32 bit writes and entirely
//              memory-mapped I/O.
//
// ASSUMPTIONS: none
//
// CALLS:       none
//
// PARAMETERS:  PDEV*   ppdev       pointer to display's pdevice
//              BYTE*   pjSrc       Source pointer
//              LONG    lDelta      Delta from start of scan to start of next
//              LONG    cjSrc       Number of bytes to be output on every scan
//              LONG    cScans      Number of scans
//              ULONG   ulCmd       Accelerator command
//              ULONG   ulPadSize   count of dwords to pad with dummy data
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID S3DImageTransferMm32(  // Type FNIMAGETRANSFER
PDEV*   ppdev,
BYTE*   pjSrc,              // Source pointer
LONG    lDelta,             // Delta from start of scan to start of next
LONG    cjSrc,              // Number of bytes to be output on every scan
LONG    cScans,             // Number of scans
ULONG   ulCmd,              // Accelerator command - shouldn't include bus size
ULONG   ulPadSize)          // count of dwords to pad with dummy data
{
    BYTE* pjMmBase;

    ASSERTDD(cScans > 0, "Can't handle non-positive count of scans");


    pjMmBase = ppdev->pjMmBase;

    S3DGPWait(ppdev);
    S3writeHIU(ppdev, S3D_BLT_COMMAND, ulCmd);

//    CHECK_DATA_READY(ppdev);

    #if defined(i386)
    {
    LONG culSrc;

    _asm {
        mov     eax,cScans          ;eax = cScans
        mov     esi,pjSrc           ;esi = pjSrc
        mov     ebx,lDelta          ;ebx = lDelta
        mov     edx,cjSrc
        shr     edx,2               ;edx = culSrc = (cjSrc) >> 2 (# of
                                    ; entire dwords to be transferred)

        mov     culSrc,edx
        shl     edx,2
        sub     ebx,edx             ;ebx = lDelta - (culSrc * 4)
                                    ; (true source delta)

        ; We have to be sure that we don't read past the end of the
        ; bitmap, otherwise we'll access violate if the bitmap ends
        ; exactly at a page boundary.  We do this by having separate
        ; loops for every width alignment:

        mov     edx,cjSrc           ;unfortunately, we can't define jump
        and     edx,3               ; tables in inline ASM, so we use this
        jz      short Width0Loop    ; condition tree instead
        dec     edx
        jz      short Width1Loop
        dec     edx
        jz      short Width2Loop

        ; eax = Number of scans yet to do
        ; ebx = True source delta
        ; ecx = Per-scan dword count
        ; edx = Temporary storage
        ; esi = Source pointer
        ; edi = Pointer to data transfer memory-mapped I/O

    Width3Loop:
        mov     edi,pjMmBase
        mov     ecx,culSrc
        rep     movsd
        mov     dl,[esi+2]
        shl     edx,16
        mov     dx,[esi]
        mov     [edi],edx           ;don't care about high byte
        mov     ecx, ulPadSize      ;count of dummy dwords to send
        rep     stosd               ;send them to the chip
        add     esi,ebx
        dec     eax
        jnz     short Width3Loop
        jmp     AllDone

    Width2Loop:
        mov     edi,pjMmBase
        mov     ecx,culSrc
        rep     movsd
        mov     dx,[esi]
        mov     [edi],edx           ;don't care about high bytes
        mov     ecx, ulPadSize      ;count of dummy dwords to send
        rep     stosd               ;send them to the chip
        add     esi,ebx
        dec     eax
        jnz     short Width2Loop
        jmp     short AllDone

    Width1Loop:
        mov     edi,pjMmBase
        mov     ecx,culSrc
        rep     movsd
        mov     dl,[esi]
        mov     [edi],edx           ;don't care about high bytes
        mov     ecx, ulPadSize      ;count of dummy dwords to send
        rep     stosd               ;send them to the chip
        add     esi,ebx
        dec     eax
        jnz     short Width1Loop
        jmp     short AllDone

    Width0Loop:
        mov     edi,pjMmBase
        mov     ecx,culSrc
        rep     movsd
        mov     ecx, ulPadSize      ;count of dummy dwords to send
        rep     stosd               ;send them to the chip
        add     esi,ebx
        dec     eax
        jnz     short Width0Loop

    AllDone:

        }
    }
    #else // !defined(i386)
    {
    LONG    cdSrc;
    LONG    cjEnd;
    ULONG   d;

        cdSrc = cjSrc >> 2;
        cjEnd = cdSrc << 2;

        switch (cjSrc & 3)
        {
        case 3:
            do
            {
                if (cdSrc > 0)
                {
                    MM_TRANSFER_DWORD(ppdev, pjMmBase, pjSrc, cdSrc, 0);
                }

                d = (ULONG) (*(pjSrc + cjEnd))          |
                            (*(pjSrc + cjEnd + 1) << 8) |
                            (*(pjSrc + cjEnd + 2) << 16);

                MM_TRANSFER_DWORD(ppdev, pjMmBase, &d, 1, 1);

                if ( ulPadSize > 0)
                {
                    MM_TRANSFER_REP_DWORD( ppdev, pjMmBase, &d, ulPadSize, 2);
                }

                pjSrc += lDelta;

            } while (--cScans != 0);
            break;

        case 2:
            do
            {
                if (cdSrc > 0)
                {
                    MM_TRANSFER_DWORD(ppdev, pjMmBase, pjSrc, cdSrc, 3);
                }

                d = (ULONG) (*(pjSrc + cjEnd))          |
                    (*(pjSrc + cjEnd + 1) << 8);

                MM_TRANSFER_DWORD(ppdev, pjMmBase, &d, 1, 4);

                if ( ulPadSize > 0)
                {
                    MM_TRANSFER_REP_DWORD( ppdev, pjMmBase, &d, ulPadSize, 5 );
                }

                pjSrc += lDelta;

            } while (--cScans != 0);
            break;

        case 1:
            do
            {
                if (cdSrc > 0)
                {
                    MM_TRANSFER_DWORD(ppdev, pjMmBase, pjSrc, cdSrc, 6);
                }

                d = (ULONG) (*(pjSrc + cjEnd));
                MM_TRANSFER_DWORD(ppdev, pjMmBase, &d, 1, 7);

                if ( ulPadSize > 0)
                {
                    MM_TRANSFER_REP_DWORD( ppdev, pjMmBase, &d, ulPadSize, 8 );
                }

                pjSrc += lDelta;

            } while (--cScans != 0);
            break;

        case 0:
            do
            {
                MM_TRANSFER_DWORD(ppdev, pjMmBase, pjSrc, cdSrc, 9);

                if ( ulPadSize > 0)
                {
                    MM_TRANSFER_REP_DWORD( ppdev, pjMmBase, &d, ulPadSize, 10 );
                }

                pjSrc += lDelta;

            } while (--cScans != 0);
            break;
        }
    }
    #endif

} // end S3DImageTransferMm32 ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmFillSolid3D:
//
// DESCRIPTION: Fills a list of rectangles with a solid color using the
//              graphic engine's BitBlt function.
//
// ASSUMPTIONS: The rectangle has been clipped within the calling function.
//
// CALLS:       none
//
// PARAMETERS:  PDEV*           ppdev       pointer to display's pdevice
//              OH*             poh         destination heap object
//              LONG            c           rectangle count, can't be zero
//              RECTL*          prcl        Rectangle list in relative coords
//              ULONG           ulHwForeMix Hardware mix mode
//              ULONG           ulHwBackMix Not used
//              RBRUSH_COLOR    rbc         Drawing colour is rbc.iSolidColor
//              POINTL*         pptlBrush   Not used
//
// RETURN:      none
//
// NOTES:       None
//
VOID vMmFillSolid3D (           // Type FNFILL
PDEV*           ppdev,          // Destination PDEV struct ptr
OH*             poh,            // destination heap object
LONG            c,              // Can't be zero
RECTL*          prcl,           // Rect list to be filled, relative coordinates
ULONG           ulHwForeMix,    // Hardware mix mode
ULONG           ulHwBackMix,    // Not used
RBRUSH_COLOR    rbc,            // Drawing colour is rbc.iSolidColor
POINTL*         pptlBrush)      // Not used
{
    ULONG   ulEngDrawCmd;
    ULONG   ulDstBaseAddr;
    ULONG   ulDstSrcStride;

    ASSERTDD(c > 0, "Can't handle zero rectangles");
//
// Setup the drawing variables before allocating the engine FIFO slots
//
    ulEngDrawCmd = ppdev->ulCommandBase           |
                   S3D_BITBLT                     |
                   S3D_MONOCHROME_PATTERN         |
                   (ulHwForeMix << S3D_ROP_SHIFT) |
                   S3D_X_POSITIVE_BLT             |
                   S3D_Y_POSITIVE_BLT             |
                   S3D_DRAW_ENABLE;

    ulDstBaseAddr  = poh->ulBase;
    ulDstSrcStride = ((poh->lDelta << 16) | poh->lDelta);

//
// NOTE: Although not listed in the Virge stepping information/errata, a wait
//       for engine idle is necessary (else the HCT GuiJr tests fail).
//
    S3DGPWait (ppdev);

//
// Set destination base address/stride for drawing operation
//
    S3writeHIU (ppdev, S3D_BLT_DESTINATION_BASE,          ulDstBaseAddr);
    S3writeHIU (ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, ulDstSrcStride);

//
// If solid pattern black or white PATCOPY, replace with WHITENESS/BLACKNESS
//  ROP code to eliminate the need to program the fgnd/bgnd color registers.
// If solid pattern, set foreground/background colors to same value (so there's
//  no need to load the monochrome pattern registers 0/1).
//
    if (ulHwForeMix == PAT_COPY && ((rbc.iSolidColor == ppdev->ulWhite) ||
                                    (rbc.iSolidColor == COLOR_BLACK)))
    {
        ulEngDrawCmd &= (~S3D_CMD_ROP_MASK);    // default = ROP3_BLACKNESS
        if (rbc.iSolidColor != COLOR_BLACK)
        {
            ulEngDrawCmd |= (ROP3_WHITENESS << S3D_ROP_SHIFT);
        }
    }
    else if (gajRop3 [ulHwForeMix] & AVEC_NEED_PATTERN)
    {
        S3writeHIU (ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, rbc.iSolidColor);
        S3writeHIU (ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, rbc.iSolidColor);
    }

    do // while (--c != 0)
    {
        LONG  lRectWidth;               // Dest rectangle width,  0-based
        LONG  lRectHeight;              // Dest rectangle height, 0-based
        ULONG ulRectWidHgt;             // BitBlt rectangle width/height
        ULONG ulRectDestXY;             // BitBlt rectangle dest X,Y coords

        lRectWidth  = (prcl->right  - prcl->left);          // 1-based width
        lRectHeight = (prcl->bottom - prcl->top);           // 1-based height

        if ((lRectWidth > 0) && (lRectHeight > 0))  // if visible rect to fill
        {
            ulRectWidHgt = (((lRectWidth - 1) << 16) | lRectHeight);
            ulRectDestXY = ((prcl->left       << 16) | prcl->top);

            S3DFifoWait (ppdev, 3);     // wait for FIFO slots available

            S3writeHIU (ppdev, S3D_BLT_WIDTH_HEIGHT,    ulRectWidHgt);
            S3writeHIU (ppdev, S3D_BLT_DESTINATION_X_Y, ulRectDestXY);
            S3writeHIU (ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd);

        } // end if ((lRectWidth > 0) && (lRectHeight > 0))

        prcl++;                         // advance to next rectangle in list

    } while (--c != 0);

    return;

} // end vMmFillSolid3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmFastPatRealize3D:
//
// DESCRIPTION: This routine transfers an 8x8 pattern to off-screen display
//              memory, so that it can be used by the S3 pattern hardware.
//
// ASSUMPTIONS: None
//
// CALLS:       RtlCopyMemory()
//
// PARAMETERS:  PDEV*   ppdev           pointer to device's pdevice
//              OH*     poh,            pointer to heap object for destination
//              RBRUSH* prb,            pointer to brush realization structure
//              POINTL* pptlBrush,      Brush origin for aligning realization
//              BOOL    bTransparent    FALSE for normal patterns; TRUE for
//                                      patterns with a mask when the bkgrd
//                                      mix is LEAVE_ALONE.
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmFastPatRealize3D(       // Type FNFASTPATREALIZE
PDEV*   ppdev,
OH*     poh,
RBRUSH* prb,                    // Points to brush realization structure
POINTL* pptlBrush,              // Brush origin for aligning realization
BOOL    bTransparent)           // FALSE for normal patterns; TRUE for
                                //   patterns with a mask when the background
                                //   mix is LEAVE_ALONE.
{
    LONG        i;
    LONG        xShift;
    LONG        yShift;
    BYTE*       pjSrc;
    BYTE*       pjDst;
    LONG        cjLeft;
    LONG        cjRight;


    ASSERTDD(bTransparent == FALSE, "3D chips can't handle tranparent brushes");
//
// Because we handle only 8x8 brushes, it is easy to compute the
// number of pels by which we have to rotate the brush pattern
// right and down.  Note that if we were to handle arbitrary sized
// patterns, this calculation would require a modulus operation.
//
// The brush is aligned in absolute coordinates, so we have to add
// in the surface offset:
//
    xShift = pptlBrush->x;
    yShift = pptlBrush->y;

    prb->ptlBrushOrg.x = xShift;    // We have to remember the alignment
    prb->ptlBrushOrg.y = yShift;    //   that we used for caching (we check
                                    //   this when we go to see if a brush's
                                    //   cache entry is still valid)

    xShift &= 7;                    // Rotate pattern 'xShift' pels right
    yShift &= 7;                    // Rotate pattern 'yShift' pels down

//
// Since the 3D chips don't use an off-screen brush cache, we need to
// store the aligned brush in the RBRUSH structure.  In order to keep the
// structure as compact as possible for all color depths, we store the
// aligned copy of the brush right after the unaligned brush.  We use a
// pointer within the structure for easy access to the data.  This is where
// we set up the pointer to the aligned brush to point within the same
// structure
//
    prb->pulAligned = &prb->aulPattern[(TOTAL_BRUSH_SIZE / 4) * ppdev->cjPelSize];


    if (prb->fl & RBRUSH_2COLOR)
    {
    //
    // We're going to byte pack the monochrome pattern into the aligned
    // buffer for easy use later.  The 3D chips can directly utilize a
    // monochrome pattern by simply giving a foreground color, background
    // color, and two dwords of monochrome pattern data.
    //
        pjSrc = (BYTE*) &prb->aulPattern[0];    // Copy from the start of the
                                                //   brush buffer
        pjDst = (BYTE*) prb->pulAligned;        // Copy to our aligned buffer
        pjDst += yShift * sizeof(BYTE);         //   starting yShift rows down
        i = 8 - yShift;                         //   for 8 - yShift rows

        do
        {
            *pjDst = (*pjSrc >> xShift) | (*pjSrc << (8 - xShift));
            pjDst ++;               // Destination is byte packed
            pjSrc += sizeof(WORD);  // Source is word aligned

        } while (--i != 0);

        pjDst -= 8 * sizeof(BYTE);  // Move to the beginning of the source

        ASSERTDD(pjDst == (BYTE*) prb->pulAligned, "pjDst not back at start");

        for (; yShift != 0; yShift--)
        {
            *pjDst = (*pjSrc >> xShift) | (*pjSrc << (8 - xShift));
            pjDst ++;               // Destination is byte packed
            pjSrc += sizeof(WORD);  // Source is word aligned
        }
    }
    else
    {
        ASSERTDD(!bTransparent,
        "Shouldn't have been asked for transparency with a non-1bpp brush");

    //
    // Copy and align the brush while copying to the aligned portion of
    // the RBRUSH structure.
    //
        cjLeft  = (xShift * ppdev->cjPelSize);      // Number of bytes pattern
                                                    //   is shifted to the right
        cjRight = (8 * ppdev->cjPelSize) - cjLeft;  // Number of bytes pattern
                                                    //   is shifted to the left

        pjSrc = (BYTE*) &prb->aulPattern[0];        // Copy from brush buffer
        pjDst = (BYTE*) prb->pulAligned;            // Copy to our aligned buffer
        pjDst += yShift * (8 * ppdev->cjPelSize);   //  starting yShift rows
        i = 8 - yShift;                             //  down for 8 - yShift rows

        do {
            RtlCopyMemory(pjDst + cjLeft, pjSrc,           cjRight);
            RtlCopyMemory(pjDst,          pjSrc + cjRight, cjLeft);

            pjDst += cjLeft + cjRight;
            pjSrc += cjLeft + cjRight;

        } while (--i != 0);

        pjDst = (BYTE*) prb->pulAligned; // Move to the beginning of destination

        for (; yShift != 0; yShift--)
        {
            RtlCopyMemory(pjDst + cjLeft, pjSrc,           cjRight);
            RtlCopyMemory(pjDst,          pjSrc + cjRight, cjLeft);

            pjDst += cjLeft + cjRight;
            pjSrc += cjLeft + cjRight;
        }
    }
} // end vMmFastPatRealize3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmFillPatFast3D:
//
// DESCRIPTION: This routine uses the S3 pattern hardware to draw a patterned
//              list of rectangles.
//
// ASSUMPTIONS: The rectangle has been clipped within the calling function.
//
// CALLS:       vMmFastPatRealize3D()
//
// PARAMETERS:  PDEV*           ppdev       pointer to display's pdevice
//              OH*             poh         destination heap object
//              LONG            c           rectangle count, can't be zero
//              RECTL*          prcl        list of rectangles to be filled,
//                                          in relative coordinates
//              ULONG           ulHwForeMix Hardware mix mode (foreground mix
//                                          mode if the brush has a mask)
//              ULONG           ulHwBackMix Not used (unless the brush has a
//                                          mask, in which case it's the
//                                          background mix mode)
//              RBRUSH_COLOR    rbc         rbc.prb points to brush realization
//                                          structure
//              POINTL*         pptlBrush   Pattern alignment
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmFillPatFast3D(          // Type FNFILL
PDEV*           ppdev,          // Destination PDEV device structure ptr
OH*             poh,            // destination heap object
LONG            c,              // Can't be zero
RECTL*          prcl,           // Rectangle fill list, in relative coordinates
ULONG           ulHwForeMix,    // HW mix mode (fgnd mix mode if brush has mask)
ULONG           bTransparency,  // Transparency (TRUE or FALSE)
RBRUSH_COLOR    rbc,            // rbc.prb points to brush realization structure
POINTL*         pptlBrush)      // Pattern alignment
{
    RBRUSH* prb;
    ULONG   ulEngDrawCmd;       // Engine pattern fill draw command
    ULONG   ulDstBaseAddr;      // Destination surface base address
    ULONG   ulDstSrcStride;     // Destination surface stride (width in pixels)

    ASSERTDD (c > 0, "MFPF3D: Can't handle zero rectangles");
//    ASSERTDD ( !bTransparency,
//              "vMmFillPatFast: Can't deal with transparency");

//
// The S3's pattern hardware requires that we load an aligned copy of the
// brush onto the chip.  We have to update this realization if any of the
// following are true:
//
//   1) The brush alignment has changed;
//
// To handle the initial realization of a pattern, we're a little tricky in
// order to save an 'if' in the following expression.  In DrvRealizeBrush,
// we set 'prb->ptlBrushOrg.x' to be 0x80000000 (a very negative number),
// which is guaranteed not to equal 'pptlBrush->x + poh->x'.  So our check
// for brush alignment will also handle the initialization case.
//
    prb = rbc.prb;

    if ((prb->ptlBrushOrg.x != pptlBrush->x + poh->x) ||
        (prb->ptlBrushOrg.y != pptlBrush->y + poh->y))
    {
        vMmFastPatRealize3D (ppdev, poh, prb, pptlBrush, FALSE);
    }

    ulEngDrawCmd = (ppdev->ulCommandBase           |
                    S3D_BITBLT                     |
                    (ulHwForeMix << S3D_ROP_SHIFT) |
                    S3D_X_POSITIVE_BLT             |
                    S3D_Y_POSITIVE_BLT             |
                    S3D_DRAW_ENABLE);

    ulDstBaseAddr  = poh->ulBase;
    ulDstSrcStride = ((poh->lDelta << 16) | poh->lDelta);

    if (prb->fl & RBRUSH_2COLOR)            // Monochrome brush pattern fill
    {
        ULONG   ulFgndColor;
        ULONG   ulBgndColor;
        ULONG   ulMonoPatt0;
        ULONG   ulMonoPatt1;

        ulFgndColor = prb->ulForeColor;      // preload colors and mono pattern
        ulBgndColor = prb->ulBackColor;
        ulMonoPatt0 = prb->pulAligned [0];
        ulMonoPatt1 = prb->pulAligned [1];
        ulEngDrawCmd |= S3D_MONOCHROME_PATTERN;

        S3DFifoWait (ppdev, 4);         // wait for FIFO slots available

        S3writeHIU (ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, ulFgndColor);
        S3writeHIU (ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, ulBgndColor);
        S3writeHIU (ppdev, S3D_BLT_MONO_PATTERN_0,           ulMonoPatt0);
        S3writeHIU (ppdev, S3D_BLT_MONO_PATTERN_1,           ulMonoPatt1);
    }
//
// Copy the color pattern image into the graphics engine
//
    else // !(prb->fl & RBRUSH_2COLOR)      // Color brush pattern fill
    {
        PULONG  pulSrcPattAddr;
        PBYTE   pjDstImageXfer;
        ULONG   ulPattXferDwords;

        pulSrcPattAddr   = prb->pulAligned;
        pjDstImageXfer   = (ppdev->pjMmBase + S3D_COLOR_PATTERN_DATA);
        ulPattXferDwords = (ppdev->cAlignedBrushSize / 4);

        S3DGPWait (ppdev);          // wait for all FIFO slots free

        MM_TRANSFER_DWORD (ppdev, pjDstImageXfer, pulSrcPattAddr,
                           ulPattXferDwords, 10);

    } // end else !(prb->fl & RBRUSH_2COLOR)

//
// Set the base address/stride for the destination surface
//
    S3DFifoWait (ppdev, 2);

    S3writeHIU (ppdev, S3D_BLT_DESTINATION_BASE,          ulDstBaseAddr);
    S3writeHIU (ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, ulDstSrcStride);

    do // while (--c != 0)
    {
        LONG  lRectWidth;
        LONG  lRectHeight;
        ULONG ulRectWidHgt;
        ULONG ulRectDestXY;

        lRectWidth  = (prcl->right - prcl->left);           // 1-based width
        lRectHeight = (prcl->bottom - prcl->top);           // 1-based height

        if ((lRectWidth > 0) && (lRectHeight > 0))  // if visible rect to fill
        {
            ulRectWidHgt = (((lRectWidth - 1) << 16) | lRectHeight);
            ulRectDestXY = ((prcl->left       << 16) | prcl->top);

            S3DFifoWait (ppdev, 3);     // wait for FIFO slots available

            S3writeHIU (ppdev, S3D_BLT_WIDTH_HEIGHT,    ulRectWidHgt);
            S3writeHIU (ppdev, S3D_BLT_DESTINATION_X_Y, ulRectDestXY);
            S3writeHIU (ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd);

        } // end if ((lRectWidth > 0) && (lRectHeight > 0))

        prcl++;                         // advance to next rectangle in list

    } while (--c != 0);

    return;

} // end vMmFillPatFast3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmXfer1bpp:
//
// DESCRIPTION: This routine colours expands a monochrome bitmap, possibly
//              with different Rop2's for the foreground and background.  It
//              will be called in the following cases:
//
//              1) To colour-expand the monochrome text buffer for the
//                 vFastText routine.
//              2) To blt a 1bpp source with a simple Rop2 between the source
//                 and destination.
//              3) To blt a true Rop3 when the source is a 1bpp bitmap that
//                 expands to white and black, and the pattern is a solid
//                 colour.
//              4) To handle a true Rop4 that works out to be two Rop2's
//                 between the pattern and destination.
//
// ASSUMPTIONS: None
//
// CALLS:       S3DImageTransferMm32()
//
// PARAMETERS:  PDEV*       ppdev       pointer to display's pdevice
//              OH*         poh         destination heap object
//              LONG        c           Count of rectangles, can't be zero
//              RECTL*      prcl        List of destination rectangles, in
//                                      relative coordinates
//              ULONG       ulHwForeMix Foreground hardware mix
//              ULONG       ulHwBackMix Background hardware mix
//              SURFOBJ*    psoSrc      Source surface
//              POINTL*     pptlSrc     Original unclipped source point
//              RECTL*      prclDst     Original unclipped dest rectangle
//              XLATEOBJ*   pxlo        Translate that provides color-expansion
//                                      information
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmXfer1bpp3D(       // Type FNXFER
PDEV*       ppdev,
OH*         poh,        // destination heap object
LONG        c,          // Count of rectangles, can't be zero
RECTL*      prcl,       // List of destination rectangles, in relative
                        //   coordinates
ULONG       ulHwForeMix,// Foreground hardware mix
ULONG       ulHwBackMix,// Background hardware mix
SURFOBJ*    psoSrc,     // Source surface
POINTL*     pptlSrc,    // Original unclipped source point
RECTL*      prclDst,    // Original unclipped destination rectangle
XLATEOBJ*   pxlo)       // Translate that provides colour-expansion information
{
    LONG    dxSrc;
    LONG    dySrc;
    LONG    cx;
    LONG    cy;
    LONG    lSrcDelta;
    BYTE*   pjSrcScan0;
    BYTE*   pjSrc;
    LONG    cjSrc;
    LONG    xLeft;
    LONG    xRight;
    LONG    yTop;
    LONG    xBias;
    ULONG   ulCommand;
    BYTE*   pjMmBase = ppdev->pjMmBase;
        ULONG   ulPad;

    ASSERTDD(c > 0, "Can't handle zero rectangles");
    ASSERTDD(pptlSrc != NULL && psoSrc != NULL, "Can't have NULL sources");

    ulCommand = ppdev->ulCommandBase        |
                S3D_BITBLT                  |
                ulHwForeMix << S3D_ROP_SHIFT|
                S3D_DWORD_ALIGNED           |
                S3D_HARDWARE_CLIPPING       |
                S3D_MONOCHROME_SOURCE       |
                S3D_CPU_DATA                |
                S3D_X_POSITIVE_BLT          |
                S3D_Y_POSITIVE_BLT          |
                S3D_DRAW_ENABLE;

    if (ulHwForeMix != ulHwBackMix)
    {
        ulCommand |= S3D_TRANSPARENT;
    }

    dxSrc = pptlSrc->x - prclDst->left;
    dySrc = pptlSrc->y - prclDst->top;      // Add to destination to get source

    lSrcDelta  = psoSrc->lDelta;
    pjSrcScan0 = psoSrc->pvScan0;

    S3DGPWait(ppdev);

//
// Set the base address for the destination
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_BASE, poh->ulBase);

//
// Set the source/destination stride
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
//
// Set the top and bottom clipping
//
    S3writeHIU(ppdev, S3D_BLT_CLIP_TOP_BOTTOM, 0x000007FFL);

//
// Set the foreground and background colors
//
    S3writeHIU(ppdev, S3D_BLT_SOURCE_FOREGROUND_COLOR, pxlo->pulXlate[1]);
    S3writeHIU(ppdev, S3D_BLT_SOURCE_BACKGROUND_COLOR, pxlo->pulXlate[0]);

    do
    {
    //
    // We'll byte align to the source, but do word transfers
    // (implying that we may be doing unaligned reads from the
    // source).  We do this because it may reduce the total
    // number of word outs/writes that we'll have to do to the
    // display:
    //
        yTop   = prcl->top;
        xLeft  = prcl->left;
        xRight = prcl->right;

        xBias = (xLeft + dxSrc) & 7;        // This is the byte-align bias

        S3DFifoWait(ppdev, 4);

        S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT, (xLeft << 16) | xRight-1);

        xLeft -= xBias;

        cx = (xRight  - xLeft +7) & ~7;
//        cx = xRight  - xLeft - 1; //can't do this  cause cr22298.
        cy = prcl->bottom - yTop;

        cjSrc = (cx + 7) >> 3;

                //
                // Temporary fix for the first pass ViRGE silicon
                //
        // We must set the low order 5 bits of the width.
        // Thus we may need to do up to one additional DWORD write.
        //
        // If none of the low order 5 bits (cx) were set, then we need to
        // write out an additional DWORD.  Otherwise our standard DWORD
        // padding will take care of this for us.
        //

        ulPad = (cx & 0x1F) ? 0 : 1;

        cx |= 0x1F;

        S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT, ((cx) << 16) | cy);

        S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y, (xLeft << 16) | yTop);

        pjSrc = pjSrcScan0 + (yTop  + dySrc) * lSrcDelta
                   + ((xLeft + dxSrc) >> 3);
                        // Start is byte aligned (note
                        //   that we don't have to add
                        //   xBias)

        S3DImageTransferMm32(ppdev, pjSrc, lSrcDelta, cjSrc, cy, ulCommand, ulPad);

        prcl++;

    } while (--c != 0);

} // end vMmXfer1bpp3D ()


//----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmPatXfer1bpp:
//
// DESCRIPTION: This routine color expands a monochrome bitmap, possibly with
//              different Rop3's for the foreground and background.  It will
//              be called in the following cases:
//
//              1) To colour-expand the monochrome text buffer for the
//                 vFastText routine.
//              2) To blt a 1bpp source with a Rop3 between the source, pattern
//                 and destination.
//              3) To blt a true Rop3 when the source is a 1bpp bitmap that
//                 expands to white and black, and the pattern is a solid colour.
//
// ASSUMPTIONS: None
//
// CALLS:       S3DImageTransferMm32()
//              vMmFastPatRealize3D()
//
// PARAMETERS:  PDEV*       ppdev       pointer to display's pdevice
//              OH*         poh         destination heap object
//              LONG        c           Count of rectangles, can't be zero
//              RECTL*      prcl        List of destination rectangles, in
//                                      relative coordinates
//              ULONG       ulHwForeMix Foreground hardware mix
//              ULONG       ulHwBackMix Background hardware mix
//              SURFOBJ*    psoSrc      Source surface
//              POINTL*     pptlSrc     Original unclipped source point
//              RECTL*      prclDst     Original unclipped dest rectangle
//              XLATEOBJ*   pxlo        Translate that provides colour-
//                                      expansion information
//              BRUSHOBJ*   pbo         pointer brush object
//              POINTL*     pptlBrush   Pattern alignment
//
// RETURN:      None
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmPatXfer1bpp3D(  // Type FNPATXFER
PDEV*       ppdev,
OH*         poh,        // destination heap object
LONG        c,          // Count of rectangles, can't be zero
RECTL*      prcl,       // List of destination rectangles, in relative
                        //   coordinates
ULONG       ulHwForeMix,// Foreground hardware mix
ULONG       ulHwBackMix,// Background hardware mix
SURFOBJ*    psoSrc,     // Source surface
POINTL*     pptlSrc,    // Original unclipped source point
RECTL*      prclDst,    // Original unclipped destination rectangle
XLATEOBJ*   pxlo,       // Translate that provides colour-expansion information
BRUSHOBJ*   pbo,        // Brush to use
POINTL*     pptlBrush)  // Pattern alignment
{
    LONG    dxSrc;
    LONG    dySrc;
    LONG    cx;
    LONG    cy;
    LONG    lSrcDelta;
    BYTE*   pjSrcScan0;
    BYTE*   pjSrc;
    LONG    cjSrc;
    LONG    xLeft;
    LONG    xRight;
    LONG    yTop;
    LONG    xBias;
    ULONG   ulCommand;
    BYTE*   pjMmBase = ppdev->pjMmBase;
        ULONG   ulPad;

    ASSERTDD(c > 0, "Can't handle zero rectangles");
    ASSERTDD(pptlSrc != NULL && psoSrc != NULL, "Can't have NULL sources");

    ulCommand = ppdev->ulCommandBase        |
                S3D_BITBLT                  |
                ulHwForeMix << S3D_ROP_SHIFT|
                S3D_DWORD_ALIGNED           |
                S3D_HARDWARE_CLIPPING       |
                S3D_MONOCHROME_SOURCE       |
                S3D_CPU_DATA                |
                S3D_X_POSITIVE_BLT          |
                S3D_Y_POSITIVE_BLT          |
                S3D_DRAW_ENABLE;

    if (ulHwForeMix != ulHwBackMix)
    {
        ulCommand |= S3D_TRANSPARENT;
    }

    S3DGPWait(ppdev);

//
// Set the base address for the destination
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_BASE, poh->ulBase);

//
// Set the source/destination stride
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
//
// Set the top and bottom clipping
//
    S3writeHIU(ppdev, S3D_BLT_CLIP_TOP_BOTTOM, 0x000007FFL);

//
// Set the foreground and background colors
//
    S3writeHIU(ppdev, S3D_BLT_SOURCE_FOREGROUND_COLOR, pxlo->pulXlate[1]);
    S3writeHIU(ppdev, S3D_BLT_SOURCE_BACKGROUND_COLOR, pxlo->pulXlate[0]);

    dxSrc = pptlSrc->x - prclDst->left;
    dySrc = pptlSrc->y - prclDst->top;      // Add to destination to get source

    lSrcDelta  = psoSrc->lDelta;
    pjSrcScan0 = psoSrc->pvScan0;

//
// Take care of the pattern
//
    if (pbo->iSolidColor != -1)
    {
    //
    // Its a solid pattern
    //
        ulCommand |= S3D_MONOCHROME_PATTERN;
        S3DFifoWait(ppdev, 3);
        S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, pbo->iSolidColor);
        S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, pbo->iSolidColor);
    }
    else
    {
        RBRUSH* prb;

        prb = pbo->pvRbrush;

    //
    // The S3's pattern hardware requires that we laod an aligned copy
    // of the brush onto the chip.  We have to update this
    // realization if any of the following are true:
    //
    //   1) The brush alignment has changed;
    //
    // To handle the initial realization of a pattern, we're a little
    // tricky in order to save an 'if' in the following expression.  In
    // DrvRealizeBrush, we set 'prb->ptlBrushOrg.x' to be 0x80000000 (a
    // very negative number), which is guaranteed not to equal 'pptlBrush->x
    // + poh->x'.  So our check for brush alignment will also
    // handle the initialization case.
    //
        if ((prb->ptlBrushOrg.x != pptlBrush->x + poh->x) ||
            (prb->ptlBrushOrg.y != pptlBrush->y + poh->y))
        {
            vMmFastPatRealize3D(ppdev, poh, prb, pptlBrush, FALSE);
        }

        if (prb->fl & RBRUSH_2COLOR)
        {
            ULONG* Src;

        //
        // Its a monochrome pattern.  Send the colors and pattern data to the
        // chip.
        //
            ulCommand |= S3D_MONOCHROME_PATTERN;

            Src = prb->pulAligned;

            S3DFifoWait(ppdev, 5);

            S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, prb->ulForeColor);
            S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, prb->ulBackColor);
            S3writeHIU(ppdev, S3D_BLT_MONO_PATTERN_0, *Src++);
            S3writeHIU(ppdev, S3D_BLT_MONO_PATTERN_1, *Src);

        }
        else
        {
            ULONG  i;
            ULONG  Dst;
            ULONG* Src;

        //
        // Its a color pattern.  Send the color data to the chip
        //
            Dst = S3D_COLOR_PATTERN_DATA;
            Src = prb->pulAligned;

            S3DGPWait(ppdev);
            for (i = ppdev->cAlignedBrushSize / 4; i != 0; i--)
            {

                S3writeHIU(ppdev, Dst, *Src++);
                Dst += 4;

            }
        }
    }

    do
    {
    // We'll byte align to the source, but do word transfers
    // (implying that we may be doing unaligned reads from the
    // source).  We do this because it may reduce the total
    // number of word outs/writes that we'll have to do to the
    // display:
    //
        yTop   = prcl->top;
        xLeft  = prcl->left;
        xRight = prcl->right;

        xBias = (xLeft + dxSrc) & 7;        // This is the byte-align bias

        S3DFifoWait(ppdev, 4);

        S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT, (xLeft << 16) | xRight-1);

        xLeft -= xBias;

        cx = xRight  - xLeft;
        cy = prcl->bottom - yTop;

        cjSrc = (cx + 7) >> 3;

                //
                // Temporary fix for the first pass ViRGE silicon
                //
        // We must set the low order 5 bits of the width.
        // Thus we may need to do up to one additional DWORD write.
        //
        // If none of the low order 5 bits (cx) were set, then we need to
        // write out an additional DWORD.  Otherwise our standard DWORD
        // padding will take care of this for us.
        //

        ulPad = (cx & 0x1F) ? 0 : 1;

        cx |= 0x1F;


        S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT, ((cx) << 16) | cy);

        S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y, (xLeft << 16) | yTop);

        pjSrc = pjSrcScan0 + (yTop  + dySrc) * lSrcDelta
                   + ((xLeft + dxSrc) >> 3);
                        // Start is byte aligned (note that we don't have to
                        //  add xBias)

        S3DImageTransferMm32(ppdev, pjSrc, lSrcDelta, cjSrc, cy, ulCommand, ulPad);

        prcl++;

    } while (--c != 0);

} // end vMmPatXfer1bpp3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmXferNative3D:
//
// DESCRIPTION: Transfers a bitmap that is the same colour depth as the
//              display to the screen via the data transfer register, with no
//              translation.
//
// ASSUMPTIONS: None
//
// CALLS:       S3DImageTransferMm32()
//
// PARAMETERS:  PDEV*       ppdev       pointer to display's pdevice
//              OH*         poh         destination heap object
//              LONG        c           Count of rectangles, can't be zero
//              RECTL*      prcl        Array of relative coordinate dest
//                                      rectangles
//              ULONG       ulHwForeMix Hardware mix
//              ULONG       ulHwBackMix Not used
//              SURFOBJ*    psoSrc      Source surface
//              POINTL*     pptlSrc     Original unclipped source point
//              RECTL*      prclDst     Original unclipped dest rectangle
//              XLATEOBJ*   pxlo        Not used
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmXferNative3D(   // Type FNXFER
PDEV*       ppdev,
OH*         poh,        // destination heap object
LONG        c,          // Count of rectangles, can't be zero
RECTL*      prcl,       // Array of relative coordinates destination rectangles
ULONG       ulHwForeMix,// Hardware mix
ULONG       ulHwBackMix,// Not used
SURFOBJ*    psoSrc,     // Source surface
POINTL*     pptlSrc,    // Original unclipped source point
RECTL*      prclDst,    // Original unclipped destination rectangle
XLATEOBJ*   pxlo)       // Not used
{
    LONG    dx;
    LONG    dy;
    LONG    cx;
    LONG    cy;
    LONG    lSrcDelta;
    BYTE*   pjSrcScan0;
    BYTE*   pjSrc;
    LONG    cjSrc;
    ULONG   ulCommand;
    ULONG   yTop;
    ULONG   xLeft;
    ULONG   xRight;

    ASSERTDD((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL),
            "Can handle trivial xlate only");
    ASSERTDD(psoSrc->iBitmapFormat == ppdev->iBitmapFormat,
            "Source must be same colour depth as screen");
    ASSERTDD(c > 0, "Can't handle zero rectangles");

    ASSERTDD(ulHwForeMix == ulHwBackMix, "S3D engines can't do ROP4's.");

    ulCommand = ppdev->ulCommandBase        |
                S3D_BITBLT                  |
                ulHwForeMix << S3D_ROP_SHIFT|
                S3D_DWORD_ALIGNED           |
                S3D_CPU_DATA                |
                S3D_X_POSITIVE_BLT          |
                S3D_Y_POSITIVE_BLT          |
                S3D_DRAW_ENABLE;

    dx = pptlSrc->x - prclDst->left;
    dy = pptlSrc->y - prclDst->top;     // Add to destination to get source

    lSrcDelta  = psoSrc->lDelta;
    pjSrcScan0 = psoSrc->pvScan0;

    S3DGPWait(ppdev);
//
// Set the base address for the destination
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_BASE, poh->ulBase);

//
// Set the source/destination stride
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
//
// Set the top and bottom clipping
//
    S3writeHIU(ppdev, S3D_BLT_CLIP_TOP_BOTTOM, 0x000007FFL);

    do
    {
        ULONG ulClip;
        ULONG TransferBytes;
        ULONG LastSpan;
        ULONG TotalPixels;
        ULONG DwordPad;
    //
    // We'll byte align to the source, but do word transfers
    // (implying that we may be doing unaligned reads from the
    // source).  We do this because it may reduce the total
    // number of word outs/writes that we'll have to do to the
    // display:
    //
        yTop   = prcl->top;
        xLeft  = prcl->left;
        xRight = prcl->right;

        cx = xRight  - xLeft;
        cy = prcl->bottom - yTop;

    //
    // Bug fix for ViRGE -- #A-30 in SI017-G //SNH
    //
        TransferBytes = ((cx * ppdev->cjPelSize) + 3) & 0xFFFFFFFCL;
        LastSpan = TransferBytes % 64;
        if (LastSpan <=  16)
        {

            DwordPad = (24 - LastSpan) >> 2;
            TotalPixels = (TransferBytes + (24 - LastSpan)) / ppdev->cjPelSize;
            ulClip = S3D_HARDWARE_CLIPPING;

            S3DFifoWait(ppdev, 4);
            S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT, (xLeft << 16) | xRight-1);
            S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                      ((TotalPixels - 1) << 16) | cy);

        }
        else
        {
            ulClip = 0;
            DwordPad = 0;
            S3DFifoWait(ppdev, 3);
            S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                      ((cx - 1) << 16) | cy);
        }


        S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y, (xLeft << 16) | yTop);

        cjSrc = cx * ppdev->cjPelSize;
        pjSrc = pjSrcScan0 + (prcl->top  + dy) * lSrcDelta
                   + ((prcl->left + dx) * ppdev->cjPelSize);

        S3DImageTransferMm32(ppdev, pjSrc, lSrcDelta, cjSrc, cy,
                                ulCommand | ulClip, DwordPad);

        prcl++;

    } while (--c);

} // end vMmXferNative3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmPatXferNative3D:
//
// DESCRIPTION: Transfers a bitmap that is the same colour depth as the
//              display to the screen via the data transfer register, with
//              no translation.
//
// ASSUMPTIONS: None
//
// CALLS:       S3DImageTransferMm32()
//              vMmFastPatRealize3D()
//
// PARAMETERS:  PDEV*       ppdev       pointer to display's pdevice
//              OH*         poh         destination heap object
//              LONG        c           Count of rectangles, can't be zero
//              RECTL*      prcl        Array of relative coordinates
//                                      destination rectangles
//              ULONG       ulHwForeMix Hardware mix
//              ULONG       ulHwBackMix Not used
//              SURFOBJ*    psoSrc      Source surface
//              POINTL*     pptlSrc     Original unclipped source point
//              RECTL*      prclDst     Original unclipped dest rectangle
//              XLATEOBJ*   pxlo        Not used
//              BRUSHOBJ*   pbo         pointer to brush object
//              POINTL*     pptlBrush   Pattern alignment
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmPatXferNative3D(// Type FNPATXFER
PDEV*       ppdev,
OH*         poh,        // destination heap object
LONG        c,          // Count of rectangles, can't be zero
RECTL*      prcl,       // Array of relative coordinates destination rectangles
ULONG       ulHwForeMix,// Hardware mix
ULONG       ulHwBackMix,// Not used
SURFOBJ*    psoSrc,     // Source surface
POINTL*     pptlSrc,    // Original unclipped source point
RECTL*      prclDst,    // Original unclipped destination rectangle
XLATEOBJ*   pxlo,       // Not used
BRUSHOBJ*   pbo,        // Brush to use
POINTL*     pptlBrush)  // Pattern alignment
{
    LONG    dx;
    LONG    dy;
    LONG    cx;
    LONG    cy;
    LONG    lSrcDelta;
    BYTE*   pjSrcScan0;
    BYTE*   pjSrc;
    LONG    cjSrc;
    ULONG   ulCommand;
    ULONG   yTop;
    ULONG   xLeft;
    ULONG   xRight;

    ASSERTDD((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL),
            "Can handle trivial xlate only");
    ASSERTDD(psoSrc->iBitmapFormat == ppdev->iBitmapFormat,
            "Source must be same colour depth as screen");
    ASSERTDD(c > 0, "Can't handle zero rectangles");

    ASSERTDD(ulHwForeMix == ulHwBackMix, "S3D engines can't do ROP4's.");

    ulCommand = ppdev->ulCommandBase        |
                S3D_BITBLT                  |
                ulHwForeMix << S3D_ROP_SHIFT|
                S3D_DWORD_ALIGNED           |
                S3D_CPU_DATA                |
                S3D_X_POSITIVE_BLT          |
                S3D_Y_POSITIVE_BLT          |
                S3D_DRAW_ENABLE;


    S3DFifoWait(ppdev, 3);
//
// Set the base address for the destination
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_BASE, poh->ulBase);

//
// Set the source/destination stride
//
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, poh->lDelta << 16);
//
// Take care of the pattern
//
    if (pbo->iSolidColor != -1)
    {
    //
    // Its a solid pattern
    //
        ulCommand |= S3D_MONOCHROME_PATTERN;
        S3DFifoWait(ppdev, 3);
        S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, pbo->iSolidColor);
        S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, pbo->iSolidColor);
    }
    else
    {
        RBRUSH* prb;

        prb = pbo->pvRbrush;
    //
    // The S3's pattern hardware requires that we load an aligned copy
    // of the brush onto the chip.  We have to update this
    // realization if any of the following are true:
    //
    //   1) The brush alignment has changed;
    //
    // To handle the initial realization of a pattern, we're a little
    // tricky in order to save an 'if' in the following expression.  In
    // DrvRealizeBrush, we set 'prb->ptlBrushOrg.x' to be 0x80000000 (a
    // very negative number), which is guaranteed not to equal 'pptlBrush->x
    // + poh->x'.  So our check for brush alignment will also
    // handle the initialization case.
    //
        if ((prb->ptlBrushOrg.x != pptlBrush->x + poh->x) ||
            (prb->ptlBrushOrg.y != pptlBrush->y + poh->y))
        {
            vMmFastPatRealize3D(ppdev, poh, prb, pptlBrush, FALSE);
        }

        if (prb->fl & RBRUSH_2COLOR)
        {
            ULONG* Src;

        //
        // Its a monochrome pattern.  Send the colors and pattern data to the
        // chip.
        //

            ulCommand |= S3D_MONOCHROME_PATTERN;

            Src = prb->pulAligned;

            S3DFifoWait(ppdev, 5);

            S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, prb->ulForeColor);
            S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, prb->ulBackColor);
            S3writeHIU(ppdev, S3D_BLT_MONO_PATTERN_0, *Src++);
            S3writeHIU(ppdev, S3D_BLT_MONO_PATTERN_1, *Src);

        }
        else
        {
            ULONG  i;
            ULONG  Dst;
            ULONG* Src;

        //
        // Its a color pattern.  Send the color data to the chip
        //
            Dst = S3D_COLOR_PATTERN_DATA;
            Src = prb->pulAligned;

            S3DGPWait(ppdev);
            for (i = ppdev->cAlignedBrushSize / 4; i != 0; i--)
            {

                S3writeHIU(ppdev, Dst, *Src++);
                Dst += 4;

            }
        }
    }

    dx = pptlSrc->x - prclDst->left;
    dy = pptlSrc->y - prclDst->top;     // Add to destination to get source

    lSrcDelta  = psoSrc->lDelta;
    pjSrcScan0 = psoSrc->pvScan0;

    do
    {
        ULONG ulClip;
        ULONG TransferBytes;
        ULONG LastSpan;
        ULONG TotalPixels;
        ULONG DwordPad;
    //
    // We'll byte align to the source, but do word transfers
    // (implying that we may be doing unaligned reads from the
    // source).  We do this because it may reduce the total
    // number of word outs/writes that we'll have to do to the
    // display:
    //
        yTop   = prcl->top;
        xLeft  = prcl->left;
        xRight = prcl->right;

        cx = xRight  - xLeft;
        cy = prcl->bottom - yTop;

    //
    // Bug fix for ViRGE -- #A-30 in SI017-G //SNH
    //
        TransferBytes = ((cx * ppdev->cjPelSize) + 3) & 0xFFFFFFFCL;
        LastSpan = TransferBytes % 64;
        if (LastSpan <=  16)
        {

            DwordPad = (24 - LastSpan) >> 2;
            TotalPixels = (TransferBytes + (24 - LastSpan)) / ppdev->cjPelSize;
            ulClip = S3D_HARDWARE_CLIPPING;

            S3DFifoWait(ppdev, 6);
            S3writeHIU(ppdev, S3D_BLT_CLIP_TOP_BOTTOM, 0x00007FFF);
            S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT, (xLeft << 16) | xRight-1);
            S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                      ((TotalPixels - 1) << 16) | cy);

        }
        else
        {
            ulClip = 0;
            DwordPad = 0;
            S3DFifoWait(ppdev, 3);
            S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                      ((cx - 1) << 16) | cy);
        }

        S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y, (xLeft << 16) | yTop);


        cjSrc = cx * ppdev->cjPelSize;
        pjSrc = pjSrcScan0 + (prcl->top  + dy) * lSrcDelta
                   + ((prcl->left + dx) * ppdev->cjPelSize);

        S3DImageTransferMm32(ppdev, pjSrc, lSrcDelta, cjSrc, cy, ulCommand | ulClip,
                             DwordPad);
        prcl++;

    } while (--c != 0);

} // end vMmPatXferNative3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmCopyBlt3D:
//
// DESCRIPTION: Perform a screen-to-screen blt of a list of rectangles.
//
// ASSUMPTIONS: None
//
// CALLS:       None
//
// PARAMETERS:  PDEV*   ppdev   pointer to display's pdevice
//              OH*     pohDst  destination heap object
//              OH*     pohSrc  source heap object
//              LONG    c       rectangle count, can't be zero
//              RECTL*  prcl    Array of relative coordinates dest rectangles
//              ULONG   ulHwMix Hardware mix
//              POINTL* pptlSrc Original unclipped source point
//              RECTL*  prclDst Original unclipped destination rectangle
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmCopyBlt3D(  // Type FNCOPY
PDEV*   ppdev,      // Destination screen/offscreen PDEV
OH*     pohDst,     // destination heap object
OH*     pohSrc,     // source heap object
LONG    c,          // Can't be zero
RECTL*  prcl,       // Array of destination rectangles in relative coordinates
ULONG   ulHwMix,    // Hardware mix
POINTL* pptlSrc,    // Original unclipped source point
RECTL*  prclDst)    // Original unclipped destination rectangle
{
    ULONG   ulEngDrawCmd;               // Graphics engine draw cmd
    ULONG   ulSrcBaseAddr;              // Source surface base address
    ULONG   ulDstBaseAddr;              // Dest surface base address
    ULONG   ulDstSrcStride;             // Dest/Source stride
    ULONG   ulDstRectWidHgt;            // Dest rect width/height
    ULONG   ulSrcRectXY;                // Source rect X,Y coords
    ULONG   ulDstRectXY;                // Dest   rect X,Y coords
    ULONG   ulBltDirection;             // Bitblt source/dest X,Y direction
    LONG    lDstRectWidth;              // Dest rect width,  0-based/unclipped
    LONG    lDstRectHeight;             // Dest rect height, 0-based/unclipped
    RECTL   rclSrcRectAbs;              // Source rect absolute coordinates
    RECTL   rclDstRectAbs;              // Dest   rect absolute coordinates

    ASSERTDD(c > 0, "Can't handle zero rectangles");

//
// Setup the static screen to screen register contents
// NOTE: The Virge patch code below uses the left/right clipping register but
//       not the top/bottom clipping register.  The top/bottom register is set
//       to the maximum screen value and remains unchanged through the blt.
//
    ulEngDrawCmd = ppdev->ulCommandBase       |
                   S3D_BITBLT                 |
                   (ulHwMix << S3D_ROP_SHIFT) |
                   S3D_DRAW_ENABLE;

    ulSrcBaseAddr   = pohSrc->ulBase;
    ulDstBaseAddr   = pohDst->ulBase;

    ulDstSrcStride  = ((pohDst->lDelta << 16) | pohSrc->lDelta);

//
// Calculate the absolute source/destination rectangle X,Y coordinates (place
//  all points on one large rectangle to check for source/dest rect overlap)
//
    lDstRectWidth  = (prclDst->right  - prclDst->left - 1); // 0-based width
    lDstRectHeight = (prclDst->bottom - prclDst->top  - 1); // 0-based height

    rclSrcRectAbs.left   = (pohSrc->x + pptlSrc->x);
    rclSrcRectAbs.top    = (pohSrc->y + pptlSrc->y);
    rclSrcRectAbs.right  = (pohSrc->x + pptlSrc->x + lDstRectWidth);
    rclSrcRectAbs.bottom = (pohSrc->y + pptlSrc->y + lDstRectHeight);

    rclDstRectAbs.left   = (pohDst->x + prclDst->left);
    rclDstRectAbs.top    = (pohDst->y + prclDst->top);
    rclDstRectAbs.right  = (pohDst->x + prclDst->left + lDstRectWidth);
    rclDstRectAbs.bottom = (pohDst->y + prclDst->top  + lDstRectHeight);

//
// Setup the source->dest bitblt X,Y direction based on rectangle overlap
//
    if ((rclSrcRectAbs.left   > rclDstRectAbs.right)  ||  // no Src/Dst overlap
        (rclSrcRectAbs.right  < rclDstRectAbs.left)   ||
        (rclSrcRectAbs.top    > rclDstRectAbs.bottom) ||
        (rclSrcRectAbs.bottom < rclDstRectAbs.top))
    {
        ulBltDirection = (S3D_X_POSITIVE_BLT | S3D_Y_POSITIVE_BLT);
    }
    else    // source/destination rectangles overlap
    {
        ulBltDirection = (S3D_X_NEGATIVE_BLT | S3D_Y_NEGATIVE_BLT); // -X,-Y dir

        if (rclSrcRectAbs.left >= rclDstRectAbs.left)
        {
            ulBltDirection |= S3D_X_POSITIVE_BLT;
        }

        if (rclSrcRectAbs.top  >= rclDstRectAbs.top)
        {
            ulBltDirection |= S3D_Y_POSITIVE_BLT;
        }
    }

    ulEngDrawCmd  |= ulBltDirection;

//
// Set the source/destination surface base address/stride values
//
    S3DFifoWait (ppdev, 3);             // wait for FIFO slots available

    S3writeHIU (ppdev, S3D_BLT_SOURCE_BASE,               ulSrcBaseAddr);
    S3writeHIU (ppdev, S3D_BLT_DESTINATION_BASE,          ulDstBaseAddr);
    S3writeHIU (ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE, ulDstSrcStride);

//
// Process the screen to screen bitblt rectangle list
//
    do // while (--c != 0)
    {
        ULONG   ulSrcStartX;            // Source starting X coord
        ULONG   ulSrcStartY;            // Source starting Y coord
        ULONG   ulDstStartX;            // Dest   starting X coord
        ULONG   ulDstStartY;            // Dest   starting Y coord
        ULONG   ulClipLeftRight;        // Clip rect left/right boundary
        ULONG   ulClipTopBottom;        // Clip rect top/bottom boundary
        ULONG   ulClipRectWidth;        // Clip rect width (for Virge patch)

        ulSrcStartX = (pptlSrc->x + (prcl->left - prclDst->left)); // SrcX coord
        ulSrcStartY = (pptlSrc->y + (prcl->top  - prclDst->top));  // SrcY coord
        ulDstStartX = prcl->left;                   // Dest X+ coord (default)
        ulDstStartY = prcl->top;                    // Dest Y+ coord (default)

        lDstRectWidth  = (prcl->right - prcl->left - 1);   // 0-based width
        lDstRectHeight = (prcl->bottom - prcl->top - 1);   // 0-based height

        if (!(ulBltDirection & S3D_X_POSITIVE_BLT))  // -X blt direction
        {
            ulSrcStartX += lDstRectWidth;   // Src start X = rect.right edge
            ulDstStartX += lDstRectWidth;   // Dst start X = rect.right edge
        }

        if (!(ulBltDirection & S3D_Y_POSITIVE_BLT))  // -Y blt direction
        {
            ulSrcStartY += lDstRectHeight;  // Src start Y = rect.bottom edge
            ulDstStartY += lDstRectHeight;  // Dst start Y = rect.bottom edge
        }

#ifdef   VIRGE_PATCH12 //---------------
//
// Apply Virge source/dest bitblt patch (8 Qword Screen to Screen BitBlt)
//
         if ((lDstRectWidth <= ppdev->Virge_Patch12_MaxWidth) &&
             (lDstRectWidth >= ppdev->Virge_Patch12_MinWidth))
         {
            ulClipRectWidth = lDstRectWidth;
            lDstRectWidth   = ppdev->Virge_Patch12_BltWidth;

            ulEngDrawCmd |= S3D_HARDWARE_CLIPPING;

            ulClipLeftRight = ((prcl->left << 16) |
                               (prcl->left + ulClipRectWidth));
            ulClipTopBottom = 0x000007FFL;

            S3DFifoWait (ppdev, 2);     // wait for FIFO slots available

            S3writeHIU (ppdev, S3D_BLT_CLIP_LEFT_RIGHT, ulClipLeftRight);
            S3writeHIU (ppdev, S3D_BLT_CLIP_TOP_BOTTOM, ulClipTopBottom);
        }
        else // no VIRGE_PATCH12 required; disable HW clipping
        {
            ulEngDrawCmd &= (~S3D_HARDWARE_CLIPPING);
        }

#endif // VIRGE_PATCH12 //--------------

//
// Perform the screen to screen bitblt operation
// NOTE: The engine rectangle width is 0-based but the height is 1-based; the
//       height is adjusted here.
//
        ulSrcRectXY     = ((ulSrcStartX   << 16) | ulSrcStartY);
        ulDstRectXY     = ((ulDstStartX   << 16) | ulDstStartY);
        ulDstRectWidHgt = ((lDstRectWidth << 16) | (lDstRectHeight + 1));

        S3DFifoWait (ppdev, 4);         // wait for FIFO slots available

        S3writeHIU (ppdev, S3D_BLT_WIDTH_HEIGHT,    ulDstRectWidHgt);
        S3writeHIU (ppdev, S3D_BLT_SOURCE_X_Y,      ulSrcRectXY);
        S3writeHIU (ppdev, S3D_BLT_DESTINATION_X_Y, ulDstRectXY);
        S3writeHIU (ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd);

//
//  Added wait for engine idle here to fix a problem where we're holding the
//  PCI bus too long and causing PCI resets.
//
        S3DGPWait(ppdev);

        prcl++;                         // advance to next rectangle in list

    } while (--c != 0);

    return;

} // end vMmCopyBlt3D ()


//-----------------------------------------------------------------------------
// PROCEDURE:   VOID vMmPatCopyBlt3D:
//
// DESCRIPTION: Does a screen-to-screen blt of a list of rectangles.
//
// ASSUMPTIONS: None
//
// CALLS:       vMmFastPatRealize3D()
//
// PARAMETERS:  PDEV*       ppdev       pointer to display's pdevice
//              OH*         pohDst      destination heap object
//              OH*         pohSrc      source heap object
//              LONG        c           rectangle count, can't be zero
//              RECTL*      prcl        Array of relative coordinate dest
//                                      rectangles
//              ULONG       ulHwMix     Hardware mix
//              POINTL*     pptlSrc     Original unclipped source point
//              RECTL*      prclDst     Original unclipped destination rectangle
//              BRUSHOBJ*   pbo         pointer to brush object
//              POINTL*     pptlBrush   Pattern alignment
//
// RETURN:      none
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
VOID vMmPatCopyBlt3D(   // Type FNPATCOPY
PDEV*       ppdev,
OH*         pohDst,     // destination heap object
OH*         pohSrc,     // source heap object
LONG        c,          // Can't be zero
RECTL*      prcl,       // Array of relative coordinates destination rectangles
ULONG       ulHwMix,    // Hardware mix
POINTL*     pptlSrc,    // Original unclipped source point
RECTL*      prclDst,    // Original unclipped destination rectangle
BRUSHOBJ*   pbo,        // Brush to use
POINTL*     pptlBrush)  // Pattern alignment
{
    LONG    dx;
    LONG    dy;         // delta between org destination and destination rect
    LONG    cx;
    LONG    cy;         // Size of current rectangle - 1
    BYTE*   pjMmBase = ppdev->pjMmBase;
    ULONG   cmd;
    ULONG   SrcXEnd;
    ULONG   DstXEnd;
    ULONG   WidthAdjust;
    POINTL  ptlRelSrc;          // source point relative to destination

    ASSERTDD(c > 0, "Can't handle zero rectangles");

//
//  get the source starting point relative to the destination
//
    ptlRelSrc.x = pptlSrc->x - (pohDst->x - pohSrc->x);
    ptlRelSrc.y = pptlSrc->y - (pohDst->y - pohSrc->y);

    cmd = ppdev->ulCommandBase           |
          S3D_BITBLT                     |
          (ulHwMix << S3D_ROP_SHIFT)     |
          S3D_HARDWARE_CLIPPING          |
          S3D_DRAW_ENABLE;

//
// Set the base address for the destination
//
    S3DGPWait(ppdev);
    S3writeHIU(ppdev, S3D_BLT_DESTINATION_BASE, pohDst->ulBase);
    S3writeHIU(ppdev, S3D_BLT_SOURCE_BASE, pohSrc->ulBase);

    S3writeHIU(ppdev, S3D_BLT_DESTINATION_SOURCE_STRIDE,
               pohDst->lDelta << 16 | pohSrc->lDelta);
    //
    // Take care of the pattern
    //
    if (pbo->iSolidColor != -1)
    {
        //
        // Its a solid pattern
        //
        cmd |= S3D_MONOCHROME_PATTERN;
        S3DFifoWait(ppdev, 3);
        S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, pbo->iSolidColor);
        S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, pbo->iSolidColor);
    }
    else
    {
        RBRUSH* prb;

        prb = pbo->pvRbrush;

        // The S3's pattern hardware requires that we load an aligned copy
        // of the brush onto the chip.  We have to update this
        // realization if any of the following are true:
        //
        //   1) The brush alignment has changed;
        //
        // To handle the initial realization of a pattern, we're a little
        // tricky in order to save an 'if' in the following expression.  In
        // DrvRealizeBrush, we set 'prb->ptlBrushOrg.x' to be 0x80000000 (a
        // very negative number), which is guaranteed not to equal 'pptlBrush->x
        // + pohDst->x'.  So our check for brush alignment will also
        // handle the initialization case.

        if ((prb->ptlBrushOrg.x != pptlBrush->x + pohDst->x) ||
            (prb->ptlBrushOrg.y != pptlBrush->y + pohDst->y))
        {
            vMmFastPatRealize3D(ppdev, pohDst, prb, pptlBrush, FALSE);
        }

        if (prb->fl & RBRUSH_2COLOR)
        {
            ULONG* Src;

            //
            // Its a monochrome pattern.  Send the colors and pattern data to the
            // chip.
            //

            cmd |= S3D_MONOCHROME_PATTERN;

            Src = prb->pulAligned;

            S3DFifoWait(ppdev, 5);

            S3writeHIU(ppdev, S3D_BLT_PATTERN_FOREGROUND_COLOR, prb->ulForeColor);
            S3writeHIU(ppdev, S3D_BLT_PATTERN_BACKGROUND_COLOR, prb->ulBackColor);
            S3writeHIU(ppdev, S3D_BLT_MONO_PATTERN_0, *Src++);
            S3writeHIU(ppdev, S3D_BLT_MONO_PATTERN_1, *Src);

        }
        else
        {
            ULONG  i;
            ULONG  Dst;
            ULONG* Src;

            //
            // Its a color pattern.  Send the color data to the chip
            //
            Dst = S3D_COLOR_PATTERN_DATA;
            Src = prb->pulAligned;

            S3DGPWait(ppdev);

            for (i = ppdev->cAlignedBrushSize / 4; i != 0; i--)
            {
                S3writeHIU(ppdev, Dst, *Src++);
                Dst += 4;
            }
        }
    }

//
// We're going to have to do clipping due to a bug in ViRGE.  However,
// we're really only interested in clipping the left and right extents so
// we'll just set the top and bottom once and forget about it.
//
    S3DFifoWait(ppdev, 2);
    S3writeHIU(ppdev, S3D_BLT_CLIP_TOP_BOTTOM,  0x000007FFL);

    // The accelerator may not be as fast at doing right-to-left copies, so
    // only do them when the rectangles truly overlap:

    if (!OVERLAP(prclDst, &ptlRelSrc))
        goto Top_Down_Left_To_Right;

    if (prclDst->left <= ptlRelSrc.x)
    {
        if (prclDst->top <= ptlRelSrc.y)
        {
            do  // Top to Bottom, Left to Right
            {
Top_Down_Left_To_Right:

                dx = prcl->left - prclDst->left;
                dy = prcl->top - prclDst->top;

                SrcXEnd = (((prcl->right + dx - 1 ) * (ppdev->cjPelSize))
                           + ppdev->cPelSize) & 7;
                DstXEnd = (((prcl->right      - 1 ) * (ppdev->cjPelSize))
                           + ppdev->cPelSize) & 7;

                if (SrcXEnd > DstXEnd)
                {
                    WidthAdjust = 8 - SrcXEnd;
                    if (ppdev->iBitmapFormat == BMF_24BPP)
                    {
                        WidthAdjust = 7 - ((prcl->right - 1) & 7);
                    }
                    else if (ppdev->iBitmapFormat == BMF_16BPP)
                    {
                        WidthAdjust = (WidthAdjust + 1) >> 1;
                    }
                }
                else
                {
                    WidthAdjust = 0;
                }

                cx = prcl->right - prcl->left - 1;
                cy = prcl->bottom - prcl->top;

                S3DFifoWait(ppdev, 5);

                S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT,
                           ((prcl->left << 16) | prcl->right - 1));

                S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                           (((cx + WidthAdjust) << 16) | cy));

                S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                           (((prcl->left) << 16) | prcl->top));

                S3writeHIU(ppdev, S3D_BLT_SOURCE_X_Y,
                           (((pptlSrc->x + dx) << 16) | pptlSrc->y + dy));

                S3DGPWait(ppdev);
                S3writeHIU(ppdev, S3D_BLT_COMMAND,
                           cmd | S3D_X_POSITIVE_BLT
                               | S3D_Y_POSITIVE_BLT);

                prcl++;

            } while (--c != 0);
        }
        else
        {
            do  // Bottom to Top, Left to Right
            {
                dx = prcl->left - prclDst->left;
                dy = prcl->top - prclDst->top;

                SrcXEnd = (((prcl->right + dx - 1) * (ppdev->cjPelSize))
                           + ppdev->cPelSize) & 7;
                DstXEnd = (((prcl->right      - 1) * (ppdev->cjPelSize))
                           + ppdev->cPelSize) & 7;

                if (SrcXEnd > DstXEnd)
                {
                    WidthAdjust = 8 - SrcXEnd;
                    if (ppdev->iBitmapFormat == BMF_24BPP)
                    {
                        WidthAdjust = 7 - ((prcl->right - 1) & 7);
                    }
                    else if (ppdev->iBitmapFormat == BMF_16BPP)
                    {
                        WidthAdjust = (WidthAdjust + 1) >> 1;
                    }
                }
                else
                {
                    WidthAdjust = 0;
                }

                cx = prcl->right - prcl->left - 1;
                cy = prcl->bottom - prcl->top;

                S3DFifoWait(ppdev, 5);

                S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT,
                           ((prcl->left << 16) | prcl->right - 1));

                S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                           (((cx + WidthAdjust) << 16) | cy));

                S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                           (((prcl->left) << 16) | prcl->top + cy - 1));

                S3writeHIU(ppdev, S3D_BLT_SOURCE_X_Y,
                           (((pptlSrc->x + dx) << 16) |
                             pptlSrc->y + cy + dy - 1));

                S3DGPWait(ppdev);
                S3writeHIU(ppdev, S3D_BLT_COMMAND,
                           cmd | S3D_X_POSITIVE_BLT
                               | S3D_Y_NEGATIVE_BLT);

                prcl++;

            } while (--c != 0);
        }
    }
    else // (prclDst->left > ptlRelSrc.x)
    {
        if (prclDst->top <= pptlSrc->y)
        {
            do  // Top to Bottom, Right to Left
            {
                dx = prcl->left - prclDst->left;
                dy = prcl->top - prclDst->top;

                SrcXEnd = ((prcl->left + dx) * (ppdev->cjPelSize)) & 7;
                DstXEnd =  (prcl->left * (ppdev->cjPelSize)) & 7;

                if (SrcXEnd < DstXEnd)
                {
                    WidthAdjust = SrcXEnd + 1;
                    if (ppdev->iBitmapFormat == BMF_24BPP)
                    {
                        WidthAdjust = pptlSrc->x & 7;
                    }
                    else if (ppdev->iBitmapFormat == BMF_16BPP)
                    {
                        WidthAdjust = (WidthAdjust + 1) >> 1;
                    }
                }
                else
                {
                    WidthAdjust = 0;
                }

                cx = prcl->right - prcl->left - 1;
                cy = prcl->bottom - prcl->top;

                S3DFifoWait(ppdev, 5);

                S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT,
                           ((prcl->left << 16) | prcl->right - 1));

                S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                           (((cx + WidthAdjust) << 16) | cy));

                S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                           (((prcl->left + cx) << 16) | prcl->top));

                S3writeHIU(ppdev, S3D_BLT_SOURCE_X_Y,
                           (((pptlSrc->x + cx + dx) << 16) |
                             pptlSrc->y + dy));

                S3DGPWait(ppdev);
                S3writeHIU(ppdev, S3D_BLT_COMMAND,
                           cmd | S3D_X_NEGATIVE_BLT
                               | S3D_Y_POSITIVE_BLT);

                prcl++;

            } while (--c != 0);
        }
        else
        {
            do  // Bottom to Top, Right to Left
            {
                dx = prcl->left - prclDst->left;
                dy = prcl->top - prclDst->top;

                SrcXEnd = ((prcl->left + dx) * (ppdev->cjPelSize)) & 7;
                DstXEnd =  (prcl->left * (ppdev->cjPelSize)) & 7;

                if (SrcXEnd < DstXEnd)
                {
                    WidthAdjust = SrcXEnd + 1;
                    if (ppdev->iBitmapFormat == BMF_24BPP)
                    {
                        WidthAdjust = pptlSrc->x & 7;
                    }
                    else if (ppdev->iBitmapFormat == BMF_16BPP)
                    {
                        WidthAdjust = (WidthAdjust + 1) >> 1;
                    }
                }
                else
                {
                    WidthAdjust = 0;
                }

                cx = prcl->right - prcl->left - 1;
                cy = prcl->bottom - prcl->top;

                S3DFifoWait(ppdev, 5);

                S3writeHIU(ppdev, S3D_BLT_CLIP_LEFT_RIGHT,
                           ((prcl->left << 16) | prcl->right - 1));

                S3writeHIU(ppdev, S3D_BLT_WIDTH_HEIGHT,
                           (((cx + WidthAdjust) << 16) | cy));

                S3writeHIU(ppdev, S3D_BLT_DESTINATION_X_Y,
                           (((prcl->left + cx) << 16) | prcl->top + cy - 1));

                S3writeHIU(ppdev, S3D_BLT_SOURCE_X_Y,
                           (((pptlSrc->x + cx + dx) << 16) |
                              pptlSrc->y + cy + dy - 1));

                S3DGPWait(ppdev);
                S3writeHIU(ppdev, S3D_BLT_COMMAND,
                           cmd | S3D_X_NEGATIVE_BLT
                               | S3D_Y_NEGATIVE_BLT);

                prcl++;

            } while (--c != 0);
        }
    }
} // end vMmPatCopyBlt3D ()







