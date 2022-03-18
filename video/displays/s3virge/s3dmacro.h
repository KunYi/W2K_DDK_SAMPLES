//******************************Module*Header********************************
//*  Module Name: s3dmacrof.h
//*
//*
//*  Macros to implement OS specific hw managment
//*  Copyright (c) Microsoft 1998, All Rights Reserved
//*
//***************************************************************************

#ifndef __S3DMACRO_H

#define __S3DMACRO_H

/*********************************************************************
**
**  COMMON MACROS
**
**********************************************************************/
#ifdef WINNT
#define WaitFifo(ctxt, x)   \
        S3DFifoWait(ctxt->ppdev, x);
#define WaitFifoEmpty(ctxt) \
        S3DFifoWait(ctxt->ppdev, 16);
#else
#define MmioRegWrite(mmReg,value)   (*((volatile ULONG *)(sData->lpMMReg+mmReg)))=value
#define MmioRegRead(mmReg)          (*((volatile ULONG *)(sData->lpMMReg+mmReg)))

#define WaitFifo(x) { \
    if (!( sData->D3DGlobals.dma_possible )) { \
        {while(((MmioRegRead(0x8504) & 0x1F00) >> 8) < x);} \
    } \
}

#define WaitFifoEmpty() { \
    if (!( sData->D3DGlobals.dma_possible )) { \
        {while(((MmioRegRead(0x8504) & 0x1F00) >> 8) < 16 );} \
    } \
}
#endif

#define WaitForEngineIdle()         {while(!(MmioRegRead(0x8504) & 0x2000));}

#define DDSurf_Width(lpLcl) ( lpLcl->lpGbl->wWidth )
#define DDSurf_Height(lpLcl) ( lpLcl->lpGbl->wHeight )
#define DDSurf_Get_dwCaps(lpLcl) (lpLcl->ddsCaps.dwCaps)
#define DDSurf_Pitch(lpLcl) ( lpLcl->lpGbl->lPitch )
#ifdef WINNT
#define DDSurf_BitDepth(lpLcl) (lpLcl->lpGbl->ddpfSurface.dwRGBBitCount)
#define DDSurf_AlphaBitDepth(lpLcl) (lpLcl->lpGbl->ddpfSurface.dwAlphaBitDepth)
#define DDSurf_RGBAlphaBitMask(lpLcl) (lpLcl->lpGbl->ddpfSurface.dwRGBAlphaBitMask)
#define DDSurf_VideoMemoryPtr(pd, lpLcl) (lpLcl->lpGbl->fpVidMem + pd->pjScreen )
#else

#define DDSurf_BitDepth(lpLcl) \
    ( (lpLcl->dwFlags & DDRAWISURF_HASPIXELFORMAT) ? \
      (lpLcl->lpGbl->ddpfSurface.dwRGBBitCount) : \
      (lpLcl->lpGbl->lpDD->vmiData.ddpfDisplay.dwRGBBitCount) \
    )

#define DDSurf_AlphaBitDepth(lpLcl) \
    ( (lpLcl->dwFlags & DDRAWISURF_HASPIXELFORMAT) ? \
      (lpLcl->lpGbl->ddpfSurface.dwAlphaBitDepth) : \
      (lpLcl->lpGbl->lpDD->vmiData.ddpfDisplay.dwAlphaBitDepth) \
    )

#define DDSurf_RGBAlphaBitMask(lpLcl) \
    ( (lpLcl->dwFlags & DDRAWISURF_HASPIXELFORMAT) ? \
      (lpLcl->lpGbl->ddpfSurface.dwRGBAlphaBitMask) : \
      (lpLcl->lpGbl->lpDD->vmiData.ddpfDisplay.dwRGBAlphaBitMask) \
    )

#endif

typedef float /* double */ _PRECISION;

#define _WRITE_REG_DEST( Dest, Src, Format ) \
{ \
        Dest = ( Format ) ( Src ); \
}


#ifdef WINNT

#define _WRITE_REG_TRI_LONG( Offset, Value ) \
WRITE_REGISTER_ULONG( (BYTE*)_TRI_BASE + Offset, (Value) )
#define _WRITE_REG_SET_LONG(pCtxt, Offset, Value ) \
WRITE_REGISTER_ULONG( (BYTE*)D3DGLOBALPTR(pCtxt)->D3DGlobals.g_p3DStp + Offset, (Value) )
#define _WRITE_HEADER( SLOTS_NEEDED, DMAVALUE )
#define _ADVANCE_INDEX( ADVANCE )
#define _UNDO_DMA_HDR 
#define _START_DMA
#define _DMAFILTER		0x0FFF		//4K
 
#else


#define _DMAFILTER		0x0FFF		//4K
#define _WRITE_REG_TRI_LONG( Offset, Value ) { \
  *( volatile ULONG * )( ( char * )_TRI_BASE + ((sData->D3DGlobals.g_DmaIndex + Offset) & _DMAFILTER) ) =  Value; \
}

#define _WRITE_REG_SET_LONG(pCtxt, Offset, Value ) { \
  *( volatile ULONG * )( ( char * )sData->D3DGlobals.g_p3DStp + ((sData->D3DGlobals.g_DmaIndex + Offset) & _DMAFILTER) ) =  Value; \
}

#define _INDEX_ADVANCE	4
#define _WRITE_HEADER( SLOTS_NEEDED, DMAVALUE ) { \
	if ( sData->D3DGlobals.dma_possible ) { \
        ULONG tmp; \
        do { \
            tmp = sData->D3DGlobals.g_DmaReadPtr; \
            if (sData->D3DGlobals.g_DmaReadPtr <= sData->D3DGlobals.g_DmaIndex) \
                tmp += _DMAFILTER + 1; \
            if ( (tmp - sData->D3DGlobals.g_DmaIndex) <= (SLOTS_NEEDED) ) { /*need to re-read read ptr*/ \
                tmp = sData->D3DGlobals.g_DmaReadPtr = *(sData->D3DGlobals.g_lpReadPtrReg) & 0xFFC; \
                if (tmp <= sData->D3DGlobals.g_DmaIndex) \
                    tmp +=  _DMAFILTER + 1; \
            } \
        } while ((tmp - sData->D3DGlobals.g_DmaIndex) <= (SLOTS_NEEDED) ); \
        _WRITE_REG_SET_LONG( 0, DMAVALUE ); \
		sData->D3DGlobals.g_DmaIndex += _INDEX_ADVANCE; \
    } \
}

#define _ADVANCE_INDEX( ADVANCE ) { \
    if ( sData->D3DGlobals.dma_possible ) { \
        sData->D3DGlobals.g_DmaIndex += ADVANCE; \
    	sData->D3DGlobals.g_DmaIndex &= _DMAFILTER; \
    } \
}

	if ( sData->D3DGlobals.dma_possible ) { \
        *(sData->D3DGlobals.g_lpWritePtrReg) = 0x00010000 | sData->D3DGlobals.g_DmaIndex; \
    } \
}


#endif

#define _WRITE_S3D_HEADER( datasize ) _WRITE_HEADER( datasize + HDR_SIZE, ( ( ( ( TRI_3D + TRI_3D_DATA )& 0xFFFC) << 14) | ( datasize >> 2 ) ));

#endif

