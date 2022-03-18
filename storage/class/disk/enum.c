/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    pnp.c

Abstract:

    SCSI disk class driver

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "disk.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, DiskEnumerateDevice)
#pragma alloc_text(PAGE, DiskUpdateRemovablePartitions)
#pragma alloc_text(PAGE, DiskUpdatePartitions)
#pragma alloc_text(PAGE, DiskCreatePdo)

#endif


NTSTATUS
DiskEnumerateDevice(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This routine is called by the class driver to update the PDO list off
    of this FDO.  The disk driver also calls it internally to re-create
    device objects.

    This routine will read the partition table and create new PDO objects as
    necessary.  PDO's that no longer exist will be pulled out of the PDO list
    so that pnp will destroy them.

Arguments:

    Fdo - a pointer to the FDO being re-enumerated

Return Value:

    status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = Fdo->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PPHYSICAL_DEVICE_EXTENSION pdoExtension = NULL;

    PDISK_DATA diskData = (PDISK_DATA) commonExtension->DriverData;

    PDEVICE_OBJECT pdo = NULL;

    ULONG numberListElements = 0;

    PDRIVE_LAYOUT_INFORMATION partitionList;

    NTSTATUS status;

    ASSERT(commonExtension->IsFdo);

    PAGED_CODE();

    //
    // Update our image of the size of the drive.  This may be necessary if
    // the drive size is extended or we just released a reservation to 
    // ensure the kernel doesn't reject the partition table.
    //

    DiskReadDriveCapacity(Fdo);

    //
    // Lock out anyone else trying to repartition the disk.
    //

    DiskAcquirePartitioningLock(fdoExtension);

    //
    // Create objects for all the partitions on the device.
    //

    status = IoReadPartitionTable(Fdo,
                                  fdoExtension->DiskGeometry.BytesPerSector,
                                  TRUE,
                                  (PVOID)&partitionList);

    // If the I/O read partition table failed and this is a removable device,
    // then fix up the partition list to make it look like there is one
    // zero length partition.
    //

    if ((!NT_SUCCESS(status) || partitionList->PartitionCount == 0) &&
         Fdo->Characteristics & FILE_REMOVABLE_MEDIA) {

        //
        // Remember whether the drive is ready.
        //

        diskData->ReadyStatus = status;

        if (NT_SUCCESS(status)) {

            //
            // Free the partition list allocated by IoReadPartitionTable.
            //

            ExFreePool(partitionList);
        }

        //
        // Allocate and zero a partition list.
        //

        partitionList = ExAllocatePoolWithTag(NonPagedPool,
                                              sizeof(*partitionList ),
                                              DISK_TAG_PART_LIST);

        if (partitionList != NULL) {

            RtlZeroMemory( partitionList, sizeof( *partitionList ));

            //
            // Set the partition count to one and the status to success
            // so one device object will be created. Set the partition type
            // to a bogus value.
            //

            partitionList->PartitionCount = 1;

            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(status)) {

        diskData->UpdatePartitionRoutine(Fdo, partitionList);

        //
        // Record disk signature.  
        //

        diskData->Signature = partitionList->Signature;

        ExFreePool(partitionList);
    }

    DiskReleasePartitioningLock(fdoExtension);

    return(STATUS_SUCCESS);

} // end DiskEnumerateDevice()


VOID
DiskUpdateRemovablePartitions(
    IN PDEVICE_OBJECT Fdo,
    IN OUT PDRIVE_LAYOUT_INFORMATION PartitionList
    )

/*++

Routine Description:

    This routine is called by the class DLL to update the PDO list off of this
    FDO.  The disk driver also calls it internally to re-create device objects.

    This routine will read the partition table and update the size of the
    single partition device object which always exists for removable devices.

Arguments:

    Fdo - a pointer to the FDO being reenumerated.

Return Value:

    status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PPHYSICAL_DEVICE_EXTENSION pdoExtension = NULL;

    ULONG partitionCount;

    ULONG partitionNumber;
    ULONG partitionOrdinal = 0;
    ULONG newPartitionNumber;

    PPARTITION_INFORMATION partitionEntry;
    PDISK_DATA pdoData;

    NTSTATUS status;

    PAGED_CODE();

    ASSERT(Fdo->Characteristics & FILE_REMOVABLE_MEDIA);

    partitionCount = PartitionList->PartitionCount;

    for(partitionNumber = 0;
        partitionNumber < partitionCount;
        partitionNumber++) {

        partitionEntry = &(PartitionList->PartitionEntry[partitionNumber]);

        partitionEntry->PartitionNumber = 0;
    }

    //
    // Get exclusive access to the child list while repartitioning.
    //

    ClassAcquireChildLock(fdoExtension);

    //
    // Removable media should never have more than one PDO.
    //

    pdoExtension = fdoExtension->CommonExtension.ChildList;

    if(pdoExtension == NULL) {

        PARTITION_INFORMATION partitionEntry;
        PDEVICE_OBJECT pdo;

        //
        // There is no PDO currently.  Create one and pre-initialize it with
        // a zero length.
        //

        RtlZeroMemory(&partitionEntry, sizeof(partitionEntry));

        partitionEntry.PartitionNumber = 1;

        DebugPrint((1, "DiskUpdateRemovablePartitions: Creating RM partition\n"));

        status = DiskCreatePdo(Fdo,
                               0,
                               &partitionEntry,
                               &pdo);

        if(!NT_SUCCESS(status)) {

            DebugPrint((1, "DiskUpdateRemovablePartitions: error %lx creating "
                           "new PDO for RM partition\n",
                           status));

            ClassReleaseChildLock(fdoExtension);
            return;
        }

        //
        // mark the new device as enumerated
        //

        pdoExtension = pdo->DeviceExtension;
        pdoExtension->IsMissing = FALSE;

    }

    pdoData = pdoExtension->CommonExtension.DriverData;

    //
    // Search the partition list for a valid entry.  We're looking for a
    // primary partition since we only support the one.
    //

    for(partitionNumber = 0;
        partitionNumber < partitionCount;
        partitionNumber++) {

        partitionEntry = &(PartitionList->PartitionEntry[partitionNumber]);

        //
        // Is this partition interesting?
        //

        if(partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED) {
            continue;
        }

        if(IsContainerPartition(partitionEntry->PartitionType)) {
            continue;
        }

        partitionOrdinal++;

        //
        // I guess we've found the first and thus only partition allowed on
        // this disk.  Update the information in the PDO to match the new
        // partition.
        //

        partitionEntry->PartitionNumber = 1;

        pdoData->HiddenSectors = partitionEntry->HiddenSectors;

        DebugPrint((1, "DiskUpdateRemovablePartitions: Matched %wZ to #%d, "
                       "ordinal %d\n",
                       &pdoExtension->CommonExtension.DeviceName,
                       partitionEntry->PartitionNumber,
                       partitionOrdinal));

        ASSERT(partitionEntry->PartitionLength.LowPart != 0x23456789);

        //
        // we found a match - update the information in the device object
        // extension and driverdata
        //

        pdoData->PartitionOrdinal = partitionOrdinal;
        pdoData->BootIndicator = partitionEntry->BootIndicator;

        pdoExtension->CommonExtension.StartingOffset =
            partitionEntry->StartingOffset;

        pdoExtension->CommonExtension.PartitionLength =
            partitionEntry->PartitionLength;

        //
        // If this partition is being re-written then update the type
        // information as well
        //

        if(partitionEntry->RewritePartition) {

            pdoData->PartitionType = partitionEntry->PartitionType;
        }

        //
        // Mark this one as found
        //

        pdoExtension->IsMissing = FALSE;
        ClassReleaseChildLock(fdoExtension);
        return;
    }

    //
    // No interesting partition was found.
    //

    pdoData->HiddenSectors = 0;
    pdoData->PartitionType = PARTITION_ENTRY_UNUSED;
    pdoExtension->CommonExtension.StartingOffset.QuadPart =
       pdoExtension->CommonExtension.PartitionLength.QuadPart = 0;

    ClassReleaseChildLock(fdoExtension);
    return;
}


VOID
DiskUpdatePartitions(
    IN PDEVICE_OBJECT Fdo,
    IN OUT PDRIVE_LAYOUT_INFORMATION PartitionList
    )

/*++

Routine Description:

    This routine will synchronize the information held in the partition list
    with the device objects hanging off this Fdo.  Any new partition objects
    will be created, any non-existant ones will be marked as un-enumerated.

    This will be done in several stages:

        * Clear state (partition number) from every entry in the partition
          list

        * Set IsMissing flag on every child of this FDO

        * For each child of the FDO:
            if a matching partition exists in the partition list,
            update the partition number in the table, update the
            ordinal in the object and mark the object as enumerated

        * For each un-enumerated device object
            zero out the partition information to invalidate the device
            delete the symbolic link if any

        * For each un-matched entry in the partition list:
            create a new partition object
            update the partition number in the list entry
            create a new symbolic link if necessary

Arguments:

    Fdo - a pointer to the functional device object this partition list is for

    PartitionList - a pointer to the partition list being updated

Return Value:

    none

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PPHYSICAL_DEVICE_EXTENSION oldChildList = NULL;
    PPHYSICAL_DEVICE_EXTENSION pdoExtension = NULL;
    // PDEVICE_OBJECT pdo = NULL;

    ULONG partitionCount;

    ULONG partitionNumber;
    ULONG partitionOrdinal;
    ULONG newPartitionNumber;

    PPARTITION_INFORMATION partitionEntry;
    PDISK_DATA pdoData;

    NTSTATUS status;

    PAGED_CODE();

    //
    // Get exclusive access to the child list.
    //

    ClassAcquireChildLock(fdoExtension);

#if 0
#if defined(JAPAN) && defined(_X86_)

    //
    // FMR has 10 partition entries.
    //

    if(ISFUJITSUFMR( MachineID )) {

        partitionCount = ((PartitionList->PartitionCount + 9) / 10) * 10;

    } else {

        partitionCount = ((PartitionList->PartitionCount + 3) / 4) * 4;
    }
#else

    partitionCount = ((PartitionList->PartitionCount + 3) / 4) * 4;

#endif

#else

    partitionCount = PartitionList->PartitionCount;

#endif

    //
    // Pull all the child device objects off the children list.  We'll
    // add them back later.
    //

    oldChildList = fdoExtension->CommonExtension.ChildList;
    fdoExtension->CommonExtension.ChildList = NULL;

    //
    // Clear the partition numbers from the list entries
    //

    for(partitionNumber = 0;
        partitionNumber < partitionCount;
        partitionNumber++) {

        partitionEntry = &(PartitionList->PartitionEntry[partitionNumber]);
        partitionEntry->PartitionNumber = 0;
    }

    //
    // Now match each child partition to it's entry (if any) in the partition
    // list.
    //

    while(oldChildList != NULL) {

        pdoExtension = oldChildList;
        pdoData = pdoExtension->CommonExtension.DriverData;

        //
        // Check all partition entries for a match on offset and length
        //

        partitionOrdinal = 0;

        for(partitionNumber = 0;
            partitionNumber < partitionCount;
            partitionNumber++) {

            partitionEntry = &(PartitionList->PartitionEntry[partitionNumber]);

            //
            // Is this an interesting partition entry?
            //

            if((partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED) ||
               (IsContainerPartition(partitionEntry->PartitionType))) {

                continue;
            }

            partitionOrdinal++;

            if(partitionEntry->PartitionNumber) {

                //
                // This partition has already been found - skip it
                //

                continue;
            }

            //
            // Let's see if the partition information matches
            //

            if(partitionEntry->StartingOffset.QuadPart !=
               pdoExtension->CommonExtension.StartingOffset.QuadPart) {
                continue;
            }

            if(partitionEntry->PartitionLength.QuadPart !=
               pdoExtension->CommonExtension.PartitionLength.QuadPart) {
                continue;
            }

            //
            // Yep - it matches.  Update the information in the entry
            //

            DebugPrint((1, "DiskUpdatePartitions: Pdo %#p - hidden sectors is %#d, should be %#d\n",
                        pdoExtension,
                        pdoData->HiddenSectors,
                        partitionEntry->HiddenSectors));

            partitionEntry->PartitionNumber = pdoExtension->CommonExtension.PartitionNumber;
            pdoData->HiddenSectors = partitionEntry->HiddenSectors;

            break;
        }

        if(partitionNumber != partitionCount) {

            DebugPrint((1, "DiskUpdatePartitions: Matched %wZ to #%d, ordinal "
                           "%d\n",
                           &pdoExtension->CommonExtension.DeviceName,
                           partitionEntry->PartitionNumber,
                           partitionOrdinal));

            ASSERT(partitionEntry->PartitionLength.LowPart != 0x23456789);
            // ASSERT(pdoExtension->CommonExtension.PartitionLength.QuadPart != 0);

            //
            // we found a match - update the information in the device object
            // extension and driverdata
            //

            pdoData->PartitionOrdinal = partitionOrdinal;

            //
            // If this partition is being re-written then update the type
            // information as well
            //

            if(partitionEntry->RewritePartition) {

                pdoData->PartitionType = partitionEntry->PartitionType;
            }

            //
            // Mark this one as found.
            //

            pdoExtension->IsMissing = FALSE;

            //
            // Pull it out of the old child list and add it into the
            // real one.
            //

            oldChildList = pdoExtension->CommonExtension.ChildList;

            pdoExtension->CommonExtension.ChildList =
                fdoExtension->CommonExtension.ChildList;

            fdoExtension->CommonExtension.ChildList = pdoExtension;

        } else {

            PDEVICE_OBJECT nextPdo;

            DebugPrint((1, "DiskUpdatePartitions: Deleting %wZ\n",
                           &pdoExtension->CommonExtension.DeviceName));

            //
            // no matching entry in the partition list - throw this partition
            // object away
            //

            pdoExtension->CommonExtension.PartitionLength.QuadPart = 0;

            //
            // grab a pointer to the next child before we mark this one as
            // missing since missing devices could vanish at any time.
            //

            oldChildList = pdoExtension->CommonExtension.ChildList;
            pdoExtension->CommonExtension.ChildList = (PVOID) -1;

            //
            // Now tell the class driver that this child is "missing" - this
            // will cause it to be deleted.
            //

            ClassMarkChildMissing(pdoExtension, FALSE);
        }
    }

    //
    // At this point the old child list had best be empty.
    //

    ASSERT(oldChildList == NULL);

    //
    // Iterate through the partition entries and create any partition
    // objects that don't already exist
    //

    partitionOrdinal = 0;
    newPartitionNumber = 0;

    for(partitionNumber = 0;
        partitionNumber < partitionCount;
        partitionNumber++) {

        PDEVICE_OBJECT pdo;

        partitionEntry = &(PartitionList->PartitionEntry[partitionNumber]);

        //
        // Is this partition interesting
        //

        if((partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED) ||
           (IsContainerPartition(partitionEntry->PartitionType))) {

            continue;
        }

        //
        // Increment the count of interesting partitions
        //

        partitionOrdinal++;
        newPartitionNumber++;

        //
        // Has this already been matched
        //

        if(partitionEntry->PartitionNumber == 0) {

            LONG i;

            //
            // find the first safe partition number for this device
            //

            for(i = 0; i < (LONG) partitionCount; i++) {

                PPARTITION_INFORMATION tmp = &(PartitionList->PartitionEntry[i]);

                if(tmp->PartitionType == PARTITION_ENTRY_UNUSED) {
                    continue;
                }

                if(IsContainerPartition(tmp->PartitionType)) {
                    continue;
                }

                if(tmp->PartitionNumber == newPartitionNumber) {

                    //
                    // Found a matching partition number - increment the count
                    // and restart the scan.
                    //

                    newPartitionNumber++;
                    i = -1;
                    continue;
                }
            }

            //
            // Assign this partition a partition number
            //

            partitionEntry->PartitionNumber = newPartitionNumber;

            DebugPrint((1, "DiskUpdatePartitions: Found new partition #%d, ord %d "
                           "starting at %#016I64x and running for %#016I64x\n",
                           partitionEntry->PartitionNumber,
                           partitionOrdinal,
                           partitionEntry->StartingOffset.QuadPart,
                           partitionEntry->PartitionLength.QuadPart));

            ClassReleaseChildLock(fdoExtension);
            status = DiskCreatePdo(Fdo,
                                   partitionOrdinal,
                                   partitionEntry,
                                   &pdo);
            ClassAcquireChildLock(fdoExtension);

            if(!NT_SUCCESS(status)) {

                DebugPrint((1, "DiskUpdatePartitions: error %lx creating "
                               "new PDO for partition ordinal %d, number %d\n",
                               status,
                               partitionOrdinal,
                               partitionEntry->PartitionNumber));

                //
                // don't increment the partition number - we'll try to reuse
                // it for the next child.
                //

                partitionEntry->PartitionNumber = 0;
                newPartitionNumber--;

                continue;
            }

            //
            // mark the new device as enumerated
            //

            pdoExtension = pdo->DeviceExtension;
            pdoExtension->IsMissing = FALSE;

            //
            // This number's taken already - try to scanning the partition
            // table more than once for a new number.
            //

        }

    }

    ClassReleaseChildLock(fdoExtension);
    return;
}


NTSTATUS
DiskCreatePdo(
    IN PDEVICE_OBJECT Fdo,
    IN ULONG PartitionOrdinal,
    IN PPARTITION_INFORMATION PartitionEntry,
    OUT PDEVICE_OBJECT *Pdo
    )

/*++

Routine Description:

    This routine will create and initialize a new partition device object
    (PDO) and insert it into the FDO partition list.

Arguments:

    Fdo - a pointer to the functional device object this PDO will be a child
          of

    PartitionOrdinal - the partition ordinal for this PDO

    PartitionEntry - the partition information for this device object

    Pdo - a location to store the pdo pointer upon successful completion

Return Value:

    status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PDEVICE_OBJECT pdo = NULL;
    PPHYSICAL_DEVICE_EXTENSION pdoExtension = NULL;

    PUCHAR deviceName = NULL;

    PDISK_DATA diskData = fdoExtension->CommonExtension.DriverData;

    ULONG numberListElements;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    //
    // Create partition object and set up partition parameters.
    //

    status = DiskGenerateDeviceName(FALSE,
                                    fdoExtension->DeviceNumber,
                                    PartitionEntry->PartitionNumber,
                                    &PartitionEntry->StartingOffset,
                                    &PartitionEntry->PartitionLength,
                                    &deviceName);

    if(!NT_SUCCESS(status)) {

        DebugPrint((1, "DiskCreatePdo - Can't generate name %lx\n", status));
        return status;
    }

    DebugPrint((2, "DiskCreatePdo: Create device object %s\n", deviceName));

    status = ClassCreateDeviceObject(Fdo->DriverObject,
                                     deviceName,
                                     Fdo,
                                     FALSE,
                                     &pdo);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1, "DiskEnumerateDevice: Can't create device object for %s\n", deviceName));

        return status;
    }

    //
    // Set up device extension fields.
    //

    pdoExtension = pdo->DeviceExtension;

    //
    // Set up device object fields.
    //

    pdo->Flags |= DO_DIRECT_IO;

    pdo->StackSize = (CCHAR)
        pdoExtension->CommonExtension.LowerDeviceObject->StackSize + 1;

    //
    // Get pointer to new disk data.
    //

    diskData = (PDISK_DATA) pdoExtension->CommonExtension.DriverData;

    //
    // Set the alignment requirements for the device based on the
    // host adapter requirements
    //

    if (Fdo->AlignmentRequirement > pdo->AlignmentRequirement) {
        pdo->AlignmentRequirement = Fdo->AlignmentRequirement;
    }

    if (fdoExtension->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) {
        numberListElements = 30;
    } else {
        numberListElements = 8;
    }

    //
    // Build the lookaside list for srb's for this partition based on
    // whether the adapter and disk can do tagged queueing.
    //

    ClassInitializeSrbLookasideList((PCOMMON_DEVICE_EXTENSION) pdoExtension,
                                    numberListElements);

    //
    // Set the sense-data pointer in the device extension.
    //

    diskData->PartitionOrdinal = PartitionOrdinal;
    pdoExtension->CommonExtension.PartitionNumber = PartitionEntry->PartitionNumber;
    diskData->PartitionType = PartitionEntry->PartitionType;
    diskData->BootIndicator = PartitionEntry->BootIndicator;

    DebugPrint((2, "DiskEnumerateDevice: Partition type is %x\n",
        diskData->PartitionType));

    pdoExtension->CommonExtension.StartingOffset  =
        PartitionEntry->StartingOffset;

    pdoExtension->CommonExtension.PartitionLength =
        PartitionEntry->PartitionLength;

    diskData->HiddenSectors = PartitionEntry->HiddenSectors;

    DebugPrint((1, "DiskCreatePdo: hidden sectors value for pdo %#p set to %#x\n",
                pdo,
                diskData->HiddenSectors));

    //
    // Check for removable media support.
    //

    if (fdoExtension->DeviceDescriptor->RemovableMedia) {
        pdo->Characteristics |= FILE_REMOVABLE_MEDIA;
    }

    pdoExtension->CommonExtension.DeviceObject = pdo;

    pdo->Flags &= ~DO_DEVICE_INITIALIZING;

    *Pdo = pdo;

    return status;
}





VOID
DiskAcquirePartitioningLock(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    PDISK_DATA diskData = FdoExtension->CommonExtension.DriverData;

    PAGED_CODE();

    ASSERT_FDO(FdoExtension->DeviceObject);

    KeWaitForSingleObject(&(diskData->PartitioningEvent),
                          UserRequest,
                          UserMode,
                          FALSE,
                          NULL);
    return;
}


VOID
DiskReleasePartitioningLock(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
{
    PDISK_DATA diskData = FdoExtension->CommonExtension.DriverData;

    PAGED_CODE();

    ASSERT_FDO(FdoExtension->DeviceObject);

    KeSetEvent(&(diskData->PartitioningEvent), IO_NO_INCREMENT, FALSE);
    return;
}

