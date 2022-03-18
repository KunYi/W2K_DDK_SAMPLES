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

#define CLASS_INIT_GUID 0
#include "classp.h"

ULONG BreakOnClose = 0;

PFILE_OBJECT_EXTENSION
ClasspGetFsContext(
    IN PCOMMON_DEVICE_EXTENSION CommonExtension,
    IN PFILE_OBJECT FileObject
    );

VOID
ClasspCleanupDisableMcn(
    IN PFILE_OBJECT_EXTENSION FsContext
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ClassCreateClose)
#pragma alloc_text(PAGE, ClasspCreateClose)
#pragma alloc_text(PAGE, ClasspCleanupProtectedLocks)
#pragma alloc_text(PAGE, ClasspEjectionControl)
#pragma alloc_text(PAGE, ClasspCleanupDisableMcn)
#pragma alloc_text(PAGE, ClasspGetFsContext)
#endif

NTSTATUS
ClassCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    SCSI class driver create and close routine.  This is called by the I/O system
    when the device is opened or closed.

Arguments:

    DriverObject - Pointer to driver object created by system.

    Irp - IRP involved.

Return Value:

    Device-specific drivers return value or STATUS_SUCCESS.

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    ULONG removeState;
    NTSTATUS status;

    PAGED_CODE();

    //
    // If we're getting a close request then we know the device object hasn't
    // been completely destroyed.  Let the driver cleanup if necessary.
    //

    removeState = ClassAcquireRemoveLock(DeviceObject, Irp);

    //
    // Invoke the device-specific routine, if one exists. Otherwise complete
    // with SUCCESS
    //

    if((removeState == NO_REMOVE) ||
       IS_CLEANUP_REQUEST(IoGetCurrentIrpStackLocation(Irp)->MajorFunction)) {

        status = ClasspCreateClose(DeviceObject, Irp);

        if((NT_SUCCESS(status)) &&
           (commonExtension->DevInfo->ClassCreateClose)) {

            return commonExtension->DevInfo->ClassCreateClose(DeviceObject, Irp);
        }

    } else {
        status = STATUS_DEVICE_DOES_NOT_EXIST;
    }

    Irp->IoStatus.Status = status;
    ClassReleaseRemoveLock(DeviceObject, Irp);
    ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
    return status;
}


NTSTATUS
ClasspCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine will handle create/close operations for a given classpnp
    device if the class driver doesn't supply it's own handler.  If there
    is a file object supplied for our driver (if it's a FO_DIRECT_DEVICE_OPEN
    file object) then it will initialize a file extension on create or destroy
    the extension on a close.

Arguments:

    DeviceObject - the device object being opened or closed.

    Irp - the create/close irp

Return Value:

    status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    PFILE_OBJECT fileObject = irpStack->FileObject;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    if(irpStack->MajorFunction == IRP_MJ_CREATE) {

        PIO_SECURITY_CONTEXT securityContext =
            irpStack->Parameters.Create.SecurityContext;

        DebugPrint((2, "ClasspCREATEClose: create received for device %p\n",
                    DeviceObject));
        DebugPrint((2, "ClasspCREATEClose: desired access %lx\n",
                    securityContext->DesiredAccess));
        DebugPrint((2, "ClasspCREATEClose: file object %lx\n",
                    irpStack->FileObject));

        ASSERT(BreakOnClose == FALSE);

        if(irpStack->FileObject != NULL) {

            PFILE_OBJECT_EXTENSION fsContext;

            //
            // Allocate our own file object extension for this device object.
            //

            status = AllocateDictionaryEntry(
                        &commonExtension->FileObjectDictionary,
                        (ULONGLONG) irpStack->FileObject,
                        sizeof(FILE_OBJECT_EXTENSION),
                        CLASS_TAG_FILE_OBJECT_EXTENSION,
                        &fsContext);

            if(NT_SUCCESS(status)) {

                RtlZeroMemory(fsContext,
                              sizeof(FILE_OBJECT_EXTENSION));

                fsContext->FileObject = irpStack->FileObject;
                fsContext->DeviceObject = DeviceObject;
            } else if (status == STATUS_OBJECT_NAME_COLLISION) {
                status = STATUS_SUCCESS;
            }
        }

    } else {

        DebugPrint((2, "ClasspCreateCLOSE: close received for device %p\n",
                    DeviceObject));
        DebugPrint((2, "ClasspCreateCLOSE: file object %p\n",
                    fileObject));

        if(irpStack->FileObject != NULL) {

            PFILE_OBJECT_EXTENSION fsContext =
                ClasspGetFsContext(commonExtension, irpStack->FileObject);

            DebugPrint((2, "ClasspCreateCLOSE: file extnesion %p\n",
                        fsContext));

            if(fsContext != NULL) {

                DebugPrint((2, "ClasspCreateCLOSE: extension is ours - "
                               "freeing\n"));
                ASSERT(BreakOnClose == FALSE);

                ClasspCleanupProtectedLocks(fsContext);

                ClasspCleanupDisableMcn(fsContext);

                FreeDictionaryEntry(&(commonExtension->FileObjectDictionary),
                                    fsContext);
            }
        }
    }

    //
    // Notify the lower levels about the create or close operation - give them
    // a chance to cleanup too.
    //

    if(NT_SUCCESS(status)) {
        KEVENT event;

        //
        // Set up the event to wait on
        //

        KeInitializeEvent(&event, SynchronizationEvent, FALSE);

        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoSetCompletionRoutine( Irp, ClassSignalCompletion, &event,
                                TRUE, TRUE, TRUE);

        status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);

        if(status == STATUS_PENDING) {
            KeWaitForSingleObject(&event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);
            status = Irp->IoStatus.Status;
        }
    }

    return status;
}


VOID
ClasspCleanupProtectedLocks(
    IN PFILE_OBJECT_EXTENSION FsContext
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension =
        FsContext->DeviceObject->DeviceExtension;

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
        commonExtension->PartitionZeroExtension;

    ULONG newDeviceLockCount = 1;

    PAGED_CODE();

    DebugPrint((2,"ClasspCleanupProtectedLocks called for %p\n",
                FsContext->DeviceObject));
    DebugPrint((2,"ClasspCleanupProtectedLocks - FsContext %p is locked "
                  "%d times\n", FsContext, FsContext->LockCount));

    ASSERT(BreakOnClose == FALSE);

    //
    // Synchronize with ejection and ejection control requests.
    //

    KeWaitForSingleObject(&(fdoExtension->EjectSynchronizationEvent),
                          UserRequest,
                          UserMode,
                          FALSE,
                          NULL);

    //
    // BUGBUG - we need to wrap the ejection control & eject ioctls in
    // a synchronization event and use it here to avoid the currently
    // implemented race condition.
    //

    //
    // For each secure lock on this handle decrement the secured lock count
    // for the FDO.  Keep track of the new value.
    //

    if(FsContext->LockCount != 0) {

        do {

            InterlockedDecrement(&FsContext->LockCount);

            newDeviceLockCount =
                InterlockedDecrement(&fdoExtension->ProtectedLockCount);

        } while(FsContext->LockCount != 0);

        //
        // If the new lock count has been dropped to zero then issue a lock
        // command to the device.
        //

        DebugPrint((2, "ClasspCleanupProtectedLocks: FDO secured lock count = %d "
                       "lock count = %d\n",
                    fdoExtension->ProtectedLockCount,
                    fdoExtension->LockCount));

        if((newDeviceLockCount == 0) && (fdoExtension->LockCount == 0)) {

            SCSI_REQUEST_BLOCK srb;
            PCDB cdb;
            NTSTATUS status;

            DebugPrint((2, "ClasspCleanupProtectedLocks: FDO lock count dropped "
                           "to zero\n"));

            RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);
            cdb = (PCDB) &(srb.Cdb);

            srb.CdbLength = 6;

            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

            //
            // TRUE - prevent media removal.
            // FALSE - allow media removal.
            //

            cdb->MEDIA_REMOVAL.Prevent = FALSE;

            //
            // Set timeout value.
            //

            srb.TimeOutValue = fdoExtension->TimeOutValue;
            status = ClassSendSrbSynchronous(fdoExtension->DeviceObject,
                                             &srb,
                                             NULL,
                                             0,
                                             FALSE);

            DebugPrint((2, "ClasspCleanupProtectedLocks: unlock request to drive "
                           "returned status %lx\n", status));
        }
    }

    KeSetEvent(&(fdoExtension->EjectSynchronizationEvent),
               IO_NO_INCREMENT,
               FALSE);
    return;
}


VOID
ClasspCleanupDisableMcn(
    IN PFILE_OBJECT_EXTENSION FsContext
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension =
        FsContext->DeviceObject->DeviceExtension;

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
        commonExtension->PartitionZeroExtension;

    ULONG newCount = 1;

    PAGED_CODE();

    DebugPrint((2,"ClasspCleanupDisableMcn called for %p\n",
                FsContext->DeviceObject));
    DebugPrint((2,"ClasspCleanupDisableMcn - FsContext %p is disabled "
                  "%d times\n", FsContext, FsContext->McnDisableCount));

    //
    // For each secure lock on this handle decrement the secured lock count
    // for the FDO.  Keep track of the new value.
    //

    while(FsContext->McnDisableCount != 0) {
        FsContext->McnDisableCount--;
        ClassEnableMediaChangeDetection(fdoExtension);
    }

    return;
}

NTSTATUS
ClasspEjectionControl(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PIRP Irp,
    IN MEDIA_LOCK_TYPE LockType,
    IN BOOLEAN Lock
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension =
        (PCOMMON_DEVICE_EXTENSION) FdoExtension;
    NTSTATUS status;
    PSCSI_REQUEST_BLOCK srb = NULL;

    PAGED_CODE();

    //
    // Interlock with ejection and secure lock cleanup code.  This is a
    // user request so we can allow the stack to get swapped out while we
    // wait for synchronization.
    //

    status = KeWaitForSingleObject(
                &(FdoExtension->EjectSynchronizationEvent),
                UserRequest,
                UserMode,
                FALSE,
                NULL);

    ASSERT(status == STATUS_SUCCESS);

    try {
        PFILE_OBJECT_EXTENSION fsContext = NULL;
        PCDB cdb;

        srb = ClasspAllocateSrb(FdoExtension);

        if(srb == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            leave;
        }

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

        cdb = (PCDB) srb->Cdb;

        //
        // Determine if this is a "secured" request.
        //

        if(LockType == SecureMediaLock) {

            PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
            PFILE_OBJECT fileObject = irpStack->FileObject;

            //
            // Make sure that the file object we are supplied has a
            // proper FsContext before we try doing a secured lock.
            //

            if(fileObject != NULL) {
                fsContext = ClasspGetFsContext(commonExtension, fileObject);
            }

            if (fsContext == NULL) {

                //
                // This handle isn't setup correctly.  We can't let the
                // operation go.
                //

                status = STATUS_INVALID_PARAMETER;
                leave;
            }
        }

        if(Lock) {

            //
            // This is a lock command.  Reissue the command in case bus or
            // device was reset and the lock was cleared.
            //

            switch(LockType) {

                case SimpleMediaLock: {
                    FdoExtension->LockCount++;
                    break;
                }

                case SecureMediaLock: {
                    fsContext->LockCount++;
                    FdoExtension->ProtectedLockCount++;
                    break;
                }

                case InternalMediaLock: {
                    FdoExtension->InternalLockCount++;
                    break;
                }
            }

        } else {

            //
            // This is an unlock command.  If it's a secured one then make sure
            // the caller has a lock outstanding or return an error.
            //

            switch(LockType) {

                case SimpleMediaLock: {
                    if(FdoExtension->LockCount != 0) {
                        FdoExtension->LockCount--;
                    }
                    break;
                }

                case SecureMediaLock: {
                    if(fsContext->LockCount == 0) {
                        status = STATUS_INVALID_DEVICE_STATE;
                        leave;
                    }
                    fsContext->LockCount--;
                    FdoExtension->ProtectedLockCount--;
                    break;
                }

                case InternalMediaLock: {
                    ASSERT(FdoExtension->InternalLockCount != 0);
                    FdoExtension->InternalLockCount--;
                    break;
                }
            }

            //
            // We only send an unlock command to the drive if both the
            // secured and unsecured lock counts have dropped to zero.
            //

            if((FdoExtension->ProtectedLockCount != 0) ||
               (FdoExtension->InternalLockCount != 0) ||
               (FdoExtension->LockCount != 0)) {

                status = STATUS_SUCCESS;
                leave;
            }
        }

        srb->CdbLength = 6;
        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

        //
        // TRUE - prevent media removal.
        // FALSE - allow media removal.
        //

        cdb->MEDIA_REMOVAL.Prevent = Lock;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = FdoExtension->TimeOutValue;

        //
        // The actual lock operation on the device isn't so important
        // as the internal lock counts.  Ignore failures.
        //

        ClassSendSrbSynchronous(FdoExtension->DeviceObject,
                                srb,
                                NULL,
                                0,
                                FALSE);
    } finally {

        KeSetEvent(&(FdoExtension->EjectSynchronizationEvent),
                   IO_NO_INCREMENT,
                   FALSE);
        if (srb) {
            DebugPrint((0, "Freeing srb %p\n"));
            ClasspFreeSrb(FdoExtension, srb);
        }

    }
    return status;
}


PFILE_OBJECT_EXTENSION
ClasspGetFsContext(
    IN PCOMMON_DEVICE_EXTENSION CommonExtension,
    IN PFILE_OBJECT FileObject
    )
{
    PAGED_CODE();
    return GetDictionaryEntry(&(CommonExtension->FileObjectDictionary),
                              (ULONGLONG) FileObject);
}

