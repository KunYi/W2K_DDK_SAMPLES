/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    disk.c

Abstract:

    SCSI disk class driver

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "disk.h"
//
// Now instantiate the GUIDs contained in ntddstor.h
//
#include "initguid.h"
#include "ntddstor.h"

NTSTATUS
DiskDetermineMediaTypes(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP     Irp,
    IN UCHAR    MediumType,
    IN UCHAR    DensityCode,
    IN BOOLEAN  MediaPresent,
    IN BOOLEAN  IsWritable
    );

NTSTATUS
DiskGetCacheInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PDISK_CACHE_INFORMATION CacheInfo
    );

NTSTATUS
DiskSetCacheInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PDISK_CACHE_INFORMATION CacheInfo
    );

PPARTITION_INFORMATION
DiskPdoFindPartitionEntry(
    IN PPHYSICAL_DEVICE_EXTENSION Pdo,
    IN PDRIVE_LAYOUT_INFORMATION LayoutInfo
    );

PPARTITION_INFORMATION
DiskFindAdjacentPartition(
    IN PDRIVE_LAYOUT_INFORMATION LayoutInfo,
    IN PPARTITION_INFORMATION BasePartition
    );

PPARTITION_INFORMATION
DiskFindContainingPartition(
    IN PDRIVE_LAYOUT_INFORMATION LayoutInfo,
    IN PPARTITION_INFORMATION BasePartition,
    IN BOOLEAN SearchTopToBottom
    );


#if defined(JAPAN) && defined(_X86_)

//
//  The following field keeps machine platform ID for Non PC/AT
//  machine with Intel CPU, such as NEC PC-9800 Series or Fujitsu
//  FMR Series and so on. These machines supports extra formats on
//  FAT file system, or specific physical devices.
//

ULONG MachineID;
#endif

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DiskUnload)
#pragma alloc_text(PAGE, DiskCreateFdo)
#pragma alloc_text(PAGE, DiskDetermineMediaTypes)
#pragma alloc_text(PAGE, DiskModeSelect)
#pragma alloc_text(PAGE, DisableWriteCache)
#pragma alloc_text(PAGE, ScanForSpecial)
#pragma alloc_text(PAGE, DiskQueryPnpCapabilities)
#pragma alloc_text(PAGE, DiskGetCacheInformation)
#pragma alloc_text(PAGE, DiskSetCacheInformation)
#pragma alloc_text(PAGE, DiskSetInfoExceptionInformation)
#pragma alloc_text(PAGE, DiskGetInfoExceptionInformation)

#pragma alloc_text(PAGE, DiskPdoFindPartitionEntry)
#pragma alloc_text(PAGE, DiskFindAdjacentPartition)
#pragma alloc_text(PAGE, DiskFindContainingPartition)

#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the SCSI hard disk class driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    RegistryPath - Pointer to the name of the services node for this driver.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    CLASS_INIT_DATA InitializationData;

    NTSTATUS status;

#if defined(_X86_)
    //
    // Read the information NtDetect squirreled away about the disks in this
    // system.
    //

    status = DiskSaveDetectInfo(DriverObject);

    if(!NT_SUCCESS(status)) {
        DebugPrint((0, "Disk: couldn't save NtDetect information (%#08lx)\n",
                    status));
    }
#endif

    //
    // Zero InitData
    //

    RtlZeroMemory (&InitializationData, sizeof(CLASS_INIT_DATA));

    InitializationData.InitializationDataSize = sizeof(CLASS_INIT_DATA);

    //
    // Setup sizes and entry points for functional device objects
    //

    InitializationData.FdoData.DeviceExtensionSize = FUNCTIONAL_EXTENSION_SIZE;
    InitializationData.FdoData.DeviceType = FILE_DEVICE_DISK;
    InitializationData.FdoData.DeviceCharacteristics = 0;

    InitializationData.FdoData.ClassInitDevice = DiskInitFdo;
    InitializationData.FdoData.ClassStartDevice = DiskStartFdo;
    InitializationData.FdoData.ClassStopDevice = DiskStopDevice;
    InitializationData.FdoData.ClassRemoveDevice = DiskRemoveDevice;
    InitializationData.FdoData.ClassPowerDevice = ClassSpinDownPowerHandler;

    InitializationData.FdoData.ClassError = DiskFdoProcessError;
    InitializationData.FdoData.ClassReadWriteVerification = DiskReadWriteVerification;
    InitializationData.FdoData.ClassDeviceControl = DiskDeviceControl;
    InitializationData.FdoData.ClassShutdownFlush = DiskShutdownFlush;
    InitializationData.FdoData.ClassCreateClose = NULL;

    //
    // Setup sizes and entry points for physical device objects
    //

    InitializationData.PdoData.DeviceExtensionSize = PHYSICAL_EXTENSION_SIZE;
    InitializationData.PdoData.DeviceType = FILE_DEVICE_DISK;
    InitializationData.PdoData.DeviceCharacteristics = 0;

    InitializationData.PdoData.ClassInitDevice = DiskInitPdo;
    InitializationData.PdoData.ClassStartDevice = DiskStartPdo;
    InitializationData.PdoData.ClassStopDevice = DiskStopDevice;
    InitializationData.PdoData.ClassRemoveDevice = DiskRemoveDevice;

    //
    // Use default power routine for PDOs
    //

    InitializationData.PdoData.ClassPowerDevice = NULL;

    InitializationData.PdoData.ClassError = NULL;
    InitializationData.PdoData.ClassReadWriteVerification = DiskReadWriteVerification;
    InitializationData.PdoData.ClassDeviceControl = DiskDeviceControl;
    InitializationData.PdoData.ClassShutdownFlush = DiskShutdownFlush;
    InitializationData.PdoData.ClassCreateClose = NULL;

    InitializationData.PdoData.ClassDeviceControl = DiskDeviceControl;

    InitializationData.PdoData.ClassQueryPnpCapabilities = DiskQueryPnpCapabilities;

    InitializationData.ClassAddDevice = DiskAddDevice;
    InitializationData.ClassEnumerateDevice = DiskEnumerateDevice;

    InitializationData.ClassQueryId = DiskQueryId;


    InitializationData.FdoData.ClassWmiInfo.GuidCount = 5;
    InitializationData.FdoData.ClassWmiInfo.GuidRegInfo = DiskWmiFdoGuidList;
    InitializationData.FdoData.ClassWmiInfo.ClassQueryWmiRegInfo = DiskFdoQueryWmiRegInfo;
    InitializationData.FdoData.ClassWmiInfo.ClassQueryWmiDataBlock = DiskFdoQueryWmiDataBlock;
    InitializationData.FdoData.ClassWmiInfo.ClassSetWmiDataBlock = DiskFdoSetWmiDataBlock;
    InitializationData.FdoData.ClassWmiInfo.ClassSetWmiDataItem = DiskFdoSetWmiDataItem;
    InitializationData.FdoData.ClassWmiInfo.ClassExecuteWmiMethod = DiskFdoExecuteWmiMethod;
    InitializationData.FdoData.ClassWmiInfo.ClassWmiFunctionControl = DiskWmiFunctionControl;


#if 0
    //
    // Enable this to add WMI support for PDOs
    InitializationData.PdoData.ClassWmiInfo.GuidCount = 1;
    InitializationData.PdoData.ClassWmiInfo.GuidRegInfo = DiskWmiPdoGuidList;
    InitializationData.PdoData.ClassWmiInfo.ClassQueryWmiRegInfo = DiskPdoQueryWmiRegInfo;
    InitializationData.PdoData.ClassWmiInfo.ClassQueryWmiDataBlock = DiskPdoQueryWmiDataBlock;
    InitializationData.PdoData.ClassWmiInfo.ClassSetWmiDataBlock = DiskPdoSetWmiDataBlock;
    InitializationData.PdoData.ClassWmiInfo.ClassSetWmiDataItem = DiskPdoSetWmiDataItem;
    InitializationData.PdoData.ClassWmiInfo.ClassExecuteWmiMethod = DiskPdoExecuteWmiMethod;
    InitializationData.PdoData.ClassWmiInfo.ClassWmiFunctionControl = DiskWmiFunctionControl;
#endif

    InitializationData.ClassUnload = DiskUnload;
    //
    // Call the class init routine
    //

    status = ClassInitialize( DriverObject, RegistryPath, &InitializationData);

#if defined(_X86_)
    if(NT_SUCCESS(status)) {
        IoRegisterBootDriverReinitialization(DriverObject,
                                             DiskDriverReinitialization,
                                             NULL);
    }
#endif

    return status;

} // end DriverEntry()

VOID
DiskUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    PAGED_CODE();

#if defined(_X86_)
    DiskCleanupDetectInfo(DriverObject);
#endif
    return;
}

NTSTATUS
DiskCreateFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PULONG DeviceCount,
    IN BOOLEAN DasdAccessOnly
    )

/*++

Routine Description:

    This routine creates an object for the functional device

Arguments:

    DriverObject - Pointer to driver object created by system.

    PhysicalDeviceObject - Lower level driver we should attach to

    DeviceCount  - Number of previously installed devices.

    DasdAccessOnly - indicates whether or not a file system is allowed to mount
                     on this device object.  Used to avoid double-mounting of 
                     file systems on super-floppies (which can unfortunately be 
                     fixed disks).  If set the i/o system will only allow rawfs
                     to be mounted.

Return Value:

    NTSTATUS

--*/
{
    CCHAR          ntNameBuffer[MAXIMUM_FILENAME_LENGTH];
    STRING         ntNameString;
    UNICODE_STRING ntUnicodeString;

    PUCHAR         deviceName = NULL;

    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE         handle;

    NTSTATUS       status;

    PDEVICE_OBJECT lowerDevice = NULL;
    PDEVICE_OBJECT deviceObject = NULL;

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    STORAGE_PROPERTY_ID propertyId;
    PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor;

    PAGED_CODE();

    *DeviceCount = 0;

    //
    // Set up an object directory to contain the objects for this
    // device and all its partitions.
    //

    do {

        WCHAR buffer[64];
        UNICODE_STRING unicodeDirectoryName;

        swprintf(buffer, L"\\Device\\Harddisk%d", *DeviceCount);

        RtlInitUnicodeString(&unicodeDirectoryName, buffer);

        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeDirectoryName,
                                   OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                                   NULL,
                                   NULL);

        status = ZwCreateDirectoryObject(&handle,
                                         DIRECTORY_ALL_ACCESS,
                                         &objectAttributes);

        (*DeviceCount)++;

    } while((status == STATUS_OBJECT_NAME_COLLISION) ||
            (status == STATUS_OBJECT_NAME_EXISTS));

    if (!NT_SUCCESS(status)) {

        DebugPrint((1, "DiskCreateFdo: Could not create directory - %lx\n",
                    status));

        return(status);
    }

    //
    // When this loop exits the count is inflated by one - fix that.
    //

    (*DeviceCount)--;

    //
    // Claim the device.
    //

    lowerDevice = IoGetAttachedDeviceReference(PhysicalDeviceObject);

    status = ClassClaimDevice(lowerDevice, FALSE);

    if (!NT_SUCCESS(status)) {
        ZwMakeTemporaryObject(handle);
        ZwClose(handle);
        ObDereferenceObject(lowerDevice);
        return status;
    }

    //
    // Create a device object for this device. Each physical disk will
    // have at least one device object. The required device object
    // describes the entire device. Its directory path is
    // \Device\HarddiskN\Partition0, where N = device number.
    //

    status = DiskGenerateDeviceName(TRUE,
                                    *DeviceCount,
                                    0,
                                    NULL,
                                    NULL,
                                    &deviceName);

    if(!NT_SUCCESS(status)) {
        DebugPrint((1, "DiskCreateFdo - couldn't create name %lx\n",
                       status));

        goto DiskCreateFdoExit;

    }

    status = ClassCreateDeviceObject(DriverObject,
                                     deviceName,
                                     PhysicalDeviceObject,
                                     TRUE,
                                     &deviceObject);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,
                    "DiskCreateFdo: Can not create device object %s\n",
                    ntNameBuffer));

        goto DiskCreateFdoExit;
    }

    //
    // Indicate that IRPs should include MDLs for data transfers.
    //

    SET_FLAG(deviceObject->Flags, DO_DIRECT_IO);

    fdoExtension = deviceObject->DeviceExtension;

    if(DasdAccessOnly) {

        //
        // Inidicate that only RAW should be allowed to mount on the root 
        // partition object.  This ensures that a file system can't doubly 
        // mount on a super-floppy by mounting once on P0 and once on P1.
        //

        SET_FLAG(deviceObject->Vpb->Flags, VPB_RAW_MOUNT);
    }

    //
    // Initialize lock count to zero. The lock count is used to
    // disable the ejection mechanism on devices that support
    // removable media. Only the lock count in the physical
    // device extension is used.
    //

    fdoExtension->LockCount = 0;

    //
    // Save system disk number.
    //

    fdoExtension->DeviceNumber = *DeviceCount;

    //
    // Set the alignment requirements for the device based on the
    // host adapter requirements
    //

    if (lowerDevice->AlignmentRequirement > deviceObject->AlignmentRequirement) {
        deviceObject->AlignmentRequirement = lowerDevice->AlignmentRequirement;
    }

    //
    // Finally, attach to the pdo
    //

    fdoExtension->LowerPdo = PhysicalDeviceObject;

    fdoExtension->CommonExtension.LowerDeviceObject =
        IoAttachDeviceToDeviceStack(
            deviceObject,
            PhysicalDeviceObject);

    if(fdoExtension->CommonExtension.LowerDeviceObject == NULL) {

        //
        // Uh - oh, we couldn't attach
        // cleanup and return
        //

        status = STATUS_UNSUCCESSFUL;
        goto DiskCreateFdoExit;
    }

    //
    // Clear the init flag.
    //

    CLEAR_FLAG(deviceObject->Flags, DO_DEVICE_INITIALIZING);

    //
    // Store a handle to the device object directory for this disk
    //

    fdoExtension->DeviceDirectory = handle;

    ObDereferenceObject(lowerDevice);

    return STATUS_SUCCESS;

DiskCreateFdoExit:

    //
    // Release the device since an error occurred.
    //

    if (deviceObject != NULL) {
        IoDeleteDevice(deviceObject);
    }

    //
    // Delete directory and return.
    //

    if (!NT_SUCCESS(status)) {
        ZwMakeTemporaryObject(handle);
        ZwClose(handle);
    }

    ObDereferenceObject(lowerDevice);

    return(status);
}


NTSTATUS
DiskReadWriteVerification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    I/O System entry for read and write requests to SCSI disks.

Arguments:

    DeviceObject - Pointer to driver object created by system.
    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER startingOffset;

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =
        commonExtension->PartitionZeroExtension;

    ULONG residualBytes;
    NTSTATUS status;

    //
    // Verify parameters of this request.
    // Check that ending sector is within partition and
    // that number of bytes to transfer is a multiple of
    // the sector size.
    //

    startingOffset.QuadPart =
        (currentIrpStack->Parameters.Read.ByteOffset.QuadPart +
         transferByteCount);

    residualBytes = transferByteCount &
                    (fdoExtension->DiskGeometry.BytesPerSector - 1);


    if ((startingOffset.QuadPart > commonExtension->PartitionLength.QuadPart) ||
        (residualBytes != 0)) {

        //
        // This error may be caused by the fact that the drive is not ready.
        //

        status = ((PDISK_DATA) commonExtension->DriverData)->ReadyStatus;

        if (!NT_SUCCESS(status)) {

            //
            // Flag this as a user errror so that a popup is generated.
            //

            DebugPrint((1, "DiskReadWriteVerification: ReadyStatus is %lx\n",
                        status));

            IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);

            //
            // status will keep the current error
            //

        } else if((commonExtension->IsFdo == TRUE) && (residualBytes == 0)) {

            //
            // This failed because we think the physical disk is too small.
            // Send it down to the drive and let the hardware decide for
            // itself.
            //

            status = STATUS_SUCCESS;

        } else {

            //
            // Note fastfat depends on this parameter to determine when to
            // remount due to a sector size change.
            //

            status = STATUS_INVALID_PARAMETER;

        }

    } else {

        //
        // the drive is ready, so ok the read/write
        //

        status = STATUS_SUCCESS;

    }

    Irp->IoStatus.Status = status;
    return status;

} // end DiskReadWrite()



NTSTATUS
DiskDetermineMediaTypes(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP     Irp,
    IN UCHAR    MediumType,
    IN UCHAR    DensityCode,
    IN BOOLEAN  MediaPresent,
    IN BOOLEAN  IsWritable
    )

/*++

Routine Description:

    Determines number of types based on the physical device, validates the user buffer
    and builds the MEDIA_TYPE information.

Arguments:

    DeviceObject - Pointer to functional device object created by system.
    Irp - IOCTL_STORAGE_GET_MEDIA_TYPES_EX Irp.
    MediumType - byte returned in mode data header.
    DensityCode - byte returned in mode data block descriptor.
    NumberOfTypes - pointer to be updated based on actual device.

Return Value:

    Status is returned.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PPHYSICAL_DEVICE_EXTENSION pdoExtension = Fdo->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION   commonExtension = Fdo->DeviceExtension;
    PIO_STACK_LOCATION         irpStack = IoGetCurrentIrpStackLocation(Irp);

    PGET_MEDIA_TYPES  mediaTypes = Irp->AssociatedIrp.SystemBuffer;
    PDEVICE_MEDIA_INFO mediaInfo = &mediaTypes->MediaInfo[0];
    BOOLEAN deviceMatched = FALSE;

    PAGED_CODE();

    //
    // this should be checked prior to calling into this routine
    // as we use the buffer as mediaTypes
    //
    ASSERT(irpStack->Parameters.DeviceIoControl.OutputBufferLength >=
           sizeof(GET_MEDIA_TYPES));


    //
    // Determine if this device is removable or fixed.
    //

    if (!TEST_FLAG(Fdo->Characteristics, FILE_REMOVABLE_MEDIA)) {

        //
        // Fixed disk.
        //

        mediaTypes->DeviceType = FILE_DEVICE_DISK;
        mediaTypes->MediaInfoCount = 1;

        mediaInfo->DeviceSpecific.DiskInfo.Cylinders.QuadPart = fdoExtension->DiskGeometry.Cylinders.QuadPart;
        mediaInfo->DeviceSpecific.DiskInfo.TracksPerCylinder = fdoExtension->DiskGeometry.TracksPerCylinder;
        mediaInfo->DeviceSpecific.DiskInfo.SectorsPerTrack = fdoExtension->DiskGeometry.SectorsPerTrack;
        mediaInfo->DeviceSpecific.DiskInfo.BytesPerSector = fdoExtension->DiskGeometry.BytesPerSector;
        mediaInfo->DeviceSpecific.RemovableDiskInfo.NumberMediaSides = 1;

        mediaInfo->DeviceSpecific.DiskInfo.MediaCharacteristics = (MEDIA_CURRENTLY_MOUNTED | MEDIA_READ_WRITE);

        if (!IsWritable) {
            SET_FLAG(mediaInfo->DeviceSpecific.DiskInfo.MediaCharacteristics,
                     MEDIA_WRITE_PROTECTED);
        }

        mediaInfo->DeviceSpecific.DiskInfo.MediaType = FixedMedia;


    } else {

        PUCHAR vendorId = (PUCHAR) fdoExtension->DeviceDescriptor + fdoExtension->DeviceDescriptor->VendorIdOffset;
        PUCHAR productId = (PUCHAR) fdoExtension->DeviceDescriptor + fdoExtension->DeviceDescriptor->ProductIdOffset;
        PUCHAR productRevision = (PUCHAR) fdoExtension->DeviceDescriptor + fdoExtension->DeviceDescriptor->ProductRevisionOffset;
        DISK_MEDIA_TYPES_LIST const *mediaListEntry;
        ULONG  currentMedia;
        ULONG  i;
        ULONG  j;
        ULONG  sizeNeeded;

        DebugPrint((1,
                   "DiskDetermineMediaTypes: Vendor %s, Product %s\n",
                   vendorId,
                   productId));

        //
        // Run through the list until we find the entry with a NULL Vendor Id.
        //

        for (i = 0; DiskMediaTypes[i].VendorId != NULL; i++) {

            mediaListEntry = &DiskMediaTypes[i];

            if (strncmp(mediaListEntry->VendorId,vendorId,strlen(mediaListEntry->VendorId))) {
                continue;
            }

            if ((mediaListEntry->ProductId != NULL) &&
                 strncmp(mediaListEntry->ProductId, productId, strlen(mediaListEntry->ProductId))) {
                continue;
            }

            if ((mediaListEntry->Revision != NULL) &&
                 strncmp(mediaListEntry->Revision, productRevision, strlen(mediaListEntry->Revision))) {
                continue;
            }

            deviceMatched = TRUE;

            mediaTypes->DeviceType = FILE_DEVICE_DISK;
            mediaTypes->MediaInfoCount = mediaListEntry->NumberOfTypes;

            //
            // Ensure that buffer is large enough.
            //

            sizeNeeded = FIELD_OFFSET(GET_MEDIA_TYPES, MediaInfo[0]) +
                         (mediaListEntry->NumberOfTypes *
                          sizeof(DEVICE_MEDIA_INFO)
                          );

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeNeeded) {

                //
                // Buffer too small
                //

                Irp->IoStatus.Information = sizeNeeded;
                Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            for (j = 0; j < mediaListEntry->NumberOfTypes; j++) {

                mediaInfo->DeviceSpecific.RemovableDiskInfo.Cylinders.QuadPart = fdoExtension->DiskGeometry.Cylinders.QuadPart;
                mediaInfo->DeviceSpecific.RemovableDiskInfo.TracksPerCylinder = fdoExtension->DiskGeometry.TracksPerCylinder;
                mediaInfo->DeviceSpecific.RemovableDiskInfo.SectorsPerTrack = fdoExtension->DiskGeometry.SectorsPerTrack;
                mediaInfo->DeviceSpecific.RemovableDiskInfo.BytesPerSector = fdoExtension->DiskGeometry.BytesPerSector;
                mediaInfo->DeviceSpecific.RemovableDiskInfo.NumberMediaSides = mediaListEntry->NumberOfSides;

                //
                // Set the type.
                //

                mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaType = mediaListEntry->MediaTypes[j];

                if (mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaType == MO_5_WO) {
                    mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics = MEDIA_WRITE_ONCE;
                } else {
                    mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics = MEDIA_READ_WRITE;
                }

                //
                // Status will either be success, if media is present, or no media.
                // It would be optimal to base from density code and medium type, but not all devices
                // have values for these fields.
                //

                if (MediaPresent) {

                    //
                    // The usage of MediumType and DensityCode is device specific, so this may need
                    // to be extended to further key off of product/vendor ids.
                    // Currently, the MO units are the only devices that return this information.
                    //

                    if (MediumType == 2) {
                        currentMedia = MO_5_WO;
                    } else if (MediumType == 3) {
                        currentMedia = MO_5_RW;

                        if (DensityCode == 0x87) {

                            //
                            // Indicate that the pinnacle 4.6 G media
                            // is present. Other density codes will default to normal
                            // RW MO media.
                            //

                            currentMedia = PINNACLE_APEX_5_RW;
                        }
                    } else {
                        currentMedia = 0;
                    }

                    if (currentMedia) {
                        if (mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaType == (STORAGE_MEDIA_TYPE)currentMedia) {
                            SET_FLAG(mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics, MEDIA_CURRENTLY_MOUNTED);
                        }

                    } else {
                        SET_FLAG(mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics, MEDIA_CURRENTLY_MOUNTED);
                    }
                }

                if (!IsWritable) {
                    SET_FLAG(mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics, MEDIA_WRITE_PROTECTED);
                }

                //
                // Advance to next entry.
                //

                mediaInfo++;
            }
        }

        if (!deviceMatched) {

            DebugPrint((1,
                       "DiskDetermineMediaTypes: Unknown device. Vendor: %s Product: %s Revision: %s\n",
                                   vendorId,
                                   productId,
                                   productRevision));
            //
            // Build an entry for unknown.
            //

            mediaInfo->DeviceSpecific.RemovableDiskInfo.Cylinders.QuadPart = fdoExtension->DiskGeometry.Cylinders.QuadPart;
            mediaInfo->DeviceSpecific.RemovableDiskInfo.TracksPerCylinder = fdoExtension->DiskGeometry.TracksPerCylinder;
            mediaInfo->DeviceSpecific.RemovableDiskInfo.SectorsPerTrack = fdoExtension->DiskGeometry.SectorsPerTrack;
            mediaInfo->DeviceSpecific.RemovableDiskInfo.BytesPerSector = fdoExtension->DiskGeometry.BytesPerSector;

            //
            // Set the type.
            //

            mediaTypes->DeviceType = FILE_DEVICE_DISK;
            mediaTypes->MediaInfoCount = 1;

            mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaType = RemovableMedia;
            mediaInfo->DeviceSpecific.RemovableDiskInfo.NumberMediaSides = 1;

            mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics = MEDIA_READ_WRITE;
            if (MediaPresent) {
                SET_FLAG(mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics, MEDIA_CURRENTLY_MOUNTED);
            }

            if (!IsWritable) {
                SET_FLAG(mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics, MEDIA_WRITE_PROTECTED);
            }
        }
    }

    Irp->IoStatus.Information =
        FIELD_OFFSET(GET_MEDIA_TYPES, MediaInfo[0]) +
        (mediaTypes->MediaInfoCount * sizeof(DEVICE_MEDIA_INFO));

    return STATUS_SUCCESS;

}


NTSTATUS
DiskDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    I/O system entry for device controls to SCSI disks.

Arguments:

    Fdo - Pointer to functional device object created by system.
    Irp - IRP involved.

Return Value:

    Status is returned.

--*/

#define SendToFdo(Dev, Irp, Rval)   {                       \
    PCOMMON_DEVICE_EXTENSION ce = Dev->DeviceExtension;     \
    ASSERT_PDO(Dev);                                        \
    IoCopyCurrentIrpStackLocationToNext(Irp);               \
    Rval = IoCallDriver(ce->LowerDeviceObject, Irp);        \
    }

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PPHYSICAL_DEVICE_EXTENSION pdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDISK_DATA             diskData = (PDISK_DATA)(commonExtension->DriverData);
    PSCSI_REQUEST_BLOCK    srb;
    PCDB                   cdb;
    PMODE_PARAMETER_HEADER modeData;
    PIRP                   irp2;
    ULONG                  length;
    NTSTATUS               status;
    KEVENT                 event;
    IO_STATUS_BLOCK        ioStatus;

    BOOLEAN                b = FALSE;

    srb = ExAllocatePoolWithTag(NonPagedPool,
                                SCSI_REQUEST_BLOCK_SIZE,
                                DISK_TAG_SRB);
    Irp->IoStatus.Information = 0;

    if (srb == NULL) {

        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Write zeros to Srb.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb->Cdb;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_DISK_GET_CACHE_INFORMATION:
        b = TRUE;
    case IOCTL_DISK_SET_CACHE_INFORMATION: {

        BOOLEAN getCaching = b;

        if(!commonExtension->IsFdo) {

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        //
        // Validate the request.
        //

        if((getCaching) &&
           (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(DISK_CACHE_INFORMATION))
           ) {
            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = sizeof(DISK_CACHE_INFORMATION);
            break;
        }

        if((!getCaching) &&
           (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(DISK_CACHE_INFORMATION))
           ) {
            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }


        ASSERT(Irp->AssociatedIrp.SystemBuffer != NULL);

        if(getCaching) {
            status = DiskGetCacheInformation(fdoExtension,
                                             Irp->AssociatedIrp.SystemBuffer);

            if(NT_SUCCESS(status)) {
                Irp->IoStatus.Information = sizeof(DISK_CACHE_INFORMATION);
            }
        } else {
            status = DiskSetCacheInformation(fdoExtension,
                                             Irp->AssociatedIrp.SystemBuffer);
        }
        break;
    }

    case SMART_GET_VERSION: {

        PUCHAR buffer;
        PSRB_IO_CONTROL  srbControl;
        PGETVERSIONINPARAMS versionParams;

        if(!commonExtension->IsFdo) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(GETVERSIONINPARAMS)) {
                status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = sizeof(GETVERSIONINPARAMS);
                break;
        }

        //
        // Create notification event object to be used to signal the
        // request completion.
        //

        KeInitializeEvent(&event, NotificationEvent, FALSE);

        srbControl = ExAllocatePoolWithTag(NonPagedPool,
                                           sizeof(SRB_IO_CONTROL) +
                                           sizeof(GETVERSIONINPARAMS),
                                           DISK_TAG_SMART);

        if (!srbControl) {
            status =  STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory(srbControl,
                      sizeof(SRB_IO_CONTROL) + sizeof(GETVERSIONINPARAMS)
                      );

        //
        // fill in srbControl fields
        //

        srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
        RtlMoveMemory (srbControl->Signature, "SCSIDISK", 8);
        srbControl->Timeout = fdoExtension->TimeOutValue;
        srbControl->Length = sizeof(GETVERSIONINPARAMS);
        srbControl->ControlCode = IOCTL_SCSI_MINIPORT_SMART_VERSION;

        //
        // Point to the 'buffer' portion of the SRB_CONTROL
        //

        buffer = (PUCHAR)srbControl;
        (ULONG_PTR)buffer += srbControl->HeaderLength;

        //
        // Ensure correct target is set in the cmd parameters.
        //

        versionParams = (PGETVERSIONINPARAMS)buffer;
        versionParams->bIDEDeviceMap = diskData->ScsiAddress.TargetId;

        //
        // Copy the IOCTL parameters to the srb control buffer area.
        //

        RtlMoveMemory(buffer,
                      Irp->AssociatedIrp.SystemBuffer,
                      sizeof(GETVERSIONINPARAMS));

        ClassSendDeviceIoControlSynchronous(
            IOCTL_SCSI_MINIPORT,
            commonExtension->LowerDeviceObject,
            srbControl,
            sizeof(SRB_IO_CONTROL) + sizeof(GETVERSIONINPARAMS),
            sizeof(SRB_IO_CONTROL) + sizeof(GETVERSIONINPARAMS),
            FALSE,
            &ioStatus);

        status = ioStatus.Status;

        //
        // If successful, copy the data received into the output buffer.
        // This should only fail in the event that the IDE driver is older
        // than this driver.
        //

        if (NT_SUCCESS(status)) {

            buffer = (PUCHAR)srbControl;
            (ULONG_PTR)buffer += srbControl->HeaderLength;

            RtlMoveMemory (Irp->AssociatedIrp.SystemBuffer, buffer,
                           sizeof(GETVERSIONINPARAMS));
            Irp->IoStatus.Information = sizeof(GETVERSIONINPARAMS);
        }

        ExFreePool(srbControl);
        break;
    }

    case SMART_RCV_DRIVE_DATA: {

        PSENDCMDINPARAMS cmdInParameters = ((PSENDCMDINPARAMS)Irp->AssociatedIrp.SystemBuffer);
        ULONG            controlCode = 0;
        PSRB_IO_CONTROL  srbControl;
        PUCHAR           buffer;

        if(!commonExtension->IsFdo) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            (sizeof(SENDCMDINPARAMS) - 1)) {
                status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
                break;

        } else if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            (sizeof(SENDCMDOUTPARAMS) + 512 - 1)) {
                status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = sizeof(SENDCMDOUTPARAMS) + 512 - 1;
                break;
        }

        //
        // Create notification event object to be used to signal the
        // request completion.
        //

        KeInitializeEvent(&event, NotificationEvent, FALSE);

        //
        // use controlCode as a sort of 'STATUS_SUCCESS' to see if it's
        // a valid request type
        //

        if (cmdInParameters->irDriveRegs.bCommandReg == ID_CMD) {

            length = IDENTIFY_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS);
            controlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;

        } else if (cmdInParameters->irDriveRegs.bCommandReg == SMART_CMD) {
            switch (cmdInParameters->irDriveRegs.bFeaturesReg) {
                case READ_ATTRIBUTES:
                    controlCode = IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS;
                    length = READ_ATTRIBUTE_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS);
                    break;
                case READ_THRESHOLDS:
                    controlCode = IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS;
                    length = READ_THRESHOLD_BUFFER_SIZE + sizeof(SENDCMDOUTPARAMS);
                    break;
                default:
                    status = STATUS_INVALID_PARAMETER;
                    break;
            }
        } else {

            status = STATUS_INVALID_PARAMETER;
        }

        if (controlCode == 0) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        srbControl = ExAllocatePoolWithTag(NonPagedPool,
                                           sizeof(SRB_IO_CONTROL) + length,
                                           DISK_TAG_SMART);

        if (!srbControl) {
            status =  STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        //
        // fill in srbControl fields
        //

        srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
        RtlMoveMemory (srbControl->Signature, "SCSIDISK", 8);
        srbControl->Timeout = fdoExtension->TimeOutValue;
        srbControl->Length = length;
        srbControl->ControlCode = controlCode;

        //
        // Point to the 'buffer' portion of the SRB_CONTROL
        //

        buffer = (PUCHAR)srbControl;
        (ULONG_PTR)buffer += srbControl->HeaderLength;

        //
        // Ensure correct target is set in the cmd parameters.
        //

        cmdInParameters->bDriveNumber = diskData->ScsiAddress.TargetId;

        //
        // Copy the IOCTL parameters to the srb control buffer area.
        //

        RtlMoveMemory(buffer,
                      Irp->AssociatedIrp.SystemBuffer,
                      sizeof(SENDCMDINPARAMS) - 1);

        irp2 = IoBuildDeviceIoControlRequest(IOCTL_SCSI_MINIPORT,
                                            commonExtension->LowerDeviceObject,
                                            srbControl,
                                            sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS) - 1,
                                            srbControl,
                                            sizeof(SRB_IO_CONTROL) + length,
                                            FALSE,
                                            &event,
                                            &ioStatus);

        if (irp2 == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            ExFreePool(srbControl);
            break;
        }

        //
        // Call the port driver with the request and wait for it to complete.
        //

        status = IoCallDriver(commonExtension->LowerDeviceObject, irp2);

        if (status == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
            status = ioStatus.Status;
        }

        //
        // Copy the data received into the output buffer. Since the status buffer
        // contains error information also, always perform this copy. IO will will
        // either pass this back to the app, or zero it, in case of error.
        //

        buffer = (PUCHAR)srbControl;
        (ULONG_PTR)buffer += srbControl->HeaderLength;

        if (NT_SUCCESS(status)) {

            RtlMoveMemory ( Irp->AssociatedIrp.SystemBuffer, buffer, length - 1);
            Irp->IoStatus.Information = length - 1;

        } else {

            RtlMoveMemory ( Irp->AssociatedIrp.SystemBuffer, buffer, (sizeof(SENDCMDOUTPARAMS) - 1));
            Irp->IoStatus.Information = sizeof(SENDCMDOUTPARAMS) - 1;

        }

        ExFreePool(srbControl);
        break;

    }

    case SMART_SEND_DRIVE_COMMAND: {

        PSENDCMDINPARAMS cmdInParameters = ((PSENDCMDINPARAMS)Irp->AssociatedIrp.SystemBuffer);
        PSRB_IO_CONTROL  srbControl;
        ULONG            controlCode = 0;
        PUCHAR           buffer;

        if(!commonExtension->IsFdo) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
               (sizeof(SENDCMDINPARAMS) - 1)) {
                status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
                break;

        } else if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                      (sizeof(SENDCMDOUTPARAMS) - 1)) {
                status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = sizeof(SENDCMDOUTPARAMS) - 1;
                break;
        }

        //
        // Create notification event object to be used to signal the
        // request completion.
        //

        KeInitializeEvent(&event, NotificationEvent, FALSE);

        length = 0;

        if (cmdInParameters->irDriveRegs.bCommandReg == SMART_CMD) {
            switch (cmdInParameters->irDriveRegs.bFeaturesReg) {

                case ENABLE_SMART:
                    controlCode = IOCTL_SCSI_MINIPORT_ENABLE_SMART;
                    break;

                case DISABLE_SMART:
                    controlCode = IOCTL_SCSI_MINIPORT_DISABLE_SMART;
                    break;

                case  RETURN_SMART_STATUS:

                    //
                    // Ensure bBuffer is at least 2 bytes (to hold the values of
                    // cylinderLow and cylinderHigh).
                    //

                    if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                        (sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS))) {

                        status = STATUS_BUFFER_TOO_SMALL;
                        Irp->IoStatus.Information =
                            sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS);
                        break;
                    }

                    controlCode = IOCTL_SCSI_MINIPORT_RETURN_STATUS;
                    length = sizeof(IDEREGS);
                    break;

                case ENABLE_DISABLE_AUTOSAVE:
                    controlCode = IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE;
                    break;

                case SAVE_ATTRIBUTE_VALUES:
                    controlCode = IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES;
                    break;

                case EXECUTE_OFFLINE_DIAGS:
                    controlCode = IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS;
                    break;

                case ENABLE_DISABLE_AUTO_OFFLINE:
                    controlCode = IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE;
                    break;

                default:
                    status = STATUS_INVALID_PARAMETER;
                    break;
            }
        } else {

            status = STATUS_INVALID_PARAMETER;
        }

        if (controlCode == 0) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        length += (sizeof(SENDCMDOUTPARAMS) > sizeof(SENDCMDINPARAMS)) ? sizeof(SENDCMDOUTPARAMS) : sizeof(SENDCMDINPARAMS);;
        srbControl = ExAllocatePoolWithTag(NonPagedPool,
                                           sizeof(SRB_IO_CONTROL) + length,
                                           DISK_TAG_SMART);

        if (!srbControl) {
            status =  STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        //
        // fill in srbControl fields
        //

        srbControl->HeaderLength = sizeof(SRB_IO_CONTROL);
        RtlMoveMemory (srbControl->Signature, "SCSIDISK", 8);
        srbControl->Timeout = fdoExtension->TimeOutValue;
        srbControl->Length = length;

        //
        // Point to the 'buffer' portion of the SRB_CONTROL
        //

        buffer = (PUCHAR)srbControl;
        (ULONG_PTR)buffer += srbControl->HeaderLength;

        //
        // Ensure correct target is set in the cmd parameters.
        //

        cmdInParameters->bDriveNumber = diskData->ScsiAddress.TargetId;

        //
        // Copy the IOCTL parameters to the srb control buffer area.
        //

        RtlMoveMemory(buffer, Irp->AssociatedIrp.SystemBuffer, sizeof(SENDCMDINPARAMS) - 1);

        srbControl->ControlCode = controlCode;

        irp2 = IoBuildDeviceIoControlRequest(IOCTL_SCSI_MINIPORT,
                                            commonExtension->LowerDeviceObject,
                                            srbControl,
                                            sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS) - 1,
                                            srbControl,
                                            sizeof(SRB_IO_CONTROL) + length,
                                            FALSE,
                                            &event,
                                            &ioStatus);

        if (irp2 == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            ExFreePool(srbControl);
            break;
        }

        //
        // Call the port driver with the request and wait for it to complete.
        //

        status = IoCallDriver(commonExtension->LowerDeviceObject, irp2);

        if (status == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
            status = ioStatus.Status;
        }

        //
        // Copy the data received into the output buffer. Since the status buffer
        // contains error information also, always perform this copy. IO will will
        // either pass this back to the app, or zero it, in case of error.
        //

        buffer = (PUCHAR)srbControl;
        (ULONG_PTR)buffer += srbControl->HeaderLength;

        //
        // Update the return buffer size based on the sub-command.
        //

        if (cmdInParameters->irDriveRegs.bFeaturesReg == RETURN_SMART_STATUS) {
            length = sizeof(SENDCMDOUTPARAMS) - 1 + sizeof(IDEREGS);
        } else {
            length = sizeof(SENDCMDOUTPARAMS) - 1;
        }

        RtlMoveMemory ( Irp->AssociatedIrp.SystemBuffer, buffer, length);
        Irp->IoStatus.Information = length;

        ExFreePool(srbControl);
        break;

    }

    case IOCTL_STORAGE_GET_MEDIA_TYPES_EX: {

        PMODE_PARAMETER_BLOCK blockDescriptor;
        ULONG modeLength;
        ULONG retries = 4;
        BOOLEAN writable = FALSE;
        BOOLEAN mediaPresent = FALSE;

        DebugPrint((3,
                   "Disk.DiskDeviceControl: GetMediaTypes\n"));

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(GET_MEDIA_TYPES)) {
            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = sizeof(GET_MEDIA_TYPES);
            break;
        }

        if(!commonExtension->IsFdo) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        //
        // Send a TUR to determine if media is present.
        //

        srb->CdbLength = 6;
        cdb = (PCDB)srb->Cdb;
        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;

        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);


        if (NT_SUCCESS(status)) {
            mediaPresent = TRUE;
        }

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

        //
        // Allocate memory for mode header and block descriptor.
        //

        modeLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK);
        modeData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                         modeLength,
                                         DISK_TAG_MODE_DATA);

        if (modeData == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory(modeData, modeLength);

        //
        // Build the MODE SENSE CDB.
        //

        srb->CdbLength = 6;
        cdb = (PCDB)srb->Cdb;

        //
        // Set timeout value from device extension.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;

        //
        // Page code of 0 will return header and block descriptor only.
        //

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = 0;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)modeLength;

Retry:
        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         modeData,
                                         modeLength,
                                         FALSE);


        if (status == STATUS_VERIFY_REQUIRED) {

            if (retries--) {

                //
                // Retry request.
                //

                goto Retry;
            }
        } else if (SRB_STATUS(srb->SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
            status = STATUS_SUCCESS;
        }

        if (NT_SUCCESS(status) || (status == STATUS_NO_MEDIA_IN_DEVICE)) {

            //
            // Get the block descriptor.
            //

            blockDescriptor = (PMODE_PARAMETER_BLOCK)modeData;
            (ULONG_PTR)blockDescriptor += sizeof(MODE_PARAMETER_HEADER);

            //
            // Do some validation.
            //

            if (modeData->BlockDescriptorLength != sizeof(MODE_PARAMETER_BLOCK)) {

                DebugPrint((1,
                           "DiskDeviceControl: BlockDescriptor length - "
                           "Expected %x, actual %x\n",
                           modeData->BlockDescriptorLength,
                           sizeof(MODE_PARAMETER_BLOCK)));
            }

            DebugPrint((1,
                       "DiskDeviceControl: DensityCode %x, MediumType %x\n",
                       blockDescriptor->DensityCode,
                       modeData->MediumType));

            if (TEST_FLAG(modeData->DeviceSpecificParameter,
                          MODE_DSP_WRITE_PROTECT)) {
                writable = FALSE;
            } else {
                writable = TRUE;
            }

            status = DiskDetermineMediaTypes(DeviceObject,
                                             Irp,
                                             modeData->MediumType,
                                             blockDescriptor->DensityCode,
                                             mediaPresent,
                                             writable);

            //
            // If the buffer was too small, DetermineMediaTypes updated the status and information and the request will fail.
            //

        } else {
            DebugPrint((1,
                       "DiskDeviceControl: Mode sense for header/bd failed. %lx\n",
                       status));
        }

        ExFreePool(modeData);
        break;
    }

    case IOCTL_DISK_GET_DRIVE_GEOMETRY: {

        DebugPrint((2, "IOCTL_DISK_GET_DRIVE_GEOMETRY to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(DISK_GEOMETRY)) {

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = sizeof(DISK_GEOMETRY);
            break;
        }

        if(!commonExtension->IsFdo) {

            //
            // Pdo should issue this request to the lower device object
            //

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        // DiskAcquirePartitioningLock(fdoExtension);

        if (TEST_FLAG(DeviceObject->Characteristics, FILE_REMOVABLE_MEDIA)) {

            //
            // Issue ReadCapacity to update device extension
            // with information for current media.
            //

            status = DiskReadDriveCapacity(commonExtension->PartitionZeroExtension->DeviceObject);

            //
            // Note whether the drive is ready.
            //

            diskData->ReadyStatus = status;

            if (!NT_SUCCESS(status)) {

                // DiskReleasePartitioningLock(fdoExtension);
                break;
            }
        }

        //
        // Copy drive geometry information from device extension.
        //

        RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                      &(fdoExtension->DiskGeometry),
                      sizeof(DISK_GEOMETRY));

        status = STATUS_SUCCESS;
        Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
        // DiskReleasePartitioningLock(fdoExtension);

        break;
    }
    case IOCTL_STORAGE_PREDICT_FAILURE : {

        PSTORAGE_PREDICT_FAILURE checkFailure;
        STORAGE_FAILURE_PREDICT_STATUS diskSmartStatus;

        DebugPrint((2, "IOCTL_STORAGE_PREDICT_FAILURE to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        checkFailure = (PSTORAGE_PREDICT_FAILURE)Irp->AssociatedIrp.SystemBuffer;

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(STORAGE_PREDICT_FAILURE)) {

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = sizeof(STORAGE_PREDICT_FAILURE);
            break;
        }

        if(!commonExtension->IsFdo) {

            //
            // Pdo should issue this request to the lower device object
            //

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        //
        // See if the disk is predicting failure
        //

        if (diskData->FailurePredictionCapability == FailurePredictionSense) {
            ULONG readBufferSize;
            PUCHAR readBuffer;
            PIRP readIrp;
            IO_STATUS_BLOCK ioStatus;
            PDEVICE_OBJECT topOfStack;

            KeInitializeEvent(&event, SynchronizationEvent, FALSE);

            topOfStack = IoGetAttachedDeviceReference(DeviceObject);

            //
            // SCSI disks need to have a read sent down to provoke any
            // failures to be reported.
            //
            // Issue a normal read operation.  The error-handling code in
            // classpnp will take care of a failure prediction by logging the
            // correct event.
            //

            readBufferSize = fdoExtension->DiskGeometry.BytesPerSector;
            readBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                               readBufferSize,
                                               DISK_TAG_SMART);

            if (readBuffer != NULL) {
                LARGE_INTEGER offset;

                offset.QuadPart = 0;
                readIrp = IoBuildSynchronousFsdRequest(
                        IRP_MJ_READ,
                        topOfStack,
                        readBuffer,
                        readBufferSize,
                        &offset,
                        &event,
                        &ioStatus);


                if (readIrp != NULL) {
                    status = IoCallDriver(topOfStack, readIrp);
                    if (status == STATUS_PENDING) {
                        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
                        status = ioStatus.Status;
                    }
                }

                ExFreePool(readBuffer);
            }
            ObDereferenceObject(topOfStack);
        }

        if ((diskData->FailurePredictionCapability == FailurePredictionSmart) ||
            (diskData->FailurePredictionCapability == FailurePredictionSense))
        {
            status = DiskReadFailurePredictStatus(fdoExtension,
                                                  &diskSmartStatus);

            if (NT_SUCCESS(status))
            {
                if (diskSmartStatus.PredictFailure)
                {
                    status = DiskReadFailurePredictData(fdoExtension,
                                           Irp->AssociatedIrp.SystemBuffer);

                    checkFailure->PredictFailure = 1;
                } else {
                    checkFailure->PredictFailure = 0;
                }

                Irp->IoStatus.Information = sizeof(STORAGE_PREDICT_FAILURE);
            }
        } else {
            status = STATUS_INVALID_DEVICE_REQUEST;
        }

        break;
    }

    case IOCTL_DISK_VERIFY: {

        PVERIFY_INFORMATION verifyInfo = Irp->AssociatedIrp.SystemBuffer;
        LARGE_INTEGER byteOffset;
        ULONG         sectorOffset;
        USHORT        sectorCount;

        DebugPrint((2, "IOCTL_DISK_VERIFY to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        //
        // Validate buffer length.
        //

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(VERIFY_INFORMATION)) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        //
        // Verify sectors
        //

        srb->CdbLength = 10;

        cdb->CDB10.OperationCode = SCSIOP_VERIFY;

        //
        // Add disk offset to starting sector.
        //

        byteOffset.QuadPart = commonExtension->StartingOffset.QuadPart +
                              verifyInfo->StartingOffset.QuadPart;

        if(!commonExtension->IsFdo) {

            //
            // Adjust the request and forward it down
            //

            verifyInfo->StartingOffset.QuadPart = byteOffset.QuadPart;

            ClassReleaseRemoveLock(DeviceObject, Irp);
            SendToFdo(DeviceObject, Irp, status);
            ExFreePool(srb);
            return status;
        }

        //
        // Convert byte offset to sector offset.
        //

        sectorOffset = (ULONG)(byteOffset.QuadPart >> fdoExtension->SectorShift);

        //
        // Convert ULONG byte count to USHORT sector count.
        //

        sectorCount = (USHORT)(verifyInfo->Length >> fdoExtension->SectorShift);

        //
        // Move little endian values into CDB in big endian format.
        //

        cdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&sectorOffset)->Byte3;
        cdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&sectorOffset)->Byte2;
        cdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&sectorOffset)->Byte1;
        cdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&sectorOffset)->Byte0;

        cdb->CDB10.TransferBlocksMsb = ((PFOUR_BYTE)&sectorCount)->Byte1;
        cdb->CDB10.TransferBlocksLsb = ((PFOUR_BYTE)&sectorCount)->Byte0;

        //
        // The verify command is used by the NT FORMAT utility and
        // requests are sent down for 5% of the volume size. The
        // request timeout value is calculated based on the number of
        // sectors verified.
        //

        srb->TimeOutValue = ((sectorCount + 0x7F) >> 7) *
                            fdoExtension->TimeOutValue;

        status = ClassSendSrbAsynchronous(DeviceObject,
                                          srb,
                                          Irp,
                                          NULL,
                                          0,
                                          FALSE);

        return(status);

    }

    case IOCTL_DISK_GET_PARTITION_INFO: {

        //
        // Return the information about the partition specified by the device
        // object.  Note that no information is ever returned about the size
        // or partition type of the physical disk, as this doesn't make any
        // sense.
        //

        DebugPrint((1, "IOCTL_DISK_GET_PARTITION_INFO to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((1, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(PARTITION_INFORMATION)) {

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = sizeof(PARTITION_INFORMATION);

        } else {

            PPARTITION_INFORMATION outputBuffer;
            PFUNCTIONAL_DEVICE_EXTENSION p0Extension =
                commonExtension->PartitionZeroExtension;

            PDISK_DATA partitionZeroData =
                ((PDISK_DATA) p0Extension->CommonExtension.DriverData);

            NTSTATUS oldReadyStatus;

            //
            // Update the geometry in case it has changed
            //

            status = DiskReadDriveCapacity(p0Extension->DeviceObject);

            //
            // Note whether the drive is ready.  If the status has changed then
            // notify pnp.
            //

            oldReadyStatus = InterlockedExchange(
                                &(partitionZeroData->ReadyStatus),
                                status);

            if(partitionZeroData->ReadyStatus != oldReadyStatus) {
                IoInvalidateDeviceRelations(p0Extension->LowerPdo,
                                            BusRelations);
            }

            if(!NT_SUCCESS(status)) {
                break;
            }

            //
            // If this is something other than partition 0 then do a
            // re-enumeration to make sure we've got up-to-date information.
            //

            if(commonExtension->PartitionNumber != 0) {
                DiskEnumerateDevice(p0Extension->DeviceObject);
                DiskAcquirePartitioningLock(p0Extension);
            }

            outputBuffer = (PPARTITION_INFORMATION)
                Irp->AssociatedIrp.SystemBuffer;

            outputBuffer->PartitionType = diskData->PartitionType;
            outputBuffer->StartingOffset = commonExtension->StartingOffset;
            outputBuffer->PartitionLength = commonExtension->PartitionLength;
            outputBuffer->HiddenSectors = diskData->HiddenSectors;
            outputBuffer->PartitionNumber = commonExtension->PartitionNumber;
            outputBuffer->BootIndicator = diskData->BootIndicator;
            outputBuffer->RewritePartition = FALSE;
            outputBuffer->RecognizedPartition =
                IsRecognizedPartition(diskData->PartitionType);

            DebugPrint((1, "HiddenSectors from disk data of device %p was "
                           "%#x\n", DeviceObject, diskData->HiddenSectors));

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(PARTITION_INFORMATION);

            if(commonExtension->PartitionNumber != 0) {
                DiskReleasePartitioningLock(p0Extension);
            }
        }

        break;
    }

    case IOCTL_DISK_SET_PARTITION_INFO: {

        DebugPrint((1, "IOCTL_DISK_SET_PARTITION_INFO to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((1, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if(commonExtension->IsFdo) {

            status = STATUS_UNSUCCESSFUL;

        } else {

            PSET_PARTITION_INFORMATION inputBuffer =
                (PSET_PARTITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

            //
            // Validate buffer length
            //

            if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
               sizeof(SET_PARTITION_INFORMATION)) {

                status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            DiskAcquirePartitioningLock(commonExtension->PartitionZeroExtension);

            //
            // The HAL routines IoGet- and IoSetPartitionInformation were
            // developed before support of dynamic partitioning and therefore
            // don't distinguish between partition ordinal (that is the order
            // of a paritition on a disk) and the partition number.  (The
            // partition number is assigned to a partition to identify it to
            // the system.) Use partition ordinals for these legacy calls.
            //

            status = IoSetPartitionInformation(
                        commonExtension->PartitionZeroExtension->CommonExtension.DeviceObject,
                        commonExtension->PartitionZeroExtension->DiskGeometry.BytesPerSector,
                        diskData->PartitionOrdinal,
                        inputBuffer->PartitionType);

            if(NT_SUCCESS(status)) {

                diskData->PartitionType = inputBuffer->PartitionType;
            }

            DiskReleasePartitioningLock(commonExtension->PartitionZeroExtension);
        }

        break;
    }

    case IOCTL_DISK_GET_DRIVE_LAYOUT: {

        DebugPrint((1, "IOCTL_DISK_GET_DRIVE_LAYOUT to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((1, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        //
        // Return the partition layout for the physical drive.  Note that
        // the layout is returned for the actual physical drive, regardless
        // of which partition was specified for the request.
        //

        if (!commonExtension->IsFdo) {

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);

            return status;

        } else {

            PDRIVE_LAYOUT_INFORMATION partitionList;
            PPHYSICAL_DEVICE_EXTENSION pdoExtension = NULL;
            PPARTITION_INFORMATION    partitionEntry;
            ULONG                     tempSize;
            ULONG                     i;

            //
            // Issue a read capacity to update the apparent size of the disk.
            //

            DiskReadDriveCapacity(fdoExtension->DeviceObject);

            DiskAcquirePartitioningLock(fdoExtension);

            //
            // Read partition information.
            //

            status = IoReadPartitionTable(
                        DeviceObject,
                        fdoExtension->DiskGeometry.BytesPerSector,
                        FALSE,
                        &partitionList);

            if (!NT_SUCCESS(status)) {
                DiskReleasePartitioningLock(fdoExtension);
                break;
            }

            //
            // The disk layout has been returned in the partitionList
            // buffer.  Determine its size and, if the data will fit
            // into the intermediate buffer, return it.
            //

            tempSize = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION,PartitionEntry[0]);
            tempSize += partitionList->PartitionCount * sizeof(PARTITION_INFORMATION);

            if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
                tempSize) {

                Irp->IoStatus.Status = status = STATUS_BUFFER_TOO_SMALL;
                Irp->IoStatus.Information = tempSize;

                ExFreePool(partitionList);
                DiskReleasePartitioningLock(fdoExtension);
                break;
            }

            //
            // Update the partition device objects and set valid partition
            // numbers
            //

            ASSERT(diskData->UpdatePartitionRoutine != NULL);
            diskData->UpdatePartitionRoutine(DeviceObject, partitionList);

            //
            // Copy partition information to system buffer.
            //

            RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                          partitionList,
                          tempSize);

            Irp->IoStatus.Information = tempSize;
            Irp->IoStatus.Status = status;

            //
            // Finally, free the buffer allocated by reading the
            // partition table.
            //

            ExFreePool(partitionList);
        }

        DiskReleasePartitioningLock(fdoExtension);

        ClassInvalidateBusRelations(DeviceObject);

        break;
    }

    case IOCTL_DISK_SET_DRIVE_LAYOUT: {

        //
        // Update the disk with new partition information.
        //

        PDRIVE_LAYOUT_INFORMATION partitionList = Irp->AssociatedIrp.SystemBuffer;
        ULONGLONG listSize;

        // BUGBUG - why is listSize a ULONGLONG instead of ULONG_PTR?

        DebugPrint((1, "IOCTL_DISK_SET_DRIVE_LAYOUT to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((1, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        //
        // Validate the input buffer length.
        //

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(DRIVE_LAYOUT_INFORMATION)) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        if(!commonExtension->IsFdo) {

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        DiskAcquirePartitioningLock(fdoExtension);

        listSize = (partitionList->PartitionCount - 1);
        listSize *= sizeof(PARTITION_INFORMATION);

        if ((irpStack->Parameters.DeviceIoControl.InputBufferLength -
             sizeof(DRIVE_LAYOUT_INFORMATION)) <
            listSize) {

            //
            // The remaning size of the input buffer not big enough to
            // hold the additional partition entries
            //

            status = STATUS_INFO_LENGTH_MISMATCH;
            Irp->IoStatus.Information =
                (ULONG_PTR)(listSize + sizeof(DRIVE_LAYOUT_INFORMATION));

            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        listSize += sizeof(DRIVE_LAYOUT_INFORMATION);

        //
        // Redo all the partition numbers in the partition information
        //

        ASSERT(diskData->UpdatePartitionRoutine != NULL);
        diskData->UpdatePartitionRoutine(DeviceObject, partitionList);

        //
        // Write changes to disk.
        //

        status = IoWritePartitionTable(
                    DeviceObject,
                    fdoExtension->DiskGeometry.BytesPerSector,
                    fdoExtension->DiskGeometry.SectorsPerTrack,
                    fdoExtension->DiskGeometry.TracksPerCylinder,
                    partitionList);

        //
        // Update IRP with bytes returned.  Make sure we don't claim to be
        // returning more bytes than the caller is expecting to get back.
        //

        if (NT_SUCCESS(status)) {
            if(listSize <
               (ULONGLONG)irpStack->Parameters.DeviceIoControl.OutputBufferLength
               ) {
                Irp->IoStatus.Information = (ULONG_PTR) listSize;
            } else {
                Irp->IoStatus.Information =
                    irpStack->Parameters.DeviceIoControl.OutputBufferLength;
            }
        }

        DiskReleasePartitioningLock(fdoExtension);
        ClassInvalidateBusRelations(DeviceObject);

        Irp->IoStatus.Status = status;

        break;
    }

    case IOCTL_DISK_DELETE_DRIVE_LAYOUT: {

        //
        // Update the disk with new partition information.
        //

        DebugPrint((1, "IOCTL_DISK_DELETE_DRIVE_LAYOUT to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((1, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if(!commonExtension->IsFdo) {

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        DiskAcquirePartitioningLock(fdoExtension);

        //
        // Write changes to disk.
        //

        status = DiskClearPartitionTable(
                    DeviceObject,
                    fdoExtension->DiskGeometry.BytesPerSector,
                    fdoExtension->DiskGeometry.SectorsPerTrack,
                    fdoExtension->DiskGeometry.TracksPerCylinder);

        DiskReleasePartitioningLock(fdoExtension);
        ClassInvalidateBusRelations(DeviceObject);

        Irp->IoStatus.Status = status;

        break;
    }

    case IOCTL_DISK_REASSIGN_BLOCKS: {

        //
        // Map defective blocks to new location on disk.
        //

        PREASSIGN_BLOCKS badBlocks = Irp->AssociatedIrp.SystemBuffer;
        ULONG bufferSize;
        ULONG blockNumber;
        ULONG blockCount;

        DebugPrint((2, "IOCTL_DISK_REASSIGN_BLOCKS to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        //
        // Validate buffer length.
        //

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(REASSIGN_BLOCKS)) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        //
        // Send to FDO
        //

        if(!commonExtension->IsFdo) {

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        bufferSize = sizeof(REASSIGN_BLOCKS) +
            ((badBlocks->Count - 1) * sizeof(ULONG));

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            bufferSize) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        //
        // Build the data buffer to be transferred in the input buffer.
        // The format of the data to the device is:
        //
        //      2 bytes Reserved
        //      2 bytes Length
        //      x * 4 btyes Block Address
        //
        // All values are big endian.
        //

        badBlocks->Reserved = 0;
        blockCount = badBlocks->Count;

        //
        // Convert # of entries to # of bytes.
        //

        blockCount *= 4;
        badBlocks->Count = (USHORT) ((blockCount >> 8) & 0XFF);
        badBlocks->Count |= (USHORT) ((blockCount << 8) & 0XFF00);

        //
        // Convert back to number of entries.
        //

        blockCount /= 4;

        for (; blockCount > 0; blockCount--) {

            blockNumber = badBlocks->BlockNumber[blockCount-1];

            REVERSE_BYTES((PFOUR_BYTE) &badBlocks->BlockNumber[blockCount-1],
                          (PFOUR_BYTE) &blockNumber);
        }

        srb->CdbLength = 6;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_REASSIGN_BLOCKS;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;

        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         badBlocks,
                                         bufferSize,
                                         TRUE);

        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        ExFreePool(srb);
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);

        return(status);
    }

    case IOCTL_DISK_IS_WRITABLE: {

        DebugPrint((2, "IOCTL_DISK_IS_WRITABLE to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if(!commonExtension->IsFdo) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        //
        // Determine if the device is writable.
        //

        modeData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                         MODE_DATA_SIZE,
                                         DISK_TAG_MODE_DATA);

        if (modeData == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory(modeData, MODE_DATA_SIZE);

        length = ClassModeSense(DeviceObject,
                                (PUCHAR) modeData,
                                MODE_DATA_SIZE,
                                MODE_SENSE_RETURN_ALL);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {

            //
            // Retry the request in case of a check condition.
            //

            length = ClassModeSense(DeviceObject,
                                    (PUCHAR) modeData,
                                    MODE_DATA_SIZE,
                                    MODE_SENSE_RETURN_ALL);

            if (length < sizeof(MODE_PARAMETER_HEADER)) {
                status = STATUS_IO_DEVICE_ERROR;
                ExFreePool(modeData);
                break;
            }
        }

        if (TEST_FLAG(modeData->DeviceSpecificParameter,
                      MODE_DSP_WRITE_PROTECT)) {

            status = STATUS_MEDIA_WRITE_PROTECTED;

        } else {

            status = STATUS_SUCCESS;

        }

        ExFreePool(modeData);
        break;
    }

    case IOCTL_DISK_INTERNAL_SET_VERIFY: {

        //
        // If the caller is kernel mode, set the verify bit.
        //

        if (Irp->RequestorMode == KernelMode) {

            SET_FLAG(DeviceObject->Flags, DO_VERIFY_VOLUME);

            if(commonExtension->IsFdo) {

                Irp->IoStatus.Information = 0;
            }
        }
        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_DISK_INTERNAL_CLEAR_VERIFY: {

        //
        // If the caller is kernel mode, clear the verify bit.
        //

        if (Irp->RequestorMode == KernelMode) {
            CLEAR_FLAG(DeviceObject->Flags, DO_VERIFY_VOLUME);
        }
        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_DISK_UPDATE_DRIVE_SIZE: {

        DebugPrint((2, "IOCTL_DISK_UPDATE_DRIVE_SIZE to device %p "
                       "through irp %p\n",
                    DeviceObject, Irp));

        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(DISK_GEOMETRY)) {

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Status = sizeof(DISK_GEOMETRY);
            break;
        }

        if(!commonExtension->IsFdo) {

            //
            // Pdo should issue this request to the lower device object.
            //

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        DiskAcquirePartitioningLock(fdoExtension);

        //
        // At this point, commonExtension *is* the FDO extension.  This
        // should be the same as PartitionZeroExtension.
        //

        ASSERT(commonExtension ==
               &(commonExtension->PartitionZeroExtension->CommonExtension));

        //
        // Issue ReadCapacity to update device extension with information
        // for current media.
        //

        status = DiskReadDriveCapacity(DeviceObject);

        //
        // Note whether the drive is ready.
        //

        diskData->ReadyStatus = status;

        if(NT_SUCCESS(status)) {

            //
            // Copy drive geometry information from the device extension.
            //

            RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                          &(fdoExtension->DiskGeometry),
                          sizeof(DISK_GEOMETRY));

            Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
            status = STATUS_SUCCESS;

        }

        DiskReleasePartitioningLock(fdoExtension);

        break;
    }

    case IOCTL_DISK_GROW_PARTITION: {

        PDISK_GROW_PARTITION inputBuffer;

        // PDEVICE_OBJECT pdo;
        PCOMMON_DEVICE_EXTENSION pdoExtension;

        LARGE_INTEGER bytesPerCylinder;
        LARGE_INTEGER newStoppingOffset;
        LARGE_INTEGER newPartitionLength;

        PPHYSICAL_DEVICE_EXTENSION sibling;

        PDRIVE_LAYOUT_INFORMATION layoutInfo;
        PPARTITION_INFORMATION pdoPartition;
        PPARTITION_INFORMATION containerPartition;
        ULONG partitionIndex;

        DebugPrint((2, "IOCTL_DISK_GROW_PARTITION to device %p through "
                       "irp %p\n",
                    DeviceObject, Irp));

        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        Irp->IoStatus.Information = 0;

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(DISK_GROW_PARTITION)) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            Irp->IoStatus.Status = sizeof(DISK_GROW_PARTITION);
            break;
        }

        if(!commonExtension->IsFdo) {

            //
            // Pdo should issue this request to the lower device object
            //

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject, Irp, status);
            return status;
        }

        DiskAcquirePartitioningLock(fdoExtension);
        ClassAcquireChildLock(fdoExtension);

        //
        // At this point, commonExtension *is* the FDO extension.  This should
        // be the same as PartitionZeroExtension.
        //

        ASSERT(commonExtension ==
               &(commonExtension->PartitionZeroExtension->CommonExtension));

        //
        // Get the input parameters
        //

        inputBuffer = (PDISK_GROW_PARTITION) Irp->AssociatedIrp.SystemBuffer;

        ASSERT(inputBuffer);

        //
        // Make sure that we are actually being asked to grow the partition.
        //

        if(inputBuffer->BytesToGrow.QuadPart == 0) {

            status = STATUS_INVALID_PARAMETER;
            ClassReleaseChildLock(fdoExtension);
            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        //
        // Find the partition that matches the supplied number
        //

        pdoExtension = &commonExtension->ChildList->CommonExtension;

        while(pdoExtension != NULL) {

            //
            // Is this the partition we are searching for?
            //

            if(inputBuffer->PartitionNumber == pdoExtension->PartitionNumber) {
                break;
            }

            pdoExtension = &pdoExtension->ChildList->CommonExtension;
        }

        // Did we find the partition?

        if(pdoExtension == NULL) {
            status = STATUS_INVALID_PARAMETER;
            ClassReleaseChildLock(fdoExtension);
            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        ASSERT(pdoExtension);

        //
        // Compute the new values for the partition to grow.
        //

        newPartitionLength.QuadPart =
            (pdoExtension->PartitionLength.QuadPart +
             inputBuffer->BytesToGrow.QuadPart);

        newStoppingOffset.QuadPart =
            (pdoExtension->StartingOffset.QuadPart +
             newPartitionLength.QuadPart - 1);

        //
        // Test the partition alignment before getting to involved.
        //
        // NOTE:
        //     All partition stopping offsets should be one byte less
        //     than a cylinder boundary offset. Also, all first partitions
        //     (within partition0 and within an extended partition) start
        //     on the second track while all other partitions start on a
        //     cylinder boundary.
        //
        bytesPerCylinder.QuadPart =
            ((LONGLONG) fdoExtension->DiskGeometry.TracksPerCylinder *
             (LONGLONG) fdoExtension->DiskGeometry.SectorsPerTrack *
             (LONGLONG) fdoExtension->DiskGeometry.BytesPerSector);

        // Temporarily adjust up to cylinder boundary.

        newStoppingOffset.QuadPart += 1;

        if(newStoppingOffset.QuadPart % bytesPerCylinder.QuadPart) {

            // Adjust the length first...
            newPartitionLength.QuadPart -=
                (newStoppingOffset.QuadPart % bytesPerCylinder.QuadPart);

            // ...and then the stopping offset.
            newStoppingOffset.QuadPart -=
                (newStoppingOffset.QuadPart % bytesPerCylinder.QuadPart);

            DebugPrint((2, "IOCTL_DISK_GROW_PARTITION: "
                           "Adjusted the requested partition size to cylinder boundary"));
        }

        // Restore to one byte less than a cylinder boundary.
        newStoppingOffset.QuadPart -= 1;

        //
        // Will the new partition fit within Partition0?
        // Remember: commonExtension == &PartitionZeroExtension->CommonExtension
        //

        if(newStoppingOffset.QuadPart >
            (commonExtension->StartingOffset.QuadPart +
             commonExtension->PartitionLength.QuadPart - 1)) {

            //
            // The new partition falls outside Partition0
            //

            status = STATUS_UNSUCCESSFUL;
            ClassReleaseChildLock(fdoExtension);
            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        //
        // Search for any partition that will conflict with the new partition.
        // This is done before testing for any containing partitions to
        // simplify the container handling.
        //

        sibling = commonExtension->ChildList;

        while(sibling != NULL) {
            LARGE_INTEGER sibStoppingOffset;
            PCOMMON_DEVICE_EXTENSION siblingExtension;

            siblingExtension = &(sibling->CommonExtension);

            ASSERT( siblingExtension );

            sibStoppingOffset.QuadPart =
                (siblingExtension->StartingOffset.QuadPart +
                 siblingExtension->PartitionLength.QuadPart - 1);

            //
            // Only check the siblings that start beyond the new partition
            // starting offset.  Also, assume that since the starting offset
            // has not changed, it will not be in conflict with any other
            // partitions; only the new stopping offset needs to be tested.
            //

            if((inputBuffer->PartitionNumber !=
                siblingExtension->PartitionNumber) &&

               (siblingExtension->StartingOffset.QuadPart >
                pdoExtension->StartingOffset.QuadPart) &&

               (newStoppingOffset.QuadPart >=
                siblingExtension->StartingOffset.QuadPart)) {

                //
                // We have a conflict; bail out leaving pdoSibling set.
                //

                break;
            }
            sibling = siblingExtension->ChildList;
        }

        //
        // If there is a sibling that conflicts, it will be in pdoSibling; there
        // could be more than one, but this is the first one detected.
        //

        if(sibling != NULL) {
            //
            // Report the conflict and abort the grow request.
            //

            status = STATUS_UNSUCCESSFUL;
            ClassReleaseChildLock(fdoExtension);
            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        //
        // Everything looks good, so go ahead and get the on-disk structures
        // to modify.
        //

        status = IoReadPartitionTable(
                    DeviceObject,
                    fdoExtension->DiskGeometry.BytesPerSector,
                    FALSE,
                    &layoutInfo );

        if( !NT_SUCCESS(status) ) {
            ClassReleaseChildLock(fdoExtension);
            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        ASSERT( layoutInfo );

        //
        // Search the layout for the partition that matches the
        // PDO in hand.
        //

        pdoPartition =
            DiskPdoFindPartitionEntry(
                (PPHYSICAL_DEVICE_EXTENSION) pdoExtension,
                layoutInfo);

        if(pdoPartition == NULL) {
            // Looks like something is wrong interally-- error ok?
            status = STATUS_DRIVER_INTERNAL_ERROR;
            ExFreePool(layoutInfo);
            ClassReleaseChildLock(fdoExtension);
            DiskReleasePartitioningLock(fdoExtension);
            break;
        }

        //
        // Search the on-disk partition information to find the root containing
        // partition (top-to-bottom).
        //
        // Remember: commonExtension == &PartitionZeroExtension->CommonExtension
        //

        //
        // All affected containers will have a new stopping offset
        // that is equal to the new partition (logical drive)
        // stopping offset.  Walk the layout information from
        // bottom-to-top searching for logical drive containers and
        // propagating the change.
        //

        containerPartition =
            DiskFindContainingPartition(
                layoutInfo,
                pdoPartition,
                FALSE);

        //
        // This loop should only execute at most 2 times; once for
        // the logical drive container, and once for the root
        // extended partition container.  If the growing partition
        // is not contained, the loop does not run.
        //

        while(containerPartition != NULL) {
            LARGE_INTEGER containerStoppingOffset;
            PPARTITION_INFORMATION nextContainerPartition;

            //
            // Plan ahead and get the container's container before
            // modifing the current size.
            //

            nextContainerPartition =
                DiskFindContainingPartition(
                    layoutInfo,
                    containerPartition,
                    FALSE);

            //
            // Figure out where the current container ends and test
            // to see if it already encompasses the containee.
            //

            containerStoppingOffset.QuadPart =
                (containerPartition->StartingOffset.QuadPart +
                 containerPartition->PartitionLength.QuadPart - 1);

            if(newStoppingOffset.QuadPart <=
               containerStoppingOffset.QuadPart) {

                //
                // No need to continue since this container fits
                //
                break;
            }

            //
            // Adjust the container to have a stopping offset that
            // matches the grown partition stopping offset.
            //

            containerPartition->PartitionLength.QuadPart =
                newStoppingOffset.QuadPart + 1 -
                containerPartition->StartingOffset.QuadPart;

            containerPartition->RewritePartition = TRUE;

            // Continue with the next container
            containerPartition = nextContainerPartition;
        }

        //
        // Wait until after searching the containers to update the
        // partition size.
        //

        pdoPartition->PartitionLength.QuadPart =
            newPartitionLength.QuadPart;

        pdoPartition->RewritePartition = TRUE;

        //
        // Commit the changes to disk
        //

        status = IoWritePartitionTable(
                    DeviceObject,
                    fdoExtension->DiskGeometry.BytesPerSector,
                    fdoExtension->DiskGeometry.SectorsPerTrack,
                    fdoExtension->DiskGeometry.TracksPerCylinder,
                    layoutInfo );

        if( NT_SUCCESS(status) ) {

            //
            // Everything looks good so commit the new length to the
            // PDO.  This has to be done carefully.  We may potentially
            // grow the partition in three steps:
            //  * increase the high-word of the partition length
            //    to be just below the new size - the high word should
            //    be greater than or equal to the current length.
            //
            //  * change the low-word of the partition length to the
            //    new value - this value may potentially be lower than
            //    the current value (if the high part was changed which
            //    is why we changed that first)
            //
            //  * change the high part to the correct value.
            //

            if(newPartitionLength.HighPart >
               pdoExtension->PartitionLength.HighPart) {

                //
                // Swap in one less than the high word.
                //

                InterlockedExchange(
                    &(pdoExtension->PartitionLength.HighPart),
                    (newPartitionLength.HighPart - 1));
            }

            //
            // Swap in the low part.
            //

            InterlockedExchange(
                &(pdoExtension->PartitionLength.LowPart),
                newPartitionLength.LowPart);

            if(newPartitionLength.HighPart !=
               pdoExtension->PartitionLength.HighPart) {

                //
                // Swap in one less than the high word.
                //

                InterlockedExchange(
                    &(pdoExtension->PartitionLength.HighPart),
                    newPartitionLength.HighPart);
            }
        }

        //
        // Free the partition buffer regardless of the status
        //

        ExFreePool( layoutInfo );

        ClassReleaseChildLock(fdoExtension);
        DiskReleasePartitioningLock(fdoExtension);

        break;
    }

    case IOCTL_DISK_MEDIA_REMOVAL: {

        //
        // If the disk is not removable then don't allow this command.
        //

        DebugPrint((2, "IOCTL_DISK_MEDIA_REMOVAL to device %p through irp %p\n",
                    DeviceObject, Irp));
        DebugPrint((2, "Device is a%s.\n",
                    commonExtension->IsFdo ? "n fdo" : " pdo"));

        if(!commonExtension->IsFdo) {
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ExFreePool(srb);
            SendToFdo(DeviceObject,Irp,status);
            return status;
        }

        if (!TEST_FLAG(DeviceObject->Characteristics, FILE_REMOVABLE_MEDIA)) {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        //
        // Fall through and let the class driver process the request.
        //

    }

#if defined(_X86_)
    goto SkipQuerySuggestedLinkName;
    case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME: {
        if (IsNEC_98) {
            //
            //  Try to find a suggested link name of Removable from registry
            // using device object names of NT4 and NT3.51.
            //
            status = DiskQuerySuggestedLinkName(DeviceObject, Irp);
            break;
        }
    }
SkipQuerySuggestedLinkName:
#endif

    default: {

        //
        // Free the Srb, since it is not needed.
        //

        ExFreePool(srb);

        //
        // Pass the request to the common device control routine.
        //

        return(ClassDeviceControl(DeviceObject, Irp));

        break;
    }

    } // end switch( ...

    Irp->IoStatus.Status = status;

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
    }

    ClassReleaseRemoveLock(DeviceObject, Irp);
    ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
    ExFreePool(srb);
    return(status);

} // end DiskDeviceControl()


NTSTATUS
DiskShutdownFlush (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called for a shutdown and flush IRPs.  These are sent by the
    system before it actually shuts down or when the file system does a flush.
    A synchronize cache command is sent to the device if it is write caching.
    If the device is removable an unlock command will be sent. This routine
    will sent a shutdown or flush Srb to the port driver.

Arguments:

    DriverObject - Pointer to device object to being shutdown by system.

    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = commonExtension->PartitionZeroExtension;

    PIO_STACK_LOCATION irpStack;
    PSCSI_REQUEST_BLOCK srb;
    NTSTATUS status;
    PCDB cdb;

    //
    // Send partition flush requests to the FDO
    //

    if(!commonExtension->IsFdo) {

        PDEVICE_OBJECT lowerDevice = commonExtension->LowerDeviceObject;

        ClassReleaseRemoveLock(DeviceObject, Irp);
        IoMarkIrpPending(Irp);
        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoCallDriver(lowerDevice, Irp);
        return STATUS_PENDING;
    }

    //
    // Allocate SCSI request block.
    //

    srb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(SCSI_REQUEST_BLOCK),
                                DISK_TAG_SRB);

    if (srb == NULL) {

        //
        // Set the status and complete the request.
        //

        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        ClassReleaseRemoveLock(DeviceObject, Irp);
        ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set timeout value and mark the request as not being a tagged request.
    //

    srb->TimeOutValue = fdoExtension->TimeOutValue * 4;
    srb->QueueTag = SP_UNTAGGED;
    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
    srb->SrbFlags = fdoExtension->SrbFlags;

    //
    // If the write cache is enabled then send a synchronize cache request.
    //

    if (TEST_FLAG(fdoExtension->DeviceFlags, DEV_WRITE_CACHE)) {

        srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
        srb->CdbLength = 10;

        srb->Cdb[0] = SCSIOP_SYNCHRONIZE_CACHE;

        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         TRUE);

        DebugPrint((1, "DiskShutdownFlush: Synchonize cache sent. Status = %lx\n", status ));
    }

    //
    // Unlock the device if it is removable and this is a shutdown.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    if (TEST_FLAG(DeviceObject->Characteristics, FILE_REMOVABLE_MEDIA) &&
        irpStack->MajorFunction == IRP_MJ_SHUTDOWN) {

        srb->CdbLength = 6;
        cdb = (PVOID) srb->Cdb;
        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
        cdb->MEDIA_REMOVAL.Prevent = FALSE;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = fdoExtension->TimeOutValue;
        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         TRUE);

        DebugPrint((1, "DiskShutdownFlush: Unlock device request sent. Status = %lx\n", status ));
    }

    srb->CdbLength = 0;

    //
    // Save a few parameters in the current stack location.
    //

    srb->Function = irpStack->MajorFunction == IRP_MJ_SHUTDOWN ?
        SRB_FUNCTION_SHUTDOWN : SRB_FUNCTION_FLUSH;

    //
    // Set the retry count to zero.
    //

    irpStack->Parameters.Others.Argument4 = (PVOID) 0;

    //
    // Set up IoCompletion routine address.
    //

    IoSetCompletionRoutine(Irp, ClassIoComplete, srb, TRUE, TRUE, TRUE);

    //
    // Get next stack location and
    // set major function code.
    //

    irpStack = IoGetNextIrpStackLocation(Irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Set up SRB for execute scsi request.
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Scsi.Srb = srb;

    //
    // Set up Irp Address.
    //

    srb->OriginalRequest = Irp;

    //
    // Call the port driver to process the request.
    //

    IoMarkIrpPending(Irp);
    IoCallDriver(commonExtension->LowerDeviceObject, Irp);
    return STATUS_PENDING;
} // end DiskShutdown()


NTSTATUS
DiskModeSelect(
    IN PDEVICE_OBJECT Fdo,
    IN PCHAR ModeSelectBuffer,
    IN ULONG Length,
    IN BOOLEAN SavePage
    )

/*++

Routine Description:

    This routine sends a mode select command.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    ModeSelectBuffer - Supplies a buffer containing the page data.

    Length - Supplies the length in bytes of the mode select buffer.

    SavePage - Indicates that parameters should be written to disk.

Return Value:

    Length of the transferred data is returned.

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCDB cdb;
    SCSI_REQUEST_BLOCK srb;
    ULONG retries = 1;
    ULONG length2;
    NTSTATUS status;
    PULONG buffer;
    PMODE_PARAMETER_BLOCK blockDescriptor;

    PAGED_CODE();

    ASSERT_FDO(Fdo);

    length2 = Length + sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK);

    //
    // Allocate buffer for mode select header, block descriptor, and mode page.
    //

    buffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                   length2,
                                   DISK_TAG_MODE_DATA);

    if(buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(buffer, length2);

    //
    // Set length in header to size of mode page.
    //

    ((PMODE_PARAMETER_HEADER)buffer)->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

    (PULONG)blockDescriptor = (buffer + 1);

    //
    // Set size
    //

    blockDescriptor->BlockLength[1]=0x02;

    //
    // Copy mode page to buffer.
    //

    RtlCopyMemory(buffer + 3, ModeSelectBuffer, Length);

    //
    // Zero SRB.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Build the MODE SELECT CDB.
    //

    srb.CdbLength = 6;
    cdb = (PCDB)srb.Cdb;

    //
    // Set timeout value from device extension.
    //

    srb.TimeOutValue = fdoExtension->TimeOutValue * 2;

    cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
    cdb->MODE_SELECT.SPBit = SavePage;
    cdb->MODE_SELECT.PFBit = 1;
    cdb->MODE_SELECT.ParameterListLength = (UCHAR)(length2);

Retry:

    status = ClassSendSrbSynchronous(Fdo,
                                     &srb,
                                     buffer,
                                     length2,
                                     TRUE);

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // Routine ClassSendSrbSynchronous does not retry requests returned with
        // this status.
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

    ExFreePool(buffer);

    return status;
} // end DiskModeSelect()


VOID
DisableWriteCache(
    IN PDEVICE_OBJECT Fdo,
    IN PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor
    )

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PUCHAR                     vendorId = (PUCHAR) DeviceDescriptor + DeviceDescriptor->VendorIdOffset;
    PUCHAR                     productId = (PUCHAR) DeviceDescriptor + DeviceDescriptor->ProductIdOffset;
    PUCHAR                     productRevision = (PUCHAR) DeviceDescriptor + DeviceDescriptor->ProductRevisionOffset;

    ULONG                      j,length;

    PAGED_CODE();

    //
    // ScanForSpecial is done in the disk's Init routine, so the flag will
    // be set by this point
    //

    if (TEST_FLAG(fdoExtension->ScanForSpecialFlags,
                  CLASS_SPECIAL_DISABLE_WRITE_CACHE)) {

        DISK_CACHE_INFORMATION cacheInfo;
        NTSTATUS status;

        DebugPrint((1, "Disk.DisableWriteCache: Disabling Write Cache\n"));

        status = DiskGetCacheInformation(fdoExtension, &cacheInfo);

        if(NT_SUCCESS(status) && (cacheInfo.WriteCacheEnabled == TRUE)) {
            cacheInfo.WriteCacheEnabled = FALSE;
            DiskSetCacheInformation(fdoExtension, &cacheInfo);
        }

    }

    return;
}


VOID
DiskFdoProcessError(
    PDEVICE_OBJECT Fdo,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )
/*++

Routine Description:

   This routine checks the type of error.  If the error indicates an underrun
   then indicate the request should be retried.

Arguments:

    Fdo - Supplies a pointer to the functional device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - Status with which the IRP will be completed.

    Retry - Indication of whether the request will be retried.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    ASSERT(fdoExtension->CommonExtension.IsFdo);

    if (*Status == STATUS_DATA_OVERRUN &&
        ( Srb->Cdb[0] == SCSIOP_WRITE || Srb->Cdb[0] == SCSIOP_READ)) {

            *Retry = TRUE;

            //
            // Update the error count for the device.
            //

            fdoExtension->ErrorCount++;
    }

    if (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_ERROR &&
        Srb->ScsiStatus == SCSISTAT_BUSY) {

        //
        // The disk drive should never be busy this long. Reset the scsi bus
        // maybe this will clear the condition.
        //

        ResetBus(Fdo);

        //
        // Update the error count for the device.
        //

        fdoExtension->ErrorCount++;
    }
}


VOID
ScanForSpecial(
    PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This function checks to see if an SCSI logical unit requires speical
    flags to be set.

Arguments:

    Fdo - Supplies the device object to be tested.

    InquiryData - Supplies the inquiry data returned by the device of interest.

    AdapterDescriptor - Supplies the capabilities of the device object.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    BAD_CONTROLLER_INFORMATION const *controller;

    PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor = fdoExtension->DeviceDescriptor;

    PUCHAR vendorId =
        ((PUCHAR) deviceDescriptor) + deviceDescriptor->VendorIdOffset;

    PUCHAR productId =
        ((PUCHAR) deviceDescriptor) + deviceDescriptor->ProductIdOffset;

    PUCHAR productRevision =
        ((PUCHAR) deviceDescriptor) + deviceDescriptor->ProductRevisionOffset;

    ULONG                      j,length;

    PVOID                      modeData;
    PUCHAR                     pageData;

    PAGED_CODE();

    for (j = 0; DiskBadControllers[j].VendorId != NULL; j++) {

        controller = &DiskBadControllers[j];

        if (strncmp(controller->VendorId,
                    vendorId,
                    strlen(controller->VendorId))) {
            continue;
        }

        if ((controller->ProductId != NULL) &&
            strncmp(controller->ProductId,
                    productId,
                    strlen(controller->ProductId))) {
            continue;
        }

        if ((controller->ProductRevision != NULL) &&
            strncmp(controller->ProductRevision,
                    productRevision,
                    strlen(controller->ProductRevision))) {
            continue;
        }

        DebugPrint((1, "Disk ScanForSpecial, Found bad controller! "
                    "Ven: %s Prod: %s Rev: %s\n",
                    vendorId, productId, productRevision));

        //
        // Found a listed controller.  Determine what must be done.
        //

        if (controller->DisableTaggedQueuing) {

            //
            // Disable tagged queuing.
            //

            CLEAR_FLAG(fdoExtension->SrbFlags, SRB_FLAGS_QUEUE_ACTION_ENABLE);
        }

        if (controller->DisableSynchronousTransfers) {

            //
            // Disable synchronous data transfers.
            //

            SET_FLAG(fdoExtension->SrbFlags, SRB_FLAGS_DISABLE_SYNCH_TRANSFER);

        }

        if (controller->DisableSpinDown) {

            //
            // Disable spinning down of drives.
            //

            SET_FLAG(fdoExtension->ScanForSpecialFlags,
                     CLASS_SPECIAL_DISABLE_SPIN_DOWN);

        }

        if (controller->DisableWriteCache) {

            //
            // Disable the drive's write cache
            //

            SET_FLAG(fdoExtension->ScanForSpecialFlags,
                     CLASS_SPECIAL_DISABLE_WRITE_CACHE);

        }

        if (controller->CauseNotReportableHack) {

            SET_FLAG(fdoExtension->ScanForSpecialFlags,
                     CLASS_SPECIAL_CAUSE_NOT_REPORTABLE_HACK);
        }

        //
        // Found device so exit the loop and return.
        //

        break;
    }

    //
    // Set the StartUnit flag appropriately.
    //

    SET_FLAG(fdoExtension->DeviceFlags, DEV_SAFE_START_UNIT);

    if (TEST_FLAG(Fdo->Characteristics, FILE_REMOVABLE_MEDIA)) {

        //
        // this is a list of vendors who require the START_UNIT command
        //

        if        ( _strnicmp(vendorId,  "iomega",   strlen("iomega")) == 0) {
            DebugPrint((1, "DiskScanForSpecial (%p) => This unit requires "
                        " START_UNITS\n", Fdo));
        } else if ((_strnicmp(vendorId,  "hp      ", strlen("hp      ")) == 0) &&
                   (_strnicmp(productId, "C1113F  ", strlen("C1113F  ")) == 0)
                   ) {
            DebugPrint((1, "DiskScanForSpecial (%p) => This unit requires "
                        " START_UNITS\n", Fdo));
        } else {
            CLEAR_FLAG(fdoExtension->DeviceFlags, DEV_SAFE_START_UNIT);
        }

    }

    return;
}


VOID
ResetBus(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This command sends a reset bus command to the SCSI port driver.

Arguments:

    Fdo - The functional device object for the logical unit with hardware problem.

Return Value:

    None.

--*/
{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCOMPLETION_CONTEXT context;

    DebugPrint((1, "Disk ResetBus: Sending reset bus request to port driver.\n"));

    //
    // Allocate Srb from nonpaged pool.
    //

    context = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                    sizeof(COMPLETION_CONTEXT),
                                    DISK_TAG_CCONTEXT);

    if(context == NULL) {
        return;
    }

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

    srb->Function = SRB_FUNCTION_RESET_BUS;

    //
    // Build the asynchronous request to be sent to the port driver.
    // Since this routine is called from a DPC the IRP should always be
    // available.
    //

    irp = IoAllocateIrp(Fdo->StackSize, FALSE);

    if(irp == NULL) {
        ExFreePool(context);
        return;
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

} // end ResetBus()


NTSTATUS
DiskQueryPnpCapabilities(
    IN PDEVICE_OBJECT DeviceObject,
    IN PDEVICE_CAPABILITIES Capabilities
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = commonExtension->DriverData;

    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(Capabilities);

    if(commonExtension->IsFdo) {
        return STATUS_NOT_IMPLEMENTED;
    } else {

        PPHYSICAL_DEVICE_EXTENSION physicalExtension =
            DeviceObject->DeviceExtension;

        Capabilities->SilentInstall = 1;
        Capabilities->RawDeviceOK = 1;
        Capabilities->Address = commonExtension->PartitionNumber;

        if(!TEST_FLAG(DeviceObject->Characteristics, FILE_REMOVABLE_MEDIA)) {

            //
            // Media's not removable, deviceId/DeviceInstance should be
            // globally unique.
            //

            Capabilities->UniqueID = 1;
        } else {
            Capabilities->UniqueID = 0;
        }
    }

    return STATUS_SUCCESS;
}


NTSTATUS
DiskGetCacheInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PDISK_CACHE_INFORMATION CacheInfo
    )
{
    PMODE_PARAMETER_HEADER modeData;
    PMODE_CACHING_PAGE pageData;

    ULONG length;

    NTSTATUS status;

    PAGED_CODE();

    modeData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                     MODE_DATA_SIZE,
                                     DISK_TAG_DISABLE_CACHE);

    if (modeData == NULL) {

        DebugPrint((1, "DiskGetSetCacheInformation: Unable to allocate mode "
                       "data buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ClassModeSense(FdoExtension->DeviceObject,
                            (PUCHAR) modeData,
                            MODE_DATA_SIZE,
                            MODE_SENSE_RETURN_ALL);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {

        //
        // Retry the request in case of a check condition.
        //

        length = ClassModeSense(FdoExtension->DeviceObject,
                                (PUCHAR) modeData,
                                MODE_DATA_SIZE,
                                MODE_SENSE_RETURN_ALL);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {


            DebugPrint((1, "Disk.DisableWriteCache: Mode Sense failed\n"));

            ExFreePool(modeData);
            return STATUS_IO_DEVICE_ERROR;
        }
    }

    //
    // If the length is greater than length indicated by the mode data reset
    // the data to the mode data.
    //

    if (length > (ULONG) (modeData->ModeDataLength + 1)) {
        length = modeData->ModeDataLength + 1;
    }

    //
    // Check to see if the write cache is enabled.
    //

    pageData = ClassFindModePage((PUCHAR) modeData,
                                 length,
                                 MODE_PAGE_CACHING,
                                 TRUE);

    //
    // Check if valid caching page exists.
    //

    if (pageData == NULL) {
        ExFreePool(modeData);
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Copy the parameters over.
    //

    RtlZeroMemory(CacheInfo, sizeof(DISK_CACHE_INFORMATION));

    CacheInfo->ParametersSavable = pageData->PageSavable;

    CacheInfo->ReadCacheEnabled = !(pageData->ReadDisableCache);
    CacheInfo->WriteCacheEnabled = pageData->WriteCacheEnable;

    CacheInfo->ReadRetentionPriority = pageData->ReadRetensionPriority;
    CacheInfo->WriteRetentionPriority = pageData->WriteRetensionPriority;

    CacheInfo->DisablePrefetchTransferLength =
        ((pageData->DisablePrefetchTransfer[0] << 8) +
         pageData->DisablePrefetchTransfer[1]);

    CacheInfo->ScalarPrefetch.Minimum =
        ((pageData->MinimumPrefetch[0] << 8) + pageData->MinimumPrefetch[1]);

    CacheInfo->ScalarPrefetch.Maximum =
        ((pageData->MaximumPrefetch[0] << 8) + pageData->MaximumPrefetch[1]);

    if(pageData->MultiplicationFactor) {
        CacheInfo->PrefetchScalar = TRUE;
        CacheInfo->ScalarPrefetch.MaximumBlocks =
            ((pageData->MaximumPrefetchCeiling[0] << 8) +
             pageData->MaximumPrefetchCeiling[1]);
    }

    ExFreePool(modeData);
    return STATUS_SUCCESS;
}


NTSTATUS
DiskSetCacheInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PDISK_CACHE_INFORMATION CacheInfo
    )

{
    PMODE_PARAMETER_HEADER modeData;
    ULONG length;

    PMODE_CACHING_PAGE pageData;

    ULONG i;

    ULONG errorCode;
    NTSTATUS status;

    PAGED_CODE();

    modeData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                     MODE_DATA_SIZE,
                                     DISK_TAG_DISABLE_CACHE);

    if (modeData == NULL) {

        DebugPrint((1, "DiskSetCacheInformation: Unable to allocate mode "
                       "data buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ClassModeSense(FdoExtension->DeviceObject,
                            (PUCHAR) modeData,
                            MODE_DATA_SIZE,
                            MODE_PAGE_CACHING);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {

        //
        // Retry the request in case of a check condition.
        //

        length = ClassModeSense(FdoExtension->DeviceObject,
                                (PUCHAR) modeData,
                                MODE_DATA_SIZE,
                                MODE_PAGE_CACHING);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {


            DebugPrint((1, "Disk.DisableWriteCache: Mode Sense failed\n"));

            ExFreePool(modeData);
            return STATUS_IO_DEVICE_ERROR;
        }
    }

    //
    // If the length is greater than length indicated by the mode data reset
    // the data to the mode data.
    //

    if (length > (ULONG) (modeData->ModeDataLength + 1)) {
        length = modeData->ModeDataLength + 1;
    }

    //
    // Check to see if the write cache is enabled.
    //

    pageData = ClassFindModePage((PUCHAR) modeData,
                                 length,
                                 MODE_PAGE_CACHING,
                                 TRUE);

    //
    // Check if valid caching page exists.
    //

    if (pageData == NULL) {
        ExFreePool(modeData);
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Don't touch any of the normal parameters - not all drives actually
    // use the correct size of caching mode page.  Just change the things
    // which the user could have modified.
    //

    pageData->PageSavable = FALSE;

    pageData->ReadDisableCache = !(CacheInfo->ReadCacheEnabled);
    pageData->MultiplicationFactor = CacheInfo->PrefetchScalar;
    pageData->WriteCacheEnable = CacheInfo->WriteCacheEnabled;

    pageData->WriteRetensionPriority = (UCHAR) CacheInfo->WriteRetentionPriority;
    pageData->ReadRetensionPriority = (UCHAR) CacheInfo->ReadRetentionPriority;

    pageData->DisablePrefetchTransfer[0] =
        (UCHAR) (CacheInfo->DisablePrefetchTransferLength >> 8);
    pageData->DisablePrefetchTransfer[1] =
        (UCHAR) (CacheInfo->DisablePrefetchTransferLength & 0x00ff);

    pageData->MinimumPrefetch[0] =
        (UCHAR) (CacheInfo->ScalarPrefetch.Minimum >> 8);
    pageData->MinimumPrefetch[1] =
        (UCHAR) (CacheInfo->ScalarPrefetch.Minimum & 0x00ff);

    pageData->MaximumPrefetch[0] =
        (UCHAR) (CacheInfo->ScalarPrefetch.Maximum >> 8);
    pageData->MaximumPrefetch[1] =
        (UCHAR) (CacheInfo->ScalarPrefetch.Maximum & 0x00ff);

    if(pageData->MultiplicationFactor) {

        pageData->MaximumPrefetchCeiling[0] =
            (UCHAR) (CacheInfo->ScalarPrefetch.MaximumBlocks >> 8);
        pageData->MaximumPrefetchCeiling[1] =
            (UCHAR) (CacheInfo->ScalarPrefetch.MaximumBlocks & 0x00ff);
    }

    //
    // We will attempt (twice) to issue the mode select with the page.
    //

    //
    // First save away the current state of the disk cache so we know what to
    // log if the request fails.
    //

    if(TEST_FLAG(FdoExtension->DeviceFlags, DEV_WRITE_CACHE)) {
        errorCode = IO_WRITE_CACHE_ENABLED;
    } else {
        errorCode = IO_WRITE_CACHE_DISABLED;
    }

    for(i = 0; i < 2; i++) {
        status = DiskModeSelect(FdoExtension->DeviceObject,
                                (PUCHAR) pageData,
                                (pageData->PageLength + 2),
                                CacheInfo->ParametersSavable);

        if(NT_SUCCESS(status)) {
            if(CacheInfo->WriteCacheEnabled) {
                SET_FLAG(FdoExtension->DeviceFlags, DEV_WRITE_CACHE);
                errorCode = IO_WRITE_CACHE_ENABLED;
            } else {
                CLEAR_FLAG(FdoExtension->DeviceFlags, DEV_WRITE_CACHE);
                errorCode = IO_WRITE_CACHE_DISABLED;
            }

            break;
        }
    }

    {
        PIO_ERROR_LOG_PACKET logEntry;

        //
        // Log the appropriate informational or error entry.
        //

        logEntry = IoAllocateErrorLogEntry(
                        FdoExtension->DeviceObject,
                        sizeof(IO_ERROR_LOG_PACKET) + (4 * sizeof(ULONG)));

        if (logEntry != NULL) {

            PDISK_DATA diskData = FdoExtension->CommonExtension.DriverData;

            logEntry->FinalStatus       = status;
            logEntry->ErrorCode         = errorCode;
            logEntry->SequenceNumber    = 0;
            logEntry->MajorFunctionCode = IRP_MJ_SCSI;
            logEntry->IoControlCode     = 0;
            logEntry->RetryCount        = 0;
            logEntry->UniqueErrorValue  = 0x1;
            logEntry->DumpDataSize      = 4;

            logEntry->DumpData[0] = diskData->ScsiAddress.PathId;
            logEntry->DumpData[1] = diskData->ScsiAddress.TargetId;
            logEntry->DumpData[2] = diskData->ScsiAddress.Lun;
            logEntry->DumpData[3] = CacheInfo->WriteCacheEnabled;

            //
            // Write the error log packet.
            //

            IoWriteErrorLogEntry(logEntry);
        }
    }

    ExFreePool(modeData);
    return status;
}


PPARTITION_INFORMATION
DiskPdoFindPartitionEntry(
    IN PPHYSICAL_DEVICE_EXTENSION Pdo,
    IN PDRIVE_LAYOUT_INFORMATION LayoutInfo
    )

{
    PCOMMON_DEVICE_EXTENSION commonExtension= &(Pdo->CommonExtension);
    ULONG partitionIndex;

    PAGED_CODE();


    DebugPrint((1, "DiskPdoFindPartitionEntry: Searching layout for "
                   "matching partition.\n"));

    for(partitionIndex = 0;
        partitionIndex < LayoutInfo->PartitionCount;
        partitionIndex++) {

        PPARTITION_INFORMATION partitionInfo;

        //
        // Get the partition entry
        //

        partitionInfo = &LayoutInfo->PartitionEntry[partitionIndex];

        //
        // See if it is the one we are looking for...
        //

        if((partitionInfo->PartitionType != PARTITION_ENTRY_UNUSED) &&
           (!IsContainerPartition(partitionInfo->PartitionType)) &&
           (commonExtension->StartingOffset.QuadPart ==
            partitionInfo->StartingOffset.QuadPart) &&
           (commonExtension->PartitionLength.QuadPart ==
            partitionInfo->PartitionLength.QuadPart)) {

            //
            // Found it!
            //

            DebugPrint((1, "DiskPdoFindPartitionEntry: Found matching "
                           "partition.\n"));
            return partitionInfo;
        }
    }

    return NULL;
}


PPARTITION_INFORMATION
DiskFindAdjacentPartition(
    IN PDRIVE_LAYOUT_INFORMATION LayoutInfo,
    IN PPARTITION_INFORMATION BasePartition
    )
{
    ULONG partitionIndex;
    LONGLONG baseStoppingOffset;
    LONGLONG adjacentStartingOffset;
    PPARTITION_INFORMATION adjacentPartition = 0;

    ASSERT(LayoutInfo && BasePartition);

    PAGED_CODE();

    DebugPrint((1, "DiskPdoFindAdjacentPartition: Searching layout for adjacent partition.\n"));

    //
    // Construct the base stopping offset for comparison
    //

    baseStoppingOffset = (BasePartition->StartingOffset.QuadPart +
                          BasePartition->PartitionLength.QuadPart -
                          1);

    adjacentStartingOffset = MAXLONGLONG;

    for(partitionIndex = 0;
        partitionIndex < LayoutInfo->PartitionCount;
        partitionIndex++) {

        PPARTITION_INFORMATION partitionInfo;

        //
        // Get the partition entry
        //

        partitionInfo = &LayoutInfo->PartitionEntry[partitionIndex];

        //
        // See if it is the one we are looking for...
        //

        if((partitionInfo->PartitionType != PARTITION_ENTRY_UNUSED) &&
           (partitionInfo->StartingOffset.QuadPart > baseStoppingOffset) &&
           (partitionInfo->StartingOffset.QuadPart < adjacentStartingOffset)) {

            // Found a closer neighbor...update and remember.
            adjacentPartition = partitionInfo;

            adjacentStartingOffset = adjacentPartition->StartingOffset.QuadPart;

            DebugPrint((1, "DiskPdoFindAdjacentPartition: Found adjacent "
                           "partition.\n"));
        }
    }
    return adjacentPartition;
}


PPARTITION_INFORMATION
DiskFindContainingPartition(
    IN PDRIVE_LAYOUT_INFORMATION LayoutInfo,
    IN PPARTITION_INFORMATION BasePartition,
    IN BOOLEAN SearchTopToBottom
    )

{

    LONG partitionIndex;
    LONG startIndex;
    LONG stopIndex;
    LONG stepIndex;

    LONGLONG baseStoppingOffset;
    LONGLONG containerStoppingOffset;

    PPARTITION_INFORMATION partitionInfo = 0;
    PPARTITION_INFORMATION containerPartition = 0;

    PAGED_CODE();

    ASSERT( LayoutInfo && BasePartition);

    DebugPrint((1, "DiskFindContainingPartition: Searching for extended partition.\n"));

    if( LayoutInfo->PartitionCount != 0) {

        baseStoppingOffset = (BasePartition->StartingOffset.QuadPart +
                              BasePartition->PartitionLength.QuadPart - 1);

        //
        // Determine the search direction and setup the loop
        //
        if(SearchTopToBottom == TRUE) {

            startIndex = 0;
            stopIndex = LayoutInfo->PartitionCount;
            stepIndex = +1;
        } else {
            startIndex = LayoutInfo->PartitionCount - 1;
            stopIndex = -1;
            stepIndex = -1;
        }

        //
        // Using the loop parameters, walk the layout information and
        // return the first containing partition.
        //

        for(partitionIndex = startIndex;
            partitionIndex != stopIndex;
            partitionIndex += stepIndex) {

            //
            // Get the next partition entry
            //

            partitionInfo = &LayoutInfo->PartitionEntry[partitionIndex];

            containerStoppingOffset = (partitionInfo->StartingOffset.QuadPart +
                                       partitionInfo->PartitionLength.QuadPart -
                                       1);

            //
            // Search for a containing partition without detecting the
            // same partition as a container of itself.  The starting
            // offset of a partition and its container should never be
            // the same; however, the stopping offset can be the same.
            //

            if((IsContainerPartition(partitionInfo->PartitionType)) &&

               (BasePartition->StartingOffset.QuadPart >
                partitionInfo->StartingOffset.QuadPart) &&

               (baseStoppingOffset <= containerStoppingOffset)) {

                containerPartition = partitionInfo;

                DebugPrint((1, "DiskFindContainingPartition: Found a "
                               "containing extended partition.\n"));

                break;
            }
        }
    }
    return containerPartition;
}


NTSTATUS
DiskGetInfoExceptionInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    OUT PBOOLEAN LogErr,
    OUT PBOOLEAN SupportInfoException
    )
{
    PMODE_PARAMETER_HEADER modeData;
    PMODE_INFO_EXCEPTIONS pageData;

    ULONG length;

    NTSTATUS status;

    PAGED_CODE();

    *SupportInfoException = FALSE;

    //
    // CONSIDER: Why are we so sure that MODE_DATA_SIZE = c0 ??
    //

    modeData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                     MODE_DATA_SIZE,
                                     DISK_TAG_INFO_EXCEPTION);

    if (modeData == NULL) {

        DebugPrint((1, "DiskGetInfoExceptionInformation: Unable to allocate mode "
                       "data buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ClassModeSense(FdoExtension->DeviceObject,
                            (PUCHAR) modeData,
                            MODE_DATA_SIZE,
                            MODE_PAGE_FAULT_REPORTING);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {

        //
        // Retry the request in case of a check condition.
        //

        length = ClassModeSense(FdoExtension->DeviceObject,
                                (PUCHAR) modeData,
                                MODE_DATA_SIZE,
                                MODE_PAGE_FAULT_REPORTING);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {


            DebugPrint((1, "Disk.DisableWriteCache: Mode Sense failed\n"));

            ExFreePool(modeData);
            return STATUS_IO_DEVICE_ERROR;
        }
    }

    //
    // If the length is greater than length indicated by the mode data reset
    // the data to the mode data.
    //

    if (length > (ULONG) (modeData->ModeDataLength + 1)) {
        length = modeData->ModeDataLength + 1;
    }

    //
    // Check to see if the write cache is enabled.
    //

    pageData = ClassFindModePage((PUCHAR) modeData,
                                 length,
                                 MODE_PAGE_FAULT_REPORTING,
                                 TRUE);

    //
    // Check if valid info exceptions page exists.
    // CONSIDER: If mode page already setup correctly, don't need to reset it
    //

    if (pageData != NULL) {
        *SupportInfoException = TRUE;
        *LogErr = ((PMODE_INFO_EXCEPTIONS)modeData)->LogErr;
        status =  STATUS_SUCCESS;
    } else {
        status = STATUS_NOT_SUPPORTED;
    }

    DebugPrint((3, "DiskGetInfoExceptionInformation: %s support SMART for device %x\n",
                  NT_SUCCESS(status) ? "does" : "does not",
                  FdoExtension->DeviceObject));


    ExFreePool(modeData);
    return(status);
}


NTSTATUS
DiskSetInfoExceptionInformation(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN BOOLEAN Enable,
    IN BOOLEAN AllowPerfHit,
    IN BOOLEAN InduceFailure
    )

{
    PMODE_INFO_EXCEPTIONS pageData;
    ULONG i;
    ULONG errorCode;
    NTSTATUS status;
    BOOLEAN logErr, supportsFP;

    PAGED_CODE();

    status = DiskGetInfoExceptionInformation(FdoExtension,
                                             &logErr,
                                             &supportsFP);

    if (NT_SUCCESS(status))
    {
        //
        // Allocate the page data from cache-aligned memory.
        //

        pageData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                         sizeof(MODE_INFO_EXCEPTIONS),
                                         DISK_TAG_INFO_EXCEPTION);

        if(pageData == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(pageData, sizeof(MODE_INFO_EXCEPTIONS));

        //
        // Configure INFO EXCEPTIONS to be reported via method 4, which is ...
        //

        pageData->PageCode = MODE_PAGE_FAULT_REPORTING;
        pageData->PageLength = sizeof(MODE_INFO_EXCEPTIONS) - 2;

        pageData->PSBit = 0;
        pageData->LogErr = logErr;
        pageData->Dexcpt = Enable ? 0 : 1;

        pageData->Test = InduceFailure ? 1 : 0;

        pageData->ReportMethod = Enable ? 3 : 0; // TODO: Make symbolic constants

        pageData->Perf = AllowPerfHit ? 0 : 1;   // Allow performance degredation

//        pageData->IntervalTimer = 0;      // Report only once
        pageData->ReportCount[3] = 1;       // Report only once

        //
        // We will attempt (twice) to issue the mode select with the page.
        // Make the setting persistant so that we don't have to turn it back
        // on after a bus reset.
        //

        for(i = 0; i < 2; i++) {
            status = DiskModeSelect(FdoExtension->DeviceObject,
                                    (PUCHAR) pageData,
                                    sizeof(MODE_INFO_EXCEPTIONS),
                                    TRUE);

        }

        //
        // If method 3 does not work then lets try method 4. The WD Enterprise
        // only supports method 4 and not 3
        //

        if (! NT_SUCCESS(status)) {
            pageData->ReportMethod = Enable ? 4 : 0;
            for(i = 0; i < 2; i++) {
                status = DiskModeSelect(FdoExtension->DeviceObject,
                                        (PUCHAR) pageData,
                                        sizeof(MODE_INFO_EXCEPTIONS),
                                        TRUE);
            }
        }
        ExFreePool(pageData);
    }

    DebugPrint((3, "DiskSetInfoExceptionInformation: %s %s for device %p\n",
                        Enable ? "Enable" : "Disable",
                        NT_SUCCESS(status) ? "succeeded" : "failed",
                        FdoExtension->DeviceObject));

    return status;
}


NTSTATUS
DiskClearPartitionTable(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG SectorsPerTrack,
    IN ULONG NumberOfHeads
    )

/*++

Routine Description:

    This routine walks the disk writing the partition tables from
    the entries in the partition list buffer for each partition.

    Applications that create and delete partitions should issue a
    IoReadPartitionTable call with the 'return recognized partitions'
    boolean set to false to get a full description of the system.

    Then the drive layout structure can be modified by the application to
    reflect the new configuration of the disk and then is written back
    to the disk using this routine.

Arguments:

    DeviceObject - Pointer to device object for this disk.

    SectorSize - Sector size on the device.

    SectorsPerTrack - Track size on the device.

    NumberOfHeads - Same as tracks per cylinder.

Return Value:

    The functional value is STATUS_SUCCESS if all writes are completed
    without error.

--*/

{

#define PARTITION_TABLE_OFFSET         (0x1be / 2)
#define BOOT_SIGNATURE_OFFSET          ((0x200 / 2) - 1)

//
// Boot record signature value.
//

#define BOOT_RECORD_SIGNATURE          (0xaa55)

    ULONG writeSize;
    PUSHORT writeBuffer = NULL;
    CCHAR shiftCount;
    LARGE_INTEGER partitionTableOffset;
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    PIRP irp;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Ensure that no one is calling this function illegally.
    //

    PAGED_CODE();

    //
    // Determine the size of a write operation to ensure that at least 512
    // bytes are written.  This will guarantee that enough data is written to
    // include an entire partition table.  Note that this code assumes that
    // the actual sector size of the disk (if less than 512 bytes) is a
    // multiple of 2, a fairly reasonable assumption.
    //

    if (SectorSize >= 512) {
        writeSize = SectorSize;
    } else {
        writeSize = 512;
    }

    //
    // Look to see if this is an EZDrive Disk.  If it is then get the
    // real partititon table at 1.
    //

    {

        PVOID buff;

        HalExamineMBR(
            DeviceObject,
            writeSize,
            (ULONG)0x55,
            &buff
            );

        if (buff) {

            ExFreePool(buff);
            partitionTableOffset.QuadPart = 512;

        } else {

            partitionTableOffset.QuadPart = 0;

        }

    }

    //
    // Calculate shift count for converting between byte and sector.
    //

    WHICH_BIT( SectorSize, shiftCount );

    //
    // Allocate a buffer for the sector writes.
    //

    writeBuffer = ExAllocatePoolWithTag( NonPagedPoolCacheAligned, PAGE_SIZE, 'btsF');

    if (writeBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Read the boot record that's already there into the write buffer
    // and save its boot code area if the signature is valid.  This way
    // we don't clobber any boot code that might be there already.
    //

    KeInitializeEvent( &event, SynchronizationEvent, FALSE );

    irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                        DeviceObject,
                                        writeBuffer,
                                        writeSize,
                                        &partitionTableOffset,
                                        &event,
                                        &ioStatus );

    if (!irp) {
        ExFreePool(writeBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver( DeviceObject, irp );

    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  (PLARGE_INTEGER) NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS( status )) {
        ExFreePool(writeBuffer);
        return status;
    }

    //
    // If there's an 0xaa55 in the MBR signature, clear it out.
    //

    if(writeBuffer[BOOT_SIGNATURE_OFFSET] == BOOT_RECORD_SIGNATURE) {
        writeBuffer[BOOT_SIGNATURE_OFFSET] += 0x1111;
    }

    irp = IoBuildSynchronousFsdRequest( IRP_MJ_WRITE,
                                    DeviceObject,
                                    writeBuffer,
                                    writeSize,
                                    &partitionTableOffset,
                                    &event,
                                    &ioStatus );

    if (!irp) {
        ExFreePool(writeBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver( DeviceObject, irp );

    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  (PLARGE_INTEGER) NULL);
        status = ioStatus.Status;
    }

    //
    // Deallocate write buffer if it was allocated it.
    //

    ExFreePool( writeBuffer );

    return status;
}
#if defined(_X86_)

NTSTATUS
DiskQuerySuggestedLinkName(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    The routine try to find a suggested link name from registry for Removable
    using device object names of NT4 and NT3.51.

Arguments:

    DeviceObject - Pointer to driver object created by system.
    Irp - IRP involved.

Return Value:

    NTSTATUS

--*/

{
    PMOUNTDEV_SUGGESTED_LINK_NAME   suggestedName;
    WCHAR                           driveLetterNameBuffer[10];
    RTL_QUERY_REGISTRY_TABLE        queryTable[2];
    PWSTR                           valueName;
    UNICODE_STRING                  driveLetterName;
    NTSTATUS                        status;
    PIO_STACK_LOCATION              irpStack = IoGetCurrentIrpStackLocation(Irp);
    PCOMMON_DEVICE_EXTENSION        commonExtension = DeviceObject->DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION    p0Extension     = commonExtension->PartitionZeroExtension;
    ULONG                           i, diskCount;
    PCONFIGURATION_INFORMATION      configurationInformation;

    PAGED_CODE();

    DebugPrint((1, "DISK: IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME to device %#08lx"
                " through irp %#08lx\n",
                DeviceObject, Irp));

    DebugPrint((1, "      - DeviceNumber %d, - PartitionNumber %d\n",
                p0Extension->DeviceNumber,
                commonExtension->PartitionNumber));

    if (!TEST_FLAG(DeviceObject->Characteristics, FILE_REMOVABLE_MEDIA)) {

        status = STATUS_NOT_FOUND;
        return status;
    }

    if (commonExtension->PartitionNumber == 0) {

        status = STATUS_NOT_FOUND;
        return status;
    }

    if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
        sizeof(MOUNTDEV_SUGGESTED_LINK_NAME)) {

        status = STATUS_INVALID_PARAMETER;
        return status;
    }

    valueName = ExAllocatePoolWithTag(PagedPool,
                               sizeof(WCHAR) * 64,
                               DISK_TAG_NEC_98);

    if (!valueName) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    //
    // Look for a device object name of NT4.
    //
    swprintf(valueName, L"\\Device\\Harddisk%d\\Partition%d",
                                p0Extension->DeviceNumber,
                                commonExtension->PartitionNumber);

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

    if (!NT_SUCCESS(status)) {

        //
        // Look for a device object name of NT3.51.
        // scsimo.sys on NT3.51 created it as \Device\OpticalDiskX.
        // The number X were a serial number from zero on only Removable,
        // so we look for it serially without above DeviceNumber and PartitionNumber.
        //

        configurationInformation = IoGetConfigurationInformation();
        diskCount = configurationInformation->DiskCount;

        for (i = 0; i < diskCount; i++) {
            swprintf(valueName, L"\\Device\\OpticalDisk%d",i);

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

            if (NT_SUCCESS(status)) {
                break;
            }
        }

        if (!NT_SUCCESS(status)) {
            ExFreePool(valueName);
            return status;
        }
    }

    if (driveLetterName.Length != 4 ||
        driveLetterName.Buffer[0] < 'A' ||
        driveLetterName.Buffer[0] > 'Z' ||
        driveLetterName.Buffer[1] != ':') {

        status = STATUS_NOT_FOUND;
        ExFreePool(valueName);
        return status;
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
        return status;
    }

    RtlDeleteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                           L"\\Registry\\Machine\\System\\DISK",
                           valueName);

    ExFreePool(valueName);

    RtlCopyMemory(suggestedName->Name, L"\\DosDevices\\", 24);
    suggestedName->Name[12] = driveLetterName.Buffer[0];
    suggestedName->Name[13] = ':';

    return status;
}
#endif

