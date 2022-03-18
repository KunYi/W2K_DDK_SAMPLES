//(C) COPYRIGHT MICROSOFT CORP., 1998-1999

#ifndef _CLASSES_H_
#define _CLASSES_H_


/*****************************************************************************
class CShellExt

Implement our regular shell extensions.


******************************************************************************/

class ATL_NO_VTABLE CShellExt :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CShellExt, &CLSID_TestShellExt>,
    public IShellExtInit, public IContextMenu, public IShellPropSheetExt
{
    private:
        UINT_PTR m_idCmd;
        CComPtr<IWiaItem> m_pItem;

        static INT_PTR CALLBACK PropPageProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
        HRESULT GetNewRootPath (HWND hwnd);

    public:
    BEGIN_COM_MAP(CShellExt)
        COM_INTERFACE_ENTRY(IShellExtInit)
        COM_INTERFACE_ENTRY(IContextMenu)
        COM_INTERFACE_ENTRY(IShellPropSheetExt)
    END_COM_MAP()
        DECLARE_NO_REGISTRY()

        // IShellExtInit
        STDMETHODIMP Initialize (LPCITEMIDLIST pidlFolder,LPDATAOBJECT lpdobj,HKEY hkeyProgID);

        // IShellPropSheetExt
        STDMETHODIMP AddPages (LPFNADDPROPSHEETPAGE lpfnAddPage,LPARAM lParam);
        STDMETHODIMP ReplacePage (UINT uPageID,LPFNADDPROPSHEETPAGE lpfnReplacePage,LPARAM lParam) {return E_NOTIMPL;};

        // IContextMenu
        STDMETHODIMP QueryContextMenu (HMENU hmenu,UINT indexMenu,UINT idCmdFirst,UINT idCmdLast,UINT uFlags);
        STDMETHODIMP InvokeCommand    (LPCMINVOKECOMMANDINFO lpici);
        STDMETHODIMP GetCommandString (UINT_PTR idCmd, UINT uType,UINT* pwReserved,LPSTR pszName,UINT cchMax);
        ~CShellExt ();
        CShellExt ();
};

class ATL_NO_VTABLE CLogoElement :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IWIAWebvwElement
{
    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CLogoElement)
        COM_INTERFACE_ENTRY(IWIAWebvwElement)
    END_COM_MAP()
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    public:
    //IWIAWebvwElement
        STDMETHODIMP Clicked(BSTR string, BOOL bOn);
        STDMETHODIMP String(VARIANT index, /*out, retval*/BSTR *pStr);
        STDMETHODIMP get_StringCount(/*[out, retval]*/ long *pVal);
        STDMETHODIMP get_innerHTML(/*[out, retval]*/ BSTR *pVal);
        STDMETHODIMP get_Type(/*[out, retval]*/ BSTR *pVal);
        STDMETHODIMP get_Name(/*[out, retval]*/ BSTR *pVal);
};

class ATL_NO_VTABLE CExtCategories :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IWIAWebvwCategories
{

    BEGIN_COM_MAP(CExtCategories)
        COM_INTERFACE_ENTRY(IWIAWebvwCategories)
    END_COM_MAP()

    public:
        DECLARE_PROTECT_FINAL_CONSTRUCT()
        DECLARE_NO_REGISTRY()

        //IWIACategories
        STDMETHODIMP Item(VARIANT index, IWIAWebvwCategory **ppCat);
        STDMETHODIMP get_Count(/*[out, retval]*/ long *pVal);

};

class ATL_NO_VTABLE CExtCategory :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IWIAWebvwCategory
{

    BEGIN_COM_MAP(CExtCategory)
        COM_INTERFACE_ENTRY(IWIAWebvwCategory)
    END_COM_MAP()

    public:
        DECLARE_PROTECT_FINAL_CONSTRUCT()
        DECLARE_NO_REGISTRY()
        //IWIACategory
        STDMETHODIMP Element(VARIANT index, IWIAWebvwElement **ppElement);
        STDMETHODIMP get_ElementCount(/*[out, retval]*/ long *pVal);
        STDMETHODIMP get_Name(/*[out, retval]*/ BSTR *pVal);
        STDMETHODIMP get_OnSelect (/*[out, retval]*/BSTR *pStr);
        STDMETHODIMP get_OnExit (/*[out, retval]*/BSTR *pStr);
        STDMETHODIMP get_Icon (/*[out, retval*/ BSTR *pStr);
};




class ATL_NO_VTABLE CWebViewExt :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CWebViewExt, &CLSID_ExtWIACamUI>,
    public IWIAWebvwUI
{
    public:

        CWebViewExt ();
        ~CWebViewExt ();

        DECLARE_REGISTRY_RESOURCEID(IDR_VIEWREG)
        DECLARE_PROTECT_FINAL_CONSTRUCT()
        BEGIN_COM_MAP(CWebViewExt)
            COM_INTERFACE_ENTRY(IWIAWebvwUI)
        END_COM_MAP()

        //IWIADeviceUI
        STDMETHODIMP GetUICategories (BSTR ItemName, IWIAWebvwCategories **ppCats);
        STDMETHODIMP InitDeviceUI (BSTR strDeviceId, BSTR strFolderPath);
        STDMETHODIMP SelectionChanged (IUnknown *pSelection);

    private:

        CComPtr<IWiaItem> m_pDevice;    // root item pointer
        LPWSTR             m_pSelection; // currently selected items in the view
        LPITEMIDLIST      m_pidlFolder; // idlist of the open folder


};

class ATL_NO_VTABLE CWiaUIExtension :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CWiaUIExtension, &CLSID_TestUIExtension>,
    public IWiaUIExtension
{
    public:

        CWiaUIExtension ();
        ~CWiaUIExtension ();

        DECLARE_REGISTRY_RESOURCEID(IDR_VIEWREG)
        DECLARE_PROTECT_FINAL_CONSTRUCT()
        BEGIN_COM_MAP(CWiaUIExtension)
            COM_INTERFACE_ENTRY(IWiaUIExtension)
        END_COM_MAP()

        //
        // IWiaUIExtension
        //
        STDMETHODIMP DeviceDialog( PDEVICEDIALOGDATA pDeviceDialogData );
        STDMETHODIMP GetDeviceIcon( BSTR bstrDeviceId, HICON *phIcon, ULONG nSize );
        STDMETHODIMP GetDeviceBitmapLogo( BSTR bstrDeviceId, HBITMAP *phBitmap, ULONG nMaxWidth, ULONG nMaxHeight );
};


#endif

