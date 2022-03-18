/******************************Module*Header*******************************\
*
*                           *******************
*                           * D3D SAMPLE CODE *
*                           *******************
* Module Name: d3ddrv.h
*
*  Content:    Header for Direct3D HAL driver.
*
* Copyright (C) 1996-1998 Microsoft Corporation.  All Rights Reserved.
\**************************************************************************/

#ifndef __D3DDRV__
#define __D3DDRV__

#include "dx95type.h"
#include "math.h"

#include <d3dtypes.h>
#include "virge1.h"
#include "s3dmacro.h"

#define D3D_STATEBLOCKS 1

// S3 Virge chip types

#define D_S3VIRGE       0x3156      // Virge
#define D_S3VIRGEVX     0x3d88      // Virge VX
#define D_S3VIRGEDXGX   0x018a      // Virge DX/GX
#define D_S3VIRGEGX2    0x108a      // Virge GX2
#define D_S3M3          0x008c      // M3
#define D_S3M5          0x018c      // M5

// Debugging fine detail macro , if you want
//to fine level debug then #define DPF_DBG DPF
__inline VOID  DPF_DBG(LPSTR szFormat, ...) {}
//#define DPF_DBG DPF

#define VTX_VAL_LIMIT (4096.0)

#define CHECK_VERTEX_VALUE(pTLVertex)                \
    if ((pTLVertex->sx >  VTX_VAL_LIMIT) ||          \
        (pTLVertex->sy >  VTX_VAL_LIMIT) ||          \
        (pTLVertex->sx < -VTX_VAL_LIMIT) ||          \
        (pTLVertex->sy < -VTX_VAL_LIMIT)) {          \
        DPF("Vertex exceeded limits: "               \
                "(%li,%li)",                         \
                 (LONG)pTLVertex->sx,                \
                 (LONG)pTLVertex->sy);               \
        /* abort rendering operation */              \
        return;                                      \
    }

// State management macros
#define MAX_STATE       D3DSTATE_OVERRIDE_BIAS
#define DWORD_BITS      32
#define DWORD_SHIFT     5

typedef struct _D3DSAMPStateSet {
    DWORD               bits[MAX_STATE >> DWORD_SHIFT];
} D3DSAMPStateSet;

#define IS_OVERRIDE(type)       ((DWORD)(type) > D3DSTATE_OVERRIDE_BIAS)
#define GET_OVERRIDE(type)      ((DWORD)(type) - D3DSTATE_OVERRIDE_BIAS)

#define STATESET_MASK(set, state)       \
(set).bits[((state) - 1) >> DWORD_SHIFT]

#define STATESET_BIT(state)     (1 << (((state) - 1) & (DWORD_BITS - 1)))

#define STATESET_ISSET(set, state) \
STATESET_MASK(set, state) & STATESET_BIT(state)

#define STATESET_SET(set, state) \
STATESET_MASK(set, state) |= STATESET_BIT(state)

#define STATESET_CLEAR(set, state) \
STATESET_MASK(set, state) &= ~STATESET_BIT(state)

#define STATESET_INIT(set)      memset(&(set), 0, sizeof(set))


// Bit mask for RGBA4444 texture formats
#define S3VIRGE_RGBA4444_ALPHABITMASK     (0xf000)

//Following are masks for dwRCode in _S3_CONTEXT, by seting and clearing
//this bits we select a specific triangle rendering function out of 64
#define S3GOURAUD       0x0001
#define S3DMA           0x0002
#define S3ZBUFFER       0x0004
#define S3TEXTURED      0x0008
#define S3PERSPECTIVE   0x0010
#define S3FOGGED        0x0020

//dwStatus bits in _S3_CONTEXT , they allow us to handle the indicated states
#define S3COLORKEYENABLE    0x01
#define S3ALPHABLENDENABLE  0x02
#define S3FOGENABLE         0x04
#define S3SPECULARENABLE    0x08
#define S3MULTITEXTURE      0x20    // this controls which texture to use

//Forward definition of table of pointers to triangle rendering
//functions, indexed by dwRCode
typedef void (*LPRENDERTRIANGLE)(LPS3_CONTEXT,
                                 LPD3DTLVERTEX,
                                 LPD3DTLVERTEX,
                                 LPD3DTLVERTEX,
                                 LPS3FVFOFFSETS);
extern LPRENDERTRIANGLE pRenderTriangle[];

#if D3D_STATEBLOCKS
//-----------------------------------------------------------------------------
//                     State sets structure definitions
//-----------------------------------------------------------------------------
#define FLAG DWORD
#define FLAG_SIZE (8*sizeof(DWORD))

typedef struct _S3StateSetRec {
    DWORD                   dwHandle;
    DWORD                   bCompressed;

    union {
        struct {
            // Stored state block info (uncompressed)
            DWORD RenderStates[MAX_STATE];
            DWORD TssStates[D3DTSS_TEXTURETRANSFORMFLAGS+1];

            FLAG bStoredRS[(MAX_STATE + FLAG_SIZE)/ FLAG_SIZE];
            FLAG bStoredTSS[(D3DTSS_TEXTURETRANSFORMFLAGS + FLAG_SIZE) / FLAG_SIZE];
        } uc;
        struct {
            // Stored state block info (compressed)
            DWORD dwNumRS;
            DWORD dwNumTSS;
            struct {
                DWORD dwType;
                DWORD dwValue;
            } pair[1];
        } cc;
    } u;

} S3StateSetRec;

#define SSPTRS_PERPAGE (4096/sizeof(S3StateSetRec *))

#define FLAG_SET(flag, number)     \
    flag[ (number) / FLAG_SIZE ] |= (1 << ((number) % FLAG_SIZE))

#define IS_FLAG_SET(flag, number)  \
    (flag[ (number) / FLAG_SIZE ] & (1 << ((number) % FLAG_SIZE) ))

#endif //D3D_STATEBLOCKS

//-----------------------------------------------------------------------------
//                     Texture structure definitions
//-----------------------------------------------------------------------------
//Texture management structure
typedef struct _S3_TEXTURES {
    HANDLE              hTexture;
    LPDDRAWI_DDRAWSURFACE_LCL lpLcl;  // Texture
    //added color key for texture map
    DWORD       ColorKeyValueLow;     // color key value low
    DWORD       ColorKeyValueHigh;    // color key value high
    BOOL        ColorKey;             // is there a color key ?
    BOOL        ColorKeySet;          // has the color key been handled by alpha bit?
    ULONG HandleListIndex;    // indicating which list it's with
    // Linked list continues with this..
    struct _S3_TEXTURES *Next, *Prev;
    DWORD dwRGBBitCount;
    DWORD lPitch;
    DWORD dwWidth;
    DWORD dwHeight;
    DWORD dwCaps;
    DWORD dwRGBAlphaBitMask;
    DWORD dwAlphaBitDepth;
    FLATPTR fpVidMem;

} S3_TEXTURE, *LPS3_TEXTURE;


//-----------------------------------------------------------------------------
//                       DX7 Texture management definitions
//-----------------------------------------------------------------------------

#define LISTGROWSIZE    1024
typedef struct _DWLIST
{
    LPS3_TEXTURE   *dwSurfaceList;   // array to hold handles, 
                                     // dynamically allocated 
                                     // dwSurfaceList[0] is the number 
                                     // of entries in dwSurfaceList 
                                     // if allocated
    LPVOID  pDDLcl;                  // owning ddraw pointer as a key
} DWLIST;
typedef DWLIST FAR* LPDWLIST;
extern DWLIST  HandleList[]; 
extern LPDWLIST GetSurfaceHandleList(LPVOID);
void ReleaseSurfaceHandleList(LPVOID);


//--------------------------------------------------------------------------
//                       Context indexing structure
//--------------------------------------------------------------------------
#define MAX_CONTEXT_NUM 200
extern  UINT_PTR ContextSlots[];

//-----------------------------------------------------------------------------
//                     Context structure definitions
//-----------------------------------------------------------------------------

// Driver D3D Rendering Context, each app creates its own
typedef struct _S3_CONTEXT {

    PDEV                *ppdev;         // associated pdev

    volatile vi13D_TRIANGLE *g_p3DTri;  // hw parm regs for 3D triangle
    volatile vi13D_SETUP *g_p3DStp;     // hw setup regs for 3D triangle

    ULONG               dwPID;          // This context's Process ID
    D3DSAMPStateSet     overrides;
    DWORD_PTR           dwTexture;
    ULONG               FrameAddr;      // DD object primary display.
    S3_TEXTURE          *pSurfRender;
    S3_TEXTURE          *pSurfZBuffer;
    UINT_PTR             RenderSurfaceHandle;
    UINT_PTR             ZBufferHandle;
    D3DCULL             CullMode;       // NONE, CW (clockwise), CCW
    D3DTEXTUREBLEND     BlendMode;      // Texture blend mode (decal, modulate, etc.)
    D3DTEXTUREFILTER    TextureMode;    // Texture filtering function
    D3DCMPFUNC          ZCmpFunc;       // Z comparison function
    D3DSHADEMODE        ShadeMode;      // Shade mode (flat, Gouraud, etc.)
    D3DTEXTUREADDRESS   TextureAddress; // Texture address mode (wrap/clamp)
    D3DFILLMODE         FillMode;       // filled
    BOOL                bSpecular;      // Specular lighting
    BOOL                bWrapU, bWrapV; // TRUE to enable wraps for each coord
    DWORD               dwStatus;       // All blending
    BOOL                bFogEnabled;    // Per-vertex fog
    BOOL                bZEnabled;      // Z buffer updates enabled
    BOOL                bZWriteEnabled;
    BOOL                bZVisible;
    DWORD               dwRCode;        // for binary value
    DWORD               rndCommand;     // Command word ready to send to hardware
    vi13D_SETUP         rnd3DSetup;     // Setup registers
    DWORD               rsfMaxMipMapLevel; // size of texture as power of 2
    float               fTextureWidth;  // precalculate these
    float               fTextureHeight;
    double              dTexValtoInt;   // and this
    BOOL                bChopped;       // Indicated the triangle is already chopped in UV space
    BOOL                bChanged;       // setup needed
    BOOL                Alpha_workaround; // 24 bit alpha workaround
    D3DBLEND            SrcBlend;
    D3DBLEND            DstBlend;
    DWORD               tssStates[D3DTSS_BUMPENVLOFFSET+1]; // D3D DX6 TSS States for one texture stage
#if D3D_STATEBLOCKS
    DWORD RenderStates[MAX_STATE];
    BOOL bStateRecMode;            // Toggle for executing or recording states
    S3StateSetRec   *pCurrSS;      // Ptr to SS currently being recorded
    S3StateSetRec   **pIndexTableSS; // Pointer to table of indexes
    DWORD           dwMaxSSIndex;    // size of table of indexes
#endif
    struct _S3_CONTEXT  *pCtxtLast;
    struct _S3_CONTEXT  *pCtxtNext;

    LPDWLIST    pHandleList;
    LPVOID      pDDLcl;             // Local surface pointer used as a ID

} S3_CONTEXT, *LPS3_CONTEXT;

// macros for global device data
#define D3DGLOBALPTR(c) c->ppdev

#if D3D_STATEBLOCKS

void __BeginStateSet(S3_CONTEXT* pContext, DWORD dwParam);
void __EndStateSet(S3_CONTEXT* pContext);
void __DeleteStateSet(S3_CONTEXT* pContext, DWORD dwParam);
void __ExecuteStateSet(S3_CONTEXT* pContext, DWORD dwParam);
void __CaptureStateSet(S3_CONTEXT* pContext, DWORD dwParam);
void __DeleteAllStateSets(S3_CONTEXT* pContext);

#endif //D3D_STATEBLOCKS


//Validation functions

#define CHK_CONTEXT(pCtxt, retVar, funcName)               \
    if (!pCtxt){                                           \
        DPF( "In %s ,bad context handle = NULL",funcName); \
        retVar = D3DHAL_CONTEXT_BAD;                       \
        return DDHAL_DRIVER_HANDLED;                       \
    }

#define CHK_VALID_SURFACE(lpLcl, retVar, funcName)            \
    if (!lpLcl) {                                             \
        DPF( "In %s ,null surface ",funcName);                \
        retVar = DDERR_CURRENTLYNOTAVAIL;                     \
        return DDHAL_DRIVER_HANDLED;                          \
    }

#define CHK_NOTSYSTEM_SURF(surf, retVar, type, funcName)                   \
    if ( DDSurf_Get_dwCaps(DDS_LCL(surf)) & DDSCAPS_SYSTEMMEMORY ) {       \
        DPF( "in %s, %ssurface in SYSTEM MEMORY, failing",funcName,type ); \
        retVar = DDERR_CURRENTLYNOTAVAIL;                                  \
        return DDHAL_DRIVER_HANDLED;                                       \
    }


// Memory heap management
#define MEMALLOC(cbSize)    EngAllocMem(FL_ZERO_MEMORY, cbSize, 'dD3D')
#define MEMFREE(lpPtr)      EngFreeMem(lpPtr)
#define GetMyRefData(x) ((PDEV *)x->dhpdev)

// DX6 FVF  Support declarations

typedef struct _S3TEXCOORDS{
    D3DVALUE tu;
    D3DVALUE tv;
} S3TEXCOORDS, *LPS3TEXCOORDS;

typedef struct _S3COLOR {
    D3DCOLOR color;
} S3COLOR, *LPS3COLOR;

typedef struct _S3SPECULAR {
    D3DCOLOR specular;
} S3SPECULAR, *LPS3SPECULAR;

typedef struct _S3FVFOFFSETS{
        DWORD dwColOffset;
        DWORD dwSpcOffset;
        DWORD dwTexOffset;
        DWORD dwTexBaseOffset;
        DWORD dwStride;
} S3FVFOFFSETS , *LPS3FVFOFFSETS;

    // copy vertexes correctly even if they are not D3DTLVERTEXes
__inline void __CpyFVFVertexes(LPD3DTLVERTEX pVDest,
                               LPD3DTLVERTEX pVSrc,
                               LPS3FVFOFFSETS lpS3FVFOff)
{
    if (lpS3FVFOff == NULL) {
        *pVDest = *pVSrc;
    } else {
        memcpy( pVDest, pVSrc, lpS3FVFOff->dwStride );
    }
}

    // track appropriate pointers to fvf vertex components
__inline void __SetFVFOffsets (DWORD  *lpdwColorOffs,
                               DWORD  *lpdwSpecularOffs,
                               DWORD  *lpdwTexOffs,
                               LPS3FVFOFFSETS lpS3FVFOff)
{
    if (lpS3FVFOff == NULL) {
        // Default non-FVF case , we just set up everything as for a D3DTLVERTEX
        *lpdwColorOffs    = offsetof( D3DTLVERTEX, color);
        *lpdwSpecularOffs = offsetof( D3DTLVERTEX, specular);
        *lpdwTexOffs      = offsetof( D3DTLVERTEX, tu);
    } else {
        // Use the offsets info to setup the corresponding fields
        *lpdwColorOffs    = lpS3FVFOff->dwColOffset;
        *lpdwSpecularOffs = lpS3FVFOff->dwSpcOffset;
        *lpdwTexOffs      = lpS3FVFOff->dwTexOffset;
    }
}

#define FVFTEX( lpVtx , dwOffs )     ((LPS3TEXCOORDS)((LPBYTE)(lpVtx) + dwOffs))
#define FVFCOLOR( lpVtx, dwOffs )    ((LPS3COLOR)((LPBYTE)(lpVtx) + dwOffs))
#define FVFSPEC( lpVtx, dwOffs)      ((LPS3SPECULAR)((LPBYTE)(lpVtx) + dwOffs))

typedef  struct {     // structure to hold temporary FVF vertexes
        D3DTLVERTEX TLvtx;
        D3DVALUE    tu1,tv1,  // we must provide for (largest) FVF case
                    tu2,tv2,
                    tu3,tv3,
                    tu4,tv4,
                    tu5,tv5,
                    tu6,tv6,
                    tu7,tv7;
    } FVFVERTEX;

// Utility function declarations

void GenericWireFrameTriangle (S3_CONTEXT     *pCtxt,
                               LPD3DTLVERTEX  p0,
                               LPD3DTLVERTEX  p1,
                               LPD3DTLVERTEX  p2,
                               LPS3FVFOFFSETS   lpS3FVFOff);

void GenericPointTriangle (S3_CONTEXT     *pCtxt,
                               LPD3DTLVERTEX  p0,
                               LPD3DTLVERTEX  p1,
                               LPD3DTLVERTEX  p2,
                               LPS3FVFOFFSETS   lpS3FVFOff);

BOOL GetChromaValue(LPDDRAWI_DDRAWSURFACE_LCL lpLcl,
                    LPDWORD lpdwChromaLow,
                    LPDWORD lpdwChromaHigh);

BOOL TesselateInUVSpace(S3_CONTEXT *pCtxt,
                        LPD3DTLVERTEX p0,
                        LPD3DTLVERTEX p1,
                        LPD3DTLVERTEX p2,
                        LPS3FVFOFFSETS   lpS3FVFOff);


void __HWRenderLine(S3_CONTEXT *pCtxt,
                    LPD3DTLVERTEX p0,
                    LPD3DTLVERTEX p1,
                    LPD3DTLVERTEX pFlat,
                    LPS3FVFOFFSETS lpS3FVFOff);

void __HWRenderPoint(S3_CONTEXT *pCtxt,
                     LPD3DTLVERTEX p0,
                     LPS3FVFOFFSETS lpS3FVFOff);


HRESULT WINAPI __HWSetupState(S3_CONTEXT *pCtxt,
                              DWORD StateType,
                              DWORD StateValue,
                              LPDWORD lpdwRS);

HRESULT WINAPI __HWSetupPrimitive(S3_CONTEXT *pCtxt,
                                  LPS3FVFOFFSETS lpS3FVFOff);

LPRENDERTRIANGLE __HWSetTriangleFunc(S3_CONTEXT *pCtxt);

HRESULT S3VirgeInit(S3_CONTEXT *pCtxt);

HRESULT S3GetDeviceRevision(PPDEV ppdev,WORD *pId ,ULONG *pRevision);

DWORD CALLBACK
D3DGetDriverState( LPDDHAL_GETDRIVERSTATEDATA);

DWORD CALLBACK
D3DCreateSurfaceEx( LPDDHAL_CREATESURFACEEXDATA);

DWORD CALLBACK
D3DDestroyDDLocal( LPDDHAL_DESTROYDDLOCALDATA);

DWORD CALLBACK
D3DSetColorKey32(LPDDHAL_SETCOLORKEYDATA psckd);

DWORD DdSetColorKey(PDD_SETCOLORKEYDATA lpSetColorKey);

// 3D Related utility definitions

//Line texture coordinate wrapping macro
#define WRAP2( p0 , p1 )                    \
{                                           \
    _PRECISION rp;                          \
                                            \
    rp = p1 - p0;                           \
    if( rp > (_PRECISION)0.5 ) {            \
       p1 -= (_PRECISION)1.0;               \
    } else if( rp < (_PRECISION)(-0.5) ) {  \
       p1 += (_PRECISION)1.0;               \
    }                                       \
}

//Triangle texture coordinate wrapping macro
#define MYWRAP(t0, t1, t2)         \
{                                  \
    if ((t0 <= t1) && (t0 <= t2)) {\
        WRAP2(t0,t1);              \
        WRAP2(t0,t2);              \
    } else                         \
    if ((t1 <= t0) && (t1 <= t2)) {\
        WRAP2(t1,t0);              \
        WRAP2(t1,t2);              \
    } else {                       \
        WRAP2(t2,t0);              \
        WRAP2(t2,t1);              \
    }                              \
}


//Triangle culling macro
#define CULL_TRI(ctxt,p0,p1,p2)                                              \
    ((ctxt->CullMode!=D3DCULL_NONE) &&                                       \
     ((ctxt->CullMode==D3DCULL_CCW &&                                        \
        (p1->sx-p0->sx)*(p2->sy-p0->sy)<=(p2->sx-p0->sx)*(p1->sy-p0->sy)) || \
      (ctxt->CullMode== D3DCULL_CW &&                                        \
        (p1->sx-p0->sx)*(p2->sy-p0->sy)>=(p2->sx-p0->sx)*(p1->sy-p0->sy)) ) )


//Color utility macros
#define D3D_GETALPHA(c) (((BYTE *)&c)[3])
#define D3D_GETRED(c)   (((BYTE *)&c)[2])
#define D3D_GETGREEN(c) (((BYTE *)&c)[1])
#define D3D_GETBLUE(c)  (((BYTE *)&c)[0])

#define D3D_GETSPECULAR(c)                          \
      ( ( D3D_GETBLUE(c) > D3D_GETGREEN(c) ) ?      \
            ( (D3D_GETBLUE(c) > D3D_GETRED(c))?     \
                  D3D_GETBLUE(c)                    \
            :                                       \
                  D3D_GETRED(c) )                   \
        :  ( (D3D_GETGREEN(c) > D3D_GETRED(c))?     \
                  D3D_GETGREEN(c)                   \
            :                                       \
                  D3D_GETRED(c) )  )

#define INTERPCOLOR(color0, color1, RGBA_GETCOMPONENT, ratio)       \
        (BYTE) ((RGBA_GETCOMPONENT(color0) + MYFLINTUCHAR(ratio *   \
              ((float) RGBA_GETCOMPONENT(color1) -                  \
               (float) RGBA_GETCOMPONENT(color0)) ) )&0xFF)

#define CLAMP888(result, color, specular) \
     result = (color & 0xfefefe) + (specular & 0xfefefe);   \
     result |= ((0x808080 - ((result >> 8) & 0x010101)) & 0x7f7f7f) << 1;

#define TEXEL_IN_COLORKEY_RANGE(texel, colHigh, colLow, rMask, gmask, bMask) \
    ( ((texel) & (rMask)) <= ((colHigh) & (rMask)) ) &&                              \
    ( ((texel) & (rMask)) >= ((colLow)  & (rMask)) ) &&                              \
    ( ((texel) & (gmask)) <= ((colHigh) & (gmask)) ) &&                              \
    ( ((texel) & (gmask)) >= ((colLow)  & (gmask)) ) &&                              \
    ( ((texel) & (bMask)) <= ((colHigh) & (bMask)) ) &&                              \
    ( ((texel) & (bMask)) >= ((colLow)  & (bMask)) )

// texture coordinate fixup value
#define TEXTURE_FACTOR (float)(1.0/256.0)


// non-specific
#define DWSWAP( x, y ) { DWORD temp=(DWORD)y; y = x; (DWORD)x = temp; }
#define PTRSWAP( x, y ) { ULONG_PTR temp=(ULONG_PTR)y; y = x; (ULONG_PTR)x =temp;}

//-----------------------------------------------------------------------------
//                      Texture hash table definitions
//-----------------------------------------------------------------------------
#define TEXTURE_HASH_SIZE   256     // these many entries in the hash table

void InitTextureHashTable(S3_CONTEXT   *pContext);

// Then the hash funtion is just an 'and'
#define TEXTURE_HASH_OF(i)  ((i) & 0xff)

S3_TEXTURE *TextureHandleToPtr(UINT_PTR thandle, S3_CONTEXT* pContext);

//-----------------------------------------------------------------------------
//  One special legacy texture op we can;t easily map into the new texture ops
//-----------------------------------------------------------------------------
#define D3DTOP_LEGACY_ALPHAOVR (0x7fffffff)

// Temporary data structure we are using here until d3dnthal.h gets updated AZN
typedef struct {
    DWORD       dwOperation;
    DWORD       dwParam;
    DWORD       dwReserved;
} P2D3DHAL_DP2STATESET;

#endif // __D3DDRV__


