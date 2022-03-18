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

#define CLASS_INIT_GUID 1
#include "classp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)

#pragma alloc_text(PAGE, ClassInitialize)
#pragma alloc_text(PAGE, ClassAddDevice)
#pragma alloc_text(PAGE, ClassDispatchPnp)
#pragma alloc_text(PAGE, ClassPnpStartDevice)
#pragma alloc_text(PAGE, ClassModeSense)
#pragma alloc_text(PAGE, ClassCreateDeviceObject)
#pragma alloc_text(PAGE, ClassDeleteSrbLookasideList)
#pragma alloc_text(PAGE, ClassInitializeSrbLookasideList)
#pragma alloc_text(PAGE, ClassQueryTimeOutRegistryValue)
#pragma alloc_text(PAGE, ClassGetDescriptor)
#pragma alloc_text(PAGE, ClassPnpQueryFdoRelations)
#pragma alloc_text(PAGE, ClassMarkChildrenMissing)
#pragma alloc_text(PAGE, ClassMarkChildMissing)
#pragma alloc_text(PAGE, ClassRetrieveDeviceRelations)
#pragma alloc_text(PAGE, ClassGetPdoId)
#pragma alloc_text(PAGE, ClassQueryPnpCapabilities)
#pragma alloc_text(PAGE, ClassInvalidateBusRelations)
#pragma alloc_text(PAGE, ClassRemoveDevice)
#pragma alloc_text(PAGE, ClassUpdateInformationInRegistry)
#pragma alloc_text(PAGE, ClassUnload)
#pragma alloc_text(PAGE, ClassClaimDevice)
#pragma alloc_text(PAGE, ClassSendDeviceIoControlSynchronous)
#pragma alloc_text(PAGE, ClassSendSrbAsynchronous)
#pragma alloc_text(PAGE, ClasspRegisterMountedDeviceInterface)
#pragma alloc_text(PAGE, ClasspAllocateReleaseRequest)
#pragma alloc_text(PAGE, ClasspFreeReleaseRequest)

#endif

ULONG ClassPnpAllowUnload = TRUE;

#define NOT_READY_RETRY_INTERVAL    10

//
// NEC98 machines have drive-letter A and B which are non FD too.
//
#define FirstDriveLetter (IsNEC_98 ? 'A' : 'C')
#define LastDriveLetter  'Z'



#if DBG

#pragma data_seg("NONPAGE")


typedef struct _CLASSPNP_GLOBALS {

    //
    // whether or not to ASSERT for lost irps
    //

    ULONG BreakOnLostIrps;
    ULONG SecondsToWaitForIrps;

    //
    // use a buffered debug print to help
    // catch timing issues that do not
    // reproduce with std debugprints enabled
    //

    ULONG UseBufferedDebugPrint;

    //
    // the next three require the spinlock
    // and are the buffered printing support
    // (currently unimplemented)
    //

    KSPIN_LOCK SpinLock;
    ULONG Index;
    PUCHAR PrintBuffer;

    //
    // interlocked variables to initialize
    // this data only once
    //

    LONG Initializing;
    LONG Initialized;

} CLASSPNP_GLOBALS, *PCLASSPNP_GLOBALS;

//
// ANSI spec requires initialization to ZERO
// for global data.
//

CLASSPNP_GLOBALS Globals;
ULONG GlobalPrintBufferSize = 64*1024;

#pragma data_seg()


#pragma code_seg()

#endif // DBG


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Temporary entry point needed to initialize the class system dll.
    It doesn't do anything.

Arguments:

    DriverObject - Pointer to the driver object created by the system.

Return Value:

   STATUS_SUCCESS

--*/

{
    return STATUS_SUCCESS;
}


ULONG
ClassInitialize(
    IN  PVOID            Argument1,
    IN  PVOID            Argument2,
    IN  PCLASS_INIT_DATA InitializationData
    )

/*++

Routine Description:

    This routine is called by a class driver during its
    DriverEntry routine to initialize the driver.

Arguments:

    Argument1          - Driver Object.
    Argument2          - Registry Path.
    InitializationData - Device-specific driver's initialization data.

Return Value:

    A valid return code for a DriverEntry routine.

--*/

{
    PDRIVER_OBJECT  DriverObject = Argument1;
    PUNICODE_STRING RegistryPath = Argument2;

    PCLASS_DRIVER_EXTENSION driverExtension;

    NTSTATUS        status;

    PAGED_CODE();

    DebugPrint((3,"\n\nSCSI Class Driver\n"));

#if DBG
    if (InterlockedCompareExchange(&Globals.Initializing, 1, 0) == 0) {

        DebugPrint((1, "CLASSPNP.SYS => Initializing Globals...\n"));
        Globals.BreakOnLostIrps = FALSE;
        Globals.SecondsToWaitForIrps = 30;
        Globals.Index = -1;
        Globals.PrintBuffer = NULL;
        KeInitializeSpinLock(&(Globals.SpinLock));
        InterlockedExchange(&Globals.Initialized, 1);

    }
#endif // DBG


    //
    // Validate the length of this structure. This is effectively a
    // version check.
    //

    if (InitializationData->InitializationDataSize != sizeof(CLASS_INIT_DATA)) {

        DebugPrint((0,"ClassInitialize: Class driver wrong version\n"));
        return (ULONG) STATUS_REVISION_MISMATCH;
    }

    //
    // Check that each required entry is not NULL. Note that Shutdown, Flush and Error
    // are not required entry points.
    //

    if ((!InitializationData->FdoData.ClassDeviceControl) ||
        (!((InitializationData->FdoData.ClassReadWriteVerification) ||
           (InitializationData->ClassStartIo))) ||
        (!InitializationData->ClassAddDevice) ||
        (!InitializationData->FdoData.ClassStartDevice)) {

        DebugPrint((0,
            "ClassInitialize: Class device-specific driver missing required "
            "FDO entry\n"));

        return (ULONG) STATUS_REVISION_MISMATCH;
    }

    if ((InitializationData->ClassEnumerateDevice) &&
        ((!InitializationData->PdoData.ClassDeviceControl) ||
         (!InitializationData->PdoData.ClassStartDevice) ||
         (!((InitializationData->PdoData.ClassReadWriteVerification) ||
            (InitializationData->ClassStartIo))))) {

        DebugPrint((0, "ClassInitialize: Class device-specific missing "
                       "required PDO entry\n"));

        return (ULONG) STATUS_REVISION_MISMATCH;
    }

    if((InitializationData->FdoData.ClassStopDevice == NULL) ||
        ((InitializationData->ClassEnumerateDevice != NULL) &&
         (InitializationData->PdoData.ClassStopDevice == NULL))) {

        DebugPrint((0, "ClassInitialize: Class device-specific missing "
                       "required PDO entry\n"));
        ASSERT(FALSE);
        return (ULONG) STATUS_REVISION_MISMATCH;
    }

    //
    // Setup the default power handlers if the class driver didn't provide
    // any.
    //

    if(InitializationData->FdoData.ClassPowerDevice == NULL) {
        InitializationData->FdoData.ClassPowerDevice = ClassMinimalPowerHandler;
    }

    if((InitializationData->ClassEnumerateDevice != NULL) &&
       (InitializationData->PdoData.ClassPowerDevice == NULL)) {
        InitializationData->PdoData.ClassPowerDevice = ClassMinimalPowerHandler;
    }

    //
    // warn that unload is not supported
    // non-fatal error for now
    //

    if(InitializationData->ClassUnload == NULL) {
        DebugPrint((0, "ClassInitialize: driver does not support unload %wZ\n",
                    RegistryPath));
    }

    //
    // Create an extension for the driver object
    //

    status = IoAllocateDriverObjectExtension(DriverObject,
                                             CLASS_DRIVER_EXTENSION_KEY,
                                             sizeof(CLASS_DRIVER_EXTENSION),
                                             &driverExtension);

    if(NT_SUCCESS(status)) {

        //
        // Copy the registry path into the driver extension so we can use it later
        //

        driverExtension->RegistryPath.Length = RegistryPath->Length;
        driverExtension->RegistryPath.MaximumLength = RegistryPath->MaximumLength;

        driverExtension->RegistryPath.Buffer =
            ExAllocatePoolWithTag(PagedPool,
                                  RegistryPath->MaximumLength,
                                  '1CcS');

        if(driverExtension->RegistryPath.Buffer == NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            return status;
        }

        RtlCopyUnicodeString(
            &(driverExtension->RegistryPath),
            RegistryPath);

        //
        // Copy the initialization data into the driver extension so we can reuse
        // it during our add device routine
        //

        RtlCopyMemory(
            &(driverExtension->InitData),
            InitializationData,
            sizeof(CLASS_INIT_DATA));

        driverExtension->DeviceCount = 0;

    } else if (status == STATUS_OBJECT_NAME_COLLISION) {

        //
        // The extension already exists - get a pointer to it
        //

        driverExtension = IoGetDriverObjectExtension(DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);

        ASSERT(driverExtension != NULL);

    } else {

        DebugPrint((0, "ClassInitialize: Class driver extension could not be "
                       "allocated %lx\n", status));
        return status;
    }

    //
    // Update driver object with entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = ClassCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = ClassCreateClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = ClassReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = ClassReadWrite;
    DriverObject->MajorFunction[IRP_MJ_SCSI] = ClassInternalIoControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ClassDeviceControlDispatch;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = ClassShutdownFlush;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = ClassShutdownFlush;
    DriverObject->MajorFunction[IRP_MJ_PNP] = ClassDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = ClassDispatchPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = ClassSystemControl;

    if (InitializationData->ClassStartIo) {
        DriverObject->DriverStartIo = ClasspStartIo;
    }

    if ((InitializationData->ClassUnload) && (ClassPnpAllowUnload == TRUE)) {
        DriverObject->DriverUnload = ClassUnload;
    } else {
        DriverObject->DriverUnload = NULL;
    }

    DriverObject->DriverExtension->AddDevice = ClassAddDevice;

    status = STATUS_SUCCESS;
    return status;
}

VOID
ClassUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    called when there are no more references to the driver.  this allows
    drivers to be updated without rebooting.

Arguments:

    DriverObject - a pointer to the driver object that is being unloaded

Status:

--*/
{
    PCLASS_DRIVER_EXTENSION driverExtension;
    NTSTATUS status;

    PAGED_CODE();

    ASSERT( DriverObject->DeviceObject == NULL );

    driverExtension = IoGetDriverObjectExtension( DriverObject,
                                                  CLASS_DRIVER_EXTENSION_KEY
                                                  );

    ASSERT(driverExtension != NULL);
    ASSERT(driverExtension->RegistryPath.Buffer != NULL);
    ASSERT(driverExtension->InitData.ClassUnload != NULL);

    DebugPrint((1, "ClassUnload: driver unloading %wZ\n",
                &driverExtension->RegistryPath));

    //
    // attempt to process the driver's unload routine first.
    //

    driverExtension->InitData.ClassUnload(DriverObject);

    //
    // free own allocated resources and return
    //

    ExFreePool( driverExtension->RegistryPath.Buffer );
    driverExtension->RegistryPath.Buffer = NULL;
    driverExtension->RegistryPath.Length = 0;
    driverExtension->RegistryPath.MaximumLength = 0;

    return;
}



NTSTATUS
ClassAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )

/*++

Routine Description:

    SCSI class driver add device routine.  This is called by pnp when a new
    physical device come into being.

    This routine will call out to the class driver to verify that it should
    own this device then will create and attach a device object and then hand
    it to the driver to initialize and create symbolic links

Arguments:

    DriverObject - a pointer to the driver object that this is being created for
    PhysicalDeviceObject - a pointer to the physical device object

Status: STATUS_NO_SUCH_DEVICE if the class driver did not want this device
    STATUS_SUCCESS if the creation and attachment was successful
    status of device creation and initialization

--*/

{
    PCLASS_DRIVER_EXTENSION driverExtension =
        IoGetDriverObjectExtension(DriverObject,
                                   CLASS_DRIVER_EXTENSION_KEY);

    NTSTATUS status;

    PAGED_CODE();

    status = driverExtension->InitData.ClassAddDevice(DriverObject,
                                                      PhysicalDeviceObject);

    return STATUS_SUCCESS;
}


NTSTATUS
ClassDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    Storage class driver pnp routine.  This is called by the io system when
    a PNP request is sent to the device.

Arguments:

    DeviceObject - pointer to the device object

    Irp - pointer to the io request packet

Return Value:

    status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    BOOLEAN isFdo = commonExtension->IsFdo;

    PCLASS_DRIVER_EXTENSION driverExtension;
    PCLASS_INIT_DATA initData;
    PCLASS_DEV_INFO devInfo;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);

    NTSTATUS status = Irp->IoStatus.Status;
    BOOLEAN completeRequest = TRUE;
    BOOLEAN lockReleased = FALSE;

    ULONG isRemoved;

    PAGED_CODE();

    //
    // Extract all the useful information out of the driver object
    // extension
    //

    driverExtension = IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                                 CLASS_DRIVER_EXTENSION_KEY);

    initData = &(driverExtension->InitData);

    if(isFdo) {
        devInfo = &(initData->FdoData);
    } else {
        devInfo = &(initData->PdoData);
    }

    isRemoved = ClassAcquireRemoveLock(DeviceObject, Irp);

    DebugPrint((2, "ClassDispatchPnp (%p,%p): minor code %#x for %s %p\n",
                   DeviceObject, Irp,
                   irpStack->MinorFunction,
                   isFdo ? "fdo" : "pdo",
                   DeviceObject));
    DebugPrint((2, "ClassDispatchPnp (%p,%p): previous %#x, current %#x\n",
                   DeviceObject, Irp,
                   commonExtension->PreviousState,
                   commonExtension->CurrentState));

    switch(irpStack->MinorFunction) {

        case IRP_MN_START_DEVICE: {

            //
            // if this is sent to the FDO we should forward it down the
            // attachment chain before we start the FDO.
            //

            if(isFdo) {

                status = ClassForwardIrpSynchronous(commonExtension, Irp);

                if(!NT_SUCCESS(status)) {

                    //
                    // Start of the underlying driver failed - don't even
                    // attempt to start here.
                    //

                    break;
                }
            }

            status = Irp->IoStatus.Status = ClassPnpStartDevice(DeviceObject);

            break;
        }

        case IRP_MN_QUERY_DEVICE_RELATIONS: {

            DEVICE_RELATION_TYPE type =
                irpStack->Parameters.QueryDeviceRelations.Type;

            PDEVICE_RELATIONS deviceRelations = NULL;

            if(!isFdo) {

                if(type == TargetDeviceRelation) {

                    //
                    // Device relations has one entry built in to it's size.
                    //

                    status = STATUS_INSUFFICIENT_RESOURCES;

                    deviceRelations = ExAllocatePoolWithTag(PagedPool,
                                                     sizeof(DEVICE_RELATIONS),
                                                     '2CcS');

                    if(deviceRelations != NULL) {

                        RtlZeroMemory(deviceRelations,
                                      sizeof(DEVICE_RELATIONS));

                        Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;

                        deviceRelations->Count = 1;
                        deviceRelations->Objects[0] = DeviceObject;
                        ObReferenceObject(deviceRelations->Objects[0]);

                        status = STATUS_SUCCESS;
                    }

                } else {
                    //
                    // PDO's just complete enumeration requests without altering
                    // the status.
                    //

                    status = Irp->IoStatus.Status;
                }

                completeRequest = TRUE;
                break;

            } else if (type == BusRelations) {

                //
                // Make sure we support enumeration
                //

                if(initData->ClassEnumerateDevice == NULL) {

                    //
                    // Just send the request down to the lower driver.  Perhaps
                    // It can enumerate children.
                    //

                } else {

                    //
                    // Re-enumerate the device
                    //

                    status = ClassPnpQueryFdoRelations(DeviceObject, Irp);

                    if(!NT_SUCCESS(status)) {
                        completeRequest = TRUE;
                        break;
                    }
                }
            }

            IoCopyCurrentIrpStackLocationToNext(Irp);
            ClassReleaseRemoveLock(DeviceObject, Irp);
            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
            completeRequest = FALSE;

            break;
        }

        case IRP_MN_QUERY_ID: {

            BUS_QUERY_ID_TYPE idType = irpStack->Parameters.QueryId.IdType;
            UNICODE_STRING unicodeString;

            if(isFdo) {

                //
                // FDO's should just forward the query down to the lower
                // device objects
                //

                IoCopyCurrentIrpStackLocationToNext(Irp);
                ClassReleaseRemoveLock(DeviceObject, Irp);

                status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
                completeRequest = FALSE;
                break;
            }

            //
            // PDO's need to give an answer - this is easy for now
            //

            RtlInitUnicodeString(&unicodeString, NULL);

            status = ClassGetPdoId(DeviceObject,
                                   idType,
                                   &unicodeString);

            if(status == STATUS_NOT_IMPLEMENTED) {
                //
                // The driver doesn't implement this ID (whatever it is).
                // Use the status out of the IRP so that we don't mangle a
                // response from someone else.
                //

                status = Irp->IoStatus.Status;
            } else if(NT_SUCCESS(status)) {
                Irp->IoStatus.Information = (ULONG_PTR) unicodeString.Buffer;
            } else {
                Irp->IoStatus.Information = (ULONG_PTR) NULL;
            }

            break;
        }

        case IRP_MN_QUERY_STOP_DEVICE:
        case IRP_MN_QUERY_REMOVE_DEVICE: {

            completeRequest = TRUE;

            DebugPrint((2, "ClassDispatchPnp (%p,%p): Processing QUERY_%s irp\n",
                        DeviceObject, Irp,
                        ((irpStack->MinorFunction == IRP_MN_QUERY_STOP_DEVICE) ?
                         "STOP" : "REMOVE")));

            //
            // If this device is in use for some reason (paging, etc...)
            // then we need to fail the request.
            //

            if(commonExtension->PagingPathCount != 0) {

                DebugPrint((1, "ClassDispatchPnp (%p,%p): device is in paging "
                            "path and cannot be removed\n",
                            DeviceObject, Irp));
                status = STATUS_DEVICE_BUSY;
                break;
            }

            //
            // Check with the class driver to see if the query operation
            // can succeed.
            //

            if(irpStack->MinorFunction == IRP_MN_QUERY_STOP_DEVICE) {
                status = devInfo->ClassStopDevice(DeviceObject,
                                                  irpStack->MinorFunction);
            } else {
                status = devInfo->ClassRemoveDevice(DeviceObject,
                                                    irpStack->MinorFunction);
            }

            if(NT_SUCCESS(status)) {

                //
                // ASSERT that we never get two queries in a row, as
                // this will severly mess up the state machine
                //

                ASSERT(commonExtension->CurrentState != irpStack->MinorFunction);

                commonExtension->PreviousState =
                    commonExtension->CurrentState;
                commonExtension->CurrentState = irpStack->MinorFunction;

                if(isFdo) {
                    DebugPrint((2, "ClassDispatchPnp (%p,%p): Forwarding QUERY_"
                                "%s irp\n", DeviceObject, Irp,
                                ((irpStack->MinorFunction == IRP_MN_QUERY_STOP_DEVICE) ?
                                 "STOP" : "REMOVE")));
                    status = ClassForwardIrpSynchronous(commonExtension, Irp);
                }
            }
            DebugPrint((2, "ClassDispatchPnp (%p,%p): Final status == %x\n",
                        DeviceObject, Irp, status));

            break;
        }

        case IRP_MN_CANCEL_STOP_DEVICE:
        case IRP_MN_CANCEL_REMOVE_DEVICE: {

            //
            // Check with the class driver to see if the query or cancel
            // operation can succeed.
            //

            if(irpStack->MinorFunction == IRP_MN_CANCEL_STOP_DEVICE) {
                status = devInfo->ClassStopDevice(DeviceObject,
                                                  irpStack->MinorFunction);
                ASSERTMSG("ClassDispatchPnp !! CANCEL_STOP_DEVICE should "
                          "never be failed\n", NT_SUCCESS(status));
            } else {
                status = devInfo->ClassRemoveDevice(DeviceObject,
                                                    irpStack->MinorFunction);
                ASSERTMSG("ClassDispatchPnp !! CANCEL_REMOVE_DEVICE should "
                          "never be failed\n", NT_SUCCESS(status));
            }

            Irp->IoStatus.Status = status;

            //
            // We got a CANCEL - roll back to the previous state only
            // if the current state is the respective QUERY state.
            //

            if(((irpStack->MinorFunction == IRP_MN_CANCEL_STOP_DEVICE) &&
                (commonExtension->CurrentState == IRP_MN_QUERY_STOP_DEVICE)
                ) ||
               ((irpStack->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) &&
                (commonExtension->CurrentState == IRP_MN_QUERY_REMOVE_DEVICE)
                )
               ) {

                commonExtension->CurrentState =
                    commonExtension->PreviousState;
                commonExtension->PreviousState = 0xff;

            }

            if(isFdo) {
                IoCopyCurrentIrpStackLocationToNext(Irp);
                ClassReleaseRemoveLock(DeviceObject, Irp);
                status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
                completeRequest = FALSE;
            } else {
                status = STATUS_SUCCESS;
                completeRequest = TRUE;
            }

            break;
        }

        case IRP_MN_STOP_DEVICE: {

            //
            // These all mean nothing to the class driver currently.  The
            // port driver will handle all queueing when necessary.
            //

            DebugPrint((2, "ClassDispatchPnp (%p,%p): got stop request for %s\n",
                        DeviceObject, Irp,
                        (isFdo ? "fdo" : "pdo")
                        ));

            ASSERT(commonExtension->PagingPathCount == 0);

            //
            // BUGBUG - stopping the timer here may prevent class
            //          drivers from stopping if they must do io
            //          to stop.  this is currently not the case.
            //

            if (DeviceObject->Timer) {
                IoStopTimer(DeviceObject);
            }

            status = devInfo->ClassStopDevice(DeviceObject, IRP_MN_STOP_DEVICE);

            ASSERTMSG("ClassDispatchPnp !! STOP_DEVICE should "
                      "never be failed\n", NT_SUCCESS(status));

            if(isFdo) {
                status = ClassForwardIrpSynchronous(commonExtension, Irp);
            }

            if(NT_SUCCESS(status)) {
                commonExtension->CurrentState = irpStack->MinorFunction;
                commonExtension->PreviousState = 0xff;
            }

            completeRequest = TRUE;
            break;
        }

        case IRP_MN_REMOVE_DEVICE:
        case IRP_MN_SURPRISE_REMOVAL: {

            PDEVICE_OBJECT lowerDeviceObject =
                commonExtension->LowerDeviceObject;

            DebugPrint((2, "ClassDispatchPnp (%p,%p): %s remove request for %s\n",
                        DeviceObject, Irp,
                        ((irpStack->MinorFunction == IRP_MN_SURPRISE_REMOVAL) ?
                         "surprise " : ""),
                        (isFdo ? "fdo" : "pdo")
                        ));

            if (commonExtension->PagingPathCount != 0) {
                DebugPrint((0, "ClassDispatchPnp (%p,%p): paging device is "
                            "getting removed!\n", DeviceObject, Irp));
            }

            //
            // Release the lock for this IRP before calling in.
            //

            ClassReleaseRemoveLock(DeviceObject, Irp);
            lockReleased = TRUE;

            //
            // If the once a second timer was enabled for this device we
            // stop it.
            //

            if (DeviceObject->Timer) {
                IoStopTimer(DeviceObject);
            }

            //
            // Send the remove IRP to the device we're attached to.  This
            // should convince it to abort all outstanding i/o in the case
            // of a remove (surprise remove will be somewhat benign but we'll
            // get a real remove after that).
            //

            if (isFdo) {
                status = ClassForwardIrpSynchronous(commonExtension, Irp);
                ASSERT(status == STATUS_SUCCESS);
            }

            status = ClassRemoveDevice(DeviceObject, irpStack->MinorFunction);

            if (irpStack->MinorFunction == IRP_MN_REMOVE_DEVICE) {
                ASSERTMSG("ClassDispatchPnp !! REMOVE_DEVICE was failed by "
                          "lower driver.\n", NT_SUCCESS(status));
            } else {
                ASSERTMSG("ClassDispatchPnp !! SURPRISE_REMOVAL was failed "
                          " by lower driver.\n", NT_SUCCESS(status));
            }

            commonExtension->PreviousState = commonExtension->CurrentState;
            commonExtension->CurrentState = irpStack->MinorFunction;

            //
            // Set the status for the lower device so that irp checking doesn't
            // think we failed to handle this irp.
            //

            Irp->IoStatus.Status = status;

            break;
        }

        case IRP_MN_DEVICE_USAGE_NOTIFICATION: {
            completeRequest = TRUE;

            switch(irpStack->Parameters.UsageNotification.Type) {

                case DeviceUsageTypePaging: {

                    BOOLEAN setPagable;

                    if((irpStack->Parameters.UsageNotification.InPath) &&
                       (commonExtension->CurrentState != IRP_MN_START_DEVICE)) {

                        //
                        // Device isn't started.  Don't allow adding a
                        // paging file, but allow a removal of one.
                        //

                        status = STATUS_DEVICE_NOT_READY;
                        break;
                    }

                    ASSERT(commonExtension->IsInitialized);

                    //
                    // need to synchronize this now...
                    //

                    status = KeWaitForSingleObject(&commonExtension->PathCountEvent,
                                                   Executive, KernelMode,
                                                   FALSE, NULL);
                    ASSERT(NT_SUCCESS(status));
                    status = STATUS_SUCCESS;

                    //
                    // If the volume is removable we should try to lock it in
                    // place or unlock it once per paging path count
                    //

                    if(commonExtension->IsFdo) {
                        status = ClasspEjectionControl(
                                    DeviceObject->DeviceExtension,
                                    Irp,
                                    InternalMediaLock,
                                    (BOOLEAN)irpStack->Parameters.UsageNotification.InPath);
                    }

                    if(!NT_SUCCESS(status)) {
                        KeSetEvent(&commonExtension->PathCountEvent, IO_NO_INCREMENT, FALSE);
                        break;
                    }

                    //
                    // if removing last paging device, need to set DO_POWER_PAGABLE
                    // bit here, and possible re-set it below on failure.
                    //

                    setPagable = FALSE;

                    if (!irpStack->Parameters.UsageNotification.InPath &&
                        commonExtension->PagingPathCount == 1
                        ) {

                        //
                        // removing last paging file
                        // must have DO_POWER_PAGABLE bits set, but only
                        // if noone set the DO_POWER_INRUSH bit
                        //


                        if (TEST_FLAG(DeviceObject->Flags, DO_POWER_INRUSH)) {
                            DebugPrint((2, "ClassDispatchPnp (%p,%p): Last "
                                        "paging file removed, but "
                                        "DO_POWER_INRUSH was set, so NOT "
                                        "setting DO_POWER_PAGABLE\n",
                                        DeviceObject, Irp));
                        } else {
                            DebugPrint((2, "ClassDispatchPnp (%p,%p): Last "
                                        "paging file removed, "
                                        "setting DO_POWER_PAGABLE\n",
                                        DeviceObject, Irp));
                            SET_FLAG(DeviceObject->Flags, DO_POWER_PAGABLE);
                            setPagable = TRUE;
                        }

                    }

                    //
                    // forward the irp before finishing handling the
                    // special cases
                    //

                    status = ClassForwardIrpSynchronous(commonExtension, Irp);

                    //
                    // now deal with the failure and success cases.
                    // note that we are not allowed to fail the irp
                    // once it is sent to the lower drivers.
                    //

                    if (NT_SUCCESS(status)) {

                        IoAdjustPagingPathCount(
                            &commonExtension->PagingPathCount,
                            irpStack->Parameters.UsageNotification.InPath);

                        if (irpStack->Parameters.UsageNotification.InPath) {
                            if (commonExtension->PagingPathCount == 1) {
                                DebugPrint((2, "ClassDispatchPnp (%p,%p): "
                                            "Clearing PAGABLE bit\n",
                                            DeviceObject, Irp));
                                CLEAR_FLAG(DeviceObject->Flags, DO_POWER_PAGABLE);
                            }
                        }

                    } else {

                        //
                        // cleanup the changes done above
                        //

                        if (setPagable == TRUE) {
                            DebugPrint((2, "ClassDispatchPnp (%p,%p): Unsetting "
                                        "PAGABLE bit due to irp failure\n",
                                        DeviceObject, Irp));
                            CLEAR_FLAG(DeviceObject->Flags, DO_POWER_PAGABLE);
                            setPagable = FALSE;
                        }

                        //
                        // relock or unlock the media if needed.
                        //

                        if(commonExtension->IsFdo) {
                            ClasspEjectionControl(
                                DeviceObject->DeviceExtension,
                                Irp,
                                InternalMediaLock,
                                (BOOLEAN)!irpStack->Parameters.UsageNotification.InPath);
                        }

                    }

                    //
                    // set the event so the next one can occur.
                    //

                    KeSetEvent(&commonExtension->PathCountEvent,
                               IO_NO_INCREMENT, FALSE);
                    break;
                }

                case DeviceUsageTypeHibernation: {

                    IoAdjustPagingPathCount(
                        &commonExtension->HibernationPathCount,
                        irpStack->Parameters.UsageNotification.InPath
                        );
                    status = ClassForwardIrpSynchronous(commonExtension, Irp);
                    if (!NT_SUCCESS(status)) {
                        IoAdjustPagingPathCount(
                            &commonExtension->HibernationPathCount,
                            !irpStack->Parameters.UsageNotification.InPath
                            );
                    }

                    break;
                }

                case DeviceUsageTypeDumpFile: {
                    IoAdjustPagingPathCount(
                        &commonExtension->DumpPathCount,
                        irpStack->Parameters.UsageNotification.InPath
                        );
                    status = ClassForwardIrpSynchronous(commonExtension, Irp);
                    if (!NT_SUCCESS(status)) {
                        IoAdjustPagingPathCount(
                            &commonExtension->DumpPathCount,
                            !irpStack->Parameters.UsageNotification.InPath
                            );
                    }

                    break;
                }

                default: {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
            }
            break;
        }

        case IRP_MN_QUERY_CAPABILITIES: {

            DebugPrint((2, "ClassDispatchPnp (%p,%p): QueryCapabilities\n",
                        DeviceObject, Irp));

            if(!isFdo) {

                status = ClassQueryPnpCapabilities(
                            DeviceObject,
                            irpStack->Parameters.DeviceCapabilities.Capabilities
                            );

                completeRequest = TRUE;
                break;

            } else {

                //
                // BUGBUG: Need to attach a completion routine if necessary
                // so we can call the query capabilities routine as the irp
                // is completed.  For now just fall through.
                //
            }
        }

        default: {

            if(isFdo) {
                IoCopyCurrentIrpStackLocationToNext(Irp);

                ClassReleaseRemoveLock(DeviceObject, Irp);
                status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);

                completeRequest = FALSE;
            }

            break;
        }
    }

    if(completeRequest) {
        Irp->IoStatus.Status = status;

        if(lockReleased == FALSE) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
        }

        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
    }

    DebugPrint((2, "ClassDispatchPnp (%p,%p): leaving with previous %#x, current %#x\n",
                   DeviceObject, Irp,
                   commonExtension->PreviousState,
                   commonExtension->CurrentState));
    return status;
}


NTSTATUS
ClassPnpStartDevice(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    Storage class driver routine for IRP_MN_START_DEVICE requests.
    This routine kicks off any device specific initialization

Arguments:

    DeviceObject - a pointer to the device object

    Irp - a pointer to the io request packet

Return Value:

    none

--*/

{
    PCLASS_DRIVER_EXTENSION driverExtension;
    PCLASS_INIT_DATA initData;

    PCLASS_DEV_INFO devInfo;

    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    BOOLEAN isFdo = commonExtension->IsFdo;

    BOOLEAN isMountedDevice = TRUE;
    UNICODE_STRING  interfaceName;

    BOOLEAN timerStarted;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    driverExtension = IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                                 CLASS_DRIVER_EXTENSION_KEY);

    initData = &(driverExtension->InitData);
    if(isFdo) {
        devInfo = &(initData->FdoData);
    } else {
        devInfo = &(initData->PdoData);
    }

    ASSERT(devInfo->ClassInitDevice != NULL);
    ASSERT(devInfo->ClassStartDevice != NULL);

    if(!commonExtension->IsInitialized) {

        if(isFdo) {
            STORAGE_PROPERTY_ID propertyId;
            PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
                DeviceObject->DeviceExtension;

            //
            // Call port driver to get adapter capabilities.
            //

            propertyId = StorageAdapterProperty;

            status = ClassGetDescriptor(
                        commonExtension->LowerDeviceObject,
                        &propertyId,
                        &fdoExtension->AdapterDescriptor);

            if(!NT_SUCCESS(status)) {
                DebugPrint((0, "ClassPnpStartDevice: ClassGetDescriptor "
                               "[ADAPTER] failed %lx\n", status));
                return status;
            }

            //
            // Call port driver to get device descriptor.
            //

            propertyId = StorageDeviceProperty;

            status = ClassGetDescriptor(
                        commonExtension->LowerDeviceObject,
                        &propertyId,
                        &fdoExtension->DeviceDescriptor);

            if(!NT_SUCCESS(status)) {
                DebugPrint((0, "ClassPnpStartDevice: ClassGetDescriptor "
                               "[DEVICE] failed %lx\n", status));
                return status;
            }

        } // end FDO

        status = devInfo->ClassInitDevice(DeviceObject);
    } else {
        status = STATUS_SUCCESS;
    }

    if(!NT_SUCCESS(status)) {

        //
        // Just bail out - the remove that comes down will clean up the
        // initialized scraps.
        //

        return status;
    } else {
        commonExtension->IsInitialized = TRUE;
    }

    //
    // If device requests autorun functionality or a once a second callback
    // then enable the once per second timer.
    //
    // NOTE: This assumes that ClassInitializeMediaChangeDetection is always
    //       called in the context of the ClassInitDevice callback. If called
    //       after then this check will have already been made and the
    //       once a second timer will not have been enabled.
    //
    if ((isFdo) &&
        ((initData->ClassTick != NULL) ||
         ((fdoExtension->MediaChangeDetectionInfo != NULL) &&
          (fdoExtension->MediaChangeDetectionInfo->MediaChangeDetectionDisableCount == 0)) ||
         ((fdoExtension->FailurePredictionInfo != NULL) &&
          (fdoExtension->FailurePredictionInfo->Method != FailurePredictionNone))))
    {
        ClasspEnableTimer(DeviceObject);
        timerStarted = TRUE;
    } else {
        timerStarted = FALSE;
    }

    //
    // NOTE: the timer looks at commonExtension->CurrentState now
    //       to prevent Media Change Notification code from running
    //       until the device is started, but allows the device
    //       specific tick handler to run.  therefore it is imperative
    //       that commonExtension->CurrentState not be updated until
    //       the device specific startdevice handler has finished.
    //

    status = devInfo->ClassStartDevice(DeviceObject);

    if(NT_SUCCESS(status)) {
        commonExtension->CurrentState = IRP_MN_START_DEVICE;

        if((isFdo) && (initData->ClassEnumerateDevice != NULL)) {
            isMountedDevice = FALSE;
        }

        if((DeviceObject->DeviceType != FILE_DEVICE_DISK) &&
           (DeviceObject->DeviceType != FILE_DEVICE_CD_ROM)) {

            isMountedDevice = FALSE;
        }


        if(isMountedDevice) {
            ClasspRegisterMountedDeviceInterface(DeviceObject);
        }

        if((commonExtension->IsFdo) &&
           (devInfo->ClassWmiInfo.GuidRegInfo != NULL)) {

            IoWMIRegistrationControl(DeviceObject, WMIREG_ACTION_REGISTER);
        }
    } else {

        if (timerStarted) {
            ClasspDisableTimer(DeviceObject);
        }
    }

    return status;
}


NTSTATUS
ClassReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the system entry point for read and write requests. The
    device-specific handler is invoked to perform any validation necessary.

    If the device object is a PDO (partition object) then the request will
    simply be adjusted for Partition0 and issued to the lower device driver.

    IF the device object is an FDO (paritition 0 object), the number of bytes
    in the request are checked against the maximum byte counts that the adapter
    supports and requests are broken up into
    smaller sizes if necessary.

Arguments:

    DeviceObject - a pointer to the device object for this request

    Irp - IO request

Return Value:

    NT Status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PDEVICE_OBJECT      lowerDeviceObject = commonExtension->LowerDeviceObject;

    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

    LARGE_INTEGER       startingOffset = currentIrpStack->Parameters.Read.ByteOffset;

    ULONG               transferByteCount = currentIrpStack->Parameters.Read.Length;

    ULONG               isRemoved;

    NTSTATUS            status;

    //
    // Grab the remove lock.  If we can't acquire it, bail out.
    //

    isRemoved = ClassAcquireRemoveLock(DeviceObject, Irp);

    if(isRemoved) {
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    if (TEST_FLAG(DeviceObject->Flags, DO_VERIFY_VOLUME) &&
        (currentIrpStack->MinorFunction != CLASSP_VOLUME_VERIFY_CHECKED) &&
        !TEST_FLAG(currentIrpStack->Flags, SL_OVERRIDE_VERIFY_VOLUME)) {

        //
        // if DO_VERIFY_VOLUME bit is set
        // in device object flags, fail request.
        //

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);

        Irp->IoStatus.Status = STATUS_VERIFY_REQUIRED;
        Irp->IoStatus.Information = 0;

        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, 0);
        return STATUS_VERIFY_REQUIRED;
    }

    //
    // Since we've bypassed the verify-required tests we don't need to repeat
    // them with this IRP - in particular we don't want to worry about
    // hitting them at the partition 0 level if the request has come through
    // a non-zero partition.
    //

    currentIrpStack->MinorFunction = CLASSP_VOLUME_VERIFY_CHECKED;

    //
    // Invoke the device specific routine to do whatever it needs to verify
    // this request.
    //

    ASSERT(commonExtension->DevInfo->ClassReadWriteVerification);

    status = commonExtension->DevInfo->ClassReadWriteVerification(DeviceObject,
                                                                  Irp);

    if (!NT_SUCCESS(status)) {

        //
        // It is up to the device specific driver to set the Irp status
        //

        if (Irp->IoStatus.Status != status) {
            DebugPrint((0, "ClassReadWrite (%p): ClassReadWriteVerification "
                        "must set irp status on error\n", DeviceObject));
            ASSERT(!"Irp->IoStatus.Status != status returned from ClassReadWriteVerification");
        }

        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest (DeviceObject, Irp, IO_NO_INCREMENT);
        return status;

    } else if (status == STATUS_PENDING) {

        //
        // It is the responsibility of the device-specific driver to Mark the irp pending.
        //

        return STATUS_PENDING;
    }

    //
    // Check for a zero length IO, as several macros will turn this into
    // seemingly a 0xffffffff length request.
    //

    if (transferByteCount == 0) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;

        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

        return STATUS_SUCCESS;

    }

    if (commonExtension->DriverExtension->InitData.ClassStartIo) {

        IoMarkIrpPending(Irp);

        IoStartPacket(DeviceObject,
                      Irp,
                      NULL,
                      NULL);

        return STATUS_PENDING;
    }

    //
    // Add partition byte offset to make starting byte relative to
    // beginning of disk.
    //

    currentIrpStack->Parameters.Read.ByteOffset.QuadPart +=
        commonExtension->StartingOffset.QuadPart;

    if(commonExtension->IsFdo) {

        PSTORAGE_ADAPTER_DESCRIPTOR adapterDescriptor =
            commonExtension->PartitionZeroExtension->AdapterDescriptor;

        ULONG transferPages;
        ULONG maximumTransferLength = adapterDescriptor->MaximumTransferLength;

        //
        // Add in any skew for the disk manager software.
        //

        currentIrpStack->Parameters.Read.ByteOffset.QuadPart +=
             commonExtension->PartitionZeroExtension->DMByteSkew;

        //
        // Calculate number of pages in this transfer.
        //

        transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                            MmGetMdlVirtualAddress(Irp->MdlAddress),
                            currentIrpStack->Parameters.Read.Length);

        //
        // Check if request length is greater than the maximum number of
        // bytes that the hardware can transfer.
        //

        if (currentIrpStack->Parameters.Read.Length > maximumTransferLength ||
            transferPages > adapterDescriptor->MaximumPhysicalPages) {

             DebugPrint((2,"ClassReadWrite: Request greater than maximum\n"));
             DebugPrint((2,"ClassReadWrite: Maximum is %lx\n",
                         maximumTransferLength));
             DebugPrint((2,"ClassReadWrite: Byte count is %lx\n",
                         currentIrpStack->Parameters.Read.Length));

             transferPages = adapterDescriptor->MaximumPhysicalPages - 1;

             if (maximumTransferLength > transferPages << PAGE_SHIFT ) {
                 maximumTransferLength = transferPages << PAGE_SHIFT;
             }

            //
            // Check that maximum transfer size is not zero.
            //

            if (maximumTransferLength == 0) {
                maximumTransferLength = PAGE_SIZE;
            }

            //
            // Mark IRP with status pending.
            //

            IoMarkIrpPending(Irp);

            //
            // Request greater than port driver maximum.
            // Break up into smaller routines.
            //

            ClassSplitRequest(DeviceObject, Irp, maximumTransferLength);

            return STATUS_PENDING;
        }

        //
        // Build SRB and CDB for this IRP.
        //

        status = ClassBuildRequest(DeviceObject, Irp);

    } else {

        IoCopyCurrentIrpStackLocationToNext(Irp);

        ClassReleaseRemoveLock(DeviceObject, Irp);
    }

    if(!NT_SUCCESS(status)) {

        Irp->IoStatus.Status = status;
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

        return status;
    }

    //
    // Return the results of the call to the port driver.
    //

    status = IoCallDriver(lowerDeviceObject, Irp);
    return status;

} // end ClassReadWrite()


NTSTATUS
ClassReadDriveCapacity(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This routine sends a READ CAPACITY to the requested device, updates
    the geometry information in the device object and returns
    when it is complete.  This routine is synchronous.

    This routine must be called with the remove lock held or some other
    assurance that the Fdo will not be removed while processing.

Arguments:

    DeviceObject - Supplies a pointer to the device object that represents
        the device whose capacity is to be read.

Return Value:

    Status is returned.

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    ULONG               retries = 1;
    ULONG               lastSector;
    PCDB                cdb;
    PREAD_CAPACITY_DATA readCapacityBuffer;
    SCSI_REQUEST_BLOCK  srb;
    NTSTATUS            status;

    ASSERT(fdoExtension->CommonExtension.IsFdo);

    //
    // Allocate read capacity buffer from nonpaged pool.
    //

    readCapacityBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                        sizeof(READ_CAPACITY_DATA),
                                        '4CcS');

    if (!readCapacityBuffer) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Build the read capacity CDB.
    //

    srb.CdbLength = 10;
    cdb = (PCDB)srb.Cdb;

    //
    // Set timeout value from device extension.
    //

    srb.TimeOutValue = fdoExtension->TimeOutValue;

    cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;

Retry:

    status = ClassSendSrbSynchronous(Fdo,
                                     &srb,
                                     readCapacityBuffer,
                                     sizeof(READ_CAPACITY_DATA),
                                     FALSE);

    if (NT_SUCCESS(status)) {

        ULONG cylinderSize;

        //
        // Copy sector size from read capacity buffer to device extension
        // in reverse byte order.
        //

        ((PFOUR_BYTE)&fdoExtension->DiskGeometry.BytesPerSector)->Byte0 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte3;

        ((PFOUR_BYTE)&fdoExtension->DiskGeometry.BytesPerSector)->Byte1 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte2;

        ((PFOUR_BYTE)&fdoExtension->DiskGeometry.BytesPerSector)->Byte2 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte1;

        ((PFOUR_BYTE)&fdoExtension->DiskGeometry.BytesPerSector)->Byte3 =
            ((PFOUR_BYTE)&readCapacityBuffer->BytesPerBlock)->Byte0;

        //
        // Copy last sector in reverse byte order.
        //

        ((PFOUR_BYTE)&lastSector)->Byte0 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte3;

        ((PFOUR_BYTE)&lastSector)->Byte1 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte2;

        ((PFOUR_BYTE)&lastSector)->Byte2 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte1;

        ((PFOUR_BYTE)&lastSector)->Byte3 =
            ((PFOUR_BYTE)&readCapacityBuffer->LogicalBlockAddress)->Byte0;

        //
        // Calculate sector to byte shift.
        //

        WHICH_BIT(fdoExtension->DiskGeometry.BytesPerSector,
                  fdoExtension->SectorShift);

        DebugPrint((2,"SCSI ClassReadDriveCapacity: Sector size is %d\n",
            fdoExtension->DiskGeometry.BytesPerSector));

        DebugPrint((2,"SCSI ClassReadDriveCapacity: Number of Sectors is %d\n",
            lastSector + 1));

        if(fdoExtension->DMActive == TRUE) {
            DebugPrint((0, "SCSI ClassReadDriveCapacity: reducing sector size "
                        "by %d sectors\n",
                        fdoExtension->DMSkew));
            lastSector -= fdoExtension->DMSkew;
        }

        //
        // Calculate media capacity in bytes.
        //

        fdoExtension->CommonExtension.PartitionLength.QuadPart =
            (LONGLONG)(lastSector + 1);

        //
        // Check to see if we have a geometry we should be using already.
        //

        cylinderSize = (fdoExtension->DiskGeometry.TracksPerCylinder *
                        fdoExtension->DiskGeometry.SectorsPerTrack);

        if(cylinderSize == 0) {
            DebugPrint((1, "ClassReadDriveCapacity: resetting H & S geometry "
                           "values from %#x/%#x to %#x/%#x\n",
                        fdoExtension->DiskGeometry.TracksPerCylinder,
                        fdoExtension->DiskGeometry.SectorsPerTrack,
                        64,
                        32));

            fdoExtension->DiskGeometry.SectorsPerTrack = 0x3f;
            fdoExtension->DiskGeometry.TracksPerCylinder = 0xff;
            //fdoExtension->DiskGeometry.SectorsPerTrack = 0x20;
            //fdoExtension->DiskGeometry.TracksPerCylinder = 0x40;

            cylinderSize = (fdoExtension->DiskGeometry.TracksPerCylinder *
                            fdoExtension->DiskGeometry.SectorsPerTrack);
        }

        //
        // Calculate number of cylinders.
        //

        fdoExtension->DiskGeometry.Cylinders.QuadPart =
            (LONGLONG)((lastSector + 1)/cylinderSize);

        fdoExtension->CommonExtension.PartitionLength.QuadPart =
            (fdoExtension->CommonExtension.PartitionLength.QuadPart <<
             fdoExtension->SectorShift);

        if (Fdo->Characteristics & FILE_REMOVABLE_MEDIA) {

            //
            // This device supports removable media.
            //

            fdoExtension->DiskGeometry.MediaType = RemovableMedia;

        } else {

            //
            // Assume media type is fixed disk.
            //

            fdoExtension->DiskGeometry.MediaType = FixedMedia;
        }
    }

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // Routine ClassSendSrbSynchronous does not retry
        // requests returned with this status.
        // Read Capacities should be retried
        // anyway.
        //

        if (retries--) {

            //
            // Retry request.
            //

            goto Retry;
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // If the read capacity fails, set the geometry to reasonable parameter
        // so things don't fail at unexpected places.  Zero the geometry
        // except for the bytes per sector and sector shift.
        //

        RtlZeroMemory(&(fdoExtension->DiskGeometry), sizeof(DISK_GEOMETRY));

        fdoExtension->DiskGeometry.BytesPerSector = 512;
        fdoExtension->SectorShift = 9;
        fdoExtension->CommonExtension.PartitionLength.QuadPart = (LONGLONG) 0;

        if (Fdo->Characteristics & FILE_REMOVABLE_MEDIA) {

            //
            // This device supports removable media.
            //

            fdoExtension->DiskGeometry.MediaType = RemovableMedia;

        } else {

            //
            // Assume media type is fixed disk.
            //

            fdoExtension->DiskGeometry.MediaType = FixedMedia;
        }
    }

    //
    // Deallocate read capacity buffer.
    //

    ExFreePool(readCapacityBuffer);

    return status;

} // end ClassReadDriveCapacity()



VOID
ClassSendStartUnit(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    Send command to SCSI unit to start or power up.
    Because this command is issued asynchronounsly, that is, without
    waiting on it to complete, the IMMEDIATE flag is not set. This
    means that the CDB will not return until the drive has powered up.
    This should keep subsequent requests from being submitted to the
    device before it has completely spun up.
    This routine is called from the InterpretSense routine, when a
    request sense returns data indicating that a drive must be
    powered up.

Arguments:

    Fdo - The functional device object for the stopped device.

Return Value:

    None.

--*/
{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCOMPLETION_CONTEXT context;
    PCDB cdb;

    //
    // Allocate Srb from nonpaged pool.
    //

    context = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                             sizeof(COMPLETION_CONTEXT),
                             '6CcS');

    //
    // Save the device object in the context for use by the completion
    // routine.
    //

    context->DeviceObject = Fdo;
    srb = &context->Srb;

    //
    // Zero out srb.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // Set timeout value large enough for drive to spin up.
    //

    srb->TimeOutValue = START_UNIT_TIMEOUT;

    //
    // Set the transfer length.
    //

    srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER |
                    SRB_FLAGS_DISABLE_AUTOSENSE |
                    SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Build the start unit CDB.
    //

    srb->CdbLength = 6;
    cdb = (PCDB)srb->Cdb;

    cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
    cdb->START_STOP.Start = 1;
    cdb->START_STOP.Immediate = 0;
    cdb->START_STOP.LogicalUnitNumber = srb->Lun;

    //
    // Build the asynchronous request to be sent to the port driver.
    // Since this routine is called from a DPC the IRP should always be
    // available.
    //

    irp = IoAllocateIrp(Fdo->StackSize, FALSE);

    if(irp == NULL) {

        KeBugCheck(SCSI_DISK_DRIVER_INTERNAL);

    }

    ClassAcquireRemoveLock(Fdo, irp);

    IoSetCompletionRoutine(irp,
                           (PIO_COMPLETION_ROUTINE)ClassAsynchronousCompletion,
                           context,
                           TRUE,
                           TRUE,
                           TRUE);

    irpStack = IoGetNextIrpStackLocation(irp);
    irpStack->MajorFunction = IRP_MJ_SCSI;
    srb->OriginalRequest = irp;

    //
    // Store the SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = srb;

    //
    // Call the port driver with the IRP.
    //

    IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp);

    return;

} // end StartUnit()


NTSTATUS
ClassAsynchronousCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
/*++

Routine Description:

    This routine is called when an asynchronous I/O request
    which was issused by the class driver completes.  Examples of such requests
    are release queue or START UNIT. This routine releases the queue if
    necessary.  It then frees the context and the IRP.

Arguments:

    DeviceObject - The device object for the logical unit; however since this
        is the top stack location the value is NULL.

    Irp - Supplies a pointer to the Irp to be processed.

    Context - Supplies the context to be used to process this request.

Return Value:

    None.

--*/

{
    PCOMPLETION_CONTEXT context = Context;
    PSCSI_REQUEST_BLOCK srb;

    if(DeviceObject == NULL) {

        DeviceObject = context->DeviceObject;
    }

    srb = &context->Srb;

    //
    // If this is an execute srb, then check the return status and make sure.
    // the queue is not frozen.
    //

    if (srb->Function == SRB_FUNCTION_EXECUTE_SCSI) {

        //
        // Check for a frozen queue.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

            //
            // Unfreeze the queue getting the device object from the context.
            //

            ClassReleaseQueue(context->DeviceObject);
        }
    }

    //
    // Free the context and the Irp.
    //

    if (Irp->MdlAddress != NULL) {
        MmUnlockPages(Irp->MdlAddress);
        IoFreeMdl(Irp->MdlAddress);

        Irp->MdlAddress = NULL;
    }

    ClassReleaseRemoveLock(DeviceObject, Irp);

    ExFreePool(context);
    IoFreeIrp(Irp);

    //
    // Indicate the I/O system should stop processing the Irp completion.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // ClassAsynchronousCompletion()


VOID
ClassSplitRequest(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp,
    IN ULONG MaximumBytes
    )

/*++

Routine Description:

    Break request into smaller requests.  Each new request will be the
    maximum transfer size that the port driver can handle or if it
    is the final request, it may be the residual size.

    The number of IRPs required to process this request is written in the
    current stack of the original IRP. Then as each new IRP completes
    the count in the original IRP is decremented. When the count goes to
    zero, the original IRP is completed.

Arguments:

    Fdo - Pointer to the functional class device object to be addressed.

    Irp - Pointer to Irp the orginal request.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);

    ULONG transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER startingOffset = currentIrpStack->Parameters.Read.ByteOffset;

    PVOID dataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
    ULONG dataLength = MaximumBytes;

    LONG irpCount = (transferByteCount + MaximumBytes - 1) / MaximumBytes;
    LONG i;

    PSCSI_REQUEST_BLOCK srb;

    DebugPrint((2, "ClassSplitRequest: Requires %d IRPs\n", irpCount));
    DebugPrint((2, "ClassSplitRequest: Original IRP %p\n", Irp));

    ASSERT(fdoExtension->CommonExtension.IsFdo);
    ASSERT(irpCount > 0);

    //
    // If all partial transfers complete successfully then the status and
    // bytes transferred are already set up. Failing a partial-transfer IRP
    // will set status to error and bytes transferred to 0 during
    // IoCompletion. Setting bytes transferred to 0 if an IRP fails allows
    // asynchronous partial transfers. This is an optimization for the
    // successful case.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = transferByteCount;

    //
    // Save number of IRPs to complete count on current stack
    // of original IRP.
    //

    nextIrpStack->Parameters.Others.Argument1 = LongToPtr( irpCount );

    for ( ; irpCount > 0; irpCount--) {

        PIRP newIrp;
        PIO_STACK_LOCATION newIrpStack;

        //
        // Allocate new IRP.
        //

        newIrp = IoAllocateIrp(Fdo->StackSize, FALSE);

        if (newIrp == NULL) {

            //
            // If an Irp can't be allocated then the orginal request cannot
            // be executed.  If this is the first request then just fail the
            //

            DebugPrint((1,"ClassSplitRequest: Can't allocate Irp\n"));

            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;

            goto CleanupSplitRequest;

        }

        DebugPrint((2, "ClassSplitRequest: New IRP %p\n", newIrp));

        //
        // Write MDL address to new IRP. In the port driver the SRB data
        // buffer field is used as an offset into the MDL, so the same MDL
        // can be used for each partial transfer. This saves having to build
        // a new MDL for each partial transfer.
        //

        newIrp->MdlAddress = Irp->MdlAddress;

        //
        // At this point there is no current stack. IoSetNextIrpStackLocation
        // will make the first stack location the current stack so that the
        // SRB address can be written there.
        //

        IoSetNextIrpStackLocation(newIrp);
        newIrpStack = IoGetCurrentIrpStackLocation(newIrp);

        newIrpStack->MajorFunction = currentIrpStack->MajorFunction;
        newIrpStack->Parameters.Read.Length = dataLength;
        newIrpStack->Parameters.Read.ByteOffset = startingOffset;
        newIrpStack->DeviceObject = Fdo;

        //
        // Build SRB and CDB.
        //

        ClassBuildRequest(Fdo, newIrp);

        if (nextIrpStack->Parameters.Scsi.Srb == NULL) {

            DebugPrint((1,"ClassSplitRequest: Can't allocate Srb\n"));

            //
            // If an Srb can't be allocated then the orginal request cannot
            // be executed.  If this is the first request then just fail the
            // orginal request; otherwise just return.  When the pending
            // requests complete, they will complete the original request.
            // In either case set the IRP status to failure.
            //

            IoFreeIrp(newIrp);
            newIrp = NULL;

            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;

            goto CleanupSplitRequest;

        }

        //
        // Adjust SRB for this partial transfer.
        //

        newIrpStack = IoGetNextIrpStackLocation(newIrp);

        srb = newIrpStack->Parameters.Others.Argument1;
        srb->DataBuffer = dataBuffer;

        //
        // Write original IRP address to new IRP.
        //

        newIrp->AssociatedIrp.MasterIrp = Irp;

        //
        // Set the completion routine to ClassIoCompleteAssociated.
        //

        IoSetCompletionRoutine(newIrp,
                               ClassIoCompleteAssociated,
                               srb,
                               TRUE,
                               TRUE,
                               TRUE);

        //
        // Call port driver with new request.
        //

        IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, newIrp);

        //
        // Set up for next request.
        //

        dataBuffer = (PCHAR)dataBuffer + MaximumBytes;

        transferByteCount -= MaximumBytes;

        if (transferByteCount > MaximumBytes) {

            dataLength = MaximumBytes;

        } else {

            dataLength = transferByteCount;
        }

        //
        // Adjust disk byte offset.
        //

        startingOffset.QuadPart = startingOffset.QuadPart + MaximumBytes;
    }

    return;

CleanupSplitRequest:

    //
    // must decrement the irpcount ourselves to account for
    // unallocate irps, potentially completing the request here.
    // irpCount should always be greater than zero, which would imply
    // that all irps have already been allocated and sent.
    //

    ASSERT(irpCount > 0);
    ASSERT((LONG_PTR)nextIrpStack->Parameters.Others.Argument1 > 0);
    ASSERT((LONG_PTR)nextIrpStack->Parameters.Others.Argument1 >= irpCount);

    //
    // decrement the count of outstanding irps (Argument1) by the number of
    // irps that were not sent yet (irpCount).  If the number of remaining
    // irps at the time of this call was equal to the number not yet sent,
    // then either none of the irps were sent or all the irps that were
    // sent are already completed (and hence reduced this count themselves).
    //

    i = InterlockedExchangeAdd((PLONG)&nextIrpStack->Parameters.Others.Argument1,
                               -(irpCount)
                               );

    //
    // if we've decremented the final irpCount, then we must complete this
    // request here.
    //

    if (i == irpCount) {

        DebugPrint((2, "ClassSplitRequest: All partial IRPs completed for "
                    "IRP %p\n", Irp));
        ASSERT(nextIrpStack->Parameters.Others.Argument1 == 0);

        if (fdoExtension->CommonExtension.DriverExtension->InitData.ClassStartIo) {

            //
            // Acquire a separate copy of the remove lock so the debugging code
            // works okay and we don't have to hold up the completion of this
            // irp until after we start the next packet(s).
            //

            KIRQL oldIrql;
            UCHAR uniqueAddress;

            ClassAcquireRemoveLock(Fdo, (PIRP)&uniqueAddress);
            ClassReleaseRemoveLock(Fdo, Irp);
            ClassCompleteRequest(Fdo, Irp, IO_DISK_INCREMENT);

            KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
            IoStartNextPacket(Fdo, FALSE);
            KeLowerIrql(oldIrql);

            ClassReleaseRemoveLock(Fdo, (PIRP)&uniqueAddress);

        } else {

            ClassReleaseRemoveLock(Fdo, Irp);
            ClassCompleteRequest(Fdo, Irp, IO_NO_INCREMENT);

        }
    }

    return;

} // end ClassSplitRequest()


NTSTATUS
ClassIoComplete(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine executes when the port driver has completed a request.
    It looks at the SRB status in the completing SRB and if not success
    it checks for valid request sense buffer information. If valid, the
    info is used to update status with more precise message of type of
    error. This routine deallocates the SRB.

    This routine should only be placed on the stack location for a class
    driver FDO.

Arguments:

    Fdo - Supplies the device object which represents the logical
        unit.

    Irp - Supplies the Irp which has completed.

    Context - Supplies a pointer to the SRB.

Return Value:

    NT status

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = Context;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    NTSTATUS status;
    BOOLEAN retry;

    ASSERT(fdoExtension->CommonExtension.IsFdo);

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        ULONG retryInterval;

        DebugPrint((2,"ClassIoComplete: IRP %p, SRB %p\n", Irp, srb));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ClassReleaseQueue(Fdo);
        }

        retry = ClassInterpretSenseInfo(
                    Fdo,
                    srb,
                    irpStack->MajorFunction,
                    irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ?
                        irpStack->Parameters.DeviceIoControl.IoControlCode :
                        0,
                    MAXIMUM_RETRIES -
                        ((ULONG)(ULONG_PTR)irpStack->Parameters.Others.Argument4),
                    &status,
                    &retryInterval);

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            status = STATUS_IO_DEVICE_ERROR;
            retry = TRUE;
        }

        if (retry && ((ULONG)(ULONG_PTR)irpStack->Parameters.Others.Argument4)--) {

            //
            // Retry request.
            //

            DebugPrint((1, "Retry request %p\n", Irp));
            RetryRequest(Fdo, Irp, srb, FALSE);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;

    } // end if (SRB_STATUS(srb->SrbStatus) ...

    //
    // ensure we have returned some info, and it matches what the
    // original request wanted for PAGING operations only
    //

    if ((NT_SUCCESS(status)) && (Irp->Flags & IRP_PAGING_IO)) {
        ASSERT(Irp->IoStatus.Information != 0);
        ASSERT(irpStack->Parameters.Read.Length == Irp->IoStatus.Information);
    }

    //
    // Free the srb
    //

    if(TEST_FLAG(srb->SrbFlags, SRB_CLASS_FLAGS_PERSISTANT) == 0) {
        ClasspFreeSrb(fdoExtension, srb);
    } else {
        DebugPrint((2, "ClassIoComplete: Not Freeing Srb @ %p because "
                    "SRB_CLASS_FLAGS_PERSISTANT set\n", srb));
    }

    //
    // Set status in completing IRP.
    //

    Irp->IoStatus.Status = status;

    //
    // Set the hard error if necessary.
    //

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        //
        // Store DeviceObject for filesystem, and clear
        // in IoStatus.Information field.
        //

        IoSetHardErrorOrVerifyDevice(Irp, Fdo);
        Irp->IoStatus.Information = 0;
    }

    //
    // If pending has be returned for this irp then mark the current stack as
    // pending.
    //

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    if (fdoExtension->CommonExtension.DriverExtension->InitData.ClassStartIo) {
        if (irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) {
            KIRQL oldIrql;
            KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
            IoStartNextPacket(Fdo, FALSE);
            KeLowerIrql(oldIrql);
        }
    }

    ClassReleaseRemoveLock(Fdo, Irp);

    return status;

} // end ClassIoComplete()


NTSTATUS
ClassIoCompleteAssociated(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine executes when the port driver has completed a request.
    It looks at the SRB status in the completing SRB and if not success
    it checks for valid request sense buffer information. If valid, the
    info is used to update status with more precise message of type of
    error. This routine deallocates the SRB.  This routine is used for
    requests which were build by split request.  After it has processed
    the request it decrements the Irp count in the master Irp.  If the
    count goes to zero then the master Irp is completed.

Arguments:

    Fdo - Supplies the functional device object which represents the target.

    Irp - Supplies the Irp which has completed.

    Context - Supplies a pointer to the SRB.

Return Value:

    NT status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = Context;

    PIRP originalIrp = Irp->AssociatedIrp.MasterIrp;
    LONG irpCount;

    NTSTATUS status;
    BOOLEAN retry;

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        ULONG retryInterval;

        DebugPrint((2,"ClassIoCompleteAssociated: IRP %p, SRB %p", Irp, srb));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ClassReleaseQueue(Fdo);
        }

        retry = ClassInterpretSenseInfo(
                    Fdo,
                    srb,
                    irpStack->MajorFunction,
                    irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ?
                        irpStack->Parameters.DeviceIoControl.IoControlCode :
                        0,
                    MAXIMUM_RETRIES -
                        ((ULONG)(ULONG_PTR)irpStack->Parameters.Others.Argument4),
                    &status,
                    &retryInterval);

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            status = STATUS_IO_DEVICE_ERROR;
            retry = TRUE;
        }

        if (retry && ((ULONG)(ULONG_PTR)irpStack->Parameters.Others.Argument4)--) {

            //
            // Retry request. If the class driver has supplied a StartIo,
            // call it directly for retries.
            //

            DebugPrint((1, "Retry request %p\n", Irp));

            RetryRequest(Fdo, Irp, srb, TRUE);

            return STATUS_MORE_PROCESSING_REQUIRED;
        }

    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;

    } // end if (SRB_STATUS(srb->SrbStatus) ...

    //
    // Return SRB to list.
    //

    ClasspFreeSrb(fdoExtension, srb);

    //
    // Set status in completing IRP.
    //

    Irp->IoStatus.Status = status;

    DebugPrint((2, "ClassIoCompleteAssociated: Partial xfer IRP %p\n", Irp));

    //
    // Get next stack location. This original request is unused
    // except to keep track of the completing partial IRPs so the
    // stack location is valid.
    //

    irpStack = IoGetNextIrpStackLocation(originalIrp);

    //
    // Update status only if error so that if any partial transfer
    // completes with error, then the original IRP will return with
    // error. If any of the asynchronous partial transfer IRPs fail,
    // with an error then the original IRP will return 0 bytes transfered.
    // This is an optimization for successful transfers.
    //

    if (!NT_SUCCESS(status)) {

        originalIrp->IoStatus.Status = status;
        originalIrp->IoStatus.Information = 0;

        //
        // Set the hard error if necessary.
        //

        if (IoIsErrorUserInduced(status)) {

            //
            // Store DeviceObject for filesystem.
            //

            IoSetHardErrorOrVerifyDevice(originalIrp, Fdo);
        }
    }

    //
    // Decrement and get the count of remaining IRPs.
    //

    irpCount = InterlockedDecrement(
                    (PLONG)&irpStack->Parameters.Others.Argument1);

    DebugPrint((2, "ClassIoCompleteAssociated: Partial IRPs left %d\n",
                irpCount));

    //
    // BUGBUG - what old bug???
    // Old bug could cause irp count to negative
    //

    ASSERT(irpCount >= 0);

    if (irpCount == 0) {


        //
        // All partial IRPs have completed.
        //

        DebugPrint((2,
                 "ClassIoCompleteAssociated: All partial IRPs complete %p\n",
                 originalIrp));

        if (fdoExtension->CommonExtension.DriverExtension->InitData.ClassStartIo) {

            //
            // Acquire a separate copy of the remove lock so the debugging code
            // works okay and we don't have to hold up the completion of this
            // irp until after we start the next packet(s).
            //

            KIRQL oldIrql;
            UCHAR uniqueAddress;
            ClassAcquireRemoveLock(Fdo, (PIRP)&uniqueAddress);
            ClassReleaseRemoveLock(Fdo, originalIrp);
            ClassCompleteRequest(Fdo, originalIrp, IO_DISK_INCREMENT);

            KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
            IoStartNextPacket(Fdo, FALSE);
            KeLowerIrql(oldIrql);

            ClassReleaseRemoveLock(Fdo, (PIRP)&uniqueAddress);

        } else {

            //
            // just complete this request
            //

            ClassReleaseRemoveLock(Fdo, originalIrp);
            ClassCompleteRequest(Fdo, originalIrp, IO_DISK_INCREMENT);

        }

    }

    //
    // Deallocate IRP and indicate the I/O system should not attempt any more
    // processing.
    //

    IoFreeIrp(Irp);
    return STATUS_MORE_PROCESSING_REQUIRED;

} // end ClassIoCompleteAssociated()


NTSTATUS
ClassSendSrbSynchronous(
    PDEVICE_OBJECT Fdo,
    PSCSI_REQUEST_BLOCK Srb,
    PVOID BufferAddress,
    ULONG BufferLength,
    BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is called by SCSI device controls to complete an
    SRB and send it to the port driver synchronously (ie wait for
    completion). The CDB is already completed along with the SRB CDB
    size and request timeout value.

Arguments:

    Fdo - Supplies the functional device object which represents the target.

    Srb - Supplies a partially initialized SRB. The SRB cannot come from zone.

    BufferAddress - Supplies the address of the buffer.

    BufferLength - Supplies the length in bytes of the buffer.

    WriteToDevice - Indicates the data should be transfer to the device.

Return Value:

    Nt status indicating the final results of the operation.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    IO_STATUS_BLOCK ioStatus;
    ULONG controlType;
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT event;
    PUCHAR senseInfoBuffer;
    ULONG retryCount = MAXIMUM_RETRIES;
    NTSTATUS status;
    BOOLEAN retry;

    //
    // NOTE: While this code may look as though it could be pagable,
    //       making it pagable creates the possibility of a page
    //       boundary between IoCallDriver() and ClassReleaseQueue(),
    //       which could leave the queue frozen as we try to page in
    //       this code, which is required to unfreeze the queue.
    //       The result would be a nice case of deadlock.
    //

    ASSERT(fdoExtension->CommonExtension.IsFdo);

    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // NOTICE:  The SCSI-II specification indicates that this field should be
    // zero; however, some target controllers ignore the logical unit number
    // in the INDENTIFY message and only look at the logical unit number field
    // in the CDB.
    //

    // Srb->Cdb[1] |= deviceExtension->Lun << 5;

    //
    // Enable auto request sense.
    //

    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Sense buffer is in aligned nonpaged pool.
    //

    senseInfoBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                     SENSE_BUFFER_SIZE,
                                     '7CcS');

    if (senseInfoBuffer == NULL) {

        DebugPrint((1, "ClassSendSrbSynchronous: Can't allocate request sense "
                       "buffer\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    Srb->SenseInfoBuffer = senseInfoBuffer;
    Srb->DataBuffer = BufferAddress;

    if(BufferAddress != NULL) {
        if(WriteToDevice) {
            Srb->SrbFlags = SRB_FLAGS_DATA_OUT;
        } else {
            Srb->SrbFlags = SRB_FLAGS_DATA_IN;
        }
    } else {
        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }

    //
    // Start retries here.
    //

retry:

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build device I/O control request with METHOD_NEITHER data transfer.
    // We'll queue a completion routine to cleanup the MDL's and such ourself.
    //

    irp = IoAllocateIrp(
            (CCHAR) (fdoExtension->CommonExtension.LowerDeviceObject->StackSize + 1),
            FALSE);

    if(irp == NULL) {
        ExFreePool(senseInfoBuffer);
        DebugPrint((1, "ClassSendSrbSynchronous: Can't allocate Irp\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Get next stack location.
    //

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Set up SRB for execute scsi request. Save SRB address in next stack
    // for the port driver.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->Parameters.Scsi.Srb = Srb;

    IoSetCompletionRoutine(irp,
                           ClasspSendSynchronousCompletion,
                           Srb,
                           TRUE,
                           TRUE,
                           TRUE);

    irp->UserIosb = &ioStatus;
    irp->UserEvent = &event;

    if(BufferAddress) {
        //
        // Build an MDL for the data buffer and stick it into the irp.  The
        // completion routine will unlock the pages and free the MDL.
        //

        irp->MdlAddress = IoAllocateMdl( BufferAddress,
                                         BufferLength,
                                         FALSE,
                                         FALSE,
                                         irp );
        if (irp->MdlAddress == NULL) {
            ExFreePool(senseInfoBuffer);
            Srb->SenseInfoBuffer = NULL;
            IoFreeIrp( irp );
            DebugPrint((1, "ClassSendSrbSynchronous: Can't allocate MDL\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        try {
            MmProbeAndLockPages( irp->MdlAddress,
                                 KernelMode,
                                 (WriteToDevice ? IoReadAccess :
                                                  IoWriteAccess));

        } except(EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();

            ExFreePool(senseInfoBuffer);
            Srb->SenseInfoBuffer = NULL;
            IoFreeMdl(irp->MdlAddress);
            IoFreeIrp(irp);

            DebugPrint((1, "ClassSendSrbSynchronous: Exception %lx "
                           "locking buffer\n", status));
            return status;
        }
    }

    //
    // Disable synchronous transfer for these requests.
    //

    SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DISABLE_SYNCH_TRANSFER);

    //
    // Set the transfer length.
    //

    Srb->DataTransferLength = BufferLength;

    //
    // Zero out status.
    //

    Srb->ScsiStatus = Srb->SrbStatus = 0;
    Srb->NextSrb = 0;

    //
    // Set up IRP Address.
    //

    Srb->OriginalRequest = irp;

    //
    // Call the port driver with the request and wait for it to complete.
    //

    status = IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    //
    // Check that request completed without error.
    //

    if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        ULONG retryInterval;

        //
        // Release the queue if it is frozen.
        //

        if (Srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ClassReleaseQueue(Fdo);
        }

        //
        // Update status and determine if request should be retried.
        //

        retry = ClassInterpretSenseInfo(Fdo,
                                        Srb,
                                        IRP_MJ_SCSI,
                                        0,
                                        MAXIMUM_RETRIES  - retryCount,
                                        &status,
                                        &retryInterval);

        if (retry) {

            if ((status == STATUS_DEVICE_NOT_READY &&
                 ((PSENSE_DATA) senseInfoBuffer)->AdditionalSenseCode ==
                                SCSI_ADSENSE_LUN_NOT_READY) ||
                (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_SELECTION_TIMEOUT)) {

                LARGE_INTEGER delay;

                //
                // Delay for at least 2 seconds.
                //

                if(retryInterval < 2) {
                    retryInterval = 2;
                }

                delay.QuadPart = (LONGLONG)( - 10 * 1000 * (LONGLONG)1000 * retryInterval);

                //
                // Stall for a while to let the controller spinup.
                //

                KeDelayExecutionThread(KernelMode, FALSE, &delay);

            }

            //
            // If retries are not exhausted then retry this operation.
            //

            if (retryCount--) {
                goto retry;
            }
        }

    } else {

        status = STATUS_SUCCESS;
    }

    Srb->SenseInfoBuffer = NULL;
    ExFreePool(senseInfoBuffer);
    return status;

} // end ClassSendSrbSynchronous()


BOOLEAN
ClassInterpretSenseInfo(
    IN PDEVICE_OBJECT Fdo,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN UCHAR MajorFunctionCode,
    IN ULONG IoDeviceCode,
    IN ULONG RetryCount,
    OUT NTSTATUS *Status,
    OUT OPTIONAL ULONG *RetryInterval
    )

/*++

Routine Description:

    This routine interprets the data returned from the SCSI
    request sense. It determines the status to return in the
    IRP and whether this request can be retried.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Srb - Supplies the scsi request block which failed.

    MajorFunctionCode - Supplies the function code to be used for logging.

    IoDeviceCode - Supplies the device code to be used for logging.

    Status - Returns the status for the request.

Return Value:

    BOOLEAN TRUE: Drivers should retry this request.
            FALSE: Drivers should not retry this request.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PSENSE_DATA       senseBuffer = Srb->SenseInfoBuffer;

    BOOLEAN           retry = TRUE;
    BOOLEAN           logError = FALSE;
    BOOLEAN           saveCdb = FALSE;

    ULONG             badSector = 0;
    ULONG             uniqueId;

    NTSTATUS          logStatus;

    ULONG             readSector;
    ULONG             index;
    PIO_ERROR_LOG_PACKET errorLogEntry;

    ULONG             retryInterval = 0;

#if DBG
    ULONG             i;
#endif

    logStatus = -1;

    if(TEST_FLAG(Srb->SrbFlags, SRB_CLASS_FLAGS_PAGING)) {

        //
        // Log anything remotely incorrect about paging i/o
        //

        logError = TRUE;
        uniqueId = 301;
        logStatus = IO_WARNING_PAGING_FAILURE;
        saveCdb = TRUE;
    }

    //
    // Check that request sense buffer is valid.
    //

    ASSERT(fdoExtension->CommonExtension.IsFdo);

#if DBG
    DebugPrint((3, "Opcode %x\nParameters: ", Srb->Cdb[0]));
    for (i = 1; i < 12; i++) {
        DebugPrint((3,"%x ",Srb->Cdb[i]));
    }
    DebugPrint((3,"\n"));
#endif

    //
    // must handle the SRB_STATUS_INTERNAL_ERROR case first,
    // as it has  all the flags set.
    //

    if (Srb->SrbStatus == SRB_STATUS_INTERNAL_ERROR) {

        DebugPrint((1,"ClassInterpretSenseInfo: Internal Error code is %x\n",
                    Srb->InternalStatus));

        retry = FALSE;
        *Status = Srb->InternalStatus;

    } else if ((Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&
        (Srb->SenseInfoBufferLength >=
            offsetof(SENSE_DATA, CommandSpecificInformation))) {

        DebugPrint((2,"ClassInterpretSenseInfo: Error code is %x\n",
                    senseBuffer->ErrorCode));
        DebugPrint((2,"ClassInterpretSenseInfo: Sense key is %x\n",
                    senseBuffer->SenseKey));
        DebugPrint((2, "ClassInterpretSenseInfo: Additional sense code is %x\n",
                    senseBuffer->AdditionalSenseCode));
        DebugPrint((2, "ClassInterpretSenseInfo: Additional sense code qualifier is %x\n",
                  senseBuffer->AdditionalSenseCodeQualifier));

        //
        // Zero the additional sense code and additional sense code qualifier
        // if they were not returned by the device.
        //

        readSector = senseBuffer->AdditionalSenseLength +
            offsetof(SENSE_DATA, AdditionalSenseLength);

        if (readSector > Srb->SenseInfoBufferLength) {
            readSector = Srb->SenseInfoBufferLength;
        }

        if (readSector <= offsetof(SENSE_DATA, AdditionalSenseCode)) {
            senseBuffer->AdditionalSenseCode = 0;
        }

        if (readSector <= offsetof(SENSE_DATA, AdditionalSenseCodeQualifier)) {
            senseBuffer->AdditionalSenseCodeQualifier = 0;
        }

        switch (senseBuffer->SenseKey & 0xf) {

        case SCSI_SENSE_NOT_READY:

            DebugPrint((2,"ClassInterpretSenseInfo: Device not ready\n"));
            *Status = STATUS_DEVICE_NOT_READY;

            switch (senseBuffer->AdditionalSenseCode) {

            case SCSI_ADSENSE_LUN_NOT_READY:

                DebugPrint((2,"ClassInterpretSenseInfo: Lun not ready\n"));

                switch (senseBuffer->AdditionalSenseCodeQualifier) {

                case SCSI_SENSEQ_OPERATION_IN_PROGRESS:
                    DebugPrint((1, "ClassInterpretSenseInfo: "
                                "Operation In Progress\n"));
                    retryInterval = NOT_READY_RETRY_INTERVAL;
                    break;

                case SCSI_SENSEQ_BECOMING_READY:
                    DebugPrint((2, "ClassInterpretSenseInfo:"
                                " In process of becoming ready\n"));
                    retryInterval = NOT_READY_RETRY_INTERVAL;
                    break;

                case SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED:

                    DebugPrint((2, "ClassInterpretSenseInfo:"
                                " Manual intervention required\n"));
                    *Status = STATUS_NO_MEDIA_IN_DEVICE;
                    retry = FALSE;
                    break;

                case SCSI_SENSEQ_FORMAT_IN_PROGRESS:

                    DebugPrint((2, "ClassInterpretSenseInfo: Format in progress\n"));
                    retry = FALSE;
                    break;

                case SCSI_SENSEQ_CAUSE_NOT_REPORTABLE: {

                    if(!TEST_FLAG(fdoExtension->ScanForSpecialFlags,
                                 CLASS_SPECIAL_CAUSE_NOT_REPORTABLE_HACK)) {

                        DebugPrint((2, "ClassInterpretSenseInfo: "
                                       "not ready, cause unknown\n"));
                        /*
                        Many non-WHQL certified drives (mostly CD-RW) return
                        this when they have no media instead of the obvious
                        choice of:

                        SCSI_SENSE_NOT_READY/SCSI_ADSENSE_NO_MEDIA_IN_DEVICE

                        These drives should not pass WHQL certification due
                        to this discrepency.

                        if(fdoExtension->DiskGeometry.MediaType == RemovableMedia) {
                            retryInterval = NOT_READY_RETRY_INTERVAL;
                        }
                        */
                        retry = FALSE;
                        break;
                    } else {

                        //
                        // Treat this as init command required and fall through.
                        //
                    }
                }

                case SCSI_SENSEQ_INIT_COMMAND_REQUIRED:

                default:

                    DebugPrint((2, "ClassInterpretSenseInfo:"
                                " Initializing command required\n"));

                    //
                    // This sense code/additional sense code
                    // combination may indicate that the device
                    // needs to be started.  Send an start unit if this
                    // is a disk device.
                    //

                    if(TEST_FLAG(fdoExtension->DeviceFlags,
                                 DEV_SAFE_START_UNIT) &&
                        !TEST_FLAG(Srb->SrbFlags,
                                   SRB_CLASS_FLAGS_LOW_PRIORITY)) {
                        ClassSendStartUnit(Fdo);
                    }

                    break;

                } // end switch (senseBuffer->AdditionalSenseCodeQualifier)

                break;

            case SCSI_ADSENSE_NO_MEDIA_IN_DEVICE:

                DebugPrint((2, "ClassInterpretSenseInfo: No Media in "
                               "device.\n"));
                *Status = STATUS_NO_MEDIA_IN_DEVICE;
                retry = FALSE;

                //
                // signal autorun that there isn't any media in the device
                //

                ClassSetMediaChangeState(fdoExtension,
                                         MediaNotPresent,
                                         FALSE);

                break;
            } // end switch (senseBuffer->AdditionalSenseCode)

            break;

        case SCSI_SENSE_DATA_PROTECT:

            DebugPrint((2, "ClassInterpretSenseInfo: Media write protected\n"));
            *Status = STATUS_MEDIA_WRITE_PROTECTED;
            retry = FALSE;
            break;

        case SCSI_SENSE_MEDIUM_ERROR:

            DebugPrint((2,"ClassInterpretSenseInfo: Bad media\n"));
            *Status = STATUS_DEVICE_DATA_ERROR;

            retry = FALSE;
            logError = TRUE;
            uniqueId = 256;
            logStatus = IO_ERR_BAD_BLOCK;

            //
            // Check if this error is due to unknown format
            //
            if (((senseBuffer->AdditionalSenseCode) == 
                 SCSI_ADSENSE_INVALID_MEDIA) &&
                ((senseBuffer->AdditionalSenseCodeQualifier) ==
                 SCSI_SENSEQ_UNKNOWN_FORMAT)) {
               *Status = STATUS_UNRECOGNIZED_MEDIA;

               //
               // Log error only if this is a paging request
               //
               if(!TEST_FLAG(Srb->SrbFlags, SRB_CLASS_FLAGS_PAGING)) {
                  logError = FALSE;
               }
            }
            break;

        case SCSI_SENSE_HARDWARE_ERROR:

            DebugPrint((2,"ClassInterpretSenseInfo: Hardware error\n"));
            *Status = STATUS_IO_DEVICE_ERROR;

            logError = TRUE;
            uniqueId = 257;
            logStatus = IO_ERR_CONTROLLER_ERROR;

            break;

        case SCSI_SENSE_ILLEGAL_REQUEST:

            DebugPrint((2, "ClassInterpretSenseInfo: Illegal SCSI request\n"));
            *Status = STATUS_INVALID_DEVICE_REQUEST;

            switch (senseBuffer->AdditionalSenseCode) {

            case SCSI_ADSENSE_ILLEGAL_COMMAND:
                DebugPrint((2, "ClassInterpretSenseInfo: Illegal command\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_ILLEGAL_BLOCK: {
                DebugPrint((2, "ClassInterpretSenseInfo: Illegal block address\n"));
                *Status = STATUS_NONEXISTENT_SECTOR;
                retry = FALSE;
                break;
            }

            case SCSI_ADSENSE_INVALID_LUN:
                DebugPrint((2,"ClassInterpretSenseInfo: Invalid LUN\n"));
                *Status = STATUS_NO_SUCH_DEVICE;
                retry = FALSE;
                break;

            case SCSI_ADSENSE_MUSIC_AREA:
                DebugPrint((2,"ClassInterpretSenseInfo: Music area\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_DATA_AREA:
                DebugPrint((2,"ClassInterpretSenseInfo: Data area\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_VOLUME_OVERFLOW:
                DebugPrint((2, "ClassInterpretSenseInfo: Volume overflow\n"));
                retry = FALSE;
                break;

            case SCSI_ADSENSE_COPY_PROTECTION_FAILURE:
                DebugPrint((2, "ClassInterpretSenseInfo: Copy protection "
                            "failure\n"));
                retry = FALSE;

                *Status = STATUS_TOO_MANY_SECRETS;

                switch (senseBuffer->AdditionalSenseCodeQualifier) {
                    case SCSI_SENSEQ_AUTHENTICATION_FAILURE:
                        DebugPrint((2, "ClassInterpretSenseInfo: Authentication "
                                    "failure\n"));
                        break;
                    case SCSI_SENSEQ_KEY_NOT_PRESENT:
                        DebugPrint((2, "ClassInterpretSenseInfo: Key not "
                                    "present\n"));
                        break;
                    case SCSI_SENSEQ_KEY_NOT_ESTABLISHED:
                        DebugPrint((2, "ClassInterpretSenseInfo: Key not "
                                    "established\n"));
                        break;
                    case SCSI_SENSEQ_READ_OF_SCRAMBLED_SECTOR_WITHOUT_AUTHENTICATION:
                        DebugPrint((2, "ClassInterpretSenseInfo: Read of "
                                    "scrambled sector w/o authentication\n"));
                        break;
                    case SCSI_SENSEQ_MEDIA_CODE_MISMATCHED_TO_LOGICAL_UNIT:
                        DebugPrint((2, "ClassInterpretSenseInfo: Media region "
                                    "does not logical unit region\n"));
                        break;
                    case SCSI_SENSEQ_LOGICAL_UNIT_RESET_COUNT_ERROR:
                        DebugPrint((2, "ClassInterpretSenseInfo: Region set "
                                    "error -- region may be permanent\n"));
                        break;
                } // end switch of ASCQ for COPY_PROTECTION_FAILURE
                break;


            case SCSI_ADSENSE_INVALID_CDB:
                DebugPrint((2, "ClassInterpretSenseInfo: Invalid CDB\n"));

                //
                // Check if write cache enabled.
                //

                if (fdoExtension->DeviceFlags & DEV_WRITE_CACHE) {

                    //
                    // Assume FUA is not supported.
                    //

                    CLEAR_FLAG(fdoExtension->DeviceFlags, DEV_WRITE_CACHE);
                    retry = TRUE;

                } else {
                    retry = FALSE;
                }

                break;

            } // end switch (senseBuffer->AdditionalSenseCode)

            break;

        case SCSI_SENSE_UNIT_ATTENTION: {

            PVPB vpb;

            switch (senseBuffer->AdditionalSenseCode) {
            case SCSI_ADSENSE_MEDIUM_CHANGED:
                DebugPrint((2, "ClassInterpretSenseInfo: Media changed\n"));

                ClassSetMediaChangeState(fdoExtension, MediaPresent, FALSE);
                break;

            case SCSI_ADSENSE_BUS_RESET:
                DebugPrint((2,"ClassInterpretSenseInfo: Bus reset\n"));
                break;

            default:
                DebugPrint((2,"ClassInterpretSenseInfo: Unit attention\n"));
                break;

            } // end  switch (senseBuffer->AdditionalSenseCode)

            if ((Fdo->Characteristics & FILE_REMOVABLE_MEDIA) &&
                (ClassGetVpb(Fdo) != NULL) &&
                (ClassGetVpb(Fdo)->Flags & VPB_MOUNTED)) {


                //
                // Set bit to indicate that media may have changed
                // and volume needs verification.
                //

                SET_FLAG(Fdo->Flags, DO_VERIFY_VOLUME);

//                ASSERTMSG("P0 Invalidated - need to invalidate others too :", FALSE);

                //
                // run through the list of PDO's and invalidate each of
                // them
                //

                *Status = STATUS_VERIFY_REQUIRED;
                retry = FALSE;

            } else {

                *Status = STATUS_IO_DEVICE_ERROR;

            }

            //
            // A media change may have occured so increment the change
            // count for the physical device
            //

            fdoExtension->MediaChangeCount++;

            DebugPrint((2, "ClassInterpretSenseInfo - Media change "
                           "count for device %d incremented to %#lx\n",
                        fdoExtension->DeviceNumber,
                        fdoExtension->MediaChangeCount));

            break;
        }

        case SCSI_SENSE_ABORTED_COMMAND:

            DebugPrint((2,"ClassInterpretSenseInfo: Command aborted\n"));
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        case SCSI_SENSE_BLANK_CHECK:

            DebugPrint((2, "InterpretSenseInfo: Media blank check\n"));

            retry = FALSE;
            *Status = STATUS_NO_DATA_DETECTED;
            break;

        case SCSI_SENSE_RECOVERED_ERROR:

            DebugPrint((2,"ClassInterpretSenseInfo: Recovered error\n"));
            *Status = STATUS_SUCCESS;
            retry = FALSE;
            logError = TRUE;
            uniqueId = 258;

            switch(senseBuffer->AdditionalSenseCode) {
            case SCSI_ADSENSE_SEEK_ERROR:
            case SCSI_ADSENSE_TRACK_ERROR:
                logStatus = IO_ERR_SEEK_ERROR;
                break;

            case SCSI_ADSENSE_REC_DATA_NOECC:
            case SCSI_ADSENSE_REC_DATA_ECC:
                logStatus = IO_RECOVERED_VIA_ECC;
                break;

            case SCSI_FAILURE_PREDICTION_THRESHOLD_EXCEEDED: {
                UCHAR wmiEventData[5];

                *((PULONG)wmiEventData) = sizeof(UCHAR);
                wmiEventData[sizeof(ULONG)] = senseBuffer->AdditionalSenseCodeQualifier;

                ClassNotifyFailurePredicted(fdoExtension,
                                            (PUCHAR)&wmiEventData,
                                            sizeof(wmiEventData),
                                            FALSE,
                                            4,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun);

                //
                // Don't log another eventlog if we have already logged once
                //

                logError = (fdoExtension->FailurePredicted == FALSE);

                fdoExtension->FailurePredicted = TRUE;
                fdoExtension->FailureReason = senseBuffer->AdditionalSenseCodeQualifier;
                logStatus = IO_WRN_FAILURE_PREDICTED;
                break;
            }

            default:
                logStatus = IO_ERR_CONTROLLER_ERROR;
                break;

            } // end switch(senseBuffer->AdditionalSenseCode)

            if (senseBuffer->IncorrectLength) {

                DebugPrint((2, "ClassInterpretSenseInfo: Incorrect length detected.\n"));
                *Status = STATUS_INVALID_BLOCK_LENGTH ;
            }

            break;

        case SCSI_SENSE_NO_SENSE:

            //
            // Check other indicators.
            //

            if (senseBuffer->IncorrectLength) {

                DebugPrint((2, "ClassInterpretSenseInfo: Incorrect length detected.\n"));
                *Status = STATUS_INVALID_BLOCK_LENGTH ;
                retry   = FALSE;

            } else {

                DebugPrint((2, "ClassInterpretSenseInfo: No specific sense key\n"));
                *Status = STATUS_IO_DEVICE_ERROR;
                retry   = TRUE;
            }

            break;

        default:

            DebugPrint((2, "ClassInterpretSenseInfo: Unrecognized sense code\n"));
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        } // end switch (senseBuffer->SenseKey & 0xf)

        //
        // Try to determine the bad sector from the inquiry data.
        //

        if ((((PCDB)Srb->Cdb)->CDB10.OperationCode == SCSIOP_READ ||
            ((PCDB)Srb->Cdb)->CDB10.OperationCode == SCSIOP_VERIFY ||
            ((PCDB)Srb->Cdb)->CDB10.OperationCode == SCSIOP_WRITE)) {

            for (index = 0; index < 4; index++) {
                badSector = (badSector << 8) | senseBuffer->Information[index];
            }

            readSector = 0;
            for (index = 0; index < 4; index++) {
                readSector = (readSector << 8) | Srb->Cdb[index+2];
            }

            index = (((PCDB)Srb->Cdb)->CDB10.TransferBlocksMsb << 8) |
                ((PCDB)Srb->Cdb)->CDB10.TransferBlocksLsb;

            //
            // Make sure the bad sector is within the read sectors.
            //

            if (!(badSector >= readSector && badSector < readSector + index)) {
                badSector = readSector;
            }
        }

    } else {

        //
        // Request sense buffer not valid. No sense information
        // to pinpoint the error. Return general request fail.
        //

        DebugPrint((2,"ClassInterpretSenseInfo: Request sense info not valid. SrbStatus %2x\n",
                    SRB_STATUS(Srb->SrbStatus)));
        retry = TRUE;

        switch (SRB_STATUS(Srb->SrbStatus)) {
        case SRB_STATUS_INVALID_LUN:
        case SRB_STATUS_INVALID_TARGET_ID:
        case SRB_STATUS_NO_DEVICE:
        case SRB_STATUS_NO_HBA:
        case SRB_STATUS_INVALID_PATH_ID:
            *Status = STATUS_NO_SUCH_DEVICE;
            retry = FALSE;
            break;

        case SRB_STATUS_COMMAND_TIMEOUT:
        case SRB_STATUS_ABORTED:
        case SRB_STATUS_TIMEOUT:

            //
            // Update the error count for the device.
            //

            fdoExtension->ErrorCount++;
            *Status = STATUS_IO_TIMEOUT;
            break;

        case SRB_STATUS_SELECTION_TIMEOUT:
            logError = TRUE;
            logStatus = IO_ERR_NOT_READY;
            uniqueId = 260;
            *Status = STATUS_DEVICE_NOT_CONNECTED;
            retry = FALSE;
            break;

        case SRB_STATUS_DATA_OVERRUN:
            *Status = STATUS_DATA_OVERRUN;
            retry = FALSE;
            break;

        case SRB_STATUS_PHASE_SEQUENCE_FAILURE:

            //
            // Update the error count for the device.
            //

            fdoExtension->ErrorCount++;
            *Status = STATUS_IO_DEVICE_ERROR;

            //
            // If there was  phase sequence error then limit the number of
            // retries.
            //

            if (RetryCount > 1 ) {
                retry = FALSE;
            }

            break;

        case SRB_STATUS_REQUEST_FLUSHED:

            //
            // If the status needs verification bit is set.  Then set
            // the status to need verification and no retry; otherwise,
            // just retry the request.
            //

            if (TEST_FLAG(Fdo->Flags, DO_VERIFY_VOLUME)) {

                *Status = STATUS_VERIFY_REQUIRED;
                retry = FALSE;

            } else {
                *Status = STATUS_IO_DEVICE_ERROR;
            }

            break;

        case SRB_STATUS_INVALID_REQUEST:

            //
            // An invalid request was attempted.
            //

            *Status = STATUS_INVALID_DEVICE_REQUEST;
            retry = FALSE;
            break;

        case SRB_STATUS_UNEXPECTED_BUS_FREE:
        case SRB_STATUS_PARITY_ERROR:

            //
            // Update the error count for the device.
            //

            fdoExtension->ErrorCount++;

            //
            // Fall through to below.
            //

        case SRB_STATUS_BUS_RESET:
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        case SRB_STATUS_ERROR:

            *Status = STATUS_IO_DEVICE_ERROR;
            if (Srb->ScsiStatus == 0) {

                //
                // This is some strange return code.  Update the error
                // count for the device.
                //

                fdoExtension->ErrorCount++;

            } if (Srb->ScsiStatus == SCSISTAT_BUSY) {

                *Status = STATUS_DEVICE_NOT_READY;

            } if (Srb->ScsiStatus == SCSISTAT_RESERVATION_CONFLICT) {

                *Status = STATUS_DEVICE_BUSY;
                retry = FALSE;
                logError = FALSE;

            }

            break;

        default:
            logError = TRUE;
            logStatus = IO_ERR_CONTROLLER_ERROR;
            uniqueId = 259;
            *Status = STATUS_IO_DEVICE_ERROR;
            saveCdb = TRUE;
            break;

        }

        //
        // If the error count has exceeded the error limit, then disable
        // any tagged queuing, multiple requests per lu queueing
        // and sychronous data transfers.
        //

        if (fdoExtension->ErrorCount == 4) {

            //
            // Clearing the no queue freeze flag prevents the port driver
            // from sending multiple requests per logical unit.
            //

            CLEAR_FLAG(fdoExtension->SrbFlags, SRB_FLAGS_NO_QUEUE_FREEZE);
            CLEAR_FLAG(fdoExtension->SrbFlags, SRB_FLAGS_QUEUE_ACTION_ENABLE);

            SET_FLAG(fdoExtension->SrbFlags, SRB_FLAGS_DISABLE_SYNCH_TRANSFER);

            DebugPrint((1, "ClassInterpretSenseInfo: Too many errors; "
                           "disabling tagged queuing and synchronous data "
                           "tranfers.\n"));

        } else if (fdoExtension->ErrorCount == 8) {

            //
            // If a second threshold is reached, disable disconnects.
            //

            SET_FLAG(fdoExtension->SrbFlags, SRB_FLAGS_DISABLE_DISCONNECT);
            DebugPrint((1, "ClassInterpretSenseInfo: Too many errors; "
                           "disabling disconnects.\n"));
        }
    }

    //
    // If there is a class specific error handler call it.
    //

    if (fdoExtension->CommonExtension.DevInfo->ClassError != NULL) {

        fdoExtension->CommonExtension.DevInfo->ClassError(Fdo,
                                                          Srb,
                                                          Status,
                                                          &retry);
    }

    //
    // If the caller wants to know the suggested retry interval tell them.
    //

    if(ARGUMENT_PRESENT(RetryInterval)) {
        *RetryInterval = retryInterval;
    }

    //
    // Log an error if necessary.
    //

    if (logError) {

        errorLogEntry = (PIO_ERROR_LOG_PACKET)
            IoAllocateErrorLogEntry(
                Fdo,
                (UCHAR) (sizeof(IO_ERROR_LOG_PACKET) +
                        (5 * sizeof(ULONG)) +
                        Srb->CdbLength));

        if (errorLogEntry == NULL) {

            //
            // Return if no packet could be allocated.
            //

            return retry;

        }

        if (retry && RetryCount < MAXIMUM_RETRIES) {
            errorLogEntry->FinalStatus = STATUS_SUCCESS;
        } else {
            errorLogEntry->FinalStatus = *Status;
        }

        //
        // Calculate the device offset if there is a geometry.
        //

        errorLogEntry->DeviceOffset.QuadPart = (LONGLONG) badSector;
        errorLogEntry->DeviceOffset = RtlExtendedIntegerMultiply(
                           errorLogEntry->DeviceOffset,
                           fdoExtension->DiskGeometry.BytesPerSector);

        if(logStatus == -1) {
            errorLogEntry->ErrorCode = STATUS_IO_DEVICE_ERROR;
        } else {
            errorLogEntry->ErrorCode = logStatus;
        }

        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->IoControlCode = IoDeviceCode;
        errorLogEntry->RetryCount = (UCHAR) RetryCount;
        errorLogEntry->UniqueErrorValue = uniqueId;
        errorLogEntry->DumpData[0] = Srb->PathId;
        errorLogEntry->DumpData[1] = Srb->TargetId;
        errorLogEntry->DumpData[2] = Srb->Lun;
        errorLogEntry->DumpData[3] = Srb->Cdb[0];
        errorLogEntry->DumpData[4] = Srb->SrbStatus << 8 | Srb->ScsiStatus;

        if (!TEST_FLAG(Srb->SrbStatus, SRB_STATUS_AUTOSENSE_VALID) ||
            senseBuffer == NULL
            ) {

            errorLogEntry->DumpData[5] = 0x00000000;

        } else {

            errorLogEntry->DumpData[5] = senseBuffer->SenseKey << 16 |
                                     senseBuffer->AdditionalSenseCode << 8 |
                                     senseBuffer->AdditionalSenseCodeQualifier;
        }

        errorLogEntry->DumpDataSize = 6 * sizeof(ULONG);

        if(saveCdb) {
            //
            // Copy the entire CDB into the event.
            //

            RtlCopyMemory(&(errorLogEntry->DumpData[6]),
                          Srb->Cdb,
                          Srb->CdbLength);

            errorLogEntry->DumpDataSize += Srb->CdbLength;
        }

        //
        // Write the error log packet.
        //

        IoWriteErrorLogEntry(errorLogEntry);
    }

    return retry;

} // end ClassInterpretSenseInfo()


VOID
RetryRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PSCSI_REQUEST_BLOCK Srb,
    BOOLEAN Associated
    )

/*++

Routine Description:

    This routine reinitalizes the necessary fields, and sends the request
    to the lower driver.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Irp - Supplies the request to be retried.

    Srb - Supplies a Pointer to the SCSI request block to be retied.

    Assocaiated - Indicates this is an assocatied Irp created by split request.

Return Value:

    None

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);
    ULONG transferByteCount;

    //
    // Determine the transfer count of the request.  If this is a read or a
    // write then the transfer count is in the Irp stack.  Otherwise assume
    // the MDL contains the correct length.  If there is no MDL then the
    // transfer length must be zero.
    //

    if (currentIrpStack->MajorFunction == IRP_MJ_READ ||
        currentIrpStack->MajorFunction == IRP_MJ_WRITE) {

        transferByteCount = currentIrpStack->Parameters.Read.Length;

    } else if (Irp->MdlAddress != NULL) {

        //
        // Note this assumes that only read and write requests are spilt and
        // other request do not need to be.  If the data buffer address in
        // the MDL and the SRB don't match then transfer length is most
        // likely incorrect.
        //

        ASSERT(Srb->DataBuffer == MmGetMdlVirtualAddress(Irp->MdlAddress));
        transferByteCount = Irp->MdlAddress->ByteCount;

    } else {

        transferByteCount = 0;
    }

    //
    // Reset byte count of transfer in SRB Extension.
    //

    Srb->DataTransferLength = transferByteCount;

    //
    // Zero SRB statuses.
    //

    Srb->SrbStatus = Srb->ScsiStatus = 0;

    //
    // Set the no disconnect flag, disable synchronous data transfers and
    // disable tagged queuing. This fixes some errors.
    //

    SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DISABLE_DISCONNECT);
    SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DISABLE_SYNCH_TRANSFER);
    CLEAR_FLAG(Srb->SrbFlags, SRB_FLAGS_QUEUE_ACTION_ENABLE);

    Srb->QueueTag = SP_UNTAGGED;

    //
    // Set up major SCSI function.
    //

    nextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    nextIrpStack->Parameters.Scsi.Srb = Srb;

    //
    // Set up IoCompletion routine address.
    //

    if (Associated) {

        IoSetCompletionRoutine(Irp,
                               ClassIoCompleteAssociated,
                               Srb,
                               TRUE,
                               TRUE,
                               TRUE);

    } else {

        IoSetCompletionRoutine(Irp, ClassIoComplete, Srb, TRUE, TRUE, TRUE);
    }

    //
    // Pass the request to the port driver.
    //

    (VOID)IoCallDriver(commonExtension->LowerDeviceObject, Irp);

} // end RetryRequest()


NTSTATUS
ClassBuildRequest(
    PDEVICE_OBJECT Fdo,
    PIRP Irp
    )

/*++

Routine Description:

    This routine allocates and builds an Srb for a read or write request.
    The block address and length are supplied by the Irp. The retry count
    is stored in the current stack for use by ClassIoComplete which
    processes these requests when they complete.  The Irp is ready to be
    passed to the port driver when this routine returns.

Arguments:

    Fdo - Supplies the functional device object associated with this request.

    Irp - Supplies the request to be retried.

Note:

    If the IRP is for a disk transfer, the byteoffset field
    will already have been adjusted to make it relative to
    the beginning of the disk.


Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  nextIrpStack = IoGetNextIrpStackLocation(Irp);

    LARGE_INTEGER       startingOffset = currentIrpStack->Parameters.Read.ByteOffset;

    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    ULONG               logicalBlockAddress;
    USHORT              transferBlocks;

    //
    // Calculate relative sector address.
    //

    logicalBlockAddress =
        (ULONG)(Int64ShrlMod32(startingOffset.QuadPart, fdoExtension->SectorShift));

    //
    // Allocate an Srb.
    //

    srb = ClasspAllocateSrb(fdoExtension);

    if(srb == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up IRP Address.
    //

    srb->OriginalRequest = Irp;

    //
    // Set up target ID and logical unit number.
    //

    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb->DataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);

    //
    // Save byte count of transfer in SRB Extension.
    //

    srb->DataTransferLength = currentIrpStack->Parameters.Read.Length;

    //
    // Initialize the queue actions field.
    //

    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;

    //
    // Queue sort key is Relative Block Address.
    //

    srb->QueueSortKey = logicalBlockAddress;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    srb->SenseInfoBuffer = fdoExtension->SenseData;
    srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Set timeout value of one unit per 64k bytes of data.
    //

    srb->TimeOutValue = ((srb->DataTransferLength + 0xFFFF) >> 16) *
                        fdoExtension->TimeOutValue;

    //
    // Zero statuses.
    //

    srb->SrbStatus = srb->ScsiStatus = 0;
    srb->NextSrb = 0;

    //
    // Indicate that 10-byte CDB's will be used.
    //

    srb->CdbLength = 10;

    //
    // Fill in CDB fields.
    //

    cdb = (PCDB)srb->Cdb;

    transferBlocks = (USHORT)(currentIrpStack->Parameters.Read.Length >>
                              fdoExtension->SectorShift);

    //
    // Move little endian values into CDB in big endian format.
    //

    cdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte3;
    cdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte2;
    cdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte1;
    cdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&logicalBlockAddress)->Byte0;

    cdb->CDB10.TransferBlocksMsb = ((PFOUR_BYTE)&transferBlocks)->Byte1;
    cdb->CDB10.TransferBlocksLsb = ((PFOUR_BYTE)&transferBlocks)->Byte0;

    //
    // Set transfer direction flag and Cdb command.
    //

    if (currentIrpStack->MajorFunction == IRP_MJ_READ) {

        DebugPrint((3, "ClassBuildRequest: Read Command\n"));

        SET_FLAG(srb->SrbFlags, SRB_FLAGS_DATA_IN);
        cdb->CDB10.OperationCode = SCSIOP_READ;

    } else {

        DebugPrint((3, "ClassBuildRequest: Write Command\n"));

        SET_FLAG(srb->SrbFlags, SRB_FLAGS_DATA_OUT);
        cdb->CDB10.OperationCode = SCSIOP_WRITE;
    }

    //
    // If this is not a write-through request, then allow caching.
    //

    if (!(currentIrpStack->Flags & SL_WRITE_THROUGH)) {

        SET_FLAG(srb->SrbFlags, SRB_FLAGS_ADAPTER_CACHE_ENABLE);

    } else {

        //
        // If write caching is enable then force media access in the
        // cdb.
        //

        if (fdoExtension->DeviceFlags & DEV_WRITE_CACHE) {
            cdb->CDB10.ForceUnitAccess = TRUE;
        }
    }

    if(TEST_FLAG(Irp->Flags, (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO))) {
        SET_FLAG(srb->SrbFlags, SRB_CLASS_FLAGS_PAGING);
    }

    //
    // OR in the default flags from the device object.
    //

    SET_FLAG(srb->SrbFlags, fdoExtension->SrbFlags);

    //
    // Set up major SCSI function.
    //

    nextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    nextIrpStack->Parameters.Scsi.Srb = srb;

    //
    // Save retry count in current IRP stack.
    //

    currentIrpStack->Parameters.Others.Argument4 = (PVOID)MAXIMUM_RETRIES;

    //
    // Set up IoCompletion routine address.
    //

    IoSetCompletionRoutine(Irp, ClassIoComplete, srb, TRUE, TRUE, TRUE);

    return STATUS_SUCCESS;

} // end ClassBuildRequest()


ULONG
ClassModeSense(
    IN PDEVICE_OBJECT Fdo,
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    )

/*++

Routine Description:

    This routine sends a mode sense command to a target ID and returns
    when it is complete.

Arguments:

    Fdo - Supplies the functional device object associated with this request.

    ModeSenseBuffer - Supplies a buffer to store the sense data.

    Length - Supplies the length in bytes of the mode sense buffer.

    PageMode - Supplies the page or pages of mode sense data to be retrived.

Return Value:

    Length of the transferred data is returned.

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCDB cdb;
    SCSI_REQUEST_BLOCK srb;
    ULONG retries = 1;
    NTSTATUS status;

    PAGED_CODE();

    ASSERT(fdoExtension->CommonExtension.IsFdo);

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Build the MODE SENSE CDB.
    //

    srb.CdbLength = 6;
    cdb = (PCDB)srb.Cdb;

    //
    // Set timeout value from device extension.
    //

    srb.TimeOutValue = fdoExtension->TimeOutValue;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = PageMode;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)Length;

Retry:

    status = ClassSendSrbSynchronous(Fdo,
                                     &srb,
                                     ModeSenseBuffer,
                                     Length,
                                     FALSE);


    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // Routine ClassSendSrbSynchronous does not retry requests returned with
        // this status. MODE SENSE commands should be retried anyway.
        //

        if (retries--) {

            //
            // Retry request.
            //

            goto Retry;
        }

    } else if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {
        return(srb.DataTransferLength);
    } else {
        return(0);
    }

} // end ClassModeSense()


PVOID
ClassFindModePage(
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode,
    IN BOOLEAN Use6Byte
    )

/*++

Routine Description:

    This routine scans through the mode sense data and finds the requested
    mode sense page code.

Arguments:
    ModeSenseBuffer - Supplies a pointer to the mode sense data.

    Length - Indicates the length of valid data.

    PageMode - Supplies the page mode to be searched for.

    Use6Byte - Indicates whether 6 or 10 byte mode sense was used.

Return Value:

    A pointer to the the requested mode page.  If the mode page was not found
    then NULL is return.

--*/
{
    PMODE_PARAMETER_HEADER10 modeParam10;
    PUCHAR limit;
    ULONG  parameterHeaderLength;
    ULONG len;

    limit = ModeSenseBuffer + Length;
    parameterHeaderLength = (Use6Byte) ? sizeof(MODE_PARAMETER_HEADER) : sizeof(MODE_PARAMETER_HEADER10);


    //
    // Skip the mode select header and block descriptors.
    //

    if (Length < parameterHeaderLength) {
        return(NULL);
    }

    if (Use6Byte)
    {
        len = ((PMODE_PARAMETER_HEADER) ModeSenseBuffer)->BlockDescriptorLength;
    } else {
        modeParam10 = (PMODE_PARAMETER_HEADER10) ModeSenseBuffer;
        len = modeParam10->BlockDescriptorLength[1];
    }

    ModeSenseBuffer += parameterHeaderLength + len;

    //
    // ModeSenseBuffer now points at pages.  Walk the pages looking for the
    // requested page until the limit is reached.
    //

    while (ModeSenseBuffer < limit) {

        if (((PMODE_DISCONNECT_PAGE) ModeSenseBuffer)->PageCode == PageMode) {
            return(ModeSenseBuffer);
        }

        //
        // Advance to the next page.
        //

        ModeSenseBuffer += ((PMODE_DISCONNECT_PAGE) ModeSenseBuffer)->PageLength + 2;
    }

    return(NULL);
}

NTSTATUS
ClassSendSrbAsynchronous(
        PDEVICE_OBJECT Fdo,
        PSCSI_REQUEST_BLOCK Srb,
        PIRP Irp,
        PVOID BufferAddress,
        ULONG BufferLength,
        BOOLEAN WriteToDevice
        )
/*++

Routine Description:

    This routine takes a partially built Srb and an Irp and sends it down to
    the port driver.

    This routine must be called with the remove lock held for the specified
    Irp.

Arguments:

    Fdo - Supplies the functional device object for the orginal request.

    Srb - Supplies a paritally build ScsiRequestBlock.  In particular, the
        CDB and the SRB timeout value must be filled in.  The SRB must not be
        allocated from zone.

    Irp - Supplies the requesting Irp.

    BufferAddress - Supplies a pointer to the buffer to be transfered.

    BufferLength - Supplies the length of data transfer.

    WriteToDevice - Indicates the data transfer will be from system memory to
        device.

Return Value:

    Returns STATUS_PENDING if the request is dispatched (since the
    completion routine may change the irp's status value we cannot simply
    return the value of the dispatch)

    or returns a status value to indicate why it failed.

--*/
{

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PIO_STACK_LOCATION irpStack;

    ULONG savedFlags;

    PAGED_CODE();

    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // This is a violation of the SCSI spec but it is required for
    // some targets.
    //

    // Srb->Cdb[1] |= deviceExtension->Lun << 5;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    Srb->SenseInfoBuffer = fdoExtension->SenseData;
    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;
    Srb->DataBuffer = BufferAddress;

    //
    // Save the class driver specific flags away.
    //

    savedFlags = Srb->SrbFlags & SRB_FLAGS_CLASS_DRIVER_RESERVED;

    if (BufferAddress != NULL) {

        //
        // Build Mdl if necessary.
        //

        if (Irp->MdlAddress == NULL) {

            if (IoAllocateMdl(BufferAddress,
                              BufferLength,
                              FALSE,
                              FALSE,
                              Irp) == NULL) {

                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                //
                // ClassIoComplete() would have free'd the srb
                //

                ClasspFreeSrb(fdoExtension, Srb);
                ClassReleaseRemoveLock(Fdo, Irp);
                ClassCompleteRequest(Fdo, Irp, IO_NO_INCREMENT);

                return STATUS_INSUFFICIENT_RESOURCES;
            }

            MmBuildMdlForNonPagedPool(Irp->MdlAddress);

        } else {

            //
            // Make sure the buffer requested matches the MDL.
            //

            ASSERT(BufferAddress == MmGetMdlVirtualAddress(Irp->MdlAddress));
        }

        //
        // Set read flag.
        //

        Srb->SrbFlags = WriteToDevice ? SRB_FLAGS_DATA_OUT : SRB_FLAGS_DATA_IN;

    } else {

        //
        // Clear flags.
        //

        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }

    //
    // Restore saved flags.
    //

    SET_FLAG(Srb->SrbFlags, savedFlags);

    //
    // Disable synchronous transfer for these requests.
    //

    SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DISABLE_SYNCH_TRANSFER);

    //
    // Set the transfer length.
    //

    Srb->DataTransferLength = BufferLength;

    //
    // Zero out status.
    //

    Srb->ScsiStatus = Srb->SrbStatus = 0;

    Srb->NextSrb = 0;

    //
    // Save a few parameters in the current stack location.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // Save retry count in current Irp stack.
    //

    irpStack->Parameters.Others.Argument4 = (PVOID)MAXIMUM_RETRIES;

    //
    // Set up IoCompletion routine address.
    //

    IoSetCompletionRoutine(Irp, ClassIoComplete, Srb, TRUE, TRUE, TRUE);

    //
    // Get next stack location and
    // set major function code.
    //

    irpStack = IoGetNextIrpStackLocation(Irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = Srb;

    //
    // Set up Irp Address.
    //

    Srb->OriginalRequest = Irp;

    //
    // Call the port driver to process the request.
    //

    IoMarkIrpPending(Irp);

    IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, Irp);

    return STATUS_PENDING;

}


NTSTATUS
ClassDeviceControlDispatch(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The routine is the common class driver device control dispatch entry point.
    This routine is invokes the device-specific drivers DeviceControl routine,
    (which may call the Class driver's common DeviceControl routine).

Arguments:

    DeviceObject - Supplies a pointer to the device object for this request.

    Irp - Supplies the Irp making the request.

Return Value:

   Returns the status returned from the device-specific driver.

--*/

{

    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    ULONG isRemoved;

    isRemoved = ClassAcquireRemoveLock(DeviceObject, Irp);

    if(isRemoved) {

        ClassReleaseRemoveLock(DeviceObject, Irp);

        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Call the class specific driver DeviceControl routine.
    // If it doesn't handle it, it will call back into ClassDeviceControl.
    //

    ASSERT(commonExtension->DevInfo->ClassDeviceControl);

    return commonExtension->DevInfo->ClassDeviceControl(DeviceObject,Irp);
}


NTSTATUS
ClassDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
/*++

Routine Description:

    The routine is the common class driver device control dispatch function.
    This routine is called by a class driver when it get an unrecognized
    device control request.  This routine will perform the correct action for
    common requests such as lock media.  If the device request is unknown it
    passed down to the next level.

    This routine must be called with the remove lock held for the specified
    irp.

Arguments:

    DeviceObject - Supplies a pointer to the device object for this request.

    Irp - Supplies the Irp making the request.

Return Value:

   Returns back a STATUS_PENDING or a completion status.

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack = NULL;

    ULONG controlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    PSCSI_REQUEST_BLOCK srb = NULL;
    PCDB cdb = NULL;

    NTSTATUS status;
    ULONG modifiedIoControlCode;

    //
    // If this is a pass through I/O control, set the minor function code
    // and device address and pass it to the port driver.
    //

    if ((controlCode == IOCTL_SCSI_PASS_THROUGH) ||
        (controlCode == IOCTL_SCSI_PASS_THROUGH_DIRECT)) {

        PSCSI_PASS_THROUGH scsiPass;

        //
        // Validiate the user buffer.
        //

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(SCSI_PASS_THROUGH)) {

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

            status = STATUS_INVALID_PARAMETER;
            goto SetStatusAndReturn;
        }

        IoCopyCurrentIrpStackLocationToNext(Irp);

        nextStack = IoGetNextIrpStackLocation(Irp);
        nextStack->MinorFunction = 1;

        ClassReleaseRemoveLock(DeviceObject, Irp);

        status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
        goto SetStatusAndReturn;
    }

    Irp->IoStatus.Information = 0;

    switch (controlCode) {

        case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID: {

            PMOUNTDEV_UNIQUE_ID uniqueId;

            if (!commonExtension->MountedDeviceInterfaceName.Buffer) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(MOUNTDEV_UNIQUE_ID)) {

                status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID);
                break;
            }

            uniqueId = Irp->AssociatedIrp.SystemBuffer;
            uniqueId->UniqueIdLength =
                    commonExtension->MountedDeviceInterfaceName.Length;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(USHORT) + uniqueId->UniqueIdLength) {

                status = STATUS_BUFFER_OVERFLOW;
                Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID);
                break;
            }

            RtlCopyMemory(uniqueId->UniqueId,
                          commonExtension->MountedDeviceInterfaceName.Buffer,
                          uniqueId->UniqueIdLength);

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(USHORT) +
                                        uniqueId->UniqueIdLength;
            break;
        }

        case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME: {

            PMOUNTDEV_NAME name;

            ASSERT(commonExtension->DeviceName.Buffer);

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(MOUNTDEV_NAME)) {

                status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
                break;
            }

            name = Irp->AssociatedIrp.SystemBuffer;
            name->NameLength = commonExtension->DeviceName.Length;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(USHORT) + name->NameLength) {

                status = STATUS_BUFFER_OVERFLOW;
                Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
                break;
            }

            RtlCopyMemory(name->Name, commonExtension->DeviceName.Buffer,
                          name->NameLength);

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(USHORT) + name->NameLength;
            break;
        }

        case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME: {

            PMOUNTDEV_SUGGESTED_LINK_NAME suggestedName;
            WCHAR driveLetterNameBuffer[10];
            RTL_QUERY_REGISTRY_TABLE queryTable[2];
            PWSTR valueName;
            UNICODE_STRING driveLetterName;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(MOUNTDEV_SUGGESTED_LINK_NAME)) {

                status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = sizeof(MOUNTDEV_SUGGESTED_LINK_NAME);
                break;
            }

            valueName = ExAllocatePoolWithTag(
                            PagedPool,
                            commonExtension->DeviceName.Length + sizeof(WCHAR),
                            '8CcS');

            if (!valueName) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            RtlCopyMemory(valueName, commonExtension->DeviceName.Buffer,
                          commonExtension->DeviceName.Length);
            valueName[commonExtension->DeviceName.Length/sizeof(WCHAR)] = 0;

            driveLetterName.Buffer = driveLetterNameBuffer;
            driveLetterName.MaximumLength = 20;
            driveLetterName.Length = 0;

            RtlZeroMemory(queryTable, 2*sizeof(RTL_QUERY_REGISTRY_TABLE));
            queryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                                  RTL_QUERY_REGISTRY_DIRECT;
            queryTable[0].Name = valueName;
            queryTable[0].EntryContext = &driveLetterName;

            status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                            L"\\Registry\\Machine\\System\\DISK",
                                            queryTable, NULL, NULL);

#if defined (_X86_)
            if (IsNEC_98) {
                if (!NT_SUCCESS(status)) {
                    IoCopyCurrentIrpStackLocationToNext(Irp);
                    ClassReleaseRemoveLock(DeviceObject, Irp);
                    status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
                    ExFreePool(valueName);
                    goto SetStatusAndReturn;
                }
            }
#endif

            if (!NT_SUCCESS(status)) {
                ExFreePool(valueName);
                break;
            }

            if (driveLetterName.Length == 4 &&
                driveLetterName.Buffer[0] == '%' &&
                driveLetterName.Buffer[1] == ':') {

                driveLetterName.Buffer[0] = 0xFF;

            } else if (driveLetterName.Length != 4 ||
                driveLetterName.Buffer[0] < FirstDriveLetter ||
                driveLetterName.Buffer[0] > LastDriveLetter ||
                driveLetterName.Buffer[1] != ':') {

                status = STATUS_NOT_FOUND;
                ExFreePool(valueName);
                break;
            }

            suggestedName = Irp->AssociatedIrp.SystemBuffer;
            suggestedName->UseOnlyIfThereAreNoOtherLinks = TRUE;
            suggestedName->NameLength = 28;

            Irp->IoStatus.Information =
                    FIELD_OFFSET(MOUNTDEV_SUGGESTED_LINK_NAME, Name) + 28;

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                Irp->IoStatus.Information) {

                Irp->IoStatus.Information =
                        sizeof(MOUNTDEV_SUGGESTED_LINK_NAME);
                status = STATUS_BUFFER_OVERFLOW;
                ExFreePool(valueName);
                break;
            }

            RtlDeleteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                   L"\\Registry\\Machine\\System\\DISK",
                                   valueName);

            ExFreePool(valueName);

            RtlCopyMemory(suggestedName->Name, L"\\DosDevices\\", 24);
            suggestedName->Name[12] = driveLetterName.Buffer[0];
            suggestedName->Name[13] = ':';

            //
            // NT_SUCCESS(status) based on RtlQueryRegistryValues
            //
            status = STATUS_SUCCESS;

            break;
        }

        default:
            status = STATUS_PENDING;
            break;
    }

    if (status != STATUS_PENDING) {
        ClassReleaseRemoveLock(DeviceObject, Irp);
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    if(commonExtension->IsFdo) {

        PULONG_PTR function;

        srb = ExAllocatePoolWithTag(NonPagedPool,
                             SCSI_REQUEST_BLOCK_SIZE +
                             (sizeof(ULONG_PTR) * 2),
                             '9CcS');

        if (srb == NULL) {

            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;
        }

        //
        // Write zeros to Srb.
        //

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

        cdb = (PCDB)srb->Cdb;

        //
        // Save the function code and the device object in the memory after
        // the SRB.
        //

        function = (PULONG_PTR) ((PSCSI_REQUEST_BLOCK) (srb + 1));
        *function = (ULONG_PTR) DeviceObject;
        function++;
        *function = (ULONG_PTR) controlCode;

    } else {
        srb = NULL;
    }

    //
    // Change the device type to storage for the switch statement, but only
    // if from a legacy device type
    //

    if (((controlCode & 0xffff0000) == (IOCTL_DISK_BASE  << 16)) ||
        ((controlCode & 0xffff0000) == (IOCTL_TAPE_BASE  << 16)) ||
        ((controlCode & 0xffff0000) == (IOCTL_CDROM_BASE << 16))
        ) {

        modifiedIoControlCode = (controlCode & ~0xffff0000);
        modifiedIoControlCode |= (IOCTL_STORAGE_BASE << 16);

    } else {

        modifiedIoControlCode = controlCode;

    }


    switch (modifiedIoControlCode) {

    case IOCTL_STORAGE_CHECK_VERIFY:
    case IOCTL_STORAGE_CHECK_VERIFY2: {

        PIRP irp2 = NULL;
        PIO_STACK_LOCATION newStack;

        PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = NULL;

        DebugPrint((1,"DeviceIoControl: Check verify\n"));

        //
        // If a buffer for a media change count was provided, make sure it's
        // big enough to hold the result
        //

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength) {

            //
            // If the buffer is too small to hold the media change count
            // then return an error to the caller
            //

            if(irpStack->Parameters.DeviceIoControl.OutputBufferLength <
               sizeof(ULONG)) {

                DebugPrint((3,"DeviceIoControl: media count "
                              "buffer too small\n"));

                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = 0;

                if(srb != NULL) {
                    ExFreePool(srb);
                }

                ClassReleaseRemoveLock(DeviceObject, Irp);
                ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

                status = STATUS_BUFFER_TOO_SMALL;
                goto SetStatusAndReturn;

            }
        }

        if(!commonExtension->IsFdo) {

            //
            // If this is a PDO then we should just forward the request down
            //

            IoCopyCurrentIrpStackLocationToNext(Irp);

            ClassReleaseRemoveLock(DeviceObject, Irp);

            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);

            goto SetStatusAndReturn;

        } else {

            fdoExtension = DeviceObject->DeviceExtension;

        }

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength) {

            //
            // The caller has provided a valid buffer.  Allocate an additional
            // irp and stick the CheckVerify completion routine on it.  We will
            // then send this down to the port driver instead of the irp the
            // caller sent in
            //

            DebugPrint((2,"DeviceIoControl: Check verify wants "
                          "media count\n"));

            //
            // Allocate a new irp to send the TestUnitReady to the port driver
            //

            irp2 = IoAllocateIrp((CCHAR) (DeviceObject->StackSize + 3), FALSE);

            if(irp2 == NULL) {
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                Irp->IoStatus.Information = 0;
                ExFreePool(srb);
                ClassReleaseRemoveLock(DeviceObject, Irp);
                ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto SetStatusAndReturn;

                break;
            }

            //
            // Make sure to acquire the lock for the new irp.
            //

            ClassAcquireRemoveLock(DeviceObject, irp2);

            irp2->Tail.Overlay.Thread = Irp->Tail.Overlay.Thread;
            IoSetNextIrpStackLocation(irp2);

            //
            // Set the top stack location and shove the master Irp into the
            // top location
            //

            newStack = IoGetCurrentIrpStackLocation(irp2);
            newStack->Parameters.Others.Argument1 = Irp;
            newStack->DeviceObject = DeviceObject;

            //
            // Stick the check verify completion routine onto the stack
            // and prepare the irp for the port driver
            //

            IoSetCompletionRoutine(irp2,
                                   ClassCheckVerifyComplete,
                                   NULL,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            IoSetNextIrpStackLocation(irp2);
            newStack = IoGetCurrentIrpStackLocation(irp2);
            newStack->DeviceObject = DeviceObject;
            newStack->MajorFunction = irpStack->MajorFunction;
            newStack->MinorFunction = irpStack->MinorFunction;

            //
            // Mark the master irp as pending - whether the lower level
            // driver completes it immediately or not this should allow it
            // to go all the way back up.
            //

            IoMarkIrpPending(Irp);

            Irp = irp2;

        }

        //
        // Test Unit Ready
        //

        srb->CdbLength = 6;
        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;

        //
        // If this was a CV2 then mark the request as low-priority so we don't
        // spin up the drive just to satisfy it.
        //

        if(controlCode == IOCTL_STORAGE_CHECK_VERIFY2) {
            SET_FLAG(srb->SrbFlags, SRB_CLASS_FLAGS_LOW_PRIORITY);
        }

        //
        // Since this routine will always hand the request to the
        // port driver if there isn't a data transfer to be done
        // we don't have to worry about completing the request here
        // on an error
        //

        //
        // This routine uses a completion routine so we don't want to release
        // the remove lock until then.
        //

        status = ClassSendSrbAsynchronous(DeviceObject,
                                          srb,
                                          Irp,
                                          NULL,
                                          0,
                                          FALSE);

        break;
    }

    case IOCTL_STORAGE_MEDIA_REMOVAL:
    case IOCTL_STORAGE_EJECTION_CONTROL: {

        PPREVENT_MEDIA_REMOVAL mediaRemoval = Irp->AssociatedIrp.SystemBuffer;

        DebugPrint((3, "DiskIoControl: ejection control\n"));

        if(srb) {
            ExFreePool(srb);
        }

        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
           sizeof(PREVENT_MEDIA_REMOVAL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            status = STATUS_BUFFER_TOO_SMALL;
            goto SetStatusAndReturn;
        }

        if(!commonExtension->IsFdo) {

            //
            // Just forward this down and return
            //

            IoCopyCurrentIrpStackLocationToNext(Irp);

            ClassReleaseRemoveLock(DeviceObject, Irp);
            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);

        } else {

            //
            // Call to the FDO - handle the ejection control.
            //

            status = ClasspEjectionControl(
                        DeviceObject->DeviceExtension,
                        Irp,
                        ((modifiedIoControlCode ==
                        IOCTL_STORAGE_EJECTION_CONTROL) ? SecureMediaLock :
                                                          SimpleMediaLock),
                        mediaRemoval->PreventMediaRemoval);

            Irp->IoStatus.Status = status;
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        }

        return status;
    }

    case IOCTL_STORAGE_MCN_CONTROL: {

        DebugPrint((3, "DiskIoControl: MCN control\n"));

        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
           sizeof(PREVENT_MEDIA_REMOVAL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;

            if(srb) {
                ExFreePool(srb);
            }

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            status = STATUS_BUFFER_TOO_SMALL;
            goto SetStatusAndReturn;
        }

        if(!commonExtension->IsFdo) {

            //
            // Just forward this down and return
            //

            IoCopyCurrentIrpStackLocationToNext(Irp);

            ClassReleaseRemoveLock(DeviceObject, Irp);
            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);

        } else {

            //
            // Call to the FDO - handle the ejection control.
            //

            status = ClasspMcnControl(DeviceObject->DeviceExtension,
                                      Irp,
                                      srb);
        }
        goto SetStatusAndReturn;
    }

    case IOCTL_STORAGE_RESERVE:
    case IOCTL_STORAGE_RELEASE: {

        //
        // Reserve logical unit.
        //

        PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = NULL;

        if(!commonExtension->IsFdo) {

            IoCopyCurrentIrpStackLocationToNext(Irp);

            ClassReleaseRemoveLock(DeviceObject, Irp);
            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
            goto SetStatusAndReturn;
        } else {
            fdoExtension = DeviceObject->DeviceExtension;
        }

        srb->CdbLength = 6;

        if(modifiedIoControlCode == IOCTL_STORAGE_RESERVE) {
            cdb->CDB6GENERIC.OperationCode = SCSIOP_RESERVE_UNIT;
        } else {
            cdb->CDB6GENERIC.OperationCode = SCSIOP_RELEASE_UNIT;
        }

        //
        // Set timeout value.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;

        status = ClassSendSrbAsynchronous(DeviceObject,
                                          srb,
                                          Irp,
                                          NULL,
                                          0,
                                          FALSE);

        break;
    }

    case IOCTL_STORAGE_EJECT_MEDIA:
    case IOCTL_STORAGE_LOAD_MEDIA:
    case IOCTL_STORAGE_LOAD_MEDIA2:{

        //
        // Eject media.
        //

        PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = NULL;

        if(!commonExtension->IsFdo) {

            IoCopyCurrentIrpStackLocationToNext(Irp);

            ClassReleaseRemoveLock(DeviceObject, Irp);

            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
            goto SetStatusAndReturn;
        } else {
            fdoExtension = DeviceObject->DeviceExtension;
        }

        if(commonExtension->PagingPathCount != 0) {

            DebugPrint((1, "ClassDeviceControl: call to eject paging device - "
                           "failure\n"));

            status = STATUS_FILES_OPEN;
            Irp->IoStatus.Status = status;

            Irp->IoStatus.Information = 0;

            if(srb) {
                ExFreePool(srb);
            }

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            goto SetStatusAndReturn;
        }

        //
        // Synchronize with ejection control and ejection cleanup code as
        // well as other eject/load requests.
        //

        KeWaitForSingleObject(&(fdoExtension->EjectSynchronizationEvent),
                              UserRequest,
                              UserMode,
                              FALSE,
                              NULL);

        if(fdoExtension->ProtectedLockCount != 0) {

            DebugPrint((1, "ClassDeviceControl: call to eject protected locked "
                           "device - failure\n"));

            status = STATUS_DEVICE_BUSY;
            Irp->IoStatus.Status = status;
            Irp->IoStatus.Information = 0;

            if(srb) {
                ExFreePool(srb);
            }

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

            KeSetEvent(&(fdoExtension->EjectSynchronizationEvent),
                       IO_NO_INCREMENT,
                       FALSE);

            goto SetStatusAndReturn;
        }

        srb->CdbLength = 6;

        cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
        cdb->START_STOP.LoadEject = 1;

        if(modifiedIoControlCode == IOCTL_STORAGE_EJECT_MEDIA) {
            cdb->START_STOP.Start = 0;
        } else {
            cdb->START_STOP.Start = 1;
        }

        //
        // Set timeout value.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;
        status = ClassSendSrbAsynchronous(DeviceObject,
                                              srb,
                                              Irp,
                                              NULL,
                                              0,
                                              FALSE);

        KeSetEvent(&(fdoExtension->EjectSynchronizationEvent),
                   IO_NO_INCREMENT,
                   FALSE);

        break;
    }

    case IOCTL_STORAGE_FIND_NEW_DEVICES: {

        if(srb) {
            ExFreePool(srb);
        }

        if(commonExtension->IsFdo) {

            IoInvalidateDeviceRelations(
                ((PFUNCTIONAL_DEVICE_EXTENSION) commonExtension)->LowerPdo,
                BusRelations);

            status = STATUS_SUCCESS;
            Irp->IoStatus.Status = status;

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

            return status;

        } else {

            IoCopyCurrentIrpStackLocationToNext(Irp);

            ClassReleaseRemoveLock(DeviceObject, Irp);
            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
        }
        break;
    }

    case IOCTL_STORAGE_GET_DEVICE_NUMBER: {

        if(srb) {
            ExFreePool(srb);
        }

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength >=
           sizeof(STORAGE_DEVICE_NUMBER)) {

            PSTORAGE_DEVICE_NUMBER deviceNumber =
                Irp->AssociatedIrp.SystemBuffer;
            PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
                commonExtension->PartitionZeroExtension;

            deviceNumber->DeviceType = fdoExtension->CommonExtension.DeviceObject->DeviceType;
            deviceNumber->DeviceNumber = fdoExtension->DeviceNumber;
            deviceNumber->PartitionNumber = commonExtension->PartitionNumber;

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(STORAGE_DEVICE_NUMBER);

        } else {
            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0L;
        }

        Irp->IoStatus.Status = status;
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

        break;
    }

    default: {

        DebugPrint((4, "IoDeviceControl: Unsupported device IOCTL %x for %p\n",
                    controlCode, DeviceObject));

        //
        // Pass the device control to the next driver.
        //

        if(srb) {
            ExFreePool(srb);
        }

        //
        // Copy the Irp stack parameters to the next stack location.
        //

        IoCopyCurrentIrpStackLocationToNext(Irp);

        ClassReleaseRemoveLock(DeviceObject, Irp);
        status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
        break;
    }

    } // end switch( ...

SetStatusAndReturn:

    return status;
}


NTSTATUS
ClassShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called for a shutdown and flush IRPs.  These are sent by the
    system before it actually shuts down or when the file system does a flush.
    If it exists, the device-specific driver's routine will be invoked. If there
    wasn't one specified, the Irp will be completed with an Invalid device request.

Arguments:

    DriverObject - Pointer to device object to being shutdown by system.

    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    ULONG isRemoved;

    NTSTATUS status;

    isRemoved = ClassAcquireRemoveLock(DeviceObject, Irp);

    if(isRemoved) {

        ClassReleaseRemoveLock(DeviceObject, Irp);

        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;

        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    if (commonExtension->DevInfo->ClassShutdownFlush) {

        //
        // Call the device-specific driver's routine.
        //

        return commonExtension->DevInfo->ClassShutdownFlush(DeviceObject, Irp);
    }

    //
    // Device-specific driver doesn't support this.
    //

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

    ClassReleaseRemoveLock(DeviceObject, Irp);
    ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

    return STATUS_INVALID_DEVICE_REQUEST;
}


NTSTATUS
ClassCreateDeviceObject(
    IN PDRIVER_OBJECT          DriverObject,
    IN PCCHAR                  ObjectNameBuffer,
    IN PDEVICE_OBJECT          LowerDevice,
    IN BOOLEAN                 IsFdo,
    IN OUT PDEVICE_OBJECT      *DeviceObject
    )

/*++

Routine Description:

    This routine creates an object for the physical device specified and
    sets up the deviceExtension's function pointers for each entry point
    in the device-specific driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    ObjectNameBuffer - Dir. name of the object to create.

    LowerDeviceObject - Pointer to the lower device object

    IsFdo - should this be an fdo or a pdo

    DeviceObject - Pointer to the device object pointer we will return.

Return Value:

    NTSTATUS

--*/

{
    BOOLEAN        isPartitionable;
    STRING         ntNameString;
    UNICODE_STRING ntUnicodeString;
    NTSTATUS       status, status2;
    PDEVICE_OBJECT deviceObject = NULL;

    ULONG          characteristics;

    PCLASS_DRIVER_EXTENSION
        driverExtension = IoGetDriverObjectExtension(DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);

    PCLASS_DEV_INFO devInfo;

    PAGED_CODE();

    *DeviceObject = NULL;
    RtlInitUnicodeString(&ntUnicodeString, NULL);

    DebugPrint((2, "ClassCreateFdo: Create device object\n"));

    ASSERT(LowerDevice);

    //
    // Make sure that if we're making PDO's we have an enumeration routine
    //

    isPartitionable = (driverExtension->InitData.ClassEnumerateDevice != NULL);

    ASSERT(IsFdo || isPartitionable);

    //
    // Grab the correct dev-info structure out of the init data
    //

    if(IsFdo) {
        devInfo = &(driverExtension->InitData.FdoData);
    } else {
        devInfo = &(driverExtension->InitData.PdoData);
    }

    characteristics = devInfo->DeviceCharacteristics;

    if(ARGUMENT_PRESENT(ObjectNameBuffer)) {
        DebugPrint((2, "ClassCreateFdo: Name is %s\n", ObjectNameBuffer));

        RtlInitString(&ntNameString, ObjectNameBuffer);

        status = RtlAnsiStringToUnicodeString(&ntUnicodeString, &ntNameString, TRUE);

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,
                        "ClassCreateFdo: Cannot convert string %s\n",
                        ObjectNameBuffer));

            ntUnicodeString.Buffer = NULL;
            return status;
        }
    } else {
        DebugPrint((2, "ClassCreateFdo: Object will be unnamed\n"));

        if(IsFdo == FALSE) {

            //
            // PDO's have to have some sort of name.
            //

            SET_FLAG(characteristics, FILE_AUTOGENERATED_DEVICE_NAME);
        }

        RtlInitUnicodeString(&ntUnicodeString, NULL);
    }

    status = IoCreateDevice(DriverObject,
                            devInfo->DeviceExtensionSize,
                            &ntUnicodeString,
                            devInfo->DeviceType,
                            devInfo->DeviceCharacteristics,
                            FALSE,
                            &deviceObject);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1, "ClassCreateFdo: Can not create device object %lx\n",
                    status));
        ASSERT(deviceObject == NULL);

        //
        // buffer is not used any longer here.
        //

        if (ntUnicodeString.Buffer != NULL) {
            DebugPrint((1, "ClassCreateFdo: Freeing unicode name buffer\n"));
            ExFreePool(ntUnicodeString.Buffer);
            RtlInitUnicodeString(&ntUnicodeString, NULL);
        }

    } else {

        PCOMMON_DEVICE_EXTENSION commonExtension = deviceObject->DeviceExtension;

        RtlZeroMemory(
            deviceObject->DeviceExtension,
            devInfo->DeviceExtensionSize);

        //
        // Setup version code
        //

        commonExtension->Version = 0x03;

        //
        // Setup the remove lock and event
        //

        commonExtension->IsRemoved = NO_REMOVE;
        commonExtension->RemoveLock = 0;
        KeInitializeEvent(&commonExtension->RemoveEvent,
                          SynchronizationEvent,
                          FALSE);

#if DBG

        KeInitializeSpinLock(&commonExtension->RemoveTrackingSpinlock);
        commonExtension->RemoveTrackingList = NULL;

#else

        commonExtension->RemoveTrackingSpinlock = (ULONG_PTR) -1;
        commonExtension->RemoveTrackingList = (PVOID) -1;
#endif

        //
        // Acquire the lock once.  This reference will be released when the
        // remove IRP has been received.
        //

        ClassAcquireRemoveLock(deviceObject, (PIRP) deviceObject);

        //
        // Store a pointer to the driver extension so we don't have to do
        // lookups to get it.
        //

        commonExtension->DriverExtension = driverExtension;

        //
        // Fill in entry points
        //

        commonExtension->DevInfo = devInfo;

        //
        // Initialize some of the common values in the structure
        //

        commonExtension->DeviceObject = deviceObject;

        commonExtension->LowerDeviceObject = NULL;

        if(IsFdo) {

            PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = (PVOID) commonExtension;

            commonExtension->PartitionZeroExtension = deviceObject->DeviceExtension;

            //
            // Set the initial device object flags.
            //

            SET_FLAG(deviceObject->Flags, DO_POWER_PAGABLE);

            //
            // Clear the PDO list
            //

            commonExtension->ChildList = NULL;

            commonExtension->DriverData =
                ((PFUNCTIONAL_DEVICE_EXTENSION) deviceObject->DeviceExtension + 1);

            if(isPartitionable) {

                commonExtension->PartitionNumber = 0;
            } else {
                commonExtension->PartitionNumber = (ULONG) (-1L);
            }

            fdoExtension->DevicePowerState = PowerDeviceD0;

            KeInitializeEvent(&fdoExtension->EjectSynchronizationEvent,
                              SynchronizationEvent,
                              TRUE);

            KeInitializeEvent(&fdoExtension->ChildLock,
                              SynchronizationEvent,
                              TRUE);

            status = ClasspAllocateReleaseRequest(deviceObject);

            if(!NT_SUCCESS(status)) {
                IoDeleteDevice(deviceObject);
                *DeviceObject = NULL;

                if (ntUnicodeString.Buffer != NULL) {
                    DebugPrint((1, "ClassCreateFdo: Freeing unicode name buffer\n"));
                    ExFreePool(ntUnicodeString.Buffer);
                    RtlInitUnicodeString(&ntUnicodeString, NULL);
                }

                return status;
            }

        } else {

            PPHYSICAL_DEVICE_EXTENSION pdoExtension =
                deviceObject->DeviceExtension;

            PFUNCTIONAL_DEVICE_EXTENSION p0Extension =
                LowerDevice->DeviceExtension;

            SET_FLAG(deviceObject->Flags, DO_POWER_PAGABLE);

            commonExtension->PartitionZeroExtension = p0Extension;

            //
            // Stick this onto the PDO list
            //

            ClassAddChild(p0Extension, pdoExtension, TRUE);

            commonExtension->DriverData = (PVOID) (pdoExtension + 1);

            //
            // Get the top of stack for the lower device - this allows
            // filters to get stuck in between the partitions and the
            // physical disk.
            //

            commonExtension->LowerDeviceObject =
                IoGetAttachedDeviceReference(LowerDevice);

            //
            // Pnp will keep a reference to the lower device object long
            // after this partition has been deleted.  Dereference now so
            // we don't have to deal with it later.
            //

            ObDereferenceObject(commonExtension->LowerDeviceObject);
        }

        KeInitializeEvent(&commonExtension->PathCountEvent, SynchronizationEvent, TRUE);

        commonExtension->IsFdo = IsFdo;

        commonExtension->DeviceName = ntUnicodeString;

        commonExtension->PreviousState = 0xff;

        InitializeDictionary(&(commonExtension->FileObjectDictionary));

        commonExtension->CurrentState = IRP_MN_STOP_DEVICE;
    }

    *DeviceObject = deviceObject;

    return status;
}


NTSTATUS
ClassClaimDevice(
    IN PDEVICE_OBJECT LowerDeviceObject,
    IN BOOLEAN Release
    )
/*++

Routine Description:

    This function claims a device in the port driver.  The port driver object
    is updated with the correct driver object if the device is successfully
    claimed.

Arguments:

    LowerDeviceObject - Supplies the base port device object.

    Release - Indicates the logical unit should be released rather than claimed.

Return Value:

    Returns a status indicating success or failure of the operation.

--*/

{
    IO_STATUS_BLOCK    ioStatus;
    PIRP               irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT             event;
    NTSTATUS           status;
    SCSI_REQUEST_BLOCK srb;

    PAGED_CODE();

    //
    // Clear the SRB fields.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Write length to SRB.
    //

    srb.Length = SCSI_REQUEST_BLOCK_SIZE;

    srb.Function = Release ? SRB_FUNCTION_RELEASE_DEVICE :
        SRB_FUNCTION_CLAIM_DEVICE;

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion
    //

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    //
    // Build synchronous request with no transfer.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_EXECUTE_NONE,
                                        LowerDeviceObject,
                                        NULL,
                                        0,
                                        NULL,
                                        0,
                                        TRUE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {
        DebugPrint((1, "ClassClaimDevice: Can't allocate Irp\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = &srb;

    //
    // Set up IRP Address.
    //

    srb.OriginalRequest = irp;

    //
    // Call the port driver with the request and wait for it to complete.
    //

    status = IoCallDriver(LowerDeviceObject, irp);
    if (status == STATUS_PENDING) {

        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    //
    // If this is a release request, then just decrement the reference count
    // and return.  The status does not matter.
    //

    if (Release) {

        // ObDereferenceObject(LowerDeviceObject);
        return STATUS_SUCCESS;
    }

    if (!NT_SUCCESS(status)) {
        return status;
    }

    ASSERT(srb.DataBuffer != NULL);

    return status;
}


NTSTATUS
ClassInternalIoControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine passes internal device controls to the port driver.
    Internal device controls are used by higher level drivers both for ioctls
    (unfortunately) and to pass through scsi requests.

    If the IoControlCode does not match any of the handled ioctls and is
    a valid system address then the request will be treated as an SRB and
    passed down to the lower driver.  If the IoControlCode is not a valid
    system address the ioctl will be failed.

Arguments:

    DeviceObject - Supplies a pointer to the device object for this request.

    Irp - Supplies the Irp making the request.

Return Value:

   Returns back a STATUS_PENDING or a completion status.

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack = IoGetNextIrpStackLocation(Irp);

    ULONG isRemoved;

    PSCSI_REQUEST_BLOCK srb;

    isRemoved = ClassAcquireRemoveLock(DeviceObject, Irp);

    if(isRemoved) {

        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;

        ClassReleaseRemoveLock(DeviceObject, Irp);

        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Get a pointer to the SRB.
    //

    srb = irpStack->Parameters.Scsi.Srb;

    //
    // Set the parameters in the next stack location.
    //

    if(commonExtension->IsFdo) {
        nextStack->Parameters.Scsi.Srb = srb;
        nextStack->MajorFunction = IRP_MJ_SCSI;
        nextStack->MinorFunction = IRP_MN_SCSI_CLASS;

    } else {

        IoCopyCurrentIrpStackLocationToNext(Irp);
    }

    ClassReleaseRemoveLock(DeviceObject, Irp);

    return IoCallDriver(commonExtension->LowerDeviceObject, Irp);
}


VOID
ClassDeleteSrbLookasideList(
    IN PCOMMON_DEVICE_EXTENSION CommonExtension
    )

{
    PAGED_CODE();

//#ifndef ALLOCATE_SRB_FROM_POOL
    if (CommonExtension->IsSrbLookasideListInitialized) {
        ExDeleteNPagedLookasideList(
            &(CommonExtension->SrbLookasideList));
        CommonExtension->IsSrbLookasideListInitialized = FALSE;
    }
//#endif
    return;
}


VOID
ClassInitializeSrbLookasideList(
    IN PCOMMON_DEVICE_EXTENSION CommonExtension,
    IN ULONG NumberElements
    )

/*++

Routine Description:

    This routine sets up a lookaside listhead for srbs.

Arguments:

    DeviceExtension - Pointer to the deviceExtension containing the listhead.

    NumberElements  - Supplies the maximum depth of the lookaside list.


Return Value:

    None

--*/

{
    PAGED_CODE();
//#ifndef ALLOCATE_SRB_FROM_POOL
    if(CommonExtension->IsSrbLookasideListInitialized == FALSE) {
        ExInitializeNPagedLookasideList(&CommonExtension->SrbLookasideList,
                                        NULL,
                                        NULL,
                                        NonPagedPoolMustSucceed,
                                        SCSI_REQUEST_BLOCK_SIZE,
                                        '$scS',
                                        (USHORT)NumberElements);
        CommonExtension->IsSrbLookasideListInitialized = TRUE;
    }
//#endif
    return;
}



ULONG
ClassQueryTimeOutRegistryValue(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine determines whether a reg key for a user-specified timeout
    value exists.

Arguments:

    DeviceObject - Pointer to the device object we are retrieving the timeout
                   value for

Return Value:

    New default timeout for a class of devices.

--*/

{
    //
    // Find the appropriate reg. key
    //

    PCLASS_DRIVER_EXTENSION
        driverExtension = IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);

    PUNICODE_STRING registryPath = &(driverExtension->RegistryPath);

    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;
    PWSTR path;
    NTSTATUS status;
    LONG     timeOut = 0;
    ULONG    zero = 0;
    ULONG    size;

    PAGED_CODE();

    if (!registryPath) {
        return 0;
    }

    parameters = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(RTL_QUERY_REGISTRY_TABLE)*2,
                                '1BcS');

    if (!parameters) {
        return 0;
    }

    size = registryPath->MaximumLength + sizeof(WCHAR);
    path = ExAllocatePoolWithTag(NonPagedPool, size, '2BcS');

    if (!path) {
        ExFreePool(parameters);
        return 0;
    }

    RtlZeroMemory(path,size);
    RtlCopyMemory(path, registryPath->Buffer, size - sizeof(WCHAR));


    //
    // Check for the Timeout value.
    //

    RtlZeroMemory(parameters,
                  (sizeof(RTL_QUERY_REGISTRY_TABLE)*2));

    parameters[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    parameters[0].Name          = L"TimeOutValue";
    parameters[0].EntryContext  = &timeOut;
    parameters[0].DefaultType   = REG_DWORD;
    parameters[0].DefaultData   = &zero;
    parameters[0].DefaultLength = sizeof(ULONG);

    status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                    path,
                                    parameters,
                                    NULL,
                                    NULL);

    if (!(NT_SUCCESS(status))) {
        timeOut = 0;
    }

    ExFreePool(parameters);
    ExFreePool(path);

    DebugPrint((2,
                "ClassQueryTimeOutRegistryValue: Timeout value %d\n",
                timeOut));


    return timeOut;

}


NTSTATUS
ClassCheckVerifyComplete(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine executes when the port driver has completed a check verify
    ioctl.  It will set the status of the master Irp, copy the media change
    count and complete the request.

Arguments:

    Fdo - Supplies the functional device object which represents the logical unit.

    Irp - Supplies the Irp which has completed.

    Context - NULL

Return Value:

    NT status

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PIRP originalIrp;

    ASSERT_FDO(Fdo);

    originalIrp = irpStack->Parameters.Others.Argument1;

    //
    // Copy the media change count and status
    //

    *((PULONG) (originalIrp->AssociatedIrp.SystemBuffer)) =
        fdoExtension->MediaChangeCount;

    DebugPrint((2, "ClassCheckVerifyComplete - Media change count for"
                   "device %d is %lx - saved as %lx\n",
                fdoExtension->DeviceNumber,
                fdoExtension->MediaChangeCount,
                *((PULONG) originalIrp->AssociatedIrp.SystemBuffer)));

    originalIrp->IoStatus.Status = Irp->IoStatus.Status;
    originalIrp->IoStatus.Information = sizeof(ULONG);

    ClassReleaseRemoveLock(Fdo, originalIrp);
    ClassCompleteRequest(Fdo, originalIrp, IO_DISK_INCREMENT);

    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;

}


NTSTATUS
ClassGetDescriptor(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSTORAGE_PROPERTY_ID PropertyId,
    OUT PSTORAGE_DESCRIPTOR_HEADER *Descriptor
    )

/*++

Routine Description:

    This routine will perform a query for the specified property id and will
    allocate a non-paged buffer to store the data in.  It is the responsibility
    of the caller to ensure that this buffer is freed.

    This routine must be run at IRQL_PASSIVE_LEVEL

Arguments:

    DeviceObject - the device to query
    DeviceInfo - a location to store a pointer to the buffer we allocate

Return Value:

    status
    if status is unsuccessful *DeviceInfo will be set to 0

--*/

{
    STORAGE_PROPERTY_QUERY query;
    IO_STATUS_BLOCK ioStatus;

    PSTORAGE_DESCRIPTOR_HEADER descriptor = NULL;
    ULONG length;

    UCHAR pass = 0;

    PAGED_CODE();

    //
    // Set the passed-in descriptor pointer to NULL as default
    //

    *Descriptor = NULL;


    RtlZeroMemory(&query, sizeof(STORAGE_PROPERTY_QUERY));
    query.PropertyId = *PropertyId;
    query.QueryType = PropertyStandardQuery;

    //
    // On the first pass we just want to get the first few
    // bytes of the descriptor so we can read it's size
    //

    descriptor = (PVOID)&query;

    ASSERT(sizeof(STORAGE_PROPERTY_QUERY) >= (sizeof(ULONG)*2));

    ClassSendDeviceIoControlSynchronous(
        IOCTL_STORAGE_QUERY_PROPERTY,
        DeviceObject,
        &query,
        sizeof(STORAGE_PROPERTY_QUERY),
        sizeof(ULONG) * 2,
        FALSE,
        &ioStatus
        );

    if(!NT_SUCCESS(ioStatus.Status)) {

        DebugPrint((1, "ClassGetDescriptor: error %lx trying to "
                       "query properties #1\n", ioStatus.Status));
        return ioStatus.Status;
    }

    if (descriptor->Size == 0) {

        DebugPrint((0, "ClassGetDescriptor: size returned was zero?! (status "
                    "%x\n", ioStatus.Status));
        return STATUS_UNSUCCESSFUL;

    }

    //
    // This time we know how much data there is so we can
    // allocate a buffer of the correct size
    //

    length = descriptor->Size;

    descriptor = ExAllocatePoolWithTag(NonPagedPool, length, '4BcS');

    if(descriptor == NULL) {

        DebugPrint((1, "ClassGetDescriptor: unable to memory for descriptor "
                    "(%d bytes)\n", length));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // setup the query again, as it was overwritten above
    //

    RtlZeroMemory(&query, sizeof(STORAGE_PROPERTY_QUERY));
    query.PropertyId = *PropertyId;
    query.QueryType = PropertyStandardQuery;

    //
    // copy the input to the new outputbuffer
    //

    RtlCopyMemory(descriptor,
                  &query,
                  sizeof(STORAGE_PROPERTY_QUERY)
                  );

    ClassSendDeviceIoControlSynchronous(
        IOCTL_STORAGE_QUERY_PROPERTY,
        DeviceObject,
        descriptor,
        sizeof(STORAGE_PROPERTY_QUERY),
        length,
        FALSE,
        &ioStatus
        );

    if(!NT_SUCCESS(ioStatus.Status)) {

        DebugPrint((1, "ClassGetDescriptor: error %lx trying to "
                       "query properties #1\n", ioStatus.Status));
        ExFreePool(descriptor);
        return ioStatus.Status;
    }

    //
    // return the memory we've allocated to the caller
    //

    *Descriptor = descriptor;
    return ioStatus.Status;
}


NTSTATUS
ClassSignalCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    )

/*++

Routine Description:

    This completion routine will signal the event given as context and then
    return STATUS_MORE_PROCESSING_REQUIRED to stop event completion.  It is
    the responsibility of the routine waiting on the event to complete the
    request and free the event.

Arguments:

    DeviceObject - a pointer to the device object

    Irp - a pointer to the irp

    Event - a pointer to the event to signal

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
ClassPnpQueryFdoRelations(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine will call the driver's enumeration routine to update the
    list of PDO's.  It will then build a response to the
    IRP_MN_QUERY_DEVICE_RELATIONS and place it into the information field in
    the irp.

Arguments:

    Fdo - a pointer to the functional device object we are enumerating

    Irp - a pointer to the enumeration request

Return Value:

    status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCLASS_DRIVER_EXTENSION
        driverExtension = IoGetDriverObjectExtension(Fdo->DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);
    NTSTATUS status;

    PAGED_CODE();

    //
    // If there's already an enumeration in progress then don't start another
    // one.
    //

    if(InterlockedIncrement(&(fdoExtension->EnumerationInterlock)) == 1) {
        status = driverExtension->InitData.ClassEnumerateDevice(Fdo);
    }

    Irp->IoStatus.Information = (ULONG_PTR) NULL;

    Irp->IoStatus.Status = ClassRetrieveDeviceRelations(
                                Fdo,
                                BusRelations,
                                &((PDEVICE_RELATIONS) Irp->IoStatus.Information));
    InterlockedDecrement(&(fdoExtension->EnumerationInterlock));

    return Irp->IoStatus.Status;
}


VOID
ClassMarkChildrenMissing(
    IN PFUNCTIONAL_DEVICE_EXTENSION Fdo
    )

{
    PCOMMON_DEVICE_EXTENSION commonExtension = &(Fdo->CommonExtension);
    PPHYSICAL_DEVICE_EXTENSION nextChild = commonExtension->ChildList;

    PAGED_CODE();

    ClassAcquireChildLock(Fdo);

    while(nextChild != NULL) {
        PPHYSICAL_DEVICE_EXTENSION tmpChild;

        tmpChild = nextChild;
        nextChild = tmpChild->CommonExtension.ChildList;

        ClassMarkChildMissing(tmpChild, FALSE);
    }
    ClassReleaseChildLock(Fdo);
    return;
}


BOOLEAN
ClassMarkChildMissing(
    IN PPHYSICAL_DEVICE_EXTENSION Child,
    IN BOOLEAN AcquireChildLock
    )
/*++

Routine Description:

    This routine will make an active child "missing."  If the device has never
    been enumerated then it will be deleted on the spot.  If the device has
    not been enumerated then it will be marked as missing so that we can
    not report it in the next device enumeration.

Arguments:

    Child - the child device to be marked as missing.

    AcquireChildLock - TRUE if the child lock should be acquired before removing
                       the missing child.  FALSE if the child lock is already
                       acquired by this thread.

Return Value:

    returns whether or not the child device object has previously been reported
    to PNP.

--*/

{
    BOOLEAN returnValue = Child->IsEnumerated;

    PAGED_CODE();
    ASSERT_PDO(Child->DeviceObject);

    Child->IsMissing = TRUE;

    //
    // Make sure this child is not in the active list.
    //

    ClassRemoveChild(Child->CommonExtension.PartitionZeroExtension,
                     Child,
                     AcquireChildLock);

    if(Child->IsEnumerated == FALSE) {
        ClassRemoveDevice(Child->DeviceObject, IRP_MN_REMOVE_DEVICE);
    }

    return returnValue;
}


NTSTATUS
ClassRetrieveDeviceRelations(
    IN PDEVICE_OBJECT Fdo,
    IN DEVICE_RELATION_TYPE RelationType,
    OUT PDEVICE_RELATIONS *DeviceRelations
    )

/*++

Routine Description:

    This routine will allocate a buffer to hold the specified list of
    relations.  It will then fill in the list with referenced device pointers
    and will return the request.

Arguments:

    Fdo - pointer to the FDO being queried

    RelationType - what type of relations are being queried

    DeviceRelations - a location to store a pointer to the response

Return Value:

    status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    ULONG count = 0;
    ULONG i;

    PPHYSICAL_DEVICE_EXTENSION nextChild;

    ULONG relationsSize;
    PDEVICE_RELATIONS deviceRelations = NULL;

    NTSTATUS status;

    PAGED_CODE();

    ClassAcquireChildLock(fdoExtension);

    nextChild = fdoExtension->CommonExtension.ChildList;

    //
    // Count the number of PDO's attached to this disk
    //

    while(nextChild != NULL) {
        PCOMMON_DEVICE_EXTENSION commonExtension;

        commonExtension = &(nextChild->CommonExtension);

        ASSERTMSG("ClassPnp internal error: missing child on active list\n",
                  (nextChild->IsMissing == FALSE));

        nextChild = commonExtension->ChildList;

        count++;
    };

    relationsSize = (sizeof(DEVICE_RELATIONS) +
                     (count * sizeof(PDEVICE_OBJECT)));

    deviceRelations = ExAllocatePoolWithTag(PagedPool, relationsSize, '5BcS');

    if(deviceRelations == NULL) {

        DebugPrint((1, "ClassRetrieveDeviceRelations: unable to allocate "
                       "%d bytes for device relations\n", relationsSize));

        ClassReleaseChildLock(fdoExtension);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(deviceRelations, relationsSize);

    nextChild = fdoExtension->CommonExtension.ChildList;
    i = count - 1;

    while(nextChild != NULL) {
        PCOMMON_DEVICE_EXTENSION commonExtension;

        commonExtension = &(nextChild->CommonExtension);

        ASSERTMSG("ClassPnp internal error: missing child on active list\n",
                  (nextChild->IsMissing == FALSE));

        deviceRelations->Objects[i--] = nextChild->DeviceObject;

        status = ObReferenceObjectByPointer(
                    nextChild->DeviceObject,
                    0,
                    NULL,
                    KernelMode);
        ASSERT(NT_SUCCESS(status));

        nextChild->IsEnumerated = TRUE;
        nextChild = commonExtension->ChildList;
    }

    ASSERTMSG("Child list has changed: ", i == -1);

    deviceRelations->Count = count;
    *DeviceRelations = deviceRelations;
    ClassReleaseChildLock(fdoExtension);
    return STATUS_SUCCESS;
}


NTSTATUS
ClassGetPdoId(
    IN PDEVICE_OBJECT Pdo,
    IN BUS_QUERY_ID_TYPE IdType,
    IN PUNICODE_STRING IdString
    )

/*++

Routine Description:

    This routine will call into the driver to retrieve a copy of one of it's
    id strings.

Arguments:

    Pdo - a pointer to the pdo being queried

    IdType - which type of id string is being queried

    IdString - an allocated unicode string structure which the driver
               can fill in.

Return Value:

    status

--*/

{
    PCLASS_DRIVER_EXTENSION
        driverExtension = IoGetDriverObjectExtension(Pdo->DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);

    ASSERT_PDO(Pdo);
    ASSERT(driverExtension->InitData.ClassQueryId);

    PAGED_CODE();

    return driverExtension->InitData.ClassQueryId( Pdo, IdType, IdString);
}


NTSTATUS
ClassQueryPnpCapabilities(
    IN PDEVICE_OBJECT DeviceObject,
    IN PDEVICE_CAPABILITIES Capabilities
    )

/*++

Routine Description:

    This routine will call into the class driver to retrieve it's pnp
    capabilities.

Arguments:

    PhysicalDeviceObject - The physical device object to retrieve properties
                           for.

Return Value:

    status

--*/

{
    PCLASS_DRIVER_EXTENSION driverExtension =
        ClassGetDriverExtension(DeviceObject->DriverObject);
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PCLASS_QUERY_PNP_CAPABILITIES queryRoutine = NULL;

    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(Capabilities);

    if(commonExtension->IsFdo) {
        queryRoutine = driverExtension->InitData.FdoData.ClassQueryPnpCapabilities;
    } else {
        queryRoutine = driverExtension->InitData.PdoData.ClassQueryPnpCapabilities;
    }

    if(queryRoutine) {
        return queryRoutine(DeviceObject,
                            Capabilities);
    } else {
        return STATUS_NOT_IMPLEMENTED;
    }
}


VOID
ClassInvalidateBusRelations(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This routine re-enumerates the devices on the "bus".  It will call into
    the driver's ClassEnumerate routine to update the device objects
    immediately.  It will then schedule a bus re-enumeration for pnp by calling
    IoInvalidateDeviceRelations.

Arguments:

    Fdo - a pointer to the functional device object for this bus

Return Value:

    none

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCLASS_DRIVER_EXTENSION
        driverExtension = IoGetDriverObjectExtension(Fdo->DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    ASSERT_FDO(Fdo);
    ASSERT(driverExtension->InitData.ClassEnumerateDevice != NULL);

    if(InterlockedIncrement(&(fdoExtension->EnumerationInterlock)) == 1) {
        status = driverExtension->InitData.ClassEnumerateDevice(Fdo);
    }
    InterlockedDecrement(&(fdoExtension->EnumerationInterlock));

    if(!NT_SUCCESS(status)) {

        DebugPrint((1, "ClassInvalidateBusRelations: EnumerateDevice routine "
                       "returned %lx\n", status));
    }

    IoInvalidateDeviceRelations(fdoExtension->LowerPdo, BusRelations);

    return;
}


NTSTATUS
ClassRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR RemoveType
    )

/*++

Routine Description:

    This routine is called to handle the "removal" of a device.  It will
    forward the request downwards if necesssary, call into the driver
    to release any necessary resources (memory, events, etc) and then
    will delete the device object.

Arguments:

    DeviceObject - a pointer to the device object being removed

    RemoveType - indicates what type of remove this is (regular or surprise).

Return Value:

    status

--*/

{
    PCLASS_DRIVER_EXTENSION
        driverExtension = IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                                     CLASS_DRIVER_EXTENSION_KEY);

    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PDEVICE_OBJECT lowerDeviceObject = commonExtension->LowerDeviceObject;

    NTSTATUS status;

    PAGED_CODE();

    commonExtension->IsRemoved = REMOVE_PENDING;

    //
    // CONSIDER: Is this the right place to do this ??
    //

    if ((commonExtension->IsFdo &&
        (driverExtension->InitData.FdoData.ClassWmiInfo.GuidRegInfo != NULL)) ||
        (! commonExtension->IsFdo &&
         (driverExtension->InitData.PdoData.ClassWmiInfo.GuidRegInfo != NULL)))
    {
        //
        // If started successfully and the driver supports WMI then
        // register it
        status = IoWMIRegistrationControl(DeviceObject,
                                           WMIREG_ACTION_DEREGISTER);
        DebugPrint((3, "ClassRemoveDevice: IoWMIRegistrationControl(%p, "
                       "WMI_ACTION_DEREGISTER) --> %lx\n",
                    DeviceObject, status));
    }

    //
    // Release the mounted device interface if we've set it.
    //

    if(commonExtension->MountedDeviceInterfaceName.Buffer != NULL) {
        IoSetDeviceInterfaceState(
            &(commonExtension->MountedDeviceInterfaceName),
            FALSE);
        RtlFreeUnicodeString(&(commonExtension->MountedDeviceInterfaceName));
        RtlInitUnicodeString(&(commonExtension->MountedDeviceInterfaceName),
                             NULL);
    }

    //
    // If this is a surprise removal we leave the device around - which means
    // we don't have to (or want to) drop the remove lock and wait for pending
    // requests to complete.
    //

    if(RemoveType == IRP_MN_REMOVE_DEVICE) {

        //
        // Release the lock we acquired when the device object was created.
        //

        ClassReleaseRemoveLock(DeviceObject, (PIRP) DeviceObject);

        DebugPrint((1, "ClasspRemoveDevice - Reference count is now %d\n",
                    commonExtension->RemoveLock));

        KeWaitForSingleObject(&(commonExtension->RemoveEvent),
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);

        DebugPrint((1, "ClasspRemoveDevice - removing device %p\n",
                    DeviceObject));

        if(commonExtension->IsFdo) {

            DebugPrint((1, "ClasspRemoveDevice - FDO %p has received a "
                           "remove request.\n", DeviceObject));

        } else {

            PPHYSICAL_DEVICE_EXTENSION pdoExtension =
                                       DeviceObject->DeviceExtension;

            if(pdoExtension->IsMissing) {

                DebugPrint((1, "ClasspRemoveDevice - PDO %p is missing and "
                               "will be removed\n", DeviceObject));

            } else {

                DebugPrint((1, "ClasspRemoveDevice - PDO %p still exists "
                               "and will be removed when it disappears\n",
                            DeviceObject));

                //
                // Reacquire the remove lock for the next time this comes around.
                //

                ClassAcquireRemoveLock(DeviceObject, (PIRP) DeviceObject);

                //
                // the device wasn't missing so it's not really been removed.
                //

                commonExtension->IsRemoved = NO_REMOVE;

                IoInvalidateDeviceRelations(
                    commonExtension->PartitionZeroExtension->LowerPdo,
                    BusRelations);

                return STATUS_SUCCESS;
            }
        }
    }

    ASSERT(commonExtension->DevInfo->ClassRemoveDevice);

    status = commonExtension->DevInfo->ClassRemoveDevice(DeviceObject,
                                                         RemoveType);

    ASSERT(NT_SUCCESS(status));
    status = STATUS_SUCCESS;

    //
    // Class driver successfully released everything - time to get rid
    // of the device
    //

    if(commonExtension->IsFdo) {

        PDEVICE_OBJECT pdo;
        PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
            DeviceObject->DeviceExtension;

        //
        // Disable once a second timer if still active
        //

        ClasspDisableTimer(fdoExtension->DeviceObject);

        //
        // Only reap children on a removal.
        //

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            PPHYSICAL_DEVICE_EXTENSION child;

            //
            // Cleanup the media detection resources now that the class driver
            // has stopped it's timer (if any) and we can be sure they won't
            // call us to do detection again.
            //

            ClassCleanupMediaChangeDetection(fdoExtension);

            //
            // Cleanup any Failure Prediction stuff
            //

            if (fdoExtension->FailurePredictionInfo) {
                ExFreePool(fdoExtension->FailurePredictionInfo);
                fdoExtension->FailurePredictionInfo = NULL;
            }

            //
            // Toss everything on the missing list.
            //

            ClassAcquireChildLock(fdoExtension);
            for (child = ClassRemoveChild(fdoExtension, NULL, FALSE);
                 child != NULL;
                 child = ClassRemoveChild(fdoExtension, NULL, FALSE)) {

                //
                // by this point anything still on the inactive list has only
                // received one remove request.  We've never re-enumerated to
                // determine whether it was really missing.
                //

                ASSERTMSG("ClassPnp Internal Error: found missing child on "
                          "the child list - it should have been removed once "
                          "it was found to be missing:",
                          child->IsMissing == FALSE);

                //
                // The disk has been removed so by definition the partitions must
                // be missing.
                //

                child->IsMissing = TRUE;

                //
                // Yank the pdo.  This routine will unlink the device from the
                // pdo list so NextPdo will point to the next one when it's
                // complete.
                //

                ClassRemoveDevice(child->DeviceObject, RemoveType);
            }
            ClassReleaseChildLock(fdoExtension);

        }

        if(fdoExtension->ReleaseQueueIrp != NULL) {
            ClasspFreeReleaseRequest(DeviceObject);
        }

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            if (commonExtension->DeviceName.Buffer) {
                ExFreePool(commonExtension->DeviceName.Buffer);
                RtlInitUnicodeString(&commonExtension->DeviceName, NULL);
            }

            //
            // Detach our device object from the stack - there's no reason
            // to hold off our cleanup any longer.
            //

            IoDetachDevice(lowerDeviceObject);
        }

    } else if(RemoveType == IRP_MN_REMOVE_DEVICE) {

        PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
            commonExtension->PartitionZeroExtension;
        PPHYSICAL_DEVICE_EXTENSION pdoExtension =
            (PPHYSICAL_DEVICE_EXTENSION) commonExtension;

        //
        // See if this device is in the child list (if this was a suprise
        // removal it might be) and remove it.
        //

        ClassRemoveChild(fdoExtension, pdoExtension, TRUE);

    }

    commonExtension->PartitionLength.QuadPart = 0;

    if(RemoveType == IRP_MN_REMOVE_DEVICE) {
        IoDeleteDevice(DeviceObject);
    }

    return STATUS_SUCCESS;
}

#if DBG

ULONG ClassDebug = 0;
UCHAR ClassBuffer[DEBUG_BUFFER_LENGTH];


VOID
ClassDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )
/*++

Routine Description:

    Debug print for all SCSI drivers

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;
    va_start(ap, DebugMessage);

    if ((DebugPrintLevel <= (ClassDebug & 0x0000ffff)) ||
        ((1 << (DebugPrintLevel + 15)) & ClassDebug)) {

        _vsnprintf(ClassBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
        DbgPrint(ClassBuffer);

    }

    va_end(ap);

} // end ClassDebugPrint()

#else

//
// ClassDebugPrint stub
//

VOID
ClassDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )
{
}

#endif

PCLASS_DRIVER_EXTENSION
ClassGetDriverExtension(
    IN PDRIVER_OBJECT DriverObject
    )

{
    return IoGetDriverObjectExtension(DriverObject, CLASS_DRIVER_EXTENSION_KEY);
}



VOID
ClasspStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine wraps the class driver's start io routine.  If the device
    is being removed it will complete any requests with
    STATUS_DEVICE_DOES_NOT_EXIST and fire up the next packet.

Arguments:

Return Value:

    none

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    //
    // We're already holding the remove lock so just check the variable and
    // see what's going on.
    //

    if(commonExtension->IsRemoved) {

        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;

        ClassAcquireRemoveLock(DeviceObject, (PIRP) ClasspStartIo);
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_DISK_INCREMENT);
        IoStartNextPacket(DeviceObject, FALSE);
        ClassReleaseRemoveLock(DeviceObject, (PIRP) ClasspStartIo);
        return;
    }

    commonExtension->DriverExtension->InitData.ClassStartIo(
        DeviceObject,
        Irp);

    return;
}


VOID
ClassUpdateInformationInRegistry(
    IN PDEVICE_OBJECT     Fdo,
    IN PCHAR              DeviceName,
    IN ULONG              DeviceNumber,
    IN PINQUIRYDATA       InquiryData,
    IN ULONG              InquiryDataLength
    )

/*++

Routine Description:

    This routine has knowledge about the layout of the device map information
    in the registry.  It will update this information to include a value
    entry specifying the dos device name that is assumed to get assigned
    to this NT device name.  For more information on this assigning of the
    dos device name look in the drive support routine in the hal that assigns
    all dos names.  Since most version of tape firmware do not work and most
    vendor did not bother to follow the specification the entire inquiry
    information must also be stored in the registry so than someone can
    figure out the firmware version.

Arguments:

    DeviceObject - A pointer to the device object for the tape device.

Return Value:

    None

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    NTSTATUS          status;
    SCSI_ADDRESS      scsiAddress;
    OBJECT_ATTRIBUTES objectAttributes;
    PUCHAR            buffer;
    STRING            string;
    UNICODE_STRING    unicodeName;
    UNICODE_STRING    unicodeRegistryPath;
    UNICODE_STRING    unicodeData;
    HANDLE            targetKey;
    IO_STATUS_BLOCK   ioStatus;


    PAGED_CODE();

    ASSERT(DeviceName);
    fdoExtension = Fdo->DeviceExtension;
    buffer = NULL;
    targetKey = NULL;
    RtlZeroMemory(&unicodeName,         sizeof(UNICODE_STRING));
    RtlZeroMemory(&unicodeData,         sizeof(UNICODE_STRING));
    RtlZeroMemory(&unicodeRegistryPath, sizeof(UNICODE_STRING));

    TRY {

        //
        // Issue GET_ADDRESS Ioctl to determine path, target, and lun information.
        //

        ClassSendDeviceIoControlSynchronous(
            IOCTL_SCSI_GET_ADDRESS,
            Fdo,
            &scsiAddress,
            0,
            sizeof(SCSI_ADDRESS),
            FALSE,
            &ioStatus
            );

        if (!NT_SUCCESS(ioStatus.Status)) {

            status = ioStatus.Status;
            DebugPrint((1,
                        "UpdateInformationInRegistry: Get Address failed %lx\n",
                        status));
            LEAVE;

        } else {

            DebugPrint((1,
                        "GetAddress: Port %x, Path %x, Target %x, Lun %x\n",
                        scsiAddress.PortNumber,
                        scsiAddress.PathId,
                        scsiAddress.TargetId,
                        scsiAddress.Lun));

        }

        //
        // Allocate a buffer for the reg. spooge.
        //

        buffer = ExAllocatePoolWithTag(PagedPool, 1024, '6BcS');

        if (buffer == NULL) {

            //
            // There is not return value for this.  Since this is done at
            // claim device time (currently only system initialization) getting
            // the registry information correct will be the least of the worries.
            //

            LEAVE;
        }

        sprintf(buffer,
                "\\Registry\\Machine\\Hardware\\DeviceMap\\Scsi\\Scsi Port %d\\Scsi Bus %d\\Target Id %d\\Logical Unit Id %d",
                scsiAddress.PortNumber,
                scsiAddress.PathId,
                scsiAddress.TargetId,
                scsiAddress.Lun);

        RtlInitString(&string, buffer);

        status = RtlAnsiStringToUnicodeString(&unicodeRegistryPath,
                                              &string,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            LEAVE;
        }

        //
        // Open the registry key for the scsi information for this
        // scsibus, target, lun.
        //

        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeRegistryPath,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        status = ZwOpenKey(&targetKey,
                           KEY_READ | KEY_WRITE,
                           &objectAttributes);

        if (!NT_SUCCESS(status)) {
            LEAVE;
        }

        //
        // Now construct and attempt to create the registry value
        // specifying the device name in the appropriate place in the
        // device map.
        //

        RtlInitUnicodeString(&unicodeName, L"DeviceName");

        sprintf(buffer, "%s%d", DeviceName, DeviceNumber);
        RtlInitString(&string, buffer);
        status = RtlAnsiStringToUnicodeString(&unicodeData,
                                              &string,
                                              TRUE);
        if (NT_SUCCESS(status)) {
            status = ZwSetValueKey(targetKey,
                                   &unicodeName,
                                   0,
                                   REG_SZ,
                                   unicodeData.Buffer,
                                   unicodeData.Length);
        }

        //
        // if they sent in data, update the registry
        //

        if (InquiryDataLength) {

            ASSERT(InquiryData);

            RtlInitUnicodeString(&unicodeName, L"InquiryData");
            status = ZwSetValueKey(targetKey,
                                   &unicodeName,
                                   0,
                                   REG_BINARY,
                                   InquiryData,
                                   InquiryDataLength);
        }

        // that's all, except to clean up.

    } FINALLY {

        if (unicodeData.Buffer) {
            RtlFreeUnicodeString(&unicodeData);
        }
        if (unicodeRegistryPath.Buffer) {
            RtlFreeUnicodeString(&unicodeRegistryPath);
        }
        if (targetKey) {
            ZwClose(targetKey);
        }
        if (buffer) {
            ExFreePool(buffer);
        }

    }

}


ClasspSendSynchronousCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This completion routine will set the user event in the irp after
    freeing the irp and the associated MDL (if any).

Arguments:

    DeviceObject - the device object which requested the completion routine

    Irp - the irp being completed

    Context - unused

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    DebugPrint((3, "ClasspSendSynchronousCompletion: %p %p %p\n",
                   DeviceObject, Irp, Context));
    //
    // First set the status and information fields in the io status block
    // provided by the caller.
    //

    *(Irp->UserIosb) = Irp->IoStatus;

    //
    // Unlock the pages for the data buffer.
    //

    if(Irp->MdlAddress) {
        MmUnlockPages(Irp->MdlAddress);
        IoFreeMdl(Irp->MdlAddress);
    }

    //
    // Signal the caller's event.
    //

    KeSetEvent(Irp->UserEvent, IO_NO_INCREMENT, FALSE);

    //
    // Free the MDL and the IRP.
    //

    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


VOID
ClasspRegisterMountedDeviceInterface(
    IN PDEVICE_OBJECT DeviceObject
    )

{

    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    BOOLEAN isFdo = commonExtension->IsFdo;

    PDEVICE_OBJECT pdo;
    UNICODE_STRING interfaceName;

    NTSTATUS status;

    if(isFdo) {

        PFUNCTIONAL_DEVICE_EXTENSION functionalExtension;

        functionalExtension =
            (PFUNCTIONAL_DEVICE_EXTENSION) commonExtension;
        pdo = functionalExtension->LowerPdo;
    } else {
        pdo = DeviceObject;
    }

    status = IoRegisterDeviceInterface(
                pdo,
                &MOUNTDEV_MOUNTED_DEVICE_GUID,
                NULL,
                &interfaceName
                );

    if(NT_SUCCESS(status)) {

        //
        // Copy the interface name before setting the interface state - the
        // name is needed by the components we notify.
        //

        commonExtension->MountedDeviceInterfaceName = interfaceName;
        status = IoSetDeviceInterfaceState(&interfaceName, TRUE);

        if(!NT_SUCCESS(status)) {
            RtlFreeUnicodeString(&interfaceName);
        }
    }

    if(!NT_SUCCESS(status)) {
        RtlInitUnicodeString(&(commonExtension->MountedDeviceInterfaceName),
                             NULL);
    }
    return;
}


VOID
ClassSendDeviceIoControlSynchronous(
    IN ULONG IoControlCode,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN OUT PVOID Buffer OPTIONAL,
    IN ULONG InputBufferLength,
    IN ULONG OutputBufferLength,
    IN BOOLEAN InternalDeviceIoControl,
    OUT PIO_STATUS_BLOCK IoStatus
    )
/*++
    This is based on IoBuildDeviceIoControlRequest()
    Modified to reduce code and memory by not double-buffering the io,
    using the same buffer for input and output, allocating and deallocating
    the mdl for the caller, and waiting for the io to complete.

    this works around the rare cases in which APC's are disabled,
    since IoBuildDeviceIoControl() uses APC's to signal completion
    this has led to a number of difficult-to detect hangs, where
    the irp is completed, but the event passed to IoBuild...() is
    still being waited upon by the caller.

--*/

{
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    ULONG method;

    PAGED_CODE();

    irp = NULL;
    method = IoControlCode & 3;


#if DBG // Begin Argument Checking (nop in fre version)

    ASSERT(ARGUMENT_PRESENT(IoStatus));

    if ((InputBufferLength != 0) || (OutputBufferLength != 0)) {

        ASSERT(ARGUMENT_PRESENT(Buffer));

    } else {

        ASSERT(!ARGUMENT_PRESENT(Buffer));

    }
#endif // DBG

    //
    // Begin by allocating the IRP for this request.  Do not charge quota to
    // the current process for this IRP.
    //

    irp = IoAllocateIrp(TargetDeviceObject->StackSize, FALSE);
    if (!irp) {
        (*IoStatus).Information = 0;
        (*IoStatus).Status = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }

    //
    // Get a pointer to the stack location of the first driver which will be
    // invoked.  This is where the function codes and the parameters are set.
    //

    irpSp = IoGetNextIrpStackLocation(irp);

    //
    // Set the major function code based on the type of device I/O control
    // function the caller has specified.
    //

    if (InternalDeviceIoControl) {
        irpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    } else {
        irpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
    }

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP for those parameters that are the same for all four methods.
    //

    irpSp->Parameters.DeviceIoControl.OutputBufferLength = OutputBufferLength;
    irpSp->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
    irpSp->Parameters.DeviceIoControl.IoControlCode = IoControlCode;

    //
    // Get the method bits from the I/O control code to determine how the
    // buffers are to be passed to the driver.
    //

    switch (method) {
        // case 0
        case METHOD_BUFFERED: {
            if ((InputBufferLength != 0) || (OutputBufferLength != 0)) {

                irp->AssociatedIrp.SystemBuffer =
                    ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                          max(InputBufferLength, OutputBufferLength),
                                          CLASS_TAG_DEVICE_CONTROL
                                          );

                if (irp->AssociatedIrp.SystemBuffer == NULL) {
                    IoFreeIrp(irp);
                    (*IoStatus).Information = 0;
                    (*IoStatus).Status = STATUS_INSUFFICIENT_RESOURCES;
                    return;
                }

                if (InputBufferLength != 0) {
                    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer,
                                  Buffer,
                                  InputBufferLength);
                }
            } // end of buffering

            irp->UserBuffer = Buffer;
            break;
        }

        // case 1, case 2
        case METHOD_IN_DIRECT:
        case METHOD_OUT_DIRECT: {


            if (InputBufferLength != 0) {
                irp->AssociatedIrp.SystemBuffer = Buffer;
            }

            if (OutputBufferLength != 0) {

                irp->MdlAddress = IoAllocateMdl(Buffer,
                                                OutputBufferLength,
                                                FALSE, FALSE,
                                                (PIRP) NULL);

                if (irp->MdlAddress == NULL) {
                    IoFreeIrp(irp);
                    (*IoStatus).Information = 0;
                    (*IoStatus).Status = STATUS_INSUFFICIENT_RESOURCES;
                    return;
                }

                if (method == METHOD_IN_DIRECT) {
                    MmProbeAndLockPages(irp->MdlAddress,
                                        KernelMode,
                                        IoReadAccess);
                } else if (method == METHOD_OUT_DIRECT) {
                    MmProbeAndLockPages(irp->MdlAddress,
                                        KernelMode,
                                        IoWriteAccess);
                } else {
                    ASSERT(!"If other methods reach here, code is out of date");
                }
            }
            break;
        }

        // case 3
        case METHOD_NEITHER: {

            ASSERT(!"This routine does not support METHOD_NEITHER ioctls");
            IoStatus->Information = 0;
            IoStatus->Status = STATUS_NOT_SUPPORTED;
            return;
            break;
        }
    } // end of switch(method)

    irp->Tail.Overlay.Thread = PsGetCurrentThread();

    //
    // send the irp synchronously
    //

    ClassSendIrpSynchronous(TargetDeviceObject, irp);

    //
    // copy the iostatus block for the caller
    //

    *IoStatus = irp->IoStatus;

    //
    // free any allocated resources
    //

    switch (method) {
        case METHOD_BUFFERED: {

            ASSERT(irp->UserBuffer == Buffer);

            //
            // first copy the buffered result, if any
            // Note that there are no security implications in
            // not checking for success since only drivers can
            // call into this routine anyways...
            //

            if (OutputBufferLength != 0) {
                RtlCopyMemory(Buffer, // irp->UserBuffer
                              irp->AssociatedIrp.SystemBuffer,
                              OutputBufferLength
                              );
            }

            //
            // then free the memory allocated to buffer the io
            //

            if ((InputBufferLength !=0) || (OutputBufferLength != 0)) {
                ExFreePool(irp->AssociatedIrp.SystemBuffer);
                irp->AssociatedIrp.SystemBuffer = NULL;
            }
            break;
        }

        case METHOD_IN_DIRECT:
        case METHOD_OUT_DIRECT: {

            //
            // we alloc a mdl if there is an output buffer specified
            // free it here after unlocking the pages
            //

            if (OutputBufferLength != 0) {
                ASSERT(irp->MdlAddress != NULL);
                MmUnlockPages(irp->MdlAddress);
                IoFreeMdl(irp->MdlAddress);
                irp->MdlAddress = (PMDL) NULL;
            }
            break;
        }

        case METHOD_NEITHER: {
            ASSERT(!"Code is out of date");
            break;
        }
    }

    //
    // we always have allocated an irp.  free it here.
    //

    IoFreeIrp(irp);
    irp = (PIRP) NULL;

    //
    // return the io status block's status to the caller
    //

    return;
}


NTSTATUS
ClassForwardIrpSynchronous(
    IN PCOMMON_DEVICE_EXTENSION CommonExtension,
    IN PIRP Irp
    )
{
    IoCopyCurrentIrpStackLocationToNext(Irp);
    return ClassSendIrpSynchronous(CommonExtension->LowerDeviceObject, Irp);
}


NTSTATUS
ClassSendIrpSynchronous(
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PIRP Irp
    )
{
    KEVENT event;
    NTSTATUS status;

    ASSERT(TargetDeviceObject != NULL);
    ASSERT(Irp != NULL);
    ASSERT(Irp->StackCount >= TargetDeviceObject->StackSize);

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);
    IoSetCompletionRoutine(Irp, ClassSignalCompletion, &event,
                           TRUE, TRUE, TRUE);

    status = IoCallDriver(TargetDeviceObject, Irp);

    if (status == STATUS_PENDING) {

#if DBG
        LARGE_INTEGER timeout;

        timeout.QuadPart = (LONGLONG)(-1 * 10 * 1000 * (LONGLONG)1000 *
                                      Globals.SecondsToWaitForIrps);

        do {
            status = KeWaitForSingleObject(&event,
                                           Executive,
                                           KernelMode,
                                           FALSE,
                                           &timeout);


            if (status == STATUS_TIMEOUT) {

                DebugPrint((0, "ClassSendIrpSynchronous: (%p) irp %p did not "
                            "complete within %x seconds\n",
                            TargetDeviceObject, Irp,
                            Globals.SecondsToWaitForIrps
                            ));

                if (Globals.BreakOnLostIrps != 0) {
                    ASSERT(!" - Irp failed to complete within 30 seconds - ");
                }
            }


        } while (status==STATUS_TIMEOUT);

        status = Irp->IoStatus.Status;

#else  // !DBG

        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        status = Irp->IoStatus.Status;

#endif // DBG

    }

    return status;
}


PVPB
ClassGetVpb(
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    return DeviceObject->Vpb;
}


NTSTATUS
ClasspAllocateReleaseRequest(
    IN PDEVICE_OBJECT Fdo
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PIO_STACK_LOCATION irpStack;

    KeInitializeSpinLock(&(fdoExtension->ReleaseQueueSpinLock));

    fdoExtension->ReleaseQueueNeeded = FALSE;
    fdoExtension->ReleaseQueueInProgress = FALSE;
    fdoExtension->ReleaseQueueIrpFromPool = FALSE;

    //
    // The class driver is responsible for allocating a properly sized irp,
    // or ClassReleaseQueue will attempt to do it on the first error.
    //

    fdoExtension->ReleaseQueueIrp = NULL;

    //
    // Write length to SRB.
    //

    fdoExtension->ReleaseQueueSrb.Length = SCSI_REQUEST_BLOCK_SIZE;

    return STATUS_SUCCESS;
}


VOID
ClasspFreeReleaseRequest(
    IN PDEVICE_OBJECT Fdo
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    if(fdoExtension->ReleaseQueueIrpFromPool) {
        ExFreePool(fdoExtension->ReleaseQueueIrp);
    } else {
        IoFreeIrp(fdoExtension->ReleaseQueueIrp);
    }

    fdoExtension->ReleaseQueueIrp = NULL;

    return;
}


VOID
ClassReleaseQueue(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This routine issues an internal device control command
    to the port driver to release a frozen queue. The call
    is issued asynchronously as ClassReleaseQueue will be invoked
    from the IO completion DPC (and will have no context to
    wait for a synchronous call to complete).

    This routine must be called with the remove lock held.

Arguments:

    Fdo - The functional device object for the device with the frozen queue.

Return Value:

    None.

--*/
{
    ClasspReleaseQueue(Fdo, NULL);
    return;
} // end ClassReleaseQueue()


VOID
ClasspReleaseQueue(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP ReleaseQueueIrp OPTIONAL
    )

/*++

Routine Description:

    This routine issues an internal device control command
    to the port driver to release a frozen queue. The call
    is issued asynchronously as ClassReleaseQueue will be invoked
    from the IO completion DPC (and will have no context to
    wait for a synchronous call to complete).

    This routine must be called with the remove lock held.

Arguments:

    Fdo - The functional device object for the device with the frozen queue.

    ReleaseQueueIrp - If this irp is supplied then the test to determine whether
                      a release queue request is in progress will be ignored.
                      The irp provided must be the IRP originally allocated
                      for release queue requests (so this parameter can only
                      really be provided by the release queue completion
                      routine.)

Return Value:

    None.

--*/
{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PDEVICE_OBJECT lowerDevice;
    PSCSI_REQUEST_BLOCK srb;
    KIRQL currentIrql;

    lowerDevice = fdoExtension->CommonExtension.LowerDeviceObject;

    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);

    KeAcquireSpinLockAtDpcLevel(&(fdoExtension->ReleaseQueueSpinLock));

    srb = &(fdoExtension->ReleaseQueueSrb);
    irp = fdoExtension->ReleaseQueueIrp;

    if((fdoExtension->ReleaseQueueInProgress == TRUE) &&
       (ReleaseQueueIrp == NULL))  {

        //
        // Someone is already using the irp - just set the flag to indicate that
        // we need to release the queue again.
        //

        fdoExtension->ReleaseQueueNeeded = TRUE;
        KeReleaseSpinLockFromDpcLevel(&(fdoExtension->ReleaseQueueSpinLock));
        KeLowerIrql(currentIrql);
        return;
    }

    ASSERT((ReleaseQueueIrp == NULL) ||
           (ReleaseQueueIrp == fdoExtension->ReleaseQueueIrp));

    //
    // Mark that there is a release queue in progress and drop the spinlock.
    //

    fdoExtension->ReleaseQueueInProgress = TRUE;

    KeReleaseSpinLockFromDpcLevel(&(fdoExtension->ReleaseQueueSpinLock));

    if(irp == NULL) {

        //
        // The class driver never allocated a release-queue irp.  Try to
        // allocate one for it now.
        //

        DebugPrint((1, "ClassReleaseQueue: Allocating release queue irp\n"));

        fdoExtension->ReleaseQueueIrp =
            ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                  IoSizeOfIrp(lowerDevice->StackSize),
                                  CLASS_TAG_RELEASE_QUEUE
                                  );

        fdoExtension->ReleaseQueueIrpFromPool = TRUE;

        irp = fdoExtension->ReleaseQueueIrp;

        IoInitializeIrp(irp,
                        IoSizeOfIrp(lowerDevice->StackSize),
                        lowerDevice->StackSize);
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    srb->OriginalRequest = irp;

    //
    // Store the SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = srb;

    //
    // If this device is removable then flush the queue.  This will also
    // release it.
    //

    if (Fdo->Characteristics & FILE_REMOVABLE_MEDIA) {
       srb->Function = SRB_FUNCTION_FLUSH_QUEUE;
    } else {
       srb->Function = SRB_FUNCTION_RELEASE_QUEUE;
    }

    ClassAcquireRemoveLock(Fdo, irp);

    IoSetCompletionRoutine(irp,
                           ClassReleaseQueueCompletion,
                           Fdo,
                           TRUE,
                           TRUE,
                           TRUE);

    IoCallDriver(lowerDevice, irp);

    KeLowerIrql(currentIrql);

    return;

} // end ClassReleaseQueue()



NTSTATUS
ClassReleaseQueueCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
/*++

Routine Description:

    This routine is called when an asynchronous I/O request
    which was issused by the class driver completes.  Examples of such requests
    are release queue or START UNIT. This routine releases the queue if
    necessary.  It then frees the context and the IRP.

Arguments:

    DeviceObject - The device object for the logical unit; however since this
        is the top stack location the value is NULL.

    Irp - Supplies a pointer to the Irp to be processed.

    Context - Supplies the context to be used to process this request.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    KIRQL oldIrql;

    BOOLEAN releaseQueueNeeded;

    DeviceObject = Context;

    fdoExtension = DeviceObject->DeviceExtension;

    ClassReleaseRemoveLock(DeviceObject, Irp);

    //
    // Grab the spinlock and clear the release queue in progress flag so others
    // can run.  Save (and clear) the state of the release queue needed flag
    // so that we can issue a new release queue outside the spinlock.
    //

    KeAcquireSpinLock(&(fdoExtension->ReleaseQueueSpinLock), &oldIrql);

    releaseQueueNeeded = fdoExtension->ReleaseQueueNeeded;

    fdoExtension->ReleaseQueueNeeded = FALSE;
    fdoExtension->ReleaseQueueInProgress = FALSE;

    KeReleaseSpinLock(&(fdoExtension->ReleaseQueueSpinLock), oldIrql);

    //
    // If we need a release queue then issue one now.  Another processor may
    // have already started one in which case we'll try to issue this one after
    // it is done - but we should never recurse more than one deep.
    //

    if(releaseQueueNeeded) {
        ClasspReleaseQueue(DeviceObject, Irp);
    }

    //
    // Indicate the I/O system should stop processing the Irp completion.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

} // ClassAsynchronousCompletion()


VOID
ClassAcquireChildLock(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    if(FdoExtension->ChildLockOwner != KeGetCurrentThread()) {
        KeWaitForSingleObject(&FdoExtension->ChildLock,
                              Executive, KernelMode,
                              FALSE, NULL);

        ASSERT(FdoExtension->ChildLockOwner == NULL);
        ASSERT(FdoExtension->ChildLockAcquisitionCount == 0);

        FdoExtension->ChildLockOwner = KeGetCurrentThread();
    } else {
        ASSERT(FdoExtension->ChildLockAcquisitionCount != 0);
    }

    FdoExtension->ChildLockAcquisitionCount++;
    return;
}


VOID
ClassReleaseChildLock(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    ASSERT(FdoExtension->ChildLockOwner == KeGetCurrentThread());
    ASSERT(FdoExtension->ChildLockAcquisitionCount != 0);

    FdoExtension->ChildLockAcquisitionCount -= 1;

    if(FdoExtension->ChildLockAcquisitionCount == 0) {
        FdoExtension->ChildLockOwner = NULL;
        KeSetEvent(&FdoExtension->ChildLock, IO_NO_INCREMENT, FALSE);
    }

    return;
}


VOID
ClassAddChild(
    IN PFUNCTIONAL_DEVICE_EXTENSION Parent,
    IN PPHYSICAL_DEVICE_EXTENSION Child,
    IN BOOLEAN AcquireLock
    )
/*++
Routine Description:

    This routine will insert a new child into the head of the child list.

Arguments:

    Parent - the child's parent (contains the head of the list)
    Child - the child to be inserted.
    AcquireLock - whether the child lock should be acquired (TRUE) or whether
                  it's already been acquired by or on behalf of the caller
                  (FALSE).

Return Value:

    None.

--*/

{
    if(AcquireLock) {
        ClassAcquireChildLock(Parent);
    }

#if DBG
    //
    // Make sure this child's not already in the list.
    //
    {
        PPHYSICAL_DEVICE_EXTENSION testChild;

        for (testChild = Parent->CommonExtension.ChildList;
             testChild != NULL;
             testChild = testChild->CommonExtension.ChildList) {

            ASSERT(testChild != Child);
        }
    }
#endif //DBG

    Child->CommonExtension.ChildList = Parent->CommonExtension.ChildList;
    Parent->CommonExtension.ChildList = Child;

    if(AcquireLock) {
        ClassReleaseChildLock(Parent);
    }
    return;
}


PPHYSICAL_DEVICE_EXTENSION
ClassRemoveChild(
    IN PFUNCTIONAL_DEVICE_EXTENSION Parent,
    IN PPHYSICAL_DEVICE_EXTENSION Child,
    IN BOOLEAN AcquireLock
    )

/*++
Routine Description:

    This routine will remove a child from the child list.

Arguments:

    Parent - the parent to be removed from.

    Child - the child to be removed or NULL if the first child should be
            removed.

    AcquireLock - whether the child lock should be acquired (TRUE) or whether
                  it's already been acquired by or on behalf of the caller
                  (FALSE).

Return Value:

    A pointer to the child which was removed or NULL if no such child could
    be found in the list (or if Child was NULL but the list is empty).

--*/
{
    if(AcquireLock) {
        ClassAcquireChildLock(Parent);
    }

    TRY {
        PCOMMON_DEVICE_EXTENSION previousChild = &Parent->CommonExtension;

        //
        // If the list is empty then bail out now.
        //

        if(Parent->CommonExtension.ChildList == NULL) {
            Child = NULL;
            LEAVE;
        }

        //
        // If the caller specified a child then find the child object before
        // it.  If none was specified then the FDO is the child object before
        // the one we want to remove.
        //

        if(Child != NULL) {

            //
            // Scan through the child list to find the entry which points to
            // this one.
            //

            do {
                ASSERT(previousChild != &Child->CommonExtension);

                if(previousChild->ChildList == Child) {
                    break;
                }

                previousChild = &previousChild->ChildList->CommonExtension;
            } while(previousChild != NULL);

            if(previousChild == NULL) {
                Child = NULL;
                LEAVE;
            }
        }

        //
        // Save the next child away then unlink it from the list.
        //

        Child = previousChild->ChildList;
        previousChild->ChildList = Child->CommonExtension.ChildList;
        Child->CommonExtension.ChildList = NULL;

    } FINALLY {
        if(AcquireLock) {
            ClassReleaseChildLock(Parent);
        }
    }
    return Child;
}

