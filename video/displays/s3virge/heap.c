//****************************************************************************
// Module Name: heap.c
//
// This module contains the routines for a linear heap.  It is used primarily
// for allocating space for device-format-bitmaps in off-screen memory.
//
// Off-screen bitmaps are a big deal on NT because:
//
//    1) It reduces the working set.  Any bitmap stored in off-screen
//       memory is a bitmap that isn't taking up space in main memory.
//
//    2) There is a speed win by using the accelerator hardware for
//       drawing, in place of NT's GDI code.  NT's GDI is written entirely
//       in 'C++' and perhaps isn't as fast as it could be.
//
//    3) It leads naturally to nifty tricks that can take advantage of
//       the hardware, such as MaskBlt support and cheap double buffering
//       for OpenGL.
//
// When the heap gets full, old allocations will automatically be punted
// from off-screen and copied to DIBs, which we'll let GDI draw on.
//
// Copyright (c) 1993-1996  Microsoft Corporation
// Copyright (c) 1996       S3 Inc.
//****************************************************************************

#include "precomp.h"
#include "hw3d.h"

#define OH_ALLOC_SIZE   4000        // Do all memory allocations in 4k chunks
#define SENTINEL        0x7fffffff  // The sentinel at the end of the available

                                    //  list has this very large 'cxcy' value

/******************************Public*Routine******************************\
* OH* pohNewNode
*
* Allocates a basic memory unit in which we'll pack our data structures.
*
* Since we'll have a lot of OH nodes, most of which we will be
* occasionally traversing, we do our own memory allocation scheme to
* keep them densely packed in memory.
*
* It would be the worst possible thing for the working set to simply
* call EngAllocMem(sizeof(OH)) every time we needed a new node.  There
* would be no locality; OH nodes would get scattered throughout memory,
* and as we traversed the available list for one of our allocations,
* it would be far more likely that we would hit a hard page fault.
\**************************************************************************/

OH* pohNewNode(
PDEV*   ppdev)
{
    LONG     i;
    LONG     cOhs;
    OHALLOC* poha;
    OH*      poh;

    if (ppdev->heap.pohFreeList == NULL)
    {
        // We zero-init to initialize all the OH flags, and to help in
        // debugging (we can afford to do this since we'll be doing this
        // very infrequently):

        poha = EngAllocMem(FL_ZERO_MEMORY, OH_ALLOC_SIZE, ALLOC_TAG);
        if (poha == NULL)
            return(NULL);

        // Insert this OHALLOC at the begining of the OHALLOC chain:

        poha->pohaNext  = ppdev->heap.pohaChain;
        ppdev->heap.pohaChain = poha;

        // This has a '+ 1' because OHALLOC includes an extra OH in its
        // structure declaration:

        cOhs = (OH_ALLOC_SIZE - sizeof(OHALLOC)) / sizeof(OH) + 1;

        // The big OHALLOC allocation is simply a container for a bunch of
        // OH data structures in an array.  The new OH data structures are
        // linked together and added to the OH free list:

        poh = &poha->aoh[0];
        for (i = cOhs - 1; i != 0; i--)
        {
            poh->pohNext = poh + 1;
            poh          = poh + 1;
        }

        poh->pohNext      = NULL;
        ppdev->heap.pohFreeList = &poha->aoh[0];
    }

    poh = ppdev->heap.pohFreeList;
    ppdev->heap.pohFreeList = poh->pohNext;

    return(poh);
}

/******************************Public*Routine******************************\
* VOID vOhFreeNode
*
* Frees our basic data structure allocation unit by adding it to a free
* list.
*
\**************************************************************************/

VOID vOhFreeNode(
PDEV*   ppdev,
OH*     poh)
{
    if (poh == NULL)
        return;

    poh->pohNext            = ppdev->heap.pohFreeList;
    ppdev->heap.pohFreeList = poh;
    poh->ohState            = -1;
}

/******************************Public*Routine******************************\
* VOID vOhDeleteNode
*
* To remove a node from a list.
*
\**************************************************************************/

VOID vOhDeleteNode(OH * poh)
{
    OH* pohPrev;
    OH* pohNext;


    // dlist for LRU
    pohPrev = poh->pohPrev;
    pohNext = poh->pohNext;

    pohPrev->pohNext = pohNext;
    pohNext->pohPrev = pohPrev;

}

/******************************Public*Routine******************************\
* VOID vOhInsertNode
*
* To insert a node to a list.
*
\**************************************************************************/
VOID vOhInsertNode(OH * pohList, OH * poh)
{
    OH* pohTemp;
    OH* pohThis;


    // keep double links for insert and delete
    pohTemp = pohList->pohNext;

    pohList->pohNext = poh;
    pohTemp->pohPrev = poh;

    poh->pohPrev = pohList;
    poh->pohNext = pohTemp;

}

/******************************Public*Routine******************************\
* OH* pohFree
*
* Frees an off-screen heap allocation.  The free space will be combined
* with any adjacent free spaces in order to speed up the procedure.
*
\**************************************************************************/

OH* pohFree(
PDEV*   ppdev,
OH*     poh)
{

    OH* pohThis;
    OH* pohPrev;
    OH* pohNext;

    ULONG ulUpper;
    ULONG ulLower;
    ULONG ulThis;


    if (poh == NULL)
        return(NULL);

    DISPDBG((1, "Freeing %li x %li at (%li, %li)",
            poh->cx, poh->cy, poh->x, poh->y));

    if(poh->ohState == OH_PERMANENT)
    {
        ppdev->heap.ulMaxOffscrnSize += poh->ulSize;
    }

    ppdev->heap.ulMaxFreeSize += poh->ulSize;
    poh->ohState      = OH_FREE;

    pohThis = ppdev->heap.ohFree.pohNext;

    pohPrev = pohNext = NULL;
    ulUpper = poh->ulBase + poh->ulSize;
    ulLower = poh->ulBase;

    while(pohThis != &ppdev->heap.ohFree)
    {
        if( pohThis->ulBase == ulUpper)
        {
            // upper adjacent free space
            pohNext = pohThis;
        }
        else if( (pohThis->ulBase + pohThis->ulSize) == ulLower)
        {

            // lower adjacent free space
            pohPrev = pohThis;
        }
        pohThis = pohThis->pohNext;
    }

    // Deleting from discardable list
    vOhDeleteNode(poh);

    if(pohPrev && pohNext)
    {
        pohPrev->ulSize += (poh->ulSize + pohNext->ulSize);
        vOhDeleteNode(pohNext);
        vOhFreeNode(ppdev, poh);
        vOhFreeNode(ppdev, pohNext);
        return(pohPrev);
    }
    else if(pohPrev)
    {
        pohPrev->ulSize += poh->ulSize;
        vOhFreeNode(ppdev, poh);
        return(pohPrev);
    }
    else if(pohNext)
    {
        pohNext->ulSize += poh->ulSize;
        pohNext->ulBase =  poh->ulBase;
        vOhFreeNode(ppdev, poh);
        return(pohNext);
    }
    else
    {
        // Adding to the free list
        vOhInsertNode(&ppdev->heap.ohFree, poh);
    }

    return(poh);
}


/******************************Public*Routine******************************\
* OH* pohAllocate
*
* Allocates space for an off-screen rectangle.  It will attempt to find
* the first available space in the free list.
*
*
\**************************************************************************/

OH* pohAllocate(
PDEV*   ppdev,
LONG    cxThis,             // Width of rectangle to be allocated
LONG    cyThis,             // Height of rectangle to be allocated
FLOH   floh)               // May have FLOH_ONLY_IF_ROOM set
{
    OH*   pohTemp;
    OH*   pohThis;          // Points to found available rectangle we'll use
    OH*   pohRoot;
    ULONG ulSize;
    BOOL  bAllocated = FALSE;
    LONG  lDelta;

    ASSERTDD((cxThis > 0) && (cyThis > 0), "Illegal allocation size");

    // Increase the width to get the proper alignment (thus ensuring that all
    // allocations will be properly aligned):


    lDelta = CONVERT_TO_BYTES(cxThis, ppdev);

    lDelta = (lDelta + (HEAP_X_ALIGNMENT - 1)) & ~(HEAP_X_ALIGNMENT - 1);

    if ((!ppdev->bAllocFromDD) && ((lDelta & 0xFFFFF000) || (cyThis & 0xFFFFF800)))
    {
        DISPDBG((1, "Stride (%li) is too large for 12-bit stride reg.", lDelta));
        DISPDBG((1, "or Height (%li) is too big for 11-bit height reg.", cyThis));

        return(NULL);
    }    

    cxThis = lDelta / (CONVERT_TO_BYTES(1, ppdev));

    ulSize = lDelta * cyThis;


    // It is too big.
    if (ulSize > ppdev->heap.ulMaxOffscrnSize)
    {
        DISPDBG((1, "Can't allocate %li x %li with flags %li",
                        cxThis, cyThis, floh));
        return(NULL);
    }




    if(floh != FLOH_MAKE_PERMANENT)
        if( (cxThis * cyThis) >= 756000L)
        {

            DISPDBG((1, "Bitmap (%li x %li) is too large.",
                        cxThis, cyThis));
            return(NULL);

        }

    pohThis = ppdev->heap.ohFree.pohNext;

    while(pohThis != &ppdev->heap.ohFree)
    {
        if(ulSize <= pohThis->ulSize)
        {
            bAllocated = TRUE;
            break;
        }
        pohThis = pohThis->pohNext;
    }

    if(!bAllocated)
    {
        // Least-recently created
        pohThis = ppdev->heap.ohDiscardable.pohPrev;

        pohRoot = ppdev->heap.ohFree.pohPrev;

        while((!pohRoot) || (ulSize > pohRoot->ulSize))
        {
            pohTemp = pohThis;
            pohThis = pohThis->pohPrev;
            if(pohTemp == &ppdev->heap.ohDiscardable)
             return(NULL);
            if(pohTemp->ohState != OH_PERMANENT)
            {
                pohRoot = pohMoveOffscreenDfbToDib(ppdev, pohTemp);
            }
        }

        pohThis = pohRoot;

    }

    if(ulSize == pohThis->ulSize)
    {
        pohRoot = pohThis;
        // remove it from free list
        vOhDeleteNode(pohThis);
    }
    else
    {
        pohRoot = pohNewNode(ppdev);
        if (pohRoot == NULL) 
        {
            DISPDBG((1, "Cannot allocate a new node"));
            return (NULL);
        }
        pohRoot->ulBase = pohThis->ulBase;
        pohThis->ulBase += ulSize;
        pohThis->ulSize -= ulSize;
    }

    if(floh == FLOH_MAKE_PERMANENT)
    {
        ppdev->heap.ulMaxOffscrnSize -= ulSize;
        pohRoot->ohState = OH_PERMANENT;
    }
    else
    {
        pohRoot->ohState = OH_DISCARDABLE;
    }

    ppdev->heap.ulMaxFreeSize -= ulSize;

    pohRoot->ulSize = ulSize;
    pohRoot->cx = cxThis;
    pohRoot->cy = cyThis;
    pohRoot->lDelta = lDelta;
    pohRoot->y = 0;
    pohRoot->x = 0;

//    pohRoot->y = pohRoot->ulBase / ppdev->lDelta;
//    pohRoot->x = (pohRoot->ulBase % ppdev->lDelta) / ppdev->cPelBytes;



    pohThis = &ppdev->heap.ohDiscardable;
    vOhInsertNode(pohThis, pohRoot);

//
// Calculate the linear address for this bitmap.
//
    pohRoot->pvScan0 = pohRoot->ulBase + ppdev->pjScreen;

//
// The caller is responsible for setting this field:
//
    pohRoot->pdsurf = NULL;

    DISPDBG((1, "Allocated %li x %li at (%li, %li) with flags %li",
                        pohRoot->cx, pohRoot->cy,
                        pohRoot->x, pohRoot->y,
                        floh));

    return(pohRoot);
}


/******************************Public*Routine******************************\
* BOOL bMoveDibToOffscreenDfbIfRoom
*
* Converts the DIB DFB to an off-screen DFB, if there's room for it in
* off-screen memory.
*
* Returns: FALSE if there wasn't room, TRUE if successfully moved.
*
\**************************************************************************/

BOOL bMoveDibToOffscreenDfbIfRoom(
PDEV*   ppdev,
DSURF*  pdsurf)
{
    OH*         poh;
    SURFOBJ*    pso;
    HSURF       hsurf;
    SURFOBJ*    psoSrc;

    BYTE*   pjDst;
    BYTE*   pjSrc;

    ASSERTDD(pdsurf->dt == DT_DIB,
             "Can't move a bitmap off-screen when it's already off-screen");

    // If we're in full-screen mode, we can't move anything to off-screen
    // memory:

    if (!ppdev->bEnabled)
        return(FALSE);

    poh = pohAllocate(ppdev, pdsurf->sizl.cx, pdsurf->sizl.cy,
                      FLOH_ONLY_IF_ROOM);
    if (poh == NULL)
    {
        // There wasn't any free room.

        return(FALSE);
    }
    pso           = pdsurf->pso;

    pjDst = (BYTE *)poh->pvScan0;
    pjSrc = (BYTE *)pso->pvScan0;

    WAIT_DMA_IDLE( ppdev );
    TRIANGLE_WORKAROUND( ppdev );

    vAlignedCopy(
        ppdev,
        &pjDst,
        poh->lDelta,
        &pjSrc,
        pso->lDelta,
        poh->lDelta,
        poh->cy,
        TRUE);

    // Update the data structures to reflect the new off-screen node:

    pdsurf->dt    = DT_SCREEN;
    pdsurf->poh   = poh;
    poh->pdsurf   = pdsurf;

    // Now free the DIB.  Get the hsurf from the SURFOBJ before we unlock
    // it (it's not legal to dereference psoDib when it's unlocked):

    hsurf = pso->hsurf;
    EngUnlockSurface(pso);
    EngDeleteSurface(hsurf);

    return(TRUE);
}

/******************************Public*Routine******************************\
* OH* pohMoveOffscreenDfbToDib
*
* Converts the DFB from being off-screen to being a DIB.
*
* Note: The caller does NOT have to call 'pohFree' on 'poh' after making
*       this call.
*
* Returns: NULL if the function failed (due to a memory allocation).
*          Otherwise, it returns a pointer to the coalesced off-screen heap
*          node that has been made available for subsequent allocations
*          (useful when trying to free enough memory to make a new
*          allocation).
\**************************************************************************/

OH* pohMoveOffscreenDfbToDib(
PDEV*   ppdev,
OH*     poh)
{
    DSURF*   pdsurf;
    HBITMAP  hbmDib;
    SURFOBJ* pso;
    RECTL    rclDst;
    SIZEL    sizl;
    POINTL   ptlSrc;

    BYTE*   pjDst;
    BYTE*   pjSrc;

    DISPDBG((1, "Throwing out %li x %li !",
                 poh->cx, poh->cy));

    pdsurf = poh->pdsurf;

    ASSERTDD(pdsurf->dt != DT_DIB,
            "Can't make a DIB into even more of a DIB");

    sizl.cx = poh->cx;
    sizl.cy = poh->cy;

    hbmDib = EngCreateBitmap(sizl, poh->lDelta, ppdev->iBitmapFormat,
                             BMF_TOPDOWN, NULL);
    if (hbmDib)
    {
        if (EngAssociateSurface((HSURF) hbmDib, ppdev->hdevEng, 0))
        {
            pso = EngLockSurface((HSURF) hbmDib);
            if (pso != NULL)
            {

                pdsurf->dt    = DT_DIB;
                pdsurf->pso   = pso;

                pjDst = (BYTE *)pso->pvScan0;
                pjSrc = (BYTE *)poh->pvScan0;

                WAIT_DMA_IDLE( ppdev );
                TRIANGLE_WORKAROUND( ppdev );

                vAlignedCopy(
                    ppdev,
                    &pjDst,
                    pso->lDelta,
                    &pjSrc,
                    poh->lDelta,
                    poh->lDelta,
                    poh->cy,
                    FALSE);
                // Don't even bother checking to see if this DIB should
                // be put back into off-screen memory until the next
                // heap 'free' occurs:

                pdsurf->iUniq = ppdev->iHeapUniq;
                pdsurf->cBlt  = 0;

                // Remove this node from the off-screen DFB list, and free
                // it.  'pohFree' will never return NULL:

                return(pohFree(ppdev, poh));
            }

        }

        // Fail case:

        EngDeleteSurface((HSURF) hbmDib);
    }

    return(NULL);
}

/******************************Public*Routine******************************\
* BOOL bMoveEverythingFromOffscreenToDibs
*
* This function is used when we're about to enter full-screen mode, which
* would wipe all our off-screen bitmaps.  GDI can ask us to draw on
* device bitmaps even when we're in full-screen mode, and we do NOT have
* the option of stalling the call until we switch out of full-screen.
* We have no choice but to move all the off-screen DFBs to DIBs.
*
* Returns TRUE if all DSURFs have been successfully moved.
*
\**************************************************************************/

BOOL bMoveAllDfbsFromOffscreenToDibs(
PDEV*   ppdev)
{
    BOOL bRet;
    OH*  poh;
    OH*  pohNext;

    bRet = TRUE;        // Assume success

    poh = ppdev->heap.ohDiscardable.pohNext;
    while (poh != &ppdev->heap.ohDiscardable)
    {
        pohNext = poh->pohNext;

        if(poh->ohState == OH_DISCARDABLE)
        {
            if(!pohMoveOffscreenDfbToDib(ppdev, poh))
                bRet = FALSE;
        }

        poh = pohNext;
    }

    return(bRet);

}

/******************************Public*Routine******************************\
* HBITMAP DrvCreateDeviceBitmap
*
* Function called by GDI to create a device-format-bitmap (DFB).  We will
* always try to allocate the bitmap in off-screen; if we can't, we simply
* fail the call and GDI will create and manage the bitmap itself.
*
* Note: We do not have to zero the bitmap bits.  GDI will automatically
*       call us via DrvBitBlt to zero the bits (which is a security
*       consideration).
*
\**************************************************************************/

#define IOCTL_VIDEO_S3_COLOR_ADJUST_STATUS                                              \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x830, METHOD_BUFFERED, FILE_ANY_ACCESS)



HBITMAP DrvCreateDeviceBitmap(
DHPDEV  dhpdev,
SIZEL   sizl,
ULONG   iFormat)
{
    PDEV*   ppdev;
    OH*     poh = NULL;
    DSURF*  pdsurf;
    HBITMAP hbmDevice;
    FLONG   flHooks;
    LONG    lDelta;


    ppdev = (PDEV*) dhpdev;

    // If we're in full-screen mode, we hardly have any off-screen memory
    // in which to allocate a DFB.  LATER: We could still allocate an
    // OH node and put the bitmap on the DIB DFB list for later promotion.

    if (!ppdev->bEnabled)
        return(0);

    // We only support device bitmaps that are the same colour depth
    // as our display.
    //
    // Actually, those are the only kind GDI will ever call us with,
    // but we may as well check.  Note that this implies you'll never
    // get a crack at 1bpp bitmaps.

    if (iFormat != ppdev->iBitmapFormat)
        return(0);

    // We don't want anything 8x8 or smaller -- they're typically brush
    // patterns which we don't particularly want to stash in off-screen
    // memory:

    if ((sizl.cx <= 8) && (sizl.cy <= 8))
     return(0);


    poh = pohAllocate(ppdev, sizl.cx, sizl.cy, 0);

    if (poh != NULL)
    {
        pdsurf = EngAllocMem(0, sizeof(DSURF), ALLOC_TAG);

        if (pdsurf != NULL)
        {
            hbmDevice = EngCreateDeviceBitmap((DHSURF) pdsurf, sizl, iFormat);

            if (hbmDevice != NULL)
            {
                flHooks = ppdev->flHooks;

                // Setting the SYNCHRONIZEACCESS flag tells GDI that we
                // want all drawing to the bitmaps to be synchronized (GDI
                // is multi-threaded and by default does not synchronize
                // device bitmap drawing -- it would be a Bad Thing for us
                // to have multiple threads using the accelerator at the
                // same time):

                flHooks |= HOOK_SYNCHRONIZEACCESS;

                // It's a device-managed surface; make sure we don't set
                // HOOK_SYNCHRONIZE, otherwise we may confuse GDI:

                flHooks &= ~HOOK_SYNCHRONIZE;

                if (EngAssociateSurface((HSURF) hbmDevice, ppdev->hdevEng,
                                        flHooks))
                {
                    pdsurf->dt    = DT_SCREEN;
                    pdsurf->poh   = poh;
                    pdsurf->sizl  = sizl;
                    pdsurf->ppdev = ppdev;
                    poh->pdsurf   = pdsurf;

                    return(hbmDevice);
                }

                EngDeleteSurface((HSURF) hbmDevice);
            }
            EngFreeMem(pdsurf);
        }

       pohFree(ppdev, poh);
    }

    return(0);
}

/******************************Public*Routine******************************\
* VOID DrvDeleteDeviceBitmap
*
* Deletes a DFB.
*
\**************************************************************************/

VOID DrvDeleteDeviceBitmap(
DHSURF  dhsurf)
{
    DSURF*   pdsurf;
    PDEV*    ppdev;
    SURFOBJ* psoDib;
    HSURF    hsurfDib;

    pdsurf = (DSURF*) dhsurf;
    ppdev  = pdsurf->ppdev;

    if (pdsurf->dt == DT_SCREEN)
    {
        pohFree(ppdev, pdsurf->poh);
    }
    else
    {
        ASSERTDD(pdsurf->dt == DT_DIB, "Expected DIB type");

        psoDib = pdsurf->pso;

        // Get the hsurf from the SURFOBJ before we unlock it (it's not
        // legal to dereference psoDib when it's unlocked):

        hsurfDib = psoDib->hsurf;
        EngUnlockSurface(psoDib);
        EngDeleteSurface(hsurfDib);
    }

    EngFreeMem(pdsurf);
}

/******************************Public*Routine******************************\
* BOOL bAssertModeOffscreenHeap
*
* This function is called whenever we switch in or out of full-screen
* mode.  We have to convert all the off-screen bitmaps to DIBs when
* we switch to full-screen (because we may be asked to draw on them even
* when in full-screen, and the mode switch would probably nuke the video
* memory contents anyway).
*
\**************************************************************************/

BOOL bAssertModeOffscreenHeap(
PDEV*   ppdev,
BOOL    bEnable)
{
    BOOL b;

    b = TRUE;

    if (!bEnable)
    {
        b = bMoveAllDfbsFromOffscreenToDibs(ppdev);
    }

    return(b);
}

/******************************Public*Routine******************************\
* VOID vDisableOffscreenHeap
*
* Frees any resources allocated by the off-screen heap.
*
\**************************************************************************/

VOID vDisableOffscreenHeap(
PDEV*   ppdev)
{
    OHALLOC* poha;
    OHALLOC* pohaNext;
    SURFOBJ* psoPunt;
    HSURF    hsurf;

    psoPunt = ppdev->psoPunt;
    if (psoPunt != NULL)
    {
        hsurf = psoPunt->hsurf;
        EngUnlockSurface(psoPunt);
        EngDeleteSurface(hsurf);
    }

    psoPunt = ppdev->psoPunt2;
    if (psoPunt != NULL)
    {
        hsurf = psoPunt->hsurf;
        EngUnlockSurface(psoPunt);
        EngDeleteSurface(hsurf);
    }

    poha = ppdev->heap.pohaChain;
    while (poha != NULL)
    {
        pohaNext = poha->pohaNext;  // Grab the next pointer before it's freed
        EngFreeMem(poha);
        poha = pohaNext;
    }
}

/******************************Public*Routine******************************\
* BOOL bEnableOffscreenHeap
*
* Initializes the off-screen heap using all available video memory,
* accounting for the portion taken by the visible screen.
*
* Input: ppdev->cxScreen
*        ppdev->cyScreen
*        ppdev->cxMemory
*        ppdev->cyMemory
*
\**************************************************************************/

BOOL bEnableOffscreenHeap(
PDEV*   ppdev)
{
    OH*         poh;
    SIZEL       sizl;
    HSURF       hsurf;
    POINTL      ptlScreen;

    char buffer[20];
    WORD * wPtr;
    DWORD                       ReturnedDataLength = 10;

    DISPDBG((1, "Screen: %li x %li  Memory: %li x %li",
        ppdev->cxScreen, ppdev->cyScreen, ppdev->cxMemory, ppdev->cyMemory));

    ASSERTDD((ppdev->cxScreen <= ppdev->cxMemory) &&
             (ppdev->cyScreen <= ppdev->cyMemory),
             "Memory must not have smaller dimensions than visible screen!");

    ppdev->heap.pohaChain   = NULL;
    ppdev->heap.pohFreeList = NULL;

    // Initialize the available list, which will be a circular
    // doubly-linked list kept in ascending 'cxcy' order, with a
    // 'sentinel' at the end of the list:

    poh = pohNewNode(ppdev);
    if (poh == NULL)
        goto ReturnFalse;


    // The first node describes the entire video memory size:

    poh->pohNext      = &ppdev->heap.ohFree;
    poh->pohPrev      = &ppdev->heap.ohFree;
    poh->ohState      = OH_FREE;
    poh->cx           = ppdev->cxMemory;

    poh->cy           = ppdev->cyMemory;
    poh->ulSize       = CONVERT_TO_BYTES(poh->cx * poh->cy, ppdev);
    poh->pvScan0      = ppdev->pjScreen;
    poh->ulBase       = 0L;

    // The second node is our free list sentinel:

    ppdev->heap.ohFree.pohNext         = poh;
    ppdev->heap.ohFree.pohPrev         = poh;
    ppdev->heap.ohFree.ohState         = OH_FREE;
    ppdev->heap.ohFree.ulSize          = 0;

    ppdev->heap.ulMaxFreeSize          = poh->ulSize;
    ppdev->heap.ulMaxOffscrnSize       = poh->ulSize;

    // Initialize the discardable list, which will be a circular
    // doubly-linked list kept in order, with a sentinel at the end.
    // This node is also used for the screen-surface, for its offset:

    ppdev->heap.ohDiscardable.pohNext = &ppdev->heap.ohDiscardable;
    ppdev->heap.ohDiscardable.pohPrev = &ppdev->heap.ohDiscardable;
    ppdev->heap.ohDiscardable.ohState = OH_DISCARDABLE;
    ppdev->heap.ohDiscardable.ulSize = SENTINEL;

    poh = pohAllocate(ppdev, ppdev->cxScreen, ppdev->cyScreen,
                      FLOH_MAKE_PERMANENT);

    ASSERTDD((poh != NULL) && (poh->x == 0) && (poh->y == 0) &&
             (poh->cx >= ppdev->cxScreen) && (poh->cy >= ppdev->cyScreen),
             "Screen allocation messed up");

    // Remember it so that we can associate the screen SURFOBJ with this
    // poh:

    ppdev->pohScreen = poh;

    // Allocate a 'punt' SURFOBJ we'll use when the device-bitmap is in
    // off-screen memory, but we want GDI to draw to it directly as an
    // engine-managed surface:

    sizl.cx = ppdev->cxMemory;
    sizl.cy = ppdev->cyMemory;

    // We want to create it with exactly the same hooks and capabilities
    // as our primary surface.

    hsurf = (HSURF) EngCreateBitmap(sizl,
                                    ppdev->lDelta,
                                    ppdev->iBitmapFormat,
                                    BMF_TOPDOWN,
                                    (VOID*) ppdev->pjScreen);

    if ((hsurf == 0)                                                  ||
        (!EngAssociateSurface(hsurf, ppdev->hdevEng, ppdev->flHooks)) ||
        (!(ppdev->psoPunt = EngLockSurface(hsurf))))
    {
        DISPDBG((0, "Failed punt surface creation"));

        EngDeleteSurface(hsurf);
        goto ReturnFalse;
    }

    // We need another for doing DrvBitBlt and DrvCopyBits when both
    // surfaces are off-screen bitmaps:

    hsurf = (HSURF) EngCreateBitmap(sizl,
                                    ppdev->lDelta,
                                    ppdev->iBitmapFormat,
                                    BMF_TOPDOWN,
                                    (VOID*) ppdev->pjScreen);

    if ((hsurf == 0)                                                  ||
        (!EngAssociateSurface(hsurf, ppdev->hdevEng, ppdev->flHooks)) ||
        (!(ppdev->psoPunt2 = EngLockSurface(hsurf))))
    {
        DISPDBG((0, "Failed punt surface creation"));

        EngDeleteSurface(hsurf);
        goto ReturnFalse;
    }

    DISPDBG((5, "Passed bEnableOffscreenHeap"));

    if (poh != NULL)
        return(TRUE);

ReturnFalse:

    DISPDBG((0, "Failed bEnableOffscreenHeap"));

    return(FALSE);
}


SURFOBJ* psoCreateFromOh(PDEV* ppdev, OH * poh)
{
    HSURF       hsurf;
    SIZEL       sizl;
    SURFOBJ     * psoNew;

    sizl.cx = poh->cx;
    sizl.cy = poh->cy;


    hsurf = (HSURF) EngCreateBitmap(sizl,
        poh->lDelta,
        ppdev->iBitmapFormat,
        BMF_TOPDOWN,
        (VOID*) poh->pvScan0);

    if ((hsurf == 0)                                                  ||
        (!EngAssociateSurface(hsurf, ppdev->hdevEng, ppdev->flHooks)) ||
        (!(psoNew = EngLockSurface(hsurf))))
    {
            DISPDBG((0, "Failed punt surface creation"));
            EngDeleteSurface(hsurf);
            psoNew = NULL;
    }

    return(psoNew);

}

