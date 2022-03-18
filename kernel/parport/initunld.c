/*++

Copyright (C) Microsoft Corporation, 1998 - 1999

File Name:

    initunld.c

Module Name:

    parport.sys

Abstract:

    This file contains routines for initializing, cleaning up, and 
      unloading the driver.

Major Functions:

    DriverEntry()        - First driver routine called after driver load to
                             to initialize the driver.

    PptUnload()          - Last driver routine called prior to driver unload

--*/

#include "pch.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING pRegistryPath
    )

/*++

Routine Description:

    This routine is called at system initialization time to initialize
    this driver.

Arguments:

    DriverObject    - Supplies the driver object.

    pRegistryPath    - Supplies the registry path for this driver.

Return Value:

    STATUS_SUCCESS

--*/

{
    //
    // Initialize driver settings from registry
    //
    PptRegInitDriverSettings( pRegistryPath );


    //
    // In a CHECKED driver, Break if the user set a registry value:
    //   HKLM\SYSTEM\CurrentControlSet\Services\Parport - PptBreakOn : REG_DWORD : 1
    //
    // See debug.h for PAR_BREAK_ON_* bit definitions
    //
#if DBG
    PptBreak(PAR_BREAK_ON_DRIVER_ENTRY, ("DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING)\n") );
#endif


    //
    // Initialize Driver Globals
    //
    RegistryPath.Buffer = ExAllocatePool( PagedPool, pRegistryPath->MaximumLength );
    if( NULL == RegistryPath.Buffer ) {
        PptDumpV( ("initunld::DriverEntry - unable to alloc space to hold RegistryPath\n") );
        return STATUS_INSUFFICIENT_RESOURCES;
    } else {
        RtlZeroMemory( RegistryPath.Buffer, pRegistryPath->MaximumLength );
        RegistryPath.Length        = pRegistryPath->Length;
        RegistryPath.MaximumLength = pRegistryPath->MaximumLength;
        RtlMoveMemory( RegistryPath.Buffer, pRegistryPath->Buffer, pRegistryPath->Length );
    }


    //
    // Initialize function call table for handling PnP IRPs
    //
    PptPnpInitDispatchFunctionTable();


    //
    // Initialize the Driver Object with our driver's entry points.
    //
    DriverObject->MajorFunction[IRP_MJ_CREATE]                  = PptDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                   = PptDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = PptDispatchInternalDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                 = PptDispatchCleanup;
    DriverObject->MajorFunction[IRP_MJ_PNP]                     = PptDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER]                   = PptDispatchPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = PptDispatchSystemControl;
    DriverObject->DriverExtension->AddDevice                    = PptPnpAddDevice;
    DriverObject->DriverUnload                                  = PptUnload;

    return STATUS_SUCCESS;
}

VOID
PptUnload(
    IN  PDRIVER_OBJECT  DriverObject
    )

/*++
      
Routine Description:
      
    This routine cleans up all of the memory associated with
      any of the devices belonging to the driver.  It  will
      loop through the device list.
      
Arguments:
      
    DriverObject    - Supplies the driver object controlling all of the
                        devices.
      
Return Value:
      
    None.
      
--*/
    
{
    PDEVICE_OBJECT                  CurrentDevice;
    PDEVICE_EXTENSION               Extension;
    PLIST_ENTRY                     Head;
    PISR_LIST_ENTRY                 Entry;
    
    PptDump2(PARUNLOAD, ("PptUnload()\n") );

    CurrentDevice = DriverObject->DeviceObject;

    while( CurrentDevice ) {
        
        Extension = CurrentDevice->DeviceExtension;
        
        if (Extension->InterruptRefCount) {
            PptDisconnectInterrupt(Extension);
        }
        
        while (!IsListEmpty(&Extension->IsrList)) {
            Head = RemoveHeadList(&Extension->IsrList);
            Entry = CONTAINING_RECORD(Head, ISR_LIST_ENTRY, ListEntry);
            ExFreePool(Entry);
        }
        
        ExFreePool(Extension->DeviceName.Buffer);

        IoDeleteDevice(CurrentDevice);
        
        IoGetConfigurationInformation()->ParallelCount--;

        CurrentDevice = DriverObject->DeviceObject;
    }
    
    if( PortInfoMutex ) {
        ExFreePool( PortInfoMutex );
        PortInfoMutex = NULL;
    }

    RtlFreeUnicodeString( &RegistryPath );
}

