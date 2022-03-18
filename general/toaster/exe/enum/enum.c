/*++

Copyright (c) 1990-2000 Microsoft Corporation All rights Reserved

Module Name:

    Enum.c

Abstract:
        This application simulates the plugin, unplug or ejection
        of devices.

Environment:

    usermode console application

Revision History:

  Eliyas Yakub  Oct 14, 1998
    

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


//
// Prototypes
//

BOOLEAN
OpenBusInterface (
    IN       HDEVINFO                    HardwareDeviceInfo,
    IN       PSP_INTERFACE_DEVICE_DATA   DeviceInterfaceData
    );



#define USAGE  \
"Usage: Enum [-p SerialNo] Plugs in a device. SerialNo must be greater than zero.\n\
             [-u SerialNo or 0] Unplugs device(s) - specify 0 to unplug all \
                                the devices enumerated so far.\n\
             [-e SerialNo or 0] Ejects device(s) - specify 0 to eject all \
                                the devices enumerated so far.\n"

BOOLEAN     bPlugIn, bUnplug, bEject;
ULONG       SerialNo;

int _cdecl main (int argc, char *argv[])
{
    HDEVINFO                    hardwareDeviceInfo;
    SP_INTERFACE_DEVICE_DATA    deviceInterfaceData;

    bPlugIn = bUnplug = FALSE;

    if(argc <3) {
        goto usage;
    }

    if(argv[1][0] == '-') {
        if(tolower(argv[1][1]) == 'p') {
            if(argv[2])
                SerialNo = (USHORT)atoi(argv[2]);
        bPlugIn = TRUE;
        }
        else if(tolower(argv[1][1]) == 'u') {
            if(argv[2])
                SerialNo = (ULONG)atoi(argv[2]); 
            bUnplug = TRUE;
        }
        else if(tolower(argv[1][1]) == 'e') {
            if(argv[2])
                SerialNo = (ULONG)atoi(argv[2]); 
            bEject = TRUE;
        }
        else {
            goto usage;
        }
    }
    else
        goto usage;

    if(bPlugIn && 0 == SerialNo)
        goto usage;
    //
    // Open a handle to the device interface information set of all 
    // present toaster bus enumerator interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&GUID_TOASTER_BUS_ENUMERATOR_INTERFACE_CLASS,
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

    if (SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                 0, // No care about specific PDOs
                                 (LPGUID)&GUID_TOASTER_BUS_ENUMERATOR_INTERFACE_CLASS,
                                 0, //
                                 &deviceInterfaceData)) {

        OpenBusInterface(hardwareDeviceInfo, &deviceInterfaceData);
    } else if (ERROR_NO_MORE_ITEMS == GetLastError()) {
    
        printf(
        "Error:Interface GUID_TOASTER_BUS_ENUMERATOR_INTERFACE_CLASS is not registered\n");
    }

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return 0;
usage: 
    printf(USAGE);
    exit(0);
}

BOOLEAN
OpenBusInterface (
    IN       HDEVINFO                    HardwareDeviceInfo,
    IN       PSP_INTERFACE_DEVICE_DATA   DeviceInterfaceData
    )
{
    HANDLE                              file;
    PSP_INTERFACE_DEVICE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;
    ULONG                               bytes;
    BUSENUM_UNPLUG_HARDWARE             unplug;
    BUSENUM_EJECT_HARDWARE              eject;
    PBUSENUM_PLUGIN_HARDWARE            hardware;

    //
    // Allocate a function class device data structure to receive the
    // information about this particular device.
    //
    
    SetupDiGetInterfaceDeviceDetail (
            HardwareDeviceInfo,
            DeviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node


    predictedLength = requiredLength;

    deviceInterfaceDetailData = malloc (predictedLength);
    deviceInterfaceDetailData->cbSize = 
                    sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);

    
    if (! SetupDiGetInterfaceDeviceDetail (
               HardwareDeviceInfo,
               DeviceInterfaceData,
               deviceInterfaceDetailData,
               predictedLength,
               &requiredLength,
               NULL)) {
        printf("Error in SetupDiGetInterfaceDeviceDetail\n");
        free (deviceInterfaceDetailData);
        return FALSE;
    }

    printf("Opening %s\n", deviceInterfaceDetailData->DevicePath);

    file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL); // No template file

    if (INVALID_HANDLE_VALUE == file) {
        printf("Device not ready: %x", GetLastError());
        free (deviceInterfaceDetailData);
        return FALSE;
    }
    
    printf("Bus interface opened!!!\n");


    //
    // Enumerate Devices
    //
    
    if(bPlugIn) {

        printf("SerialNo. of the device to be enumerated: %d\n", SerialNo);

        hardware = malloc (bytes = (sizeof (BUSENUM_PLUGIN_HARDWARE) +
                                              BUS_HARDWARE_IDS_LENGTH));

        hardware->Size = sizeof (BUSENUM_PLUGIN_HARDWARE);
        hardware->SerialNo = SerialNo;

        //
        // Allocate storage for the Device ID
        //
        
        memcpy (hardware->HardwareIDs,
                BUS_HARDWARE_IDS,
                BUS_HARDWARE_IDS_LENGTH);

        if (!DeviceIoControl (file,
                              IOCTL_BUSENUM_PLUGIN_HARDWARE ,
                              hardware, bytes,
                              hardware, bytes,
                              &bytes, NULL)) {
              free (hardware);
              printf("PlugIn failed:0x%x\n", GetLastError());
              goto End;
        }

        free (hardware);
    }

    //
    // Removes a device if given the specific Id of the device. Otherwise this
    // ioctls removes all the devices that are enumerated so far.
    //
    
    if(bUnplug) {
        printf("Unplugging device(s)....\n");

        unplug.Size = bytes = sizeof (unplug);
        unplug.SerialNo = SerialNo;
        if (!DeviceIoControl (file,
                              IOCTL_BUSENUM_UNPLUG_HARDWARE,
                              &unplug, bytes,
                              &unplug, bytes,
                              &bytes, NULL)) {
            printf("Unplug failed: 0x%x\n", GetLastError());
            goto End;
        }
    }

    //
    // Ejects a device if given the specific Id of the device. Otherwise this
    // ioctls ejects all the devices that are enumerated so far.
    //

    if(bEject)
    {
        printf("Ejecting Device(s)\n");

        eject.Size = bytes = sizeof (eject);
        eject.SerialNo = SerialNo;
        if (!DeviceIoControl (file,
                              IOCTL_BUSENUM_EJECT_HARDWARE,
                              &eject, bytes,
                              &eject, bytes,
                              &bytes, NULL)) {
            printf("Eject failed: 0x%x\n", GetLastError());
            goto End;
        }
    }

    printf("Success!!!\n");

End:
    CloseHandle(file);
    free (deviceInterfaceDetailData);
    return TRUE;
}



