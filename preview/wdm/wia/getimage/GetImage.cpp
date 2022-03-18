/*******************************************************************************
*
*  (C) COPYRIGHT MICROSOFT CORP., 1999
*
*  TITLE:       GetImage.Cpp
*
*  VERSION:     1.0
*
*  DATE:        11 Jun, 1999
*
*  DESCRIPTION:
*   simple command-line application for WIA device communication
*
*******************************************************************************/
#include "stdafx.h"

//
// Helper functions
//

BOOL CreateWIADeviceManager();
BOOL EnumerateDevices();
BOOL EnumerateItems();
BOOL CreateThisDevice(BSTR DeviceID);
BOOL SetWIAItem();

//
// IWiaDataTransfer (data transfer helpers)
//

BOOL DoIdtGetDataTransfer(IWiaItem *pIWiaItem, DWORD Tymed, GUID WIAFormat);
BOOL MoveTempFile(LPWSTR pwszTempFileName, LPCTSTR szTargetFileName);
GUID WIAFormatStrToValue(LPTSTR pClipboardFormat);
//
// IWiaItem enumeration helpers
//

HRESULT EnumerateAllItems(IWiaItem *pIRootItem);
HRESULT EnumNextLevel(IEnumWiaItem *pEnumItem);
VOID DisplayItemName(IWiaItem *pIWiaItem);

void CleanUp();

//
// Property manipulation helpers
//

HRESULT ReadPropStr(PROPID   propid, IWiaPropertyStorage *pIWiaPropStg, BSTR *pbstr);
HRESULT WritePropStr(PROPID  propid, IWiaPropertyStorage *pIWiaPropStg, BSTR bstr);
HRESULT ReadPropLong(PROPID  propid, IWiaPropertyStorage *pIWiaPropStg, LONG *plval);
HRESULT WritePropLong(PROPID propid, IWiaPropertyStorage *pIWiaPropStg, LONG lVal);
HRESULT WritePropGUID(PROPID propid, IWiaPropertyStorage *pIWiaPropStg, GUID guidVal);

//
// Globals
//

TCHAR g_szFileName[MAX_PATH];
TCHAR g_szClipboardFormat[MAX_PATH];
BSTR g_bstrDeviceID          = NULL;
BSTR g_bstrItemName          = NULL;
IWiaDevMgr  *g_pIWiaDevMgr   = NULL;
IWiaItem    *g_pIWiaRootItem = NULL;
IWiaItem    *g_pIWiaItem     = NULL;
BOOL g_bDisplayOnly = FALSE;


//
// Data callback class definition
//

#define WM_STATUS WM_USER+5

class CWiaDataCallback : public IWiaDataCallback
{
private:
   ULONG                    m_cRef;         // Object reference count.
   PBYTE                    m_pBuffer;      // complete data buffer
   LONG                     m_BufferLength;
   LONG                     m_BytesTransfered;
   GUID                     m_guidFormatID;

public:

    CWiaDataCallback();
    ~CWiaDataCallback();

    // IUnknown members that delegate to m_pUnkRef.
    HRESULT WINAPI QueryInterface(const IID&,void**);
    ULONG   WINAPI AddRef();
    ULONG   WINAPI Release();

    HRESULT WINAPI Initialize();
    HRESULT WINAPI CWiaDataCallback::DisplayWindow();

    BYTE* WINAPI GetDataPtr();
    HRESULT WINAPI BandedDataCallback(
       LONG                            lMessage,
       LONG                            lStatus,
       LONG                            lPercentComplete,
       LONG                            lOffset,
       LONG                            lLength,
       LONG                            lReserved,
       LONG                            lResLength,
       BYTE*                           pbBuffer);
};

/**************************************************************************\
* main()
*
*   Main program execution function
*
*
*
* Arguments:
*
*   argc - Number of command-line arguments
*   argv[] - Array of command-line strings
*
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
extern "C"
#ifdef UNICODE
INT _cdecl wmain(INT argc, WCHAR *argv[ ], WCHAR *envp[ ] )
#else
INT _cdecl main(INT argc, LPTSTR argv[])
#endif
{
    HRESULT hr = S_OK;
    hr = ::OleInitialize(NULL);
    if(SUCCEEDED(hr)){

        //
        // assign default setting values
        //

        lstrcpy(g_szFileName,TEXT("image.bmp"));
        lstrcpy(g_szClipboardFormat,TEXT("IMGFMT_BMP"));

        TCHAR ValidTokens[]  = "/";
        TCHAR *ptoken        = NULL;
        INT Index           = 1;
        INT strIndex        = 0;

        for(Index = 1;Index < argc;Index++){
            ptoken = strtok(argv[Index],ValidTokens);
            while(ptoken != NULL){
                switch(ptoken[0])
                {
                case '?':
                    {
                        printf(TEXT("\n\n"));
                        printf(TEXT("getimage - retrieves an image from a specified device\n"));
                        printf(TEXT("Usage: getimage [/d \"deviceID\"] [/c \"clipboard format\"][/f \"filename\"] \n"));
                        printf(TEXT("Defaults: IMGFMT_BMP, image.bmp\n\n"));
                        printf(TEXT("Arguments:\n\n"));
                        printf(TEXT("/? This help screen\n"));
                        printf(TEXT("/d target device ID  (ex. /d \"{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}\\0001\")\n"));
                        printf(TEXT("/c Clipboard Format  (ex. /c \"IMGFMT_BMP\")\n"));
                        printf(TEXT("possible choices: (IMGFMT_BMP, IMGFMT_JPEG, IMGFMT_TIFF)\n"));
                        printf(TEXT("/f image file name   (ex. /f \"c:\\myimage.bmp\")\n"));
                        printf(TEXT("/i name of target child item for transfer (ex. /i \"0001\\Root\\002\")\n"));
                        printf(TEXT("/enum Enumerate Devices on system\n"));
                        printf(TEXT("/items Enumerate child items, (must be used with /d )\n"));
                        g_bDisplayOnly = TRUE;
                    }
                    break;
                case 'd':
                    Index++;
                    if(Index == argc)
                        break;
                    ptoken = strtok(argv[Index],ValidTokens);
                    if(ptoken != NULL){
                        WCHAR wszBuffer[64];
                        memset(wszBuffer,0,sizeof(wszBuffer));

                        //
                        // convert DeviceID string
                        //

                        MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,ptoken,-1,wszBuffer,sizeof(wszBuffer));

                        //
                        // alloc BSTR for Device ID
                        //

                        g_bstrDeviceID = SysAllocString(wszBuffer);
                    }
                    else
                        printf(TEXT("Invalid: Device ID = (NULL)\n"));
                    break;
                case 'i':
                    Index++;
                    if(Index == argc)
                        break;
                    ptoken = strtok(argv[Index],ValidTokens);
                    if(ptoken != NULL){
                        WCHAR wszBuffer[64];
                        memset(wszBuffer,0,sizeof(wszBuffer));

                        //
                        // convert Item Name string
                        //

                        MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,ptoken,-1,wszBuffer,sizeof(wszBuffer));

                        //
                        // alloc BSTR for Item Name
                        //

                        g_bstrItemName = SysAllocString(wszBuffer);
                    }
                    else
                        printf(TEXT("Invalid: Item Name = (NULL)\n"));
                    break;
                case 'c':
                    Index++;
                    if(Index == argc)
                        break;
                    ptoken = strtok(argv[Index],ValidTokens);
                    if(ptoken != NULL)
                        lstrcpy(g_szClipboardFormat,ptoken);
                    else
                        printf(TEXT("Invalid: Clipboard Format = (NULL)\n"));
                    break;
                case 'f':
                    if(Index == argc)
                        break;
                    Index++;
                    ptoken = strtok(argv[Index],ValidTokens);
                    if(ptoken != NULL)
                        lstrcpy(g_szFileName,ptoken);
                    else
                        printf(TEXT("Invalid: FileName = (NULL)\n"));
                    break;
                default:
                    break;
                }

                //
                // Check for full command strings
                //

                if(ptoken != NULL){
                    if(lstrcmpi(ptoken,TEXT("enum")) == 0){
                        printf(TEXT("Enumerate devices requested...\n"));
                        EnumerateDevices();
                        g_bDisplayOnly = TRUE;
                    }
                }

                if(ptoken != NULL){
                    if(lstrcmpi(ptoken,TEXT("items")) == 0){
                        printf(TEXT("Enumerate items requested...\n"));
                        EnumerateItems();
                        g_bDisplayOnly = TRUE;
                    }
                }

                //
                //printf( "DEBUG ONLY -->CommandLine Argument  %s found\n", ptoken );
                //

                ptoken = strtok(NULL,ValidTokens);
            }
        }
    }
    else{
        printf(TEXT("OLE failed to initialize\n"));
        return 0;
    }

    //
    // Dump stats about current operation
    //

    if(!g_bDisplayOnly){
        printf(TEXT("\n\n"));
        printf(TEXT("=== Current settings ===\n"));
        printf(TEXT("Filename:\t%s\n"),g_szFileName);
        if(g_bstrDeviceID == NULL)
            printf(TEXT("DeviceID:\t[ none specified]\n"));
        else
            printf(TEXT("DeviceID:\t%ws\n"),g_bstrDeviceID);
        if(g_bstrItemName == NULL)
            printf(TEXT("Item Name:\t[ none specified]\n"));
        else
            printf(TEXT("Item Name:\t%ws\n"),g_bstrItemName);
        printf(TEXT("Format:  \t%s\n"),g_szClipboardFormat);
        printf(TEXT("========================\n"));
        printf(TEXT("\n\n"));

    //
    // Start WIA operations
    //

        if(CreateWIADeviceManager()){
            if(CreateThisDevice(g_bstrDeviceID))
            {
                printf(TEXT("Device Created successfully...\n"));
                DWORD Tymed = TYMED_FILE;
                GUID WIAFormat = WIAFormatStrToValue(g_szClipboardFormat);
                if(SetWIAItem()){
                    if(DoIdtGetDataTransfer(g_pIWiaItem,Tymed,WIAFormat)){
                        printf(TEXT("\nTransfer completed successfully...\n"));
                    }
                }
            }
        }
    }

    //
    // End WIA operations
    //

    CleanUp();
    ::OleUninitialize();
    return 0;
}
/**************************************************************************\
* WIAFormatStrToValue()
*
*   Converts the command-line string to a valid format value.
*   IMGFMT_BMP is the default is this function fails
*
*
*
* Arguments:
*
*   pWIAFormat - format in string form
*
*
* Return Value:
*
*   WIAFormat format value
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
GUID WIAFormatStrToValue(LPTSTR pWIAFormat)
{
    if(lstrcmpi(pWIAFormat,TEXT("IMGFMT_BMP")) == 0)
        return IMGFMT_BMP;
    if(lstrcmpi(pWIAFormat,TEXT("IMGFMT_JPEG")) == 0)
        return IMGFMT_JPEG;
    if(lstrcmpi(pWIAFormat,TEXT("IMGFMT_TIFF")) == 0)
        return IMGFMT_TIFF;        

    //
    // default is IMGFMT_BMP
    //

    return IMGFMT_BMP;
}
/**************************************************************************\
* SetWIAItem()
*
*   Sets the value of the global IWiaItem.  This function uses the API
*   FindItemByName(), with the global item name taken from the command-line
*
*
*
* Arguments:
*
*   none
*
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
BOOL SetWIAItem()
{
    HRESULT hr = S_OK;
    hr = g_pIWiaRootItem->FindItemByName(0,g_bstrItemName,&g_pIWiaItem);
    if(hr == S_OK)
        return TRUE;
    else if(hr == S_FALSE)
        printf(TEXT("Item not found...\n"));
    return FALSE;
}
/**************************************************************************\
* CleanUp()
*
*   Releases the WIA Device Manager, and any IWiaItems obtained during the life
*   of this exe
*
*
*
* Arguments:
*
*   none
*
*
* Return Value:
*
*   void
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
void CleanUp()
{

    //
    // Release any WIA Root Items (Devices)
    //

    if(g_pIWiaRootItem != NULL)
        g_pIWiaRootItem->Release();

    //
    // Release WIA Device Manager
    //

    if(g_pIWiaDevMgr != NULL)
        g_pIWiaDevMgr->Release();
}
/**************************************************************************\
* EnumerateDevices()
*
*   Enumerates all WIA devices installed on the system, and displays their
*   Name, Device ID, and Server name
*
*
*
* Arguments:
*
*   none
*
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
BOOL EnumerateDevices()
{
    HRESULT hr = S_OK;
    ULONG ulFetched = 0;
    if(CreateWIADeviceManager()){

        IWiaPropertyStorage    *pIWiaPropStg     = NULL;
        IEnumWIA_DEV_INFO   *pWiaEnumDevInfo = NULL;

        if (g_pIWiaDevMgr != NULL) {
            hr = g_pIWiaDevMgr->EnumDeviceInfo(WIA_DEVINFO_ENUM_LOCAL,&pWiaEnumDevInfo);
            if (SUCCEEDED(hr)){

                //
                // Call Reset on Enumerator
                //

                hr = pWiaEnumDevInfo->Reset();
                if (SUCCEEDED(hr)) {
                    do {

                        //
                        // Enumerate requesting 1 Device on each Next call
                        //

                        hr = pWiaEnumDevInfo->Next(1,&pIWiaPropStg,&ulFetched);
                        if (hr == S_OK) {

                            //
                            // Check Returned Values
                            //

                            if(ulFetched != 1){
                                printf(TEXT("Next returned %d item(s), after requesting for 1 item\n"),ulFetched);
                                hr = E_FAIL;
                            }

                            PROPSPEC        PropSpec[3];
                            PROPVARIANT     PropVar[3];

                            memset(PropVar,0,sizeof(PropVar));

                            PropSpec[0].ulKind = PRSPEC_PROPID;
                            PropSpec[0].propid = WIA_DIP_DEV_ID;

                            PropSpec[1].ulKind = PRSPEC_PROPID;
                            PropSpec[1].propid = WIA_DIP_DEV_NAME;

                            PropSpec[2].ulKind = PRSPEC_PROPID;
                            PropSpec[2].propid = WIA_DIP_SERVER_NAME;

                            hr = pIWiaPropStg->ReadMultiple(sizeof(PropSpec)/sizeof(PROPSPEC),
                                PropSpec,
                                PropVar);

                            if (hr == S_OK) {
                                printf(TEXT("Device Name: \t%ws\n"),PropVar[1].bstrVal);
                                printf(TEXT("Device ID: \t%ws\n"),PropVar[0].bstrVal);
                                printf(TEXT("Server Name: \t%ws\n"),PropVar[2].bstrVal);
                                printf(TEXT("\n"));

                                //
                                // Free Prop Varient
                                //

                                FreePropVariantArray(sizeof(PropSpec)/sizeof(PROPSPEC),PropVar);

                                //
                                // Release Property Storage
                                //

                                pIWiaPropStg->Release();
                            } else
                                printf(TEXT("ReadMultiple() Failed while reading device name,server,and deviceID\n"));
                        }
                        else if (hr == S_FALSE) {

                            //
                            // Enumeration Complete
                            //

                        } else
                            printf(TEXT("Next() Failed requesting 1 item\n"));

                    } while (hr == S_OK);
                } else
                    printf(TEXT("Reset() Failed\n"));
            }
            else{
                printf(TEXT("EnumDeviceInfo Failed\n"));
                return FALSE;
            }
        }
        else {
            printf(TEXT("WIA Device Manager is NULL\n"));
            return FALSE;
        }
    }
    else
        return FALSE;
    return TRUE;
}
/**************************************************************************\
* EnumerateItems()
*
*   Enumerates WIA items using the global RootItem, and global DeviceID
*
*
*
* Arguments:
*
*   none
*
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
BOOL EnumerateItems()
{
    if(CreateWIADeviceManager()){
        if(CreateThisDevice(g_bstrDeviceID)){
            if(!EnumerateAllItems(g_pIWiaRootItem))
                return FALSE;
        }
        else
            return FALSE;
    }
    return TRUE;
}

/**************************************************************************\
* EnumerateAllItems()
*
*   Creates an Active Tree list of all items using the target Root Item
*
* Arguments:
*
* pRootItem - target root item
*
* Return Value:
*
*   status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT EnumerateAllItems(IWiaItem *pIRootItem)
{
    HRESULT hr = S_OK;
    IEnumWiaItem* pEnumItem = NULL;

    if (pIRootItem != NULL) {
        DisplayItemName(pIRootItem);
        hr = pIRootItem->EnumChildItems(&pEnumItem);

        //
        // we have children so continue to enumerate them
        //

        if (hr == S_OK && pEnumItem != NULL) {
            EnumNextLevel(pEnumItem);
            pEnumItem->Release();
        }
    } else {

        //
        // pIRootItem is NULL!!
        //

        printf(TEXT("RootItem is NULL\n"));
        return E_INVALIDARG;
    }

    return  hr;
}
/**************************************************************************\
* EnumNextLevel()
*
*   Continues the Active Tree list creation of all items using a target Root Item
*
* Arguments:
*
* pEnumItem - Item Enumerator
*
* Return Value:
*
*   status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT EnumNextLevel(IEnumWiaItem *pEnumItem)
{
    IWiaItem *pIWiaItem;
    ULONG     cEnum;
    LONG      lType;
    HRESULT   hResult;

    while (pEnumItem->Next(1,&pIWiaItem,&cEnum) == S_OK) {
        if (pIWiaItem != NULL) {
            DisplayItemName(pIWiaItem);
        } else
            return E_FAIL;

        //
        // find out if the item is a folder, if it is,
        // recursive enumerate
        //

        pIWiaItem->GetItemType(&lType);
        if (lType & WiaItemTypeFolder) {
            IEnumWiaItem *pEnumNext;
            hResult = pIWiaItem->EnumChildItems(&pEnumNext);
            if (hResult == S_OK) {
                EnumNextLevel(pEnumNext);
                pEnumNext->Release();
            }
        }
    }
    return S_OK;
}
/**************************************************************************\
* DisplayItemName()
*
*   Displays the FULL ITEM NAME, of the target item
*
*
*
* Arguments:
*
*   none
*
*
* Return Value:
*
*   void
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
void DisplayItemName(IWiaItem   *pIWiaItem)
{
    HRESULT hr = S_OK;
    BSTR bstrItemName = NULL;
    IWiaPropertyStorage  *pIWiaPropStg = NULL;

    if(pIWiaItem != NULL){
        hr = pIWiaItem->QueryInterface(IID_IWiaPropertyStorage,(void **)&pIWiaPropStg);
        if (hr == S_OK) {
            hr = ReadPropStr(WIA_IPA_FULL_ITEM_NAME,pIWiaPropStg,&bstrItemName);
            if(hr == S_OK){
                printf(TEXT("Item Name = %ws\n"),bstrItemName);
                SysFreeString(bstrItemName);
            }
            else{
                printf(TEXT("ReadPropStr(WIA_IPA_FULL_ITEM_NAME) failed\n"));
                printf(TEXT("Item Name = (NULL)\n"));
            }
            pIWiaPropStg->Release();
        }
        else
            printf(TEXT("QI for IWiaPropertyStorage failed\n"));
    }
}
/**************************************************************************\
* CreateThisDevice()
*
*   Creates a WIA Device from the specified Device ID
*
*
*
* Arguments:
*
*   DeviceID - Target device ID
*
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
BOOL CreateThisDevice(BSTR DeviceID)
{
    //
    // Create a WIA Device (IWiaItem* - pIWiaRootItem)
    //

    HRESULT hr = S_OK;

    hr = g_pIWiaDevMgr->CreateDevice(g_bstrDeviceID,&g_pIWiaRootItem);
    if (hr != S_OK) {
        printf(TEXT("Failed to Create Device, possible invalid DeviceID\n"));
        return FALSE;
    }
    return TRUE;
}
/**************************************************************************\
* CreateWIADeviceManager()
*
*   Creates a WIA Device manager
*
*
*
* Arguments:
*
*   none
*
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
BOOL CreateWIADeviceManager()
{
    HRESULT hr = S_OK;
    g_pIWiaDevMgr = NULL;
    hr = CoCreateInstance(CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER,
                          IID_IWiaDevMgr,(void**)&g_pIWiaDevMgr);
    if (SUCCEEDED(hr))
        return TRUE;
    else
        return FALSE;
}

/**************************************************************************\
* DoIdtGetDataTransfer
*
*   Executes an IWiaData Transfer on an item
*
*
* Arguments:
*
*   pIWiaItem - target item
*   Tymed - TYMED value
*   WIAFormat - current format
*
* Return Value:
*
*    status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
BOOL DoIdtGetDataTransfer(IWiaItem *pIWiaItem, DWORD Tymed, GUID WIAFormat)
{
    HRESULT hr = S_OK;

    //
    // Check Item pointer
    //

    if (pIWiaItem == NULL) {
        printf(TEXT("pIWiaItem is NULL\n"));
        return FALSE;
    }

    //
    // set the two properties (TYMED, and CF_ )
    //
    IWiaPropertyStorage *pIWiaPropStg;
    PROPSPEC PropSpec;
    hr = pIWiaItem->QueryInterface(IID_IWiaPropertyStorage,(void **)&pIWiaPropStg);
    if (hr != S_OK) {
        printf(TEXT("pIWiaItem->QueryInterface() Failed\n"));
        return FALSE;
    } else {

        //
        // Write property value for TYMED
        //

        PropSpec.propid = WIA_IPA_TYMED;
        hr = WritePropLong(WIA_IPA_TYMED,pIWiaPropStg,Tymed);
        if (SUCCEEDED(hr))
            printf(TEXT("TYMED was successfully written...\n"));
        else{
            printf(TEXT("WritePropLong(WIA_IPA_TYMED) failed\n"));
            return FALSE;
        }

        //
        // Write property value for FORMAT
        //

        hr = WritePropGUID(WIA_IPA_FORMAT,pIWiaPropStg,WIAFormat);
        if (hr == S_OK)
            printf(TEXT("Format was successfully written...\n"));
        else{
            printf(TEXT("WritePropGUID(WIA_IPA_FORMAT) failed\n"));
            return FALSE;
        }
    }
    printf(TEXT("Executing an IWiaData Transfer...\n"));

    //
    // get IWiaDatatransfer interface
    //

    IWiaDataTransfer *pIWiadataTransfer = NULL;
    hr = pIWiaItem->QueryInterface(IID_IWiaDataTransfer, (void **)&pIWiadataTransfer);
    if (SUCCEEDED(hr)) {

        //
        // set storage medium
        //

        STGMEDIUM StgMedium;
        StgMedium.tymed    = Tymed;
        StgMedium.lpszFileName   = NULL;
        StgMedium.pUnkForRelease = NULL;
        StgMedium.hGlobal        = NULL;

        //
        // create Data callback Interface
        //

        IWiaDataCallback* pIWiaDataCallback = NULL;
        CWiaDataCallback* pCWiaDataCB = new CWiaDataCallback();
        if (pCWiaDataCB) {
            hr = pCWiaDataCB->QueryInterface(IID_IWiaDataCallback,(void **)&pIWiaDataCallback);
            if (hr == S_OK) {

                //
                // Initialize Callback Interface
                //

                pCWiaDataCB->Initialize();

                //
                // Execute transfer call
                //

                hr = pIWiadataTransfer->idtGetData(&StgMedium,pIWiaDataCallback);

                //
                // Release IWiadataTransfer Interface
                //

                pIWiadataTransfer->Release();

                //
                // Release Callback Interface
                //

                pCWiaDataCB->Release();
                if (SUCCEEDED(hr)) {
                    if (Tymed == TYMED_FILE) {

                        //
                        // Rename file using set file name
                        //

                        printf(TEXT("Done!\n"));
                        if (MoveTempFile(StgMedium.lpszFileName,g_szFileName)) {
                            printf(TEXT("IWiaDataTransfer ( Saving %s )\n"),g_szFileName);
                        }
                    }

                    //
                    // Release storage medium
                    //

                    ReleaseStgMedium(&StgMedium);
                } else{
                    printf(TEXT("idtGetData() failed\n"));
                    return FALSE;
                }
            } else
                printf(TEXT("QueryInterface(IID_IWiaDataCallback) failed\n"));
        } else
            printf(TEXT("Callback interface object failed to create\n"));
    } else
        printf(TEXT("QueryInterface(IID_IWiaDataTransfer) failed\n"));
    return TRUE;
}


/////////////////////////
//   Helper functions  //
/////////////////////////

/**************************************************************************\
* ReadPropStr
*
*   Reads a BSTR value of a target property
*
*
* Arguments:
*
*   propid - property ID
*   pIWiaPropStg - property storage
*   pbstr - returned BSTR read from property
*
* Return Value:
*
*    status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT ReadPropStr(PROPID propid,IWiaPropertyStorage  *pIWiaPropStg,BSTR *pbstr)
{
    HRESULT     hr = S_OK;
    PROPSPEC    PropSpec;
    PROPVARIANT PropVar;
    UINT        cbSize = 0;

    *pbstr = NULL;

    //
    // memset PropVar
    //

    memset(&PropVar, 0, sizeof(PropVar));

    //
    // Set PropSpec
    //

    PropSpec.ulKind = PRSPEC_PROPID;
    PropSpec.propid = propid;

    //
    // call ReadMultiple
    //

    hr = pIWiaPropStg->ReadMultiple(1, &PropSpec, &PropVar);
    if (SUCCEEDED(hr)) {
        if (PropVar.pwszVal) {
            *pbstr = SysAllocString(PropVar.pwszVal);
        } else {
            *pbstr = SysAllocString(L"");
        }
        if (*pbstr == NULL) {
            hr = E_OUTOFMEMORY;
        }

        //
        // clear the prop variant
        //

        PropVariantClear(&PropVar);

    } else {
        printf(TEXT("ReadPropStr, ReadMultiple of propid: %d, Failed"), propid);
    }
    return hr;
}
/**************************************************************************\
* WritePropStr
*
*   Writes a BSTR value to a target property
*
*
* Arguments:
*
*   propid - property ID
*   pIWiaPropStg - property storage
*   pbstr - BSTR to write to target property
*
* Return Value:
*
*    status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT WritePropStr(PROPID propid, IWiaPropertyStorage  *pIWiaPropStg, BSTR bstr)
{
#define MIN_PROPID 2

    PROPSPEC    propspec;
    PROPVARIANT propvar;

    //
    // Set the PropSpec
    //

    propspec.ulKind = PRSPEC_PROPID;
    propspec.propid = propid;

    //
    // Set the PropVar
    //

    propvar.vt      = VT_BSTR;
    propvar.pwszVal = bstr;

    //
    // call WriteMultiple
    //

    return pIWiaPropStg->WriteMultiple(1, &propspec, &propvar, MIN_PROPID);
}

/**************************************************************************\
* WritePropLong
*
*   Writes a LONG value of a target property
*
*
* Arguments:
*
*   propid - property ID
*   pIWiaPropStg - property storage
*   lVal - LONG to be written to target property
*
* Return Value:
*
*    status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT WritePropLong(PROPID propid, IWiaPropertyStorage *pIWiaPropStg, LONG lVal)
{
    PROPSPEC    propspec;
    PROPVARIANT propvar;

    //
    // Set the PropSpec
    //

    propspec.ulKind = PRSPEC_PROPID;
    propspec.propid = propid;

    //
    // Set the PropVar
    //

    propvar.vt   = VT_I4;
    propvar.lVal = lVal;

    //
    // call WriteMultiple
    //

    return pIWiaPropStg->WriteMultiple(1, &propspec, &propvar, MIN_PROPID);
}
/**************************************************************************\
* ReadPropLong
*
*   Reads a long value from a target property
*
*
* Arguments:
*
*   propid - property ID
*   pIWiaPropStg - property storage
*   plval - returned long read from property
*
* Return Value:
*
*    status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT ReadPropLong(PROPID propid, IWiaPropertyStorage  *pIWiaPropStg, LONG *plval)
{
    HRESULT           hr = S_OK;
    PROPSPEC          PropSpec;
    PROPVARIANT       PropVar;
    UINT              cbSize = 0;

    //
    // memest the PropVAR
    //

    memset(&PropVar, 0, sizeof(PropVar));

    //
    // Set the PropSpec
    //

    PropSpec.ulKind = PRSPEC_PROPID;
    PropSpec.propid = propid;

    //
    // Call ReadMultiple
    //

    hr = pIWiaPropStg->ReadMultiple(1, &PropSpec, &PropVar);

    //
    // if it succeeded, write the value to the pointer
    //

    if (SUCCEEDED(hr))
        *plval = PropVar.lVal;

    return hr;
}

/**************************************************************************\
* WritePropGUID
*
*   Writes a GUID value of a target property
*
*
* Arguments:
*
*   propid - property ID
*   pIWiaPropStg - property storage
*   guidVal - GUID to be written to target property
*
* Return Value:
*
*    status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
HRESULT WritePropGUID(PROPID propid, IWiaPropertyStorage *pIWiaPropStg, GUID guidVal)
{
    HRESULT     hResult;
    PROPSPEC    propspec[1];
    PROPVARIANT propvar[1];

    propspec[0].ulKind = PRSPEC_PROPID;
    propspec[0].propid = propid;

    propvar[0].vt   = VT_CLSID;
    propvar[0].puuid = &guidVal;

    hResult = pIWiaPropStg->WriteMultiple(1, propspec, propvar, MIN_PROPID);
    return hResult;
}
/**************************************************************************\
* MoveTempFile()
*
*   Copies the temporary file created by WIA to a new location, and
*   deletes the old temp file after copy is complete
*
*
* Arguments:
*
*   pTempFileName - Temporary file created by WIA
*       pTargetFileName - New file
*
* Return Value:
*
*   status
*
* History:
*
*    6/10/1999 Original Version
*
\**************************************************************************/
BOOL MoveTempFile(LPWSTR pwszTempFileName, LPCTSTR szTargetFileName)
{
    TCHAR buffer[MAX_PATH];
    sprintf(buffer,TEXT("%ws"),pwszTempFileName);
    if(CopyFile(buffer,szTargetFileName,FALSE)){
        DeleteFile(buffer);
    }
    else{
        printf(TEXT(" Failed to copy temp file..\n"));
        return FALSE;
    }
    return TRUE;
}

/////////////////////////
// Data Callback class //
/////////////////////////

/**************************************************************************\
* CWiaDataCallback::QueryInterface()
*
*   QI for IWiadataCallback Interface
*
*
* Arguments:
*
*   iid - Interface ID
*   ppv - Callback Interface pointer
*
* Return Value:
*
*    none
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT WINAPI CWiaDataCallback::QueryInterface(const IID& iid, void** ppv)
{
    *ppv = NULL;
    if (iid == IID_IUnknown || iid == IID_IWiaDataCallback)
        *ppv = (IWiaDataCallback*) this;
    else
        return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

/**************************************************************************\
* CWiaDataCallback::AddRef()
*
*   Increment the Ref count
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*    ULONG - current ref count
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
ULONG   WINAPI CWiaDataCallback::AddRef()
{
    InterlockedIncrement((LONG*) &m_cRef);
    return m_cRef;
}

/**************************************************************************\
* CWiaDataCallback::Release()
*
*   Release the callback Interface
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*   ULONG - Current Ref count
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
ULONG   WINAPI CWiaDataCallback::Release()
{
    ULONG ulRefCount = m_cRef - 1;
    if (InterlockedDecrement((LONG*) &m_cRef) == 0)
    {
        delete this;
        return 0;
    }
    return ulRefCount;
}

/**************************************************************************\
* CWiaDataCallback::CWiaDataCallback()
*
*   Constructor for callback class
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*    none
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
CWiaDataCallback::CWiaDataCallback()
{
    m_cRef              = 0;
    m_pBuffer           = NULL;
    m_BytesTransfered   = 0;
}

/**************************************************************************\
* CWiaDataCallback::~CWiaDataCallback()
*
*   Destructor for Callback class
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*    none
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
CWiaDataCallback::~CWiaDataCallback()
{
    if (m_pBuffer != NULL)
    {
        LocalFree(m_pBuffer);
        m_pBuffer = NULL;
    }
}

/**************************************************************************\
* CWiaDataCallback::Initialize()
*
*   Initializes Progress control.
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*    none
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT WINAPI CWiaDataCallback::Initialize()
{
    // "Starting Transfer"
    return S_OK;
}

/**************************************************************************\
* CWiaDataCallback::DisplayWindow()
*
*   This does nothing yet
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*    none
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT WINAPI CWiaDataCallback::DisplayWindow()
{
    return S_OK;
}

/**************************************************************************\
* CWiaDataCallback::BandedDataCallback()
*
*   Callback member which handles Banded Data transfers
*
*
* Arguments:
*
*   lMessage - callback message
*   lStatus - additional message information
*   lPercentComplete - current percent complete status
*   lOffset - amount of data offset (bytes)
*   lLength - amount of data read (bytes)
*   lReserved - not used
*   lResLength - not used
*   pbBuffer - Data header information
*
* Return Value:
*
*    status
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
HRESULT WINAPI CWiaDataCallback::BandedDataCallback(
       LONG                            lMessage,
       LONG                            lStatus,
       LONG                            lPercentComplete,
       LONG                            lOffset,
       LONG                            lLength,
       LONG                            lReserved,
       LONG                            lResLength,
       BYTE*                           pbBuffer)
{
    switch (lMessage)
    {
    case IT_MSG_DATA_HEADER:
        {
            OutputDebugString(TEXT("Reading Header information\n"));
            //data header contains bitmap final size
            PWIA_DATA_CALLBACK_HEADER pHeader = (PWIA_DATA_CALLBACK_HEADER)pbBuffer;
            m_pBuffer = (PBYTE)LocalAlloc(LPTR,pHeader->lBufferSize);
            m_BufferLength = pHeader->lBufferSize;
            m_BytesTransfered   = 0;
            m_guidFormatID      = pHeader->guidFormatID;
            //OutputDebugString("Header info:\n");
            //OutputDebugString("   lBufferSize = %li\n",pHeader->lBufferSize);
            //OutputDebugString("   lFormat = %li\n",pHeader->lFormat);
            //OutputDebugString("   BytesTransferred = %li\n",m_BytesTransfered);
        }
        break;

    case IT_MSG_DATA:
        {
            if (m_pBuffer != NULL)
            {
                TCHAR tBuffer[MAX_PATH];
                wsprintf(tBuffer,TEXT(TEXT("%i out of %li")),lOffset,m_BufferLength);
                // m_pMainFrm->SetProgressText(tBuffer);
                printf(".");
                // m_pMainFrm->UpdateProgress(lPercentComplete);
                //OutputDebugString("writing %li\n",lLength);
                memcpy(m_pBuffer + lOffset, pbBuffer,   lLength);
                m_BytesTransfered += lLength;
                //OutputDebugString("lOffset = %li, lLength = %li, BytesTransferred = %li\n",lOffset,lLength,m_BytesTransfered);
           }
        }
        break;

    case IT_MSG_STATUS:
        {
            if (lStatus & IT_STATUS_TRANSFER_FROM_DEVICE)
            {
                // m_pMainFrm->SetProgressText(TEXT("Transfer from device"));
                // m_pMainFrm->UpdateProgress(lPercentComplete);
                printf(".");
            }
            else if (lStatus & IT_STATUS_PROCESSING_DATA)
            {
                // m_pMainFrm->SetProgressText(TEXT("Processing Data"));
                // m_pMainFrm->UpdateProgress(lPercentComplete);
                printf(".");
            }
            else if (lStatus & IT_STATUS_TRANSFER_TO_CLIENT)
            {
                // m_pMainFrm->SetProgressText(TEXT("Transfer to Client"));
                // m_pMainFrm->UpdateProgress(lPercentComplete);
                printf(".");
            }
        }
        break;
    }
   return S_OK;
}
/**************************************************************************\
* CWiaDataCallback::GetDataPtr()
*
*   Returns the memory acquired during a transfer
*
*
* Arguments:
*
*   none
*
* Return Value:
*
*    BYTE* pBuffer - memory block
*
* History:
*
*    6/11/1999 Original Version
*
\**************************************************************************/
// GetDataPtr
BYTE* WINAPI CWiaDataCallback::GetDataPtr()
{
    return m_pBuffer;
}

