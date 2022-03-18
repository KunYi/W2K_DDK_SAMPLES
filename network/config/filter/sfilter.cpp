//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1997.
//
//  File:       S F I L T E R . C P P
//
//  Contents:   Notify object code for the sample filter.
//
//  Notes:
//
//----------------------------------------------------------------------------

#include "pch.h"
#pragma hdrstop
#include "sfilter.h"


// =================================================================
// Forward declarations

LRESULT CALLBACK SampleFilterDialogProc(HWND hWnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);
UINT CALLBACK SampleFilterPropSheetPageProc(HWND hWnd, UINT uMsg,
                                            LPPROPSHEETPAGE ppsp);
HRESULT HrOpenAdapterParamsKey(GUID* pguidAdapter,
                               HKEY* phkeyAdapter);
// void SetUnicodeString (IN OUT UNICODE_STRING*  pustr,
//                        IN     PCWSTR          psz);
inline ULONG ReleaseObj(IUnknown* punk)
{
    return (punk) ? punk->Release () : 0;
}

#if DBG
void TraceMsg(PCWSTR szFormat, ...);
#else
#define TraceMsg   (void)0
#endif

// =================================================================
// string constants
//
const WCHAR c_szBundleId[]        = L"BundleId";
const WCHAR c_szBundleIdDefault[] = L"<no-bundle>";
const WCHAR c_szParam1[]          = L"Param1";
const WCHAR c_szSFilterParams[]   = L"System\\CurrentControlSet\\Services\\SFilter\\Parameters";
const WCHAR c_szSFilterId[]       = L"MS_SFilter";
const WCHAR c_szSFilterNdisName[] = L"SFilter";

// =================================================================


//+---------------------------------------------------------------------------
//
// Function:  CSampleFilterParams::CSampleFilterParams
//
// Purpose:   constructor for class CSampleFilterParams
//
// Arguments: None
//
// Returns:
//
// Notes:
//
CSampleFilterParams::CSampleFilterParams(VOID)
{
    m_szParam1[0]      = '\0';
    m_szBundleId[0]    = '\0';
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::CSampleFilter
//
// Purpose:   constructor for class CSampleFilter
//
// Arguments: None
//
// Returns:   None
//
// Notes:
//
CSampleFilter::CSampleFilter(VOID) :
        m_pncc(NULL),
        m_pnc(NULL),
        m_eApplyAction(eActUnknown),
        m_pUnkContext(NULL)
{
    TraceMsg(L"--> CSampleFilter::CSampleFilter\n");

    m_cAdaptersAdded   = 0;
    m_cAdaptersRemoved = 0;
    m_fConfigRead      = FALSE;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::~CSampleFilter
//
// Purpose:   destructor for class CSampleFilter
//
// Arguments: None
//
// Returns:   None
//
// Notes:
//
CSampleFilter::~CSampleFilter(VOID)
{
    TraceMsg(L"--> CSampleFilter::~CSampleFilter\n");

    // release interfaces if acquired

    ReleaseObj(m_pncc);
    ReleaseObj(m_pnc);
    ReleaseObj(m_pUnkContext);
}

// =================================================================
// INetCfgNotify
//
// The following functions provide the INetCfgNotify interface
// =================================================================


// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::Initialize
//
// Purpose:   Initialize the notify object
//
// Arguments:
//    pnccItem    [in]  pointer to INetCfgComponent object
//    pnc         [in]  pointer to INetCfg object
//    fInstalling [in]  TRUE if we are being installed
//
// Returns:
//
// Notes:
//
STDMETHODIMP CSampleFilter::Initialize(INetCfgComponent* pnccItem,
        INetCfg* pnc, BOOL fInstalling)
{
    TraceMsg(L"--> CSampleFilter::Initialize\n");

    // save INetCfg & INetCfgComponent and add refcount

    m_pncc = pnccItem;
    m_pnc = pnc;

    if (m_pncc)
    {
        m_pncc->AddRef();
    }
    if (m_pnc)
    {
        m_pnc->AddRef();
    }

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::ReadAnswerFile
//
// Purpose:   Read settings from answerfile and configure SampleFilter
//
// Arguments:
//    pszAnswerFile    [in]  name of AnswerFile
//    pszAnswerSection [in]  name of parameters section
//
// Returns:
//
// Notes:     Dont do anything irreversible (like modifying registry) yet
//            since the config. actually complete only when Apply is called!
//
STDMETHODIMP CSampleFilter::ReadAnswerFile(PCWSTR pszAnswerFile,
        PCWSTR pszAnswerSection)
{
    TraceMsg(L"--> CSampleFilter::ReadAnswerFile\n");

    PCWSTR pszParamReadFromAnswerFile = L"ParamFromAnswerFile";

    // We will pretend here that szParamReadFromAnswerFile was actually
    // read from the AnswerFile using the following steps
    //
    //   - Open file pszAnswerFile using SetupAPI
    //   - locate section pszAnswerSection
    //   - locate the required key and get its value
    //   - store its value in pszParamReadFromAnswerFile
    //   - close HINF for pszAnswerFile

    // Now that we have read pszParamReadFromAnswerFile from the
    // AnswerFile, store it in our memory structure.
    // Remember we should not be writing it to the registry till
    // our Apply is called!!
    //
    wcscpy(m_sfParams.m_szParam1, pszParamReadFromAnswerFile);

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::Install
//
// Purpose:   Do operations necessary for install.
//
// Arguments:
//    dwSetupFlags [in]  Setup flags
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:     Dont do anything irreversible (like modifying registry) yet
//            since the config. actually complete only when Apply is called!
//
STDMETHODIMP CSampleFilter::Install(DWORD dw)
{
    TraceMsg(L"--> CSampleFilter::Install\n");

    // Start up the install process
    HRESULT hr = S_OK;

    m_eApplyAction = eActInstall;

    return hr;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::Removing
//
// Purpose:   Do necessary cleanup when being removed
//
// Arguments: None
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:     Dont do anything irreversible (like modifying registry) yet
//            since the removal is actually complete only when Apply is called!
//
STDMETHODIMP CSampleFilter::Removing(VOID)
{
    TraceMsg(L"--> CSampleFilter::Removing\n");

    HRESULT     hr = S_OK;

    m_eApplyAction = eActRemove;

    return hr;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::Cancel
//
// Purpose:   Cancel any changes made to internal data
//
// Arguments: None
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::CancelChanges(VOID)
{
    TraceMsg(L"--> CSampleFilter::CancelChanges\n");

    m_sfParams.m_szParam1[0] = '\0';

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::ApplyRegistryChanges
//
// Purpose:   Apply changes.
//
// Arguments: None
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:     We can make changes to registry etc. here.
//
STDMETHODIMP CSampleFilter::ApplyRegistryChanges(VOID)
{
    TraceMsg(L"--> CSampleFilter::ApplyRegistryChanges\n");

    HRESULT hr=S_OK;
    HKEY hkeyParams=NULL;
    HKEY hkeyAdapter;

    //
    // set default BundleId for newly added adapters
    //
    for (UINT cAdapter=0; cAdapter < m_cAdaptersAdded; cAdapter++)
    {
        hr = HrOpenAdapterParamsKey(&m_guidAdaptersAdded[cAdapter],
                                    &hkeyAdapter);
        if (S_OK == hr)
        {
            RegSetValueEx(
                    hkeyAdapter,
                    c_szBundleId,
                    NULL, REG_SZ,
                    (LPBYTE) c_szBundleIdDefault,
                    wcslen(c_szBundleIdDefault)
                    *sizeof(WCHAR));

            RegCloseKey(hkeyAdapter);
        }
    }

    //
    // delete parameters of adapters that are unbound/removed
    //
    for (cAdapter=0; cAdapter < m_cAdaptersRemoved; cAdapter++)
    {
        //$ REVIEW  kumarp 23-November-98
        //
        // code to remove sfilter\Parameters\Adapters\{guid} key
    }

    // do things that are specific to a config action

    switch (m_eApplyAction)
    {
    case eActPropertyUI:
        // A possible improvement might be to write the reg. only
        // if Param1 is modified.

        hr = m_pncc->OpenParamKey(&hkeyParams);
        if (S_OK == hr)
        {
            RegSetValueEx(hkeyParams, c_szParam1, NULL, REG_SZ,
                          (LPBYTE) m_sfParams.m_szParam1,
                          wcslen(m_sfParams.m_szParam1)*sizeof(WCHAR));

            RegCloseKey(hkeyParams);
        }

        HKEY hkeyAdapter;
        hr = HrOpenAdapterParamsKey(&m_guidAdapter, &hkeyAdapter);
        if (S_OK == hr)
        {
            RegSetValueEx(hkeyAdapter, c_szBundleId, NULL, REG_SZ,
                          (LPBYTE) m_sfParams.m_szBundleId,
                          wcslen(m_sfParams.m_szBundleId)*sizeof(WCHAR));
            RegCloseKey(hkeyAdapter);
        }
        break;


    case eActInstall:
    case eActRemove:
        break;
    }

    return hr;
}

STDMETHODIMP
CSampleFilter::ApplyPnpChanges(
    IN INetCfgPnpReconfigCallback* pICallback)
{
    WCHAR szDeviceName[64];

    StringFromGUID2(
        m_guidAdapter,
        szDeviceName,
        (sizeof(szDeviceName) / sizeof(szDeviceName[0])));

    pICallback->SendPnpReconfig (
        NCRL_NDIS,
        c_szSFilterNdisName,
        szDeviceName,
        m_sfParams.m_szBundleId,
        (wcslen(m_sfParams.m_szBundleId) + 1) * sizeof(WCHAR));

    return S_OK;
}

// =================================================================
// INetCfgSystemNotify
// =================================================================

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::GetSupportedNotifications
//
// Purpose:   Tell the system which notifications we are interested in
//
// Arguments:
//    pdwNotificationFlag [out]  pointer to NotificationFlag
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::GetSupportedNotifications(
        OUT DWORD* pdwNotificationFlag)
{
    TraceMsg(L"--> CSampleFilter::GetSupportedNotifications\n");

    *pdwNotificationFlag = NCN_NET | NCN_NETTRANS | NCN_ADD | NCN_REMOVE;

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::SysQueryBindingPath
//
// Purpose:   Allow or veto formation of a binding path
//
// Arguments:
//    dwChangeFlag [in]  type of binding change
//    pncbp        [in]  pointer to INetCfgBindingPath object
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::SysQueryBindingPath(DWORD dwChangeFlag,
        INetCfgBindingPath* pncbp)
{
    TraceMsg(L"--> CSampleFilter::SysQueryBindingPath\n");

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::SysNotifyBindingPath
//
// Purpose:   System tells us by calling this function which
//            binding path has just been formed.
//
// Arguments:
//    dwChangeFlag [in]  type of binding change
//    pncbpItem    [in]  pointer to INetCfgBindingPath object
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::SysNotifyBindingPath(DWORD dwChangeFlag,
        INetCfgBindingPath* pncbpItem)
{
    TraceMsg(L"--> CSampleFilter::SysNotifyBindingPath\n");

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::SysNotifyComponent
//
// Purpose:   System tells us by calling this function which
//            component has undergone a change (installed/removed)
//
// Arguments:
//    dwChangeFlag [in]  type of system change
//    pncc         [in]  pointer to INetCfgComponent object
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::SysNotifyComponent(DWORD dwChangeFlag,
        INetCfgComponent* pncc)
{
    TraceMsg(L"--> CSampleFilter::SysNotifyComponent\n");

    return S_OK;
}

// =================================================================
// INetCfgProperties
// =================================================================


STDMETHODIMP CSampleFilter::SetContext(
        IUnknown * pUnk)
{
    TraceMsg(L"--> CSampleFilter::SetContext\n");

    HRESULT hr = S_OK;

    // release previous context, if any
    ReleaseObj(m_pUnkContext);
    m_pUnkContext = NULL;

    if (pUnk) // set the new context
    {
        m_pUnkContext = pUnk;
        m_pUnkContext->AddRef();
        ZeroMemory(&m_guidAdapter, sizeof(m_guidAdapter));

        // here we assume that we are going to be called only for
        // a LAN connection since the sample IM works only with
        // LAN devices
        INetLanConnectionUiInfo * pLanConnUiInfo;
        hr = m_pUnkContext->QueryInterface(
                IID_INetLanConnectionUiInfo,
                reinterpret_cast<PVOID *>(&pLanConnUiInfo));
        if (S_OK == hr)
        {
            hr = pLanConnUiInfo->GetDeviceGuid(&m_guidAdapter);
            ReleaseObj(pLanConnUiInfo);
        }
    }

    return hr;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::MergePropPages
//
// Purpose:   Supply our property page to system
//
// Arguments:
//    pdwDefPages   [out]  pointer to num default pages
//    pahpspPrivate [out]  pointer to array of pages
//    pcPages       [out]  pointer to num pages
//    hwndParent    [in]   handle of parent window
//    szStartPage   [in]   pointer to
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::MergePropPages(
    IN OUT DWORD* pdwDefPages,
    OUT LPBYTE* pahpspPrivate,
    OUT UINT* pcPages,
    IN HWND hwndParent,
    OUT PCWSTR* szStartPage)
{
    TraceMsg(L"--> CSampleFilter::MergePropPages\n");

    HRESULT             hr      = S_OK;
    HPROPSHEETPAGE*     ahpsp   = NULL;

    m_eApplyAction = eActPropertyUI;

    // We don't want any default pages to be shown
    *pdwDefPages = 0;
    *pcPages = 0;
    *pahpspPrivate = NULL;

    ahpsp = (HPROPSHEETPAGE*)CoTaskMemAlloc(sizeof(HPROPSHEETPAGE));
    if (ahpsp)
    {
        PROPSHEETPAGE   psp = {0};

        psp.dwSize            = sizeof(PROPSHEETPAGE);
        psp.dwFlags           = PSP_DEFAULT;
        psp.hInstance         = _Module.GetModuleInstance();
        psp.pszTemplate       = MAKEINTRESOURCE(IDD_SAMPLE_FILTER_GENERAL);
        psp.pfnDlgProc        = (DLGPROC) SampleFilterDialogProc;
        psp.pfnCallback       = (LPFNPSPCALLBACK) SampleFilterPropSheetPageProc;
        // for Win64, use LONG_PTR instead of LPARAM
        psp.lParam            = (LPARAM) this;
        psp.pszHeaderTitle    = NULL;
        psp.pszHeaderSubTitle = NULL;

        ahpsp[0] = ::CreatePropertySheetPage(&psp);
        *pcPages = 1;
        *pahpspPrivate = (LPBYTE) ahpsp;
    }

    return hr;
}


// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::ValidateProperties
//
// Purpose:   Validate changes to property page
//
// Arguments:
//    hwndSheet [in]  window handle of property sheet
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::ValidateProperties(HWND hwndSheet)
{
    TraceMsg(L"--> CSampleFilter::ValidateProperties\n");

    // Accept any change to Param1

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::CancelProperties
//
// Purpose:   Cancel changes to property page
//
// Arguments: None
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::CancelProperties(VOID)
{
    TraceMsg(L"--> CSampleFilter::CancelProperties\n");

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::ApplyProperties
//
// Purpose:   Apply value of controls on property page
//            to internal memory structure
//
// Arguments: None
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:     We do this work in OnOk so no need to do it here again.
//
STDMETHODIMP CSampleFilter::ApplyProperties(VOID)
{
    TraceMsg(L"--> CSampleFilter::ApplyProperties\n");

    return S_OK;
}


// =================================================================
// INetCfgBindNotify
// =================================================================

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::QueryBindingPath
//
// Purpose:   Allow or veto a binding path involving us
//
// Arguments:
//    dwChangeFlag [in]  type of binding change
//    pncbi        [in]  pointer to INetCfgBindingPath object
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::QueryBindingPath(DWORD dwChangeFlag,
        INetCfgBindingPath* pncbp)
{
    TraceMsg(L"--> CSampleFilter::QueryBindingPath\n");

    // we do not want to veto any binding path

    return S_OK;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::NotifyBindingPath
//
// Purpose:   System tells us by calling this function which
//            binding path involving us has just been formed.
//
// Arguments:
//    dwChangeFlag [in]  type of binding change
//    pncbp        [in]  pointer to INetCfgBindingPath object
//
// Returns:   S_OK on success, otherwise an error code
//
// Notes:
//
STDMETHODIMP CSampleFilter::NotifyBindingPath(DWORD dwChangeFlag,
        INetCfgBindingPath* pncbp)
{
    TraceMsg(L"--> CSampleFilter::NotifyBindingPath\n");

    return S_OK;
}

// ------------ END OF NOTIFY OBJECT FUNCTIONS --------------------



// -----------------------------------------------------------------
// Property Sheet related functions
//

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::OnInitDialog
//
// Purpose:   Initialize controls
//
// Arguments:
//    hWnd [in]  window handle
//
// Returns:
//
// Notes:
//
LRESULT CSampleFilter::OnInitDialog(IN HWND hWnd)
{
    HKEY hkeyParams;
    HRESULT hr;
    DWORD dwSize;
    DWORD dwError;

    // read in Param1 & BundleId if not already read
    if (!m_fConfigRead)
    {
        m_fConfigRead = TRUE;
        hr = m_pncc->OpenParamKey(&hkeyParams);
        if (S_OK == hr)
        {
            // if this fails, we will show an empty edit box for Param1
            dwSize = MAX_PATH;
            RegQueryValueExW(hkeyParams, c_szParam1, NULL, NULL,
                            (LPBYTE) m_sfParams.m_szParam1, &dwSize);
            RegCloseKey(hkeyParams);
        }

        HKEY hkeyAdapter;
        hr = HrOpenAdapterParamsKey(&m_guidAdapter, &hkeyAdapter);
        if (S_OK == hr)
        {
            dwSize = MAX_PATH;
            dwError = RegQueryValueExW(hkeyAdapter, c_szBundleId, NULL, NULL,
                                      (LPBYTE) m_sfParams.m_szBundleId, &dwSize);
            RegCloseKey(hkeyAdapter);
        }
    }

    // Param1 edit box
    ::SendMessage(GetDlgItem(hWnd, IDC_PARAM1), EM_SETLIMITTEXT, MAX_PATH-1, 0);
    ::SetWindowText(GetDlgItem(hWnd, IDC_PARAM1), m_sfParams.m_szParam1);

    // BundleId edit box
    ::SendMessage(GetDlgItem(hWnd, IDC_BundleId), EM_SETLIMITTEXT, MAX_PATH-1, 0);
    ::SetWindowText(GetDlgItem(hWnd, IDC_BundleId), m_sfParams.m_szBundleId);

    return PSNRET_NOERROR;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::OnOk
//
// Purpose:   Do actions when OK is pressed
//
// Arguments:
//    hWnd [in]  window handle
//
// Returns:
//
// Notes:
//
LRESULT CSampleFilter::OnOk(IN HWND hWnd)
{
    TraceMsg(L"--> CSampleFilter::OnOk\n");

    ::GetWindowText(GetDlgItem(hWnd, IDC_PARAM1),
                    m_sfParams.m_szParam1, MAX_PATH);
    ::GetWindowText(GetDlgItem(hWnd, IDC_BundleId),
                    m_sfParams.m_szBundleId, MAX_PATH);

    return PSNRET_NOERROR;
}

// ----------------------------------------------------------------------
//
// Function:  CSampleFilter::OnCancel
//
// Purpose:   Do actions when CANCEL is pressed
//
// Arguments:
//    hWnd [in]  window handle
//
// Returns:
//
// Notes:
//
LRESULT CSampleFilter::OnCancel(IN HWND hWnd)
{
    TraceMsg(L"--> CSampleFilter::OnCancel\n");

    return FALSE;
}

// ----------------------------------------------------------------------
//
// Function:  SampleFilterDialogProc
//
// Purpose:   Dialog proc
//
// Arguments:
//    hWnd   [in]  see win32 documentation
//    uMsg   [in]  see win32 documentation
//    wParam [in]  see win32 documentation
//    lParam [in]  see win32 documentation
//
// Returns:
//
// Notes:
//
LRESULT
CALLBACK
SampleFilterDialogProc (
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PROPSHEETPAGE*  ppsp;
    LRESULT         lRes = 0;

    static PROPSHEETPAGE* psp = NULL;
    CSampleFilter* psf;

    if (uMsg == WM_INITDIALOG)
    {
        ppsp = (PROPSHEETPAGE *)lParam;
        psf = (CSampleFilter *)ppsp->lParam;
        SetWindowLongPtr(hWnd, DWLP_USER, (LONG_PTR)psf);

        lRes = psf->OnInitDialog(hWnd);
        return lRes;
    }
    else
    {
        psf = (CSampleFilter *)::GetWindowLongPtr(hWnd, DWLP_USER);

        // Until we get WM_INITDIALOG, just return FALSE
        if (!psf)
        {
            return FALSE;
        }
    }

    if (WM_COMMAND == uMsg)
    {
        if (EN_CHANGE == HIWORD(wParam))
        {
            // Set the property sheet changed flag if any of our controls
            // get changed.  This is important so that we get called to
            // apply our property changes.
            //
            PropSheet_Changed(GetParent(hWnd), hWnd);
        }
    }
    else if (WM_NOTIFY == uMsg)
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;

        switch (pnmh->code)
        {
        case PSN_SETACTIVE:
            lRes = 0;        // accept activation
            break;

        case PSN_KILLACTIVE:
            // ok to loose being active
            SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);
            lRes = TRUE;
            break;

        case PSN_APPLY:
            lRes = psf->OnOk(hWnd);
            break;

        case PSN_RESET:
            lRes = psf->OnCancel(hWnd);
            break;

        default:
            lRes = FALSE;
            break;
        }
    }

    return lRes;
}

// ----------------------------------------------------------------------
//
// Function:  SampleFilterPropSheetPageProc
//
// Purpose:   Prop sheet proc
//
// Arguments:
//    hWnd [in]  see win32 documentation
//    uMsg [in]  see win32 documentation
//    ppsp [in]  see win32 documentation
//
// Returns:
//
// Notes:
//
UINT CALLBACK SampleFilterPropSheetPageProc(HWND hWnd, UINT uMsg,
                                            LPPROPSHEETPAGE ppsp)
{
    UINT uRet = TRUE;


    return uRet;
}


// -----------------------------------------------------------------
//
//  Utility Functions
//

HRESULT HrGetBindingInterfaceComponents (
    INetCfgBindingInterface*    pncbi,
    INetCfgComponent**          ppnccUpper,
    INetCfgComponent**          ppnccLower)
{
    HRESULT hr=S_OK;

    // Initialize output parameters
    *ppnccUpper = NULL;
    *ppnccLower = NULL;

    INetCfgComponent* pnccUpper;
    INetCfgComponent* pnccLower;

    hr = pncbi->GetUpperComponent(&pnccUpper);
    if (SUCCEEDED(hr))
    {
        hr = pncbi->GetLowerComponent(&pnccLower);
        if (SUCCEEDED(hr))
        {
            *ppnccUpper = pnccUpper;
            *ppnccLower = pnccLower;
        }
        else
        {
            ReleaseObj(pnccUpper);
        }
    }

    return hr;
}

HRESULT HrOpenAdapterParamsKey(GUID* pguidAdapter,
                               HKEY* phkeyAdapter)
{
    HRESULT hr=S_OK;

    HKEY hkeyServiceParams;
    WCHAR szGuid[48];
    WCHAR szAdapterSubkey[128];
    DWORD dwError;

    if (ERROR_SUCCESS ==
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, c_szSFilterParams,
                     0, KEY_ALL_ACCESS, &hkeyServiceParams))
    {
        StringFromGUID2(*pguidAdapter, szGuid, 47);
        _stprintf(szAdapterSubkey, L"Adapters\\%s", szGuid);
        if (ERROR_SUCCESS !=
            (dwError = RegOpenKeyEx(hkeyServiceParams,
                                    szAdapterSubkey, 0,
                                    KEY_ALL_ACCESS, phkeyAdapter)))
        {
            hr = HRESULT_FROM_WIN32(dwError);
        }
        RegCloseKey(hkeyServiceParams);
    }

    return hr;
}

#if DBG
void TraceMsg(PCWSTR szFormat, ...)
{
    static WCHAR szTempBuf[4096];

    va_list arglist;

    va_start(arglist, szFormat);

    _vstprintf(szTempBuf, szFormat, arglist);
    OutputDebugString(szTempBuf);

    va_end(arglist);
}

#endif

