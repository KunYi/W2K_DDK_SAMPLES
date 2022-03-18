//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1997-1999.
//
//  File:       S F I L T E R . H
//
//  Contents:   Notify object code for the sample filter.
//
//  Notes:
//
//  Author:     kumarp 26-March-98
//
//----------------------------------------------------------------------------

#pragma once
#include "sfiltern.h"
#include "resource.h"

// What type of config change the user/system is performing
enum ConfigAction {eActUnknown, eActInstall, eActRemove, eActPropertyUI};

#define MAX_ADAPTERS 64         // max no. of physical adapters in a machine

class CSampleFilterParams
{
public:
    WCHAR m_szParam1[MAX_PATH];
    WCHAR m_szBundleId[MAX_PATH];

    CSampleFilterParams();
};

class CSampleFilter :
    public CComObjectRoot,
    public CComCoClass<CSampleFilter, &CLSID_CSampleFilter>,
    public INetCfgComponentControl,
    public INetCfgComponentSetup,
    public INetCfgComponentPropertyUi,
    public INetCfgComponentNotifyBinding,
    public INetCfgComponentNotifyGlobal
{
public:
    CSampleFilter(VOID);
    ~CSampleFilter(VOID);

    BEGIN_COM_MAP(CSampleFilter)
        COM_INTERFACE_ENTRY(INetCfgComponentControl)
        COM_INTERFACE_ENTRY(INetCfgComponentSetup)
        COM_INTERFACE_ENTRY(INetCfgComponentPropertyUi)
        COM_INTERFACE_ENTRY(INetCfgComponentNotifyBinding)
        COM_INTERFACE_ENTRY(INetCfgComponentNotifyGlobal)
    END_COM_MAP()

    // DECLARE_NOT_AGGREGATABLE(CSampleFilter)
    // Remove the comment from the line above if you don't want your object to
    // support aggregation.  The default is to support it

    DECLARE_REGISTRY_RESOURCEID(IDR_REG_SAMPLE_FILTER)

// INetCfgComponentControl
    STDMETHOD (Initialize) (
        IN INetCfgComponent* pIComp,
        IN INetCfg* pINetCfg,
        IN BOOL fInstalling);
    STDMETHOD (ApplyRegistryChanges) ();
    STDMETHOD (ApplyPnpChanges) (
        IN INetCfgPnpReconfigCallback* pICallback);
    STDMETHOD (CancelChanges) ();

// INetCfgComponentSetup
    STDMETHOD (ReadAnswerFile)      (PCWSTR szAnswerFile,
                                     PCWSTR szAnswerSections);
    STDMETHOD (Upgrade)             (DWORD, DWORD) {return S_OK;}
    STDMETHOD (Install)             (DWORD);
    STDMETHOD (Removing)            ();

// INetCfgProperties
    STDMETHOD (QueryPropertyUi) (
        IN IUnknown* pUnk) { return S_OK; }
    STDMETHOD (SetContext) (
        IN IUnknown* pUnk);
    STDMETHOD (MergePropPages) (
        IN OUT DWORD* pdwDefPages,
        OUT LPBYTE* pahpspPrivate,
        OUT UINT* pcPrivate,
        IN HWND hwndParent,
        OUT PCWSTR* pszStartPage);
    STDMETHOD (ValidateProperties) (
        HWND hwndSheet);
    STDMETHOD (CancelProperties) ();
    STDMETHOD (ApplyProperties) ();

// INetCfgNotifyBinding
    STDMETHOD (QueryBindingPath)       (DWORD dwChangeFlag, INetCfgBindingPath* pncbp);
    STDMETHOD (NotifyBindingPath)      (DWORD dwChangeFlag, INetCfgBindingPath* pncbp);

// INetCfgNotifyGlobal
    STDMETHOD (GetSupportedNotifications) (DWORD* pdwNotificationFlag );
    STDMETHOD (SysQueryBindingPath)       (DWORD dwChangeFlag, INetCfgBindingPath* pncbp);
    STDMETHOD (SysNotifyBindingPath)      (DWORD dwChangeFlag, INetCfgBindingPath* pncbp);
    STDMETHOD (SysNotifyComponent)        (DWORD dwChangeFlag, INetCfgComponent* pncc);

private:
    INetCfgComponent*   m_pncc;
    INetCfg*            m_pnc;
    ConfigAction        m_eApplyAction;
    CSampleFilterParams m_sfParams;
    IUnknown*           m_pUnkContext;
    GUID                m_guidAdapter;
    GUID                m_guidAdaptersAdded[MAX_ADAPTERS];
    UINT                m_cAdaptersAdded;
    GUID                m_guidAdaptersRemoved[MAX_ADAPTERS];
    UINT                m_cAdaptersRemoved;
    BOOL                m_fConfigRead;

// Utility functions
public:
    LRESULT OnInitDialog(IN HWND hWnd);
    LRESULT OnOk(IN HWND hWnd);
    LRESULT OnCancel(IN HWND hWnd);

private:

};


