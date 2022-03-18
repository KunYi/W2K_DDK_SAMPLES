// 45678901234567890123456789012345678901234567890123456789012345678901234567890
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1999.
//
//  File:       COINST.C
//
//  Contents:   co-installer hook.
//
//  Notes:      For a complete description of CoInstallers, please see the 
//                 Microsoft Windows 2000 DDK Documentation
//
//  Author:     keithga   4 June 1999
//  
// Revision History:
//              Added FriendlyName interface (Eliyas Yakub Aug 2, 1999)
//
//----------------------------------------------------------------------------
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>

//+---------------------------------------------------------------------------
//
// WARNING! 
//
// A Coinstaller must not generate any popup to the user.
//     it should provide appropriate defaults.
//
//  OutputDebugString should be fine...
//
#if DBG
#define DbgOut(Text) OutputDebugString(TEXT("CoInstaller: " Text "\n"))
#else
#define DbgOut(Text) 
#endif 

//+---------------------------------------------------------------------------
//
//  Function:   MyOpenInfFile
//
//  Purpose:    Will open the handle to the INF file being installed
//
//  Arguments:
//      DeviceInfoSet     [in]
//      DeviceInfoData    [in]
//      ErrorLine         [out] // Optional, See SetupOpenInfFile
//
//  Returns:    HINF Handle of INF file
//
HINF MyOpenInfFile (               
                    IN     HDEVINFO          DeviceInfoSet,
                    IN     PSP_DEVINFO_DATA  DeviceInfoData,  OPTIONAL
                    OUT    PUINT             ErrorLine   OPTIONAL 
                    )
{
    SP_DRVINFO_DATA DriverInfoData;
    SP_DRVINFO_DETAIL_DATA DriverInfoDetailData;
    HRESULT Status;
    HINF FileHandle;
    
    DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);
    if (!SetupDiGetSelectedDriver( DeviceInfoSet,
        DeviceInfoData,
        &DriverInfoData))
    {
        DbgOut("Fail: SetupDiGetSelectedDriver");
        return INVALID_HANDLE_VALUE;
    }
    
    DriverInfoDetailData.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
    if (!SetupDiGetDriverInfoDetail(DeviceInfoSet,
        DeviceInfoData,
        &DriverInfoData,
        &DriverInfoDetailData,
        sizeof(SP_DRVINFO_DETAIL_DATA),
        NULL))
    {
        if ((Status = GetLastError()) == ERROR_INSUFFICIENT_BUFFER)
        {
            // We don't need the extended information.  Ignore.        
        }
        else
        {
            DbgOut("Fail: SetupDiGetDriverInfoDetail");
            return INVALID_HANDLE_VALUE;
        }
    }
    
    if (INVALID_HANDLE_VALUE == (FileHandle = SetupOpenInfFile( 
        DriverInfoDetailData.InfFileName,     
        NULL,
        INF_STYLE_WIN4,
        ErrorLine)))
    {
        DbgOut("Fail: SetupOpenInfFile");
    }
    return FileHandle;
}

//+---------------------------------------------------------------------------
//
//  Function:   MyCoInstaller
//
//  Purpose:    Responds to co-installer messages
//
//  Arguments:
//      InstallFunction   [in] 
//      DeviceInfoSet     [in]
//      DeviceInfoData    [in]
//      Context           [inout]
//
//  Returns:    NO_ERROR, ERROR_DI_POSTPROCESSING_REQUIRED, or an error code.
//
HRESULT
MyCoInstaller (
               IN     DI_FUNCTION               InstallFunction,
               IN     HDEVINFO                  DeviceInfoSet,
               IN     PSP_DEVINFO_DATA          DeviceInfoData,  OPTIONAL
               IN OUT PCOINSTALLER_CONTEXT_DATA Context
               )
{
    switch (InstallFunction)
    {
    case DIF_INSTALLDEVICE: 
        if(!Context->PostProcessing) 
        {
            DbgOut("DIF_INSTALLDEVICE");
            
            // You can use PrivateData to pass Data needed for PostProcessing
            // Context->PrivateData = Something;
            
            return ERROR_DI_POSTPROCESSING_REQUIRED; //Set for PostProcessing
        }
        else // post processing 
        {
            INFCONTEXT  InfContext;
            HINF        InfFile;
            TCHAR       FriendlyName[30];
            BOOL        fSuccess=FALSE;
            DWORD       dwRegType, UINumber;

            DbgOut("DIF_INSTALLDEVICE PostProcessing");
            
            if (INVALID_HANDLE_VALUE == (InfFile = 
                MyOpenInfFile(DeviceInfoSet,DeviceInfoData,NULL)))
            {
                return GetLastError();
            }
            
            if (SetupFindFirstLine(InfFile, // InfHandle 
                TEXT("MySection"),
                TEXT("MySpecialFlag"),
                &InfContext))
            {
                DbgOut("DIF_INSTALLDEVICE MySpecicalFlag, Do something here!");

            }                
            //
            // To give a friendly name to the device,
            // first get the UINumber.
            //
            fSuccess =
                    SetupDiGetDeviceRegistryProperty(DeviceInfoSet, 
                                                     DeviceInfoData,
                                                     SPDRP_UI_NUMBER,
                                                     &dwRegType,
                                                     (BYTE*) &UINumber,
                                                     sizeof(UINumber),
                                                     NULL);
                if (fSuccess)
                {
                    //
                    // Cook a FriendlyName and add it to the registry
                    //
                    wsprintf(FriendlyName, "ToasterDevice%02d", UINumber);
                    fSuccess = SetupDiSetDeviceRegistryProperty(DeviceInfoSet, 
                                 DeviceInfoData,
                                 SPDRP_FRIENDLYNAME,
                                 (BYTE*) &FriendlyName,
                                 (lstrlen(FriendlyName)+1) * sizeof(TCHAR)
                                 );
                    if(!fSuccess) {
                        DbgOut("SetupDiSetDeviceRegistryProperty failed!");                   
                    }
                }

        }              
        break;
        
    case DIF_REMOVE:
        DbgOut("DIF_REMOVE");

        // A Coinstaller could handle other types of DIF requests.
        
        // but not here, fall through...
        break;
    default:
        break;
    }
    
    return NO_ERROR;    
}


