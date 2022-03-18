/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    class.c

Abstract:

    SCSI class driver routines

Environment:

    kernel mode only

Notes:


Revision History:

--*/

#include "stddef.h"
#include "ntddk.h"
#include "scsi.h"
#include "classp.h"

#include <stdarg.h>

#define CLASS_TAG_POWER     'WLcS'

VOID
ClasspPowerRetryDpc(
    IN PKDPC Dpc,
    IN PCLASS_POWER_CONTEXT Context,
    IN PVOID Arg1,
    IN PVOID Arg2
    );

NTSTATUS
ClasspPowerHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN CLASS_POWER_OPTIONS Options
    );

NTSTATUS
ClasspPowerDownCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PCLASS_POWER_CONTEXT Context
    );

NTSTATUS
ClasspPowerUpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PCLASS_POWER_CONTEXT Context
    );

VOID
RetryPowerRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PCLASS_POWER_CONTEXT Context
    );

NTSTATUS
ClasspStartNextPowerIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );



NTSTATUS
ClassDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    ULONG isRemoved;
    PCLASS_POWER_DEVICE powerRoutine = NULL;

    //
    // NOTE: This code may be called at PASSIVE or DISPATCH, depending
    //       upon the device object it is being called for.
    //       don't do anything that would break under either circumstance.
    //

    NTSTATUS status;

    isRemoved = ClassAcquireRemoveLock(DeviceObject, Irp);

    if(isRemoved) {
        ClassReleaseRemoveLock(DeviceObject, Irp);
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        PoStartNextPowerIrp(Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    return commonExtension->DevInfo->ClassPowerDevice(DeviceObject, Irp);
}

NTSTATUS
ClasspPowerUpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PCLASS_POWER_CONTEXT Context
    )

/*++

Routine Description:

    This routine is used for intermediate completion of a power up request.
    PowerUp requires four requests to be sent to the lower driver in sequence.

        * The queue is "power locked" to ensure that the class driver power-up
          work can be done before request processing resumes.

        * The power irp is sent down the stack for any filter drivers and the
          port driver to return power and resume command processing for the
          device.  Since the queue is locked, no queued irps will be sent
          immediately.

        * A start unit command is issued to the device with appropriate flags
          to override the "power locked" queue.

        * The queue is "power unlocked" to start processing requests again.

    This routine uses the function in the srb which just completed to determine
    which state it is in.

Arguments:

    DeviceObject - the device object being powered up

    Irp - the IO_REQUEST_PACKET containing the power request

    Srb - the SRB used to perform port/class operations.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED or
    STATUS_SUCCESS

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION currentStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack = IoGetNextIrpStackLocation(Irp);


    NTSTATUS status = STATUS_MORE_PROCESSING_REQUIRED;

    DebugPrint((1, "ClasspPowerUpCompletion: Device Object %p, Irp %p, "
                   "Context %p\n",
                DeviceObject, Irp, Context));

    ASSERT(Context->Options.PowerDown == FALSE);
    ASSERT(Context->Options.HandleSpinUp);

    if(Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    Context->PowerChangeState.PowerUp++;

    switch(Context->PowerChangeState.PowerUp) {

        case PowerUpDeviceLocked: {

            DebugPrint((1, "(%p)\tPreviously sent power lock\n", Irp));

            //
            // Issue the actual power request to the lower driver.
            //

            IoCopyCurrentIrpStackLocationToNext(Irp);

            //
            // If the lock wasn't successful then just bail out on the power
            // request unless we can ignore failed locks
            //

            if((Context->Options.LockQueue == TRUE) &&
               (!NT_SUCCESS(Irp->IoStatus.Status))) {

                DebugPrint((1, "(%p)\tIrp status was %lx\n",
                            Irp, Irp->IoStatus.Status));
                DebugPrint((1, "(%p)\tSrb status was %lx\n",
                            Irp, Context->Srb.SrbStatus));

                //
                // Lock was not successful - throw down the power IRP
                // by itself and don't try to spin up the drive or unlock
                // the queue.
                //

                Context->InUse = FALSE;
                Context = NULL;

                //
                // Set the new power state
                //

                fdoExtension->DevicePowerState =
                    currentStack->Parameters.Power.State.DeviceState;

                Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

                IoCopyCurrentIrpStackLocationToNext(Irp);

                IoSetCompletionRoutine(Irp, 
                                       ClasspStartNextPowerIrpCompletion,
                                       NULL,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                //
                // Indicate to Po that we've been successfully powered up so
                // it can do it's notification stuff.
                //

                PoSetPowerState(DeviceObject,
                                currentStack->Parameters.Power.Type,
                                currentStack->Parameters.Power.State);

                PoCallDriver(commonExtension->LowerDeviceObject, Irp);

                ClassReleaseRemoveLock(commonExtension->DeviceObject,
                                       Irp);

                return STATUS_MORE_PROCESSING_REQUIRED;

            } else {
                Context->QueueLocked = (UCHAR) Context->Options.LockQueue;
            }

            Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

            Context->PowerChangeState.PowerUp = PowerUpDeviceLocked;

            IoSetCompletionRoutine(Irp,
                                   ClasspPowerUpCompletion,
                                   Context,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            status = PoCallDriver(commonExtension->LowerDeviceObject, Irp);

            DebugPrint((2, "(%p)\tPoCallDriver returned %lx\n", Irp, status));
            break;
        }

        case PowerUpDeviceOn: {

            PCDB cdb;

            if(NT_SUCCESS(Irp->IoStatus.Status)) {
    
                DebugPrint((1, "(%p)\tSending start unit to device\n", Irp));
    
                //
                // Issue the start unit command to the device.
                //
    
                Context->Srb.Length = sizeof(SCSI_REQUEST_BLOCK);
                Context->Srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
    
                Context->Srb.TimeOutValue = START_UNIT_TIMEOUT;
    
                Context->Srb.SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER |
                                        SRB_FLAGS_DISABLE_AUTOSENSE |
                                        SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
                                        SRB_FLAGS_NO_QUEUE_FREEZE;
    
                if(Context->Options.LockQueue) {
                    SET_FLAG(Context->Srb.SrbFlags, SRB_FLAGS_BYPASS_LOCKED_QUEUE);
                }
    
                Context->Srb.CdbLength = 6;
    
                cdb = (PCDB) (Context->Srb.Cdb);
    
                cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
                cdb->START_STOP.Start = 1;
    
                Context->PowerChangeState.PowerUp = PowerUpDeviceOn;
    
                IoSetCompletionRoutine(Irp,
                                       ClasspPowerUpCompletion,
                                       Context,
                                       TRUE,
                                       TRUE,
                                       TRUE);
    
                nextStack->Parameters.Scsi.Srb = &(Context->Srb);
                nextStack->MajorFunction = IRP_MJ_SCSI;
    
                status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
    
                DebugPrint((2, "(%p)\tIoCallDriver returned %lx\n", Irp, status));

            } else {

                //
                // we're done.  
                //

                Context->FinalStatus = Irp->IoStatus.Status;
                goto ClasspPowerUpCompletionFailure;
            }

            break;
        }

        case PowerUpDeviceStarted: { // 3

            //
            // First deal with an error if one occurred.
            //

            if(SRB_STATUS(Context->Srb.SrbStatus) != SRB_STATUS_SUCCESS) {

                BOOLEAN retry;

                DebugPrint((0, "%p\tError occured when issuing START_UNIT "
                            "command to device. Srb %p, Status %x\n",
                            Irp,
                            &Context->Srb,
                            Context->Srb.SrbStatus));

                ASSERT(!(TEST_FLAG(Context->Srb.SrbStatus,
                                   SRB_STATUS_QUEUE_FROZEN)));
                ASSERT(Context->Srb.Function == SRB_FUNCTION_EXECUTE_SCSI);

                Context->RetryInterval = 0;

                retry = ClassInterpretSenseInfo(
                            commonExtension->DeviceObject,
                            &Context->Srb,
                            IRP_MJ_SCSI,
                            IRP_MJ_POWER,
                            MAXIMUM_RETRIES - Context->RetryCount,
                            &status,
                            &Context->RetryInterval);

                if((retry == TRUE) && (Context->RetryCount-- != 0)) {

                    DebugPrint((0, "(%p)\tRetrying failed request\n", Irp));

                    //
                    // Decrement the state so we come back through here the
                    // next time.
                    //

                    Context->PowerChangeState.PowerUp--;

                    RetryPowerRequest(commonExtension->DeviceObject,
                                      Irp,
                                      Context);

                    break;

                }
            }

ClasspPowerUpCompletionFailure:

            DebugPrint((1, "(%p)\tPreviously spun device up\n", Irp));

            if (Context->QueueLocked) {
                DebugPrint((1, "(%p)\tUnlocking queue\n", Irp));

                Context->Srb.Function = SRB_FUNCTION_UNLOCK_QUEUE;
                Context->Srb.SrbFlags = SRB_FLAGS_BYPASS_LOCKED_QUEUE;
                nextStack->Parameters.Scsi.Srb = &(Context->Srb);
                nextStack->MajorFunction = IRP_MJ_SCSI;

                Context->PowerChangeState.PowerUp = PowerUpDeviceStarted;

                IoSetCompletionRoutine(Irp,
                                       ClasspPowerUpCompletion,
                                       Context,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
                DebugPrint((1, "(%p)\tIoCallDriver returned %lx\n",
                            Irp, status));
                break;
            }

            // Fall-through to next case...

        }

        case PowerUpDeviceUnlocked: {

            PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = 
                DeviceObject->DeviceExtension;

            //
            // This is the end of the dance.  Free the srb and complete the
            // request finally.  We're ignoring possible intermediate
            // error conditions ....
            //

            if (Context->QueueLocked) {
                DebugPrint((1, "(%p)\tPreviously unlocked queue\n", Irp));
                ASSERT(NT_SUCCESS(Irp->IoStatus.Status));
                ASSERT(Context->Srb.SrbStatus == SRB_STATUS_SUCCESS);
            } else {
                DebugPrint((1, "(%p)\tFall-through (queue not locked)\n", Irp));
            }

            DebugPrint((1, "(%p)\tFreeing srb and completing\n", Irp));
            Context->InUse = FALSE;

            status = Context->FinalStatus;
            Irp->IoStatus.Status = status;

            Context = NULL;

            //
            // Set the new power state
            //

            if(NT_SUCCESS(status)) {
                fdoExtension->DevicePowerState =
                    currentStack->Parameters.Power.State.DeviceState;
            }

            //
            // Indicate to Po that we've been successfully powered up so
            // it can do it's notification stuff.
            //

            PoSetPowerState(DeviceObject,
                            currentStack->Parameters.Power.Type,
                            currentStack->Parameters.Power.State);

            DebugPrint((1, "(%p)\tStarting next power irp\n", Irp));
            ClassReleaseRemoveLock(DeviceObject, Irp);
            PoStartNextPowerIrp(Irp);

            return status;
        }
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
ClasspPowerDownCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PCLASS_POWER_CONTEXT Context
    )

/*++

Routine Description:

    This routine is used for intermediate completion of a power up request.
    PowerUp requires four requests to be sent to the lower driver in sequence.

        * The queue is "power locked" to ensure that the class driver power-up
          work can be done before request processing resumes.

        * The power irp is sent down the stack for any filter drivers and the
          port driver to return power and resume command processing for the
          device.  Since the queue is locked, no queued irps will be sent
          immediately.

        * A start unit command is issued to the device with appropriate flags
          to override the "power locked" queue.

        * The queue is "power unlocked" to start processing requests again.

    This routine uses the function in the srb which just completed to determine
    which state it is in.

Arguments:

    DeviceObject - the device object being powered up

    Irp - the IO_REQUEST_PACKET containing the power request

    Srb - the SRB used to perform port/class operations.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED or
    STATUS_SUCCESS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION currentStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack = IoGetNextIrpStackLocation(Irp);

    NTSTATUS status = STATUS_MORE_PROCESSING_REQUIRED;

    DebugPrint((1, "ClasspPowerDownCompletion: Device Object %p, "
                   "Irp %p, Context %p\n",
                DeviceObject, Irp, Context));

    ASSERT(Context->Options.PowerDown == TRUE);
    ASSERT(Context->Options.HandleSpinDown);

    if(Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    Context->PowerChangeState.PowerDown++;

    switch(Context->PowerChangeState.PowerDown) {

        case PowerDownDeviceLocked: {

            PCDB cdb;

            DebugPrint((1, "(%p)\tPreviously sent power lock\n", Irp));

            if((Context->Options.LockQueue == TRUE) &&
               (!NT_SUCCESS(Irp->IoStatus.Status))) { 

                DebugPrint((1, "(%p)\tIrp status was %lx\n",
                            Irp,
                            Irp->IoStatus.Status));
                DebugPrint((1, "(%p)\tSrb status was %lx\n",
                            Irp,
                            Context->Srb.SrbStatus));

                Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

                //
                // Lock was not successful - throw down the power IRP
                // by itself and don't try to spin down the drive or unlock
                // the queue.
                //

                Context->InUse = FALSE;
                Context = NULL;

                //
                // Set the new power state
                //

                fdoExtension->DevicePowerState =
                    currentStack->Parameters.Power.State.DeviceState;

                //
                // Indicate to Po that we've been successfully powered down
                // so it can do it's notification stuff.
                //

                IoCopyCurrentIrpStackLocationToNext(Irp);
                IoSetCompletionRoutine(Irp,
                                       ClasspStartNextPowerIrpCompletion,
                                       NULL,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                PoSetPowerState(DeviceObject,
                                currentStack->Parameters.Power.Type,
                                currentStack->Parameters.Power.State);

                fdoExtension->PowerDownInProgress = FALSE;

                PoCallDriver(commonExtension->LowerDeviceObject, Irp);

                ClassReleaseRemoveLock(commonExtension->DeviceObject,
                                       Irp);

                return STATUS_MORE_PROCESSING_REQUIRED;

            } else {
                Context->QueueLocked = (UCHAR) Context->Options.LockQueue;
            }

            DebugPrint((1, "(%p)\tSending stop unit to device\n", Irp));

            //
            // Issue the start unit command to the device.
            //

            Context->Srb.Length = sizeof(SCSI_REQUEST_BLOCK);
            Context->Srb.Function = SRB_FUNCTION_EXECUTE_SCSI;

            Context->Srb.TimeOutValue = START_UNIT_TIMEOUT;

            Context->Srb.SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER |
                                    SRB_FLAGS_DISABLE_AUTOSENSE |
                                    SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
                                    SRB_FLAGS_NO_QUEUE_FREEZE |
                                    SRB_FLAGS_BYPASS_LOCKED_QUEUE;

            Context->Srb.CdbLength = 6;

            cdb = (PCDB) Context->Srb.Cdb;

            cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
            cdb->START_STOP.Start = 0;
            cdb->START_STOP.Immediate = 1;

            IoSetCompletionRoutine(Irp,
                                   ClasspPowerDownCompletion,
                                   Context,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            nextStack->Parameters.Scsi.Srb = &(Context->Srb);
            nextStack->MajorFunction = IRP_MJ_SCSI;

            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);

            DebugPrint((1, "(%p)\tIoCallDriver returned %lx\n", Irp, status));
            break;

        }

        case PowerDownDeviceStopped: {

            //
            // stop was sent
            //

            if(SRB_STATUS(Context->Srb.SrbStatus) != SRB_STATUS_SUCCESS) {

                BOOLEAN retry;

                DebugPrint((1, "(%p)\tError occured when issueing STOP_UNIT "
                            "command to device. Srb %p, Status %lx\n",
                            Irp,
                            &Context->Srb,
                            Context->Srb.SrbStatus));

                ASSERT(!(TEST_FLAG(Context->Srb.SrbStatus,
                                   SRB_STATUS_QUEUE_FROZEN)));
                ASSERT(Context->Srb.Function == SRB_FUNCTION_EXECUTE_SCSI);

                Context->RetryInterval = 0;
                retry = ClassInterpretSenseInfo(
                            commonExtension->DeviceObject,
                            &Context->Srb,
                            IRP_MJ_SCSI,
                            IRP_MJ_POWER,
                            MAXIMUM_RETRIES - Context->RetryCount,
                            &status,
                            &Context->RetryInterval);

                if((retry == TRUE) && (Context->RetryCount-- != 0)) {

                        DebugPrint((0, "(%p)\tRetrying failed request\n", Irp));

                        //
                        // decrement the state so we come back through here
                        // the next time.
                        //

                        Context->PowerChangeState.PowerDown--;
                        RetryPowerRequest(commonExtension->DeviceObject,
                                          Irp,
                                          Context);
                        break;
                }

                DebugPrint((1, "(%p)\tSTOP_UNIT not retried\n", Irp));

            } // end !SRB_STATUS_SUCCESS


            DebugPrint((1, "(%p)\tPreviously sent stop unit\n", Irp));

            //
            // Issue the actual power request to the lower driver.
            //

            Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

            IoCopyCurrentIrpStackLocationToNext(Irp);

            IoSetCompletionRoutine(Irp,
                                   ClasspPowerDownCompletion,
                                   Context,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            status = PoCallDriver(commonExtension->LowerDeviceObject, Irp);

            DebugPrint((1, "(%p)\tPoCallDriver returned %lx\n", Irp, status));

            break;

        }

        case PowerDownDeviceOff: {

            //
            // SpinDown request completed ... whether it succeeded or not is
            // another matter entirely.
            //

            DebugPrint((1, "(%p)\tPreviously sent power irp\n", Irp));

            if (Context->QueueLocked) {

                DebugPrint((1, "(%p)\tUnlocking queue\n", Irp));

                Context->Srb.Length = sizeof(SCSI_REQUEST_BLOCK);

                Context->Srb.Function = SRB_FUNCTION_UNLOCK_QUEUE;
                Context->Srb.SrbFlags = SRB_FLAGS_BYPASS_LOCKED_QUEUE;
                nextStack->Parameters.Scsi.Srb = &(Context->Srb);
                nextStack->MajorFunction = IRP_MJ_SCSI;

                IoSetCompletionRoutine(Irp,
                                       ClasspPowerDownCompletion,
                                       Context,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
                DebugPrint((1, "(%p)\tIoCallDriver returned %lx\n",
                            Irp,
                            status));
                break;
            }

        }

        case PowerDownDeviceUnlocked: {

            //
            // This is the end of the dance.  Free the srb and complete the
            // request finally.  We're ignoring possible intermediate
            // error conditions ....
            //

            if (Context->QueueLocked == FALSE) {
                DebugPrint((1, "(%p)\tFall through (queue not locked)\n", Irp));
            } else {
                DebugPrint((1, "(%p)\tPreviously unlocked queue\n", Irp));
                ASSERT(NT_SUCCESS(Irp->IoStatus.Status));
                ASSERT(Context->Srb.SrbStatus == SRB_STATUS_SUCCESS);
            }

            DebugPrint((1, "(%p)\tFreeing srb and completing\n", Irp));
            Context->InUse = FALSE;
            Context = NULL;


            status = Irp->IoStatus.Status;

            if(Irp->PendingReturned) {
                IoMarkIrpPending(Irp);
            }

            //
            // Set the new power state
            //

            fdoExtension->DevicePowerState =
                currentStack->Parameters.Power.State.DeviceState;


            DebugPrint((1, "(%p)\tStarting next power irp\n", Irp));

            ClassReleaseRemoveLock(DeviceObject, Irp);
            PoStartNextPowerIrp(Irp);
            fdoExtension->PowerDownInProgress = FALSE;

            return status;
        }
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
ClasspPowerHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN CLASS_POWER_OPTIONS Options
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PDEVICE_OBJECT lowerDevice = commonExtension->LowerDeviceObject;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    PCLASS_POWER_CONTEXT context;

    if (!commonExtension->IsFdo) {

        //
        // certain assumptions are made here,
        // particularly: having the fdoExtension
        //

        DebugPrint((0, "ClasspPowerHandler: Called for PDO %p???\n",
                    DeviceObject));
        ASSERT(!"PDO using ClasspPowerHandler");
    }

    DebugPrint((1, "ClasspPowerHandler: Power irp %p to %s %p\n",
                Irp, (commonExtension->IsFdo ? "fdo" : "pdo"), DeviceObject));


    switch(irpStack->MinorFunction) {

        case IRP_MN_SET_POWER: {

            DebugPrint((1, "(%p)\tIRP_MN_SET_POWER\n", Irp));

            DebugPrint((1, "(%p)\tSetting %s state to %d\n",
                        Irp,
                        (irpStack->Parameters.Power.Type == SystemPowerState ?
                            "System" : "Device"),
                        irpStack->Parameters.Power.State.SystemState));
            break;
        }

        default: {

            DebugPrint((1, "(%p)\tIrp minor code = %#x\n",
                        Irp, irpStack->MinorFunction));
            break;
        }
    }

    if (irpStack->Parameters.Power.Type != DevicePowerState ||
        irpStack->MinorFunction != IRP_MN_SET_POWER) {

        DebugPrint((1, "(%p)\tSending to lower device\n", Irp));

        goto ClassSpinDownPowerHandlerCleanup;

    }

    nextIrpStack = IoGetNextIrpStackLocation(Irp);
    fdoExtension = DeviceObject->DeviceExtension;

    //
    // already in exact same state, don't work to transition to it.
    //

    if(irpStack->Parameters.Power.State.DeviceState ==
       fdoExtension->DevicePowerState) {

        DebugPrint((1, "(%p)\tAlready in device state %x\n",
                    Irp, fdoExtension->DevicePowerState));
        goto ClassSpinDownPowerHandlerCleanup;

    }

    //
    // or powering down from non-d0 state (device already stopped)
    // BUGBUG -- Isn't this an invalid request per the ddk docs?
    //

    if ((irpStack->Parameters.Power.State.DeviceState != PowerDeviceD0) &&
        (fdoExtension->DevicePowerState != PowerDeviceD0)) {
        DebugPrint((1, "(%p)\tAlready powered down to %x???\n",
                    Irp, fdoExtension->DevicePowerState));
        fdoExtension->DevicePowerState =
            irpStack->Parameters.Power.State.DeviceState;
        goto ClassSpinDownPowerHandlerCleanup;
    }

    //
    // or going into a hibernation state when we're in the hibernation path.
    // If the device is spinning then we should leave it spinning - if it's not
    // then the dump driver will start it up for us.
    //

    if((irpStack->Parameters.Power.State.DeviceState == PowerDeviceD3) &&
       (irpStack->Parameters.Power.ShutdownType == PowerActionHibernate) &&
       (commonExtension->HibernationPathCount != 0)) {

        DebugPrint((1, "(%p)\tdoing nothing for hibernation request for "
                       "state %x???\n",
                    Irp, fdoExtension->DevicePowerState));
        fdoExtension->DevicePowerState =
            irpStack->Parameters.Power.State.DeviceState;
        goto ClassSpinDownPowerHandlerCleanup;
    }
    //
    // or when not handling powering up and are powering up
    //

    if ((!Options.HandleSpinUp) &&
        (irpStack->Parameters.Power.State.DeviceState == PowerDeviceD0)) {

        DebugPrint((2, "(%p)\tNot handling spinup to state %x\n",
                    Irp, fdoExtension->DevicePowerState));
        fdoExtension->DevicePowerState =
            irpStack->Parameters.Power.State.DeviceState;
        goto ClassSpinDownPowerHandlerCleanup;

    }

    //
    // or when not handling powering down and are powering down
    //

    if ((!Options.HandleSpinDown) &&
        (irpStack->Parameters.Power.State.DeviceState != PowerDeviceD0)) {

        DebugPrint((2, "(%p)\tNot handling spindown to state %x\n",
                    Irp, fdoExtension->DevicePowerState));
        fdoExtension->DevicePowerState =
            irpStack->Parameters.Power.State.DeviceState;
        goto ClassSpinDownPowerHandlerCleanup;

    }

    context = &(fdoExtension->PowerContext);
    
#if DBG
    //
    // Mark the context as in use.  We should be synchronizing this but 
    // since it's just for debugging purposes we don't worry too much.
    //

    ASSERT(context->InUse == FALSE);
#endif

    RtlZeroMemory(context, sizeof(CLASS_POWER_CONTEXT));
    context->InUse = TRUE;

    nextIrpStack->Parameters.Scsi.Srb = &(context->Srb);
    nextIrpStack->MajorFunction = IRP_MJ_SCSI;

    context->FinalStatus = STATUS_SUCCESS;

    context->Srb.Length = sizeof(SCSI_REQUEST_BLOCK);
    context->Srb.OriginalRequest = Irp;
    context->Srb.SrbFlags |= SRB_FLAGS_BYPASS_LOCKED_QUEUE
                          |  SRB_FLAGS_NO_QUEUE_FREEZE;
    context->Srb.Function = SRB_FUNCTION_LOCK_QUEUE;

    context->Srb.SenseInfoBuffer =
        commonExtension->PartitionZeroExtension->SenseData;
    context->Srb.SenseInfoBufferLength = SENSE_BUFFER_SIZE;
    context->RetryCount = MAXIMUM_RETRIES;

    context->Options = Options;
    context->DeviceObject = DeviceObject;
    context->Irp = Irp;

    if(irpStack->Parameters.Power.State.DeviceState == PowerDeviceD0) {

        ASSERT(Options.HandleSpinUp);

        DebugPrint((2, "(%p)\tpower up - locking queue\n", Irp));

        //
        // We need to issue a queue lock request so that we
        // can spin the drive back up after the power is restored
        // but before any requests are processed.
        //

        context->Options.PowerDown = FALSE;
        context->PowerChangeState.PowerUp = PowerUpDeviceInitial;
        context->CompletionRoutine = ClasspPowerUpCompletion;

    } else {

        ASSERT(Options.HandleSpinDown);

        fdoExtension->PowerDownInProgress = TRUE;

        DebugPrint((2, "(%p)\tPowering down - locking queue\n", Irp));

        PoSetPowerState(DeviceObject,
                        irpStack->Parameters.Power.Type,
                        irpStack->Parameters.Power.State);

        context->Options.PowerDown = TRUE;
        context->PowerChangeState.PowerDown = PowerDownDeviceInitial;
        context->CompletionRoutine = ClasspPowerDownCompletion;

    }

    if(Options.LockQueue) {

        //
        // Send the lock irp down.
        //

        IoSetCompletionRoutine(Irp,
                               context->CompletionRoutine,
                               context,
                               TRUE,
                               TRUE,
                               TRUE);
    
        IoMarkIrpPending(Irp);
        IoCallDriver(lowerDevice, Irp);

    } else {

        //
        // Call the completion routine directly.  It won't care what the 
        // status of the "lock" was - it will just go and do the next 
        // step of the operation.
        //

        context->CompletionRoutine(DeviceObject, Irp, context);
    }

    return STATUS_PENDING;

ClassSpinDownPowerHandlerCleanup:

    ClassReleaseRemoveLock(DeviceObject, Irp);

    DebugPrint((1, "(%p)\tStarting next power irp\n", Irp));
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp,
                           ClasspStartNextPowerIrpCompletion,
                           NULL,
                           TRUE,
                           TRUE,
                           TRUE);
    return PoCallDriver(lowerDevice, Irp);
}


NTSTATUS
ClassMinimalPowerHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    ClassReleaseRemoveLock(DeviceObject, Irp);
    PoStartNextPowerIrp(Irp);

    if(commonExtension->IsFdo) {
        IoCopyCurrentIrpStackLocationToNext(Irp);
        return PoCallDriver(commonExtension->LowerDeviceObject, Irp);
    } else {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
}


NTSTATUS
ClassSpinDownPowerHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is a callback for disks and other things which require both
    a start and a stop to be sent to the device.  (actually the starts are
    almost always optional, since most device power themselves on to process
    commands, but i digress).

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

Return Value:

    None

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    CLASS_POWER_OPTIONS options;

    fdoExtension = (PFUNCTIONAL_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // this will set all options to FALSE
    //

    RtlZeroMemory(&options, sizeof(CLASS_POWER_OPTIONS));

    //
    // check the flags to see what options we need to worry about
    //

    if (!TEST_FLAG(fdoExtension->ScanForSpecialFlags,
                  CLASS_SPECIAL_DISABLE_SPIN_DOWN)) {
        options.HandleSpinDown = TRUE;
    }

    if (!TEST_FLAG(fdoExtension->ScanForSpecialFlags,
                  CLASS_SPECIAL_DISABLE_SPIN_UP)) {
        options.HandleSpinUp = TRUE;
    }

    if (!TEST_FLAG(fdoExtension->ScanForSpecialFlags,
                  CLASS_SPECIAL_NO_QUEUE_LOCK)) {
        options.LockQueue = TRUE;
    } 

    DebugPrint((3, "ClasspPowerHandler: Devobj %p\n"
                "\t%shandling spin down\n"
                "\t%shandling spin up\n"
                "\t%slocking queue\n",
                DeviceObject,
                (options.HandleSpinDown ? "" : "not "),
                (options.HandleSpinUp   ? "" : "not "),
                (options.LockQueue      ? "" : "not ")
                ));

    //
    // do all the dirty work
    //

    return ClasspPowerHandler(DeviceObject, Irp, options);
}


NTSTATUS
ClassStopUnitPowerHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is a callback for cdroms and dvds.  It handles only the
    spindown requests, in order to stop audio playback during sleep.  It
    also ignores queue lock failures, since it is non-critical if another
    play request occurs in the timing window which would otherwise occur.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

Return Value:

    None

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;

    DebugPrint((0, "ClassStopUnitPowerHandler - Devobj %p using outdated call\n"
                "Drivers should set the following flags in ScanForSpecialFlags "
                " in the FDO extension:\n"
                "\tCLASS_SPECIAL_DISABLE_SPIN_UP\n"
                "\tCLASS_SPECIAL_NO_QUEUE_LOCK\n"
                "This will provide equivalent functionality if the power "
                "routine is then set to ClassSpinDownPowerHandler\n\n",
                DeviceObject));

    fdoExtension = (PFUNCTIONAL_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    SET_FLAG(fdoExtension->ScanForSpecialFlags,
             CLASS_SPECIAL_DISABLE_SPIN_UP);
    SET_FLAG(fdoExtension->ScanForSpecialFlags,
             CLASS_SPECIAL_NO_QUEUE_LOCK);

    return ClassSpinDownPowerHandler(DeviceObject, Irp);
}

VOID
RetryPowerRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PCLASS_POWER_CONTEXT Context
    )

/*++

Routine Description:

    This routine reinitalizes the necessary fields, and sends the request
    to the lower driver.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

    Context - Supplies a pointer to the power up context for this request.

Return Value:

    None

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    LARGE_INTEGER dueTime;

    DebugPrint((1, "(%p)\tDelaying retry by queueing DPC\n", Irp));

    ASSERT(Context->Irp == Irp);
    ASSERT(Context->DeviceObject == DeviceObject);

    KeInitializeDpc(&fdoExtension->PowerRetryDpc,
                    ClasspPowerRetryDpc,
                    Context);
    KeInitializeTimer(&fdoExtension->PowerRetryTimer);

    if (Context->RetryInterval == 0) {
        DebugPrint((2, "(%p)\tDelaying minimum time (.2 sec)\n", Irp));
        dueTime.QuadPart = (LONGLONG)1000000 * -2;
    } else {
        DebugPrint((2, "(%p)\tDelaying %x seconds\n",
                    Irp, Context->RetryInterval));
        dueTime.QuadPart = (LONGLONG)1000000 * -10 * Context->RetryInterval;
    }

    KeSetTimerEx(&fdoExtension->PowerRetryTimer,
                 dueTime, 0,
                 &fdoExtension->PowerRetryDpc);
    return;

} // end RetryRequest()


VOID
ClasspPowerRetryDpc(
    IN PKDPC Dpc,
    IN PCLASS_POWER_CONTEXT Context,
    IN PVOID Arg1,
    IN PVOID Arg2
    )
/*++

Routine Description:

    This routine reinitalizes the necessary fields, and sends the request
    to the lower driver.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

    Context - Supplies a pointer to the power up context for this request.

Return Value:

    None

--*/
{
    PDEVICE_OBJECT DeviceObject = Context->DeviceObject;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PIRP Irp = Context->Irp;
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = &(Context->Srb);


    DebugPrint((1, "ClasspPowerRetryDpc: Retrying: DeviceObject %p, "
                "Irp %p, Context %p\n",
                DeviceObject, Irp, Context));

    //
    // reset the retry interval
    //

    Context->RetryInterval = 0;

    //
    // Reset byte count of transfer in SRB Extension.
    //

    srb->DataTransferLength = 0;

    //
    // Zero SRB statuses.
    //

    srb->SrbStatus = srb->ScsiStatus = 0;

    //
    // Set up major SCSI function.
    //

    nextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    nextIrpStack->Parameters.Scsi.Srb = srb;

    //
    // Set the completion routine up again.
    //

    IoSetCompletionRoutine(Irp, Context->CompletionRoutine, Context,
                           TRUE, TRUE, TRUE);

    //
    // Pass the request to the port driver.
    //

    IoCallDriver(commonExtension->LowerDeviceObject, Irp);

    return;
}

NTSTATUS
ClasspStartNextPowerIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PoStartNextPowerIrp(Irp);
    return STATUS_SUCCESS;
}

