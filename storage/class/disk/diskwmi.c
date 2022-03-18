/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    diskwmi.c

Abstract:

    SCSI disk class driver - WMI support routines

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "disk.h"

NTSTATUS
DiskSendFailurePredictIoctl(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PSTORAGE_PREDICT_FAILURE checkFailure
    );

NTSTATUS
DiskGetIdentifyInfo(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PBOOLEAN SupportSmart
    );

NTSTATUS
DiskDetectFailurePrediction(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PFAILURE_PREDICTION_METHOD FailurePredictCapability
    );


GUIDREGINFO DiskWmiFdoGuidList[] =
{
    {
        WMI_DISK_GEOMETRY_GUID,
        1,
        0
    },

    {
        WMI_STORAGE_FAILURE_PREDICT_STATUS_GUID,
        1,
        WMIREG_FLAG_EXPENSIVE
    },
    {
        WMI_STORAGE_FAILURE_PREDICT_DATA_GUID,
        1,
        WMIREG_FLAG_EXPENSIVE
    },

    {
        WMI_STORAGE_FAILURE_PREDICT_FUNCTION_GUID,
        1,
        WMIREG_FLAG_EXPENSIVE
    },

    {
        WMI_STORAGE_PREDICT_FAILURE_EVENT_GUID,
        1,
        WMIREG_FLAG_EVENT_ONLY_GUID
    },

};


GUID DiskPredictFailureEventGuid = WMI_STORAGE_PREDICT_FAILURE_EVENT_GUID;

#define DiskGeometryGuid           0
#define SmartStatusGuid            1
#define SmartDataGuid              2
#define SmartPerformFunction       3
    #define AllowDisallowPerformanceHit                 1
    #define EnableDisableHardwareFailurePrediction      2
    #define EnableDisableFailurePredictionPolling       3
    #define GetFailurePredictionCapability              4
    #define EnableOfflineDiags                          5

#define SmartEventGuid             4

#if 0
    //
    // Enable this to add WMI support for PDOs
GUIDREGINFO DiskWmiPdoGuidList[] =
{
    {
        // {25007F51-57C2-11d1-A528-00A0C9062910}
        { 0x25007f52, 0x57c2, 0x11d1,
                       { 0xa5, 0x28, 0x0, 0xa0, 0xc9, 0x6, 0x29, 0x10 } },
        0
    },

};

ULONG DiskDummyData[4] = { 1, 2, 3, 4};
#endif

#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, DiskWmiFunctionControl)
#pragma alloc_text(PAGE, DiskFdoQueryWmiRegInfo)
#pragma alloc_text(PAGE, DiskFdoQueryWmiDataBlock)
#pragma alloc_text(PAGE, DiskFdoSetWmiDataBlock)
#pragma alloc_text(PAGE, DiskFdoSetWmiDataItem)
#pragma alloc_text(PAGE, DiskFdoExecuteWmiMethod)

#pragma alloc_text(PAGE, DiskDetectFailurePrediction)
#pragma alloc_text(PAGE, DiskEnableDisableFailurePrediction)
#pragma alloc_text(PAGE, DiskEnableDisableFailurePredictPolling)
#pragma alloc_text(PAGE, DiskReadFailurePredictStatus)
#pragma alloc_text(PAGE, DiskReadFailurePredictData)
#pragma alloc_text(PAGE, DiskGetIdentifyInfo)

#pragma alloc_text(PAGE, DiskPerformSmartCommand)

#pragma alloc_text(PAGE, DiskSendFailurePredictIoctl)

#endif


//
// SMART/IDE specific routines

//
// Read SMART data attributes.
// SrbControl should be sizeof(SRB_IO_CONTROL) +
//                      (sizeof(SENDCMDINPARAMS)-1) +
//                      READ_ATTRIBUTE_BUFFER_SIZE
// Attribute data returned at &SendCmdOutParams->bBuffer[0]
//
#define DiskReadSmartData(FdoExtension, \
                          SrbControl, \
                          BufferSize) \
    DiskPerformSmartCommand(FdoExtension, \
                            IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS,  \
                            SMART_CMD, \
                            READ_ATTRIBUTES, \
                            0, \
                            (SrbControl), \
                            (BufferSize))


//
// Read SMART status
// SrbControl should be sizeof(SRB_IO_CONTROL) +
//                      (sizeof(SENDCMDINPARAMS)-1) +
//                      sizeof(IDEREGS)
// Failure predicted if cmdOutParameters[3] == 0xf4 and [4] == 0x2c
//
#define DiskReadSmartStatus(FdoExtension, \
                          SrbControl, \
                          BufferSize) \
    DiskPerformSmartCommand(FdoExtension, \
                            IOCTL_SCSI_MINIPORT_RETURN_STATUS, \
                            SMART_CMD, \
                            RETURN_SMART_STATUS, \
                            0, \
                            (SrbControl), \
                            (BufferSize))


//
// Read disks IDENTIFY data
// SrbControl should be sizeof(SRB_IO_CONTROL) +
//                      (sizeof(SENDCMDINPARAMS)-1) +
//                      sizeof(IDENTIFY_BUFFER_SIZE)
// Identify data returned at &cmdOutParams.bBuffer[0]
//
#define DiskGetIdentifyData(FdoExtension, \
                          SrbControl, \
                          BufferSize) \
    DiskPerformSmartCommand(FdoExtension, \
                            IOCTL_SCSI_MINIPORT_IDENTIFY, \
                            ID_CMD, \
                            0, \
                            0, \
                            (SrbControl), \
                            (BufferSize))


//
// Enable SMART
//
_inline NTSTATUS
DiskEnableSmart(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    UCHAR srbControl[sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS)];
    ULONG bufferSize = sizeof(srbControl);

    return DiskPerformSmartCommand(FdoExtension,
                                   IOCTL_SCSI_MINIPORT_ENABLE_SMART,
                                   SMART_CMD,
                                   ENABLE_SMART,
                                   0,
                                   (PSRB_IO_CONTROL)srbControl,
                                   &bufferSize);
}

//
// Disable SMART
//
_inline NTSTATUS
DiskDisableSmart(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    UCHAR srbControl[sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS)];
    ULONG bufferSize = sizeof(srbControl);
    return DiskPerformSmartCommand(FdoExtension,
                                   IOCTL_SCSI_MINIPORT_DISABLE_SMART,
                                   SMART_CMD,
                                   DISABLE_SMART,
                                   0,
                                   (PSRB_IO_CONTROL)srbControl,
                                   &bufferSize);
}

//
// Enable Attribute Autosave
//
_inline NTSTATUS
DiskEnableSmartAttributeAutosave(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    UCHAR srbControl[sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS)];
    ULONG bufferSize = sizeof(srbControl);
    return DiskPerformSmartCommand(FdoExtension,
                                   IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE,
                                   SMART_CMD,
                                   ENABLE_DISABLE_AUTOSAVE,
                                   0xf1,
                                   (PSRB_IO_CONTROL)srbControl,
                                   &bufferSize);
}

//
// Disable Attribute Autosave
//
_inline NTSTATUS
DiskDisableSmartAttributeAutosave(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    UCHAR srbControl[sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS)];
    ULONG bufferSize = sizeof(srbControl);
    return DiskPerformSmartCommand(FdoExtension,
                                   IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE,
                                   SMART_CMD,
                                   ENABLE_DISABLE_AUTOSAVE,
                                   0x00,
                                   (PSRB_IO_CONTROL)srbControl,
                                   &bufferSize);
}

//
// Initialize execution of SMART online diagnostics
//
_inline NTSTATUS
DiskExecuteSmartDiagnostics(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    UCHAR srbControl[sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS)];
    ULONG bufferSize = sizeof(srbControl);
    return DiskPerformSmartCommand(FdoExtension,
                                   IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS,
                                   SMART_CMD,
                                   EXECUTE_OFFLINE_DIAGS,
                                   0,
                                   (PSRB_IO_CONTROL)srbControl,
                                   &bufferSize);
}


NTSTATUS
DiskPerformSmartCommand(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN ULONG SrbControlCode,
    IN UCHAR Command,
    IN UCHAR Feature,
    IN UCHAR SectorCount,
    IN OUT PSRB_IO_CONTROL SrbControl,
    OUT PULONG BufferSize
    )
/*++

Routine Description:

    This routine will perform some SMART command

Arguments:

    FdoExtension is the FDO device extension

    SrbControlCode is the SRB control code to use for the request

    Command is the SMART command to be executed. It may be SMART_CMD or
        ID_CMD.

    Feature is the value to place in the IDE feature register.

    SectorCount is the value to place in the IDE SectorCount register

    SrbControl is the buffer used to build the SRB_IO_CONTROL and pass
        any input parameters. It also returns the output parameters.

    *BufferSize on entry has total size of SrbControl and on return has
        the size used in SrbControl.



Return Value:

    status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = (PCOMMON_DEVICE_EXTENSION)FdoExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    PUCHAR buffer;
    PSENDCMDINPARAMS cmdInParameters;
    PSENDCMDOUTPARAMS cmdOutParameters;
    ULONG outBufferSize;
    NTSTATUS status;
    ULONG availableBufferSize;
    KEVENT event;
    PIRP irp;
    IO_STATUS_BLOCK ioStatus;
    SCSI_REQUEST_BLOCK      srb;
    LARGE_INTEGER           startingOffset;
    ULONG length;
    PIO_STACK_LOCATION      irpStack;

    PAGED_CODE();

    //
    // Point to the 'buffer' portion of the SRB_CONTROL and compute how
    // much room we have left in the srb control
    //

    buffer = (PUCHAR)SrbControl;
    (ULONG_PTR)buffer +=  sizeof(SRB_IO_CONTROL);

    cmdInParameters = (PSENDCMDINPARAMS)buffer;
    cmdOutParameters = (PSENDCMDOUTPARAMS)buffer;

    availableBufferSize = *BufferSize - sizeof(SRB_IO_CONTROL);

#if DBG
    //
    // Ensure control codes and buffer lengths passed are correct
    //
    {
        ULONG controlCode;
        ULONG lengthNeeded = sizeof(SENDCMDINPARAMS) - 1;

        if (Command == SMART_CMD)
        {
            switch (Feature)
            {

                case ENABLE_SMART:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_ENABLE_SMART;

                    break;
                }

                case DISABLE_SMART:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_DISABLE_SMART;
                    break;
                }

                case  RETURN_SMART_STATUS:
                {
                     //
                    // Ensure bBuffer is at least 2 bytes (to hold the values of
                    // cylinderLow and cylinderHigh).
                    //

                    lengthNeeded = sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS);

                    controlCode = IOCTL_SCSI_MINIPORT_RETURN_STATUS;
                    break;
                }

                case ENABLE_DISABLE_AUTOSAVE:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE;
                    break;
                }

                case SAVE_ATTRIBUTE_VALUES:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES;
                    break;
                }


                case EXECUTE_OFFLINE_DIAGS:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS;
                    break;
                }

                case READ_ATTRIBUTES:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS;
                    lengthNeeded = READ_ATTRIBUTE_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS) - 1;
                    break;
                }

                case READ_THRESHOLDS:
                {
                    controlCode = IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS;
                    lengthNeeded = READ_THRESHOLD_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS) - 1;
                    break;
                }
            }
        } else if (Command == ID_CMD) {
            controlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;
            lengthNeeded = IDENTIFY_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS) -1;
        } else {
            ASSERT(FALSE);
        }

        ASSERT(controlCode == SrbControlCode);
        ASSERT(availableBufferSize >= lengthNeeded);
    }
#endif

    //
    // Build SrbControl and input to SMART command
    //

    SrbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
    RtlMoveMemory (SrbControl->Signature, "SCSIDISK", 8);
    SrbControl->Timeout = FdoExtension->TimeOutValue;
    SrbControl->Length = availableBufferSize;

    SrbControl->ControlCode = SrbControlCode;

    cmdInParameters->cBufferSize = sizeof(SENDCMDINPARAMS);
    cmdInParameters->bDriveNumber = diskData->ScsiAddress.TargetId;
    cmdInParameters->irDriveRegs.bFeaturesReg = Feature;
    cmdInParameters->irDriveRegs.bSectorCountReg = SectorCount;
    cmdInParameters->irDriveRegs.bCylLowReg = SMART_CYL_LOW;
    cmdInParameters->irDriveRegs.bCylHighReg = SMART_CYL_HI;
    cmdInParameters->irDriveRegs.bCommandReg = Command;


    //
    // Create and send irp
    //
    KeInitializeEvent(&event, NotificationEvent, FALSE);

    startingOffset.QuadPart = (LONGLONG) 1;

    length = SrbControl->HeaderLength + SrbControl->Length;

    irp = IoBuildSynchronousFsdRequest(
                IRP_MJ_SCSI,
                commonExtension->LowerDeviceObject,
                SrbControl,
                length,
                &startingOffset,
                &event,
                &ioStatus);

    if (irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Set major and minor codes.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = 1;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    srb.PathId = diskData->ScsiAddress.PathId;
    srb.TargetId = diskData->ScsiAddress.TargetId;
    srb.Lun = diskData->ScsiAddress.Lun;

    srb.Function = SRB_FUNCTION_IO_CONTROL;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    srb.SrbFlags = SRB_FLAGS_DATA_IN |
                   SRB_FLAGS_NO_QUEUE_FREEZE |
                   SRB_FLAGS_NO_KEEP_AWAKE;

    srb.OriginalRequest = irp;

    //
    // Set timeout to requested value.
    //

    srb.TimeOutValue = SrbControl->Timeout;

    //
    // Set the data buffer.
    //

    srb.DataBuffer = SrbControl;
    srb.DataTransferLength = length;

    //
    // Flush the data buffer for output. This will insure that the data is
    // written back to memory.  Since the data-in flag is the the port driver
    // will flush the data again for input which will ensure the data is not
    // in the cache.
    //

    KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);

    //
    // Call port driver to handle this request.
    //

    status = IoCallDriver(commonExtension->LowerDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    return status;
}


NTSTATUS
DiskGetIdentifyInfo(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PBOOLEAN SupportSmart
    )
{
    UCHAR outBuffer[sizeof(SRB_IO_CONTROL) + (sizeof(SENDCMDINPARAMS)-1) + IDENTIFY_BUFFER_SIZE];
    ULONG outBufferSize = sizeof(outBuffer);
    NTSTATUS status;

    PAGED_CODE();

    status = DiskGetIdentifyData(FdoExtension,
                                 (PSRB_IO_CONTROL)outBuffer,
                                 &outBufferSize);

    if (NT_SUCCESS(status))
    {
        PUSHORT identifyData = (PUSHORT)&(outBuffer[sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDOUTPARAMS)-1]);
        USHORT commandSetSupported = identifyData[82];

        *SupportSmart = ((commandSetSupported != 0xffff) &&
                         (commandSetSupported != 0) &&
                         ((commandSetSupported & 1) == 1));
    } else {
        *SupportSmart = FALSE;
    }

    DebugPrint((3, "DiskGetIdentifyInfo: SMART %s supported for device %p, status %lx\n",
                   *SupportSmart ? "is" : "is not",
                   FdoExtension->DeviceObject,
                   status));

    return status;
}


//
// FP Ioctl specific routines
//

NTSTATUS
DiskSendFailurePredictIoctl(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PSTORAGE_PREDICT_FAILURE checkFailure
    )
{
    KEVENT event;
    PDEVICE_OBJECT deviceObject;
    IO_STATUS_BLOCK ioStatus;
    PIRP irp;
    NTSTATUS status;

    PAGED_CODE();

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    deviceObject = IoGetAttachedDeviceReference(FdoExtension->DeviceObject);

    irp = IoBuildDeviceIoControlRequest(
                    IOCTL_STORAGE_PREDICT_FAILURE,
                    deviceObject,
                    NULL,
                    0,
                    checkFailure,
                    sizeof(STORAGE_PREDICT_FAILURE),
                    FALSE,
                    &event,
                    &ioStatus);

    if (irp != NULL)
    {
        status = IoCallDriver(deviceObject, irp);
        if (status == STATUS_PENDING)
        {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
            status = ioStatus.Status;
        }

    } else {
        status = STATUS_INSUFFICIENT_RESOURCES;
    }

    ObDereferenceObject(deviceObject);

    return status;
}


//
// FP type independent routines
//

NTSTATUS
DiskEnableDisableFailurePrediction(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    BOOLEAN Enable
    )
/*++

Routine Description:

    Enable or disable failure prediction at the hardware level

Arguments:

    FdoExtension

    Enable

Return Value:

    NT Status

--*/
{
    NTSTATUS status;
    PCOMMON_DEVICE_EXTENSION commonExtension = &(FdoExtension->CommonExtension);
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);

    PAGED_CODE();

    switch(diskData->FailurePredictionCapability)
    {
        case FailurePredictionSmart:
        {

            if (Enable)
            {
                status = DiskEnableSmart(FdoExtension);
                if (NT_SUCCESS(status))
                {
                    DiskEnableSmartAttributeAutosave(FdoExtension);
                }
            } else {
                status = DiskDisableSmart(FdoExtension);
            }

            break;
        }

        case  FailurePredictionSense:
        {
            status = DiskSetInfoExceptionInformation(FdoExtension,
                                                Enable,
                                                diskData->AllowFPPerfHit,
                                                FALSE);
            break;
        }

        case  FailurePredictionIoctl:
        {
            status = STATUS_SUCCESS;
            break;
        }

        default:
        {
            status = STATUS_INVALID_DEVICE_REQUEST;
        }
    }
    return status;
}

NTSTATUS
DiskEnableDisableFailurePredictPolling(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    BOOLEAN Enable,
    ULONG PollTimeInSeconds
    )
/*++

Routine Description:

    Enable or disable polling for hardware failure detection

Arguments:

    FdoExtension

    Enable

    PollTimeInSeconds - if 0 then no change to current polling timer

Return Value:

    NT Status

--*/
{
    NTSTATUS status;
    PCOMMON_DEVICE_EXTENSION commonExtension = (PCOMMON_DEVICE_EXTENSION)FdoExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);

    PAGED_CODE();

    if (Enable)
    {
        status = DiskEnableDisableFailurePrediction(FdoExtension,
                                           Enable);
    } else {
        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status))
    {
        status = ClassSetFailurePredictionPoll(FdoExtension,
                        Enable ? diskData->FailurePredictionCapability :
                                 FailurePredictionNone,
                                     PollTimeInSeconds);

        //
        // Even if this failed we do not want to disable FP on the
        // hardware. FP is only ever disabled on the hardware by
        // specific command of the user.
        //
    }

    return status;
}


NTSTATUS
DiskReadFailurePredictStatus(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PSTORAGE_FAILURE_PREDICT_STATUS DiskSmartStatus
    )
/*++

Routine Description:

    Obtains current failure prediction status

Arguments:

    FdoExtension

    DiskSmartStatus

Return Value:

    NT Status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = (PCOMMON_DEVICE_EXTENSION)FdoExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    NTSTATUS status;

    PAGED_CODE();

    DiskSmartStatus->PredictFailure = FALSE;

    switch(diskData->FailurePredictionCapability)
    {
        case FailurePredictionSmart:
        {
            UCHAR outBuffer[sizeof(SRB_IO_CONTROL) + (sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS))];
            ULONG outBufferSize = sizeof(outBuffer);
            PSENDCMDOUTPARAMS cmdOutParameters;

            status = DiskReadSmartStatus(FdoExtension,
                                     (PSRB_IO_CONTROL)outBuffer,
                                     &outBufferSize);

            if (NT_SUCCESS(status))
            {
                cmdOutParameters = (PSENDCMDOUTPARAMS)(outBuffer +
                                               sizeof(SRB_IO_CONTROL));

                DiskSmartStatus->Reason = 0; // Unknown;
                DiskSmartStatus->PredictFailure = ((cmdOutParameters->bBuffer[3] == 0xf4) &&
                                                   (cmdOutParameters->bBuffer[4] == 0x2c));
            }
            break;
        }

        case FailurePredictionSense:
        {
            DiskSmartStatus->Reason = FdoExtension->FailureReason;
            DiskSmartStatus->PredictFailure = FdoExtension->FailurePredicted;
            status = STATUS_SUCCESS;
            break;
        }

        case FailurePredictionIoctl:
        case FailurePredictionNone:
        default:
        {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

    return status;
}

NTSTATUS
DiskReadFailurePredictData(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PSTORAGE_FAILURE_PREDICT_DATA DiskSmartData
    )
/*++

Routine Description:

    Obtains current failure prediction data. Not available for
    FAILURE_PREDICT_SENSE types.

Arguments:

    FdoExtension

    DiskSmartData

Return Value:

    NT Status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = (PCOMMON_DEVICE_EXTENSION)FdoExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    NTSTATUS status;

    PAGED_CODE();

    switch(diskData->FailurePredictionCapability)
    {
        case FailurePredictionSmart:
        {
            UCHAR outBuffer[(sizeof(SRB_IO_CONTROL) +
                            (sizeof(SENDCMDOUTPARAMS)-1) +
                            READ_ATTRIBUTE_BUFFER_SIZE)];
            PSENDCMDOUTPARAMS cmdOutParameters;
            ULONG outBufferSize = sizeof(outBuffer);

            status = DiskReadSmartData(FdoExtension,
                               (PSRB_IO_CONTROL)outBuffer,
                               &outBufferSize);

            if (NT_SUCCESS(status))
            {
                cmdOutParameters = (PSENDCMDOUTPARAMS)(outBuffer +
                                           sizeof(SRB_IO_CONTROL));

                DiskSmartData->Length = READ_ATTRIBUTE_BUFFER_SIZE;
                RtlCopyMemory(DiskSmartData->VendorSpecific,
                          cmdOutParameters->bBuffer,
                          READ_ATTRIBUTE_BUFFER_SIZE);
            }

            break;
        }

        case FailurePredictionSense:
        {
            DiskSmartData->Length = sizeof(ULONG);
            *((PULONG)DiskSmartData->VendorSpecific) = FdoExtension->FailureReason;

            status = STATUS_SUCCESS;
            break;
        }

        case FailurePredictionIoctl:
        case FailurePredictionNone:
        default:
        {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

    return status;
}

NTSTATUS
DiskDetectFailurePrediction(
    PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    PFAILURE_PREDICTION_METHOD FailurePredictCapability
    )
/*++

Routine Description:

    Detect if device has any failure prediction capabilities. First we
    check for IDE SMART capability. This is done by sending the drive an
    IDENTIFY command and checking if the SMART command set bit is set.

    Next we check if SCSI SMART (aka Information Exception Control Page,
    X3T10/94-190 Rev 4). This is done by querying for the Information
    Exception mode page.

    Lastly we check if the device has IOCTL failure prediction. This mechanism
    a filter driver implements IOCTL_STORAGE_PREDICT_FAILURE and will respond
    with the information in the IOCTL. We do this by sending the ioctl and
    if the status returned is STATUS_SUCCESS we assume that it is supported.

Arguments:

    FdoExtension

    *FailurePredictCapability

Return Value:

    NT Status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = (PCOMMON_DEVICE_EXTENSION)FdoExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    BOOLEAN supportFP;
    NTSTATUS status;
    STORAGE_PREDICT_FAILURE checkFailure;
    STORAGE_FAILURE_PREDICT_STATUS diskSmartStatus;
    BOOLEAN logErr;

    PAGED_CODE();

    //
    // Assume no failure predict mechanisms
    //
    *FailurePredictCapability = FailurePredictionNone;

    //
    // See if this is an IDE drive that supports SMART. If so enable SMART
    // and then ensure that it suports the SMART READ STATUS command
    //

    status = DiskGetIdentifyInfo(FdoExtension,
                                 &supportFP);

    if (supportFP)
    {
        status = DiskEnableSmart(FdoExtension);
        if (NT_SUCCESS(status))
        {
            DiskEnableSmartAttributeAutosave(FdoExtension);

            *FailurePredictCapability = FailurePredictionSmart;

            status = DiskReadFailurePredictStatus(FdoExtension,
                                                  &diskSmartStatus);

            DebugPrint((1, "Disk: Device %p %s IDE SMART\n",
                       FdoExtension->DeviceObject,
                       NT_SUCCESS(status) ? "does" : "does not"));

            if (NT_SUCCESS(status))
            {
                return status;
            } else {
                *FailurePredictCapability = FailurePredictionNone;
            }
        }
    }

    //
    // No SMART so see if we can enable SCSI failure predictioo
    //
    DiskGetInfoExceptionInformation(FdoExtension,
                                    &logErr,
                                    &supportFP);

    if (supportFP)
    {
        status = DiskSetInfoExceptionInformation(FdoExtension,
                                                 TRUE,
                                                 diskData->AllowFPPerfHit,
                                                 FALSE);
        DebugPrint((1, "Disk: Device %p %s SCSI SMART\n",
                       FdoExtension->DeviceObject,
                       NT_SUCCESS(status) ? "does" : "does not"));

        if(NT_SUCCESS(status))
        {
            *FailurePredictCapability = FailurePredictionSense;

            //
            // Enable media change notification. A side effect of this
            // is to cause polling of the disk and so we check the returned
            // sense code for a failure prediction condition
            //

            return status;
        }
    }

    //
    // No scsi support either. See if there is a a filter driver to intercept
    // IOCTL_STORAGE_PREDICT_FAILURE
    //

    status = DiskSendFailurePredictIoctl(FdoExtension,
                                         &checkFailure);

    if (NT_SUCCESS(status))
    {
        *FailurePredictCapability = FailurePredictionIoctl;
        if (checkFailure.PredictFailure)
        {
            checkFailure.PredictFailure = 512;
            ClassNotifyFailurePredicted(FdoExtension,
                                            (PUCHAR)&checkFailure,
                                            sizeof(checkFailure),
                                            (BOOLEAN)(FdoExtension->FailurePredicted == FALSE),
                                            0x11,
                                            diskData->ScsiAddress.PathId,
                                            diskData->ScsiAddress.TargetId,
                                            diskData->ScsiAddress.Lun);

            FdoExtension->FailurePredicted = TRUE;
         }
    }

    DebugPrint((1, "Disk: Device %p %s IOCTL_STORAGE_FAILURE_PREDICT\n",
                       FdoExtension->DeviceObject,
                       NT_SUCCESS(status) ? "does" : "does not"));

    return status;
}


NTSTATUS
DiskWmiFunctionControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN CLASSENABLEDISABLEFUNCTION Function,
    IN BOOLEAN Enable
    )
/*++

Routine Description:

    This routine is a callback into the driver to enabled or disable event
    generation or data block collection. A device should only expect a
    single enable when the first event or data consumer enables events or
    data collection and a single disable when the last event or data
    consumer disables events or data collection. Data blocks will only
    receive collection enable/disable if they were registered as requiring
    it.


    When NT boots, failure prediction is not automatically enabled, although
    it may have been persistantly enabled on a previous boot. Polling is also
    not automatically enabled. When the first data block that accesses SMART
    such as SmartStatusGuid, SmartDataGuid, SmartPerformFunction, or
    SmartEventGuid is accessed then SMART is automatically enabled in the
    hardware. Polling is enabled when SmartEventGuid is enabled and disabled
    when it is disabled. Hardware SMART is only disabled when the DisableSmart
    method is called. Polling is also disabled when this is called regardless
    of the status of the other guids or events.

Arguments:

    DeviceObject is the device whose data block is being queried

    GuidIndex is the index into the list of guids provided when the
        device registered

    Function specifies which functionality is being enabled or disabled

    Enable is TRUE then the function is being enabled else disabled

Return Value:

    status

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    ULONG enableCount;

    PAGED_CODE();

    if ((Function == DataBlockCollection) && Enable)
    {
        if ((GuidIndex == SmartStatusGuid) ||
            (GuidIndex == SmartDataGuid) ||
            (GuidIndex == SmartPerformFunction))
        {
            status = DiskEnableDisableFailurePrediction(fdoExtension,
                                                        TRUE);
            DebugPrint((3, "Disk: DeviceObject %p, Irp %p Enable -> %lx\n",
                       DeviceObject,
                       Irp,
                       status));

        } else {
            DebugPrint((3, "Disk: DeviceObject %p, Irp %p, GuidIndex %d %s for Collection\n",
                      DeviceObject, Irp,
                      GuidIndex,
                      Enable ? "Enabled" : "Disabled"));        }
    } else if (Function == EventGeneration) {
        DebugPrint((3, "Disk: DeviceObject %p, Irp %p, GuidIndex %d %s for Event Generation\n",
                  DeviceObject, Irp,
                  GuidIndex,
                  Enable ? "Enabled" : "Disabled"));


        if ((GuidIndex == SmartEventGuid) && Enable)
        {
            status = DiskEnableDisableFailurePredictPolling(fdoExtension,
                                                   Enable,
                                                   0);
            DebugPrint((3, "Disk: DeviceObject %p, Irp %p %s -> %lx\n",
                       DeviceObject,
                       Irp,
                       Enable ? "DiskEnableSmartPolling" : "DiskDisableSmartPolling",
                       status));
        }

#if DBG
    } else {
        DebugPrint((3, "Disk: DeviceObject %p, Irp %p, GuidIndex %d %s for function %d\n",
                  DeviceObject, Irp,
                  GuidIndex,
                  Enable ? "Enabled" : "Disabled",
                  Function));
#endif
    }

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     0,
                                     IO_NO_INCREMENT);
    return status;
}



NTSTATUS
DiskFdoQueryWmiRegInfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT ULONG *RegFlags,
    OUT PUNICODE_STRING InstanceName
    )
/*++

Routine Description:

    This routine is a callback into the driver to retrieve the list of
    guids or data blocks that the driver wants to register with WMI. This
    routine may not pend or block. Driver should NOT call
    ClassWmiCompleteRequest.

Arguments:

    DeviceObject is the device whose data block is being queried

    *RegFlags returns with a set of flags that describe the guids being
        registered for this device. If the device wants enable and disable
        collection callbacks before receiving queries for the registered
        guids then it should return the WMIREG_FLAG_EXPENSIVE flag. Also the
        returned flags may specify WMIREG_FLAG_INSTANCE_PDO in which case
        the instance name is determined from the PDO associated with the
        device object. Note that the PDO must have an associated devnode. If
        WMIREG_FLAG_INSTANCE_PDO is not set then Name must return a unique
        name for the device.

    InstanceName returns with the instance name for the guids if
        WMIREG_FLAG_INSTANCE_PDO is not set in the returned *RegFlags. The
        caller will call ExFreePool with the buffer returned.


Return Value:

    status

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    NTSTATUS status;

    PAGED_CODE();

    switch (diskData->FailurePredictionCapability)
    {
        case FailurePredictionIoctl:
        case FailurePredictionSmart:
        {
            DiskWmiFdoGuidList[SmartStatusGuid].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartDataGuid].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartEventGuid].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartPerformFunction].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            break;
        }

        case FailurePredictionSense:
        {
            DiskWmiFdoGuidList[SmartStatusGuid].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartEventGuid].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartPerformFunction].Flags &= ~WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartDataGuid].Flags |= ~WMIREG_FLAG_REMOVE_GUID;
            break;
        }


        default:
        {
            DiskWmiFdoGuidList[SmartStatusGuid].Flags |= WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartDataGuid].Flags |= WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartEventGuid].Flags |= WMIREG_FLAG_REMOVE_GUID;
            DiskWmiFdoGuidList[SmartPerformFunction].Flags |= WMIREG_FLAG_REMOVE_GUID;
            break;
        }
    }

    //
    // Use devnode for FDOs
    *RegFlags = WMIREG_FLAG_INSTANCE_PDO;

    return STATUS_SUCCESS;
}

NTSTATUS
DiskFdoQueryWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    BufferAvail on has the maximum size available to write the data
        block.

    Buffer on return is filled with the returned data block


Return Value:

    status

--*/
{
    NTSTATUS status;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    ULONG sizeNeeded;

    PAGED_CODE();

    DebugPrint((3, "Disk: DiskQueryWmiDataBlock, Device %p, Irp %p, GuiIndex %d\n"
             "      BufferAvail %lx Buffer %lx\n",
             DeviceObject, Irp,
             GuidIndex, BufferAvail, Buffer));

    switch (GuidIndex)
    {
        case DiskGeometryGuid:
        {
            sizeNeeded = sizeof(DISK_GEOMETRY);
            if (BufferAvail >= sizeNeeded)
            {
                if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA)
                {
                    //
                    // Issue ReadCapacity to update device extension
                    // with information for current media.
                    status = DiskReadDriveCapacity(commonExtension->PartitionZeroExtension->DeviceObject);

                    //
                    // Note whether the drive is ready.
                    diskData->ReadyStatus = status;

                    if (!NT_SUCCESS(status))
                    {
                        break;
                    }
                }

                //
                // Copy drive geometry information from device extension.
                RtlMoveMemory(Buffer,
                              &(fdoExtension->DiskGeometry),
                              sizeof(DISK_GEOMETRY));

                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case SmartStatusGuid:
        {
            PSTORAGE_FAILURE_PREDICT_STATUS diskSmartStatus;

            ASSERT(diskData->FailurePredictionCapability != FailurePredictionNone);


            sizeNeeded = sizeof(STORAGE_FAILURE_PREDICT_STATUS);
            if (BufferAvail >= sizeNeeded)
            {
                STORAGE_PREDICT_FAILURE checkFailure;

                diskSmartStatus = (PSTORAGE_FAILURE_PREDICT_STATUS)Buffer;

                status = DiskSendFailurePredictIoctl(fdoExtension,
                                                     &checkFailure);

                if (NT_SUCCESS(status))
                {
                    if (diskData->FailurePredictionCapability ==
                                                      FailurePredictionSense)
                    {
                        diskSmartStatus->Reason =  *((PULONG)checkFailure.VendorSpecific);
                    } else {
                        diskSmartStatus->Reason =  0; // unknown
                    }

                    diskSmartStatus->PredictFailure = (checkFailure.PredictFailure != 0);
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case SmartDataGuid:
        {
            PSTORAGE_FAILURE_PREDICT_DATA diskSmartData;

            ASSERT((diskData->FailurePredictionCapability ==
                                                  FailurePredictionSmart) ||
                   (diskData->FailurePredictionCapability ==
                                                  FailurePredictionIoctl));

            sizeNeeded = sizeof(STORAGE_FAILURE_PREDICT_DATA);
            if (BufferAvail >= sizeNeeded)
            {
                PSTORAGE_PREDICT_FAILURE checkFailure = (PSTORAGE_PREDICT_FAILURE)Buffer;

                diskSmartData = (PSTORAGE_FAILURE_PREDICT_DATA)Buffer;

                status = DiskSendFailurePredictIoctl(fdoExtension,
                                                     checkFailure);

                if (NT_SUCCESS(status))
                {
                    diskSmartData->Length = 512;
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }

            break;
        }

        case SmartPerformFunction:
        {
            sizeNeeded = 0;
            status = STATUS_SUCCESS;
            break;
        }

        default:
        {
            sizeNeeded = 0;
            status = STATUS_WMI_GUID_NOT_FOUND;
        }
    }
    DebugPrint((3, "Disk: DiskQueryWmiDataBlock Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     sizeNeeded,
                                     IO_NO_INCREMENT);

    return status;
}

NTSTATUS
DiskFdoSetWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    BufferSize has the size of the data block passed

    Buffer has the new values for the data block


Return Value:

    status

--*/
{
    NTSTATUS status;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    DebugPrint((3, "Disk: DiskSetWmiDataBlock, Device %p, Irp %p, GuiIndex %d\n"
             "      BufferSize %#x Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, BufferSize, Buffer));

    if ((GuidIndex >= DiskGeometryGuid) &&
        (GuidIndex <= SmartEventGuid))
    {
        status = STATUS_WMI_READ_ONLY;
    } else {
        status = STATUS_WMI_GUID_NOT_FOUND;
    }

    DebugPrint((3, "Disk: DiskSetWmiDataBlock Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     0,
                                     IO_NO_INCREMENT);

    return status;
}

NTSTATUS
DiskFdoSetWmiDataItem(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG DataItemId,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    DataItemId has the id of the data item being set

    BufferSize has the size of the data item passed

    Buffer has the new values for the data item


Return Value:

    status

--*/
{
    NTSTATUS status;

    PAGED_CODE();

    DebugPrint((3, "Disk: DiskSetWmiDataItem, Device %p, Irp %p, GuiIndex %d, DataId %d\n"
             "      BufferSize %#x Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, DataItemId, BufferSize, Buffer));

    if ((GuidIndex >= DiskGeometryGuid) &&
        (GuidIndex <= SmartEventGuid))
    {
        status = STATUS_WMI_READ_ONLY;
    } else {
        status = STATUS_WMI_GUID_NOT_FOUND;
    }

    DebugPrint((3, "Disk: DiskSetWmiDataItem Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     0,
                                     IO_NO_INCREMENT);

    return status;
}


NTSTATUS
DiskFdoExecuteWmiMethod(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG MethodId,
    IN ULONG InBufferSize,
    IN ULONG OutBufferSize,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to execute a method. When the
    driver has finished filling the data block it must call
    ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    MethodId has the id of the method being called

    InBufferSize has the size of the data block passed in as the input to
        the method.

    OutBufferSize on entry has the maximum size available to write the
        returned data block.

    Buffer is filled with the returned data block


Return Value:

    status

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    ULONG sizeNeeded;
    NTSTATUS status;

    PAGED_CODE();

    DebugPrint((3, "Disk: DiskExecuteWmiMethod, DeviceObject %p, Irp %p, Guid Id %d, MethodId %d\n"
             "      InBufferSize %#x, OutBufferSize %#x, Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, MethodId, InBufferSize, OutBufferSize, Buffer));

    switch(GuidIndex)
    {
        case SmartPerformFunction:
        {

            ASSERT((diskData->FailurePredictionCapability ==
                                                  FailurePredictionSmart) ||
                   (diskData->FailurePredictionCapability ==
                                                  FailurePredictionIoctl) ||
                   (diskData->FailurePredictionCapability ==
                                                  FailurePredictionSense));


            switch(MethodId)
            {
                //
                // void AllowPerformanceHit([in] boolean Allow)
                //
                case AllowDisallowPerformanceHit:
                {
                    BOOLEAN allowPerfHit;

                    sizeNeeded = 0;
                    if (InBufferSize >= sizeof(BOOLEAN))
                    {
                        status = STATUS_SUCCESS;

                        allowPerfHit = *((PBOOLEAN)Buffer);
                        if (diskData->AllowFPPerfHit !=  allowPerfHit)
                        {
                            diskData->AllowFPPerfHit = allowPerfHit;
                            if (diskData->FailurePredictionCapability ==
                                FailurePredictionSense)
                            {
//
// #define FP_TEST_HACK to allow testing of failure prediction for SCSI
// SMART via FailurePredictionSense. By calling this method to change the
// PERF bit in the info exceptions control page we also enable the TEST bit
// which causes the drive to report a failure prediction on the next command.
// Note that the sense code qualifier for a test failure prediction is 0xff
// and that it is only reported once per drive reset.
//
#if DBG
#define FP_TEST_HACK
#endif
#ifdef FP_TEST_HACK
                                status = DiskSetInfoExceptionInformation(
                                                                fdoExtension,
                                                                TRUE,
                                                                allowPerfHit,
                                                                TRUE);
#else
                                status = DiskSetInfoExceptionInformation(
                                                                fdoExtension,
                                                                TRUE,
                                                                allowPerfHit,
                                                                FALSE);
#endif
                            }
                        }

                        DebugPrint((3, "DiskFdoWmiExecuteMethod: AllowPerformanceHit %x for device %p --> %lx\n",
                                    allowPerfHit,
                                    fdoExtension->DeviceObject,
                                    status));
                    } else {
                        status = STATUS_INVALID_PARAMETER;
                    }
                    break;
                }

                //
                // void EnableDisableHardwareFailurePrediction([in] boolean Enable)
                //
                case EnableDisableHardwareFailurePrediction:
                {
                    BOOLEAN enable;

                    sizeNeeded = 0;
                    if (InBufferSize >= sizeof(BOOLEAN))
                    {
                        status = STATUS_SUCCESS;
                        enable = *((PBOOLEAN)Buffer);
                        if (! enable)
                        {
                            //
                            // If we are disabling we need to also disable
                            // polling
                            //
                            DiskEnableDisableFailurePredictPolling(
                                                               fdoExtension,
                                                               enable,
                                                               0);
                        }

                        status = DiskEnableDisableFailurePrediction(
                                                           fdoExtension,
                                                           enable);

                        DebugPrint((3, "DiskFdoWmiExecuteMethod: EnableDisableHardwareFailurePrediction: %x for device %p --> %lx\n",
                                    enable,
                                    fdoExtension->DeviceObject,
                                    status));
                    } else {
                        status = STATUS_INVALID_PARAMETER;
                    }
                    break;
                }

                //
                // void EnableDisableFailurePredictionPolling(
                //                               [in] uint32 Period,
                //                               [in] boolean Enable)
                //
                case EnableDisableFailurePredictionPolling:
                {
                    BOOLEAN enable;
                    ULONG period;

                    sizeNeeded = 0;
                    if (InBufferSize >= (sizeof(ULONG) + sizeof(BOOLEAN)))
                    {
                        period = *((PULONG)Buffer);
                        Buffer += sizeof(ULONG);
                        enable = *((PBOOLEAN)Buffer);

                           status = DiskEnableDisableFailurePredictPolling(
                                                               fdoExtension,
                                                               enable,
                                                               period);

                        DebugPrint((3, "DiskFdoWmiExecuteMethod: EnableDisableFailurePredictionPolling: %x %x for device %p --> %lx\n",
                                    enable,
                                    period,
                                    fdoExtension->DeviceObject,
                                    status));
                    } else {
                        status = STATUS_INVALID_PARAMETER;
                    }
                    break;
                }

                //
                // void GetFailurePredictionCapability([out] uint32 Capability)
                //
                case GetFailurePredictionCapability:
                {
                    sizeNeeded = sizeof(ULONG);
                    if (OutBufferSize >= sizeNeeded)
                    {
                        status = STATUS_SUCCESS;
                        *((PFAILURE_PREDICTION_METHOD)Buffer) = diskData->FailurePredictionCapability;
                        DebugPrint((3, "DiskFdoWmiExecuteMethod: GetFailurePredictionCapability: %x for device %p --> %lx\n",
                                    *((PFAILURE_PREDICTION_METHOD)Buffer),
                                    fdoExtension->DeviceObject,
                                    status));
                    } else {
                        status = STATUS_BUFFER_TOO_SMALL;
                    }
                    break;
                }

                //
                // void EnableOfflineDiags([out] boolean Success);
                //
                case EnableOfflineDiags:
                {
                    sizeNeeded = sizeof(BOOLEAN);
                    if (OutBufferSize >= sizeNeeded)
                    {
                        if (diskData->FailurePredictionCapability ==
                                  FailurePredictionSmart)
                        {
                            //
                            // Initiate or resume offline diagnostics.
                            // This may cause a loss of performance
                            // to the disk, but mayincrease the amount
                            // of disk checking.
                            //
                            status = DiskExecuteSmartDiagnostics(fdoExtension);

                        } else {
                            status = STATUS_INVALID_DEVICE_REQUEST;
                        }

                        *((PBOOLEAN)Buffer) = NT_SUCCESS(status);

                        DebugPrint((3, "DiskFdoWmiExecuteMethod: EnableOfflineDiags for device %p --> %lx\n",
                                    fdoExtension->DeviceObject,
                                    status));
                    } else {
                        status = STATUS_BUFFER_TOO_SMALL;
                    }
                    break;
                }


                default :
                {
                    sizeNeeded = 0;
                    status = STATUS_WMI_ITEMID_NOT_FOUND;
                    break;
                }
            }

            break;
        }

        case DiskGeometryGuid:
        case SmartStatusGuid:
        case SmartDataGuid:
        {

            sizeNeeded = 0;
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        default:
        {
            sizeNeeded = 0;
            status = STATUS_WMI_GUID_NOT_FOUND;
        }
    }

    DebugPrint((3, "Disk: DiskExecuteMethod Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     sizeNeeded,
                                     IO_NO_INCREMENT);

    return status;
}


#if 0
//
// Enable this to add WMI support for PDOs
NTSTATUS
DiskPdoQueryWmiRegInfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT ULONG *RegFlags,
    OUT PUNICODE_STRING InstanceName
    )
/*++

Routine Description:

    This routine is a callback into the driver to retrieve the list of
    guids or data blocks that the driver wants to register with WMI. This
    routine may not pend or block. Driver should NOT call
    ClassWmiCompleteRequest.

Arguments:

    DeviceObject is the device whose data block is being queried

    *RegFlags returns with a set of flags that describe the guids being
        registered for this device. If the device wants enable and disable
        collection callbacks before receiving queries for the registered
        guids then it should return the WMIREG_FLAG_EXPENSIVE flag. Also the
        returned flags may specify WMIREG_FLAG_INSTANCE_PDO in which case
        the instance name is determined from the PDO associated with the
        device object. Note that the PDO must have an associated devnode. If
        WMIREG_FLAG_INSTANCE_PDO is not set then Name must return a unique
        name for the device.

    InstanceName returns with the instance name for the guids if
        WMIREG_FLAG_INSTANCE_PDO is not set in the returned *RegFlags. The
        caller will call ExFreePool with the buffer returned.


Return Value:

    status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION parentFunctionalExtension;
    ANSI_STRING ansiString;
    CHAR name[256];
    NTSTATUS status;

    //
    // We need to pick a name for PDOs since they do not have a devnode
    parentFunctionalExtension = commonExtension->PartitionZeroExtension;
    sprintf(name,
                "Disk(%d)_Partition(%d)_Start(%#I64x)_Length(%#I64x)",
                parentFunctionalExtension->DeviceNumber,
                commonExtension->PartitionNumber,
                commonExtension->StartingOffset.QuadPart,
                commonExtension->PartitionLength.QuadPart);
    RtlInitAnsiString(&ansiString,
                          name);

    status = RtlAnsiStringToUnicodeString(InstanceName,
                                     &ansiString,
                                     TRUE);

    return status;
}

NTSTATUS
DiskPdoQueryWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    BufferAvail on has the maximum size available to write the data
        block.

    Buffer on return is filled with the returned data block


Return Value:

    status

--*/
{
    NTSTATUS status;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = (PDISK_DATA)(commonExtension->DriverData);
    ULONG sizeNeeded;

    DebugPrint((3, "Disk: DiskQueryWmiDataBlock, Device %p, Irp %p, GuiIndex %d\n"
             "      BufferAvail %#x Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, BufferAvail, Buffer));

    switch (GuidIndex)
    {
        case 0:
        {
            sizeNeeded = 4 * sizeof(ULONG);
            if (BufferAvail >= sizeNeeded)
            {
                RtlCopyMemory(Buffer, DiskDummyData, sizeNeeded);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        default:
        {
            status = STATUS_WMI_GUID_NOT_FOUND;
        }
    }

    DebugPrint((3, "Disk: DiskQueryWmiDataBlock Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     sizeNeeded,
                                     IO_NO_INCREMENT);

    return status;
}

NTSTATUS
DiskPdoSetWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    BufferSize has the size of the data block passed

    Buffer has the new values for the data block


Return Value:

    status

--*/
{
    NTSTATUS status;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    ULONG sizeNeeded;

    DebugPrint((3, "Disk: DiskSetWmiDataBlock, Device %p, Irp %p, GuiIndex %d\n"
             "      BufferSize %#x Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, BufferSize, Buffer));

    switch(GuidIndex)
    {
        case 0:
        {
            sizeNeeded = 4 * sizeof(ULONG);
            if (BufferSize == sizeNeeded)
              {
                RtlCopyMemory(DiskDummyData, Buffer, sizeNeeded);
                status = STATUS_SUCCESS;
               } else {
                status = STATUS_INFO_LENGTH_MISMATCH;
            }
            break;
        }

        default:
        {
            status = STATUS_WMI_GUID_NOT_FOUND;
        }
    }

    DebugPrint((3, "Disk: DiskSetWmiDataBlock Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     0,
                                     IO_NO_INCREMENT);

    return status;
}

NTSTATUS
DiskPdoSetWmiDataItem(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG DataItemId,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    DataItemId has the id of the data item being set

    BufferSize has the size of the data item passed

    Buffer has the new values for the data item


Return Value:

    status

--*/
{
    NTSTATUS status;

    DebugPrint((3, "Disk: DiskSetWmiDataItem, Device %p, Irp %p, GuiIndex %d, DataId %d\n"
             "      BufferSize %#x Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, DataItemId, BufferSize, Buffer));

    switch(GuidIndex)
    {
        case 0:
        {
            if ((BufferSize == sizeof(ULONG)) &&
                (DataItemId <= 3))
              {
                  DiskDummyData[DataItemId] = *((PULONG)Buffer);
                   status = STATUS_SUCCESS;
               } else {
                   status = STATUS_INVALID_DEVICE_REQUEST;
               }
            break;
        }

        default:
        {
            status = STATUS_WMI_GUID_NOT_FOUND;
        }
    }


    DebugPrint((3, "Disk: DiskSetWmiDataItem Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     0,
                                     IO_NO_INCREMENT);

    return status;
}


NTSTATUS
DiskPdoExecuteWmiMethod(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG MethodId,
    IN ULONG InBufferSize,
    IN ULONG OutBufferSize,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to execute a method. When the
    driver has finished filling the data block it must call
    ClassWmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    MethodId has the id of the method being called

    InBufferSize has the size of the data block passed in as the input to
        the method.

    OutBufferSize on entry has the maximum size available to write the
        returned data block.

    Buffer is filled with the returned data block


Return Value:

    status

--*/
{
    ULONG sizeNeeded = 4 * sizeof(ULONG);
    NTSTATUS status;
    ULONG tempData[4];

    DebugPrint((3, "Disk: DiskExecuteWmiMethod, DeviceObject %p, Irp %p, Guid Id %d, MethodId %d\n"
             "      InBufferSize %#x, OutBufferSize %#x, Buffer %p\n",
             DeviceObject, Irp,
             GuidIndex, MethodId, InBufferSize, OutBufferSize, Buffer));

    switch(GuidIndex)
    {
        case 0:
        {
            if (MethodId == 1)
            {
                if (OutBufferSize >= sizeNeeded)
                {

                    if (InBufferSize == sizeNeeded)
                    {
                        RtlCopyMemory(tempData, Buffer, sizeNeeded);
                        RtlCopyMemory(Buffer, DiskDummyData, sizeNeeded);
                        RtlCopyMemory(DiskDummyData, tempData, sizeNeeded);

                        status = STATUS_SUCCESS;
                    } else {
                        status = STATUS_INVALID_DEVICE_REQUEST;
                    }
                } else {
                    status = STATUS_BUFFER_TOO_SMALL;
                }
            } else {
                   status = STATUS_INVALID_DEVICE_REQUEST;
            }
            break;
        }

        default:
        {
            status = STATUS_WMI_GUID_NOT_FOUND;
        }
    }

    DebugPrint((3, "Disk: DiskExecuteMethod Device %p, Irp %p returns %lx\n",
             DeviceObject, Irp, status));

    status = ClassWmiCompleteRequest(DeviceObject,
                                     Irp,
                                     status,
                                     0,
                                     IO_NO_INCREMENT);

    return status;
}
#endif



