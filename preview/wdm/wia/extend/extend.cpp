////////////////////////////////////
// (C) COPYRIGHT MICROSOFT CORP., 1998-1999
//
// FILE: EXTEND.CPP
//
// DESCRIPTION: Implements core DLL routines as well as web view extensions.
//
#include "precomp.h"
#pragma hdrstop
#include <string.h>
#include <tchar.h>
#include "resource.h"

#include "extidl_i.c"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_ExtWIACamUI, CWebViewExt)
OBJECT_ENTRY(CLSID_TestShellExt,  CShellExt)
OBJECT_ENTRY(CLSID_TestUIExtension,  CWiaUIExtension)
END_OBJECT_MAP()

static CComBSTR          g_strCategory;

STDAPI DllRegisterServer(void)
{

    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}


STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer();
}


EXTERN_C
BOOL
DllMain(
    HINSTANCE   hinst,
    DWORD       dwReason,
    LPVOID      lpReserved)
{
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:

            _Module.Init (ObjectMap, hinst);
            DisableThreadLibraryCalls(hinst);

            break;

        case DLL_PROCESS_DETACH:
            _Module.Term();
            break;
    }
    return TRUE;
}


extern "C" STDMETHODIMP DllCanUnloadNow(void)
{
    return _Module.GetLockCount()==0 ? S_OK : S_FALSE;
}

extern "C" STDAPI DllGetClassObject(
    REFCLSID    rclsid,
    REFIID      riid,
    LPVOID      *ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);

}


/*****************************************************************************

ShowMessage

Utility function for displaying messageboxes

******************************************************************************/

BOOL ShowMessage (HWND hParent, INT idCaption, INT idMessage)
{
    MSGBOXPARAMS mbp;
    BOOL bRet;
    INT  i;

    ZeroMemory (&mbp, sizeof(mbp));
    mbp.cbSize = sizeof(mbp);
    mbp.hwndOwner = hParent;
    mbp.hInstance = g_hInst;
    mbp.lpszText = MAKEINTRESOURCE(idMessage);
    mbp.lpszCaption = MAKEINTRESOURCE(idCaption);
    mbp.dwStyle = MB_OK | MB_APPLMODAL;

    i = MessageBoxIndirect (&mbp);
    bRet = (IDOK==i);
    return bRet;
}

/*****************************************************************************

FindLastID

Utility for getting the last relative pidl from a full pidl

******************************************************************************/
// unsafe macros
#define _ILSkip(pidl, cb)       ((LPITEMIDLIST)(((BYTE*)(pidl))+cb))
#define ILNext(pidl)           _ILSkip(pidl, (pidl)->mkid.cb)

LPITEMIDLIST
FindLastID(LPCITEMIDLIST pidl)
{
    LPCITEMIDLIST pidlLast = pidl;
    LPCITEMIDLIST pidlNext = pidl;

    if (pidl == NULL)
        return NULL;

    // Find the last one
    while (pidlNext->mkid.cb)
    {
        pidlLast = pidlNext;
        pidlNext = ILNext(pidlLast);
    }

    return (LPITEMIDLIST)pidlLast;
}
/*****************************************************************************


CWebViewExt methods.

******************************************************************************/
#define ELEMENT_MAX 0

#define LOGO_IDX     0

static const WCHAR cszLogo[] = L"Logo";


/*****************************************************************************

CWebViewExt constructor



******************************************************************************/

CWebViewExt::CWebViewExt () : m_pSelection(NULL)
{
    TCHAR szName[MAX_PATH];
    LoadString (_Module.GetResourceInstance(), IDS_CATEGORYNAME,  szName, MAX_PATH);
    #ifdef UNICODE
    g_strCategory = szName;
    #else
    WCHAR szwName[MAX_PATH];
    MultiByteToWideChar (CP_ACP, 0, szName, -1, szwName, MAX_PATH);
    g_strCategory = szwName;
    #endif

}
/*****************************************************************************

CWebViewExt destructor

Free the current selection array and full pidl
******************************************************************************/

CWebViewExt::~CWebViewExt ()
{
    if (m_pSelection)
    {
        delete [] m_pSelection;
    }
    if (m_pidlFolder)
    {
        CComPtr<IMalloc> pMalloc;
        SHGetMalloc (&pMalloc);
        pMalloc->Free (m_pidlFolder);
    }

}


/*****************************************************************************

CWebViewExt::GetUICategories

Called by the WIA view script to enumerate our UI categories for the left-hand
panel in a web view for our WIA device.

******************************************************************************/

STDMETHODIMP
CWebViewExt::GetUICategories (BSTR ItemName, IWIAWebvwCategories **ppCats)
{

    // Regardless of the item name, we only have 1 category.
    CComObject<CExtCategories> *pCats;
    HRESULT hr = CComObject<CExtCategories>::CreateInstance (&pCats);
    if (SUCCEEDED(hr))
    {
        hr = pCats->QueryInterface (IID_IWIAWebvwCategories, reinterpret_cast<LPVOID*>(ppCats));
    }

    return hr;
}

/*****************************************************************************

CWebViewExt::InitDeviceUI

This is the first method called when we are loaded. strDeviceId is the WIA
device identifier, strFolder is the path to the current subfolder in that
device. If strFolder is NULL we are in the root. For the sample we ignore
strFolder because we show the same UI regardless of the folder.

******************************************************************************/

STDMETHODIMP
CWebViewExt::InitDeviceUI (BSTR strDeviceId, BSTR strFolder)
{
    HRESULT hr;



    CComPtr<IShellFolder> pDesktop;
    WCHAR szDeviceId[MAX_PATH];
    CComPtr<IShellFolder> pFolder;


    if (m_pSelection)
    {
        delete [] m_pSelection;
        m_pSelection = NULL;
    }

    hr = SHGetDesktopFolder (&pDesktop);

    if (SUCCEEDED(hr))
    {
        hr = CreateDeviceFromId (strDeviceId, &m_pDevice);
    }
    return hr;

}


/*****************************************************************************

CWebViewExt::SelectionChanged

Called when the user changes the items selected in the view

******************************************************************************/

STDMETHODIMP
CWebViewExt::SelectionChanged (IUnknown *pSelection)
{
    CComQIPtr<IDataObject, &IID_IDataObject> pdo(pSelection);
    UINT uItems;
    if (pdo)
    {
        m_pSelection = GetNamesFromDataObject (pdo, &uItems);
        return NOERROR;
    }

    return E_INVALIDARG;

}

/*****************************************************************************

CExtCategories::Item

Return an IWIAWebvwCategory interface for the requested category. We only have 1
category.

******************************************************************************/

STDMETHODIMP
CExtCategories::Item (VARIANT index, IWIAWebvwCategory **ppCat)
{
    HRESULT hr;
    CComObject<CExtCategory> *pCat;
    switch (index.vt)
    {
        case VT_I4:
            if (index.lVal > 0)
            {
                return DISP_E_BADINDEX;
            }
            break;

        case VT_BSTR:
            if (_wcsicmp (index.bstrVal, g_strCategory)  )
            {
                return DISP_E_BADINDEX;
            }
            break;

        default:
            return DISP_E_BADVARTYPE;
            break;
    }
    hr =  CComObject<CExtCategory>::CreateInstance (&pCat);
    if (SUCCEEDED(hr))
    {
        hr = pCat->QueryInterface (IID_IWIAWebvwCategory,
                                   reinterpret_cast<LPVOID*>(ppCat));


    }
    return hr;
}


/*****************************************************************************

CExtCategories::get_Count

Return number of categories we support

******************************************************************************/

STDMETHODIMP
CExtCategories::get_Count (LONG *plVal)
{
    *plVal = 1;
    return NOERROR;
}

/*****************************************************************************

CExtCategory::Element

Return the requested IVisibleElement from our category

******************************************************************************/
struct
{
    LPCWSTR strName;
    LONG   lClass;
}
ElemData [] =
{
    {cszLogo, LOGO_IDX}
};

STDMETHODIMP
CExtCategory::Element (VARIANT index, IWIAWebvwElement **ppElement)
{
    LONG lClass = -1;
    INT i;
    *ppElement = NULL;
    HRESULT hr;
    switch (index.vt)
    {
        case VT_I4:
            if (index.lVal > ELEMENT_MAX)
            {
                return DISP_E_BADINDEX;
            }
            lClass = index.lVal;
            break;

        case VT_BSTR:
            for (i=0;i<ELEMENT_MAX;i++)
            {
                if (!_wcsicmp (index.bstrVal, ElemData[i].strName))
                {
                    lClass = index.lVal;
                    i = ELEMENT_MAX;
                }
            }
            if (-1 == lClass)
            {
                return DISP_E_BADINDEX;
            }
            break;

        default:
            return DISP_E_BADVARTYPE;

    }
    switch (lClass)
    {
        case LOGO_IDX:
        {

            CComObject<CLogoElement> *pLogo;
            hr = CComObject<CLogoElement>::CreateInstance(&pLogo);
            if (SUCCEEDED(hr))
            {
                hr = pLogo->QueryInterface (IID_IWIAWebvwElement,
                                            reinterpret_cast<LPVOID*>(ppElement));

            }
        }
            break;

        default:
            // shouldn't get here
            hr =  E_FAIL;
            break;
    }
    return hr;
}


/*****************************************************************************

CExtCategory::get_ElementCount

Return number of UI elements in our category

******************************************************************************/

STDMETHODIMP
CExtCategory::get_ElementCount (LONG *plVal)
{
    *plVal = ELEMENT_MAX+1;
    return NOERROR;
}

/*****************************************************************************

CExtCategory::get_Name

Return the name of this category

******************************************************************************/

STDMETHODIMP
CExtCategory::get_Name (BSTR *pName)
{
    *pName = SysAllocString (g_strCategory);
    if (!*pName)
    {
        return E_OUTOFMEMORY;
    }
    return NOERROR;
}

/*****************************************************************************

CExtCategory::get_OnSelect

Return a script to run when our category is chosen.

******************************************************************************/

STDMETHODIMP
CExtCategory::get_OnSelect (BSTR *pSelect)
{
    *pSelect = NULL;
    return S_OK;
}

/*****************************************************************************

CExtCategory::get_OnExit

Return a script to run when the user removes focus from our category to another
category.

******************************************************************************/

STDMETHODIMP
CExtCategory::get_OnExit (BSTR *pExit)
{
    *pExit = NULL;
    return S_OK;
}


STDMETHODIMP
CExtCategory::get_Icon (BSTR *pStr)
{
    *pStr = SysAllocString (L"testcam.ico");
    if (*pStr)
    {
        return S_OK;
    }
    return E_OUTOFMEMORY;
}
/*****************************************************************************

CLogoElement element

This is a simple HTML element that displays a bitmap.

******************************************************************************/
static const WCHAR cszLogoHTML [] = L"<img src='TCamLogo.jpg'>";

/*****************************************************************************

CLogoElement::Clicked

Called when a user clicks a string in this element.

******************************************************************************/
STDMETHODIMP
CLogoElement::Clicked (BSTR string, BOOL bOn)
{
    return NOERROR;
}


/*****************************************************************************

CLogoElement::String

Return our text

******************************************************************************/

STDMETHODIMP
CLogoElement::String (VARIANT index, BSTR *pStr)
{
    if (index.vt != VT_I4)
    {
        return DISP_E_BADVARTYPE;
    }
    else if (index.lVal != 0)
    {
        return DISP_E_BADINDEX;
    }
    *pStr = SysAllocString (cszLogoHTML);
    if (!*pStr)
    {
        return E_OUTOFMEMORY;
    }
    return NOERROR;
}

/*****************************************************************************

CLogoElement::get_StringCount

Return the number of strings in this element. Only listbox-type elements have
more than 1 string

******************************************************************************/

STDMETHODIMP
CLogoElement::get_StringCount (LONG *plVal)
{
    *plVal = 1;
    return NOERROR;
}

/*****************************************************************************

CLogoElement::get_Type

Return the type of element. Valid types are "html", "static", "button", "menuitem",
    and "listbox"

******************************************************************************/

STDMETHODIMP
CLogoElement::get_Type (BSTR *pStr)
{
    *pStr = SysAllocString (L"html");
    if (!*pStr)
    {
        return E_OUTOFMEMORY;
    }
    return NOERROR;
}

/*****************************************************************************

CLogoElement::get_Name

Return the language-independent name of this element

******************************************************************************/

STDMETHODIMP
CLogoElement::get_Name (BSTR *pStr)
{
    *pStr = SysAllocString (cszLogo);
    if (!*pStr)
    {
        return E_OUTOFMEMORY;
    }
    return NOERROR;
}

/*****************************************************************************

CreateDeviceFromID

Utility for attaching to WIA and getting a root IWiaItem interface

*****************************************************************************/
HRESULT
CreateDeviceFromId (LPWSTR szDeviceId, IWiaItem **ppItem)
{
    IWiaDevMgr *pDevMgr;
    HRESULT hr = CoCreateInstance (CLSID_WiaDevMgr,
                                   NULL,
                                   CLSCTX_LOCAL_SERVER,
                                   IID_IWiaDevMgr,
                                   reinterpret_cast<LPVOID*>(&pDevMgr));
    if (SUCCEEDED(hr))
    {
        BSTR strId = SysAllocString (szDeviceId);
        hr = pDevMgr->CreateDevice (strId, ppItem);
        SysFreeString (strId);
        pDevMgr->Release ();
    }
    return hr;
}

/*****************************************************************************\

    GetNamesFromDataObject

    Return the list of selected item identifiers. Each identifier is of the form
    "<DEVICEID>::<FULL PATH NAME>". the list is double-null terminated

*****************************************************************************/

LPWSTR
GetNamesFromDataObject (IDataObject *lpdobj, UINT *puItems)
{
    FORMATETC fmt;
    STGMEDIUM stg;
    LPWSTR szRet = NULL;
    LPWSTR szCurrent;
    UINT nItems;
    size_t size;
    if (puItems)
    {
        *puItems = 0;
    }

    fmt.cfFormat = (CLIPFORMAT) RegisterClipboardFormat (TEXT("WIAItemNames"));
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.ptd = NULL;
    fmt.tymed = TYMED_HGLOBAL;

    if (lpdobj && puItems && SUCCEEDED(lpdobj->GetData (&fmt, &stg)))
    {
        szCurrent = reinterpret_cast<LPWSTR>(GlobalLock (stg.hGlobal));

        // count the number of items in the double-null terminated string
        szRet  = szCurrent;
        nItems = 0;
        while (*szRet)
        {
            nItems++;
            while (*szRet)
            {
                szRet++;
            }
            szRet++;
        }
        *puItems = nItems;
        size = (szRet-szCurrent+1)*sizeof(WCHAR);
        szRet = new WCHAR[size];
        CopyMemory (szRet, szCurrent, size);
        GlobalUnlock (stg.hGlobal);
        GlobalFree (stg.hGlobal);
    }
    return szRet;
}

CWiaUIExtension::CWiaUIExtension(void)
{
}

CWiaUIExtension::~CWiaUIExtension(void)
{
}

//
// IWiaUIExtension
//
STDMETHODIMP CWiaUIExtension::DeviceDialog( PDEVICEDIALOGDATA pDeviceDialogData )
{
    //
    // We are not going to implement an actual device dialog here.  Just say "hi" and return.
    //
    MessageBox( NULL, TEXT("CWiaUIExtension::DeviceDialog is being called"), TEXT("DEBUG"), 0 );
    return E_NOTIMPL;
}

STDMETHODIMP CWiaUIExtension::GetDeviceIcon( BSTR bstrDeviceId, HICON *phIcon, ULONG nSize )
{
    //
    // Load an icon, and copy it, using CopyIcon, so it will still be valid if our interface is freed
    //
    HICON hIcon = reinterpret_cast<HICON>(LoadImage( _Module.m_hInst, MAKEINTRESOURCE(IDI_TESTDEVICE), IMAGE_ICON, nSize, nSize, LR_DEFAULTCOLOR ));
    if (hIcon)
    {
        *phIcon = CopyIcon(hIcon);
        DestroyIcon(hIcon);
        return S_OK;
    }
    return E_NOTIMPL;
}

STDMETHODIMP CWiaUIExtension::GetDeviceBitmapLogo( BSTR bstrDeviceId, HBITMAP *phBitmap, ULONG nMaxWidth, ULONG nMaxHeight )
{
    //
    // This method is never actually called currently.  It is only here for future use.
    //
    MessageBox( NULL, TEXT("CWiaUIExtension::GetDeviceBitmapLogo is being called"), TEXT("DEBUG"), 0 );
    return E_NOTIMPL;
}


