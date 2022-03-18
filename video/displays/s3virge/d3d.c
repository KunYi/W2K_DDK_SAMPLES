/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3d.c
*
*  Content:    Direct3D DX3 Callback function interface
*
* Copyright (C) 1995-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#include "precomp.h"
#include "d3ddrv.h"
#include "d3dmath.h"


/******************************Public*Routine******************************\
*
* DWORD DevD3DContextCreate
*
* The ContextCreate callback is invoked when a new Direct3D device is being
* created by a Direct3D application. The driver is required to generate a
* unique context id for this new context. Direct3D will then use this context
* id in every subsequent callback invocation for this Direct3D device.
*
* Context is the current rasterization state. For instance, if there are 3
* applications running, each will have a different state at any point in time.
* When each one is running, the hardware has to make sure that the context,
* (whether doing Gouraud shading, for example) is the same as the last time
* that application got a time slice.
*
* State is anything that the particular device needs to know per context
* i.e. what surface is being rendered to, shading, texture, texture handles,
* what physical surfaces those texture handles represent, etc. The context
* encapsulates all state for the Direct3D device - state is not shared
* between contexts. Therefore the driver needs to maintain full state
* information for each context. This state will be changed by calls to the
* RenderState callback. In the case of rasterization only hardware, the
* driver need only maintain rasterization state. As well as state, the driver
* will also want to store the lpDDS, lpDDSZ, and dwPid from the callback
* data argument.
*
* The driver should not create a context handle of zero. This is guaranteed
* to be an invalid context handle.
*
* The driver must be able to reference all texture handles (see TextureCreate)
* that are created within the created context. This is so that the driver
* can clean up all driver specific data related to textures created within
* this context when a ContextDestroy or ContextDestroyAll HAL call is made.
*
* Parameters
*      pccd
*           Pointer to a structure containing things including the current
*           rendering surface, the current Z surface, and the DirectX object
*           handle, etc.
*
*          .lpDDGbl
*                Points to the DirectDraw structure representing the
*                DirectDraw object.
*          .lpDDS
*                This is the surface that is to be used as the rendering
*                target, i.e., the 3D accelerator sprays its bits at this
*                surface.
*          .lpDDSZ
*                The surface that is to be used as the Z buffer. If this
*                is NULL, no Z buffering is to be performed.
*          .dwPid
*                The process id of the Direct3D application that initiated
*                the creation of the Direct3D device.
*          .dwhContext
*                The driver should place the context ID that it wants Direct3D
*                to use when communicating with the driver. This should be
*                unique.
*          .ddrval
*                Return code. DD_OK indicates success.
*
* Return Value
*      Returns one of the following values:
*                DDHAL_DRIVER_HANDLED   
*                DDHAL_DRIVER_NOTHANDLED    
*
\**************************************************************************/
DWORD WINAPI DevD3DContextCreate(LPD3DHAL_CONTEXTCREATEDATA pccd)
{
    S3_CONTEXT *pCtxt;
    ULONG frameAddress = 0;

    LPDDRAWI_DIRECTDRAW_GBL lpDDGbl=pccd->lpDDLcl->lpGbl;
    LPDDRAWI_DDRAWSURFACE_LCL lpLclFrame = DDS_LCL(pccd->lpDDS);
    LPDDRAWI_DDRAWSURFACE_LCL lpLclZ = DDS_LCL(pccd->lpDDSZ);

    // Get Pointer to context
    pCtxt = (LPS3_CONTEXT)MEMALLOC(sizeof(S3_CONTEXT));
    if (! pCtxt) {
        DPF( "in DevD3DContextCreate, MEMALLOC returns NULL, failing" );
        pccd->ddrval = D3DHAL_OUTOFCONTEXTS;
        return (DDHAL_DRIVER_HANDLED);
    }

    // Zero the memory allocated for the context
    memset(pCtxt, 0, sizeof(S3_CONTEXT));
    D3DGLOBALPTR(pCtxt) = GetMyRefData(lpDDGbl);
    pCtxt->pCtxtNext=D3DGLOBALPTR(pCtxt)->D3DGlobals.FirstContxt;

    if (pCtxt->pCtxtNext){
        pCtxt->pCtxtNext->pCtxtLast = pCtxt;
    }

    D3DGLOBALPTR(pCtxt)->D3DGlobals.FirstContxt = pCtxt;
    pccd->dwhContext = (ULONG_PTR)pCtxt;

    // Peform D3D specific device initialization only once
    if ( ! D3DGLOBALPTR(pCtxt)->D3DGlobals.bInitialized ) {
        HRESULT hr = S3VirgeInit(pCtxt);
        if (hr != DD_OK) {
            pccd->ddrval = hr;
            return DDHAL_DRIVER_HANDLED;
        }
        D3DGLOBALPTR(pCtxt)->D3DGlobals.bInitialized = TRUE;
    }

    // cannot render into system memory
    CHK_NOTSYSTEM_SURF(pccd->lpDDS,pccd->ddrval,"","DevD3DContextCreate");

    // cannot render if z buffer is in system memory
    if (pccd->lpDDSZ)
        CHK_NOTSYSTEM_SURF(pccd->lpDDSZ,pccd->ddrval,"Z buffer",
                            "DevD3DContextCreate");

    // Note that this context is now valid by inserting the PID
    pCtxt->dwPID = pccd->dwPID;

    DPF_DBG( "in DevD3DContextCreate, pccd->lpDD =%08lx, pccd->lpDDS =%08lx",
              lpDDGbl, pccd->lpDDS);
    DPF( "                    pccd->dwhContext =%08lx, pccd->lpDDSZ= %08lx",
        pccd->dwhContext, pccd->lpDDSZ);
    DPF( "                    pccd->dwPID =%08lx",
        pccd->dwPID);

    pCtxt->ppdev = (PDEV *)lpDDGbl->dhpdev;
    pCtxt->FrameAddr = 0;                          // pCtxt->ppdev->pjScreen;
    pCtxt->g_p3DStp = ( vi13D_SETUP * )( pCtxt->ppdev->pjMmBase + 0xB4D4L );
    pCtxt->g_p3DTri = ( vi13D_TRIANGLE * )( pCtxt->ppdev->pjMmBase + 0xB504L );


    pCtxt->pDDLcl = pccd->lpDDLcl;
    pCtxt->pHandleList = GetSurfaceHandleList(pccd->lpDDLcl);
    if (pCtxt->pHandleList == NULL)
    {
        DPF_DBG("ERROR: Couldn't get a surface handle for lpDDLcl");
        pccd->ddrval = DDERR_OUTOFMEMORY;
        return (DDHAL_DRIVER_HANDLED);
    }

    // Record the surface information
    pCtxt->pSurfRender = 
        TextureHandleToPtr(lpLclFrame->lpSurfMore->dwSurfaceHandle, pCtxt);

    if (NULL == pCtxt->pSurfRender)
    {
        DPF_DBG("ERROR: D3DContextCreate NULL==pSurfRender handle=%08lx",
            lpLclFrame->lpSurfMore->dwSurfaceHandle);
        pccd->ddrval = D3DHAL_CONTEXT_BAD;
        return (DDHAL_DRIVER_HANDLED);
    }
    
    pCtxt->RenderSurfaceHandle = lpLclFrame->lpSurfMore->dwSurfaceHandle;

    if (lpLclZ)
    {
        pCtxt->pSurfZBuffer = 
            TextureHandleToPtr(lpLclZ->lpSurfMore->dwSurfaceHandle, pCtxt);
        if (NULL == pCtxt->pSurfZBuffer)
        {
            DPF_DBG("ERROR: D3DContextCreate NULL==pSurfZBuffer handle=%08lx",
                lpLclZ->lpSurfMore->dwSurfaceHandle);
            pccd->ddrval = D3DHAL_CONTEXT_BAD;
            return (DDHAL_DRIVER_HANDLED);
        }
        pCtxt->ZBufferHandle = lpLclZ->lpSurfMore->dwSurfaceHandle;
    }
    else
    {
        pCtxt->pSurfZBuffer = NULL;
        pCtxt->ZBufferHandle = 0;
    }

    DPF_DBG("Getting pHandleList=%08lx for pDDLcl %08lx",
                                 pCtxt->pHandleList,pccd->dwPID);

     // On context creation, no render states are overridden

    // set default render states for this context

    if( pCtxt->pSurfZBuffer ) {
        pCtxt->bZEnabled = TRUE;
        pCtxt->bZWriteEnabled = TRUE;
        pCtxt->dwRCode=S3GOURAUD | S3ZBUFFER;
    } else {
        pCtxt->bZEnabled = FALSE;
        pCtxt->bZWriteEnabled = FALSE;
        pCtxt->dwRCode=S3GOURAUD;
    }

    if (D3DGLOBALPTR(pCtxt)->D3DGlobals.dma_possible)
        pCtxt->dwRCode |= S3DMA;

    // S3COLORKEYENABLE is the default to preserve backward compatibility
    pCtxt->dwStatus = S3COLORKEYENABLE;
    pCtxt->bChopped = FALSE;
    pCtxt->bChanged = TRUE;
    pCtxt->bZVisible = FALSE;
    pCtxt->BlendMode = D3DTBLEND_MODULATE;
    pCtxt->TextureAddress = D3DTADDRESS_WRAP;
    pCtxt->bWrapU = FALSE;
    pCtxt->bWrapV = FALSE;
    pCtxt->ShadeMode = D3DSHADE_GOURAUD;
    pCtxt->bSpecular = TRUE;
    pCtxt->ZCmpFunc = D3DCMP_LESS;
    pCtxt->FillMode = D3DFILL_SOLID;
    pCtxt->CullMode = D3DCULL_CCW;
    pCtxt->Alpha_workaround = FALSE;
    pCtxt->SrcBlend = D3DPBLENDCAPS_ONE;
    pCtxt->DstBlend = D3DPBLENDCAPS_ZERO;
    pCtxt->dwTexture = 0;


    // Texture stage state defaults (only one stage present on Virge)
    pCtxt->dwStatus |= S3MULTITEXTURE;
    pCtxt->tssStates[D3DTSS_TEXTUREMAP] = 0;
    pCtxt->tssStates[D3DTSS_COLOROP] = D3DTOP_MODULATE;
    pCtxt->tssStates[D3DTSS_ALPHAOP] = D3DTOP_SELECTARG1;
    pCtxt->tssStates[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
    pCtxt->tssStates[D3DTSS_COLORARG2] = D3DTA_CURRENT;
    pCtxt->tssStates[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
    pCtxt->tssStates[D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
    pCtxt->tssStates[D3DTSS_TEXCOORDINDEX] = 0; //default to stage number
    pCtxt->tssStates[D3DTSS_ADDRESS] = D3DTADDRESS_WRAP;
    pCtxt->tssStates[D3DTSS_ADDRESSU] = D3DTADDRESS_WRAP;
    pCtxt->tssStates[D3DTSS_ADDRESSV] = D3DTADDRESS_WRAP;
    pCtxt->tssStates[D3DTSS_MAGFILTER] = D3DTFG_POINT;
    pCtxt->tssStates[D3DTSS_MINFILTER] = D3DTFN_POINT;
    pCtxt->tssStates[D3DTSS_MIPFILTER] = D3DTFP_NONE;
    pCtxt->tssStates[D3DTSS_MAXANISOTROPY] = 1;

    // Report success
    pccd->ddrval = DD_OK;
    return (DDHAL_DRIVER_HANDLED);

}

/******************************Public*Routine******************************\
*
* DWORD DevD3DContextDestroy
*
* This callback is invoked when a Direct3D Device is being destroyed. As each
* device is represented by a context ID, the driver is passed a context to
* destroy.
*
* On Windows 95 the driver should free all resources allocated to the context.
* For example, the driver should free all textures resources associated with
* the context. This does not include the actual DirectDraw surfaces, which
* will be freed by DirectDraw in the usual manner. This should not be done on
* Windows NT.
*
* Parameters
*     pcdd
*          Pointer to Context destroy information.
*
*          .dwhContext
*               The ID of the context to be destroyed.
*          .ddrval
*               Return code. DD_OK indicates success.
*
* Return Value
*      Returns one of the following values:
*                DDHAL_DRIVER_HANDLED   
*                DDHAL_DRIVER_NOTHANDLED    
*
\**************************************************************************/
DWORD WINAPI DevD3DContextDestroy(LPD3DHAL_CONTEXTDESTROYDATA pcdd)
{
    S3_TEXTURE  *pTexture,*pTempTexture;
    S3_CONTEXT  *pCtxt = (LPS3_CONTEXT)pcdd->dwhContext;
    DWORD i;

    if (pCtxt){


        if (pCtxt->pHandleList->dwSurfaceList != NULL)
        {
            // avoid AV if our surface list is missing

            for (i = 1; i < PtrToUlong(pCtxt->pHandleList->dwSurfaceList[0]); i++)
            {
                if (pCtxt->pHandleList->dwSurfaceList[i])
                {
                    MEMFREE(pCtxt->pHandleList->dwSurfaceList[i]);
                    pCtxt->pHandleList->dwSurfaceList[i] = NULL;
                }
            }
        }

        // Destroy any leftover state blocks
        __DeleteAllStateSets(pCtxt);

        // Destroy context
        DPF_DBG( "in DevD3DContextDestroy, pcdd->dwhContext =%08lx", pCtxt);
        if (pCtxt->pCtxtNext)
            pCtxt->pCtxtNext->pCtxtLast = pCtxt->pCtxtLast;
        if (pCtxt->pCtxtLast)
            pCtxt->pCtxtLast->pCtxtNext = pCtxt->pCtxtNext;
        else
            D3DGLOBALPTR(pCtxt)->D3DGlobals.FirstContxt = pCtxt->pCtxtNext;

        MEMFREE(pCtxt);
        pcdd->dwhContext = 0;
    }

    pcdd->ddrval = DD_OK;
    return (DDHAL_DRIVER_HANDLED);
}

//*****************************************************************************
// Direct3D HAL Table.
//
// This table contains all of the HAL calls that this driver supports in the
// D3DHAL_Callbacks structure. These calls pertain to device context, scene
// capture, execution, textures, transform, lighting, and pipeline state.
// None of this is emulation code. The calls take the form of a return code
// equal to: HalCall(HalCallData* lpData). All of the information in this
// table will be implementation specific according to the specifications of
// the hardware.
//
//*****************************************************************************

D3DHAL_CALLBACKS deviceD3DHALCallbacks = {
    sizeof(D3DHAL_CALLBACKS),

    // Device context
    DevD3DContextCreate,          // Required.
    DevD3DContextDestroy,         // Required.
    NULL,

    // Scene capture
    NULL, //DevD3DSceneCapture    // Optional.

    // Execution
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    NULL,
    NULL,
    NULL,
    NULL,
    // Transform - must be supported if lighting is supported.
    NULL, //DevD3DMatrixCreate,   // If any of these calls are supported,
    NULL, //DevD3DMatrixDestroy,  // they must all be.
    NULL, //DevD3DMatrixSetData,  // ditto
    NULL, //DevD3DMatrixGetData,  // ditto
    NULL, //DevD3DSetViewportData,// ditto

    // Lighting
    NULL, //DevD3DLightSet        // If any of these calls are supported,
    NULL, //DevD3DMaterialCreate, // they must all be.
    NULL, //DevD3DMaterialDestroy,// ditto
    NULL, //DevD3DMaterialSetData,// ditto
    NULL, //DevD3DMaterialGetData,// ditto

    // Pipeline State

    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
    0L,                           // Reserved, must be zero
};

//*****************************************************************************
// Initial D3D state information is filled in here in the nullPrimCaps,
// lineCaps and triCaps (each with misc. caps related to culling, fog,
// blending, and textures). nullPrimCaps starts empty. LineCaps includes
// miscellaneous D3DMISCCAPS related to culling, dithering, subpixel
// correction, fog, boolean operations, and D3DPBLENDCAPS related to alpha
// blending, shading, textures and stippling.
//*****************************************************************************

#define nullPrimCaps {                                          \
    sizeof(D3DPRIMCAPS), 0, 0, 0, 0, 0, 0, 0, 0 ,0 ,0 ,0 ,0     \
}


#define lineCaps {                                           \
    sizeof(D3DPRIMCAPS),                                     \
    D3DPMISCCAPS_CULLNONE |                                  \
    D3DPMISCCAPS_CULLCW   |                                  \
    D3DPMISCCAPS_CULLCCW,           /* miscCaps           */ \
    D3DPRASTERCAPS_DITHER   |                                \
    D3DPRASTERCAPS_SUBPIXEL |                                \
    D3DPRASTERCAPS_FOGVERTEX,       /* rasterCaps         */ \
    D3DPCMPCAPS_NEVER        |                               \
    D3DPCMPCAPS_LESS         |                               \
    D3DPCMPCAPS_EQUAL        |                               \
    D3DPCMPCAPS_LESSEQUAL    |                               \
    D3DPCMPCAPS_GREATER      |                               \
    D3DPCMPCAPS_NOTEQUAL     |                               \
    D3DPCMPCAPS_GREATEREQUAL |                               \
    D3DPCMPCAPS_ALWAYS,             /* zCmpCaps           */ \
    D3DPBLENDCAPS_ONE        |                               \
    D3DPBLENDCAPS_SRCALPHA,         /* sourceBlendCaps    */ \
    D3DPBLENDCAPS_ZERO       |                               \
    D3DPBLENDCAPS_INVSRCALPHA,      /* destBlendCaps      */ \
    0,                              /* alphaBlendCaps     */ \
    D3DPSHADECAPS_COLORFLATRGB       |                       \
    D3DPSHADECAPS_SPECULARFLATRGB    |                       \
    D3DPSHADECAPS_COLORGOURAUDRGB    |                       \
    D3DPSHADECAPS_SPECULARGOURAUDRGB |                       \
    D3DPSHADECAPS_FOGFLAT            |                       \
    D3DPSHADECAPS_FOGGOURAUD         |                       \
    D3DPSHADECAPS_ALPHAFLATBLEND     |                       \
    D3DPSHADECAPS_ALPHAGOURAUDBLEND,/* shadeCaps          */ \
    D3DPTEXTURECAPS_PERSPECTIVE  |                           \
    D3DPTEXTURECAPS_POW2         |                           \
    D3DPTEXTURECAPS_SQUAREONLY   |                           \
    D3DPTEXTURECAPS_TRANSPARENCY |                           \
    D3DPTEXTURECAPS_ALPHA ,         /* textureCaps        */ \
    D3DPTFILTERCAPS_NEAREST          |                       \
    D3DPTFILTERCAPS_LINEAR           |                       \
    0                              ,/* textureFilterCaps  */ \
    D3DPTBLENDCAPS_DECAL         |                           \
    D3DPTBLENDCAPS_MODULATE      |                           \
    /* The S3V doesn't support this blend modes properly when */ \
    /* both the texture and the mesh carry an alpha channel   */ \
    /*D3DPTBLENDCAPS_DECALALPHA    |*/                           \
    /*D3DPTBLENDCAPS_MODULATEALPHA |*/                           \
    D3DPTBLENDCAPS_COPY          |                           \
    D3DPTBLENDCAPS_ADD,             /* textureBlendCaps   */ \
    0,                              /* textureAddressCaps */ \
    0,                              /* stippleWidth       */ \
    0                               /* stippleHeight      */ \
}


#define triCaps {                                            \
    sizeof(D3DPRIMCAPS),                                     \
    D3DPMISCCAPS_CULLNONE |                                  \
    D3DPMISCCAPS_CULLCW   |                                  \
    D3DPMISCCAPS_CULLCCW,           /* miscCaps           */ \
    D3DPRASTERCAPS_DITHER   |                                \
    D3DPRASTERCAPS_SUBPIXEL |                                \
    D3DPRASTERCAPS_FOGVERTEX,       /* rasterCaps         */ \
    D3DPCMPCAPS_NEVER        |                               \
    D3DPCMPCAPS_LESS         |                               \
    D3DPCMPCAPS_EQUAL        |                               \
    D3DPCMPCAPS_LESSEQUAL    |                               \
    D3DPCMPCAPS_GREATER      |                               \
    D3DPCMPCAPS_NOTEQUAL     |                               \
    D3DPCMPCAPS_GREATEREQUAL |                               \
    D3DPCMPCAPS_ALWAYS,             /* zCmpCaps           */ \
    D3DPBLENDCAPS_ONE       |                                \
    D3DPBLENDCAPS_SRCALPHA,         /* sourceBlendCaps    */ \
    D3DPBLENDCAPS_ZERO        |                              \
    D3DPBLENDCAPS_INVSRCALPHA,      /* destBlendCaps      */ \
    0,                              /* alphaBlendCaps     */ \
    D3DPSHADECAPS_COLORFLATRGB       |                       \
    D3DPSHADECAPS_SPECULARFLATRGB    |                       \
    D3DPSHADECAPS_COLORGOURAUDRGB    |                       \
    D3DPSHADECAPS_SPECULARGOURAUDRGB |                       \
    D3DPSHADECAPS_FOGFLAT            |                       \
    D3DPSHADECAPS_FOGGOURAUD         |                       \
    D3DPSHADECAPS_ALPHAFLATBLEND     |                       \
    D3DPSHADECAPS_ALPHAGOURAUDBLEND,/* shadeCaps          */ \
    D3DPTEXTURECAPS_PERSPECTIVE  |                           \
    D3DPTEXTURECAPS_POW2         |                           \
    D3DPTEXTURECAPS_SQUAREONLY   |                           \
    D3DPTEXTURECAPS_TRANSPARENCY |                           \
    D3DPTEXTURECAPS_ALPHA ,         /* textureCaps        */ \
    D3DPTFILTERCAPS_NEAREST          |                       \
    D3DPTFILTERCAPS_LINEAR           |                       \
    0                              ,/* textureFilterCaps  */ \
    D3DPTBLENDCAPS_DECAL         |                           \
    D3DPTBLENDCAPS_MODULATE      |                           \
    /* The S3V doesn't support this blend modes properly when */ \
    /* both the texture and the mesh carry an alpha channel   */ \
    /*D3DPTBLENDCAPS_DECALALPHA    | */                          \
    /*D3DPTBLENDCAPS_MODULATEALPHA | */                          \
    D3DPTBLENDCAPS_COPY          |  /* textureBlendCaps   */ \
    D3DPTBLENDCAPS_ADD,                                      \
    D3DPTADDRESSCAPS_WRAP ,         /* textureAddressCaps */ \
    0,                              /* stippleWidth       */ \
    0                               /* stippleHeight      */ \
}


//*****************************************************************************
// Most of the driver state information is passed to D3D in the
// D3DDEVICEDESC_V1 deviceD3DCaps structure. This is filled in with
// D3DDEVICEDESC caps related to color model (whether monochrome or RGB),
// clipping, triCaps and lineCaps structures, bit depth, and local vertex
// buffers information and passed on to D3D during driver initialization.
//*****************************************************************************
D3DDEVICEDESC_V1 deviceD3DCaps = {
    sizeof(D3DDEVICEDESC_V1),       /* dwSize */
    D3DDD_COLORMODEL           |
    D3DDD_DEVCAPS              |
    D3DDD_TRICAPS              |
    D3DDD_LINECAPS             |
    D3DDD_DEVICERENDERBITDEPTH |
    D3DDD_DEVICEZBUFFERBITDEPTH,    /* dwFlags */
    D3DCOLOR_RGB,                   /* dcmColorModel */
    D3DDEVCAPS_EXECUTESYSTEMMEMORY  |
    D3DDEVCAPS_TEXTUREVIDEOMEMORY   |
    D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
    D3DDEVCAPS_FLOATTLVERTEX        |
    D3DDEVCAPS_DRAWPRIMTLVERTEX     |
    D3DDEVCAPS_DRAWPRIMITIVES2      |
    D3DDEVCAPS_DRAWPRIMITIVES2EX    |
    D3DDEVCAPS_SORTINCREASINGZ,     /* devCaps */
    { sizeof(D3DTRANSFORMCAPS),
      0 },                          /* dtcTransformCaps */
    FALSE,                          /* bClipping */
    { sizeof(D3DLIGHTINGCAPS),
      0,
      0,
      0 },                          /* dlcLightingCaps */
    lineCaps,                       /* lineCaps */
    triCaps,                        /* triCaps */
    DDBD_16 | DDBD_24,              /* dwDeviceRenderBitDepth */
    DDBD_16,                        /* dwDeviceZBufferBitDepth */
    0,                              /* dwMaxBufferSize */
    0                               /* dwMaxVertexCount */
};

//*****************************************************************************
// deviceTextureformats is a static structure which contains information
// pertaining to pixel format, dimensions, bit depth, surface requirements,
// overlays, and FOURCC codes. In this example code there are 5 possible
// texture formats specified. They are 16 bit pixel format, 32 bit pixel
// format, 16 bit pixel format with alpha pixels, 32 bit format with alpha
// pixels and 32 bit format with YUV colorspace conversions. These texture
// formats will vary with the driver implementation according to the
// capabilities of the hardware.
//*****************************************************************************
DDSURFACEDESC deviceTextureFormats[] = {
    {
    sizeof(DDSURFACEDESC),              /* dwSize */
    DDSD_CAPS | DDSD_PIXELFORMAT,       /* dwFlags */
    0,                                  /* dwHeight */
    0,                                  /* dwWidth */
    0,                                  /* lPitch */
    0,                                  /* dwBackBufferCount */
    0,                                  /* dwZBufferBitDepth */
    0,                                  /* dwAlphaBitDepth */
    0,                                  /* dwReserved */
    NULL,                               /* lpSurface */
    { 0, 0 },                           /* ddckCKDestOverlay */
    { 0, 0 },                           /* ddckCKDestBlt */
    { 0, 0 },                           /* ddckCKSrcOverlay */
    { 0, 0 },                           /* ddckCKSrcBlt */
    {
      sizeof(DDPIXELFORMAT),            /* ddpfPixelFormat.dwSize */
      DDPF_RGB,                         /* ddpfPixelFormat.dwFlags */
      0,                                /* FOURCC code */
      16,                               /* ddpfPixelFormat.dwRGBBitCount */
      0x7c00,
      0x03e0,
      0x001f,
      0
    },
    DDSCAPS_TEXTURE,                    /* ddscaps.dwCaps */
    },
    {
    sizeof(DDSURFACEDESC),              /* dwSize */
    DDSD_CAPS | DDSD_PIXELFORMAT,       /* dwFlags */
    0,                                  /* dwHeight */
    0,                                  /* dwWidth */
    0,                                  /* lPitch */
    0,                                  /* dwBackBufferCount */
    0,                                  /* dwZBufferBitDepth */
    0,                                  /* dwAlphaBitDepth */
    0,                                  /* dwReserved */
    NULL,                               /* lpSurface */
    { 0, 0 },                           /* ddckCKDestOverlay */
    { 0, 0 },                           /* ddckCKDestBlt */
    { 0, 0 },                           /* ddckCKSrcOverlay */
    { 0, 0 },                           /* ddckCKSrcBlt */
    {
      sizeof(DDPIXELFORMAT),            /* ddpfPixelFormat.dwSize */
      DDPF_RGB,                         /* ddpfPixelFormat.dwFlags */
      0,                                /* FOURCC code */
      32,                               /* ddpfPixelFormat.dwRGBBitCount */
      0xff0000,
      0xff00,
      0xff,
      0
    },
    DDSCAPS_TEXTURE,                    /* ddscaps.dwCaps */
    },
    {
    sizeof(DDSURFACEDESC),              /* dwSize */
    DDSD_CAPS | DDSD_PIXELFORMAT,       /* dwFlags */
    0,                                  /* dwHeight */
    0,                                  /* dwWidth */
    0,                                  /* lPitch */
    0,                                  /* dwBackBufferCount */
    0,                                  /* dwZBufferBitDepth */
    0,                                  /* dwAlphaBitDepth */
    0,                                  /* dwReserved */
    NULL,                               /* lpSurface */
    { 0, 0 },                           /* ddckCKDestOverlay */
    { 0, 0 },                           /* ddckCKDestBlt */
    { 0, 0 },                           /* ddckCKSrcOverlay */
    { 0, 0 },                           /* ddckCKSrcBlt */
    {
      sizeof(DDPIXELFORMAT),            /* ddpfPixelFormat.dwSize */
      DDPF_RGB | DDPF_ALPHAPIXELS,      /* ddpfPixelFormat.dwFlags */
      0,                                /* FOURCC code */
      16,                               /* ddpfPixelFormat.dwRGBBitCount */
      0x7c00,
      0x03e0,
      0x001f,
      0x8000
    },
    DDSCAPS_TEXTURE,                    /* ddscaps.dwCaps */
    },
    {
    sizeof(DDSURFACEDESC),              /* dwSize */
    DDSD_CAPS | DDSD_PIXELFORMAT,       /* dwFlags */
    0,                                  /* dwHeight */
    0,                                  /* dwWidth */
    0,                                  /* lPitch */
    0,                                  /* dwBackBufferCount */
    0,                                  /* dwZBufferBitDepth */
    0,                                  /* dwAlphaBitDepth */
    0,                                  /* dwReserved */
    NULL,                               /* lpSurface */
    { 0, 0 },                           /* ddckCKDestOverlay */
    { 0, 0 },                           /* ddckCKDestBlt */
    { 0, 0 },                           /* ddckCKSrcOverlay */
    { 0, 0 },                           /* ddckCKSrcBlt */
    {
      sizeof(DDPIXELFORMAT),            /* ddpfPixelFormat.dwSize */
      DDPF_RGB | DDPF_ALPHAPIXELS,      /* ddpfPixelFormat.dwFlags */
      0,                                /* FOURCC code */
      32,                               /* ddpfPixelFormat.dwRGBBitCount */
      0xff0000,
      0xff00,
      0xff,
      0xff000000
    },
    DDSCAPS_TEXTURE                     /* ddscaps.dwCaps */
    },
    {
    sizeof(DDSURFACEDESC),              /* dwSize */
    DDSD_CAPS | DDSD_PIXELFORMAT,       /* dwFlags */
    0,                                  /* dwHeight */
    0,                                  /* dwWidth */
    0,                                  /* lPitch */
    0,                                  /* dwBackBufferCount */
    0,                                  /* dwZBufferBitDepth */
    0,                                  /* dwAlphaBitDepth */
    0,                                  /* dwReserved */
    NULL,                               /* lpSurface */
    { 0, 0 },                           /* ddckCKDestOverlay */
    { 0, 0 },                           /* ddckCKDestBlt */
    { 0, 0 },                           /* ddckCKSrcOverlay */
    { 0, 0 },                           /* ddckCKSrcBlt */
    {
      sizeof(DDPIXELFORMAT),            /* ddpfPixelFormat.dwSize */
      DDPF_RGB | DDPF_ALPHAPIXELS,      /* ddpfPixelFormat.dwFlags */
      0,                                /* FOURCC code */
      16,                               /* ddpfPixelFormat.dwRGBBitCount */
      0xf00,
      0xf0,
      0xf,
      0xf000
    },
    DDSCAPS_TEXTURE                     /* ddscaps.dwCaps */
    },

};


//
// Total number of texture formats, we store it here to make use of it from
// DDraw's DdGetDriverInfo which is queried for D3D capabilities
//
DWORD deviceNumberOfTextureFormats =
            sizeof(deviceTextureFormats)/sizeof(DDSURFACEDESC);

// Handles table
// each entry is a DWLIST structure (*dwSurfaceList, pDDLcl)
DWLIST  HandleList[MAX_CONTEXT_NUM] = {0}; 

/******************************Private*Routine*****************************\
*
* BOOL ComparePixelFormat
*
* Function used to compare 2 pixels formats for equality. This is a
* helper function to D3DCheckTextureFormat.
*
* Parameters
*        The pixel formats to be compared
*
* Return Value
*        TRUE indicates equality
*
\**************************************************************************/
BOOL
ComparePixelFormat(LPDDPIXELFORMAT lpddpf1, LPDDPIXELFORMAT lpddpf2)
{
    if (lpddpf1->dwFlags != lpddpf2->dwFlags)
    {
        return FALSE;
    }

    // same bitcount for non-YUV surfaces?
    if (!(lpddpf1->dwFlags & (DDPF_YUV | DDPF_FOURCC)))
    {
        if (lpddpf1->dwRGBBitCount != lpddpf2->dwRGBBitCount )
        {
            return FALSE;
        }
    }

    // same RGB properties?
    if (lpddpf1->dwFlags & DDPF_RGB)
    {
        if ((lpddpf1->dwRBitMask != lpddpf2->dwRBitMask) ||
            (lpddpf1->dwGBitMask != lpddpf2->dwGBitMask) ||
            (lpddpf1->dwBBitMask != lpddpf2->dwBBitMask) ||
            (lpddpf1->dwRGBAlphaBitMask != lpddpf2->dwRGBAlphaBitMask))
        {
             return FALSE;
        }
    }

    // same YUV properties?
    if (lpddpf1->dwFlags & DDPF_YUV)    
    {
        if ((lpddpf1->dwFourCC != lpddpf2->dwFourCC) ||
            (lpddpf1->dwYUVBitCount != lpddpf2->dwYUVBitCount) ||
            (lpddpf1->dwYBitMask != lpddpf2->dwYBitMask) ||
            (lpddpf1->dwUBitMask != lpddpf2->dwUBitMask) ||
            (lpddpf1->dwVBitMask != lpddpf2->dwVBitMask) ||
            (lpddpf1->dwYUVAlphaBitMask != lpddpf2->dwYUVAlphaBitMask))
        {
             return FALSE;
        }
    }
    else if (lpddpf1->dwFlags & DDPF_FOURCC)
    {
        if (lpddpf1->dwFourCC != lpddpf2->dwFourCC)
        {
            return FALSE;
        }
    }

    // If Interleaved Z then check Z bit masks are the same
    if (lpddpf1->dwFlags & DDPF_ZPIXELS)
    {
        if (lpddpf1->dwRGBZBitMask != lpddpf2->dwRGBZBitMask)
        {   
            return FALSE;
        }
    }

    return TRUE;
}

/******************************Public*Routine******************************\
*
* BOOL D3DCheckTextureFormat
*
* Function used to determine if a texture format is supported. It traverses
* the deviceTextureFormats list. We use this in DdCanCreateSurface.
*
* Parameters
*      LPDDPIXELFORMAT
*            A pointer to a structure containing the requested pixel format
*
* Return Value
*      TRUE indicates that we do support the requested texture format.
*
\**************************************************************************/
BOOL WINAPI
D3DCheckTextureFormat(LPDDPIXELFORMAT lpddpf)
{
    DWORD i;

    // Run the list for a matching format
    for (i=0; i<deviceNumberOfTextureFormats; i++)
    {
        if (ComparePixelFormat(lpddpf, &deviceTextureFormats[i].ddpfPixelFormat))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/******************************Public*Routine******************************\
*
* BOOL DevD3DHALCreateDriver
*
* Function used to instantiate the 3D portion of the DirectDraw HAL
*
* Parameters
*      lplpGlobal
*            A pointer to a structure containing global information about
*            our driver. The elements of this structure are:
*
*           ->dwSize
*                The size of the structure, sizeof(D3DHAL_GLOBALDRIVERDATA).
*           ->hwCaps
*                A structure that describes the hardware capabilities.
*           ->dwNumVertices
*                This should be set to zero.
*           ->dwNumClipVertices
*                This should be set to zero.
*           ->dwNumTextureFormats
*                The number of DirectDrawSurface descriptions pointed to by
*                the following member.
*           ->lpTextureFormats
*                This points to an array of DirectDraw surface descriptions
*                (DDSURFACEDESC) that describe the texture formats supported
*                by the 3D accelerator.
*
*      lplpHALCallbacks
*           This is a pointer to a D3DHAL_CALLBACKS structure. This defines
*           the function pointers that the Direct3D HAL driver provides.
*
* Return Value
*
\**************************************************************************/

D3DHAL_GLOBALDRIVERDATA deviceD3DGlobal;

BOOL WINAPI
DevD3DHALCreateDriver(LPD3DHAL_GLOBALDRIVERDATA *lplpGlobal,
                      LPD3DHAL_CALLBACKS *lplpHALCallbacks)
{
    DPF("DevD3DHALCreateDriver");

    // Here we fill in the supplied structures.

    memset(&deviceD3DGlobal, 0, sizeof(D3DHAL_GLOBALDRIVERDATA));

    deviceD3DGlobal.dwSize = sizeof(D3DHAL_GLOBALDRIVERDATA);
    deviceD3DGlobal.hwCaps = deviceD3DCaps;
    deviceD3DGlobal.dwNumVertices = 0;
    deviceD3DGlobal.dwNumClipVertices = 0;
    deviceD3DGlobal.dwNumTextureFormats = deviceNumberOfTextureFormats;
    deviceD3DGlobal.lpTextureFormats = &deviceTextureFormats[0];

    // Return the HAL table.

    *lplpGlobal = &deviceD3DGlobal;
    *lplpHALCallbacks = &deviceD3DHALCallbacks;

    return (TRUE);
}

//=============================================================================
//
// In the new DX7 DDI we don't have the Texture Create/Destroy/Swap calls
// anymore, so now we need a mechanism for generating texture handles. This
// is done by the runtime, which will associate a surface handle for each 
// surface created with the DD local object, and will get our D3DCreateSurfaceEx
// callback called. 
//
// Since this creation can very well happen before we create a D3D context, we
// need to keep track of this association, and when we do get called to create
// a D3D context, we will now be given the relevant DD local object pointer to
// resolve which handles are ours (and to which private texture structures we
// need to use).
//
// This mechanism is also used to associate a palette to a texture
//
//=============================================================================

//-----------------------------------------------------------------------------
//
// S3_TEXTURE *TextureHandleToPtr
//
// Find the texture associated to a given texture handle vale (which is to
// say , to a surface handle )
//
//-----------------------------------------------------------------------------

S3_TEXTURE *
TextureHandleToPtr(UINT_PTR thandle, S3_CONTEXT* pContext)
{
    ASSERTDD(NULL != pContext->pHandleList,
                       "pHandleList==NULL in TextureHandleToPtr");

    if (pContext->pHandleList->dwSurfaceList == NULL)
    {
        // avoid AV if our surface list is missing
        return NULL;
    }

    if ((PtrToUlong(pContext->pHandleList->dwSurfaceList[0]) > thandle) && 
        (0 != thandle))
    {
        return pContext->pHandleList->dwSurfaceList[(DWORD)thandle];
    }

    // Request for pointer for an invalid handle returns NULL
    return NULL;
} // TextureHandleToPtr


//-----------------------------------------------------------------------------
//
// BOOL SetTextureSlot
//
// In the handle list element corresponding to this local DD object, store or
// update the pointer to the pTexture associated to the surface handle 
// from the lpDDSLcl surface.
//
//-----------------------------------------------------------------------------
BOOL
SetTextureSlot(LPVOID pDDLcl,
               LPDDRAWI_DDRAWSURFACE_LCL lpDDSLcl,
               LPS3_TEXTURE pTexture)
{
    int   i,j= -1;
    DWORD dwSurfaceHandle;

    DPF_DBG("Entering SetTextureSlot");

    ASSERTDD(NULL != pDDLcl && NULL != lpDDSLcl && NULL != pTexture,
                                    "SetTextureSlot invalid input");
    dwSurfaceHandle = lpDDSLcl->lpSurfMore->dwSurfaceHandle;

    // Find the handle list element associated with the local DD object,
    // if there's none then select an empty one to be used
    for (i = 0; i < MAX_CONTEXT_NUM;i++)
    {
        if (pDDLcl == HandleList[i].pDDLcl)
        {
            break;  // found the right slot
        }
        else
        if (0 == HandleList[i].pDDLcl && -1 == j)
        {
            j=i;    // first empty slot !
        }
    }

    // If we overrun the existing handle list elements, we need to
    // initialize an existing empty slot or return an error.
    if (i >= MAX_CONTEXT_NUM)
    {
        if (-1 != j)
        {
            //has an empty slot for this process, so use it
            i = j;  
            HandleList[j].pDDLcl = pDDLcl;
            ASSERTDD(NULL == HandleList[j].dwSurfaceList,"in SetTextureSlot");
        }
        else
        {
            //all process slots has been used, fail
            DPF_DBG("SetTextureSlot failed with pDDLcl=%x "
                       "dwSurfaceHandle=%08lx pTexture=%x",
                       pDDLcl,dwSurfaceHandle,pTexture);
            return FALSE;
        }
    }

    ASSERTDD(i < MAX_CONTEXT_NUM, "in SetTextureSlot");

    if ( NULL == HandleList[i].dwSurfaceList ||
        dwSurfaceHandle >= PtrToUlong(HandleList[i].dwSurfaceList[0]))
    {
        // dwSurfaceHandle numbers are going to be ordinal numbers starting
        // at one, so we use this number to figure out a "good" size for
        // our new list.
        DWORD newsize = ((dwSurfaceHandle + LISTGROWSIZE) / LISTGROWSIZE)
                                                              * LISTGROWSIZE;
        LPS3_TEXTURE *newlist= (LPS3_TEXTURE *)MEMALLOC(sizeof(LPS3_TEXTURE)*newsize);

        DPF_DBG("Growing pDDLcl=%x's SurfaceList[%x] size to %08lx",
                   pDDLcl,newlist,newsize);

        if (NULL == newlist)
        {
            DPF_DBG("SetTextureSlot failed to increase "
                       "HandleList[%d].dwSurfaceList",i);
            return FALSE;
        }

        memset(newlist,0,newsize);

        // we had a formerly valid surfacehandle list, so we now must 
        // copy it over and free the memory allocated for it
        if (NULL != HandleList[i].dwSurfaceList)
        {
            memcpy(newlist,HandleList[i].dwSurfaceList,
                PtrToUlong(HandleList[i].dwSurfaceList[0]) * 
                sizeof(LPS3_TEXTURE));
            MEMFREE(HandleList[i].dwSurfaceList);
            DPF_DBG("Freeing pDDLcl=%x's old SurfaceList[%x]",
                       pDDLcl,HandleList[i].dwSurfaceList);
        }

        HandleList[i].dwSurfaceList = newlist;
         //store size in dwSurfaceList[0]
        *(DWORD*)HandleList[i].dwSurfaceList = newsize;
    }

    // Store a pointer to the pTexture associated to this surface handle
    HandleList[i].dwSurfaceList[dwSurfaceHandle] = pTexture;
    pTexture->HandleListIndex = i; //store index here to facilitate search
    DPF_DBG("Set pDDLcl=%x Handle=%08lx pTexture = %x",
                pDDLcl, dwSurfaceHandle, pTexture);

    DPF_DBG("Exiting SetTextureSlot");

    return TRUE;
} // SetTextureSlot

//-----------------------------------------------------------------------------
//
// LPS3_TEXTURE GetTextureSlot
//
// Find the pointer to the PPERMEDIA_D3DTEXTURE associated to the 
// dwSurfaceHandle corresponding to the given local DD object
//
//-----------------------------------------------------------------------------
LPS3_TEXTURE
GetTextureSlot(LPVOID pDDLcl, DWORD dwSurfaceHandle)
{
    DWORD   i;
    DPF_DBG("Entering GetTextureSlot");

    for (i = 0; i < MAX_CONTEXT_NUM; i++)
    {
        if (HandleList[i].pDDLcl == pDDLcl)
        {
            if (HandleList[i].dwSurfaceList &&
                PtrToUlong(HandleList[i].dwSurfaceList[0]) > dwSurfaceHandle )
            {
                return  HandleList[i].dwSurfaceList[dwSurfaceHandle];
            }
            else
                break;
        }
    }
    DPF_DBG("Exiting GetTextureSlot");

    return NULL;    //Not found
} // GetTextureSlot

//-----------------------------------------------------------------------------
//
// LPDWLIST GetSurfaceHandleList
//
// Get the handle list which is associated to a specific PDD_DIRECTDRAW_LOCAL
// pDDLcl. It is called from D3DContextCreate to get the handle list associated
// to the pDDLcl with which the context is being created.
//
//-----------------------------------------------------------------------------
LPDWLIST 
GetSurfaceHandleList(LPVOID pDDLcl)
{
    DWORD   i;

    DPF_DBG("Entering GetSurfaceHandleList");

    ASSERTDD(NULL != pDDLcl, "GetSurfaceHandleList get NULL==pDDLcl"); 
    for (i = 0; i < MAX_CONTEXT_NUM;i++)
    {
        if (HandleList[i].pDDLcl == pDDLcl)
        {
            DPF_DBG("Getting pHandleList=%08lx for pDDLcl %x",
                &HandleList[i],pDDLcl);
            return &HandleList[i];
        }
    }

    DPF_DBG("Exiting GetSurfaceHandleList");

    return NULL;   //No surface handle available yet
} // GetSurfaceHandleList

//-----------------------------------------------------------------------------
//
// void ReleaseSurfaceHandleList
//
// Free all the associated surface handle and palette memory pools associated
// to a given DD local object.
//
//-----------------------------------------------------------------------------
void 
ReleaseSurfaceHandleList(LPVOID pDDLcl)
{
    DWORD   i;

    DPF_DBG("Entering ReleaseSurfaceHandleList");

    ASSERTDD(NULL != pDDLcl, "ReleaseSurfaceHandleList get NULL==pDDLcl"); 
    for (i = 0; i < MAX_CONTEXT_NUM; i++)
    {
        if (HandleList[i].pDDLcl == pDDLcl)
        {
            DWORD j;

            if (NULL != HandleList[i].dwSurfaceList)
            {
                DPF_DBG("Releasing HandleList[%d].dwSurfaceList[%x] "
                           "for pDDLcl %x", i, HandleList[i].dwSurfaceList,
                           pDDLcl);

                for (j = 1; j < PtrToUlong(HandleList[i].dwSurfaceList[0]); j++)
                {
                    S3_TEXTURE* pTexture = 
                        (S3_TEXTURE*)HandleList[i].dwSurfaceList[j];

                    if (NULL != pTexture)
                    {
                        MEMFREE(pTexture);
                    }
                }

                MEMFREE(HandleList[i].dwSurfaceList);
                HandleList[i].dwSurfaceList = NULL;
            }

            HandleList[i].pDDLcl = NULL;

            break;
        }
    }

    DPF_DBG("Exiting ReleaseSurfaceHandleList");
} // ReleaseSurfaceHandleList

/******************************Public*Routine******************************\
*
* DWORD D3DGetDriverState
*
* This callback is used by both the DirectDraw and Direct3D runtimes to obtain
* information from the driver about its current state.
*
* Parameters
*
*     lpgdsd
*           pointer to GetDriverState data structure
*
*           dwFlags
*                   Flags to indicate the data required
*           dwhContext
*                   The ID of the context for which information
*                   is being requested
*           lpdwStates
*                   Pointer to the state data to be filled in by the driver
*           dwLength
*                   Length of the state data buffer to be filled
*                   in by the driver
*           ddRVal
*                   Return value
*
* Return Value
*
*      DDHAL_DRIVER_HANDLED
*      DDHAL_DRIVER_NOTHANDLED
\**************************************************************************/
DWORD CALLBACK
D3DGetDriverState( LPDDHAL_GETDRIVERSTATEDATA lpgdsd )
{
    return 0;
}

//-----------------------------------------------------------------------------
//
//  __InitD3DTextureWithDDSurfInfo
//
//  initialize texture structure with its info
//
//-----------------------------------------------------------------------------
void __InitD3DTextureWithDDSurfInfo(LPS3_TEXTURE lpTexture,
                                    LPDDRAWI_DDRAWSURFACE_LCL lpLclTex,
                                    PPDEV ppdev)
{

    if (GetChromaValue(lpLclTex,&(lpTexture->ColorKeyValueLow),
                                &(lpTexture->ColorKeyValueHigh) ) ) {
        lpTexture->ColorKey = TRUE;
        lpTexture->ColorKeySet = FALSE;
    }
    else
        lpTexture->ColorKey = FALSE;

    lpTexture->dwRGBBitCount = lpLclTex->lpGbl->ddpfSurface.dwRGBBitCount;
    lpTexture->lPitch = lpLclTex->lpGbl->lPitch;
    lpTexture->dwWidth = lpLclTex->lpGbl->wWidth;
    lpTexture->dwHeight = lpLclTex->lpGbl->wHeight;
    lpTexture->dwCaps = lpLclTex->ddsCaps.dwCaps;
    lpTexture->dwAlphaBitDepth = lpLclTex->lpGbl->ddpfSurface.dwAlphaBitDepth;
    lpTexture->dwRGBAlphaBitMask = lpLclTex->lpGbl->ddpfSurface.dwRGBAlphaBitMask;
    lpTexture->fpVidMem = lpLclTex->lpGbl->fpVidMem;
}

//-----------------------------------------------------------------------------
//
//  __CreateSurfaceHandle
//
//  allocate a new surface handle
//
//  return value
//
//      DD_OK   -- no error
//      DDERR_OUTOFMEMORY -- allocation of texture handle failed
//
//-----------------------------------------------------------------------------

DWORD __CreateSurfaceHandle( PPDEV ppdev,
                             LPVOID pDDLcl,
                             LPDDRAWI_DDRAWSURFACE_LCL lpDDSLcl)
{
    LPS3_TEXTURE pTexture;

    if (0 == lpDDSLcl->lpSurfMore->dwSurfaceHandle)
    {
        DPF_DBG("D3DCreateSurfaceEx got 0 surfacehandle dwCaps=%08lx",
            lpDDSLcl->ddsCaps.dwCaps);
        return DD_OK;
    }

    pTexture = 
        GetTextureSlot(pDDLcl,lpDDSLcl->lpSurfMore->dwSurfaceHandle);

    if (NULL == pTexture)
    {
        pTexture = (LPS3_TEXTURE)MEMALLOC(sizeof(S3_TEXTURE));

        if (NULL != pTexture) 
        {
            if (!SetTextureSlot(pDDLcl,lpDDSLcl,pTexture))
            {
                return DDERR_OUTOFMEMORY;
            }
        }
        else
        {
            DPF_DBG("ERROR: Couldn't allocate Texture data mem");
            return DDERR_OUTOFMEMORY;
        } 
    }

    // store away ptr for colorkey retrieval later
    lpDDSLcl->dwReserved1 = pTexture->HandleListIndex;

    __InitD3DTextureWithDDSurfInfo(pTexture,lpDDSLcl,ppdev);

    return DD_OK;
}

/******************************Public*Routine******************************\
*
* DWORD D3DCreateSurfaceEx
*
* D3dCreateSurfaceEx creates a Direct3D surface from a DirectDraw surface and
* associates a requested handle value to it.
*
* All Direct3D drivers must support D3dCreateSurfaceEx.
*
* D3dCreateSurfaceEx creates an association between a DirectDraw surface and
* a small integer surface handle. By creating these associations between a
* handle and a DirectDraw surface, D3dCreateSurfaceEx allows a surface handle
* to be imbedded in the Direct3D command stream. For example when the
* D3DDP2OP_TEXBLT command token is sent to D3dDrawPrimitives2 to load a texture
* map, it uses a source handle and destination handle which were associated
*  with a DirectDraw surface through D3dCreateSurfaceEx.
*
* For every DirectDraw surface created under the local DirectDraw object, the
* runtime generates a valid handle that uniquely identifies the surface and
* places it in pcsxd->lpDDSLcl->lpSurfMore->dwSurfaceHandle. This handle value
* is also used with the D3DRENDERSTATE_TEXTUREHANDLE render state to enable
* texturing, and with the D3DDP2OP_SETRENDERTARGET and D3DDP2OP_CLEAR commands
* to set and/or clear new rendering and depth buffers. The driver should fail
* the call and return DDHAL_DRIVER_HANDLE if it cannot create the Direct3D
* surface. If the DDHAL_CREATESURFACEEX_SWAPHANDLES flag is set, the handles
* should be swapped over two sequential calls to D3dCreateSurfaceEx.
* As appropriate, the driver should also store any surface-related information
* that it will subsequently need when using the surface. The driver must create
* a new surface table for each new lpDDLcl and implicitly grow the table when
* necessary to accommodate more surfaces. Typically this is done with an
* exponential growth algorithm so that you don't have to grow the table too
* often. Direct3D calls D3dCreateSurfaceEx after the surface is created by
* DirectDraw by request of the Direct3D runtime or the application.
*
* Parameters
*
*      lpcsxd
*           pointer to CreateSurfaceEx structure that contains the information
*           required for the driver to create the surface (described below).
*
*           dwFlags
*                   May have the value(s):
*                   DDHAL_CREATESURFACEEX_SWAPHANDLES
*                              If this flag is set, D3DCreateSurfaceEx will be
*                              called twice, with different values in lpDDSLcl
*                              in order to swap the associated texture handles
*           lpDDLcl
*                   Handle to the DirectDraw object created by the application
*                   This is the scope within which the lpDDSLcl handles exist.
*                   A DD_DIRECTDRAW_LOCAL structure describes the driver.
*           lpDDSLcl
*                   Handle to the DirectDraw surface we are being asked to
*                   create for Direct3D. These handles are unique within each
*                   different DD_DIRECTDRAW_LOCAL. A DD_SURFACE_LOCAL structure
*                   represents the created surface object.
*           ddRVal
*                   Specifies the location in which the driver writes the return
*                   value of the D3dCreateSurfaceEx callback. A return code of
*                   DD_OK indicates success.
*
* Return Value
*
*      DDHAL_DRIVER_HANDLE
*      DDHAL_DRIVER_NOTHANDLE
*
\**************************************************************************/
DWORD CALLBACK
D3DCreateSurfaceEx( LPDDHAL_CREATESURFACEEXDATA lpcsxd )
{
    S3_TEXTURE pTexture;
    LPVOID pDDLcl= (LPVOID)lpcsxd->lpDDLcl;
    LPDDRAWI_DDRAWSURFACE_LCL   lpDDSLcl=lpcsxd->lpDDSLcl;
    LPATTACHLIST    curr;
    PPDEV ppdev;

    DPF_DBG("Entering D3DCreateSurfaceEx");

    lpcsxd->ddRVal = DD_OK;

    if (NULL == lpDDSLcl || NULL == pDDLcl)
    {
        DPF_DBG("D3DCreateSurfaceEx received 0 lpDDLcl or lpDDSLcl pointer");
        return DDHAL_DRIVER_HANDLED;
    }


    // We check that what we are handling is a texture, zbuffer or a rendering
    // target buffer. We don't check if it is however stored in local video
    // memory since it might also be a system memory texture that we will later
    // blt with __TextureBlt.
    // also if your driver supports DDSCAPS_EXECUTEBUFFER create itself, it must 
    // process DDSCAPS_EXECUTEBUFFER here as well.
    if (!(lpDDSLcl->ddsCaps.dwCaps & 
             (DDSCAPS_TEXTURE       | 
              DDSCAPS_3DDEVICE      | 
              DDSCAPS_ZBUFFER))
       )
    {
        DPF_DBG("D3DCreateSurfaceEx w/o "
             "DDSCAPS_TEXTURE/3DDEVICE/ZBUFFER Ignored"
             "dwCaps=%08lx dwSurfaceHandle=%08lx",
             lpDDSLcl->ddsCaps.dwCaps,
             lpDDSLcl->lpSurfMore->dwSurfaceHandle);
        return DDHAL_DRIVER_HANDLED;
    }

    ppdev=(PPDEV)lpcsxd->lpDDLcl->lpGbl->dhpdev;

    // Now allocate the texture data space
    do 
    {
        if (0 == lpDDSLcl->lpSurfMore->dwSurfaceHandle)
        {
            DPF_DBG("D3DCreateSurfaceEx got 0 surfacehandle dwCaps=%08lx",
                lpDDSLcl->ddsCaps.dwCaps);
            break;
        }

        DPF_DBG("** In D3DCreateSurfaceEx %x %x",
            lpDDSLcl,lpDDSLcl->lpSurfMore->dwSurfaceHandle);

        lpcsxd->ddRVal=__CreateSurfaceHandle( ppdev, pDDLcl, lpDDSLcl);
        if (lpcsxd->ddRVal!=DD_OK)
        {
            break;
        }

        // for some surfaces other than MIPMAP or CUBEMAP, such as
        // flipping chains, we make a slot for every surface, as
        // they are not as interleaved
        if ((lpDDSLcl->ddsCaps.dwCaps & DDSCAPS_MIPMAP) ||
            (lpDDSLcl->lpSurfMore->ddsCapsEx.dwCaps2 & DDSCAPS2_CUBEMAP)
           )
            break;
        curr = lpDDSLcl->lpAttachList;
        if (NULL == curr) 
            break;

        lpDDSLcl=curr->lpAttached;
    }while(NULL != lpDDSLcl);

    DPF_DBG("Exiting D3DCreateSurfaceEx");

    return DDHAL_DRIVER_HANDLED;

}

/******************************Public*Routine******************************\
*
* DWORD D3DDestroyDDLocal
*
* D3dDestroyDDLocal destroys all the Direct3D surfaces previously created by
* D3DCreateSurfaceEx that belong to the same given local DirectDraw object.
*
* All Direct3D drivers must support D3dDestroyDDLocal.
* Direct3D calls D3dDestroyDDLocal when the application indicates that the
* Direct3D context is no longer required and it will be destroyed along with
* all surfaces associated to it. The association comes through the pointer to
* the local DirectDraw object. The driver must free any memory that the
* driver's D3dCreateSurfaceExDDK_D3dCreateSurfaceEx_GG callback allocated for
* each surface if necessary. The driver should not destroy the DirectDraw
* surfaces associated with these Direct3D surfaces; this is the application's
* responsibility.
*
* Parameters
*
*      lpdddd
*            Pointer to the DestoryLocalDD structure that contains the
*            information required for the driver to destroy the surfaces.
*
*            dwFlags
*                  Currently unused
*            pDDLcl
*                  Pointer to the local Direct Draw object which serves as a
*                  reference for all the D3D surfaces that have to be destroyed
*            ddRVal
*                  Specifies the location in which the driver writes the return
*                  value of D3dDestroyDDLocal. A return code of DD_OK indicates
*                   success.
*
* Return Value
*
*      DDHAL_DRIVER_HANDLED
*      DDHAL_DRIVER_NOTHANDLED
\**************************************************************************/
DWORD CALLBACK
D3DDestroyDDLocal( LPDDHAL_DESTROYDDLOCALDATA lpdddd )
{
    DPF_DBG("Entering D3DDestroyDDLocal");

    ReleaseSurfaceHandleList((LPVOID)(lpdddd->pDDLcl));
    lpdddd->ddRVal = DD_OK;

    DPF_DBG("Exiting D3DDestroyDDLocal");

    return DDHAL_DRIVER_HANDLED;
}

/******************************Public*Routine******************************\
*
* DWORD D3DSetColorKey32
*
*  DirectDraw SetColorkey callback
*
*  Parameters
*       lpSetColorKey
*             Pointer to the LPDDHAL_SETCOLORKEYDATA parameters structure
*
*             lpDDSurface
*                  Surface struct
*             dwFlags
*                  Flags
*             ckNew
*                  New chroma key color values
*             ddRVal
*                  Return value
*             SetColorKey
*                  Unused: Win95 compatibility
*
\**************************************************************************/
DWORD CALLBACK
D3DSetColorKey32 ( LPDDHAL_SETCOLORKEYDATA lpscd)
{
    DWORD dwSurfaceHandle =
                        lpscd->lpDDSurface->lpSurfMore->dwSurfaceHandle;
    DWORD index = (DWORD)lpscd->lpDDSurface->dwReserved1;

    DPF_DBG("Entering D3DSetColorKey32 dwSurfaceHandle=%d index=%d",
        dwSurfaceHandle, index);

    lpscd->ddRVal = DD_OK;



    // We don't have to do anything for normal blt source colour keys:
    if ((DDSCAPS_TEXTURE & lpscd->lpDDSurface->ddsCaps.dwCaps) &&
        (DDSCAPS_VIDEOMEMORY & lpscd->lpDDSurface->ddsCaps.dwCaps)
       )
    {
        if (0 != dwSurfaceHandle && NULL != HandleList[index].dwSurfaceList)
        {
            S3_TEXTURE *pTexture = HandleList[index].dwSurfaceList[dwSurfaceHandle];

            ASSERTDD(PtrToUlong(HandleList[0].dwSurfaceList[index])>dwSurfaceHandle,
                "SetColorKey: incorrect dwSurfaceHandle");

            if (NULL != pTexture)
            {
                if (lpscd->lpDDSurface->dwFlags & DDRAWISURF_HASCKEYSRCBLT)
                {
                    pTexture->ColorKeyValueLow = lpscd->ckNew.dwColorSpaceLowValue;
                    pTexture->ColorKeyValueHigh = lpscd->ckNew.dwColorSpaceHighValue;
                    pTexture->ColorKey = TRUE;
                    pTexture->ColorKeySet = FALSE;
                    DPF_DBG("D3DSetColorKey32 surface=%08lx KeyLow=%08lx",
                        dwSurfaceHandle,pTexture->ColorKeyValueLow);
                }
                else
                {
                    pTexture->ColorKey = FALSE;
                    pTexture->ColorKeySet = FALSE;
                }
            } else
                lpscd->ddRVal = DDERR_SURFACELOST;
        }
        else
        {
            lpscd->ddRVal = DDERR_SURFACELOST;
        }
    }

    DPF_DBG("Exiting D3DSetColorKey32");

    // exit calling the DD Colorkey function
    return DdSetColorKey(lpscd);
}

//-----------------------------------------------------------------------------
//
//  AssertModeDirect3D
//
//  Called by DrvAssertMode as part of display mode change handling. This 
//  function does the necessary management on the Direct3D side.
//
//  Parameters
//      ppdev 
//              Pointer to PDEV structure
//      bEnable
//              bEnable from DrvAssertMode    
//
//-----------------------------------------------------------------------------
VOID AssertModeDirect3D(PDEV *ppdev, BOOL bEnable)
{
    DWORD dwContextIndex,   dwSurfaceIndex;
    S3_TEXTURE   **SurfaceList;
    S3_TEXTURE   *pTexture;
    S3_CONTEXT     *pContext;

    if (!bEnable)
    {   

         // We are having a mode switch
         //
         // All Surfaces in VideoMemory are lost. However d3d contexts which
         // have not been destroyed go to an inactive state, and all the
         // surface objects are still persistent across mode changes. If we
         // return back to this pdev after another mode switch all these
         // inactive contexts would be reactivated.
         //
         // The surfaces however would be marked as lost and any rendering
         // operations to them will be returned with DERR_SURFACELOST.
         // The application on receiving this error needs to recreate the
         // surface. Please note that any  context(s) which needed to be
         // Destroyed would have already received the callback D3DContextDestroy

        pContext = ppdev->D3DGlobals.FirstContxt;
        while (pContext)
        {
            /* We have hit any active context */
            if ((pContext->pHandleList) && (pContext->ppdev == ppdev))
            {
                SurfaceList = pContext->pHandleList->dwSurfaceList;
                if (SurfaceList)
                {
                    for (dwSurfaceIndex = 1;
                         dwSurfaceIndex < (PtrToUlong(SurfaceList[0]));
                         dwSurfaceIndex++)
                    {
                        pTexture = SurfaceList[dwSurfaceIndex];
                        if (pTexture)
                        {
                            if (pTexture->dwCaps & DDSCAPS_VIDEOMEMORY)
                                SurfaceList[dwSurfaceIndex] = NULL;
                        }  
                    }
                }
            }

            pContext = pContext->pCtxtNext; 
        }

    }
} // AssertModeDirect3D



//-----------------------------------------------------------------------------
//
//  DumpSlots
//
//  Debugging aid for the HandleList
//
//-----------------------------------------------------------------------------
void DumpSlots(void)
{
    DWORD   i,j;

    for (i = 0; i < MAX_CONTEXT_NUM; i++)
    {
        if (HandleList[i].pDDLcl != NULL)
        {
            DPF("Handle list # %d , pDDLcl = %x",i, HandleList[i].pDDLcl);
            for (j=1 ; j < 100 ; j++)
            if (HandleList[i].dwSurfaceList && HandleList[i].dwSurfaceList[j])
            {
                DPF("Texture slot # %d , fpVidMem = %x (%d x %d, %d bpp=%d)",j,
                    HandleList[i].dwSurfaceList[j]->fpVidMem,
                    HandleList[i].dwSurfaceList[j]->dwWidth,
                    HandleList[i].dwSurfaceList[j]->dwHeight,
                    HandleList[i].dwSurfaceList[j]->lPitch,
                    HandleList[i].dwSurfaceList[j]->dwAlphaBitDepth
                    );
            }
        }
    }
}
