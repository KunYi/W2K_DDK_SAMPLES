
#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0323 */
/* Compiler settings for wiaview.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
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

#ifndef __wiaview_h__
#define __wiaview_h__

/* Forward Declarations */ 

#ifndef __IVisibleElement_FWD_DEFINED__
#define __IVisibleElement_FWD_DEFINED__
typedef interface IVisibleElement IVisibleElement;
#endif 	/* __IVisibleElement_FWD_DEFINED__ */


#ifndef __IWIACategory_FWD_DEFINED__
#define __IWIACategory_FWD_DEFINED__
typedef interface IWIACategory IWIACategory;
#endif 	/* __IWIACategory_FWD_DEFINED__ */


#ifndef __IWIACategories_FWD_DEFINED__
#define __IWIACategories_FWD_DEFINED__
typedef interface IWIACategories IWIACategories;
#endif 	/* __IWIACategories_FWD_DEFINED__ */


#ifndef __IWIAVideoUI_FWD_DEFINED__
#define __IWIAVideoUI_FWD_DEFINED__
typedef interface IWIAVideoUI IWIAVideoUI;
#endif 	/* __IWIAVideoUI_FWD_DEFINED__ */


#ifndef __IVideoPreview_FWD_DEFINED__
#define __IVideoPreview_FWD_DEFINED__
typedef interface IVideoPreview IVideoPreview;
#endif 	/* __IVideoPreview_FWD_DEFINED__ */


#ifndef __WIACamUI_FWD_DEFINED__
#define __WIACamUI_FWD_DEFINED__

#ifdef __cplusplus
typedef class WIACamUI WIACamUI;
#else
typedef struct WIACamUI WIACamUI;
#endif /* __cplusplus */

#endif 	/* __WIACamUI_FWD_DEFINED__ */


#ifndef __Categories_FWD_DEFINED__
#define __Categories_FWD_DEFINED__

#ifdef __cplusplus
typedef class Categories Categories;
#else
typedef struct Categories Categories;
#endif /* __cplusplus */

#endif 	/* __Categories_FWD_DEFINED__ */


#ifndef __UIElement_FWD_DEFINED__
#define __UIElement_FWD_DEFINED__

#ifdef __cplusplus
typedef class UIElement UIElement;
#else
typedef struct UIElement UIElement;
#endif /* __cplusplus */

#endif 	/* __UIElement_FWD_DEFINED__ */


#ifndef __Category_FWD_DEFINED__
#define __Category_FWD_DEFINED__

#ifdef __cplusplus
typedef class Category Category;
#else
typedef struct Category Category;
#endif /* __cplusplus */

#endif 	/* __Category_FWD_DEFINED__ */


#ifndef __VideoPreview_FWD_DEFINED__
#define __VideoPreview_FWD_DEFINED__

#ifdef __cplusplus
typedef class VideoPreview VideoPreview;
#else
typedef struct VideoPreview VideoPreview;
#endif /* __cplusplus */

#endif 	/* __VideoPreview_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 

#ifndef __IVisibleElement_INTERFACE_DEFINED__
#define __IVisibleElement_INTERFACE_DEFINED__

/* interface IVisibleElement */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IVisibleElement;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("5B68A670-887A-11D2-8067-00805F6596D2")
    IVisibleElement : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR __RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_Type( 
            /* [retval][out] */ BSTR __RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_StringCount( 
            /* [retval][out] */ long __RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE String( 
            VARIANT index,
            /* [retval][out] */ BSTR __RPC_FAR *pStr) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Clicked( 
            BSTR string,
            BOOL bOn) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVisibleElementVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IVisibleElement __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IVisibleElement __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IVisibleElement __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IVisibleElement __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IVisibleElement __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IVisibleElement __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IVisibleElement __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Name )( 
            IVisibleElement __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *pVal);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Type )( 
            IVisibleElement __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *pVal);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_StringCount )( 
            IVisibleElement __RPC_FAR * This,
            /* [retval][out] */ long __RPC_FAR *pVal);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *String )( 
            IVisibleElement __RPC_FAR * This,
            VARIANT index,
            /* [retval][out] */ BSTR __RPC_FAR *pStr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Clicked )( 
            IVisibleElement __RPC_FAR * This,
            BSTR string,
            BOOL bOn);
        
        END_INTERFACE
    } IVisibleElementVtbl;

    interface IVisibleElement
    {
        CONST_VTBL struct IVisibleElementVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVisibleElement_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVisibleElement_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVisibleElement_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVisibleElement_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVisibleElement_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVisibleElement_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVisibleElement_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVisibleElement_get_Name(This,pVal)	\
    (This)->lpVtbl -> get_Name(This,pVal)

#define IVisibleElement_get_Type(This,pVal)	\
    (This)->lpVtbl -> get_Type(This,pVal)

#define IVisibleElement_get_StringCount(This,pVal)	\
    (This)->lpVtbl -> get_StringCount(This,pVal)

#define IVisibleElement_String(This,index,pStr)	\
    (This)->lpVtbl -> String(This,index,pStr)

#define IVisibleElement_Clicked(This,string,bOn)	\
    (This)->lpVtbl -> Clicked(This,string,bOn)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVisibleElement_get_Name_Proxy( 
    IVisibleElement __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *pVal);


void __RPC_STUB IVisibleElement_get_Name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVisibleElement_get_Type_Proxy( 
    IVisibleElement __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *pVal);


void __RPC_STUB IVisibleElement_get_Type_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVisibleElement_get_StringCount_Proxy( 
    IVisibleElement __RPC_FAR * This,
    /* [retval][out] */ long __RPC_FAR *pVal);


void __RPC_STUB IVisibleElement_get_StringCount_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVisibleElement_String_Proxy( 
    IVisibleElement __RPC_FAR * This,
    VARIANT index,
    /* [retval][out] */ BSTR __RPC_FAR *pStr);


void __RPC_STUB IVisibleElement_String_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVisibleElement_Clicked_Proxy( 
    IVisibleElement __RPC_FAR * This,
    BSTR string,
    BOOL bOn);


void __RPC_STUB IVisibleElement_Clicked_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVisibleElement_INTERFACE_DEFINED__ */


#ifndef __IWIACategory_INTERFACE_DEFINED__
#define __IWIACategory_INTERFACE_DEFINED__

/* interface IWIACategory */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IWIACategory;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("C1252E30-887A-11D2-8067-00805F6596D2")
    IWIACategory : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR __RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ElementCount( 
            /* [retval][out] */ long __RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Element( 
            VARIANT index,
            /* [retval][out] */ IVisibleElement __RPC_FAR *__RPC_FAR *ppElement) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_OnSelect( 
            /* [retval][out] */ BSTR __RPC_FAR *pStr) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_OnExit( 
            /* [retval][out] */ BSTR __RPC_FAR *pStr) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_Icon( 
            /* [retval][out] */ BSTR __RPC_FAR *pStr) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IWIACategoryVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IWIACategory __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IWIACategory __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IWIACategory __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IWIACategory __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IWIACategory __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IWIACategory __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IWIACategory __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Name )( 
            IWIACategory __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *pVal);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_ElementCount )( 
            IWIACategory __RPC_FAR * This,
            /* [retval][out] */ long __RPC_FAR *pVal);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Element )( 
            IWIACategory __RPC_FAR * This,
            VARIANT index,
            /* [retval][out] */ IVisibleElement __RPC_FAR *__RPC_FAR *ppElement);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_OnSelect )( 
            IWIACategory __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *pStr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_OnExit )( 
            IWIACategory __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *pStr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Icon )( 
            IWIACategory __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *pStr);
        
        END_INTERFACE
    } IWIACategoryVtbl;

    interface IWIACategory
    {
        CONST_VTBL struct IWIACategoryVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWIACategory_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IWIACategory_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IWIACategory_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IWIACategory_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IWIACategory_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IWIACategory_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IWIACategory_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IWIACategory_get_Name(This,pVal)	\
    (This)->lpVtbl -> get_Name(This,pVal)

#define IWIACategory_get_ElementCount(This,pVal)	\
    (This)->lpVtbl -> get_ElementCount(This,pVal)

#define IWIACategory_Element(This,index,ppElement)	\
    (This)->lpVtbl -> Element(This,index,ppElement)

#define IWIACategory_get_OnSelect(This,pStr)	\
    (This)->lpVtbl -> get_OnSelect(This,pStr)

#define IWIACategory_get_OnExit(This,pStr)	\
    (This)->lpVtbl -> get_OnExit(This,pStr)

#define IWIACategory_get_Icon(This,pStr)	\
    (This)->lpVtbl -> get_Icon(This,pStr)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIACategory_get_Name_Proxy( 
    IWIACategory __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *pVal);


void __RPC_STUB IWIACategory_get_Name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIACategory_get_ElementCount_Proxy( 
    IWIACategory __RPC_FAR * This,
    /* [retval][out] */ long __RPC_FAR *pVal);


void __RPC_STUB IWIACategory_get_ElementCount_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IWIACategory_Element_Proxy( 
    IWIACategory __RPC_FAR * This,
    VARIANT index,
    /* [retval][out] */ IVisibleElement __RPC_FAR *__RPC_FAR *ppElement);


void __RPC_STUB IWIACategory_Element_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIACategory_get_OnSelect_Proxy( 
    IWIACategory __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *pStr);


void __RPC_STUB IWIACategory_get_OnSelect_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIACategory_get_OnExit_Proxy( 
    IWIACategory __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *pStr);


void __RPC_STUB IWIACategory_get_OnExit_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIACategory_get_Icon_Proxy( 
    IWIACategory __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *pStr);


void __RPC_STUB IWIACategory_get_Icon_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IWIACategory_INTERFACE_DEFINED__ */


#ifndef __IWIACategories_INTERFACE_DEFINED__
#define __IWIACategories_INTERFACE_DEFINED__

/* interface IWIACategories */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IWIACategories;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("91830070-8879-11D2-8067-00805F6596D2")
    IWIACategories : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_Count( 
            /* [retval][out] */ long __RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Item( 
            VARIANT index,
            /* [retval][out] */ IWIACategory __RPC_FAR *__RPC_FAR *ppCat) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IWIACategoriesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IWIACategories __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IWIACategories __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IWIACategories __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IWIACategories __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IWIACategories __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IWIACategories __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IWIACategories __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Count )( 
            IWIACategories __RPC_FAR * This,
            /* [retval][out] */ long __RPC_FAR *pVal);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Item )( 
            IWIACategories __RPC_FAR * This,
            VARIANT index,
            /* [retval][out] */ IWIACategory __RPC_FAR *__RPC_FAR *ppCat);
        
        END_INTERFACE
    } IWIACategoriesVtbl;

    interface IWIACategories
    {
        CONST_VTBL struct IWIACategoriesVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWIACategories_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IWIACategories_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IWIACategories_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IWIACategories_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IWIACategories_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IWIACategories_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IWIACategories_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IWIACategories_get_Count(This,pVal)	\
    (This)->lpVtbl -> get_Count(This,pVal)

#define IWIACategories_Item(This,index,ppCat)	\
    (This)->lpVtbl -> Item(This,index,ppCat)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIACategories_get_Count_Proxy( 
    IWIACategories __RPC_FAR * This,
    /* [retval][out] */ long __RPC_FAR *pVal);


void __RPC_STUB IWIACategories_get_Count_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IWIACategories_Item_Proxy( 
    IWIACategories __RPC_FAR * This,
    VARIANT index,
    /* [retval][out] */ IWIACategory __RPC_FAR *__RPC_FAR *ppCat);


void __RPC_STUB IWIACategories_Item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IWIACategories_INTERFACE_DEFINED__ */


#ifndef __IWIAVideoUI_INTERFACE_DEFINED__
#define __IWIAVideoUI_INTERFACE_DEFINED__

/* interface IWIAVideoUI */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IWIAVideoUI;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("A94D2BA2-7BDB-4e97-89CD-51DF3E967EF1")
    IWIAVideoUI : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetUICategories( 
            BSTR ItemName,
            /* [retval][out] */ IWIACategories __RPC_FAR *__RPC_FAR *ppCats) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE InitDeviceUI( 
            BSTR strFolderPath) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SelectionChanged( 
            IUnknown __RPC_FAR *pSelection) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_Device( 
            /* [retval][out] */ IUnknown __RPC_FAR *__RPC_FAR *pVal) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_TryTakePicture( 
            /* [in] */ long bTry) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IWIAVideoUIVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IWIAVideoUI __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IWIAVideoUI __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetUICategories )( 
            IWIAVideoUI __RPC_FAR * This,
            BSTR ItemName,
            /* [retval][out] */ IWIACategories __RPC_FAR *__RPC_FAR *ppCats);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *InitDeviceUI )( 
            IWIAVideoUI __RPC_FAR * This,
            BSTR strFolderPath);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SelectionChanged )( 
            IWIAVideoUI __RPC_FAR * This,
            IUnknown __RPC_FAR *pSelection);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Device )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [retval][out] */ IUnknown __RPC_FAR *__RPC_FAR *pVal);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_TryTakePicture )( 
            IWIAVideoUI __RPC_FAR * This,
            /* [in] */ long bTry);
        
        END_INTERFACE
    } IWIAVideoUIVtbl;

    interface IWIAVideoUI
    {
        CONST_VTBL struct IWIAVideoUIVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWIAVideoUI_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IWIAVideoUI_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IWIAVideoUI_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IWIAVideoUI_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IWIAVideoUI_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IWIAVideoUI_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IWIAVideoUI_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IWIAVideoUI_GetUICategories(This,ItemName,ppCats)	\
    (This)->lpVtbl -> GetUICategories(This,ItemName,ppCats)

#define IWIAVideoUI_InitDeviceUI(This,strFolderPath)	\
    (This)->lpVtbl -> InitDeviceUI(This,strFolderPath)

#define IWIAVideoUI_SelectionChanged(This,pSelection)	\
    (This)->lpVtbl -> SelectionChanged(This,pSelection)

#define IWIAVideoUI_get_Device(This,pVal)	\
    (This)->lpVtbl -> get_Device(This,pVal)

#define IWIAVideoUI_put_TryTakePicture(This,bTry)	\
    (This)->lpVtbl -> put_TryTakePicture(This,bTry)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IWIAVideoUI_GetUICategories_Proxy( 
    IWIAVideoUI __RPC_FAR * This,
    BSTR ItemName,
    /* [retval][out] */ IWIACategories __RPC_FAR *__RPC_FAR *ppCats);


void __RPC_STUB IWIAVideoUI_GetUICategories_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IWIAVideoUI_InitDeviceUI_Proxy( 
    IWIAVideoUI __RPC_FAR * This,
    BSTR strFolderPath);


void __RPC_STUB IWIAVideoUI_InitDeviceUI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IWIAVideoUI_SelectionChanged_Proxy( 
    IWIAVideoUI __RPC_FAR * This,
    IUnknown __RPC_FAR *pSelection);


void __RPC_STUB IWIAVideoUI_SelectionChanged_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IWIAVideoUI_get_Device_Proxy( 
    IWIAVideoUI __RPC_FAR * This,
    /* [retval][out] */ IUnknown __RPC_FAR *__RPC_FAR *pVal);


void __RPC_STUB IWIAVideoUI_get_Device_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IWIAVideoUI_put_TryTakePicture_Proxy( 
    IWIAVideoUI __RPC_FAR * This,
    /* [in] */ long bTry);


void __RPC_STUB IWIAVideoUI_put_TryTakePicture_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IWIAVideoUI_INTERFACE_DEFINED__ */


#ifndef __IVideoPreview_INTERFACE_DEFINED__
#define __IVideoPreview_INTERFACE_DEFINED__

/* interface IVideoPreview */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IVideoPreview;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3FB2A423-C145-43DB-BFE0-DE4766BADB42")
    IVideoPreview : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Device( 
            /* [in] */ IUnknown __RPC_FAR *pDevice) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_TryTakePicture( 
            /* [retval][out] */ long __RPC_FAR *bTry) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVideoPreviewVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IVideoPreview __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IVideoPreview __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IVideoPreview __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IVideoPreview __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IVideoPreview __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IVideoPreview __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IVideoPreview __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Device )( 
            IVideoPreview __RPC_FAR * This,
            /* [in] */ IUnknown __RPC_FAR *pDevice);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_TryTakePicture )( 
            IVideoPreview __RPC_FAR * This,
            /* [retval][out] */ long __RPC_FAR *bTry);
        
        END_INTERFACE
    } IVideoPreviewVtbl;

    interface IVideoPreview
    {
        CONST_VTBL struct IVideoPreviewVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVideoPreview_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVideoPreview_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVideoPreview_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVideoPreview_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVideoPreview_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVideoPreview_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVideoPreview_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVideoPreview_Device(This,pDevice)	\
    (This)->lpVtbl -> Device(This,pDevice)

#define IVideoPreview_get_TryTakePicture(This,bTry)	\
    (This)->lpVtbl -> get_TryTakePicture(This,bTry)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVideoPreview_Device_Proxy( 
    IVideoPreview __RPC_FAR * This,
    /* [in] */ IUnknown __RPC_FAR *pDevice);


void __RPC_STUB IVideoPreview_Device_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVideoPreview_get_TryTakePicture_Proxy( 
    IVideoPreview __RPC_FAR * This,
    /* [retval][out] */ long __RPC_FAR *bTry);


void __RPC_STUB IVideoPreview_get_TryTakePicture_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVideoPreview_INTERFACE_DEFINED__ */



#ifndef __WIAVIEWLib_LIBRARY_DEFINED__
#define __WIAVIEWLib_LIBRARY_DEFINED__

/* library WIAVIEWLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_WIAVIEWLib;

EXTERN_C const CLSID CLSID_WIACamUI;

#ifdef __cplusplus

class DECLSPEC_UUID("42B36D70-8877-11D2-8067-00805F6596D2")
WIACamUI;
#endif

EXTERN_C const CLSID CLSID_Categories;

#ifdef __cplusplus

class DECLSPEC_UUID("91830071-8879-11D2-8067-00805F6596D2")
Categories;
#endif

EXTERN_C const CLSID CLSID_UIElement;

#ifdef __cplusplus

class DECLSPEC_UUID("5B68A672-887A-11D2-8067-00805F6596D2")
UIElement;
#endif

EXTERN_C const CLSID CLSID_Category;

#ifdef __cplusplus

class DECLSPEC_UUID("C1252E31-887A-11D2-8067-00805F6596D2")
Category;
#endif

EXTERN_C const CLSID CLSID_VideoPreview;

#ifdef __cplusplus

class DECLSPEC_UUID("457A23DF-6F2A-4684-91D0-317FB768D87C")
VideoPreview;
#endif
#endif /* __WIAVIEWLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long __RPC_FAR *, unsigned long            , BSTR __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserMarshal(  unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  BSTR_UserUnmarshal(unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, BSTR __RPC_FAR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long __RPC_FAR *, BSTR __RPC_FAR * ); 

unsigned long             __RPC_USER  VARIANT_UserSize(     unsigned long __RPC_FAR *, unsigned long            , VARIANT __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  VARIANT_UserMarshal(  unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, VARIANT __RPC_FAR * ); 
unsigned char __RPC_FAR * __RPC_USER  VARIANT_UserUnmarshal(unsigned long __RPC_FAR *, unsigned char __RPC_FAR *, VARIANT __RPC_FAR * ); 
void                      __RPC_USER  VARIANT_UserFree(     unsigned long __RPC_FAR *, VARIANT __RPC_FAR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif



