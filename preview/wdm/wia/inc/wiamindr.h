
#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 5.03.0279 */
/* at Wed Jan 19 16:38:37 2000
 */
/* Compiler settings for wiamindr.idl:
    Os (OptLev=s), W1, Zp8, env=Win32 (32b run), ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __wiamindr_h__
#define __wiamindr_h__

/* Forward Declarations */ 

#ifndef __IWiaMiniDrv_FWD_DEFINED__
#define __IWiaMiniDrv_FWD_DEFINED__
typedef interface IWiaMiniDrv IWiaMiniDrv;
#endif 	/* __IWiaMiniDrv_FWD_DEFINED__ */


#ifndef __IWiaMiniDrvCallBack_FWD_DEFINED__
#define __IWiaMiniDrvCallBack_FWD_DEFINED__
typedef interface IWiaMiniDrvCallBack IWiaMiniDrvCallBack;
#endif 	/* __IWiaMiniDrvCallBack_FWD_DEFINED__ */


#ifndef __IWiaDrvItem_FWD_DEFINED__
#define __IWiaDrvItem_FWD_DEFINED__
typedef interface IWiaDrvItem IWiaDrvItem;
#endif 	/* __IWiaDrvItem_FWD_DEFINED__ */


/* header files for imported files */
#include "unknwn.h"
#include "oaidl.h"
#include "propidl.h"
#include "wia.h"

#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 

/* interface __MIDL_itf_wiamindr_0000 */
/* [local] */ 






typedef struct _MINIDRV_TRANSFER_CONTEXT
    {
    LONG lSize;
    LONG lWidthInPixels;
    LONG lLines;
    LONG lDepth;
    LONG lXRes;
    LONG lYRes;
    LONG lCompression;
    GUID guidFormatID;
    LONG tymed;
    LONG hFile;
    LONG cbOffset;
    LONG lBufferSize;
    LONG lActiveBuffer;
    LONG lNumBuffers;
    BYTE __RPC_FAR *pBaseBuffer;
    BYTE __RPC_FAR *pTransferBuffer;
    BOOL bTransferDataCB;
    BOOL bClassDrvAllocBuf;
    LONG lClientAddress;
    IWiaMiniDrvCallBack __RPC_FAR *pIWiaMiniDrvCallBack;
    LONG lImageSize;
    LONG lHeaderSize;
    LONG lItemSize;
    LONG cbWidthInBytes;
    LONG lPage;
    LONG lCurIfdOffset;
    LONG lPrevIfdOffset;
    }	MINIDRV_TRANSFER_CONTEXT;

typedef struct _MINIDRV_TRANSFER_CONTEXT __RPC_FAR *PMINIDRV_TRANSFER_CONTEXT;

typedef struct _WIA_DEV_CAP_DRV
    {
    GUID __RPC_FAR *guid;
    ULONG ulFlags;
    LPOLESTR wszName;
    LPOLESTR wszDescription;
    LPOLESTR wszIcon;
    }	WIA_DEV_CAP_DRV;

typedef struct _WIA_DEV_CAP_DRV __RPC_FAR *PWIA_DEV_CAP_DRV;



extern RPC_IF_HANDLE __MIDL_itf_wiamindr_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_wiamindr_0000_v0_0_s_ifspec;

#ifndef __IWiaMiniDrv_INTERFACE_DEFINED__
#define __IWiaMiniDrv_INTERFACE_DEFINED__

/* interface IWiaMiniDrv */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IWiaMiniDrv;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("d8cdee14-3c6c-11d2-9a35-00c04fa36145")
    IWiaMiniDrv : public IUnknown
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvInitializeWia( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0014,
            /* [in] */ LONG __MIDL_0015,
            /* [in] */ BSTR __MIDL_0016,
            /* [in] */ BSTR __MIDL_0017,
            /* [in] */ IUnknown __RPC_FAR *__MIDL_0018,
            /* [in] */ IUnknown __RPC_FAR *__MIDL_0019,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0020,
            /* [out] */ IUnknown __RPC_FAR *__RPC_FAR *__MIDL_0021,
            /* [out] */ LONG __RPC_FAR *__MIDL_0022) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvAcquireItemData( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0023,
            /* [in] */ LONG __MIDL_0024,
            /* [out][in] */ PMINIDRV_TRANSFER_CONTEXT __MIDL_0025,
            /* [out] */ LONG __RPC_FAR *__MIDL_0026) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvInitItemProperties( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0027,
            /* [in] */ LONG __MIDL_0028,
            /* [out] */ LONG __RPC_FAR *__MIDL_0029) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvValidateItemProperties( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0030,
            /* [in] */ LONG __MIDL_0031,
            /* [in] */ ULONG __MIDL_0032,
            /* [in] */ const PROPSPEC __RPC_FAR *__MIDL_0033,
            /* [out] */ LONG __RPC_FAR *__MIDL_0034) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvWriteItemProperties( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0035,
            /* [in] */ LONG __MIDL_0036,
            /* [in] */ PMINIDRV_TRANSFER_CONTEXT __MIDL_0037,
            /* [out] */ LONG __RPC_FAR *__MIDL_0038) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvReadItemProperties( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0039,
            /* [in] */ LONG __MIDL_0040,
            /* [in] */ ULONG __MIDL_0041,
            /* [in] */ const PROPSPEC __RPC_FAR *__MIDL_0042,
            /* [out] */ LONG __RPC_FAR *__MIDL_0043) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvLockWiaDevice( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0044,
            /* [in] */ LONG __MIDL_0045,
            /* [out] */ LONG __RPC_FAR *__MIDL_0046) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvUnLockWiaDevice( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0047,
            /* [in] */ LONG __MIDL_0048,
            /* [out] */ LONG __RPC_FAR *__MIDL_0049) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvAnalyzeItem( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0050,
            /* [in] */ LONG __MIDL_0051,
            /* [in] */ LONG __RPC_FAR *__MIDL_0052) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvGetDeviceErrorStr( 
            /* [in] */ LONG __MIDL_0053,
            /* [in] */ LONG __MIDL_0054,
            /* [out] */ LPOLESTR __RPC_FAR *__MIDL_0055,
            /* [out] */ LONG __RPC_FAR *__MIDL_0056) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvDeviceCommand( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0057,
            /* [in] */ LONG __MIDL_0058,
            /* [in] */ const GUID __RPC_FAR *__MIDL_0059,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0060,
            /* [out] */ LONG __RPC_FAR *__MIDL_0061) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvGetCapabilities( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0062,
            /* [in] */ LONG __MIDL_0063,
            /* [out] */ LONG __RPC_FAR *__MIDL_0064,
            /* [out] */ WIA_DEV_CAP_DRV __RPC_FAR *__RPC_FAR *__MIDL_0065,
            /* [out] */ LONG __RPC_FAR *__MIDL_0066) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvDeleteItem( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0067,
            /* [in] */ LONG __MIDL_0068,
            /* [out] */ LONG __RPC_FAR *__MIDL_0069) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvFreeDrvItemContext( 
            /* [in] */ LONG __MIDL_0070,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0071,
            /* [out] */ LONG __RPC_FAR *__MIDL_0072) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvGetWiaFormatInfo( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0073,
            /* [in] */ LONG __MIDL_0074,
            /* [out] */ LONG __RPC_FAR *__MIDL_0075,
            /* [out] */ WIA_FORMAT_INFO __RPC_FAR *__RPC_FAR *__MIDL_0076,
            /* [out] */ LONG __RPC_FAR *__MIDL_0077) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvNotifyPnpEvent( 
            /* [in] */ const GUID __RPC_FAR *pEventGUID,
            /* [in] */ BSTR bstrDeviceID,
            /* [in] */ ULONG ulReserved) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE drvUnInitializeWia( 
            /* [in] */ BYTE __RPC_FAR *__MIDL_0078) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IWiaMiniDrvVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IWiaMiniDrv __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IWiaMiniDrv __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvInitializeWia )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0014,
            /* [in] */ LONG __MIDL_0015,
            /* [in] */ BSTR __MIDL_0016,
            /* [in] */ BSTR __MIDL_0017,
            /* [in] */ IUnknown __RPC_FAR *__MIDL_0018,
            /* [in] */ IUnknown __RPC_FAR *__MIDL_0019,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0020,
            /* [out] */ IUnknown __RPC_FAR *__RPC_FAR *__MIDL_0021,
            /* [out] */ LONG __RPC_FAR *__MIDL_0022);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvAcquireItemData )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0023,
            /* [in] */ LONG __MIDL_0024,
            /* [out][in] */ PMINIDRV_TRANSFER_CONTEXT __MIDL_0025,
            /* [out] */ LONG __RPC_FAR *__MIDL_0026);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvInitItemProperties )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0027,
            /* [in] */ LONG __MIDL_0028,
            /* [out] */ LONG __RPC_FAR *__MIDL_0029);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvValidateItemProperties )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0030,
            /* [in] */ LONG __MIDL_0031,
            /* [in] */ ULONG __MIDL_0032,
            /* [in] */ const PROPSPEC __RPC_FAR *__MIDL_0033,
            /* [out] */ LONG __RPC_FAR *__MIDL_0034);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvWriteItemProperties )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0035,
            /* [in] */ LONG __MIDL_0036,
            /* [in] */ PMINIDRV_TRANSFER_CONTEXT __MIDL_0037,
            /* [out] */ LONG __RPC_FAR *__MIDL_0038);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvReadItemProperties )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0039,
            /* [in] */ LONG __MIDL_0040,
            /* [in] */ ULONG __MIDL_0041,
            /* [in] */ const PROPSPEC __RPC_FAR *__MIDL_0042,
            /* [out] */ LONG __RPC_FAR *__MIDL_0043);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvLockWiaDevice )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0044,
            /* [in] */ LONG __MIDL_0045,
            /* [out] */ LONG __RPC_FAR *__MIDL_0046);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvUnLockWiaDevice )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0047,
            /* [in] */ LONG __MIDL_0048,
            /* [out] */ LONG __RPC_FAR *__MIDL_0049);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvAnalyzeItem )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0050,
            /* [in] */ LONG __MIDL_0051,
            /* [in] */ LONG __RPC_FAR *__MIDL_0052);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvGetDeviceErrorStr )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ LONG __MIDL_0053,
            /* [in] */ LONG __MIDL_0054,
            /* [out] */ LPOLESTR __RPC_FAR *__MIDL_0055,
            /* [out] */ LONG __RPC_FAR *__MIDL_0056);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvDeviceCommand )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0057,
            /* [in] */ LONG __MIDL_0058,
            /* [in] */ const GUID __RPC_FAR *__MIDL_0059,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0060,
            /* [out] */ LONG __RPC_FAR *__MIDL_0061);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvGetCapabilities )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0062,
            /* [in] */ LONG __MIDL_0063,
            /* [out] */ LONG __RPC_FAR *__MIDL_0064,
            /* [out] */ WIA_DEV_CAP_DRV __RPC_FAR *__RPC_FAR *__MIDL_0065,
            /* [out] */ LONG __RPC_FAR *__MIDL_0066);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvDeleteItem )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0067,
            /* [in] */ LONG __MIDL_0068,
            /* [out] */ LONG __RPC_FAR *__MIDL_0069);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvFreeDrvItemContext )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ LONG __MIDL_0070,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0071,
            /* [out] */ LONG __RPC_FAR *__MIDL_0072);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvGetWiaFormatInfo )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0073,
            /* [in] */ LONG __MIDL_0074,
            /* [out] */ LONG __RPC_FAR *__MIDL_0075,
            /* [out] */ WIA_FORMAT_INFO __RPC_FAR *__RPC_FAR *__MIDL_0076,
            /* [out] */ LONG __RPC_FAR *__MIDL_0077);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvNotifyPnpEvent )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ const GUID __RPC_FAR *pEventGUID,
            /* [in] */ BSTR bstrDeviceID,
            /* [in] */ ULONG ulReserved);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *drvUnInitializeWia )( 
            IWiaMiniDrv __RPC_FAR * This,
            /* [in] */ BYTE __RPC_FAR *__MIDL_0078);
        
        END_INTERFACE
    } IWiaMiniDrvVtbl;

    interface IWiaMiniDrv
    {
        CONST_VTBL struct IWiaMiniDrvVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWiaMiniDrv_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IWiaMiniDrv_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IWiaMiniDrv_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IWiaMiniDrv_drvInitializeWia(This,__MIDL_0014,__MIDL_0015,__MIDL_0016,__MIDL_0017,__MIDL_0018,__MIDL_0019,__MIDL_0020,__MIDL_0021,__MIDL_0022)	\
    (This)->lpVtbl -> drvInitializeWia(This,__MIDL_0014,__MIDL_0015,__MIDL_0016,__MIDL_0017,__MIDL_0018,__MIDL_0019,__MIDL_0020,__MIDL_0021,__MIDL_0022)

#define IWiaMiniDrv_drvAcquireItemData(This,__MIDL_0023,__MIDL_0024,__MIDL_0025,__MIDL_0026)	\
    (This)->lpVtbl -> drvAcquireItemData(This,__MIDL_0023,__MIDL_0024,__MIDL_0025,__MIDL_0026)

#define IWiaMiniDrv_drvInitItemProperties(This,__MIDL_0027,__MIDL_0028,__MIDL_0029)	\
    (This)->lpVtbl -> drvInitItemProperties(This,__MIDL_0027,__MIDL_0028,__MIDL_0029)

#define IWiaMiniDrv_drvValidateItemProperties(This,__MIDL_0030,__MIDL_0031,__MIDL_0032,__MIDL_0033,__MIDL_0034)	\
    (This)->lpVtbl -> drvValidateItemProperties(This,__MIDL_0030,__MIDL_0031,__MIDL_0032,__MIDL_0033,__MIDL_0034)

#define IWiaMiniDrv_drvWriteItemProperties(This,__MIDL_0035,__MIDL_0036,__MIDL_0037,__MIDL_0038)	\
    (This)->lpVtbl -> drvWriteItemProperties(This,__MIDL_0035,__MIDL_0036,__MIDL_0037,__MIDL_0038)

#define IWiaMiniDrv_drvReadItemProperties(This,__MIDL_0039,__MIDL_0040,__MIDL_0041,__MIDL_0042,__MIDL_0043)	\
    (This)->lpVtbl -> drvReadItemProperties(This,__MIDL_0039,__MIDL_0040,__MIDL_0041,__MIDL_0042,__MIDL_0043)

#define IWiaMiniDrv_drvLockWiaDevice(This,__MIDL_0044,__MIDL_0045,__MIDL_0046)	\
    (This)->lpVtbl -> drvLockWiaDevice(This,__MIDL_0044,__MIDL_0045,__MIDL_0046)

#define IWiaMiniDrv_drvUnLockWiaDevice(This,__MIDL_0047,__MIDL_0048,__MIDL_0049)	\
    (This)->lpVtbl -> drvUnLockWiaDevice(This,__MIDL_0047,__MIDL_0048,__MIDL_0049)

#define IWiaMiniDrv_drvAnalyzeItem(This,__MIDL_0050,__MIDL_0051,__MIDL_0052)	\
    (This)->lpVtbl -> drvAnalyzeItem(This,__MIDL_0050,__MIDL_0051,__MIDL_0052)

#define IWiaMiniDrv_drvGetDeviceErrorStr(This,__MIDL_0053,__MIDL_0054,__MIDL_0055,__MIDL_0056)	\
    (This)->lpVtbl -> drvGetDeviceErrorStr(This,__MIDL_0053,__MIDL_0054,__MIDL_0055,__MIDL_0056)

#define IWiaMiniDrv_drvDeviceCommand(This,__MIDL_0057,__MIDL_0058,__MIDL_0059,__MIDL_0060,__MIDL_0061)	\
    (This)->lpVtbl -> drvDeviceCommand(This,__MIDL_0057,__MIDL_0058,__MIDL_0059,__MIDL_0060,__MIDL_0061)

#define IWiaMiniDrv_drvGetCapabilities(This,__MIDL_0062,__MIDL_0063,__MIDL_0064,__MIDL_0065,__MIDL_0066)	\
    (This)->lpVtbl -> drvGetCapabilities(This,__MIDL_0062,__MIDL_0063,__MIDL_0064,__MIDL_0065,__MIDL_0066)

#define IWiaMiniDrv_drvDeleteItem(This,__MIDL_0067,__MIDL_0068,__MIDL_0069)	\
    (This)->lpVtbl -> drvDeleteItem(This,__MIDL_0067,__MIDL_0068,__MIDL_0069)

#define IWiaMiniDrv_drvFreeDrvItemContext(This,__MIDL_0070,__MIDL_0071,__MIDL_0072)	\
    (This)->lpVtbl -> drvFreeDrvItemContext(This,__MIDL_0070,__MIDL_0071,__MIDL_0072)

#define IWiaMiniDrv_drvGetWiaFormatInfo(This,__MIDL_0073,__MIDL_0074,__MIDL_0075,__MIDL_0076,__MIDL_0077)	\
    (This)->lpVtbl -> drvGetWiaFormatInfo(This,__MIDL_0073,__MIDL_0074,__MIDL_0075,__MIDL_0076,__MIDL_0077)

#define IWiaMiniDrv_drvNotifyPnpEvent(This,pEventGUID,bstrDeviceID,ulReserved)	\
    (This)->lpVtbl -> drvNotifyPnpEvent(This,pEventGUID,bstrDeviceID,ulReserved)

#define IWiaMiniDrv_drvUnInitializeWia(This,__MIDL_0078)	\
    (This)->lpVtbl -> drvUnInitializeWia(This,__MIDL_0078)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvInitializeWia_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0014,
    /* [in] */ LONG __MIDL_0015,
    /* [in] */ BSTR __MIDL_0016,
    /* [in] */ BSTR __MIDL_0017,
    /* [in] */ IUnknown __RPC_FAR *__MIDL_0018,
    /* [in] */ IUnknown __RPC_FAR *__MIDL_0019,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0020,
    /* [out] */ IUnknown __RPC_FAR *__RPC_FAR *__MIDL_0021,
    /* [out] */ LONG __RPC_FAR *__MIDL_0022);


void __RPC_STUB IWiaMiniDrv_drvInitializeWia_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvAcquireItemData_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0023,
    /* [in] */ LONG __MIDL_0024,
    /* [out][in] */ PMINIDRV_TRANSFER_CONTEXT __MIDL_0025,
    /* [out] */ LONG __RPC_FAR *__MIDL_0026);


void __RPC_STUB IWiaMiniDrv_drvAcquireItemData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvInitItemProperties_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0027,
    /* [in] */ LONG __MIDL_0028,
    /* [out] */ LONG __RPC_FAR *__MIDL_0029);


void __RPC_STUB IWiaMiniDrv_drvInitItemProperties_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvValidateItemProperties_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0030,
    /* [in] */ LONG __MIDL_0031,
    /* [in] */ ULONG __MIDL_0032,
    /* [in] */ const PROPSPEC __RPC_FAR *__MIDL_0033,
    /* [out] */ LONG __RPC_FAR *__MIDL_0034);


void __RPC_STUB IWiaMiniDrv_drvValidateItemProperties_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvWriteItemProperties_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0035,
    /* [in] */ LONG __MIDL_0036,
    /* [in] */ PMINIDRV_TRANSFER_CONTEXT __MIDL_0037,
    /* [out] */ LONG __RPC_FAR *__MIDL_0038);


void __RPC_STUB IWiaMiniDrv_drvWriteItemProperties_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvReadItemProperties_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0039,
    /* [in] */ LONG __MIDL_0040,
    /* [in] */ ULONG __MIDL_0041,
    /* [in] */ const PROPSPEC __RPC_FAR *__MIDL_0042,
    /* [out] */ LONG __RPC_FAR *__MIDL_0043);


void __RPC_STUB IWiaMiniDrv_drvReadItemProperties_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvLockWiaDevice_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0044,
    /* [in] */ LONG __MIDL_0045,
    /* [out] */ LONG __RPC_FAR *__MIDL_0046);


void __RPC_STUB IWiaMiniDrv_drvLockWiaDevice_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvUnLockWiaDevice_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0047,
    /* [in] */ LONG __MIDL_0048,
    /* [out] */ LONG __RPC_FAR *__MIDL_0049);


void __RPC_STUB IWiaMiniDrv_drvUnLockWiaDevice_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvAnalyzeItem_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0050,
    /* [in] */ LONG __MIDL_0051,
    /* [in] */ LONG __RPC_FAR *__MIDL_0052);


void __RPC_STUB IWiaMiniDrv_drvAnalyzeItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvGetDeviceErrorStr_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ LONG __MIDL_0053,
    /* [in] */ LONG __MIDL_0054,
    /* [out] */ LPOLESTR __RPC_FAR *__MIDL_0055,
    /* [out] */ LONG __RPC_FAR *__MIDL_0056);


void __RPC_STUB IWiaMiniDrv_drvGetDeviceErrorStr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvDeviceCommand_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0057,
    /* [in] */ LONG __MIDL_0058,
    /* [in] */ const GUID __RPC_FAR *__MIDL_0059,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0060,
    /* [out] */ LONG __RPC_FAR *__MIDL_0061);


void __RPC_STUB IWiaMiniDrv_drvDeviceCommand_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvGetCapabilities_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0062,
    /* [in] */ LONG __MIDL_0063,
    /* [out] */ LONG __RPC_FAR *__MIDL_0064,
    /* [out] */ WIA_DEV_CAP_DRV __RPC_FAR *__RPC_FAR *__MIDL_0065,
    /* [out] */ LONG __RPC_FAR *__MIDL_0066);


void __RPC_STUB IWiaMiniDrv_drvGetCapabilities_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvDeleteItem_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0067,
    /* [in] */ LONG __MIDL_0068,
    /* [out] */ LONG __RPC_FAR *__MIDL_0069);


void __RPC_STUB IWiaMiniDrv_drvDeleteItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvFreeDrvItemContext_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ LONG __MIDL_0070,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0071,
    /* [out] */ LONG __RPC_FAR *__MIDL_0072);


void __RPC_STUB IWiaMiniDrv_drvFreeDrvItemContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvGetWiaFormatInfo_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0073,
    /* [in] */ LONG __MIDL_0074,
    /* [out] */ LONG __RPC_FAR *__MIDL_0075,
    /* [out] */ WIA_FORMAT_INFO __RPC_FAR *__RPC_FAR *__MIDL_0076,
    /* [out] */ LONG __RPC_FAR *__MIDL_0077);


void __RPC_STUB IWiaMiniDrv_drvGetWiaFormatInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvNotifyPnpEvent_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ const GUID __RPC_FAR *pEventGUID,
    /* [in] */ BSTR bstrDeviceID,
    /* [in] */ ULONG ulReserved);


void __RPC_STUB IWiaMiniDrv_drvNotifyPnpEvent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrv_drvUnInitializeWia_Proxy( 
    IWiaMiniDrv __RPC_FAR * This,
    /* [in] */ BYTE __RPC_FAR *__MIDL_0078);


void __RPC_STUB IWiaMiniDrv_drvUnInitializeWia_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IWiaMiniDrv_INTERFACE_DEFINED__ */


#ifndef __IWiaMiniDrvCallBack_INTERFACE_DEFINED__
#define __IWiaMiniDrvCallBack_INTERFACE_DEFINED__

/* interface IWiaMiniDrvCallBack */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IWiaMiniDrvCallBack;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("33a57d5a-3de8-11d2-9a36-00c04fa36145")
    IWiaMiniDrvCallBack : public IUnknown
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE MiniDrvCallback( 
            /* [in] */ LONG lReason,
            /* [in] */ LONG lStatus,
            /* [in] */ LONG lPercentComplete,
            /* [in] */ LONG lOffset,
            /* [in] */ LONG lLength,
            /* [in] */ PMINIDRV_TRANSFER_CONTEXT pTranCtx,
            /* [in] */ LONG lReserved) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IWiaMiniDrvCallBackVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IWiaMiniDrvCallBack __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IWiaMiniDrvCallBack __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IWiaMiniDrvCallBack __RPC_FAR * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *MiniDrvCallback )( 
            IWiaMiniDrvCallBack __RPC_FAR * This,
            /* [in] */ LONG lReason,
            /* [in] */ LONG lStatus,
            /* [in] */ LONG lPercentComplete,
            /* [in] */ LONG lOffset,
            /* [in] */ LONG lLength,
            /* [in] */ PMINIDRV_TRANSFER_CONTEXT pTranCtx,
            /* [in] */ LONG lReserved);
        
        END_INTERFACE
    } IWiaMiniDrvCallBackVtbl;

    interface IWiaMiniDrvCallBack
    {
        CONST_VTBL struct IWiaMiniDrvCallBackVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWiaMiniDrvCallBack_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IWiaMiniDrvCallBack_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IWiaMiniDrvCallBack_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IWiaMiniDrvCallBack_MiniDrvCallback(This,lReason,lStatus,lPercentComplete,lOffset,lLength,pTranCtx,lReserved)	\
    (This)->lpVtbl -> MiniDrvCallback(This,lReason,lStatus,lPercentComplete,lOffset,lLength,pTranCtx,lReserved)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IWiaMiniDrvCallBack_MiniDrvCallback_Proxy( 
    IWiaMiniDrvCallBack __RPC_FAR * This,
    /* [in] */ LONG lReason,
    /* [in] */ LONG lStatus,
    /* [in] */ LONG lPercentComplete,
    /* [in] */ LONG lOffset,
    /* [in] */ LONG lLength,
    /* [in] */ PMINIDRV_TRANSFER_CONTEXT pTranCtx,
    /* [in] */ LONG lReserved);


void __RPC_STUB IWiaMiniDrvCallBack_MiniDrvCallback_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IWiaMiniDrvCallBack_INTERFACE_DEFINED__ */


#ifndef __IWiaDrvItem_INTERFACE_DEFINED__
#define __IWiaDrvItem_INTERFACE_DEFINED__

/* interface IWiaDrvItem */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IWiaDrvItem;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1f02b5c5-b00c-11d2-a094-00c04f72dc3c")
    IWiaDrvItem : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetItemFlags( 
            /* [out] */ LONG __RPC_FAR *__MIDL_0079) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetDeviceSpecContext( 
            /* [out] */ BYTE __RPC_FAR *__RPC_FAR *__MIDL_0080) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetFullItemName( 
            /* [out] */ BSTR __RPC_FAR *__MIDL_0081) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetItemName( 
            /* [out] */ BSTR __RPC_FAR *__MIDL_0082) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddItemToFolder( 
            /* [in] */ IWiaDrvItem __RPC_FAR *__MIDL_0083) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE UnlinkItemTree( 
            /* [in] */ LONG __MIDL_0084) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveItemFromFolder( 
            /* [in] */ LONG __MIDL_0085) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FindItemByName( 
            /* [in] */ LONG __MIDL_0086,
            /* [in] */ BSTR __MIDL_0087,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0088) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE FindChildItemByName( 
            /* [in] */ BSTR __MIDL_0089,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0090) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetParentItem( 
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0091) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetFirstChildItem( 
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0092) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetNextSiblingItem( 
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0093) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DumpItemData( 
            /* [out] */ BSTR __RPC_FAR *__MIDL_0094) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IWiaDrvItemVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IWiaDrvItem __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IWiaDrvItem __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetItemFlags )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ LONG __RPC_FAR *__MIDL_0079);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetDeviceSpecContext )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ BYTE __RPC_FAR *__RPC_FAR *__MIDL_0080);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetFullItemName )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ BSTR __RPC_FAR *__MIDL_0081);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetItemName )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ BSTR __RPC_FAR *__MIDL_0082);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *AddItemToFolder )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [in] */ IWiaDrvItem __RPC_FAR *__MIDL_0083);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *UnlinkItemTree )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [in] */ LONG __MIDL_0084);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *RemoveItemFromFolder )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [in] */ LONG __MIDL_0085);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *FindItemByName )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [in] */ LONG __MIDL_0086,
            /* [in] */ BSTR __MIDL_0087,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0088);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *FindChildItemByName )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [in] */ BSTR __MIDL_0089,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0090);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetParentItem )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0091);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetFirstChildItem )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0092);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetNextSiblingItem )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0093);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *DumpItemData )( 
            IWiaDrvItem __RPC_FAR * This,
            /* [out] */ BSTR __RPC_FAR *__MIDL_0094);
        
        END_INTERFACE
    } IWiaDrvItemVtbl;

    interface IWiaDrvItem
    {
        CONST_VTBL struct IWiaDrvItemVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWiaDrvItem_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IWiaDrvItem_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IWiaDrvItem_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IWiaDrvItem_GetItemFlags(This,__MIDL_0079)	\
    (This)->lpVtbl -> GetItemFlags(This,__MIDL_0079)

#define IWiaDrvItem_GetDeviceSpecContext(This,__MIDL_0080)	\
    (This)->lpVtbl -> GetDeviceSpecContext(This,__MIDL_0080)

#define IWiaDrvItem_GetFullItemName(This,__MIDL_0081)	\
    (This)->lpVtbl -> GetFullItemName(This,__MIDL_0081)

#define IWiaDrvItem_GetItemName(This,__MIDL_0082)	\
    (This)->lpVtbl -> GetItemName(This,__MIDL_0082)

#define IWiaDrvItem_AddItemToFolder(This,__MIDL_0083)	\
    (This)->lpVtbl -> AddItemToFolder(This,__MIDL_0083)

#define IWiaDrvItem_UnlinkItemTree(This,__MIDL_0084)	\
    (This)->lpVtbl -> UnlinkItemTree(This,__MIDL_0084)

#define IWiaDrvItem_RemoveItemFromFolder(This,__MIDL_0085)	\
    (This)->lpVtbl -> RemoveItemFromFolder(This,__MIDL_0085)

#define IWiaDrvItem_FindItemByName(This,__MIDL_0086,__MIDL_0087,__MIDL_0088)	\
    (This)->lpVtbl -> FindItemByName(This,__MIDL_0086,__MIDL_0087,__MIDL_0088)

#define IWiaDrvItem_FindChildItemByName(This,__MIDL_0089,__MIDL_0090)	\
    (This)->lpVtbl -> FindChildItemByName(This,__MIDL_0089,__MIDL_0090)

#define IWiaDrvItem_GetParentItem(This,__MIDL_0091)	\
    (This)->lpVtbl -> GetParentItem(This,__MIDL_0091)

#define IWiaDrvItem_GetFirstChildItem(This,__MIDL_0092)	\
    (This)->lpVtbl -> GetFirstChildItem(This,__MIDL_0092)

#define IWiaDrvItem_GetNextSiblingItem(This,__MIDL_0093)	\
    (This)->lpVtbl -> GetNextSiblingItem(This,__MIDL_0093)

#define IWiaDrvItem_DumpItemData(This,__MIDL_0094)	\
    (This)->lpVtbl -> DumpItemData(This,__MIDL_0094)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetItemFlags_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ LONG __RPC_FAR *__MIDL_0079);


void __RPC_STUB IWiaDrvItem_GetItemFlags_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetDeviceSpecContext_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ BYTE __RPC_FAR *__RPC_FAR *__MIDL_0080);


void __RPC_STUB IWiaDrvItem_GetDeviceSpecContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetFullItemName_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ BSTR __RPC_FAR *__MIDL_0081);


void __RPC_STUB IWiaDrvItem_GetFullItemName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetItemName_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ BSTR __RPC_FAR *__MIDL_0082);


void __RPC_STUB IWiaDrvItem_GetItemName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_AddItemToFolder_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [in] */ IWiaDrvItem __RPC_FAR *__MIDL_0083);


void __RPC_STUB IWiaDrvItem_AddItemToFolder_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_UnlinkItemTree_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [in] */ LONG __MIDL_0084);


void __RPC_STUB IWiaDrvItem_UnlinkItemTree_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_RemoveItemFromFolder_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [in] */ LONG __MIDL_0085);


void __RPC_STUB IWiaDrvItem_RemoveItemFromFolder_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_FindItemByName_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [in] */ LONG __MIDL_0086,
    /* [in] */ BSTR __MIDL_0087,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0088);


void __RPC_STUB IWiaDrvItem_FindItemByName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_FindChildItemByName_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [in] */ BSTR __MIDL_0089,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0090);


void __RPC_STUB IWiaDrvItem_FindChildItemByName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetParentItem_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0091);


void __RPC_STUB IWiaDrvItem_GetParentItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetFirstChildItem_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0092);


void __RPC_STUB IWiaDrvItem_GetFirstChildItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_GetNextSiblingItem_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ IWiaDrvItem __RPC_FAR *__RPC_FAR *__MIDL_0093);


void __RPC_STUB IWiaDrvItem_GetNextSiblingItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IWiaDrvItem_DumpItemData_Proxy( 
    IWiaDrvItem __RPC_FAR * This,
    /* [out] */ BSTR __RPC_FAR *__MIDL_0094);


void __RPC_STUB IWiaDrvItem_DumpItemData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IWiaDrvItem_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_wiamindr_0131 */
/* [local] */ 

typedef struct _WIA_DATA_THREAD_INFO
    {
    LONG lReason;
    LONG lStatus;
    LONG lPercentComplete;
    LONG lOffset;
    LONG lLength;
    LONG lClientAddress;
    LONG lMarshalLength;
    BYTE __RPC_FAR *pBuffer;
    BOOL bTerminateThread;
    HANDLE hEventStart;
    HANDLE hEventComplete;
    HRESULT hr;
    IWiaDataCallback __RPC_FAR *pIDataCallback;
    }	WIA_DATA_THREAD_INFO;

typedef struct _WIA_DATA_THREAD_INFO __RPC_FAR *PWIA_DATA_THREAD_INFO;

typedef struct _WIA_PROPERTY_INFO
    {
    ULONG lAccessFlags;
    VARTYPE vt;
    union 
        {
        struct 
            {
            LONG Min;
            LONG Nom;
            LONG Max;
            LONG Inc;
            }	Range;
        struct 
            {
            DOUBLE Min;
            DOUBLE Nom;
            DOUBLE Max;
            DOUBLE Inc;
            }	RangeFloat;
        struct 
            {
            LONG cNumList;
            LONG Nom;
            BYTE __RPC_FAR *pList;
            }	List;
        struct 
            {
            LONG cNumList;
            DOUBLE Nom;
            BYTE __RPC_FAR *pList;
            }	ListFloat;
        struct 
            {
            LONG cNumList;
            GUID Nom;
            GUID __RPC_FAR *pList;
            }	ListGuid;
        struct 
            {
            LONG cNumList;
            BSTR Nom;
            BSTR __RPC_FAR *pList;
            }	ListBStr;
        struct 
            {
            LONG Nom;
            LONG ValidBits;
            }	Flag;
        struct 
            {
            LONG Dummy;
            }	None;
        }	ValidVal;
    }	WIA_PROPERTY_INFO;

typedef struct _WIA_PROPERTY_INFO __RPC_FAR *PWIA_PROPERTY_INFO;

typedef struct _WIA_PROPERTY_CONTEXT
    {
    ULONG cProps;
    PROPID __RPC_FAR *pProps;
    BOOL __RPC_FAR *pChanged;
    }	WIA_PROPERTY_CONTEXT;

typedef struct _WIA_PROPERTY_CONTEXT __RPC_FAR *PWIA_PROPERTY_CONTEXT;

typedef struct _WIAS_CHANGED_VALUE_INFO
    {
    BOOL bChanged;
    LONG vt;
    union 
        {
        LONG lVal;
        FLOAT fltVal;
        BSTR bstrVal;
        GUID guidVal;
        }	Old;
    union 
        {
        LONG lVal;
        FLOAT fltVal;
        BSTR bstrVal;
        GUID guidVal;
        }	Current;
    }	WIAS_CHANGED_VALUE_INFO;

typedef struct _WIAS_CHANGED_VALUE_INFO __RPC_FAR *PWIAS_CHANGED_VALUE_INFO;

#include "wiamdef.h"


extern RPC_IF_HANDLE __MIDL_itf_wiamindr_0131_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_wiamindr_0131_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long __RPC_FAR *, unsigned long            , BSTR __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserMarshal(  unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserUnmarshal(unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long __RPC_FAR *, BSTR __RPC_FAR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif



