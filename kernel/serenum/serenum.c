/*++
Copyright (c) 1997-1998  Microsoft Corporation

Module Name:

    SERENUM.C

Abstract:

    This module contains contains the entry points for a standard bus
    PNP / WDM driver.


Environment:

    kernel mode only

Notes:



--*/

#include "pch.h"

//
// Declare some entry functions as pageable, and make DriverEntry
// discardable
//

NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Serenum_DriverUnload)
//#pragma alloc_text (PAGE, Serenum_InitPDO)
#endif

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING UniRegistryPath
    )
/*++
Routine Description:

    Initialize the entry points of the driver.

--*/
{
    ULONG i;

    UNREFERENCED_PARAMETER (UniRegistryPath);

    Serenum_KdPrint_Def (SER_DBG_SS_TRACE, ("Driver Entry\n"));
    Serenum_KdPrint_Def (SER_DBG_SS_TRACE, ("RegPath: %x\n", UniRegistryPath));

    //
    // Set ever slot to initially pass the request through to the lower
    // device object
    //
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
       DriverObject->MajorFunction[i] = Serenum_DispatchPassThrough;
    }

    //
    // Fill in the Dispatch slots intercepted by the filter driver.
    //
    DriverObject->MajorFunction [IRP_MJ_CREATE] =
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = Serenum_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_PNP] = Serenum_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER] = Serenum_Power;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = Serenum_IoCtl;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL]
        = Serenum_InternIoCtl;
    DriverObject->DriverUnload = Serenum_DriverUnload;
    DriverObject->DriverExtension->AddDevice = Serenum_AddDevice;

    return STATUS_SUCCESS;
}


NTSTATUS
SerenumSyncCompletion(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp,
                      IN PKEVENT SerenumSyncEvent)
{
   UNREFERENCED_PARAMETER(DeviceObject);
   UNREFERENCED_PARAMETER(Irp);


   KeSetEvent(SerenumSyncEvent, IO_NO_INCREMENT, FALSE);
   return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
Serenum_CreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
/*++
Routine Description:
    Some outside source is trying to create a file against us.

    If this is for the FDO (the bus itself) then the caller is trying to
    open the propriatary conection to tell us which serial port to enumerate.

    If this is for the PDO (an object on the bus) then this is a client that
    wishes to use the serial port.
--*/
{
   PIO_STACK_LOCATION irpStack;
   NTSTATUS status;
   PFDO_DEVICE_DATA fdoData;
   KEVENT completionEvent;
   PDEVICE_OBJECT pNextDevice;


   UNREFERENCED_PARAMETER(DeviceObject);

   irpStack = IoGetCurrentIrpStackLocation(Irp);

   if (((PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension)->IsFDO) {
      fdoData = (PFDO_DEVICE_DATA)DeviceObject->DeviceExtension;
      pNextDevice = ((PFDO_DEVICE_DATA)DeviceObject->DeviceExtension)
                    ->TopOfStack;
   } else {
      fdoData = ((PPDO_DEVICE_DATA) DeviceObject->DeviceExtension)->
                ParentFdo->DeviceExtension;
      pNextDevice = ((PFDO_DEVICE_DATA)((PPDO_DEVICE_DATA)DeviceObject->
                                        DeviceExtension)->ParentFdo->
                     DeviceExtension)->TopOfStack;
   }

   switch (irpStack->MajorFunction) {
   case IRP_MJ_CREATE:
      Serenum_KdPrint_Def(SER_DBG_SS_TRACE, ("Create"));

      Serenum_StopPolling(fdoData, SERENUM_OPEN_LOCK);

      //
      // Pass on the create and the close
      //

      status = Serenum_DispatchPassThrough(DeviceObject, Irp);
      break;

   case IRP_MJ_CLOSE:
      Serenum_KdPrint_Def (SER_DBG_SS_TRACE, ("Close \n"));

      //
      // Send the close down; after it finishes we can open and take
      // over the port
      //

      IoCopyCurrentIrpStackLocationToNext(Irp);

      KeInitializeEvent(&completionEvent, SynchronizationEvent, FALSE);

      IoSetCompletionRoutine(Irp, SerenumSyncCompletion, &completionEvent,
                             TRUE, TRUE, TRUE);

      status = IoCallDriver(pNextDevice, Irp);


      if (status == STATUS_PENDING) {
         KeWaitForSingleObject(&completionEvent, Executive, KernelMode, FALSE,
                               NULL);
      }

      status = Irp->IoStatus.Status;

      IoCompleteRequest(Irp, IO_NO_INCREMENT);

      if (status == STATUS_SUCCESS) {
         Serenum_StartPolling(fdoData, SERENUM_OPEN_LOCK);
      }

      break;
   }

   return status;
}

NTSTATUS
Serenum_ListPorts (
    PSERENUM_PORT_DESC Desc,
    PFDO_DEVICE_DATA    FdoData
    )
/*++
Routine Description:
    This driver has just detected that a device has departed from the bus.
    (Actually the control panels has just told us that something has departed,
    but who is counting?

    We therefore need to flag the PDO as no longer attached, remove it from
    the linked list of PDOs for this bus, and then tell Plug and Play about it.
--*/
{
    Desc->PortHandle = FdoData->Self;
    Desc->PortAddress = FdoData->PhysicalAddress;

    return STATUS_SUCCESS;
}



NTSTATUS
Serenum_IoCtl (
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
Routine Description:

--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    ULONG                   inlen;
    ULONG                   outlen;
    PCOMMON_DEVICE_DATA     commonData;
    PFDO_DEVICE_DATA        fdoData;
    PVOID                   buffer;
    HANDLE                  keyHandle;
    ULONG                   actualLength;

    status = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_DEVICE_CONTROL == irpStack->MajorFunction);

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;
    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;
    buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    // We only take Device Control requests for the FDO.
    // That is the bus itself.
    //
    // The request is one of the propriatary Ioctls for
    //
    // NB We ARE a filter driver, so we DO pass on the irp if we don't handle it
    //

    inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (!commonData->IsFDO) {
        //
        // These commands are only allowed to go to the FDO.  Since they came
        // into the PDO, we need to fire them down to the serenum Fdo.
        //
        IoSkipCurrentIrpStackLocation (Irp);

        return IoCallDriver(
            ((PPDO_DEVICE_DATA) DeviceObject->DeviceExtension)->ParentFdo,
            Irp );
//        status = STATUS_ACCESS_DENIED;
//        Irp->IoStatus.Status = status;
//        IoCompleteRequest (Irp, IO_NO_INCREMENT);
//        return status;
    }

    status = Serenum_IncIoCount (fdoData);

    if (!NT_SUCCESS (status)) {
        //
        // This bus has received the PlugPlay remove IRP.  It will no longer
        // respond to external requests.
        //
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_SERENUM_PORT_DESC:

        if ((sizeof (SERENUM_PORT_DESC) == inlen) &&
            (inlen == outlen) &&
            (((PSERENUM_PORT_DESC)buffer)->Size == inlen)) {

            status = Serenum_ListPorts ((PSERENUM_PORT_DESC) buffer, fdoData);
            Irp->IoStatus.Information = outlen;

        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_SERENUM_GET_PORT_NAME:
        //
        // Get the port name from the registry.
        // This IOCTL is used by the modem cpl.
        //

        status = IoOpenDeviceRegistryKey(fdoData->UnderlyingPDO,
                                         PLUGPLAY_REGKEY_DEVICE,
                                         STANDARD_RIGHTS_READ,
                                         &keyHandle);

        if (!NT_SUCCESS(status)) {
           //
           // This is a fatal error.  If we can't get to our registry key,
           // we are sunk.
           //
           Serenum_KdPrint_Def (SER_DBG_PNP_ERROR,
                ("IoOpenDeviceRegistryKey failed - %x \n", status));
        } else {
            status = Serenum_GetRegistryKeyValue(
                keyHandle,
                L"PortName",
                sizeof(L"PortName"),
                Irp->AssociatedIrp.SystemBuffer,
                irpStack->Parameters.DeviceIoControl.OutputBufferLength,
                &actualLength);
            if ( STATUS_OBJECT_NAME_NOT_FOUND == status ||
                 STATUS_INVALID_PARAMETER  == status ) {
                status = Serenum_GetRegistryKeyValue(
                    keyHandle,
                    L"Identifier",
                    sizeof(L"Identifier"),
                    Irp->AssociatedIrp.SystemBuffer,
                    irpStack->Parameters.DeviceIoControl.OutputBufferLength,
                    &actualLength);
            }
            Irp->IoStatus.Information = actualLength;
            ZwClose (keyHandle);
        }
        break;
    default:
        //
        // This is not intended for us - fire and forget!
        //

       Serenum_DecIoCount (fdoData);
        return Serenum_DispatchPassThrough(
            DeviceObject,
            Irp);
    }

    Serenum_DecIoCount (fdoData);

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS
Serenum_InternIoCtl (
    PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
Routine Description:

--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    PCOMMON_DEVICE_DATA     commonData;
    PPDO_DEVICE_DATA        pdoData;
    PVOID                   buffer;

//    PAGED_CODE();

    status = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_INTERNAL_DEVICE_CONTROL == irpStack->MajorFunction);

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;
    pdoData = (PPDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // We only take Internal Device Control requests for the PDO.
    // That is the objects on the bus (representing the serial ports)
    //
    // We do pass on the irp if this comes into the fdo, but not if it comes
    // into the pdo.
    //

    if (commonData->IsFDO) {
        return Serenum_DispatchPassThrough(
            DeviceObject,
            Irp);
    } else if (pdoData->Removed) {
    //
    // This bus has received the PlugPlay remove IRP.  It will no longer
    // respond to external requests.
    //
    status = STATUS_DELETE_PENDING;

    } else {
        buffer = Irp->UserBuffer;

        switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_INTERNAL_SERENUM_REMOVE_SELF:
            Serenum_KdPrint(pdoData, SER_DBG_SS_TRACE, ("Remove self\n"));

            ((PFDO_DEVICE_DATA) pdoData->ParentFdo->DeviceExtension)->
                PDOForcedRemove = TRUE;
            IoInvalidateDeviceRelations(
                ((PFDO_DEVICE_DATA) pdoData->ParentFdo->DeviceExtension)->
                UnderlyingPDO,
                BusRelations );
            status = STATUS_SUCCESS;
            break;

        default:
            //
            // Pass it through
            //
            return Serenum_DispatchPassThrough(DeviceObject, Irp);
        }
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    return status;
}


VOID
Serenum_DriverUnload (
    IN PDRIVER_OBJECT Driver
    )
/*++
Routine Description:
    Clean up everything we did in driver entry.

--*/
{
    UNREFERENCED_PARAMETER (Driver);
    PAGED_CODE();

    //
    // All the device objects should be gone.
    //

    ASSERT (NULL == Driver->DeviceObject);

    //
    // Here we free any resources allocated in DriverEntry
    //

    return;
}

NTSTATUS
Serenum_IncIoCount (
    PFDO_DEVICE_DATA Data
    )
{
    InterlockedIncrement (&Data->OutstandingIO);
    if (Data->Removed) {

        if (0 == InterlockedDecrement (&Data->OutstandingIO)) {
            KeSetEvent (&Data->RemoveEvent, 0, FALSE);
        }
        return STATUS_DELETE_PENDING;
    }
    return STATUS_SUCCESS;
}

VOID
Serenum_DecIoCount (
    PFDO_DEVICE_DATA Data
    )
{
    if (0 == InterlockedDecrement (&Data->OutstandingIO)) {
        KeSetEvent (&Data->RemoveEvent, 0, FALSE);
    }
}

NTSTATUS
Serenum_DispatchPassThrough(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++
Routine Description:

    Passes a request on to the lower driver.

--*/
{
    PIO_STACK_LOCATION IrpStack =
            IoGetCurrentIrpStackLocation( Irp );

#if 0
        Serenum_KdPrint_Def (SER_DBG_SS_TRACE, (
            "[Serenum_DispatchPassThrough] "
            "IRP: %8x; "
            "MajorFunction: %d\n",
            Irp,
            IrpStack->MajorFunction ));
#endif

    //
    // Pass the IRP to the target
    //
   IoSkipCurrentIrpStackLocation (Irp);
    // CIMEXCIMEX:  VERIFY THIS FUNCTIONS CORRECTLY!!!

    if (((PPDO_DEVICE_DATA) DeviceObject->DeviceExtension)->IsFDO) {
        return IoCallDriver(
            ((PFDO_DEVICE_DATA) DeviceObject->DeviceExtension)->TopOfStack,
            Irp );
    } else {
        return IoCallDriver(
            ((PFDO_DEVICE_DATA) ((PPDO_DEVICE_DATA) DeviceObject->
                DeviceExtension)->ParentFdo->DeviceExtension)->TopOfStack,
                Irp );
    }
}

void
Serenum_InitPDO (
    PDEVICE_OBJECT      Pdo,
    PFDO_DEVICE_DATA    FdoData
    )
/*
Description:
    Common code to initialize a newly created serenum pdo.
    Called either when the control panel exposes a device or when Serenum senses
    a new device was attached.

Parameters:
    Pdo - The pdo
    FdoData - The fdo's device extension
*/
{
    ULONG FdoFlags = FdoData->Self->Flags;
    PPDO_DEVICE_DATA pdoData = Pdo->DeviceExtension;

    //
    // Check the IO style
    //
    if (FdoFlags & DO_BUFFERED_IO) {
        Pdo->Flags |= DO_BUFFERED_IO;
    } else if (FdoFlags & DO_DIRECT_IO) {
        Pdo->Flags |= DO_DIRECT_IO;
    }

    //
    // Increment the pdo's stacksize so that it can pass irps through
    //
    Pdo->StackSize += FdoData->Self->StackSize;

    //
    // Initialize the rest of the device extension
    //
    pdoData->IsFDO = FALSE;
    pdoData->Self = Pdo;

    pdoData->ParentFdo = FdoData->Self;

    pdoData->Started = FALSE; // irp_mn_start has yet to be received
    pdoData->Attached = TRUE; // attached to the bus
    pdoData->Removed = FALSE; // no irp_mn_remove as of yet
    pdoData->DebugLevel = FdoData->DebugLevel;  // Copy the debug level

    pdoData->DeviceState = PowerDeviceD0;
    pdoData->SystemState = PowerSystemWorking;

    //
    // Add the pdo to serenum's list
    //

    ASSERT(FdoData->AttachedPDO == NULL);
    ASSERT(FdoData->PdoData == NULL);
    ASSERT(FdoData->NumPDOs == 0);

    FdoData->AttachedPDO = Pdo;
    FdoData->PdoData = pdoData;
    FdoData->NumPDOs = 1;

    Pdo->Flags &= ~DO_DEVICE_INITIALIZING;
    Pdo->Flags |= DO_POWER_PAGABLE;
}

