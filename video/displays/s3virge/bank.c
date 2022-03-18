//----------------------------------------------------------------------------
//  Module Name: bank.c
//
//  This module originally contained the code for banking support.  The ViRGE
//  driver does not support banking, so the bank related routines have been
//  removed.  This module now only contains the routines used for reading/
//  writing bitmaps from/to the frame buffer.
//
//  Copyright (c) 1996 S3 Inc.
//----------------------------------------------------------------------------
#include "precomp.h"
#include "hw3d.h"

////////////////////////////////////////////////////////////////////////////
// Old 911/924 Banking
//
// NOTE: It is the caller's responsibility to acquire the CRTC crtical
//       section before calling these routines, in all cases!
//
// if this function changes, must change .asm routines
//


////////////////////////////////////////////////////////////////////////////
// Linear Frame Buffer Banking
//
// NOTE: It is the caller's responsibility to acquire the CRTC crtical
//       section before calling these routines, in all cases!

VOID vLinearBankInitialize( PDEV*       ppdev,
                            BOOL        bMmIo)
{
    BYTE temp;


    //
    // Ensure that only new MMIO is enabled
    //
    VGAOUTP(ppdev, CRTC_INDEX, EXT_MEM_CTRL1);
    temp = VGAINP(ppdev, CRTC_DATA);
    temp &= ~ENABLE_BOTHMMIO;
    temp |=  ENABLE_NEWMMIO;
    VGAOUTP(ppdev, CRTC_DATA, temp);


    //
    // Ensure that linear addressing is enabled
    //
    VGAOUTP(ppdev, CRTC_INDEX, LAW_CTRL);
    temp = VGAINP(ppdev, CRTC_DATA);
    temp |= LIN_ADDR_ENABLE;
    VGAOUTP(ppdev, CRTC_DATA, temp);
}


/******************************Public*Routine******************************\
* BOOL bEnableBanking
*
\**************************************************************************/

BOOL bEnableBanking(
    PDEV*   ppdev)
{
    SURFOBJ*            psoBank;
    SIZEL               sizl;
    HSURF               hsurf;
    //FNBANKINITIALIZE*   pfnBankInitialize;
    //LONG                lDelta;
    //LONG                cjBank;
    //LONG                cPower2;

    if (ppdev->psoBank == NULL)
    {
        //
        // Create a GDI surface that we'll wrap around our bank in
        // call-backs:
        //

        sizl.cx = ppdev->cxMemory;
        sizl.cy = ppdev->cyMemory;

        hsurf = (HSURF) EngCreateBitmap(sizl,
                                        ppdev->lDelta,
                                        ppdev->iBitmapFormat,
                                        BMF_TOPDOWN,
                                        ppdev->pjScreen);

        // Note that we hook zero calls -- after all, the entire point
        // of all this is to have GDI do all the drawing on the bank.
        // Once we're done the association, we can leave the surface
        // permanently locked:

        if ((hsurf == 0)                                        ||
            (!EngAssociateSurface(hsurf, ppdev->hdevEng, 0))    ||
            (!(psoBank = EngLockSurface(hsurf))))
        {
            DISPDBG((0, "Failed wrapper surface creation"));

            EngDeleteSurface( hsurf );

            DISPDBG((0, "Failed bEnableBanking!"));

            return( FALSE );
        }

        ppdev->psoBank    = psoBank;
    }

    //
    // Warm up the hardware:
    //

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    vLinearBankInitialize( ppdev,
                           ppdev->ulCaps & (CAPS_MM_TRANSFER | CAPS_MM_IO) );

    RELEASE_CRTC_CRITICAL_SECTION( ppdev );

    DISPDBG((5, "Passed bEnableBanking"));

    return(TRUE);

}

/******************************Public*Routine******************************\
* VOID vDisableBanking
*
\**************************************************************************/

VOID vDisableBanking(PDEV* ppdev)
{
    HSURF hsurf;

    if (ppdev->psoBank != NULL)
    {
        hsurf = ppdev->psoBank->hsurf;
        EngUnlockSurface(ppdev->psoBank);
        EngDeleteSurface(hsurf);
    }
}

/******************************Public*Routine******************************\
* VOID vAlignedCopy
*
* Copies the given portion of a bitmap, using dword alignment for the
* screen.  Note that this routine has no notion of banking.
*
* Updates ppjDst and ppjSrc to point to the beginning of the next scan.
*
\**************************************************************************/

VOID vAlignedCopy(
PDEV*   ppdev,
BYTE**  ppjDst,
LONG    lDstDelta,
BYTE**  ppjSrc,
LONG    lSrcDelta,
LONG    cjScan,
LONG    cyScan,
BOOL    bDstIsScreen)
{
    BYTE* pjDst;
    BYTE* pjSrc;
    LONG  cjMiddle;
    LONG  culMiddle;
    LONG  cjStartPhase;
    LONG  cjEndPhase;
    LONG  i;

    S3DGPWait( ppdev );

    pjSrc = *ppjSrc;
    pjDst = *ppjDst;

    cjStartPhase = (LONG)((0 - ((bDstIsScreen) ? (ULONG_PTR) pjDst
                                               : (ULONG_PTR) pjSrc))) & 3;
    cjMiddle     = cjScan - cjStartPhase;

    if (cjMiddle < 0)
    {
        cjStartPhase = 0;
        cjMiddle     = cjScan;
    }

    lSrcDelta -= cjScan;
    lDstDelta -= cjScan;            // Account for middle

    cjEndPhase = cjMiddle & 3;
    culMiddle  = cjMiddle >> 2;

#if defined(_X86_)

    _asm {
        mov     eax,[lSrcDelta] ; Source delta accounting for middle
        mov     ebx,[lDstDelta] ; Dest delta accounting for middle
        mov     edx,[cyScan]    ; Count of scans
        mov     esi,[pjSrc]     ; Source pointer
        mov     edi,[pjDst]     ; Dest pointer

    Next_Scan:
        mov     ecx,[cjStartPhase]
        rep     movsb

        mov     ecx,[culMiddle]
        rep     movsd

        mov     ecx,[cjEndPhase]
        rep     movsb

        add     esi,eax         ; Advance to next scan
        add     edi,ebx
        dec     edx
        jnz     Next_Scan       ; loop thru scans

        mov     eax,ppjSrc      ; Save the updated pointers
        mov     ebx,ppjDst
        mov     [eax],esi
        mov     [ebx],edi
        }
#else
    if ( DIRECT_ACCESS( ppdev ) )
    {
        ///////////////////////////////////////////////////////////////////
        // Portable bus-aligned copy
        //
        // 'memcpy' usually aligns to the destination, so we could call
        // it for that case, but unfortunately we can't be sure.  We
        // always want to align to the frame buffer:

        CP_MEMORY_BARRIER();

        if (bDstIsScreen)
        {
            // Align to the destination (implying that the source may be
            // unaligned):

            for (; cyScan > 0; cyScan--)
            {
                for (i = cjStartPhase; i > 0; i--)
                {
                    *pjDst++ = *pjSrc++;
                }

                for (i = culMiddle; i > 0; i--)
                {
                    *((ULONG*) pjDst) = *((ULONG UNALIGNED *) pjSrc);
                    pjSrc += sizeof(ULONG);
                    pjDst += sizeof(ULONG);
                }

                for (i = cjEndPhase; i > 0; i--)
                {
                    *pjDst++ = *pjSrc++;
                }

                pjSrc += lSrcDelta;
                pjDst += lDstDelta;
            }
        }
        else
        {
            // Align to the source (implying that the destination may be
            // unaligned):

            for (; cyScan > 0; cyScan--)
            {
                for (i = cjStartPhase; i > 0; i--)
                {
                    *pjDst++ = *pjSrc++;
                }
                if (ppdev->ulCaps & CAPS_BAD_DWORD_READS)
                {
                    // #9 and Diamond 764 boards randomly fail in different
                    // spots on the HCTs, unless we do byte reads:

                    for (i = culMiddle; i > 0; i--)
                    {
                        *(pjDst)     = *(pjSrc);
                        *(pjDst + 1) = *(pjSrc + 1);
                        *(pjDst + 2) = *(pjSrc + 2);
                        *(pjDst + 3) = *(pjSrc + 3);

                        pjSrc += sizeof(ULONG);
                        pjDst += sizeof(ULONG);
                    }
                }
                else
                {
                    for (i = culMiddle; i > 0; i--)
                    {
                        if (ppdev->ulCaps & CAPS_FORCE_DWORD_REREADS)
                        {
                            //
                            // On fast MIPS machines, the cpu overdrives
                            // the card, so this code slows it down as
                            // little as possible while checking for
                            // consistency.
                            //

                            ULONG cnt = 4;

                            while (cnt)
                            {
                                ULONG   tmp = *((volatile ULONG*) (pjSrc));

                                *((ULONG UNALIGNED *) pjDst) =
                                        *((volatile ULONG*) (pjSrc));

                                if (tmp == *((volatile ULONG UNALIGNED *) pjDst))
                                    break;

                                --cnt;
                            }
                        }
                        else
                        {
                            *((ULONG UNALIGNED *) pjDst) = *((ULONG*) (pjSrc));
                        }

                        pjSrc += sizeof(ULONG);
                        pjDst += sizeof(ULONG);
                    }
                }
                for (i = cjEndPhase; i > 0; i--)
                {
                    *pjDst++ = *pjSrc++;
                }

                pjSrc += lSrcDelta;
                pjDst += lDstDelta;
            }
        }

        *ppjSrc = pjSrc;            // Save the updated pointers
        *ppjDst = pjDst;
    }
    else
    {
        ///////////////////////////////////////////////////////////////////
        // No direct dword reads bus-aligned copy
        //
        // Because we support the S3 on ancient Jensen Alpha's, we also
        // have to support a sparse view of the frame buffer -- which
        // means using the 'ioaccess.h' macros.
        //
        // We also go through this code path if doing dword reads would
        // crash a non-x86 system.

        MEMORY_BARRIER();

        if (bDstIsScreen)
        {
            // Align to the destination (implying that the source may be
            // unaligned):

            for (; cyScan > 0; cyScan--)
            {
                for (i = cjStartPhase; i > 0; i--)
                {
                    WRITE_REGISTER_UCHAR(pjDst, *pjSrc);
                    pjSrc++;
                    pjDst++;
                }

                for (i = culMiddle; i > 0; i--)
                {
                    WRITE_REGISTER_ULONG(pjDst, *((ULONG UNALIGNED *) pjSrc));
                    pjSrc += sizeof(ULONG);
                    pjDst += sizeof(ULONG);
                }

                for (i = cjEndPhase; i > 0; i--)
                {
                    WRITE_REGISTER_UCHAR(pjDst, *pjSrc);
                    pjSrc++;
                    pjDst++;
                }

                pjSrc += lSrcDelta;
                pjDst += lDstDelta;
            }
        }
        else
        {
            // Align to the source (implying that the destination may be
            // unaligned):

            for (; cyScan > 0; cyScan--)
            {
                for (i = cjStartPhase; i > 0; i--)
                {
                    *pjDst = READ_REGISTER_UCHAR(pjSrc);
                    pjSrc++;
                    pjDst++;
                }

                for (i = culMiddle; i > 0; i--)
                {
                    // There are some board 864/964 boards where we can't
                    // do dword reads from the frame buffer without
                    // crashing the system.

                    *((ULONG UNALIGNED *) pjDst) =
                     ((ULONG) READ_REGISTER_UCHAR(pjSrc + 3) << 24) |
                     ((ULONG) READ_REGISTER_UCHAR(pjSrc + 2) << 16) |
                     ((ULONG) READ_REGISTER_UCHAR(pjSrc + 1) << 8)  |
                     ((ULONG) READ_REGISTER_UCHAR(pjSrc));

                    pjSrc += sizeof(ULONG);
                    pjDst += sizeof(ULONG);
                }

                for (i = cjEndPhase; i > 0; i--)
                {
                    *pjDst = READ_REGISTER_UCHAR(pjSrc);
                    pjSrc++;
                    pjDst++;
                }

                pjSrc += lSrcDelta;
                pjDst += lDstDelta;
            }
        }

        *ppjSrc = pjSrc;            // Save the updated pointers
        *ppjDst = pjDst;
    }
#endif
}

/******************************Public*Routine******************************\
* VOID vPutBits
*
* Copies the bits from the given surface to the screen, using the memory
* aperture.  Must be pre-clipped.
*
\**************************************************************************/

VOID vPutBits(
PDEV*       ppdev,
SURFOBJ*    psoSrc,
RECTL*      prclDst,            // Absolute coordinates!
POINTL*     pptlSrc)            // Absolute coordinates!
{
    RECTL   rclDraw;
    LONG    lDstDelta;
    LONG    lSrcDelta;
    BYTE*   pjDst;
    BYTE*   pjSrc;

    rclDraw = *prclDst;

    ASSERTDD((rclDraw.left   >= 0) &&
             (rclDraw.top    >= 0) &&
             (rclDraw.right  <= ppdev->cxMemory) &&
             (rclDraw.bottom <= ppdev->cyMemory),
             "Rectangle wasn't fully clipped");

    //
    // Calculate the pointer to the upper-left corner of both rectangles:
    //

    lDstDelta = ppdev->lDelta;
    //pjDst     = ppdev->pjScreen + rclDraw.top  * lDstDelta
    //                            + (rclDraw.left * ppdev->cjPelSize)
    //                            - cjOffset;

    //
    // cjOffset should be 0 in a linear case
    //

    pjDst     = ppdev->pjScreen + rclDraw.top  * lDstDelta
                                + (rclDraw.left * ppdev->cjPelSize);

    lSrcDelta = psoSrc->lDelta;
    pjSrc     = (BYTE*) psoSrc->pvScan0 + pptlSrc->y * lSrcDelta
                                        + (pptlSrc->x * ppdev->cjPelSize);

    //
    //  make sure the engine is done drawing to the frame buffer
    //

    S3DGPWait( ppdev );

    vAlignedCopy (  ppdev,
                    &pjDst,
                    lDstDelta,
                    &pjSrc,
                    lSrcDelta,
                    (rclDraw.right  - rclDraw.left) * ppdev->cjPelSize,
                    (rclDraw.bottom - rclDraw.top),
                    TRUE);             // Screen is the destination

    return;
}

/******************************Public*Routine******************************\
* VOID vGetBits
*
* Copies the bits to the given surface from the screen, using the memory
* aperture.  Must be pre-clipped.
*
\**************************************************************************/

VOID vGetBits(
PDEV*       ppdev,
SURFOBJ*    psoDst,
RECTL*      prclDst,        // Absolute coordinates!
POINTL*     pptlSrc)        // Absolute coordinates!
{
    RECTL   rclDraw;
    LONG    lDstDelta;
    LONG    lSrcDelta;
    BYTE*   pjDst;
    BYTE*   pjSrc;

    rclDraw.left   = pptlSrc->x;
    rclDraw.top    = pptlSrc->y;
    rclDraw.right  = rclDraw.left + (prclDst->right  - prclDst->left);
    rclDraw.bottom = rclDraw.top  + (prclDst->bottom - prclDst->top);

    ASSERTDD((rclDraw.left   >= 0) &&
             (rclDraw.top    >= 0) &&
             (rclDraw.right  <= ppdev->cxMemory) &&
             (rclDraw.bottom <= ppdev->cyMemory),
             "Rectangle wasn't fully clipped");

    //
    // Calculate the pointer to the upper-left corner of both rectangles:
    //

    lSrcDelta = ppdev->lDelta;
    pjSrc     = ppdev->pjScreen + rclDraw.top  * lSrcDelta
                                + (rclDraw.left * ppdev->cjPelSize);

    lDstDelta = psoDst->lDelta;
    pjDst     = (BYTE*) psoDst->pvScan0 + prclDst->top  * lDstDelta
                                        + (prclDst->left * ppdev->cjPelSize);

    //
    //  make sure the engine is done drawing to the frame buffer
    //

    S3DGPWait( ppdev );

    vAlignedCopy (  ppdev,
                    &pjDst,
                    lDstDelta,
                    &pjSrc,
                    lSrcDelta,
                    (rclDraw.right  - rclDraw.left) * ppdev->cjPelSize,
                    (rclDraw.bottom - rclDraw.top),
                    FALSE);            // Screen is the source

    return;
}

