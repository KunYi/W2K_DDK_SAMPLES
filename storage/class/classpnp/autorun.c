/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    autorun.c

Abstract:

    Code for support of media change detection in the class driver

Environment:

    kernel mode only

Notes:


Revision History:

--*/

#include "classp.h"

GUID StoragePredictFailureEventGuid = WMI_STORAGE_PREDICT_FAILURE_EVENT_GUID;

//
// Only send polling irp when device is fully powered up and a
// power down irp is not in progress.
//
// NOTE:   This helps close a window in time where a polling irp could cause
//         a drive to spin up right after it has powered down. The problem is
//         that SCSIPORT, ATAPI and SBP2 will be in the process of powering
//         down (which may take a few seconds), but won't know that. It would
//         then get a polling irp which will be put into its queue since it
//         the disk isn't powered down yet. Once the disk is powered down it
//         will find the polling irp in the queue and then power up the
//         device to do the poll. They do not want to check if the polling
//         irp has the SRB_NO_KEEP_AWAKE flag here since it is in a critical
//         path and would slow down all I/Os. A better way to fix this
//         would be to serialize the polling and power down irps so that
//         only one of them is sent to the device at a time.
#define ClasspCanSendPollingIrp(fdoExtension)                           \
               ((fdoExtension->DevicePowerState == PowerDeviceD0) &&  \
                (! fdoExtension->PowerDownInProgress) )

NTSTATUS
ClassMediaChangeDeviceInstanceDisabled(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN OUT PBOOLEAN Value,
    IN BOOLEAN SetValue
    );

BOOLEAN
ClasspMediaChangeAlwaysDisabled(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
ClasspMediaChangeRegistryCallBack(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
ClasspFailurePredict(
    IN PDEVICE_OBJECT DeviceObject,
    IN PFAILURE_PREDICTION_INFO Info
    );

NTSTATUS
ClasspInitializePolling(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN BOOLEAN AllowDriveToSleep
    );


#if ALLOC_PRAGMA

#pragma alloc_text(PAGE, ClassInitializeMediaChangeDetection)
#pragma alloc_text(PAGE, ClassEnableMediaChangeDetection)
#pragma alloc_text(PAGE, ClassDisableMediaChangeDetection)
#pragma alloc_text(PAGE, ClassCleanupMediaChangeDetection)
#pragma alloc_text(PAGE, ClasspMediaChangeRegistryCallBack)
#pragma alloc_text(PAGE, ClasspInitializePolling)

#pragma alloc_text(PAGE, ClassMediaChangeDeviceInstanceDisabled)
#pragma alloc_text(PAGE, ClasspMediaChangeAlwaysDisabled)

#pragma alloc_text(PAGE, ClasspFailurePredict)
#pragma alloc_text(PAGE, ClassSetFailurePredictionPoll)
#pragma alloc_text(PAGE, ClasspDisableTimer)
#pragma alloc_text(PAGE, ClasspEnableTimer)

#endif


NTSTATUS
ClasspMediaChangeDetectionSanityCheck(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PMEDIA_CHANGE_DETECTION_INFO Info
    )
{
    DebugPrint((0, "Classpnp: (%p) Autorun has lost its marbles (info %p)\n",
                DeviceObject, Info));
    Info->MediaChangeIrpCompleted = 0x1;

    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
ClasspInternalSetMediaChangeState(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN MEDIA_CHANGE_DETECTION_STATE State
    )
/*++

Routine Description:

    This routine will (if appropriate) set the media change event for the
    device.  The event will be set if the media state is changed and
    media change events are enabled.  Otherwise the media state will be
    tracked but the event will not be set.

    This routine will lock out the other media change routines if possible
    but if not a media change notification may be lost after the enable has
    been completed.

Arguments:

    FdoExtension - the device

    MediaPresent - indicates whether the device has media inserted into it
                   (TRUE) or not (FALSE).

Return Value:

    none

--*/
{
#if DBG
    PUCHAR states[] = {"Unknown", "Present", "Not Present"};
#endif
    MEDIA_CHANGE_DETECTION_STATE oldMediaState;
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;
    TARGET_DEVICE_CUSTOM_NOTIFICATION  NotificationStructure[2];

    NTSTATUS status;

    ASSERT((State >= MediaUnknown) && (State <= MediaNotPresent));

    if(info == NULL) {
        return;
    }

    oldMediaState = info->MediaChangeDetectionState;
    info->MediaChangeDetectionState = State;

    DebugPrint((3, "ClassSetMediaChangeState: State set to %s from %s\n",
                states[State], states[oldMediaState]));

    if(info->MediaChangeDetectionDisableCount != 0) {

        DebugPrint((3, "ClassSetMediaChangeState: MCN not enabled\n"));
        return;

    }

    if(oldMediaState == MediaUnknown) {

        //
        // The media was in an indeterminate state before - don't notify for
        // this change.
        //

        DebugPrint((3, "ClassSetMediaChangeState: State was unknown - this may "
                       "not be a change\n"));
        return;

    } else if(oldMediaState == State) {

        //
        // Media is in the same state it was before.
        //

        return;
    }

    DebugPrint((2, "ClassSetMediaChangeState: Signalling Event\n"));

    if (State == MediaPresent) {

        NotificationStructure[0].Event = GUID_IO_MEDIA_ARRIVAL;

    } else if (State == MediaNotPresent) {

        NotificationStructure[0].Event = GUID_IO_MEDIA_REMOVAL;

    } else {

        //
        // Don't notify of changed going to unknown.
        //

        return;
    }


    NotificationStructure[0].Version = 1;
    NotificationStructure[0].Size = sizeof(TARGET_DEVICE_CUSTOM_NOTIFICATION) +
                                    sizeof(ULONG) - sizeof(UCHAR);
    NotificationStructure[0].FileObject = NULL;
    NotificationStructure[0].NameBufferOffset = 0;

    //
    // Increasing Index for this event
    //

    *((PULONG) (&(NotificationStructure[0].CustomDataBuffer[0]))) = 0;

    IoReportTargetDeviceChangeAsynchronous(FdoExtension->LowerPdo,
                                           &NotificationStructure[0],
                                           NULL,
                                           NULL);


    return;
}


VOID
ClassSetMediaChangeState(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN MEDIA_CHANGE_DETECTION_STATE State,
    IN BOOLEAN Wait
    )
/*++

Routine Description:

    This routine will (if appropriate) set the media change event for the
    device.  The event will be set if the media state is changed and
    media change events are enabled.  Otherwise the media state will be
    tracked but the event will not be set.

    This routine will lock out the other media change routines if possible
    but if not a media change notification may be lost after the enable has
    been completed.

Arguments:

    FdoExtension - the device

    MediaPresent - indicates whether the device has media inserted into it
                   (TRUE) or not (FALSE).

    Wait - indicates whether the function should wait until it can acquire
           the synchronization lock or not.

Return Value:

    none

--*/
{
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;
    LARGE_INTEGER zero;
    NTSTATUS status;


    //
    // Reset SMART status on media removal as the old status may not be
    // valid when there is no media in the device or when new media is
    // inserted.
    if (State == MediaNotPresent)
    {
        FdoExtension->FailurePredicted = FALSE;
        FdoExtension->FailureReason = 0;
    }


    zero.QuadPart = 0;

    if(info == NULL) {
        return;
    }

    status = KeWaitForSingleObject(
                &(info->MediaChangeSynchronizationEvent),
                Executive,
                KernelMode,
                FALSE,
                ((Wait == TRUE) ? NULL : &zero));

    if(status == STATUS_TIMEOUT) {

        //
        // Someone's in the process of doing an enable or disable.  Just drop
        // the change on the floor.
        //

        return;
    }

    //
    // Invert the media present state and save the new non-present state
    // away.
    //

    ClasspInternalSetMediaChangeState(FdoExtension, State);

    KeSetEvent(&(info->MediaChangeSynchronizationEvent),
               IO_NO_INCREMENT,
               FALSE);

    return;
}


NTSTATUS
ClasspMediaChangeCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )

/*++

Routine Description:

    This routine handles the completion of the test unit ready irps used to
    determine if the media has changed.  If the media has changed, this code
    signals the named event to wake up other system services that react to
    media change (aka AutoPlay).

Arguments:

    DeviceObject - the object for the completion
    Irp - the IRP being completed
    Context - the SRB from the IRP

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension;
    PMEDIA_CHANGE_DETECTION_INFO info;

    PSCSI_REQUEST_BLOCK srb = (PSCSI_REQUEST_BLOCK) Context;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  nextIrpStack = IoGetNextIrpStackLocation(Irp);

    UCHAR senseKey;
    PSENSE_DATA         senseBuffer;
    KIRQL  irql;

    ASSERT(Irp);
    ASSERT(currentIrpStack);


    DeviceObject = currentIrpStack->DeviceObject;

    ASSERT(DeviceObject);

    DebugPrint((4, "ClasspSendMediaStateIrp: Device %p completed FP irp %p\n",
                        DeviceObject, Irp));

    //
    // Since the class driver created this request, it's completion routine
    // will not get a valid device object handed in.  Use the one in the
    // irp stack instead
    //

    fdoExtension = DeviceObject->DeviceExtension;
    commonExtension = DeviceObject->DeviceExtension;
    info = fdoExtension->MediaChangeDetectionInfo;

    ASSERT(DO_MCD(fdoExtension) == TRUE);
    ASSERT(info->MediaChangeIrp != NULL);

    //
    // If the sense data field is valid, look for a media change or failure
    // prediction. otherwise this iteration of the polling will just assume
    // nothing changed.
    //

    if(TEST_FLAG(DeviceObject->Characteristics, FILE_REMOVABLE_MEDIA)) {

        if (srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {
            if (srb->SenseInfoBufferLength >=
                offsetof(SENSE_DATA, CommandSpecificInformation)) {

                senseBuffer = srb->SenseInfoBuffer;
                senseKey    = senseBuffer->SenseKey;

                //
                // See if this is a media change.
                //

                if (senseKey == SCSI_SENSE_UNIT_ATTENTION)  {

                    PVPB vpb;

                    if (senseBuffer->AdditionalSenseCode ==
                        SCSI_ADSENSE_MEDIUM_CHANGED) {

                        DebugPrint((1, "ClassMediaChangeCompletion: New media "
                                   "inserted into device %p [irp = %p]\n",
                                fdoExtension->DeviceObject, Irp));

                        ClasspInternalSetMediaChangeState(fdoExtension,
                                                      MediaPresent);
                    }

                    vpb = ClassGetVpb(DeviceObject);

                    if ((vpb != NULL) && (vpb->Flags & VPB_MOUNTED)) {

                        //
                        // Must remember the media changed and force the
                        // file system to verify on next access
                        //

                        DeviceObject->Flags |= DO_VERIFY_VOLUME;
                    }

                    commonExtension->PartitionZeroExtension->MediaChangeCount++;

                } else if((senseKey == SCSI_SENSE_NOT_READY) &&
                          (senseBuffer->AdditionalSenseCode ==
                                SCSI_ADSENSE_NO_MEDIA_IN_DEVICE)) {

                    //
                    // If there was no media in the device then signal the
                    // waiters if we haven't already done so before.
                    //

                    DebugPrint((3, "ClassMediaChangeCompletion: No media in device"
                               "%p [irp = %p]\n",
                            fdoExtension->DeviceObject, Irp));

                    ClasspInternalSetMediaChangeState(fdoExtension,
                                                  MediaNotPresent);
                }


                //
                // Always check for failure prediction
                //

                if ((senseKey == SCSI_SENSE_RECOVERED_ERROR) &&
                          (senseBuffer->AdditionalSenseCode ==
                                SCSI_FAILURE_PREDICTION_THRESHOLD_EXCEEDED)) {

                    //
                    // Drive has reported that a failure prediction threshold has
                    // been exceeded. Better let the world know.
                    //

                    UCHAR wmiEventData[5];

                    *((PULONG)wmiEventData) = sizeof(UCHAR);
                    wmiEventData[sizeof(ULONG)] = senseBuffer->AdditionalSenseCodeQualifier;

                    ClassNotifyFailurePredicted(fdoExtension,
                                                (PUCHAR)&wmiEventData,
                                                sizeof(wmiEventData),
                                                (BOOLEAN)(fdoExtension->FailurePredicted == FALSE),
                                                1,
                                                srb->PathId,
                                                srb->TargetId,
                                                srb->Lun);

                    fdoExtension->FailurePredicted = TRUE;
                    fdoExtension->FailureReason = senseBuffer->AdditionalSenseCodeQualifier;
                }
            }
        } else if ((srb->SrbStatus == SRB_STATUS_SUCCESS) &&
                   (info->MediaChangeDetectionState != MediaPresent)) {
            //
            // We didn't have any media before and now the requests are succeeding
            // we probably missed the Media change somehow.  Signal the change
            // anyway
            //

            DebugPrint((1, "ClassMediaChangeCompletion: Request completed normally"
                           "for device %p which was marked w/NoMedia "
                           "[irp = %p]\n",
                        fdoExtension->DeviceObject, Irp));

            ClasspInternalSetMediaChangeState(fdoExtension, MediaPresent);

        }

    } else {

        if (srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {
            if (srb->SenseInfoBufferLength >=
                offsetof(SENSE_DATA, CommandSpecificInformation)) {

                senseBuffer = srb->SenseInfoBuffer;
                senseKey    = senseBuffer->SenseKey;

                //
                // Always check for failure prediction
                //

                if ((senseKey == SCSI_SENSE_RECOVERED_ERROR) &&
                    (senseBuffer->AdditionalSenseCode ==
                        SCSI_FAILURE_PREDICTION_THRESHOLD_EXCEEDED)) {

                    //
                    // Drive has reported that a failure prediction threshold has
                    // been exceeded. Better let the world know.
                    //

                    UCHAR wmiEventData[5];

                    *((PULONG)wmiEventData) = sizeof(UCHAR);
                    wmiEventData[sizeof(ULONG)] = senseBuffer->AdditionalSenseCodeQualifier;

                    ClassNotifyFailurePredicted(fdoExtension,
                                                (PUCHAR)&wmiEventData,
                                                sizeof(wmiEventData),
                                                (BOOLEAN)(fdoExtension->FailurePredicted == FALSE),
                                                1,
                                                srb->PathId,
                                                srb->TargetId,
                                                srb->Lun);

                    fdoExtension->FailurePredicted = TRUE;
                    fdoExtension->FailureReason = senseBuffer->AdditionalSenseCodeQualifier;
                }
            }
        }
    }

    if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
        ClassReleaseQueue(DeviceObject);
    }

    //
    // Remember the IRP and SRB for use the next time.
    //

    nextIrpStack->Parameters.Scsi.Srb = srb;

    //
    // Reset the timer.
    //

    ClassResetMediaChangeTimer(fdoExtension);

    KeSetEvent(&(info->MediaChangeSynchronizationEvent),
               IO_NO_INCREMENT,
               FALSE);

    if (commonExtension->DevInfo->ClassError) {

        NTSTATUS status;
        BOOLEAN  retry;

        //
        // Throw away the status and retry values. Just give the error
        // routine a chance to do what it needs to.
        //

        commonExtension->DevInfo->ClassError(DeviceObject,
                                             srb,
                                             &status,
                                             &retry);
    }

    if (fdoExtension->DeviceObject->DriverObject->DriverStartIo != NULL) {

        irql = KeRaiseIrqlToDpcLevel();
        IoStartNextPacket(DeviceObject, FALSE);
        KeLowerIrql( irql );

        ClassReleaseRemoveLock(DeviceObject, Irp);

    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

PIRP
ClasspSendTestUnitIrp(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PMEDIA_CHANGE_DETECTION_INFO Info
)
{
    PSCSI_REQUEST_BLOCK srb;
    PIO_STACK_LOCATION irpStack;
    PIO_STACK_LOCATION nextIrpStack;
    PCDB cdb;
    PIRP irp;

    //
    // Setup the IRP to perform a test unit ready.
    //

    irp = Info->MediaChangeIrp;

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    irp->Flags = 0;
    irp->UserBuffer = NULL;

    //
    // If the irp is sent down when the volume needs to be
    // verified, CdRomUpdateGeometryCompletion won't complete
    // it since it's not associated with a thread.  Marking
    // it to override the verify causes it always be sent
    // to the port driver
    //

    irpStack = IoGetCurrentIrpStackLocation(irp);
    irpStack->Flags |= SL_OVERRIDE_VERIFY_VOLUME;

    nextIrpStack = IoGetNextIrpStackLocation(irp);
    nextIrpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    nextIrpStack->Parameters.DeviceIoControl.IoControlCode =
            IOCTL_SCSI_EXECUTE_NONE;

    //
    // Prepare the SRB for execution.
    //
    srb = nextIrpStack->Parameters.Scsi.Srb;
    srb->SrbStatus = srb->ScsiStatus = 0;
    srb->NextSrb = 0;
    srb->Length = SCSI_REQUEST_BLOCK_SIZE;
    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb->SrbFlags = Info->SrbFlags |
                    SRB_FLAGS_NO_DATA_TRANSFER |
                    SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
                    SRB_CLASS_FLAGS_LOW_PRIORITY |
                    SRB_FLAGS_NO_QUEUE_FREEZE;

    srb->DataTransferLength = 0;
    srb->OriginalRequest = irp;

    RtlZeroMemory(srb->SenseInfoBuffer, SENSE_BUFFER_SIZE);
    srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    cdb = (PCDB) &srb->Cdb[0];
    cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
    cdb->CDB6GENERIC.LogicalUnitNumber = srb->Lun;

    IoSetCompletionRoutine(irp,
                           ClasspMediaChangeCompletion,
                           srb,
                           TRUE,
                           TRUE,
                           TRUE);


    return irp;
}

VOID ClasspSendMediaStateIrp(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PMEDIA_CHANGE_DETECTION_INFO Info,
    IN ULONG CountDown
    )
{
    BOOLEAN requestPending = FALSE;
    LARGE_INTEGER zero;
    NTSTATUS status;

    if (Info->MediaChangeIrpCompleted) {
        if (!Info->MediaChangeIrpCompleted == 1) {
            DebugPrint((0, "CdRom%d: Media Change Notification has lost its "
                           "marbles, which usually means the device was "
                           "surprise removed.\n",
                           FdoExtension->DeviceNumber));
            Info->MediaChangeIrpCompleted = 2;
        }
        return;
    }

    if (((FdoExtension->CommonExtension.CurrentState != IRP_MN_START_DEVICE) ||
         (FdoExtension->DevicePowerState != PowerDeviceD0)
         ) &&
        (!Info->MediaChangeIrpLost)) {

        //
        // the device may be stopped, powered down, or otherwise queueing io,
        // so should not timeout the autorun irp (yet) -- set to zero ticks.
        // scattered code relies upon this to not prematurely "lose" an
        // autoplay irp that was queued.
        //

        Info->MediaChangeIrpTimeInUse = 0;
    }

    zero.QuadPart = 0L;
    status = KeWaitForSingleObject(
                &(Info->MediaChangeSynchronizationEvent),
                Executive,
                KernelMode,
                FALSE,
                &zero);

    if(status == STATUS_TIMEOUT) {

        DebugPrint((3, "ClasspSendMediaStateIrp: STATUS_TIMEOUT returned when "
                       "synchronizing for MCD\n"));

        if(Info->MediaChangeIrpLost == FALSE) {
            if(Info->MediaChangeIrpTimeInUse++ > MEDIA_CHANGE_TIMEOUT_TIME) {

                //
                // currently set to five minutes.  hard to imagine a drive
                // taking that long to spin up.
                //

                DebugPrint((0, "CdRom%d: Media Change Notification has lost "
                               "it's irp and doesn't know where to find it.  "
                               "Leave it alone and it'll come home dragging "
                               "it's stack behind it.\n",
                               FdoExtension->DeviceNumber));
                Info->MediaChangeIrpLost = TRUE;
            }
        }
        return;
    }

    TRY {

        if (Info->MediaChangeDetectionDisableCount != 0) {
            DebugPrint((3, "ClassCheckMediaState: device %p has detection "
                        "disabled \n", FdoExtension->DeviceObject));
            LEAVE;
        }

        if (FdoExtension->DevicePowerState != PowerDeviceD0) {

            if (TEST_FLAG(Info->SrbFlags, SRB_FLAGS_NO_KEEP_AWAKE)) {
                DebugPrint((3, "ClassCheckMediaState: device %p is powered "
                            "down and flags are set to let it sleep\n",
                            FdoExtension->DeviceObject));
                ClassResetMediaChangeTimer(FdoExtension);
                LEAVE;
            }

            //
            // NOTE: we don't increment the time in use until our power state
            // changes above.  this way, we won't "lose" the autoplay irp.
            // it's up to the lower driver to determine if powering up is a
            // good idea.
            //

            DebugPrint((1, "ClassCheckMediaState: device %p needs to powerup "
                        "to handle this io (may take a few extra seconds).\n",
                        FdoExtension->DeviceObject));

        }



        Info->MediaChangeIrpTimeInUse = 0;
        Info->MediaChangeIrpLost = FALSE;

        if (CountDown == 0) {

            PIRP irp;

            DebugPrint((2, "ClassCheckMediaState: timer expired\n"));

            if (Info->MediaChangeDetectionDisableCount != 0) {
                DebugPrint((2, "ClassCheckMediaState: detection disabled\n"));
                LEAVE;
            }

            //
            // Prepare the IRP for the test unit ready
            //

            irp = ClasspSendTestUnitIrp(FdoExtension, Info);

            //
            // Issue the request.
            //

            DebugPrint((4, "ClasspSendMediaStateIrp: Device %p getting TUR "
                        " irp %p\n", FdoExtension->DeviceObject, irp));

            if (irp == NULL) {
                LEAVE;
            }


            if (FdoExtension->DeviceObject->DriverObject->DriverStartIo == NULL) {

                IoCallDriver(FdoExtension->DeviceObject, irp);

            } else {

                //
                // don't keep sending this if the device is being removed.
                //

                status = ClassAcquireRemoveLock(FdoExtension->DeviceObject,
                                                irp);
                if (status == REMOVE_COMPLETE) {
                    LEAVE;
                } else if (status != 0) {
                    ClassReleaseRemoveLock(FdoExtension->DeviceObject,
                                           irp);
                    LEAVE;
                }

                IoStartPacket(FdoExtension->DeviceObject, irp, NULL, NULL);

            }

            requestPending = TRUE;

        }

    } FINALLY {

        if(requestPending == FALSE) {
            KeSetEvent(&(Info->MediaChangeSynchronizationEvent),
                       IO_NO_INCREMENT,
                       FALSE);
        }
    }
    return;
}


VOID
ClassCheckMediaState(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
/*++

Routine Description:

    This routine is called by the class driver to test for a media change
    condition and/or poll for disk failure prediction.  It should be called
    from the class driver's IO timer routine once per second.

Arguments:

    FdoExtension - the device extension

Return Value:

    none

--*/

{
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;
    LONG countDown;

    if(info == NULL) {
        DebugPrint((3, "ClassCheckMediaState: detection not enabled\n"));
        return;
    }

    //
    // Media change support is active and the IRP is waiting. Decrement the
    // timer.  There is no MP protection on the timer counter.  This code
    // is the only code that will manipulate the timer and only one
    // instance of it should be running at any given time.
    //

    countDown = InterlockedDecrement(&(info->MediaChangeCountDown));

    //
    // Try to acquire the media change event.  If we can't do it immediately
    // then bail out and assume the caller will try again later.
    //
    ClasspSendMediaStateIrp(FdoExtension,
                            info,
                            countDown);

    return;
}


VOID
ClassResetMediaChangeTimer(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;

    if(info != NULL) {
        InterlockedExchange(&(info->MediaChangeCountDown),
                            MEDIA_CHANGE_DEFAULT_TIME);
    }
    return;
}

NTSTATUS
ClasspInitializePolling(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN BOOLEAN AllowDriveToSleep
    )
{
    PDEVICE_OBJECT fdo = FdoExtension->DeviceObject;

    ULONG size;
    PMEDIA_CHANGE_DETECTION_INFO info;
    PIRP irp;

    PAGED_CODE();

    if (FdoExtension->MediaChangeDetectionInfo != NULL) {
        return STATUS_SUCCESS;
    }


    info = ExAllocatePoolWithTag(NonPagedPool,
                                 sizeof(MEDIA_CHANGE_DETECTION_INFO),
                                 CLASS_TAG_MEDIA_CHANGE_DETECTION);

    if(info != NULL) {
        RtlZeroMemory(info, sizeof(MEDIA_CHANGE_DETECTION_INFO));

        FdoExtension->KernelModeMcnContext.FileObject      = (PVOID)-1;
        FdoExtension->KernelModeMcnContext.DeviceObject    = (PVOID)-1;
        FdoExtension->KernelModeMcnContext.LockCount       = 0;
        FdoExtension->KernelModeMcnContext.McnDisableCount = 0;

        //
        // User wants it - preallocate IRP and SRB.
        //

        irp = IoAllocateIrp((CCHAR)(fdo->StackSize+1),
                            FALSE);
        if (irp != NULL) {

            PVOID buffer;

            buffer = ExAllocatePoolWithTag(
                        NonPagedPoolCacheAligned,
                        SENSE_BUFFER_SIZE,
                        CLASS_TAG_MEDIA_CHANGE_DETECTION);

            if (buffer != NULL) {
                PIO_STACK_LOCATION irpStack;
                PSCSI_REQUEST_BLOCK srb;
                PCDB cdb;

                srb = &(info->MediaChangeSrb);
                info->MediaChangeIrp = irp;
                info->SenseBuffer = buffer;

                //
                // this will help catch the hot unplug error.
                //

                IoSetCompletionRoutine(irp,
                                       ClasspMediaChangeDetectionSanityCheck,
                                       info, TRUE, TRUE, TRUE);

                //
                // All resources have been allocated set up the IRP.
                //

                IoSetNextIrpStackLocation(irp);
                irpStack = IoGetCurrentIrpStackLocation(irp);
                irpStack->DeviceObject = fdo;
                irpStack = IoGetNextIrpStackLocation(irp);
                info->MediaChangeIrp = irp;
                irpStack->Parameters.Scsi.Srb = srb;

                //
                // Initialize the SRB
                //

                RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

                srb->CdbLength = 6;
                srb->TimeOutValue = FdoExtension->TimeOutValue * 2;
                srb->QueueTag = SP_UNTAGGED;
                srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
                srb->Length = SCSI_REQUEST_BLOCK_SIZE;
                srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

                //
                // Initialize and set up the sense information buffer
                //

                RtlZeroMemory(buffer, SENSE_BUFFER_SIZE);
                srb->SenseInfoBuffer = buffer;
                srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

                //
                // Initialize the CDB
                //

                cdb = (PCDB)&srb->Cdb[0];
                cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
                srb->Cdb[15] = 0xff;

                //
                // Set default values for the media change notification
                // configuration.
                //

                info->MediaChangeCountDown = MEDIA_CHANGE_DEFAULT_TIME;
                info->MediaChangeDetectionDisableCount = 0;

                //
                // Assume that there is initially no media in the device
                // only notify upper layers if there is something there
                //

                info->MediaChangeDetectionState = MediaUnknown;

                info->MediaChangeIrpTimeInUse = 0;
                info->MediaChangeIrpLost = FALSE;

                info->SrbFlags = AllowDriveToSleep ?
                                        SRB_FLAGS_NO_KEEP_AWAKE :
                                        0;

                KeInitializeEvent(&(info->MediaChangeSynchronizationEvent),
                                  SynchronizationEvent,
                                  TRUE);

                //
                // It is ok to support media change events on this
                // device.
                //

                FdoExtension->MediaChangeDetectionInfo = info;

                return STATUS_SUCCESS;

            }

            IoFreeIrp(irp);
        }

        ExFreePool(info);
    }

    //
    // nothing to free here
    //
    return STATUS_INSUFFICIENT_RESOURCES;

}

NTSTATUS
ClassInitializeTestUnitPolling(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN BOOLEAN AllowDriveToSleep
    )
{
    return ClasspInitializePolling(FdoExtension, AllowDriveToSleep);
}


VOID
ClassInitializeMediaChangeDetection(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PUCHAR EventPrefix
    )
{
    PDEVICE_OBJECT fdo = FdoExtension->DeviceObject;
    NTSTATUS status;

    PCLASS_DRIVER_EXTENSION driverExtension = ClassGetDriverExtension(
                                                fdo->DriverObject);

    BOOLEAN enabledInRegistry;
    BOOLEAN disabledInRegistry;

    PAGED_CODE();

    //
    // NOTE: This assumes that ClassInitializeMediaChangeDetection is always
    //       called in the context of the ClassInitDevice callback. If called
    //       after then this check will have already been made and the
    //       once a second timer will not have been enabled.
    //

    enabledInRegistry = ClasspMediaChangeAlwaysDisabled(
                            FdoExtension,
                            &(driverExtension->RegistryPath)
                            );

    if (enabledInRegistry) {

        disabledInRegistry = FALSE;

        status = ClassMediaChangeDeviceInstanceDisabled(
                    FdoExtension,
                    &disabledInRegistry,
                    FALSE);

        if (NT_SUCCESS(status) && disabledInRegistry == TRUE) {
            return;
        }

        ClasspInitializePolling(FdoExtension, FALSE);
    }

    return;
}


NTSTATUS
ClassMediaChangeDeviceInstanceDisabled(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN OUT PBOOLEAN Disabled,
    IN BOOLEAN SetValue
    )
/*++

Routine Description:

    The user can override the global setting to enable or disable Autorun on a
    specific cdrom device via the control panel.  This routine checks and/or
    sets this value.

Arguments:

    FdoExtension - the device to set/get the value for
    Value        - the value to use in a set
    SetValue     - whether to set the value

Return Value:

    TRUE - Autorun is disabled (default)
    FALSE - Autorun is enabled

--*/
{
    HANDLE                   deviceParameterHandle;  // cdrom instance key
    HANDLE                   driverParameterHandle;  // cdrom specific key
    RTL_QUERY_REGISTRY_TABLE queryTable[2];
    OBJECT_ATTRIBUTES        objectAttributes;
    UNICODE_STRING           subkeyName;
    NTSTATUS                 status;
    ULONG                    registryValue;

    PAGED_CODE();

    deviceParameterHandle = NULL;
    driverParameterHandle = NULL;
    status = STATUS_SUCCESS;

#define CDROM_REG_SUBKEY_NAME                   (L"CdRom")
#define CDROM_REG_AUTORUN_DISABLE_INSTANCE_NAME (L"DisableAutorun")

    TRY {

        status = IoOpenDeviceRegistryKey( FdoExtension->LowerPdo,
                                          PLUGPLAY_REGKEY_DEVICE,
                                          KEY_ALL_ACCESS,
                                          &deviceParameterHandle
                                          );
        if (!NT_SUCCESS(status)) {

            //
            // this can occur when a new device is added to the system
            // this is due to cdrom.sys being an 'essential' driver
            //
            DebugPrint((1, "ClassMediaChangeDeviceInstanceValue: "
                        "Could not open device registry key [%lx]\n", status));
            LEAVE;
        }

        RtlInitUnicodeString(&subkeyName, CDROM_REG_SUBKEY_NAME);
        InitializeObjectAttributes(&objectAttributes,
                                   &subkeyName,
                                   OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                   deviceParameterHandle,
                                   (PSECURITY_DESCRIPTOR) NULL);

        status = ZwCreateKey(&driverParameterHandle,
                             KEY_READ | KEY_WRITE,
                             &objectAttributes,
                             0,
                             (PUNICODE_STRING) NULL,
                             REG_OPTION_NON_VOLATILE,
                             NULL);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1, "ClassMediaChangeDeviceInstanceValue: "
                        "subkey could not be created. %lx\n", status));
            LEAVE;
        }

        RtlZeroMemory(&queryTable[0], sizeof(queryTable));

        //
        // Default to not disabling autorun, based upon setting
        // registryValue to zero.
        //

        registryValue = 0;

        queryTable[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        queryTable[0].Name          = CDROM_REG_AUTORUN_DISABLE_INSTANCE_NAME;
        queryTable[0].EntryContext  = &registryValue;
        queryTable[0].DefaultType   = REG_DWORD;
        queryTable[0].DefaultData   = &registryValue;
        queryTable[0].DefaultLength = 0;

        status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                        (PWSTR)driverParameterHandle,
                                        queryTable,
                                        NULL,
                                        NULL);

        //
        // may be able to write the value even if couldn't query it?
        //

        if ((!NT_SUCCESS(status)) && (!SetValue)) {
            DebugPrint((1, "ClassMediaChangeDeviceInstanceValue: "
                        "could not query DisableAutorun for Device %p (%lx)\n",
                        FdoExtension->DeviceObject, status));
            LEAVE;
        }

        if (!SetValue) {

            *Disabled = (registryValue?1:0);
            DebugPrint((2, "ClassMediaChangeDeviceInstanceValue: "
                        "Autorun is%s disabled for device %p\n",
                        (registryValue ? "" : " not"),
                        FdoExtension->DeviceObject));
            LEAVE;

        }

        if ((registryValue?1:0) == ((*Disabled)?1:0)) {

            //
            // no need to change the value in the registry
            //

            DebugPrint((2, "ClassMediaChangeDeviceInstanceValue: "
                        "Autorun was already %s for device %p\n",
                        (registryValue ? "disabled" : "enabled"),
                        FdoExtension->DeviceObject));
            LEAVE;

        }

        registryValue = *Disabled;
        status = RtlWriteRegistryValue(RTL_REGISTRY_HANDLE,
                                       (PWSTR)driverParameterHandle,
                                       CDROM_REG_AUTORUN_DISABLE_INSTANCE_NAME,
                                       REG_DWORD,
                                       &registryValue,
                                       sizeof(registryValue));
        if (!NT_SUCCESS(status)) {
            DebugPrint((2, "ClassMediaChangeDeviceInstanceValue: "
                        "could not write AutoRun value for device %p (%lx)\n",
                        FdoExtension->DeviceObject,
                        status));
            LEAVE;
        }

        DebugPrint((2, "ClassMediaChangeDeviceInstanceValue: "
                    "Successfully set the autorun value to %s for device %p\n",
                    (registryValue ? "disabled" : "enabled"),
                    FdoExtension->DeviceObject));

    } FINALLY {

        if (driverParameterHandle) ZwClose(driverParameterHandle);
        if (deviceParameterHandle) ZwClose(deviceParameterHandle);

        if (!NT_SUCCESS(status)) {
            *Disabled = 0; // default to not disabling autorun
        }

    }

    return status;
}

BOOLEAN
ClasspMediaChangeAlwaysDisabled(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    The user must specify that AutoPlay is to run on the platform
    by setting the registry value HKEY_LOCAL_MACHINE\System\CurrentControlSet\
    Services\Cdrom\Autorun:REG_DWORD:1.

    The user can override the global setting to enable or disable Autorun on a
    specific cdrom device via the control panel.

Arguments:

    FdoExtension -
    RegistryPath - pointer to the unicode string inside
                   ...\CurrentControlSet\Services\Cdrom

Return Value:

    TRUE - Autorun is enabled.
    FALSE - no autorun.

--*/

{
    PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor = FdoExtension->DeviceDescriptor;

    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE serviceKey = NULL;
    HANDLE parametersKey = NULL;
    RTL_QUERY_REGISTRY_TABLE parameters[3];

    UNICODE_STRING deviceUnicodeString;
    ANSI_STRING deviceString;

    ULONG             valueFound = FALSE;
    LONG              zero = 0;
    LONG              doRun = 0;

    UNICODE_STRING    paramStr;

    NTSTATUS          status;

    PAGED_CODE();

    //
    // open the service key.
    //

    InitializeObjectAttributes(&objectAttributes,
                               RegistryPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = ZwOpenKey(&serviceKey,
                       KEY_READ,
                       &objectAttributes);

    ASSERT(NT_SUCCESS(status));

    if(!NT_SUCCESS(status)) {
        return FALSE;
    }

    TRY {
        //
        // Determine if drive is in a list of those requiring
        // autorun to be disabled.  this is stored in a REG_MULTI_SZ
        // named AutoRunAlwaysDisable.  this is required as some autochangers
        // must load the disc to reply to ChkVerify request, causing them
        // to cycle discs continuously.
        //

        PWSTR nullMultiSz;
        PUCHAR vendorId;
        PUCHAR productId;
        PUCHAR revisionId;
        ULONG  length;
        ULONG  offset;

        deviceString.Buffer        = NULL;
        deviceUnicodeString.Buffer = NULL;

        //
        // there may be nothing to check against
        //

        if ((deviceDescriptor->VendorIdOffset == 0) &&
            (deviceDescriptor->ProductIdOffset == 0)) {
            LEAVE;
        }

        length = 0;

        if (deviceDescriptor->VendorIdOffset == 0) {
            vendorId = NULL;
        } else {
            vendorId = (PUCHAR) deviceDescriptor + deviceDescriptor->VendorIdOffset;
            length = strlen(vendorId);
        }

        if ( deviceDescriptor->ProductIdOffset == 0 ) {
            productId = NULL;
        } else {
            productId = (PUCHAR) deviceDescriptor + deviceDescriptor->ProductIdOffset;
            length += strlen(productId);
        }

        if ( deviceDescriptor->ProductRevisionOffset == 0 ) {
            revisionId = NULL;
        } else {
            revisionId = (PUCHAR) deviceDescriptor + deviceDescriptor->ProductRevisionOffset;
            length += strlen(revisionId);
        }

        //
        // allocate a buffer for the string
        //

        deviceString.Length = (USHORT)( length );
        deviceString.MaximumLength = deviceString.Length + 1;
        deviceString.Buffer = (PUCHAR)ExAllocatePoolWithTag( NonPagedPool,
                                                             deviceString.MaximumLength,
                                                             CLASS_TAG_AUTORUN_DISABLE
                                                             );
        if (deviceString.Buffer == NULL) {
            DebugPrint((1, "CdRomCheckRegAP: Unable to alloc string buffer\n" ));
            LEAVE;
        }

        //
        // copy strings to the buffer
        //
        offset = 0;

        if (vendorId != NULL) {
            RtlCopyMemory(deviceString.Buffer + offset,
                          vendorId,
                          strlen(vendorId));
            offset += strlen(vendorId);
        }

        if ( productId != NULL ) {
            RtlCopyMemory(deviceString.Buffer + offset,
                          productId,
                          strlen(productId));
            offset += strlen(productId);
        }
        if ( revisionId != NULL ) {
            RtlCopyMemory(deviceString.Buffer + offset,
                          revisionId,
                          strlen(revisionId));
            offset += strlen(revisionId);
        }

        ASSERT(offset == deviceString.Length);

        deviceString.Buffer[deviceString.Length] = '\0';  // Null-terminated

        //
        // convert to unicode as registry deals with unicode strings
        //

        status = RtlAnsiStringToUnicodeString( &deviceUnicodeString,
                                               &deviceString,
                                               TRUE
                                               );
        if (!NT_SUCCESS(status)) {
            DebugPrint((1, "CdRomCheckRegAP: cannot convert to unicode %lx\n", status ));
            LEAVE;
        }

        //
        // query the value, setting valueFound to true if found
        //

        RtlZeroMemory(parameters, sizeof(parameters));

        nullMultiSz = L"\0";
        parameters[0].QueryRoutine  = ClasspMediaChangeRegistryCallBack;
        parameters[0].Flags         = RTL_QUERY_REGISTRY_REQUIRED;
        parameters[0].Name          = L"AutoRunAlwaysDisable";
        parameters[0].EntryContext  = &valueFound;
        parameters[0].DefaultType   = REG_MULTI_SZ;
        parameters[0].DefaultData   = nullMultiSz;
        parameters[0].DefaultLength = 0;

        status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                        serviceKey,
                                        parameters,
                                        &deviceUnicodeString,
                                        NULL);

        if ( !NT_SUCCESS(status) ) {
            LEAVE;
        }

    } FINALLY {

        if (deviceString.Buffer != NULL) {
            ExFreePool( deviceString.Buffer );
        }
        if (deviceUnicodeString.Buffer != NULL) {
            RtlFreeUnicodeString( &deviceUnicodeString );
        }

        if (valueFound == TRUE) {
            DebugPrint((1, "CdRomCheckRegAP: Drive is on disable list\n" ));
            return FALSE;
        }

    }

    RtlZeroMemory(parameters, sizeof(parameters));

    //
    // Open the parameters key (if any) beneath the services key.
    //

    RtlInitUnicodeString(&paramStr, L"Parameters");

    InitializeObjectAttributes(&objectAttributes,
                               &paramStr,
                               OBJ_CASE_INSENSITIVE,
                               serviceKey,
                               NULL);

    status = ZwOpenKey(&parametersKey,
                       KEY_READ,
                       &objectAttributes);

    //
    // Check for the Autorun value.
    //

    parameters[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    parameters[0].Name          = L"Autorun";
    parameters[0].EntryContext  = &doRun;
    parameters[0].DefaultType   = REG_DWORD;
    parameters[0].DefaultData   = &zero;
    parameters[0].DefaultLength = sizeof(ULONG);

    status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE | RTL_REGISTRY_OPTIONAL,
                                    serviceKey,
                                    parameters,
                                    NULL,
                                    NULL);

    DebugPrint((1, "CdRomCheckRegAP: cdrom/Autorun flag = %d\n", doRun));

    if(parametersKey != NULL) {

        parameters[0].DefaultData   = &doRun;

        status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE | RTL_REGISTRY_OPTIONAL,
                                        parametersKey,
                                        parameters,
                                        NULL,
                                        NULL);

        DebugPrint((1, "CdRomCheckRegAP: cdrom/parameters/autorun flag = %d\n", doRun));

        ZwClose(parametersKey);
    }

    DebugPrint((1, "CdRomCheckRegAP: Autoplay for device %p is %s\n",
                FdoExtension->DeviceObject,
                (doRun ? "on" : "off")));

    ZwClose(serviceKey);

    if(doRun) {
        return TRUE;
    }

    DebugPrint((1, "CdRomCheckRegAP: Autoplay disabled for all devices\n"));
    return FALSE;
}


VOID
ClassEnableMediaChangeDetection(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;
    LONG oldCount;

    PAGED_CODE();

    if(info == NULL) {
        DebugPrint((2, "ClassEnableMediaChangeDetection: not initialized\n"));
        return;
    }

    KeWaitForSingleObject(&(info->MediaChangeSynchronizationEvent),
                          UserRequest,
                          UserMode,
                          FALSE,
                          NULL);


    oldCount = --info->MediaChangeDetectionDisableCount;

    ASSERT(oldCount >= 0);

    DebugPrint((2, "ClassEnableMediaChangeDetection: Disable count "
                "reduced to %d - ",
                info->MediaChangeDetectionDisableCount));

    if(oldCount == 0) {

        //
        // We don't know what state the media is in anymore.
        //

        ClasspInternalSetMediaChangeState(FdoExtension, MediaUnknown);

        //
        // Reset the timer.
        //

        ClassResetMediaChangeTimer(FdoExtension);

        //
        // Reenable once a second timer
        //
        ClasspEnableTimer(FdoExtension->DeviceObject);

        DebugPrint((1, "MCD is enabled\n"));
    } else {
        DebugPrint((1, "MCD still disabled\n"));
    }


    //
    // Let something else run.
    //

    KeSetEvent(&(info->MediaChangeSynchronizationEvent),
               IO_NO_INCREMENT,
               FALSE);

    return;
}


VOID
ClassDisableMediaChangeDetection(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;

    PAGED_CODE();

    if(info == NULL) {
        return;
    }

    KeWaitForSingleObject(&(info->MediaChangeSynchronizationEvent),
                          UserRequest,
                          UserMode,
                          FALSE,
                          NULL);

    //
    // On transition from enabled to disabled, disable the once a second
    // timer.
    //

    if (info->MediaChangeDetectionDisableCount == 0) {
        ClasspDisableTimer(FdoExtension->DeviceObject);
    }

    info->MediaChangeDetectionDisableCount++;

    DebugPrint((1, "ClassDisableMediaChangeDetection: disable count is %d\n",
                info->MediaChangeDetectionDisableCount));

    KeSetEvent(&(info->MediaChangeSynchronizationEvent),
               IO_NO_INCREMENT,
               FALSE);

    return;
}


VOID
ClassCleanupMediaChangeDetection(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    PMEDIA_CHANGE_DETECTION_INFO info = FdoExtension->MediaChangeDetectionInfo;

    PAGED_CODE()

    if(info == NULL) {
        return;
    }

    FdoExtension->MediaChangeDetectionInfo = NULL;

    IoFreeIrp(info->MediaChangeIrp);
    ExFreePool(info->SenseBuffer);

    ExFreePool(info);
    return;
}


NTSTATUS
ClasspMcnControl(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PIRP Irp,
    IN PSCSI_REQUEST_BLOCK Srb
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension =
        (PCOMMON_DEVICE_EXTENSION) FdoExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PPREVENT_MEDIA_REMOVAL request = Irp->AssociatedIrp.SystemBuffer;

    PFILE_OBJECT fileObject = irpStack->FileObject;
    PFILE_OBJECT_EXTENSION fsContext = NULL;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    //
    // Check to make sure we have a file object extension to keep track of this
    // request.  If not we'll fail it before synchronizing.
    //

    TRY {

        if(fileObject != NULL) {
            fsContext = ClasspGetFsContext(commonExtension, fileObject);
        }else if(Irp->RequestorMode == KernelMode) { // && fileObject == NULL
            fsContext = &FdoExtension->KernelModeMcnContext;
        }

        if (fsContext == NULL) {

            //
            // This handle isn't setup correctly.  We can't let the
            // operation go.
            //

            status = STATUS_INVALID_PARAMETER;
            LEAVE;
        }

        if(request->PreventMediaRemoval) {

            //
            // This is a lock command.  Reissue the command in case bus or
            // device was reset and the lock was cleared.
            //

            ClassDisableMediaChangeDetection(FdoExtension);
            InterlockedIncrement(&(fsContext->McnDisableCount));

        } else {

            if(fsContext->McnDisableCount == 0) {
                status = STATUS_INVALID_DEVICE_STATE;
                LEAVE;
            }

            InterlockedDecrement(&(fsContext->McnDisableCount));
            ClassEnableMediaChangeDetection(FdoExtension);
        }

    } FINALLY {

        Irp->IoStatus.Status = status;

        if(Srb) {
            ExFreePool(Srb);
        }

        ClassReleaseRemoveLock(FdoExtension->DeviceObject, Irp);
        ClassCompleteRequest(FdoExtension->DeviceObject,
                             Irp,
                             IO_NO_INCREMENT);
    }
    return status;
}



NTSTATUS
ClasspMediaChangeRegistryCallBack(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
/*++

Routine Description:

    This callback for a registry SZ or MULTI_SZ is called once for each
    SZ in the value.  It will attempt to match the data with the
    UNICODE_STRING passed in as Context, and modify EntryContext if a
    match is found.  Written for ClasspCheckRegistryForMediaChangeCompletion

Arguments:

    ValueName     - name of the key that was opened
    ValueType     - type of data stored in the value (REG_SZ for this routine)
    ValueData     - data in the registry, in this case a wide string
    ValueLength   - length of the data including the terminating null
    Context       - unicode string to compare against ValueData
    EntryContext  - should be initialized to 0, will be set to 1 if match found

Return Value:

    STATUS_SUCCESS
    EntryContext will be 1 if found

--*/
{
    PULONG valueFound;
    PUNICODE_STRING deviceString;
    PWSTR keyValue;

    PAGED_CODE();
    UNREFERENCED_PARAMETER(ValueName);


    //
    // if we have already set the value to true, exit
    //

    valueFound = EntryContext;
    if (*valueFound == TRUE) {
        DebugPrint((2, "ClasspRegCB: already set to true\n"));
        return STATUS_SUCCESS;
    }

    //
    // if the data is not a terminated string, exit
    //

    if (ValueType != REG_SZ) {
        return STATUS_SUCCESS;
    }

    deviceString = Context;
    keyValue = ValueData;
    ValueLength -= sizeof(WCHAR); // ignore the null character

    //
    // do not compare more memory than is in deviceString
    //

    if (ValueLength > deviceString->Length) {
        ValueLength = deviceString->Length;
    }

    //
    // if the strings match, disable autorun
    //

    if (RtlCompareMemory(deviceString->Buffer, keyValue, ValueLength) == ValueLength) {
        DebugPrint((2, "ClasspRegCB: Match found\n"));
        DebugPrint((2, "ClasspRegCB: DeviceString at %p\n", deviceString->Buffer));
        DebugPrint((2, "ClasspRegCB: KeyValue at %p\n", keyValue));
        *valueFound = 1;
    }

    return STATUS_SUCCESS;
}

VOID
ClasspTimerTick(
    PDEVICE_OBJECT DeviceObject,
    PVOID Context
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    ULONG isRemoved;

    ASSERT(commonExtension->IsFdo);

    //
    // Do any media change work
    //
    isRemoved = ClassAcquireRemoveLock(DeviceObject, (PIRP)ClasspTimerTick);

    //
    // We stop the timer before deleting the device.  It's safe to keep going
    // if the flag value is REMOVE_PENDING because the removal thread will be
    // blocked trying to stop the timer.
    //

    ASSERT(isRemoved != REMOVE_COMPLETE);

    //
    // This routine is reasonably safe even if the device object has a pending
    // remove

    if(!isRemoved) {

        PFAILURE_PREDICTION_INFO info = fdoExtension->FailurePredictionInfo;

        //
        // Do any media change detection work
        //

        if (fdoExtension->MediaChangeDetectionInfo != NULL) {

            ClassCheckMediaState(fdoExtension);

        }

        //
        // Do any failure prediction work
        //
        if ((info != NULL) && (info->Method != FailurePredictionNone)) {

            ULONG countDown;
            ULONG active;

            if (ClasspCanSendPollingIrp(fdoExtension)) {

                //
                // Synchronization is not required here since the Interlocked
                // locked instruction guarantees atomicity. Other code that
                // resets CountDown uses InterlockedExchange which is also
                // atomic.
                //
                countDown = InterlockedDecrement(&info->CountDown);
                if (countDown == 0) {

                    DebugPrint((4, "ClasspTimerTick: Send FP irp for %p\n",
                                   DeviceObject));

                    if(info->WorkQueueItem == NULL) {

                        info->WorkQueueItem =
                            IoAllocateWorkItem(fdoExtension->DeviceObject);

                        if(info->WorkQueueItem == NULL) {

                            //
                            // Set the countdown to one minute in the future.
                            // we'll try again then in the hopes there's more
                            // free memory.
                            //

                            DebugPrint((1, "ClassTimerTick: Couldn't allocate "
                                           "item - try again in one minute\n"));
                            InterlockedExchange(&info->CountDown, 60);

                        } else {

                            //
                            // Grab the remove lock so that removal will block
                            // until the work item is done.
                            //

                            ClassAcquireRemoveLock(fdoExtension->DeviceObject,
                                                   info->WorkQueueItem);

                            IoQueueWorkItem(info->WorkQueueItem,
                                            ClasspFailurePredict,
                                            DelayedWorkQueue,
                                            info);
                        }

                    } else {

                        DebugPrint((3, "ClasspTimerTick: Failure "
                                       "Prediction work item is "
                                       "already active for device %p\n",
                                    DeviceObject));

                    }
                } // end (countdown == 0)

            } else {
                //
                // If device is sleeping then just rearm polling timer
                DebugPrint((4, "ClassTimerTick, SHHHH!!! device is %p is sleeping\n",
                            DeviceObject));
            }

        } // end failure prediction polling

        //
        // Give driver a chance to do its own specific work
        //

        if (commonExtension->DriverExtension->InitData.ClassTick != NULL) {

            commonExtension->DriverExtension->InitData.ClassTick(DeviceObject);

        } // end device specific tick handler
    } // end check for removed

    ClassReleaseRemoveLock(DeviceObject, (PIRP)ClasspTimerTick);
}


NTSTATUS
ClasspEnableTimer(
    PDEVICE_OBJECT DeviceObject
    )
{
    NTSTATUS status;

    PAGED_CODE();

    if (DeviceObject->Timer == NULL) {

        status = IoInitializeTimer(DeviceObject, ClasspTimerTick, NULL);

    } else {

        status = STATUS_SUCCESS;

    }

    if (NT_SUCCESS(status)) {

        IoStartTimer(DeviceObject);
        DebugPrint((3, "ClasspEnableTimer: Once a second timer enabled "
                    "for device %p\n", DeviceObject));

    }

    DebugPrint((1, "ClasspEnableTimer: Device %p, Status %lx "
                "initializing timer\n", DeviceObject, status));

    return status;

}

NTSTATUS
ClasspDisableTimer(
    PDEVICE_OBJECT DeviceObject
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PMEDIA_CHANGE_DETECTION_INFO mCDInfo = fdoExtension->MediaChangeDetectionInfo;
    PFAILURE_PREDICTION_INFO fPInfo = fdoExtension->FailurePredictionInfo;
    NTSTATUS status;

    PAGED_CODE();

    if (DeviceObject->Timer != NULL) {

        //
        // If there is no more reason to have the once a second timer then
        // disable it. To keep the timer running we need either a ClassTick
        // routine, have media change detection enabled or have failure
        // prediction polling enabled.
        //

        if (
            // no class tick routine
            (commonExtension->DriverExtension->InitData.ClassTick == NULL)
            &&
            // no fault prediction
            ((fPInfo == NULL) || (fPInfo->Method == FailurePredictionNone))
            &&
            // no media change notification
            ((mCDInfo == NULL) ||
             (mCDInfo->MediaChangeDetectionDisableCount > 0))
            // disable the timer
            ) {

            IoStopTimer(DeviceObject);
            DebugPrint((3, "ClasspEnableTimer: Once a second timer disabled "
                        "for device %p\n", DeviceObject));
        }

    } else {

        DebugPrint((1, "ClasspDisableTimer: Timer never enabled\n"));

    }

    return STATUS_SUCCESS;
}

VOID
ClasspFailurePredict(
    IN PDEVICE_OBJECT DeviceObject,
    IN PFAILURE_PREDICTION_INFO Info
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PIO_WORKITEM workItem;
    STORAGE_PREDICT_FAILURE checkFailure;
    SCSI_ADDRESS scsiAddress;

    NTSTATUS status;

    PAGED_CODE();

    ASSERT(Info != NULL);

    DebugPrint((1, "ClasspFailurePredict: Polling for failure\n"));

    //
    // Mark the work item as inactive and reset the countdown timer.  we
    // can't risk freeing the work item until we've released the remove-lock
    // though - if we do it might get resused as a tag before we can release
    // the lock.
    //

    InterlockedExchange(&Info->CountDown, Info->Period);
    workItem = InterlockedExchangePointer(&(Info->WorkQueueItem), NULL);

    if (ClasspCanSendPollingIrp(fdoExtension)) {

        KEVENT event;
        PDEVICE_OBJECT topOfStack;
        PIRP irp = NULL;
        IO_STATUS_BLOCK ioStatus;

        KeInitializeEvent(&event, SynchronizationEvent, FALSE);

        topOfStack = IoGetAttachedDeviceReference(DeviceObject);

        //
        // Send down irp to see if drive is predicting failure
        //

        irp = IoBuildDeviceIoControlRequest(
                        IOCTL_STORAGE_PREDICT_FAILURE,
                        topOfStack,
                        NULL,
                        0,
                        &checkFailure,
                        sizeof(STORAGE_PREDICT_FAILURE),
                        FALSE,
                        &event,
                        &ioStatus);


        if (irp != NULL) {
            status = IoCallDriver(topOfStack, irp);
            if (status == STATUS_PENDING) {
                KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
                status = ioStatus.Status;
            }
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }

        if (NT_SUCCESS(status) && (checkFailure.PredictFailure)) {

            checkFailure.PredictFailure = 512;

            //
            // Send down irp to get scsi address
            //
            KeInitializeEvent(&event, SynchronizationEvent, FALSE);

            RtlZeroMemory(&scsiAddress, sizeof(SCSI_ADDRESS));
            irp = IoBuildDeviceIoControlRequest(
                IOCTL_SCSI_GET_ADDRESS,
                topOfStack,
                NULL,
                0,
                &scsiAddress,
                sizeof(SCSI_ADDRESS),
                FALSE,
                &event,
                &ioStatus);

            if (irp != NULL) {
                status = IoCallDriver(topOfStack, irp);
                if (status == STATUS_PENDING) {
                    KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
                    status = ioStatus.Status;
                }
            }

            ClassNotifyFailurePredicted(fdoExtension,
                                    (PUCHAR)&checkFailure,
                                    sizeof(checkFailure),
                                    (BOOLEAN)(fdoExtension->FailurePredicted == FALSE),
                                    2,
                                    scsiAddress.PathId,
                                    scsiAddress.TargetId,
                                    scsiAddress.Lun);

            fdoExtension->FailurePredicted = TRUE;

        }

        ObDereferenceObject(topOfStack);
    }

    ClassReleaseRemoveLock(DeviceObject, (PIRP) workItem);
    IoFreeWorkItem(workItem);
    return;
}


VOID
ClassNotifyFailurePredicted(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PUCHAR Buffer,
    ULONG BufferSize,
    BOOLEAN LogError,
    ULONG UniqueErrorValue,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun
    )
{
    PIO_ERROR_LOG_PACKET logEntry;

    DebugPrint((1, "ClasspFailurePredictPollCompletion: Failure predicted for device %p\n", FdoExtension->DeviceObject));

    //
    // Fire off a WMI event
    //
    ClassWmiFireEvent(FdoExtension->DeviceObject,
                                   &StoragePredictFailureEventGuid,
                                   0,
                                   BufferSize,
                                   Buffer);

    //
    // Log an error into the eventlog
    //

    if (LogError)
    {
        logEntry = IoAllocateErrorLogEntry(
                            FdoExtension->DeviceObject,
                           sizeof(IO_ERROR_LOG_PACKET) + (3 * sizeof(ULONG)));

        if (logEntry != NULL)
        {

            logEntry->FinalStatus     = STATUS_SUCCESS;
            logEntry->ErrorCode       = IO_WRN_FAILURE_PREDICTED;
            logEntry->SequenceNumber  = 0;
            logEntry->MajorFunctionCode = IRP_MJ_DEVICE_CONTROL;
            logEntry->IoControlCode   = IOCTL_STORAGE_PREDICT_FAILURE;
            logEntry->RetryCount      = 0;
            logEntry->UniqueErrorValue = UniqueErrorValue;
            logEntry->DumpDataSize    = 3;

            logEntry->DumpData[0] = PathId;
            logEntry->DumpData[1] = TargetId;
            logEntry->DumpData[2] = Lun;

            //
            // Write the error log packet.
            //

            IoWriteErrorLogEntry(logEntry);
        }
    }
}


NTSTATUS
ClassSetFailurePredictionPoll(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    FAILURE_PREDICTION_METHOD FailurePredictionMethod,
    ULONG PollingPeriod
    )
/*++

Routine Description:

    Enable or disable specific polling for failure prediction

Arguments:

    FdoExtension

    FailurePredictionMethod has specific failure prediction method

    PollingPeriod - if 0 then no change to current polling timer

Return Value:

    NT Status

--*/
{
    PFAILURE_PREDICTION_INFO info;
    NTSTATUS status;
    DEVICE_POWER_STATE powerState;

    PAGED_CODE();

    if (FdoExtension->FailurePredictionInfo == NULL) {

        if (FailurePredictionMethod != FailurePredictionNone) {

            info = ExAllocatePoolWithTag(NonPagedPool,
                                         sizeof(FAILURE_PREDICTION_INFO),
                                         CLASS_TAG_FAILURE_PREDICT);

            if (info == NULL) {

                return STATUS_INSUFFICIENT_RESOURCES;

            }

            KeInitializeEvent(&info->Event, SynchronizationEvent, TRUE);

            info->WorkQueueItem = NULL;
            info->Period = DEFAULT_FAILURE_PREDICTION_PERIOD;

        } else {

            //
            // FaultPrediction has not been previously initialized, nor
            // is it being initialized now. No need to do anything.
            //
            return STATUS_SUCCESS;

        }

        FdoExtension->FailurePredictionInfo = info;

    } else {

        info = FdoExtension->FailurePredictionInfo;

    }

    KeWaitForSingleObject(&info->Event,
                          UserRequest,
                          UserMode,
                          FALSE,
                          NULL);


    //
    // Reset polling period and counter. Setup failure detection type
    //

    if (PollingPeriod != 0) {

        InterlockedExchange(&info->Period, PollingPeriod);

    }

    InterlockedExchange(&info->CountDown, info->Period);

    info->Method = FailurePredictionMethod;
    if (FailurePredictionMethod != FailurePredictionNone) {

        status = ClasspEnableTimer(FdoExtension->DeviceObject);

        if (NT_SUCCESS(status)) {
            DebugPrint((3, "ClassEnableFailurePredictPoll: Enabled for "
                        "device %p\n", FdoExtension->DeviceObject));
        }

    } else {

        status = ClasspDisableTimer(FdoExtension->DeviceObject);
        DebugPrint((3, "ClassEnableFailurePredictPoll: Disabled for "
                    "device %p\n", FdoExtension->DeviceObject));
        status = STATUS_SUCCESS;

    }

    KeSetEvent(&info->Event, IO_NO_INCREMENT, FALSE);

    return status;
}

