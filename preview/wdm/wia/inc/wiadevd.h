/*******************************************************************************
 *
 *  (C) COPYRIGHT MICROSOFT CORPORATION, 1998
 *
 *  TITLE:       WIADEVD.H
 *
 *  VERSION:     1.0
 *
 *  AUTHOR:      ShaunIv
 *
 *  DATE:        7/15/1999
 *
 *  DESCRIPTION: Device Dialog and UI extensibility declarations
 *
 *******************************************************************************/
#ifndef _WIADEVD_H_INCLUDED
#define _WIADEVD_H_INCLUDED

#include "wia.h"

#if defined(__cplusplus)
extern "C" {
#endif

#include <pshpack8.h>

typedef struct tagDEVICEDIALOGDATA
{
    DWORD            cbSize;           // Size of the structure in bytes
    HWND             hwndParent;       // Parent window
    IWiaItem         *pIWiaItemRoot;   // Valid root item
    DWORD            dwFlags;          // Flags
    LONG             lIntent;          // Intent flags
    LONG             lItemCount;       // Number of items in ppWiaItems array.  Filled on return.
    IWiaItem         **ppWiaItems;     // Array of IWiaItem interface pointers.  Array must
                                       // be allocated using LocalAlloc, all interface pointers must be AddRef'ed
} DEVICEDIALOGDATA, *LPDEVICEDIALOGDATA, *PDEVICEDIALOGDATA;

HRESULT WINAPI DeviceDialog( PDEVICEDIALOGDATA pDeviceDialogData );

// IWiaUIExtension provides a means to replace a device's image acquisition dialog
// and to provide custom icons and logo bitmaps to appear on the standard dialog
#undef  INTERFACE
#define INTERFACE IWiaUIExtension
DECLARE_INTERFACE_(IWiaUIExtension, IUnknown)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS) PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;

    // *** IWiaUIExtension methods ***
    STDMETHOD(DeviceDialog)( THIS_ PDEVICEDIALOGDATA pDeviceDialogData ) PURE;
    STDMETHOD(GetDeviceIcon)(THIS_ BSTR bstrDeviceId, HICON *phIcon, ULONG nSize ) PURE;
    STDMETHOD(GetDeviceBitmapLogo)(THIS_ BSTR bstrDeviceId, HBITMAP *phBitmap, ULONG nMaxWidth, ULONG nMaxHeight ) PURE;
};

// {da319113-50ee-4c80-b460-57d005d44a2c}
DEFINE_GUID(IID_IWiaUIExtension, 0xDA319113, 0x50EE, 0x4C80, 0xB4, 0x60, 0x57, 0xD0, 0x05, 0xD4, 0x4A, 0x2C);

typedef HRESULT (WINAPI *DeviceDialogFunction)(PDEVICEDIALOGDATA);

#define SHELLEX_WIAUIEXTENSION_NAME TEXT("WiaDialogExtensionHandlers")

// IWIAWebvwUI is implemented by web view extensions for camera devices. It lets
// them add a category of UI to appear in the web view panel. IWIAWebvwCategories,
// IWIAWebvwCategory, and IWIAWebvwElement round out this extensibility mechanism
#undef  INTERFACE
#define INTERFACE IWIAWebvwElement

DECLARE_INTERFACE_(IWIAWebvwElement, IUnknown)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS) PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;

    // ** IWIAWebvwElement methods ***
    STDMETHOD(get_Name) (THIS_ BSTR *pVal) PURE;
    STDMETHOD(get_Type)(THIS_ BSTR *pVal) PURE;
    STDMETHOD(get_StringCount)(THIS_ long *pVal) PURE;
    STDMETHOD(String)(THIS_ VARIANT index, BSTR *pStr) PURE;
    STDMETHOD(Clicked)(THIS_ BSTR string, BOOL bOn) PURE;
};

#undef  INTERFACE
#define INTERFACE IWIAWebvwCategory

DECLARE_INTERFACE_(IWIAWebvwCategory, IUnknown)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS) PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;

    // ** IWIAWebvwCategory methods ***
    STDMETHOD(get_Name)(THIS_ BSTR *pVal) PURE;
    STDMETHOD(get_ElementCount)(THIS_ long *pVal) PURE;
    STDMETHOD(Element)(THIS_ VARIANT index, IWIAWebvwElement **ppElement) PURE;
    STDMETHOD(get_OnSelect)(THIS_ BSTR *pStr) PURE;
    STDMETHOD(get_OnExit)(THIS_ BSTR *pStr) PURE;
    STDMETHOD(get_Icon)(THIS_ BSTR *pStr) PURE;
};

#undef  INTERFACE
#define INTERFACE IWIAWebvwCategories

DECLARE_INTERFACE_(IWIAWebvwCategories, IUnknown)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS) PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;

    // ** IWIAWebvwCategories methods ***
    STDMETHOD(get_Count)(THIS_ long *pVal) PURE;
    STDMETHOD(Item)(THIS_ VARIANT index, IWIAWebvwCategory **ppCat) PURE;
};

#undef  INTERFACE
#define INTERFACE IWIAWebvwUI
DECLARE_INTERFACE_(IWIAWebvwUI, IUnknown)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS) PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;


    // ** IWIAWebvwUI methods
    STDMETHOD(GetUICategories)(THIS_ BSTR bstrItemName, IWIAWebvwCategories **ppCats) PURE;
    STDMETHOD(InitDeviceUI)(THIS_ BSTR bstrDeviceId, BSTR bstrFolder) PURE;
    STDMETHOD(SelectionChanged)(THIS_ IUnknown *pSelection) PURE;
};

// Define a clipboard format name for retrieving data from an IDataObject
#define CFSTR_WIAITEMNAMES TEXT("WIAItemNames")

// Define IIDs for the web view interfaces

// {80C4D029-55BA-447f-A503-0A3593E765DB}
DEFINE_GUID(IID_IWIAWebvwUI, 0x80c4d029, 0x55ba, 0x447f, 0xa5, 0x3, 0xa, 0x35, 0x93, 0xe7, 0x65, 0xdb);
// {89A05184-F355-4cc5-8C64-3814147B1BAB}
DEFINE_GUID(IID_IWIAWebvwElement, 0x89a05184, 0xf355, 0x4cc5, 0x8c, 0x64, 0x38, 0x14, 0x14, 0x7b, 0x1b, 0xab);
// {9076892F-8BD5-4f59-A35E-B0D69A9D0EF0}
DEFINE_GUID(IID_IWIAWebvwCategories, 0x9076892f, 0x8bd5, 0x4f59, 0xa3, 0x5e, 0xb0, 0xd6, 0x9a, 0x9d, 0xe, 0xf0);
// {5CB5F77C-FAFA-4379-AC93-80224D4C7118}
DEFINE_GUID(IID_IWIAWebvwCategory, 0x5cb5f77c, 0xfafa, 0x4379, 0xac, 0x93, 0x80, 0x22, 0x4d, 0x4c, 0x71, 0x18);


#include <poppack.h>

#if defined(__cplusplus)
};
#endif

#endif // !_WIADEVD_H_INCLUDED


