
#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0323 */
/* Compiler settings for extidl.idl:
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


#ifndef __extidl_h__
#define __extidl_h__

/* Forward Declarations */ 

#ifndef __WebViewExt_FWD_DEFINED__
#define __WebViewExt_FWD_DEFINED__

#ifdef __cplusplus
typedef class WebViewExt WebViewExt;
#else
typedef struct WebViewExt WebViewExt;
#endif /* __cplusplus */

#endif 	/* __WebViewExt_FWD_DEFINED__ */


#ifndef __ShellExt_FWD_DEFINED__
#define __ShellExt_FWD_DEFINED__

#ifdef __cplusplus
typedef class ShellExt ShellExt;
#else
typedef struct ShellExt ShellExt;
#endif /* __cplusplus */

#endif 	/* __ShellExt_FWD_DEFINED__ */


#ifndef __WiaUIExtension_FWD_DEFINED__
#define __WiaUIExtension_FWD_DEFINED__

#ifdef __cplusplus
typedef class WiaUIExtension WiaUIExtension;
#else
typedef struct WiaUIExtension WiaUIExtension;
#endif /* __cplusplus */

#endif 	/* __WiaUIExtension_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 


#ifndef __WIASampleExtLib_LIBRARY_DEFINED__
#define __WIASampleExtLib_LIBRARY_DEFINED__

/* library WIASampleExtLib */
/* [uuid] */ 


EXTERN_C const IID LIBID_WIASampleExtLib;

EXTERN_C const CLSID CLSID_WebViewExt;

#ifdef __cplusplus

class DECLSPEC_UUID("a7f9264a-08bb-11d3-94b1-00805f6596d2")
WebViewExt;
#endif

EXTERN_C const CLSID CLSID_ShellExt;

#ifdef __cplusplus

class DECLSPEC_UUID("b6c280f7-0f07-11d3-94c7-00805f6596d2")
ShellExt;
#endif

EXTERN_C const CLSID CLSID_WiaUIExtension;

#ifdef __cplusplus

class DECLSPEC_UUID("edb8b35d-c15f-4e45-9658-50d7f8addb56")
WiaUIExtension;
#endif
#endif /* __WIASampleExtLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif



