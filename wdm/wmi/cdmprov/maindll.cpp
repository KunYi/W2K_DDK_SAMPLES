//***************************************************************************
//
//  MAINDLL.CPP
// 
//  Module: WINMGMT class provider sample code
//
//  Purpose: Contains DLL entry points.  Also has code that controls
//           when the DLL can be unloaded by tracking the number of
//           objects and locks as well as routines that support
//           self registration.
//
//  Copyright (c) 2000 Microsoft Corporation
//
//***************************************************************************

#include <objbase.h>
#include <initguid.h>
#include "sample.h"

#include "wbemmisc.h"

HMODULE ghModule;
HANDLE CdmMutex;

// {AC42F9A6-9945-426f-9199-86F7257365D4}
DEFINE_GUID(CLSID_classprovider,
0xac42f9a6, 0x9945, 0x426f, 0x91, 0x99, 0x86, 0xf7, 0x25, 0x73, 0x65, 0xd4);

//Count number of objects and number of locks.

long       g_cObj=0;
long       g_cLock=0;

//***************************************************************************
//
// LibMain32
//
// Purpose: Entry point for DLL.
//
// Return: TRUE if OK.
//
//***************************************************************************


extern "C" BOOL WINAPI LibMain32(HINSTANCE hInstance, ULONG ulReason
    , LPVOID pvReserved)
{
    if (DLL_PROCESS_ATTACH==ulReason)
	{
        ghModule = hInstance;
		CdmMutex = CreateMutex(NULL,
							   FALSE,
							   NULL);
		if (CdmMutex == NULL)
		{
			return(FALSE);
		}
	}
    return(TRUE);
}

//***************************************************************************
//
//  DllGetClassObject
//
//  Purpose: Called by Ole when some client wants a class factory.  Return 
//           one only if it is the sort of class this DLL supports.
//
//***************************************************************************


STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, PPVOID ppv)
{
    HRESULT hr;
    CProvFactory *pObj;

    if (CLSID_classprovider!=rclsid)
        return E_FAIL;

    pObj=new CProvFactory();

    if (NULL==pObj)
        return E_OUTOFMEMORY;

    hr=pObj->QueryInterface(riid, ppv);

    if (FAILED(hr))
        delete pObj;

    return hr;
}

//***************************************************************************
//
// DllCanUnloadNow
//
// Purpose: Called periodically by Ole in order to determine if the
//          DLL can be freed.
//
// Return:  S_OK if there are no objects in use and the class factory 
//          isn't locked.
//
//***************************************************************************

STDAPI DllCanUnloadNow(void)
{
    SCODE   sc;

    //It is OK to unload if there are no objects or locks on the 
    // class factory.
#if 0    
    sc=(0L==g_cObj && 0L==g_cLock) ? S_OK : S_FALSE;
    return sc;
#else
	//
	// We always remain loaded since we carry around result objects for
	// the results of previously run tests and do not want to unload
	// when there are result objects still held. In the future we will
	// want to check if all results are cleared and if so then unload,
	// but for now we'll stay loaded.
	//
	return(S_FALSE);
#endif
}

//***************************************************************************
//
// DllRegisterServer
//
// Purpose: Called during setup or by regsvr32.
//
// Return:  NOERROR if registration successful, error otherwise.
//***************************************************************************

STDAPI DllRegisterServer(void)
{   
    char       szID[128];
    WCHAR      wcID[128];
    char       szCLSID[128];
    char       szModule[MAX_PATH];
    char * pName = "CDMPROV Test Class Provider";
    char * pModel = "Both";
    HKEY hKey1, hKey2;

    // Create the path.

    StringFromGUID2(CLSID_classprovider, wcID, 128);
    wcstombs(szID, wcID, 128);
    lstrcpy(szCLSID, TEXT("Software\\classes\\CLSID\\"));
    lstrcat(szCLSID, szID);

    // Create entries under CLSID

    RegCreateKey(HKEY_LOCAL_MACHINE, szCLSID, &hKey1);
    RegSetValueEx(hKey1, NULL, 0, REG_SZ, (BYTE *)pName, lstrlen(pName)+1);
    RegCreateKey(hKey1,"InprocServer32",&hKey2);

    GetModuleFileName(ghModule, szModule,  MAX_PATH);
    RegSetValueEx(hKey2, NULL, 0, REG_SZ, (BYTE *)szModule, 
                                        lstrlen(szModule)+1);
    RegSetValueEx(hKey2, "ThreadingModel", 0, REG_SZ, 
                                        (BYTE *)pModel, lstrlen(pModel)+1);
    RegCloseKey(hKey1);
    RegCloseKey(hKey2);
    return NOERROR;
}

//***************************************************************************
//
// DllUnregisterServer
//
// Purpose: Called when it is time to remove the registry entries.
//
// Return:  NOERROR if registration successful, error otherwise.
//***************************************************************************

STDAPI DllUnregisterServer(void)
{
    char       szID[128];
    WCHAR      wcID[128];
    char  szCLSID[128];
    HKEY hKey;

    // Create the path using the CLSID

    StringFromGUID2(CLSID_classprovider, wcID, 128);
    wcstombs(szID, wcID, 128);
    lstrcpy(szCLSID, TEXT("Software\\classes\\CLSID\\"));
    lstrcat(szCLSID, szID);

    // First delete the InProcServer subkey.

    DWORD dwRet = RegOpenKey(HKEY_LOCAL_MACHINE, szCLSID, &hKey);
    if(dwRet == NO_ERROR)
    {
        RegDeleteKey(hKey, "InProcServer32");
        RegCloseKey(hKey);
    }

    dwRet = RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\classes\\CLSID", &hKey);
    if(dwRet == NO_ERROR)
    {
        RegDeleteKey(hKey,szID);
        RegCloseKey(hKey);
    }

    return NOERROR;
}



