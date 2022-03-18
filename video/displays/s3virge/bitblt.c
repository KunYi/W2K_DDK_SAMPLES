/******************************Module*Header*******************************\
* Module Name: bitblt.c
*
* Contains the high-level DrvBitBlt and DrvCopyBits functions.  The low-
* level stuff lives in the 'blt??.c' files.
*
* Note: Since we've implemented device-bitmaps, any surface that GDI passes
*       to us can have 3 values for its 'iType': STYPE_BITMAP, STYPE_DEVICE
*       or STYPE_DEVBITMAP.  We filter device-bitmaps that we've stored
*       as DIBs fairly high in the code, so after we adjust its 'pptlSrc',
*       we can treat STYPE_DEVBITMAP surfaces the same as STYPE_DEVICE
*       surfaces (e.g., a blt from an off-screen device bitmap to the screen
*       gets treated as a normal screen-to-screen blt).
*
*       Unfortunately, if we've created our primary surface as a device-
*       managed surface, it has an 'iType' of STYPE_BITMAP and not
*       STYPE_DEVICE.  So throughout this code, we will determine if a
*       surface is one of ours by checking 'dhsurf' -- a NULL value means
*       that it's a GDI-created DIB, otherwise it's one of our surfaces and
*       'dhsurf' points to our DSURF structure.
*
* Copyright (c) 1992-1996 Microsoft Corporation
\**************************************************************************/


#include "precomp.h"
#include "hw3d.h"

/******************************Public*Table********************************\
* BYTE gajLeftMask[] and BYTE gajRightMask[]
*
* Edge tables for vXferScreenTo1bpp.
\**************************************************************************/

BYTE gajLeftMask[]  = { 0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
BYTE gajRightMask[] = { 0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe };

/******************************Public*Data*********************************\
* ROP3 translation table
*
* Translates the usual ternary rop into A-vector notation.  Each bit in
* this new notation corresponds to a term in a polynomial translation of
* the rop.
*
* Rop(D,S,P) = a + a D + a S + a P + a  DS + a  DP + a  SP + a   DSP
*               0   d     s     p     ds      dp      sp      dsp
*
\**************************************************************************/
BYTE gajRop3[] =
{
    0x00, 0xff, 0xb2, 0x4d, 0xd4, 0x2b, 0x66, 0x99,
    0x90, 0x6f, 0x22, 0xdd, 0x44, 0xbb, 0xf6, 0x09,
    0xe8, 0x17, 0x5a, 0xa5, 0x3c, 0xc3, 0x8e, 0x71,
    0x78, 0x87, 0xca, 0x35, 0xac, 0x53, 0x1e, 0xe1,
    0xa0, 0x5f, 0x12, 0xed, 0x74, 0x8b, 0xc6, 0x39,
    0x30, 0xcf, 0x82, 0x7d, 0xe4, 0x1b, 0x56, 0xa9,
    0x48, 0xb7, 0xfa, 0x05, 0x9c, 0x63, 0x2e, 0xd1,
    0xd8, 0x27, 0x6a, 0x95, 0x0c, 0xf3, 0xbe, 0x41,
    0xc0, 0x3f, 0x72, 0x8d, 0x14, 0xeb, 0xa6, 0x59,
    0x50, 0xaf, 0xe2, 0x1d, 0x84, 0x7b, 0x36, 0xc9,
    0x28, 0xd7, 0x9a, 0x65, 0xfc, 0x03, 0x4e, 0xb1,
    0xb8, 0x47, 0x0a, 0xf5, 0x6c, 0x93, 0xde, 0x21,
    0x60, 0x9f, 0xd2, 0x2d, 0xb4, 0x4b, 0x06, 0xf9,
    0xf0, 0x0f, 0x42, 0xbd, 0x24, 0xdb, 0x96, 0x69,
    0x88, 0x77, 0x3a, 0xc5, 0x5c, 0xa3, 0xee, 0x11,
    0x18, 0xe7, 0xaa, 0x55, 0xcc, 0x33, 0x7e, 0x81,
    0x80, 0x7f, 0x32, 0xcd, 0x54, 0xab, 0xe6, 0x19,
    0x10, 0xef, 0xa2, 0x5d, 0xc4, 0x3b, 0x76, 0x89,
    0x68, 0x97, 0xda, 0x25, 0xbc, 0x43, 0x0e, 0xf1,
    0xf8, 0x07, 0x4a, 0xb5, 0x2c, 0xd3, 0x9e, 0x61,
    0x20, 0xdf, 0x92, 0x6d, 0xf4, 0x0b, 0x46, 0xb9,
    0xb0, 0x4f, 0x02, 0xfd, 0x64, 0x9b, 0xd6, 0x29,
    0xc8, 0x37, 0x7a, 0x85, 0x1c, 0xe3, 0xae, 0x51,
    0x58, 0xa7, 0xea, 0x15, 0x8c, 0x73, 0x3e, 0xc1,
    0x40, 0xbf, 0xf2, 0x0d, 0x94, 0x6b, 0x26, 0xd9,
    0xd0, 0x2f, 0x62, 0x9d, 0x04, 0xfb, 0xb6, 0x49,
    0xa8, 0x57, 0x1a, 0xe5, 0x7c, 0x83, 0xce, 0x31,
    0x38, 0xc7, 0x8a, 0x75, 0xec, 0x13, 0x5e, 0xa1,
    0xe0, 0x1f, 0x52, 0xad, 0x34, 0xcb, 0x86, 0x79,
    0x70, 0x8f, 0xc2, 0x3d, 0xa4, 0x5b, 0x16, 0xe9,
    0x08, 0xf7, 0xba, 0x45, 0xdc, 0x23, 0x6e, 0x91,
    0x98, 0x67, 0x2a, 0xd5, 0x4c, 0xb3, 0xfe, 0x01
};
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

/******************************Public*Routine******************************\
* VOID vXferNativeSrccopy
*
* Does a SRCCOPY transfer of a bitmap to the screen using the frame
* buffer, because on the S3 it's faster than using the data transfer
* register.
*
\**************************************************************************/

VOID vXferNativeSrccopy(        // Type FNXFER
PDEV*       ppdev,
OH*         poh,
LONG        c,                  // Count of rectangles, can't be zero
RECTL*      prcl,               // List of destination rectangles, in relative
                                //   coordinates
ULONG       ulHwForeMix,        // Not used
ULONG       ulHwBackMix,        // Not used
SURFOBJ*    psoSrc,             // Source surface
POINTL*     pptlSrc,            // Original unclipped source point
RECTL*      prclDst,            // Original unclipped destination rectangle
XLATEOBJ*   pxlo)               // Not used
{
    LONG    dx;
    LONG    dy;

    BYTE*   pjDst;
    BYTE*   pjSrc;
    BYTE*   pjDstBase;
    BYTE*   pjSrcBase;
    LONG    lSrcDelta;
    LONG    lDstDelta;
    LONG    lPelBytes;
    LONG    cjScan;
    LONG    cyScan;

    ASSERTDD((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL),
            "Can handle trivial xlate only");
    ASSERTDD(psoSrc->iBitmapFormat == ppdev->iBitmapFormat,
            "Source must be same colour depth as screen");
    ASSERTDD(c > 0, "Can't handle zero rectangles");
    lPelBytes = CONVERT_TO_BYTES(1, ppdev);
    lSrcDelta = psoSrc->lDelta;
    lDstDelta = poh->lDelta;

    pjDstBase = (BYTE *)poh->pvScan0;
    pjSrcBase = (BYTE *)psoSrc->pvScan0;


    dx = pptlSrc->x - prclDst->left;
    dy = pptlSrc->y - prclDst->top;     // Add to destination to get source

    while (TRUE)
    {
        cjScan = (prcl->right - prcl->left) * lPelBytes;
        cyScan = prcl->bottom - prcl->top;

        pjDst = pjDstBase + (prcl->top * lDstDelta + prcl->left*lPelBytes);

        pjSrc =  pjSrcBase +
                 (prcl->top + dy) * lSrcDelta +
                 (prcl->left + dx) * lPelBytes;

         //
         //  make sure the engine is done drawing to the frame buffer
         //
         // EKL - Driver is synchronized with GDI
         // S3DGPWait ( ppdev);

        vAlignedCopy(
                     ppdev,
                     &pjDst,
                     lDstDelta,
                     &pjSrc,
                     lSrcDelta,
                     cjScan,
                     cyScan,
                     TRUE);

        if (--c == 0)
            return;

        prcl++;
    }

}
/******************************Public*Routine******************************\
* VOID vXferScreenTo1bpp
*
* Performs a SRCCOPY transfer from the screen (when it's 8bpp) to a 1bpp
* bitmap.
*
\**************************************************************************/

#if defined(_X86_)

VOID vXferScreenTo1bpp(         // Type FNXFER
PDEV*       ppdev,
OH*         poh,
LONG        c,                  // Count of rectangles, can't be zero
RECTL*      prcl,               // List of destination rectangles, in relative
                                //   coordinates
ULONG       ulHwForeMix,        // Not used
ULONG       ulHwBackMix,        // Not used
SURFOBJ*    psoDst,             // Destination surface
POINTL*     pptlSrc,            // Original unclipped source point
RECTL*      prclDst,            // Original unclipped destination rectangle
XLATEOBJ*   pxlo)               // Provides colour-compressions information
{
    LONG    cjPelSize;
    VOID*   pfnCompute;
    SURFOBJ soTmp;
    ULONG*  pulXlate;
    ULONG   ulForeColor;
    POINTL  ptlSrc;
    RECTL   rclTmp;
    BYTE*   pjDst;
    BYTE*   pjAlignedCopySrc;
    BYTE*   pjAlignedCopyDst;
    BYTE    jLeftMask;
    BYTE    jRightMask;
    BYTE    jNotLeftMask;
    BYTE    jNotRightMask;
    LONG    cjMiddle;
    LONG    lDstDelta;
    LONG    lSrcDelta;
    LONG    cyTmpScans;
    LONG    cyThis;
    LONG    cyToGo;

    ASSERTDD(c > 0, "Can't handle zero rectangles");
    ASSERTDD(psoDst->iBitmapFormat == BMF_1BPP, "Only 1bpp destinations");
    ASSERTDD(TMP_BUFFER_SIZE >= (ppdev->cxMemory * ppdev->cjPelSize),
                "Temp buffer has to be larger than widest possible scan");

    // When the destination is a 1bpp bitmap, the foreground colour
    // maps to '1', and any other colour maps to '0'.

    if (ppdev->iBitmapFormat == BMF_8BPP)
    {
        // When the source is 8bpp or less, we find the forground colour
        // by searching the translate table for the only '1':

        pulXlate = pxlo->pulXlate;
        while (*pulXlate != 1)
            pulXlate++;

        ulForeColor = pulXlate - pxlo->pulXlate;
    }
    else
    {
        ASSERTDD((ppdev->iBitmapFormat == BMF_16BPP) ||
                 (ppdev->iBitmapFormat == BMF_32BPP),
                 "This routine only supports 8, 16 or 32bpp");

        // When the source has a depth greater than 8bpp, the foreground
        // colour will be the first entry in the translate table we get
        // from calling 'piVector':

        pulXlate = XLATEOBJ_piVector(pxlo);

        ulForeColor = 0;
        if (pulXlate != NULL)           // This check isn't really needed...
            ulForeColor = pulXlate[0];
    }

    // We use the temporary buffer to keep a copy of the source
    // rectangle:

    soTmp.pvScan0 = ppdev->pvTmpBuffer;

    do {
        // ptlSrc points to the upper-left corner of the screen rectangle
        // for the current batch:

        ptlSrc.x = prcl->left + (pptlSrc->x - prclDst->left);
        ptlSrc.y = prcl->top  + (pptlSrc->y - prclDst->top);

        // vGetBits takes absolute coordinates for the source point:

        pjDst = (BYTE*) psoDst->pvScan0 + (prcl->top * psoDst->lDelta)
                                        + (prcl->left >> 3);

        cjPelSize = ppdev->cjPelSize;

        soTmp.lDelta = (((prcl->right + 7L) & ~7L) - (prcl->left & ~7L))
                       * cjPelSize;

        // Our temporary buffer, into which we read a copy of the source,
        // may be smaller than the source rectangle.  In that case, we
        // process the source rectangle in batches.
        //
        // cyTmpScans is the number of scans we can do in each batch.
        // cyToGo is the total number of scans we have to do for this
        // rectangle.
        //
        // We take the buffer size less four so that the right edge case
        // can safely read one dword past the end:

        cyTmpScans = (TMP_BUFFER_SIZE - 4) / soTmp.lDelta;
        cyToGo     = prcl->bottom - prcl->top;

        ASSERTDD(cyTmpScans > 0, "Buffer too small for largest possible scan");

        // Initialize variables that don't change within the batch loop:

        rclTmp.top    = 0;
        rclTmp.left   = prcl->left & 7L;
        rclTmp.right  = (prcl->right - prcl->left) + rclTmp.left;

        // Note that we have to be careful with the right mask so that it
        // isn't zero.  A right mask of zero would mean that we'd always be
        // touching one byte past the end of the scan (even though we
        // wouldn't actually be modifying that byte), and we must never
        // access memory past the end of the bitmap (because we can access
        // violate if the bitmap end is exactly page-aligned).

        jLeftMask     = gajLeftMask[rclTmp.left & 7];
        jRightMask    = gajRightMask[rclTmp.right & 7];
        cjMiddle      = ((rclTmp.right - 1) >> 3) - (rclTmp.left >> 3) - 1;

        if (cjMiddle < 0)
        {
            // The blt starts and ends in the same byte:

            jLeftMask &= jRightMask;
            jRightMask = 0;
            cjMiddle   = 0;
        }

        jNotLeftMask  = (BYTE) ~jLeftMask;
        jNotRightMask = (BYTE) ~jRightMask;
        lDstDelta     = psoDst->lDelta - cjMiddle - 2;
                                // Delta from the end of the destination
                                //  to the start on the next scan, accounting
                                //  for 'left' and 'right' bytes

        lSrcDelta     = soTmp.lDelta - ((8 * (cjMiddle + 2)) * cjPelSize);
                                // Compute source delta for special cases
                                //  like when cjMiddle gets bumped up to '0',
                                //  and to correct aligned cases

        do {
            // This is the loop that breaks the source rectangle into
            // manageable batches.

            cyThis  = cyTmpScans;
            cyToGo -= cyThis;
            if (cyToGo < 0)
                cyThis += cyToGo;

            rclTmp.bottom = cyThis;

            pjAlignedCopySrc = (BYTE *)(poh->pvScan0);
            pjAlignedCopySrc += (ptlSrc.y * poh->lDelta + CONVERT_TO_BYTES(ptlSrc.x, ppdev));
            pjAlignedCopyDst = (BYTE *)(soTmp.pvScan0);
            //
            // make sure the engine is done drawing to the frame buffer
            //
            // EKL - Driver is synchronized with GDI
            // S3DGPWait ( ppdev);

            vAlignedCopy(
                        ppdev,
                        &pjAlignedCopyDst,
                        soTmp.lDelta,
                        &pjAlignedCopySrc,
                        poh->lDelta,
                        CONVERT_TO_BYTES((rclTmp.right - rclTmp.left), ppdev),
                        cyThis,
                        FALSE);

            ptlSrc.y += cyThis;         // Get ready for next batch loop

            _asm {
                mov     eax,ulForeColor     ;eax = foreground colour
                                            ;ebx = temporary storage
                                            ;ecx = count of middle dst bytes
                                            ;dl  = destination byte accumulator
                                            ;dh  = temporary storage
                mov     esi,soTmp.pvScan0   ;esi = source pointer
                mov     edi,pjDst           ;edi = destination pointer

                ; Figure out the appropriate compute routine:

                mov     ebx,cjPelSize
                mov     pfnCompute,offset Compute_Destination_Byte_From_8bpp
                dec     ebx
                jz      short Do_Left_Byte
                mov     pfnCompute,offset Compute_Destination_Byte_From_16bpp
                dec     ebx
                jz      short Do_Left_Byte
                mov     pfnCompute,offset Compute_Destination_Byte_From_32bpp
                dec     ebx
                jnz     short Do_Left_Byte

                shl     eax, 8  // position color in upper 24bits for 24bpp routine
                mov     pfnCompute,offset Compute_Destination_Byte_From_24bpp

            Do_Left_Byte:
                call    pfnCompute
                and     dl,jLeftMask
                mov     dh,jNotLeftMask
                and     dh,[edi]
                or      dh,dl
                mov     [edi],dh
                inc     edi
                mov     ecx,cjMiddle
                dec     ecx
                jl      short Do_Right_Byte

            Do_Middle_Bytes:
                call    pfnCompute
                mov     [edi],dl
                inc     edi
                dec     ecx
                jge     short Do_Middle_Bytes

            Do_Right_Byte:
                call    pfnCompute
                and     dl,jRightMask
                mov     dh,jNotRightMask
                and     dh,[edi]
                or      dh,dl
                mov     [edi],dh
                inc     edi

                add     edi,lDstDelta
                add     esi,lSrcDelta
                dec     cyThis
                jnz     short Do_Left_Byte

                mov     pjDst,edi               ;save for next batch

                jmp     All_Done

            Compute_Destination_Byte_From_8bpp:
                mov     bl,[esi]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 0

                mov     bl,[esi+1]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 1

                mov     bl,[esi+2]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 2

                mov     bl,[esi+3]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 3

                mov     bl,[esi+4]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 4

                mov     bl,[esi+5]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 5

                mov     bl,[esi+6]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 6

                mov     bl,[esi+7]
                sub     bl,al
                cmp     bl,1
                adc     dl,dl                   ;bit 7

                add     esi,8                   ;advance the source
                ret

            Compute_Destination_Byte_From_16bpp:
                mov     bx,[esi]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 0

                mov     bx,[esi+2]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 1

                mov     bx,[esi+4]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 2

                mov     bx,[esi+6]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 3

                mov     bx,[esi+8]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 4

                mov     bx,[esi+10]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 5

                mov     bx,[esi+12]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 6

                mov     bx,[esi+14]
                sub     bx,ax
                cmp     bx,1
                adc     dl,dl                   ;bit 7

                add     esi,16                  ;advance the source
                ret

// note that the comparison color is in the upper 24 bits of eax
// for the first pixel, the value is shifted up to avoid reading a byte before the bitmap
// the rest are read starting one byte before the pixel so bl can be zeroed

            Compute_Destination_Byte_From_24bpp:
                xor     dh,dh

                mov     ebx,[esi]
                shl     ebx, 8
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 0

                mov     ebx,[esi+2] 
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 1

                mov     ebx,[esi+5]
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 2

                mov     ebx,[esi+8]
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 3

                mov     ebx,[esi+11]
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 4

                mov     ebx,[esi+14]
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 5

                mov     ebx,[esi+17]
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 6

                mov     ebx,[esi+20]
                mov     bl,dh
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 7

                add     esi,24                  ;advance the source
                ret


            Compute_Destination_Byte_From_32bpp:
                mov     ebx,[esi]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 0

                mov     ebx,[esi+4]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 1

                mov     ebx,[esi+8]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 2

                mov     ebx,[esi+12]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 3

                mov     ebx,[esi+16]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 4

                mov     ebx,[esi+20]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 5

                mov     ebx,[esi+24]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 6

                mov     ebx,[esi+28]
                sub     ebx,eax
                cmp     ebx,1
                adc     dl,dl                   ;bit 7

                add     esi,32                  ;advance the source
                ret

            All_Done:
            }
        } while (cyToGo > 0);

        prcl++;
    } while (--c != 0);
}

#endif // i386
//vvvvvvvvv5/30/97vvvvvvvv For special case, such rop=P, 0, DPx, Dn vvvvvvvvvv
#define COLOR_BLACK         0x00000000L     // Black color (at any pixel depth)

#define ROP3_BLACKNESS      0x00            // Blackness ROP3 code
#define ROP3_WHITENESS      0xFF            // Whiteness ROP3 code

#define S3D_CMD_ROP_MASK    (0xFF << S3D_ROP_SHIFT) // Command reg ROP mask

//-----------------------------------------------------------------------------
// PROCEDURE:   BOOL DrvBitBlt:
//
// DESCRIPTION: Performs bit block transfers to/between screen, offscreen and
//              system memory surfaces.
//
// ASSUMPTION:  None
//
// CALLS:       bIntersect ()
//              bMoveDibToOffscreenDfbIfRoom ()
//              cIntersect ()
//              CLIPOBJ_bEnum ()
//              CLIPOBJ_cEnumStart ()
//              EngBitBlt ()
//              (ppdev->pfnCopyBlt) ()
//              (ppdev->pfnFillPat) ()
//              (ppdev->pfnFillSolid) ()
//              (ppdev->pfnXfer1bpp) ()
//              (ppdev->pfnXferNative) ()
//              vMmPatXfer1bpp3D ()
//              vMmPatXferNative3D ()
//              vXferNativeSrccopy ()
//              vXferScreenTo1bpp ()
//
// NOTES:       None
//
BOOL DrvBitBlt (
SURFOBJ*    psoDst,                     // Destination PDEV surface object ptr
SURFOBJ*    psoSrc,                     // Source      PDEV surface object ptr
SURFOBJ*    psoMsk,                     // Mask        PDEV surface object ptr
CLIPOBJ*    pco,                        // Clip object CLIPOBJ ptr
XLATEOBJ*   pxlo,                       //
RECTL*      prclDst,                    //
POINTL*     pptlSrc,                    //
POINTL*     pptlMsk,                    //
BRUSHOBJ*   pbo,                        //
POINTL*     pptlBrush,                  //
ROP4        rop4)                       //
{

    PDEV*           ppdev;
    DSURF*          pdsurfDst;
    DSURF*          pdsurfSrc;
    OH*             pohDst;
    OH*             pohSrc;
    POINTL          ptlSrc;
    BYTE            jClip;
    BOOL            bMore;
    ULONG           ulHwForeMix;
    ULONG           ulHwBackMix;
    CLIPENUM        ce;
    LONG            c;
    RECTL           rcl;
    ULONG           rop3;
    FNFILL*         pfnFill;
    FNXFER*         pfnXfer;
    FNPATXFER*      pfnPatXfer;
    RBRUSH_COLOR    rbc;         // Realized brush or solid colour
    ULONG           iSrcBitmapFormat;
    ULONG           iDir;
    SURFOBJ*        psoNewDst;
    SURFOBJ*        psoNewSrc;
    BYTE            bAVector;
    ULONG           ulEngDrawCmd;
    LONG            lRectWidth;               // Dest rectangle width,  0-based
    LONG            lRectHeight;              // Dest rectangle height, 0-based

    pdsurfDst = (DSURF*) psoDst->dhsurf;
    bAVector  = gajRop3 [rop4 & 0xFF];
    jClip     = ((pco == NULL) ? DC_TRIVIAL : pco->iDComplexity);

    //
    // For special case, such rop=P, 0, DPx, Dn
    //

    if ( ((rop4 & 0xff)== PAT_COPY)      ||
         ((rop4 & 0xff)== DEST_PAT_XOR ) ||
         ((rop4 & 0xff)== ZERO_ROP)      ||
         ((rop4 & 0xff)== DEST_NOT) )
    {
        if ( (pbo && pbo->iSolidColor != -1)         &&
             (jClip                   == DC_TRIVIAL) &&
             (pdsurfDst->dt           == DT_SCREEN)  &&
             ((rop4 & 0xff)           == (rop4 >>8)))
        {
            lRectWidth  = (prclDst->right  - prclDst->left);
            lRectHeight = (prclDst->bottom - prclDst->top);

            //
            // if not a visible rectangle bail out
            //

            if ((lRectWidth > 0) && (lRectHeight > 0))
            {
                ulHwForeMix                =
                ulHwBackMix                = rop4 & 0xff;
                ppdev                      = (PDEV*) psoDst->dhpdev;
                ppdev->bRealizeTransparent = FALSE;
                pohDst                     = pdsurfDst->poh;
                rbc.iSolidColor            = pbo->iSolidColor;

            // Setup the drawing variables before allocating the engine FIFO slots
            //

                ulEngDrawCmd = ppdev->ulCommandBase           |
                               S3D_BITBLT                     |
                               S3D_MONOCHROME_PATTERN         |
                               (ulHwForeMix << S3D_ROP_SHIFT) |
                               S3D_X_POSITIVE_BLT             |
                               S3D_Y_POSITIVE_BLT             |
                               S3D_DRAW_ENABLE;

                WAIT_DMA_IDLE( ppdev );
                TRIANGLE_WORKAROUND( ppdev );

            //
            // NOTE: Although not listed in the Virge stepping information/errata, a wait
            //       for engine idle is necessary (else the HCT GuiJr tests fail).
            //
            //  S3DGPWait (ppdev);
            //  S3DFifoWait (ppdev, 8);


            //
            // Set destination base address/stride for drawing operation
            //
                S3writeHIU (ppdev,
                            S3D_BLT_DESTINATION_BASE,
                            pohDst->ulBase);

                S3writeHIU (ppdev,
                            S3D_BLT_DESTINATION_SOURCE_STRIDE,
                            (pohDst->lDelta << 16) | pohDst->lDelta);

            //
            // If solid pattern black or white PATCOPY, replace with WHITENESS/BLACKNESS
            //  ROP code to eliminate the need to program the fgnd/bgnd color registers.
            // If solid pattern, set foreground/background colors to same value (so there's
            //  no need to load the monochrome pattern registers 0/1).
            //
                if (ulHwForeMix == PAT_COPY && ((rbc.iSolidColor == ppdev->ulWhite) ||
                                                (rbc.iSolidColor == 0L)))
                {
                    ulEngDrawCmd &= (~S3D_CMD_ROP_MASK);    // default = ROP3_BLACKNESS
                    if (rbc.iSolidColor != COLOR_BLACK)
                    {
                        ulEngDrawCmd |= (ROP3_WHITENESS << S3D_ROP_SHIFT);
                    }
                }
                else
                {
                    S3writeHIU (ppdev,
                                S3D_BLT_PATTERN_FOREGROUND_COLOR,
                                rbc.iSolidColor);

                    S3writeHIU (ppdev,
                                S3D_BLT_PATTERN_BACKGROUND_COLOR,
                                rbc.iSolidColor);
                }

                S3writeHIU (ppdev,
                            S3D_BLT_WIDTH_HEIGHT,
                            ((lRectWidth - 1) << 16) | lRectHeight);

                S3writeHIU (ppdev,
                            S3D_BLT_DESTINATION_X_Y,
                            (prclDst->left << 16) | prclDst->top);

                S3writeHIU (ppdev, S3D_BLT_COMMAND,         ulEngDrawCmd);
            }

            goto All_Done;
        }
    }

    if (psoSrc == NULL)
    {
        //
        // Solid and pattern fills are the most common BitBlt operations. For
        // performance reasons, they should be handled as quickly as possible.
        //
        //ASSERTDD ((psoDst->iType == STYPE_DEVICE) ||
        //          (psoDst->iType == STYPE_DEVBITMAP),
        //          "DBB: Expect only device destinations when no source");

        pohDst = pdsurfDst->poh;

        if (pdsurfDst->dt == DT_SCREEN)
        {
            ppdev = (PDEV*) psoDst->dhpdev;

            //
            // Make sure it doesn't involve a mask (i.e., it's really a Rop3):
            //

            if ((rop4 >> 8) == (rop4 & 0xff))
            {
                rop3 = (BYTE) (rop4 & 0xff);

                //
                // See if the Rop3 needs a source
                //
                if ( !(bAVector & AVEC_NEED_SOURCE))
                {
                    // The ROP3 doesn't require a source...

                    ulHwForeMix = rop3;
                    ulHwBackMix = ulHwForeMix;

                    ppdev->bRealizeTransparent = FALSE;

                    pfnFill = ppdev->pfnFillSolid;
                    if (bAVector & AVEC_NEED_PATTERN)
                    {
                        rbc.iSolidColor = pbo->iSolidColor;
                        if (rbc.iSolidColor == -1)
                        {
                            // Try and realize the pattern brush; by doing
                            // this call-back, GDI will eventually call us
                            // again through DrvRealizeBrush:

                            rbc.prb = pbo->pvRbrush;
                            if (rbc.prb == NULL)
                            {
                                rbc.prb = BRUSHOBJ_pvGetRbrush(pbo);
                                if (rbc.prb == NULL)
                                {
                                    // If we couldn't realize the brush, punt
                                    // the call (it may have been a non 8x8
                                    // brush or something, which we can't be
                                    // bothered to handle, so let GDI do the
                                    // drawing):

                                    goto Punt_It;
                                }
                            }
                            pfnFill = ppdev->pfnFillPat;

                        } // end if (rbc.iSolidColor == -1)
                    } // end if (bAVector & AVEC_NEED_PATTERN)

                    // Note that these 2 'if's are more efficient than
                    // a switch statement:

                    if (jClip == DC_TRIVIAL)
                    {
                        WAIT_DMA_IDLE( ppdev );
                        TRIANGLE_WORKAROUND( ppdev );

                        pfnFill (ppdev, pohDst, 1, prclDst, ulHwForeMix,
                                 ulHwBackMix, rbc, pptlBrush);
                        goto All_Done;
                    }
                    else if (jClip == DC_RECT)
                    {
                        if (bIntersect (prclDst, &pco->rclBounds, &rcl))
                        {
                            WAIT_DMA_IDLE( ppdev );
                            TRIANGLE_WORKAROUND( ppdev );

                            pfnFill (ppdev, pohDst, 1, &rcl, ulHwForeMix,
                                     ulHwBackMix, rbc, pptlBrush);
                        }
                        goto All_Done;
                    }
                    else // (jClip == DC_COMPLEX)
                    {
                        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES,
                                            CD_ANY, 0);

                        WAIT_DMA_IDLE( ppdev );
                        TRIANGLE_WORKAROUND( ppdev );

                        do // while (bMore)
                        {
                            bMore = CLIPOBJ_bEnum (pco, sizeof(ce),
                                                   (ULONG*) &ce);

                            c = cIntersect (prclDst, ce.arcl, ce.c);

                            if (c != 0)
                            {
                                pfnFill (ppdev, pohDst, c, ce.arcl, ulHwForeMix,
                                         ulHwBackMix, rbc, pptlBrush);
                            }

                        } while (bMore);

                        goto All_Done;

                    } // end else (jClip == DC_COMPLEX)
                } // end if ( !(bAVector & AVEC_NEED_SOURCE))
            } // end if ((rop4 >> 8) == (rop4 & 0xff))
        } // end if (pdsurfDst->dt == DT_SCREEN)
    } // end if (psoSrc == NULL)

    //
    // End of Fills
    //

    if ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))
    {
        pdsurfSrc = (DSURF*) psoSrc->dhsurf;
        if (pdsurfSrc->dt == DT_DIB)
        {
            // Here we consider putting a DIB DFB back into off-screen
            // memory.  If there's a translate, it's probably not worth
            // moving since we won't be able to use the hardware to do
            // the blt (a similar argument could be made for weird rops
            // and stuff that we'll only end up having GDI simulate, but
            // those should happen infrequently enough that I don't care).

            if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL))
            {
                ppdev = (PDEV*) psoSrc->dhpdev;

                // See 'DrvCopyBits' for some more comments on how this
                // moving-it-back-into-off-screen-memory thing works:

                if (pdsurfSrc->iUniq == ppdev->iHeapUniq)
                {
                    if (--pdsurfSrc->cBlt == 0)
                    {
                        if (bMoveDibToOffscreenDfbIfRoom(ppdev, pdsurfSrc))
                            goto Continue_It;
                    }
                }
                else
                {
                    // Some space was freed up in off-screen memory,
                    // so reset the counter for this DFB:

                    pdsurfSrc->iUniq = ppdev->iHeapUniq;
                    pdsurfSrc->cBlt  = HEAP_COUNT_DOWN;
                }
            }

            psoSrc = pdsurfSrc->pso;

            // Handle the case where the source is a DIB DFB and the
            // destination is a regular bitmap:

            if (psoDst->iType == STYPE_BITMAP)
                goto EngBitBlt_It;
        }
    } // end if ((psoSrc != NULL) && (psoSrc->iType == STYPE_DEVBITMAP))

Continue_It:

    if (pdsurfDst != NULL)
    {
        if (pdsurfDst->dt == DT_DIB)
        {
            //
            //  the destination is a device bitmap which has been converted
            //  to a DIB
            //
            psoDst = pdsurfDst->pso;

            // If the destination is a DIB, we can only handle this
            // call if the source is not a DIB:

            if ((psoSrc == NULL) || (psoSrc->dhsurf == NULL))
                goto EngBitBlt_It;
        }
    }

    // At this point, we know that either the source or the destination is
    // not a DIB.  Check for a DFB to screen, DFB to DFB, or screen to DFB
    // case:

    if ((psoSrc != NULL) &&
        (psoDst->dhsurf != NULL) &&
        (psoSrc->dhsurf != NULL))
    {
        //  screen to screen
        //
            pdsurfSrc = (DSURF*) psoSrc->dhsurf;
            pohSrc    = pdsurfSrc->poh;
            pohDst    = pdsurfDst->poh;

            ASSERTDD(pdsurfSrc->dt == DT_SCREEN, "Expected screen source");
            ASSERTDD(pdsurfDst->dt == DT_SCREEN, "Expected screen destination");
    }

    if (psoDst->dhsurf != NULL)
    {
    //
    //  DIB to screen or screen to screen
    //
        pohDst = pdsurfDst->poh;
        ppdev  = (PDEV*)  psoDst->dhpdev;
    }
    else
    {
    //  screen to DIB
    //
        pdsurfSrc   = (DSURF*) psoSrc->dhsurf;
        pohSrc      = pdsurfSrc->poh;
        ppdev       = (PDEV*)  psoSrc->dhpdev;
    }


    if ((rop4 >> 8) == (rop4 & 0xff))
    {
        // Since we've already handled the cases where the ROP4 is really
        // a ROP3 and no source is required, we can assert...

        ASSERTDD((psoSrc != NULL) && (pptlSrc != NULL),
                 "Expected no-source case to already have been handled");

        ///////////////////////////////////////////////////////////////////
        // Bitmap transfers
        ///////////////////////////////////////////////////////////////////

        // Since the foreground and background ROPs are the same, we
        // don't have to worry about no stinking masks (it's a simple
        // Rop3).

        rop3 = (rop4 & 0xff);   // Make it into a Rop3 (we keep the rop4
                                //  around in case we decide to punt)

        if (psoDst->dhsurf != NULL)
        {
            // The destination is the screen:

            if ((rop3 >> 4) == (rop3 & 0xF))
            {
                // The ROP3 doesn't require a pattern:

                if (psoSrc->dhsurf == NULL)
                {
                    //////////////////////////////////////////////////
                    // DIB-to-screen blt

                    // This section handles 1bpp, 4bpp and 8bpp sources.
                    // 1bpp should have 'ulHwForeMix' and 'ulHwBackMix' the
                    // same values, and 4bpp and 8bpp ignore 'ulHwBackMix'.

                    ulHwForeMix = rop3;
                    ulHwBackMix = ulHwForeMix;

                    iSrcBitmapFormat = psoSrc->iBitmapFormat;
                    if (iSrcBitmapFormat == BMF_1BPP)
                    {
                        pfnXfer = ppdev->pfnXfer1bpp;
                        goto Xfer_It;
                    }
                    else if ((iSrcBitmapFormat == ppdev->iBitmapFormat) &&
                             ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)))
                    {
                        if (rop3 != SRC_COPY)
                        {
                            pfnXfer = ppdev->pfnXferNative;
                        }
                        else
                        {
                            // Plain SRCCOPY blts will be somewhat faster on
                            // the S3 if we go through the memory aperture:

                            pfnXfer = vXferNativeSrccopy;
                        }
                        goto Xfer_It;
                    }
                    else if ((iSrcBitmapFormat == BMF_4BPP) &&
                             (ppdev->iBitmapFormat == BMF_8BPP))
                    {
                    //
                    //  let the DIB engine handle this case
                    //
                        goto Punt_It;
                    }
                }
                else // psoSrc->dhsurf != NULL
                {
                    if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL))
                    {
                        //////////////////////////////////////////////////
                        // Screen-to-screen blt with no translate

                        ulHwForeMix = rop3;

                        if (jClip == DC_TRIVIAL)
                        {
                            WAIT_DMA_IDLE( ppdev );
                            TRIANGLE_WORKAROUND( ppdev );

                            ppdev->pfnCopyBlt (ppdev, pohDst, pohSrc, 1,
                                               prclDst, ulHwForeMix,
                                               pptlSrc, prclDst);
                            goto All_Done;
                        }
                        else if (jClip == DC_RECT)
                        {
                            if (bIntersect(prclDst, &pco->rclBounds, &rcl))
                            {
                                WAIT_DMA_IDLE( ppdev );
                                TRIANGLE_WORKAROUND( ppdev );

                                ppdev->pfnCopyBlt (ppdev, pohDst, pohSrc, 1,
                                                   &rcl, ulHwForeMix,
                                                   pptlSrc, prclDst);
                            }
                            goto All_Done;
                        }
                        else // (jClip == DC_COMPLEX)
                        {
                            // Don't forget that we'll have to draw the
                            // rectangles in the correct direction:

                            if ((pptlSrc->y - (pohDst->y - pohSrc->y)) >=
                                    prclDst->top)
                            {
                                if ((pptlSrc->x - (pohDst->x - pohSrc->x)) >=
                                        prclDst->left)
                                {
                                    iDir = CD_RIGHTDOWN;
                                }
                                else
                                {
                                    iDir = CD_LEFTDOWN;
                                }
                            }
                            else
                            {
                                if ((pptlSrc->x - (pohDst->x - pohSrc->x)) >=
                                        prclDst->left)
                                {
                                    iDir = CD_RIGHTUP;
                                }
                                else
                                {
                                    iDir = CD_LEFTUP;
                                }
                            }

                            CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES,
                                               iDir, 0);

                            WAIT_DMA_IDLE( ppdev );
                            TRIANGLE_WORKAROUND( ppdev );

                            do // while (bMore)
                            {
                                bMore = CLIPOBJ_bEnum (pco, sizeof(ce),
                                                       (ULONG*) &ce);

                                c = cIntersect (prclDst, ce.arcl, ce.c);

                                if (c != 0)
                                {
                                    ppdev->pfnCopyBlt (ppdev, pohDst, pohSrc,
                                                       c, ce.arcl, ulHwForeMix,
                                                       pptlSrc, prclDst);
                                }

                            } while (bMore);
                            goto All_Done;
                        }
                    }
                }
            } // end if ((rop3 >> 4) == (rop3 & 0xF))
            else // ((rop3 >> 4) != (rop3 & 0xF))
            {
                //
                // The Rop3 does require a pattern
                //

                if (psoSrc->dhsurf == NULL)
                {
                    //////////////////////////////////////////////////
                    // DIB-to-screen blt

                    // This section handles 1bpp, 4bpp and 8bpp sources.
                    // 1bpp should have 'ulHwForeMix' and 'ulHwBackMix' the
                    // same values, and 4bpp and 8bpp ignore 'ulHwBackMix'.

                    ulHwForeMix = rop3;
                    ulHwBackMix = ulHwForeMix;

                    iSrcBitmapFormat = psoSrc->iBitmapFormat;
                    if (iSrcBitmapFormat == BMF_1BPP)
                    {
                        pfnPatXfer = vMmPatXfer1bpp3D;
                        goto PatXfer_It;
                    }
                    else if ((iSrcBitmapFormat == ppdev->iBitmapFormat) &&
                             ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)))
                    {
                        pfnPatXfer = vMmPatXferNative3D;
                        goto PatXfer_It;
                    }
                }
                else // psoSrc->dhsurf != NULL
                {
                    if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL))
                    {
                    //
                    // Ensure that the pattern has been realized
                    //
                        if (pbo->iSolidColor == -1)
                        {
                            if (pbo->pvRbrush == NULL)
                            {
                                if (BRUSHOBJ_pvGetRbrush(pbo) == NULL)
                                {
                                    goto Punt_It;
                                }
                            }
                        }

                        //////////////////////////////////////////////////
                        // Screen-to-screen blt with no translate

                        ulHwForeMix = rop3;

                        if (jClip == DC_TRIVIAL)
                        {
                            WAIT_DMA_IDLE( ppdev );
                            TRIANGLE_WORKAROUND( ppdev );

                            vMmPatCopyBlt3D (ppdev, pohDst, pohSrc, 1, prclDst,
                                             ulHwForeMix, pptlSrc, prclDst,
                                             pbo, pptlBrush);
                            goto All_Done;
                        }
                        else if (jClip == DC_RECT)
                        {
                            if (bIntersect(prclDst, &pco->rclBounds, &rcl))
                            {
                                WAIT_DMA_IDLE( ppdev );
                                TRIANGLE_WORKAROUND( ppdev );

                                vMmPatCopyBlt3D (ppdev, pohDst, pohSrc, 1, &rcl,
                                                 ulHwForeMix, pptlSrc, prclDst,
                                                 pbo, pptlBrush);
                            }
                            goto All_Done;
                        }
                        else
                        {
                        // Don't forget that we'll have to draw the
                        // rectangles in the correct direction:
                        //
                            if ((pptlSrc->y - (pohDst->y - pohSrc->y)) >=
                                    prclDst->top)
                            {
                                if ((pptlSrc->x - (pohDst->x - pohSrc->x)) >=
                                        prclDst->left)
                                {
                                    iDir = CD_RIGHTDOWN;
                                }
                                else
                                {
                                    iDir = CD_LEFTDOWN;
                                }
                            }
                            else
                            {
                                if ((pptlSrc->x - (pohDst->x - pohSrc->x)) >=
                                        prclDst->left)
                                {
                                    iDir = CD_RIGHTUP;
                                }
                                else
                                {
                                    iDir = CD_LEFTUP;
                                }
                            }

                            CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES,
                                               iDir, 0);

                            WAIT_DMA_IDLE( ppdev );
                            TRIANGLE_WORKAROUND( ppdev );

                            do
                            {
                                bMore = CLIPOBJ_bEnum(pco, sizeof(ce),
                                                      (ULONG*) &ce);

                                c = cIntersect(prclDst, ce.arcl, ce.c);

                                if (c != 0)
                                {
                                    vMmPatCopyBlt3D (ppdev, pohDst, pohSrc, c,
                                                     ce.arcl, ulHwForeMix,
                                                     pptlSrc, prclDst, pbo,
                                                     pptlBrush);
                                }

                            } while (bMore);
                            goto All_Done;
                        }
                    }
                } // end else psoSrc->dhsurf != NULL
            } // end else ((rop3 >> 4) != (rop3 & 0xF))
        } // end if (psoDst->dhsurf != NULL)
    //
    //  Destination is a DIB
    //
        else
        {
            #if defined(i386)
            {
                // We special case screen to monochrome blts because they
                // happen fairly often.  We only handle SRCCOPY rops and
                // monochrome destinations (to handle a true 1bpp DIB
                // destination, we would have to do near-colour searches
                // on every colour; as it is, the foreground colour gets
                // mapped to '1', and everything else gets mapped to '0'):

                if ((psoDst->iBitmapFormat == BMF_1BPP)     &&
                    (rop3 == SRC_COPY)                      &&
                    (psoSrc->iBitmapFormat != BMF_24BPP)    &&
                    (pxlo->flXlate & XO_TO_MONO))
                {
                    pfnXfer = vXferScreenTo1bpp;
                    psoSrc  = psoDst;               // A misnomer, I admit
                    pohDst  = pohSrc;

                    goto Xfer_It;
                }
            }
            #endif // i386
        }
    }

    //
    //  We know that either the source or destination is not a DIB
    //

Punt_It:

    if ((psoSrc == NULL) || (psoSrc->dhsurf == NULL))
    {
        //ASSERTDD(psoDst->iType != STYPE_BITMAP,
        //        "Dest should be the screen when given a DIB source");

        //
        // Do a memory-to-screen blt:
        //

        ppdev               = (PDEV *) psoDst->dhpdev;
        psoNewDst           = ppdev->psoPunt;
        psoNewDst->pvScan0  = pdsurfDst->poh->pvScan0;
        psoNewDst->lDelta   = pdsurfDst->poh->lDelta;
        psoNewSrc = psoSrc;
    }
    else
    {
        //
        //  We know the screen is the source
        //

        ppdev               = (PDEV *) psoSrc->dhpdev;
        pdsurfSrc           = (DSURF*) psoSrc->dhsurf;
        psoNewSrc           = ppdev->psoPunt;
        psoNewSrc->pvScan0  = pdsurfSrc->poh->pvScan0;
        psoNewSrc->lDelta   = pdsurfSrc->poh->lDelta;

        if (psoDst->dhsurf == NULL)
        {
            //
            // a Screen-to-DIB blt
            //

            psoNewDst = psoDst;
        }
        else
        {
            //
            // a Screen-to-Screen blt
            //

            psoNewDst           = ppdev->psoPunt2;
            psoNewDst->pvScan0  = pdsurfDst->poh->pvScan0;
            psoNewDst->lDelta   = pdsurfDst->poh->lDelta;

        }
    }

    //
    // Note: - This will only work if we have dense space!!!
    //

    return (EngBitBlt (psoNewDst, psoNewSrc, psoMsk, pco, pxlo,
                       prclDst, pptlSrc, pptlMsk, pbo, pptlBrush,
                       rop4));

///////////////////////////////////////////////////////////////////////
// Bitmap transfer with a pattern
//

PatXfer_It:
    //
    // Ensure that the pattern has been realized
    if (pbo->iSolidColor == -1)
    {
        if (pbo->pvRbrush == NULL)
        {
            if (BRUSHOBJ_pvGetRbrush(pbo) == NULL)
            {
                goto Punt_It;
            }
        }
    }

    //
    // Iterate over the necessary destination rectangles.
    //
    if (jClip == DC_TRIVIAL)
    {
        WAIT_DMA_IDLE( ppdev );
        TRIANGLE_WORKAROUND( ppdev );

        pfnPatXfer(ppdev, pohDst, 1, prclDst, ulHwForeMix, ulHwBackMix, psoSrc,
                    pptlSrc, prclDst, pxlo, pbo, pptlBrush);
        goto All_Done;
    }
    else if (jClip == DC_RECT)
    {
        if (bIntersect(prclDst, &pco->rclBounds, &rcl))
        {
            WAIT_DMA_IDLE( ppdev );
            TRIANGLE_WORKAROUND( ppdev );

            pfnPatXfer(ppdev, pohDst, 1, &rcl, ulHwForeMix, ulHwBackMix,
                        psoSrc, pptlSrc, prclDst, pxlo, pbo, pptlBrush);
        }
        goto All_Done;
    }
    else
    {
        CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES,
                           CD_ANY, 0);

        WAIT_DMA_IDLE( ppdev );
        TRIANGLE_WORKAROUND( ppdev );

        do {
            bMore = CLIPOBJ_bEnum(pco, sizeof(ce),
                                  (ULONG*) &ce);

            c = cIntersect(prclDst, ce.arcl, ce.c);

            if (c != 0)
            {
                pfnPatXfer(ppdev, pohDst, c, ce.arcl, ulHwForeMix, ulHwBackMix,
                            psoSrc, pptlSrc, prclDst, pxlo, pbo, pptlBrush);
            }

        } while (bMore);
        goto All_Done;
    }


//////////////////////////////////////////////////////////////////////
// Common bitmap transfer

Xfer_It:
    if (jClip == DC_TRIVIAL)
    {
        WAIT_DMA_IDLE( ppdev );
        TRIANGLE_WORKAROUND( ppdev );

        pfnXfer(ppdev, pohDst, 1, prclDst, ulHwForeMix, ulHwBackMix, psoSrc,
                pptlSrc, prclDst, pxlo);
        goto All_Done;
    }
    else if (jClip == DC_RECT)
    {
        if (bIntersect(prclDst, &pco->rclBounds, &rcl))
        {
            WAIT_DMA_IDLE( ppdev );
            TRIANGLE_WORKAROUND( ppdev );

            pfnXfer(ppdev, pohDst, 1, &rcl, ulHwForeMix, ulHwBackMix, psoSrc,
                    pptlSrc, prclDst, pxlo);
        }
        goto All_Done;
    }
    else
    {
        CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES,
                           CD_ANY, 0);

        WAIT_DMA_IDLE( ppdev );
        TRIANGLE_WORKAROUND( ppdev );

        do {
            bMore = CLIPOBJ_bEnum(pco, sizeof(ce),
                                  (ULONG*) &ce);

            c = cIntersect(prclDst, ce.arcl, ce.c);

            if (c != 0)
            {
                pfnXfer(ppdev, pohDst, c, ce.arcl, ulHwForeMix, ulHwBackMix,
                        psoSrc, pptlSrc, prclDst, pxlo);
            }

        } while (bMore);
        goto All_Done;
    }


////////////////////////////////////////////////////////////////////////
// Common DIB blt

EngBitBlt_It:

    // Our driver doesn't handle any blt's between two DIBs.  Normally
    // a driver doesn't have to worry about this, but we do because
    // we have DFBs that may get moved from off-screen memory to a DIB,
    // where we have GDI do all the drawing.  GDI does DIB drawing at
    // a reasonable speed (unless one of the surfaces is a device-
    // managed surface...)
    //
    // If either the source or destination surface in an EngBitBlt
    // call-back is a device-managed surface (meaning it's not a DIB
    // that GDI can draw with), GDI will automatically allocate memory
    // and call the driver's DrvCopyBits routine to create a DIB copy
    // that it can use.  So this means that this could handle all 'punts',
    // and we could conceivably get rid of bPuntBlt.  But this would have
    // a bad performance impact because of the extra memory allocations
    // and bitmap copies -- you really don't want to do this unless you
    // have to (or your surface was created such that GDI can draw
    // directly onto it) -- I've been burned by this because it's not
    // obvious that the performance impact is so bad.
    //
    // That being said, we only call EngBitBlt when all the surfaces
    // are DIBs:
    //
    //  since the DIB engine will write directly to the frame buffer, we
    //  need to make sure the GE is idle if the bitmap is device managed
    //

    if (psoDst->iType == STYPE_DEVICE)
    {
        ppdev     = (PDEV*) psoDst->dhpdev;
        psoNewDst = ppdev->psoBank;
    }
    else
    {
        psoNewDst = psoDst;
    }

    if (psoSrc != NULL)
    {
        if (psoSrc->iType == STYPE_DEVICE)
        {
            ppdev = (PDEV*) psoSrc->dhpdev;
            psoNewSrc = ppdev->psoBank;
        }
        else
        {
            psoNewSrc = psoSrc;
        }
    }
    else
    {
        psoNewSrc = NULL;
    }

    return (EngBitBlt( psoNewDst, psoNewSrc, psoMsk, pco, pxlo, prclDst,
                       pptlSrc, pptlMsk, pbo, pptlBrush, rop4));

//-----------------------------------------------------------------------------
// Common exit point (for handled BitBlts)
//
All_Done:
    return (TRUE);

} // DrvBitBlt ()


//-----------------------------------------------------------------------------
// PROCEDURE:   BOOL DrvCopyBits:
//
// DESCRIPTION: Performs fast bitmap copies between screen and system memory.
//
//              Note that GDI will (usually) automatically adjust the blt
//              extents to adjust for any rectangular clipping, so we'll
//              rarely see DC_RECT clipping in this routine (and as such,
//              we don't bother special casing it).
//
//              I'm not sure if the performance benefit from this routine
//              is actually worth the increase in code size, since SRCCOPY
//              BitBlts are hardly the most common drawing operation we'll
//              get.  But what the heck.
//
//              On the S3 it's faster to do SRCCOPY bitblt's through the
//              memory aperture than to use the data transfer register; as
//              such, this routine is the logical place to put this special
//              case.
//
// ASSUMPTION:  None
//
// CALLS:       bMoveDibToOffscreenDfbIfRoom ()
//              DrvBitBlt ()
//              EngCopyBits ()
//              ppdev->pfnCopyBlt ()
//              vGetBits ()
//              vPutBits ()
//
// NOTES:       None
//-----------------------------------------------------------------------------
// HISTORY:
//-----------------------------------------------------------------------------
//
BOOL DrvCopyBits(
SURFOBJ*  psoDst,                       // Destination surface object
SURFOBJ*  psoSrc,                       // Source      surface object
CLIPOBJ*  pco,                          // Clipping object (if !NULL)
XLATEOBJ* pxlo,                         // Translation object (if !NULL)
RECTL*    prclDst,                      // Destination rectangle
POINTL*   pptlSrc)                      // Source left/top point
{
    PDEV*   ppdev;
    DSURF*  pdsurfSrc;
    DSURF*  pdsurfDst;
    RECTL   rcl;
    POINTL  ptl;
    OH*     pohSrc;
    OH*     pohDst;
    BYTE*   pjDst;
    BYTE*   pjSrc;
    LONG    lSrcDelta;
    LONG    lDstDelta;
    LONG    lPelBytes;
    LONG    cjScan;
    LONG    cyScan;

    // DrvCopyBits is a fast-path for SRCCOPY blts.  But it can still be
    // pretty complicated: there can be translates, clipping, RLEs,
    // bitmaps that aren't the same format as the screen, plus
    // screen-to-screen, DIB-to-screen or screen-to-DIB operations,
    // not to mention DFBs (device format bitmaps).
    //
    // Rather than making this routine almost as big as DrvBitBlt, I'll
    // handle here only the speed-critical cases, and punt the rest to
    // our DrvBitBlt routine.
    //
    // We'll try to handle anything that doesn't involve clipping:
    //
    if (((pxlo != NULL) && ((pxlo->flXlate & XO_TRIVIAL) == 0)) &&
        (psoSrc->iType != STYPE_BITMAP) && (((DSURF*) psoSrc->dhsurf)->dt == DT_SCREEN)
        && (((DSURF*) psoSrc->dhsurf)->poh->ohState == OH_DISCARDABLE))
        {
            pohMoveOffscreenDfbToDib((PDEV*) psoSrc->dhpdev, ((DSURF*)psoSrc->dhsurf)->poh);

        }

    pdsurfDst = (DSURF*) psoDst->dhsurf;

    if (((pco  == NULL) || (pco->iDComplexity == DC_TRIVIAL)) &&
        ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)))
    {
        if (psoDst->dhsurf != NULL)
        {
            // We know the destination is either a DFB or the screen:

            ppdev     = (PDEV*)  psoDst->dhpdev;
            pdsurfDst = (DSURF*) psoDst->dhsurf;

            // See if the source is a plain DIB:

            if (psoSrc->dhsurf != NULL)
            {
            //
            //  the source is the screen or a DFB
            //
                pdsurfSrc = (DSURF*) psoSrc->dhsurf;

                // Make sure the destination is really the screen or an
                // off-screen DFB (i.e., not a DFB that we've converted
                // to a DIB):

                if (pdsurfDst->dt == DT_SCREEN)
                {
                    ASSERTDD(psoSrc->dhsurf != NULL, "Can't be a DIB");


                    if (pdsurfSrc->dt == DT_SCREEN)
                    {

                    Screen_To_Screen:

                        //////////////////////////////////////////////////////
                        // Screen-to-screen

                        ASSERTDD((psoSrc->dhsurf != NULL) &&
                                 (pdsurfSrc->dt == DT_SCREEN)    &&
                                 (psoDst->dhsurf != NULL) &&
                                 (pdsurfDst->dt == DT_SCREEN),
                                 "Should be a screen-to-screen case");

                        pohSrc = pdsurfSrc->poh;
                        pohDst = pdsurfDst->poh;

                        WAIT_DMA_IDLE( ppdev );
                        TRIANGLE_WORKAROUND( ppdev );

                        ppdev->pfnCopyBlt (ppdev, pohDst, pohSrc, 1, prclDst,
                                           SRC_COPY, pptlSrc, prclDst);

                        return(TRUE);
                    }
                    else // (pdsurfSrc->dt != DT_SCREEN)
                    {
                        // Ah ha, the source is a DFB that's really a DIB.

                        ASSERTDD(psoDst->dhsurf != NULL,
                                "Destination can't be a DIB here");

                        /////////////////////////////////////////////////////
                        // Put It Back Into Off-screen?
                        //
                        // We take this opportunity to decide if we want to
                        // put the DIB back into off-screen memory.  This is
                        // a pretty good place to do it because we have to
                        // copy the bits to some portion of the screen,
                        // anyway.  So we would incur only an extra screen-to-
                        // screen blt at this time, much of which will be
                        // over-lapped with the CPU.
                        //
                        // The simple approach we have taken is to move a DIB
                        // back into off-screen memory only if there's already
                        // room -- we won't throw stuff out to make space
                        // (because it's tough to know what ones to throw out,
                        // and it's easy to get into thrashing scenarios).
                        //
                        // Because it takes some time to see if there's room
                        // in off-screen memory, we only check one in
                        // HEAP_COUNT_DOWN times if there's room.  To bias
                        // in favour of bitmaps that are often blt, the
                        // counters are reset every time any space is freed
                        // up in off-screen memory.  We also don't bother
                        // checking if no space has been freed since the
                        // last time we checked for this DIB.

                        if (pdsurfSrc->iUniq == ppdev->iHeapUniq)
                        {
                            if (--pdsurfSrc->cBlt == 0)
                            {
                                if (bMoveDibToOffscreenDfbIfRoom(ppdev,
                                                                 pdsurfSrc))
                                {
                                    goto Screen_To_Screen;
                                }
                            }
                        }
                        else
                        {
                            // Some space was freed up in off-screen memory,
                            // so reset the counter for this DFB:

                            pdsurfSrc->iUniq = ppdev->iHeapUniq;
                            pdsurfSrc->cBlt  = HEAP_COUNT_DOWN;
                        }

                        // Since the destination is definitely the screen,
                        // we don't have to worry about creating a DIB to
                        // DIB copy case (for which we would have to call
                        // EngCopyBits):

                        psoSrc = pdsurfSrc->pso;

                        goto DIB_To_Screen;
                    }
                }
                else // (pdsurfDst->dt != DT_SCREEN)
                {
                    // Because the source is not a DIB, we don't have to
                    // worry about creating a DIB to DIB case here (although
                    // we'll have to check later to see if the source is
                    // really a DIB that's masquerading as a DFB...)

                    ASSERTDD(psoSrc->dhsurf != NULL,
                             "Source can't be a DIB here");

                    psoDst = pdsurfDst->pso;

                    goto Screen_To_DIB;
                }
            }
            else if (psoSrc->iBitmapFormat == ppdev->iBitmapFormat)
            {
                // Make sure the destination is really the screen:

                if (pdsurfDst->dt == DT_SCREEN)
                {

                DIB_To_Screen:

                    //////////////////////////////////////////////////////
                    // DIB-to-screen

                    ASSERTDD((psoDst->dhsurf != NULL) &&
                             (pdsurfDst->dt == DT_SCREEN)    &&
                             (psoSrc->dhsurf == NULL) &&
                             (psoSrc->iBitmapFormat == ppdev->iBitmapFormat),
                             "Should be a DIB-to-screen case");

                    // vPutBits takes absolute screen coordinates, so
                    // we have to muck with the destination rectangle:

                    pohDst = pdsurfDst->poh;

                    // We use the memory aperture to do the transfer,
                    // because that is supposed to be faster for SRCCOPY
                    // blts than using the data-transfer register:

                    lPelBytes = CONVERT_TO_BYTES(1, ppdev);
                    lSrcDelta = psoSrc->lDelta;
                    lDstDelta = pohDst->lDelta;

                    pjDst = (BYTE *)pohDst->pvScan0;
                    pjSrc = (BYTE *)psoSrc->pvScan0;

                    cjScan = (prclDst->right - prclDst->left) * lPelBytes;
                    cyScan = prclDst->bottom - prclDst->top;

                    pjDst += (prclDst->top * lDstDelta + prclDst->left * lPelBytes);
                    pjSrc += (pptlSrc->y * lSrcDelta + pptlSrc->x * lPelBytes);

                    //
                    //  make sure the engine is done drawing to the frame buffer
                    //
                    // EKL - Driver is synchronized with GDI
                    // S3DGPWait ( ppdev);

                    WAIT_DMA_IDLE( ppdev );
                    TRIANGLE_WORKAROUND( ppdev );

                    vAlignedCopy(
                        ppdev,
                        &pjDst,
                        lDstDelta,
                        &pjSrc,
                        lSrcDelta,
                        cjScan,
                        cyScan,
                        TRUE);

                    return(TRUE);
                }
            }
        }
        else // (psoDst->dhsurf == NULL)
        {

        Screen_To_DIB:

            // Now we are using engine managed surface so both Src and Dst
            // can be STYPE_BITMAPs in that case - punt.

            //if (psoSrc->iType == STYPE_BITMAP)
            //    goto EngCopyBits_It;

            pdsurfSrc = (DSURF*) psoSrc->dhsurf;
            ppdev     = (PDEV*)  psoSrc->dhpdev;

            if (psoDst->iBitmapFormat == ppdev->iBitmapFormat)
            {
                if (pdsurfSrc->dt == DT_SCREEN)
                {
                    //////////////////////////////////////////////////////
                    // Screen-to-DIB

                    ASSERTDD((psoSrc->dhsurf != NULL) &&
                             (pdsurfSrc->dt == DT_SCREEN)    &&
                             (psoDst->dhsurf == NULL) &&
                             (psoDst->iBitmapFormat == ppdev->iBitmapFormat),
                             "Should be a screen-to-DIB case");

                    // vGetBits takes absolute screen coordinates, so we have
                    // to muck with the source point:

                    pohSrc = pdsurfSrc->poh;
                    // We use the memory aperture to do the transfer,
                    // because that is supposed to be faster for SRCCOPY
                    // blts than using the data-transfer register:

                    lPelBytes = CONVERT_TO_BYTES(1, ppdev);
                    lSrcDelta = pohSrc->lDelta;
                    lDstDelta = psoDst->lDelta;

                    pjDst = (BYTE *)psoDst->pvScan0;
                    pjSrc = (BYTE *)pohSrc->pvScan0;

                    cjScan = (prclDst->right - prclDst->left) * lPelBytes;
                    cyScan = prclDst->bottom - prclDst->top;

                    pjDst += (prclDst->top * lDstDelta + prclDst->left * lPelBytes);
                    pjSrc += (pptlSrc->y * lSrcDelta + pptlSrc->x * lPelBytes);

                    //
                    //  make sure the engine is done drawing to the frame buffer
                    //
                    // EKL - Driver is synchronized with GDI
                    // S3DGPWait ( ppdev);

                    WAIT_DMA_IDLE( ppdev );
                    TRIANGLE_WORKAROUND( ppdev );

                    vAlignedCopy(
                        ppdev,
                        &pjDst,
                        lDstDelta,
                        &pjSrc,
                        lSrcDelta,
                        cjScan,
                        cyScan,
                        FALSE);

                    return(TRUE);
                }
                else
                {
                    // The source is a DFB that's really a DIB.  Since we
                    // know that the destination is a DIB, we've got a DIB
                    // to DIB operation, and should call EngCopyBits:

                    psoSrc = pdsurfSrc->pso;
                    goto EngCopyBits_It;
                }
            }
        }
    }

    // We can't call DrvBitBlt if we've accidentally converted both
    // surfaces to DIBs, because it isn't equipped to handle it:

    //if ((psoSrc->iType == STYPE_BITMAP) && (psoDst->iType == STYPE_BITMAP))
    //{
    //    goto EngCopyBits_It;
    //}

    ASSERTDD((psoSrc->dhsurf != NULL) ||
             (psoDst->dhsurf != NULL),
             "Accidentally converted both surfaces to DIBs");

    /////////////////////////////////////////////////////////////////
    // A DrvCopyBits is after all just a simplified DrvBitBlt:

    return(DrvBitBlt(psoDst, psoSrc, NULL, pco, pxlo, prclDst, pptlSrc, NULL,
                     NULL, NULL, 0x0000CCCC));

EngCopyBits_It:

    ASSERTDD((psoDst->dhsurf == NULL) &&
             (psoSrc->dhsurf == NULL),
             "Both surfaces should be DIBs to call EngCopyBits");

    return(EngCopyBits(psoDst, psoSrc, pco, pxlo, prclDst, pptlSrc));

} // DrvCopyBits ()


