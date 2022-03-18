/*++

Copyright (c) 1990-2000 Microsoft Corporation All rights Reserved

Module Name:

    TOAST.C

Abstract:

    Lists all the toaster device and interfaces present in the system 
    and opens the last interface to send an invalidate device Ioctl request
    or read requests.

Environment:

    usermode console application

Revision History:

  Eliyas Yakub  Nov 2, 1998
    

--*/

#include <basetyps.h>
#include <stdlib.h>
#include <wtypes.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdio.h>
#include <string.h>
#include <winioctl.h>
#include "..\inc\public.h"
#include <conio.h>

#define USAGE  \
"Usage: Toast <-h> {-h option causes the device to hide from Device Manager UI}\n"

BOOL PrintToasterDeviceInfo();

int _cdecl main (int argc, char *argv[])
{
    HDEVINFO                            hardwareDeviceInfo;
    SP_INTERFACE_DEVICE_DATA            deviceInterfaceData;
    PSP_INTERFACE_DEVICE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0, bytes=0;
    HANDLE                              file;
    int                                 i, ch;
    char                                buffer[10];
    BOOL                                bHide = FALSE;

    if(argc == 2) {
        if(argv[1][0] == '-') {
            if(argv[1][1] == 'h' || argv[1][1] == 'H') {
                bHide = TRUE;
            } else {
                printf(USAGE);
                exit(0);
            }
        }
        else {
            printf(USAGE);
            exit(0);
        }
    }


    //
    // Print a list of devices of Toaster Class
    // 
    if(!PrintToasterDeviceInfo())
    {
        printf("No toaster devices present\n");
        return 0;
    }
    
    //
    // Open a handle to the device interface information set of all 
    // present toaster class interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&GUID_TOASTER_DEVICE_INTERFACE_CLASS,
                       NULL, // Define no enumerator (global)
                       NULL, // Define no
                       (DIGCF_PRESENT | // Only Devices present
                       DIGCF_INTERFACEDEVICE)); // Function class devices.
    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        printf("SetupDiGetClassDevs failed: %x\n", GetLastError());
        return 0;
    }
    
    deviceInterfaceData.cbSize = sizeof (SP_INTERFACE_DEVICE_DATA);

    printf("\nList of Toaster Device Interfaces\n");
    printf("---------------------------------\n");

    i = 0;

    //
    // Enumerate devices of toaster class
    //
    
    do {
        if (SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                 0, // No care about specific PDOs
                                 (LPGUID)&GUID_TOASTER_DEVICE_INTERFACE_CLASS,
                                 i, //
                                 &deviceInterfaceData)) {
                                 
            if(deviceInterfaceDetailData)
                free (deviceInterfaceDetailData);
            //
            // Allocate a function class device data structure to 
            // receive the information about this particular device.
            //

            //
            // First find out required length of the buffer
            //
            
            SetupDiGetInterfaceDeviceDetail (
                    hardwareDeviceInfo,
                    &deviceInterfaceData,
                    NULL, // probing so no output buffer yet
                    0, // probing so output buffer length of zero
                    &requiredLength,
                    NULL); // not interested in the specific dev-node


            predictedLength = requiredLength;

            deviceInterfaceDetailData = malloc (predictedLength);
            deviceInterfaceDetailData->cbSize = 
                            sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);


            if (! SetupDiGetInterfaceDeviceDetail (
                       hardwareDeviceInfo,
                       &deviceInterfaceData,
                       deviceInterfaceDetailData,
                       predictedLength,
                       &requiredLength,
                       NULL)) {
                printf("Error in SetupDiGetInterfaceDeviceDetail\n");
                free (deviceInterfaceDetailData);
                return FALSE;
            }
            printf("%d) %s\n", ++i,
                    deviceInterfaceDetailData->DevicePath);
        }
        else if (ERROR_NO_MORE_ITEMS != GetLastError()) {
            free (deviceInterfaceDetailData);
            deviceInterfaceDetailData = NULL;
            continue;
        }
        else 
            break;

    } while (TRUE); 


    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);

    if(!i)
    {
        printf("No device interfaces present\n");
        return 0;
    }

    //
    // Open the last toaster device interface
    //

    printf("\nOpening the last interface:\n %s\n", 
                    deviceInterfaceDetailData->DevicePath);
    
    file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL);

    if (INVALID_HANDLE_VALUE == file) {
        printf("Error in CreateFile: %x", GetLastError());
        free (deviceInterfaceDetailData);
        return 0;
    }

    //
    // Invalidate the Device State
    //
        
    if(bHide)
    {
        if (!DeviceIoControl (file,
                              IOCTL_TOASTER_DONT_DISPLAY_IN_UI_DEVICE,
                              NULL, 0,
                              NULL, 0,
                              &bytes, NULL)) {
            printf("Invalidate device request failed:0x%x\n", GetLastError());
            free (deviceInterfaceDetailData);            
            CloseHandle(file);
            return 0;
        }
        printf("\nRequest to hide the device completed successfully\n");
       
    }
    

    // 
    // Read/Write to the toaster device.
    //
    
    printf("\nPress 'q' to exit, any other key to read...\n");
    fflush(stdin);
    ch = _getche();

    while(tolower(ch) != 'q' )
    {
    
        if(!ReadFile(file, buffer, sizeof(buffer), &bytes, NULL))
        {
            printf("Error in ReadFile: %x", GetLastError());
            free (deviceInterfaceDetailData);
        }
        printf("Read Successful\n");
        ch = _getche();
    }

    free (deviceInterfaceDetailData);
    CloseHandle(file);
    return 0;
}



BOOL 
PrintToasterDeviceInfo()
{
#define MAX_COMP_INSTID 2096
#define MAX_COMP_DESC   2096
#define MAX_FRIENDLY    2096
    HDEVINFO        hdi;
    DWORD           dwIndex=0;
    SP_DEVINFO_DATA deid;
    BOOL            fSuccess=FALSE;
    PCHAR           szCompInstanceId[MAX_COMP_INSTID];
    PCHAR           szCompDescription[MAX_COMP_DESC];
    PCHAR           szFriendlyName[MAX_FRIENDLY];
    DWORD           dwRegType;
    BOOL            fFound=FALSE;

    // get a list of all devices of class 'GUID_TOASTER_SETUP_CLASS'
    hdi = SetupDiGetClassDevs(&GUID_TOASTER_SETUP_CLASS, NULL, NULL, 
                                            DIGCF_PRESENT);

    if (INVALID_HANDLE_VALUE != hdi)
    {

        // enumerate over each device
        while (deid.cbSize = sizeof(SP_DEVINFO_DATA),
               SetupDiEnumDeviceInfo(hdi, dwIndex, &deid))
        {
            dwIndex++;

            // the right thing to do here would be to call this function
            // to get the size required to hold the instance ID and then
            // to call it second time with a buffer large enough for that size.
            // However, that would tend to obscure the control flow in
            // the sample code. Lets keep things simple by keeping the
            // buffer large enough.

            // get the device instance ID
            fSuccess = SetupDiGetDeviceInstanceId(hdi, &deid,
                                                  (BYTE*)szCompInstanceId,
                                                  MAX_COMP_INSTID, NULL);
            if (fSuccess)
            {
                // get the description for this instance
                fSuccess =
                    SetupDiGetDeviceRegistryProperty(hdi, &deid,
                                                     SPDRP_DEVICEDESC,
                                                     &dwRegType,
                                                     (BYTE*) szCompDescription,
                                                     MAX_COMP_DESC,
                                                     NULL);
                if (fSuccess)
                {
                    memset(szFriendlyName, 0, MAX_FRIENDLY);
                    SetupDiGetDeviceRegistryProperty(hdi, &deid,
                                                     SPDRP_FRIENDLYNAME,
                                                     &dwRegType,
                                                     (BYTE*) szFriendlyName,
                                                     MAX_FRIENDLY,
                                                     NULL);
                    fFound = TRUE;
                    printf("Instance ID : %s\n", szCompInstanceId);
                    printf("Description : %s\n", szCompDescription);
                    printf("FriendlyName: %s\n\n", szFriendlyName);
                }
            }
        }

        // release the device info list
        SetupDiDestroyDeviceInfoList(hdi);
    }

    if(fFound)
        return TRUE;
    else 
        return FALSE;
}


