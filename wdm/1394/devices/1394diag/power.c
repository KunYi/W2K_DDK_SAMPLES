/*++

Copyright (c) 1998	Microsoft Corporation

Module Name: 

    power.c

Abstract


Author:

    Peter Binder (pbinder) 4/13/98

Revision History:
Date     Who       What
-------- --------- ------------------------------------------------------------
4/13/98  pbinder   taken from 1394diag/ohcidiag
--*/

#include "pch.h"

NTSTATUS
t1394Diag_Power(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    NTSTATUS                ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION      IrpSp;
    PDEVICE_EXTENSION       deviceExtension;
    POWER_STATE             State;
    KIRQL                   Irql;

    ENTER("t1394Diag_Power");

    deviceExtension = DeviceObject->DeviceExtension;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    State = IrpSp->Parameters.Power.State;

    TRACE(TL_TRACE, ("Power.Type = 0x%x\n", IrpSp->Parameters.Power.Type));
    TRACE(TL_TRACE, ("Power.State.SystemState = 0x%x\n", State.SystemState));
    TRACE(TL_TRACE, ("Power.State.DeviceState = 0x%x\n", State.DeviceState));

    switch (IrpSp->MinorFunction) {

        case IRP_MN_SET_POWER:
            TRACE(TL_TRACE, ("IRP_MN_SET_POWER\n"));

            switch (IrpSp->Parameters.Power.Type) {

                case SystemPowerState:
                    TRACE(TL_TRACE, ("SystemPowerState\n"));

                    if (State.SystemState == PowerSystemWorking) {

                        State.DeviceState = PowerDeviceD0;
                    }
                    else {
                    
                        State.DeviceState = PowerDeviceD3;
                    }

                    if (State.DeviceState != deviceExtension->CurrentDevicePowerState.DeviceState) {

                        ntStatus = PoRequestPowerIrp( deviceExtension->StackDeviceObject,
                                                      IRP_MN_SET_POWER,
                                                      State,
                                                      t1394Diag_PowerRequestCompletion,
                                                      Irp,
                                                      NULL
                                                      );
                    }
                    else {

                        IoCopyCurrentIrpStackLocationToNext(Irp);
                        PoStartNextPowerIrp(Irp);
                        ntStatus = PoCallDriver(deviceExtension->StackDeviceObject, Irp);
                    }

                    break; // SystemPowerState

                case DevicePowerState:
                    TRACE(TL_TRACE, ("DevicePowerState\n"));

//                    KeAcquireSpinLock(&deviceExtension->SpinLock, &Irql);
                    deviceExtension->CurrentDevicePowerState = State;
//                    KeReleaseSpinLock(&deviceExtension->SpinLock, Irql);

                    IoCopyCurrentIrpStackLocationToNext(Irp);

                    PoStartNextPowerIrp(Irp);
                    ntStatus = PoCallDriver(deviceExtension->StackDeviceObject, Irp);

                    break; // DevicePowerState

                default:
                    break;

            }
            break; // IRP_MN_SET_POWER

        case IRP_MN_QUERY_POWER:
            TRACE(TL_TRACE, ("IRP_MN_QUERY_POWER\n"));

            IoCopyCurrentIrpStackLocationToNext(Irp);
            PoStartNextPowerIrp(Irp);
            ntStatus = PoCallDriver(deviceExtension->StackDeviceObject, Irp);

            break; // IRP_MN_QUERY_POWER

        default:
            TRACE(TL_TRACE, ("Default = 0x%x\n", IrpSp->MinorFunction));

            IoCopyCurrentIrpStackLocationToNext(Irp);
            PoStartNextPowerIrp(Irp);
            ntStatus = PoCallDriver(deviceExtension->StackDeviceObject, Irp);

            break; // default
            
    } // switch

    EXIT("t1394Diag_Power", ntStatus);
    return(ntStatus);
} // t1394Diag_Power

void
t1394Diag_PowerRequestCompletion(
    IN PDEVICE_OBJECT       DeviceObject,
    IN UCHAR                MinorFunction,
    IN POWER_STATE          PowerState,
    IN PIRP                 Irp,
    IN PIO_STATUS_BLOCK     IoStatus
    )
{
    NTSTATUS            ntStatus;
//    PDEVICE_EXTENSION   deviceExtension;

    ENTER("t1394Diag_PowerRequestCompletion");

    ntStatus = IoStatus->Status;

    PoStartNextPowerIrp(Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
//    PoCallDriver(deviceExtension->StackDeviceObject, Irp);   

    EXIT("t1394Diag_PowerRequestCompletion", ntStatus);

    return;
} // t1394Diag_PowerRequestCompletion


