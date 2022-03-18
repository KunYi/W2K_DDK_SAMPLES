/*++

Copyright (C) Microsoft Corporation, 1998 - 1999

File Name:

    openclos.c

Module Name:

    parport.sys

Abstract:

    This file contains routines for opening and closing a parport device.

Functions:

    PptDispatchCreate() - Dispatch function for IRP_MJ_CREATE

    PptDispatchClose()  - Dispatch function for IRP_MJ_CLOSE

--*/

#include "pch.h"

NTSTATUS
PptDispatchCreate(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
      
Routine Description:
      
    This is the dispatch function for IRP_MJ_CREATE.
      
Arguments:
      
    DeviceObject    - The target device object for the request.

    Irp             - The I/O request packet.
      
Return Value:
      
    STATUS_SUCCESS        - If Success.

    STATUS_DELETE_PENDING - If this device is in the process of being removed 
                              and will go away as soon as all outstanding
                              requests are cleaned up.
      
--*/
{
    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    NTSTATUS          status    = STATUS_SUCCESS;

    PAGED_CODE();

    //
    // Verify that our device has not been SUPRISE_REMOVED. Generally
    //   only parallel ports on hot-plug busses (e.g., PCMCIA) and
    //   parallel ports in docking stations will be surprise removed.
    //
    // dvdf - RMT - It would probably be a good idea to also check
    //   here if we are in a "paused" state (stop-pending, stopped, or
    //   remove-pending) and queue the request until we either return to
    //   a fully functional state or are removed.
    //
    if( extension->DeviceStateFlags & PPT_DEVICE_SURPRISE_REMOVED ) {
        PptDumpV( ("IRP_MJ_CREATE - FAIL - %S, DEVICE_SURPRISE_REMOVED\n", extension->PnpInfo.PortName) );
        return PptFailRequest( Irp, STATUS_DELETE_PENDING );
    }


    //
    // Try to acquire RemoveLock to prevent the device object from going
    //   away while we're using it.
    //
    status = PptAcquireRemoveLockOrFailIrp( DeviceObject, Irp );
    if ( !NT_SUCCESS(status) ) {
        PptDumpV( ("IRP_MJ_CREATE - FAIL - unable to acquire RemoveLock\n") );
        return status;
    }

    //
    // We have the RemoveLock - handle CREATE
    //
    ExAcquireFastMutex(&extension->OpenCloseMutex);
    InterlockedIncrement(&extension->OpenCloseRefCount);
    ExReleaseFastMutex(&extension->OpenCloseMutex);
    PptDumpV( ("IRP_MJ_CREATE - SUCCEED - %S, new OpenCloseRefCount=%d\n",
               extension->PnpInfo.PortName, extension->OpenCloseRefCount));

    DDPnP1(("-- Create - SUCCEED - %S, new OpenCloseRefCount=%d\n", extension->PnpInfo.PortName, extension->OpenCloseRefCount));

    PptReleaseRemoveLock(&extension->RemoveLock, Irp);

    Irp->IoStatus.Status      = status;
    Irp->IoStatus.Information = 0;
    PptCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS
PptDispatchClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++
      
Routine Description:
      
    This is the dispatch function for IRP_MJ_CLOSE.
      
Arguments:
      
    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.
      
Return Value:
      
    STATUS_SUCCESS  - Success - always.
      
--*/
    
{
    PDEVICE_EXTENSION   extension = DeviceObject->DeviceExtension;
    NTSTATUS            status;
    
    PAGED_CODE();

    //
    // Verify that our device has not been SUPRISE_REMOVED. Generally
    //   only parallel ports on hot-plug busses (e.g., PCMCIA) and
    //   parallel ports in docking stations will be surprise removed.
    //
    if( extension->DeviceStateFlags & PPT_DEVICE_SURPRISE_REMOVED ) {
        //
        // Our device has been SURPRISE removed, but since this is only 
        //   a CLOSE, SUCCEED anyway.
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        PptCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }


    //
    // Try to acquire RemoveLock to prevent the device object from going
    //   away while we're using it.
    //
    status = PptAcquireRemoveLock(&extension->RemoveLock, Irp);

    if( !NT_SUCCESS( status ) ) {
        //
        // Our device has been removed, but since this is only 
        //   a CLOSE, SUCCEED anyway.
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        PptCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    //
    // We have the RemoveLock
    //

    ExAcquireFastMutex(&extension->OpenCloseMutex);

    if( extension->OpenCloseRefCount > 0 ) {
        //
        // prevent rollover -  strange as it may seem, it is perfectly
        //   legal for us to receive more closes than creates - this
        //   info came directly from Mr. PnP himself
        //
        if( ((LONG)InterlockedDecrement(&extension->OpenCloseRefCount)) < 0 ) {
            // handle underflow
            InterlockedIncrement(&extension->OpenCloseRefCount);
        }
        ExReleaseFastMutex(&extension->OpenCloseMutex);
        PptDumpV( ("%S CloseFile, decrementing reference count, new count is %d\n",
                   extension->PnpInfo.PortName, extension->OpenCloseRefCount));
    } else {
        ExReleaseFastMutex(&extension->OpenCloseMutex);
        PptDumpV( ("%S CloseFile, reference count was already at 0\n", extension->PnpInfo.PortName) );
    }
    
    DDPnP1(("-- CloseFile %S, decrementing ref count, OpenCloseRefCount now = %d\n", extension->PnpInfo.PortName, extension->OpenCloseRefCount));

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    PptCompleteRequest(Irp, IO_NO_INCREMENT);
    PptReleaseRemoveLock(&extension->RemoveLock, Irp);
    return STATUS_SUCCESS;
}

