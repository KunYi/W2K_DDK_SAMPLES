/*++
Copyright (c) 1997  Microsoft Corporation

Module Name:

    POWER.C

Abstract:

    This module contains contains the plugplay power calls for the serenum
    PNP / WDM BUS driver.


Environment:

    kernel mode only

Notes:


Revision History:


--*/

#include "pch.h"

#ifdef ALLOC_PRAGMA
//#pragma alloc_text (PAGE, Serenum_Power)
//#pragma alloc_text (PAGE, Serenum_FDO_Power)
//#pragma alloc_text (PAGE, Serenum_PDO_Power)
#endif

NTSTATUS
Serenum_Power (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++
--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PCOMMON_DEVICE_DATA commonData;

    PAGED_CODE ();

    status = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_POWER == irpStack->MajorFunction);

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

    if (commonData->IsFDO) {
        status =
            Serenum_FDO_Power ((PFDO_DEVICE_DATA) DeviceObject->DeviceExtension,
                Irp);
    } else {
        status =
            Serenum_PDO_Power ((PPDO_DEVICE_DATA) DeviceObject->DeviceExtension,
                Irp);
    }

    return status;
}

NTSTATUS
Serenum_FDOPowerComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
Serenum_PowerFDOControllingPDO (
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Irp,
    IN PIO_STATUS_BLOCK IoStatus
    );

NTSTATUS
Serenum_FDO_Power (
    PFDO_DEVICE_DATA    Data,
    PIRP                Irp
    )
/*++
--*/
{
    NTSTATUS            status;
    BOOLEAN             hookit = FALSE;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;
    PIO_STACK_LOCATION  stack;

    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

    PAGED_CODE ();

    status = Serenum_IncIoCount (Data);
    if (!NT_SUCCESS (status)) {
        PoStartNextPowerIrp (Irp);
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    switch (stack->MinorFunction) {
    case IRP_MN_SET_POWER:
	//
	// If it hasn't started, we just pass it through
	//

	if (Data->Started != TRUE) {
	    status = Irp->IoStatus.Status = STATUS_SUCCESS;
	    break;
	}

        Serenum_KdPrint(Data,
                     SER_DBG_PNP_TRACE,
                     ("Serenum-PnP Setting %s state to %d\n",
                      ((powerType == SystemPowerState) ?  "System" : "Device"),
                      powerState.SystemState));

        switch (powerType) {
        case DevicePowerState:

            status = Irp->IoStatus.Status = STATUS_SUCCESS;

            if (Data->DeviceState < powerState.DeviceState) {
                //
                // Powering down
                // Need to stop polling at the first sign of power down
                //
                Serenum_StopPolling(Data, SERENUM_POWER_LOCK);

#if 0
              if (Data->AttachedPDO) {
                 ASSERT(((PCOMMON_DEVICE_DATA) Data->AttachedPDO->
                        DeviceExtension)->DeviceState >=
                        powerState.DeviceState);

                }
#endif // DBG
                PoSetPowerState (Data->Self, powerType, powerState);
                Data->DeviceState = powerState.DeviceState;
            } else if (Data->DeviceState > powerState.DeviceState) {
                //
                // Powering Up
                //
                hookit = TRUE;
            }

            break;

        case SystemPowerState:
            break;
        }
        break;

    case IRP_MN_QUERY_POWER:
        //
        Data->PowerQueryLock = TRUE;
        status = Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    default:
        break;
    }

    IoCopyCurrentIrpStackLocationToNext (Irp);

    if (hookit) {
        status = Serenum_IncIoCount (Data);
        ASSERT (STATUS_SUCCESS == status);
        IoSetCompletionRoutine (Irp,
                                Serenum_FDOPowerComplete,
                                NULL,
                                TRUE,
                                TRUE,
                                TRUE);

        status = PoCallDriver (Data->TopOfStack, Irp);

    } else {
        //
        // Power IRPS come synchronously; drivers must call
        // PoStartNextPowerIrp, when they are ready for the next power
        // irp.  This can be called here, or in the completetion
        // routine, but never the less must be called.
        //
        PoStartNextPowerIrp (Irp);

        status =  PoCallDriver (Data->TopOfStack, Irp);
    }

    Serenum_DecIoCount (Data);
    return status;
}

VOID
Serenum_PowerFDOControllingPDO (
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Irp,
    IN PIO_STATUS_BLOCK IoStatus
    )
/*++
--*/
{
    NTSTATUS            status = STATUS_SUCCESS;
    PFDO_DEVICE_DATA    fdoData;

    UNREFERENCED_PARAMETER (MinorFunction);

    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    if (fdoData->DeviceState == PowerState.DeviceState) {
        //
        // If the device has been fully powered up, then the only way it
        // should get this irp is if this fdo had to manually repower
        // up its child pdo.  So now we should start polling again.
        //
        if (fdoData->DeviceState == PowerDeviceD0) {
            //
            // Should only start polling again when fully powered up.
            //
            Serenum_StartPolling(fdoData, SERENUM_POWER_LOCK);
        }
        ((PIRP) Irp)->IoStatus.Information = 0;
        ((PIRP) Irp)->IoStatus.Status = status;
    } else {
        //
        // The PDO had to be manually powered down.  Now that it's done, we're
        // allowed to power down the FDO.
        //
        PoSetPowerState (fdoData->Self, DevicePowerState, PowerState);
        fdoData->DeviceState = PowerState.DeviceState;

        PoStartNextPowerIrp ((PIRP) Irp);
        IoSkipCurrentIrpStackLocation ((PIRP) Irp);
        IoStatus->Status = PoCallDriver (fdoData->TopOfStack, (PIRP) Irp);
    }
}

NTSTATUS
Serenum_FDOPowerComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++
--*/
{
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;
    PIO_STACK_LOCATION  stack;
    PFDO_DEVICE_DATA    data;

    UNREFERENCED_PARAMETER (Context);

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    data = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;
    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

    switch (stack->MinorFunction) {
    case IRP_MN_SET_POWER:
        switch (powerType) {
        case DevicePowerState:
            //
            // Powering Up
            //
            ASSERT (powerState.DeviceState < data->DeviceState);
            data->DeviceState = powerState.DeviceState;

            PoSetPowerState (data->Self, powerType, powerState);

            if (data->LastSetPowerState == PowerDeviceD0) {
               //
               // Time to start polling again!
               //
               Serenum_StartPolling(data, SERENUM_POWER_LOCK);
            }
            break;

        default:
           break;
        }
        break;

    case IRP_MN_QUERY_POWER:

        ASSERT (IRP_MN_QUERY_POWER != stack->MinorFunction);
        break;

    default:
        ASSERT (0xBADBAD == IRP_MN_QUERY_POWER);
        break;
    }


    PoStartNextPowerIrp (Irp);
    Serenum_DecIoCount (data);

    return STATUS_SUCCESS; // Continue completion...
}

VOID
Serenum_PDOPowerComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Irp,
    IN PIO_STATUS_BLOCK IoStatus
    );

NTSTATUS
Serenum_PDO_Power (
    PPDO_DEVICE_DATA    PdoData,
    PIRP                Irp
    )
/*++
--*/
{
    NTSTATUS            status = STATUS_SUCCESS;
    PIO_STACK_LOCATION  stack;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

    switch (stack->MinorFunction) {
    case IRP_MN_SET_POWER:
        switch (powerType) {
        case DevicePowerState:
            if (PdoData->DeviceState > powerState.DeviceState) {
                PoSetPowerState (PdoData->Self, powerType, powerState);
                PdoData->DeviceState = powerState.DeviceState;
            } else if (PdoData->DeviceState < powerState.DeviceState) {
                //
                // Powering down.
                //
                PoSetPowerState (PdoData->Self, powerType, powerState);
                PdoData->DeviceState = powerState.DeviceState;
            }
            break;

        case SystemPowerState:
           status = STATUS_SUCCESS;
           break;

        default:
            status = STATUS_NOT_IMPLEMENTED;
            break;
        }
        break;

    case IRP_MN_QUERY_POWER:
        PdoData->PowerQueryLock = TRUE;
        status = STATUS_SUCCESS;
        break;

    case IRP_MN_WAIT_WAKE:
    case IRP_MN_POWER_SEQUENCE:
    default:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    Irp->IoStatus.Status = status;
    PoStartNextPowerIrp (Irp);
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    return status;
}



