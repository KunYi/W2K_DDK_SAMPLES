/*++

Copyright (c) 1998 Microsoft Corporation

Module Name:

    1394diag.c

Abstract:

Author:

    Peter Binder (pbinder) 4/13/98

Revision History:
Date     Who       What
-------- --------- ------------------------------------------------------------
4/13/98  pbinder   taken from original 1394diag...
--*/

#define _1394DIAG_C
#include "pch.h"
#undef _1394DIAG_C

#if DBG

unsigned char t1394DiagDebugLevel = TL_WARNING;
unsigned char t1394DiagTrapLevel = FALSE;

#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    )
{
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    ENTER("DriverEntry");

    DriverObject->MajorFunction[IRP_MJ_CREATE]          = t1394Diag_Create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = t1394Diag_Close;
    DriverObject->MajorFunction[IRP_MJ_PNP]             = t1394Diag_Pnp;
    DriverObject->MajorFunction[IRP_MJ_POWER]           = t1394Diag_Power;
//    DriverObject->MajorFunction[IRP_MJ_READ]            = t1394Diag_AsyncRead;
//    DriverObject->MajorFunction[IRP_MJ_WRITE]           = t1394Diag_Write;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = t1394Diag_IoControl;
//    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]  = OhciDiag_SystemControl;
    DriverObject->DriverExtension->AddDevice            = t1394Diag_PnpAddDevice;
//    DriverObject->DriverUnload                          = OhciDiag_Unload;

    EXIT("DriverEntry", ntStatus);
    return(ntStatus);
} // DriverEntry

NTSTATUS
t1394Diag_Create(
    IN PDEVICE_OBJECT   DriverObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    ENTER("t1394Diag_Create");

    EXIT("t1394Diag_Create", ntStatus);
    return(ntStatus);
} // t1394Diag_Create

NTSTATUS
t1394Diag_Close(
    IN PDEVICE_OBJECT   DriverObject,
    IN PIRP             Irp
    )
{
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    ENTER("t1394Diag_Close");

    EXIT("t1394Diag_Close", ntStatus);
    return(ntStatus);
} // t1394Diag_Close

NTSTATUS
t1394Diag_SubmitIrpSynch(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PIRB                 Irb
    )
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    KEVENT              Event;
    PIO_STACK_LOCATION  NextIrpStack;

    ENTER("t1394Diag_SubmitIrpSynch");

    TRACE(TL_TRACE, ("DeviceObject = 0x%x\n", DeviceObject));
    TRACE(TL_TRACE, ("Irp = 0x%x\n", Irp));
    TRACE(TL_TRACE, ("Irb = 0x%x\n", Irb));

    if (Irb) {

        NextIrpStack = IoGetNextIrpStackLocation(Irp);
        NextIrpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
        NextIrpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_1394_CLASS;
        NextIrpStack->Parameters.Others.Argument1 = Irb;
    }
    else {

        IoCopyCurrentIrpStackLocationToNext(Irp);
    }

    KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

    IoSetCompletionRoutine( Irp,
                            t1394Diag_SynchCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE
                            );

    ntStatus = IoCallDriver(DeviceObject, Irp);

    if (ntStatus == STATUS_PENDING) {

        TRACE(TL_TRACE, ("t1394Diag_SubmitIrpSynch: Irp is pending...\n"));
        
        KeWaitForSingleObject( &Event,
                               Executive,
                               KernelMode,
                               FALSE,
                               NULL
                               );

    }

    ntStatus = Irp->IoStatus.Status;

    EXIT("t1394Diag_SubmitIrpSynch", ntStatus);
    return(ntStatus);
} // t1394Diag_SubmitIrpSynch

NTSTATUS
t1394Diag_SynchCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PKEVENT          Event
    )
{
    NTSTATUS        ntStatus = STATUS_SUCCESS;

    ENTER("t1394Diag_SynchCompletionRoutine");

    if (Event)
        KeSetEvent(Event, 0, FALSE);
    
    EXIT("t1394Diag_SynchCompletionRoutine", ntStatus);
    return(STATUS_MORE_PROCESSING_REQUIRED);
} // t1394Diag_SynchCompletionRoutine

void
t1394Diag_CancelIrp(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    KIRQL               Irql;
    PBUS_RESET_IRP      BusResetIrp;
    PDEVICE_EXTENSION   deviceExtension;

    ENTER("t1394Diag_CancelIrp");

    deviceExtension = DeviceObject->DeviceExtension;

    KeAcquireSpinLock(&deviceExtension->ResetSpinLock, &Irql);

    BusResetIrp = (PBUS_RESET_IRP) deviceExtension->BusResetIrps.Flink;

    TRACE(TL_TRACE, ("Irp = 0x%x\n", Irp));

    while (BusResetIrp) {

        TRACE(TL_TRACE, ("Cancelling BusResetIrp->Irp = 0x%x\n", BusResetIrp->Irp));

        if (BusResetIrp->Irp == Irp) {

            RemoveEntryList(&BusResetIrp->BusResetIrpList);
            ExFreePool(BusResetIrp);
            break;
        }
        else if (BusResetIrp->BusResetIrpList.Flink == &deviceExtension->BusResetIrps) {
            break;
        }
        else
            BusResetIrp = (PBUS_RESET_IRP)BusResetIrp->BusResetIrpList.Flink;
    }

    KeReleaseSpinLock(&deviceExtension->ResetSpinLock, Irql);

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    Irp->IoStatus.Status = STATUS_CANCELLED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

  	EXIT("t1394Diag_CancelIrp", STATUS_SUCCESS);
} // t1394Diag_CancelIrp


