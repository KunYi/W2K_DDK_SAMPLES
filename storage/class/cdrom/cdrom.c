/*--

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    cdrom.c

Abstract:

    The CDROM class driver tranlates IRPs to SRBs with embedded CDBs
    and sends them to its devices through the port driver.

Environment:

    kernel mode only

Notes:

    SCSI Tape, CDRom and Disk class drivers share common routines
    that can be found in the CLASS directory (..\ntos\dd\class).

Revision History:

--*/

#include "stddef.h"
#include "string.h"

#include "ntddk.h"

#include "ntddcdvd.h"
#include "classpnp.h"

#include "initguid.h"
#include "ntddstor.h"
#include "cdrom.h"

ULONG CdRomCounter = 0;

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, DriverEntry)

#pragma alloc_text(PAGE, CdRomUnload)
#pragma alloc_text(PAGE, CdRomAddDevice)
#pragma alloc_text(PAGE, CreateCdRomDeviceObject)
#pragma alloc_text(PAGE, CdRomStartDevice)
#pragma alloc_text(PAGE, ScanForSpecial)
#pragma alloc_text(PAGE, CdRomRemoveDevice)
#pragma alloc_text(PAGE, CdRomGetDeviceType)
#pragma alloc_text(PAGE, CdRomReadVerification)
#pragma alloc_text(PAGE, CdromGetDeviceParameter)
#pragma alloc_text(PAGE, CdromSetDeviceParameter)
#pragma alloc_text(PAGE, CdromPickDvdRegion)
#pragma alloc_text(PAGE, CdRomIsPlayActive)

#pragma alloc_text(PAGEHITA, HitachiProcessError)
#pragma alloc_text(PAGEHIT2, HitachiProcessErrorGD2000)

#pragma alloc_text(PAGETOSH, ToshibaProcessErrorCompletion)
#pragma alloc_text(PAGETOSH, ToshibaProcessError)

#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the cdrom class driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    RegistryPath - Pointer to the name of the services node for this driver.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    CLASS_INIT_DATA InitializationData;

    PAGED_CODE();

    DebugPrint((2, "CDROM.SYS DriverObject %p loading\n", DriverObject));

    //
    // Zero InitData
    //

    RtlZeroMemory (&InitializationData, sizeof(CLASS_INIT_DATA));

    //
    // Set sizes
    //

    InitializationData.InitializationDataSize = sizeof(CLASS_INIT_DATA);

    InitializationData.FdoData.DeviceExtensionSize = DEVICE_EXTENSION_SIZE;

    InitializationData.FdoData.DeviceType = FILE_DEVICE_CD_ROM;
    InitializationData.FdoData.DeviceCharacteristics = FILE_REMOVABLE_MEDIA | FILE_READ_ONLY_DEVICE;

    //
    // Set entry points
    //

    InitializationData.FdoData.ClassInitDevice = CdRomInitDevice;
    InitializationData.FdoData.ClassStartDevice = CdRomStartDevice;
    InitializationData.FdoData.ClassStopDevice = CdRomStopDevice;
    InitializationData.FdoData.ClassRemoveDevice = CdRomRemoveDevice;

    InitializationData.FdoData.ClassReadWriteVerification = CdRomReadVerification;
    InitializationData.FdoData.ClassDeviceControl = CdRomDeviceControl;

    InitializationData.FdoData.ClassPowerDevice = ClassSpinDownPowerHandler;
    InitializationData.FdoData.ClassShutdownFlush = NULL;
    InitializationData.FdoData.ClassCreateClose = NULL;

    InitializationData.ClassStartIo = CdRomStartIo;
    InitializationData.ClassAddDevice = CdRomAddDevice;

    InitializationData.ClassTick = CdRomTickHandler;
    InitializationData.ClassUnload = CdRomUnload;

    //
    // Call the class init routine
    //

    return ClassInitialize( DriverObject, RegistryPath, &InitializationData);

} // end DriverEntry()

VOID
CdRomUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DriverObject);
    DebugPrint((2, "CDROM.SYS DriverObject %p unloading\n", DriverObject));
    return;
} // end CdRomUnload()


NTSTATUS
CdRomAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )

/*++

Routine Description:

    This routine creates and initializes a new FDO for the corresponding
    PDO.  It may perform property queries on the FDO but cannot do any
    media access operations.

Arguments:

    DriverObject - CDROM class driver object.

    Pdo - the physical device object we are being added to

Return Value:

    status

--*/

{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Get the address of the count of the number of cdroms already initialized.
    //

    status = CreateCdRomDeviceObject(
                DriverObject,
                PhysicalDeviceObject,
                CdRomCounter);

    //
    // Note: this always increments CdRomCounter
    //       it will eventually wrap, and fail additions
    //       if an existing cdrom has the given number.
    //       so unlikely that we won't even bother considering
    //       this case, since the cure is quite likely worse
    //       than the symptoms.
    //

    if(NT_SUCCESS(status)) {
        DebugPrint((2, "CDROM.SYS Add #%x succeeded\n",CdRomCounter));
        IoGetConfigurationInformation()->CdRomCount++;
        CdRomCounter++;
    } else {
        DebugPrint((1, "CDROM.SYS Add #%x failed! %x\n",CdRomCounter, status));
    }

    return status;
}


NTSTATUS
CreateCdRomDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN ULONG         DeviceCount
    )

/*++

Routine Description:

    This routine creates an object for the device and then calls the
    SCSI port driver for media capacity and sector size.

Arguments:

    DriverObject - Pointer to driver object created by system.
    PortDeviceObject - to connect to SCSI port driver.
    DeviceCount - Number of previously installed CDROMs.
    PortCapabilities - Pointer to structure returned by SCSI port
        driver describing adapter capabilites (and limitations).
    LunInfo - Pointer to configuration information for this device.

Return Value:

    NTSTATUS

--*/
{
    UCHAR ntNameBuffer[64];
    STRING ntNameString;
    NTSTATUS status;

    PDEVICE_OBJECT lowerDevice = NULL;
    PDEVICE_OBJECT deviceObject = NULL;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = NULL;
    PCDROM_DATA cdData = NULL;

    CCHAR                   dosNameBuffer[64];
    CCHAR                   deviceNameBuffer[64];
    STRING                  deviceNameString;
    STRING                  dosString;
    UNICODE_STRING          dosUnicodeString;
    UNICODE_STRING          unicodeString;

    PAGED_CODE();

    //
    // Claim the device. Note that any errors after this
    // will goto the generic handler, where the device will
    // be released.
    //

    lowerDevice = IoGetAttachedDeviceReference(PhysicalDeviceObject);

    status = ClassClaimDevice(lowerDevice, FALSE);

    if(!NT_SUCCESS(status)) {

        //
        // Someone already had this device - we're in trouble
        //

        ObDereferenceObject(lowerDevice);
        return status;
    }

    //
    // Create device object for this device.
    //

    sprintf(ntNameBuffer, "\\Device\\CdRom%d", DeviceCount);

    status = ClassCreateDeviceObject(DriverObject,
                                     ntNameBuffer,
                                     PhysicalDeviceObject,
                                     TRUE,
                                     &deviceObject);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"CreateCdRomDeviceObjects: Can not create device %s\n",
                    ntNameBuffer));

        goto CreateCdRomDeviceObjectExit;
    }

    //
    // Indicate that IRPs should include MDLs.
    //

    deviceObject->Flags |= DO_DIRECT_IO;

    fdoExtension = deviceObject->DeviceExtension;

    //
    // Back pointer to device object.
    //

    fdoExtension->CommonExtension.DeviceObject = deviceObject;

    //
    // This is the physical device.
    //

    fdoExtension->CommonExtension.PartitionZeroExtension = fdoExtension;

    //
    // Initialize lock count to zero. The lock count is used to
    // disable the ejection mechanism when media is mounted.
    //

    fdoExtension->LockCount = 0;

    //
    // Save system cdrom number
    //

    fdoExtension->DeviceNumber = DeviceCount;

    //
    // Set the alignment requirements for the device based on the
    // host adapter requirements
    //

    if (lowerDevice->AlignmentRequirement > deviceObject->AlignmentRequirement) {
        deviceObject->AlignmentRequirement = lowerDevice->AlignmentRequirement;
    }

    //
    // Save the device descriptors
    //

    fdoExtension->AdapterDescriptor = NULL;

    fdoExtension->DeviceDescriptor = NULL;

    //
    // Clear the SrbFlags and disable synchronous transfers
    //

    fdoExtension->SrbFlags = SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Finally, attach to the PDO
    //

    fdoExtension->LowerPdo = PhysicalDeviceObject;

    fdoExtension->CommonExtension.LowerDeviceObject =
        IoAttachDeviceToDeviceStack(deviceObject, PhysicalDeviceObject);

    if(fdoExtension->CommonExtension.LowerDeviceObject == NULL) {

        //
        // Uh - oh, we couldn't attach
        // cleanup and return
        //

        status = STATUS_UNSUCCESSFUL;
        goto CreateCdRomDeviceObjectExit;
    }

    //
    // CdRom uses an extra stack location for synchronizing it's start io
    // routine
    //

    deviceObject->StackSize++;

    //
    // cdData is used a few times below
    //

    cdData = fdoExtension->CommonExtension.DriverData;

    //
    // For NTMS to be able to easily determine drives-drv. letter matches.
    //

    status = CdRomCreateWellKnownName( deviceObject );

    if (!NT_SUCCESS(status)) {
        DebugPrint((1, "CdromCreateDeviceObjects: unable to create symbolic "
                    "link for device %wZ\n", &fdoExtension->CommonExtension.DeviceName));
        DebugPrint((1, "CdromCreateDeviceObjects: (non-fatal error)\n"));
    }

    ClassUpdateInformationInRegistry(deviceObject, "CdRom",
                                     fdoExtension->DeviceNumber, NULL, 0);

    //
    // from above IoGetAttachedDeviceReference
    //

    ObDereferenceObject(lowerDevice);

    //
    // need to init timerlist here in case a remove occurs
    // without a start, since we check the list is empty on remove.
    //

    cdData->DelayedRetryIrp = NULL;
    cdData->DelayedRetryInterval = 0;

    //
    // The device is initialized properly - mark it as such.
    //

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return(STATUS_SUCCESS);

CreateCdRomDeviceObjectExit:

    //
    // Release the device since an error occured.
    //

    // ClassClaimDevice(PortDeviceObject,
    //                      LunInfo,
    //                      TRUE,
    //                      NULL);

    //
    // from above IoGetAttachedDeviceReference
    //

    ObDereferenceObject(lowerDevice);

    if (deviceObject != NULL) {
        IoDeleteDevice(deviceObject);
    }

    return status;

} // end CreateCdRomDeviceObject()


NTSTATUS
CdRomInitDevice(
    IN PDEVICE_OBJECT Fdo
    )

/*++

Routine Description:

    This routine will complete the cd-rom initialization.  This includes
    allocating sense info buffers and srb s-lists, reading drive capacity
    and setting up Media Change Notification (autorun).

    This routine will not clean up allocate resources if it fails - that
    is left for device stop/removal

Arguments:

    Fdo - a pointer to the functional device object for this device

Return Value:

    status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = Fdo->DeviceExtension;
    PCLASS_DRIVER_EXTENSION driverExtension = ClassGetDriverExtension(
                                                Fdo->DriverObject);

    PVOID senseData = NULL;

    ULONG timeOut;

    BOOLEAN changerDevice;

    PCDROM_DATA cddata = NULL;

    ULONG bps;
    ULONG lastBit;


    NTSTATUS status;

    PAGED_CODE();


    //
    // Build the lookaside list for srb's for the physical disk.  Should only
    // need a couple.
    //

    ClassInitializeSrbLookasideList(&(fdoExtension->CommonExtension),
                                    CDROM_SRB_LIST_SIZE);

    //
    // Allocate request sense buffer.
    //

    senseData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                      SENSE_BUFFER_SIZE,
                                      CDROM_TAG_SENSE_INFO);

    if (senseData == NULL) {

        //
        // The buffer cannot be allocated.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CdRomInitDeviceExit;
    }

    //
    // Set the sense data pointer in the device extension.
    //

    fdoExtension->SenseData = senseData;

    //
    // CDROMs are not partitionable so starting offset is 0.
    //

    commonExtension->StartingOffset.LowPart = 0;
    commonExtension->StartingOffset.HighPart = 0;

    //
    // Set timeout value in seconds.
    //

    timeOut = ClassQueryTimeOutRegistryValue(Fdo);
    if (timeOut) {
        fdoExtension->TimeOutValue = timeOut;
    } else {
        fdoExtension->TimeOutValue = SCSI_CDROM_TIMEOUT;
    }

    cddata = (PCDROM_DATA)(commonExtension->DriverData);

    //
    // Set up media change support defaults.
    //

    KeInitializeSpinLock(&cddata->DelayedRetrySpinLock);

    cddata->DelayedRetryIrp = NULL;
    cddata->DelayedRetryInterval = 0;

    //
    // Scan for  controllers that require special processing.
    //

    ScanForSpecial(Fdo);

    //
    // Set the default geometry for the cdrom to match what NT 4 used.
    // Classpnp will use these values to compute the cylinder count rather
    // than using it's NT 5.0 defaults.
    //

    fdoExtension->DiskGeometry.TracksPerCylinder = 0x40;
    fdoExtension->DiskGeometry.SectorsPerTrack = 0x20;

    //
    // Do READ CAPACITY. This SCSI command
    // returns the last sector address on the device
    // and the bytes per sector.
    // These are used to calculate the drive capacity
    // in bytes.
    //

    status = ClassReadDriveCapacity(Fdo);

    bps = fdoExtension->DiskGeometry.BytesPerSector;

    if (!NT_SUCCESS(status) || !bps) {

        DebugPrint((1,
                "CdRomStartDevice: Can't read capacity for device %wZ\n",
                &(fdoExtension->CommonExtension.DeviceName)));

        //
        // Set disk geometry to default values (per ISO 9660).
        //

        bps = 2048;
        fdoExtension->SectorShift = 11;
        commonExtension->PartitionLength.QuadPart = (LONGLONG)(0x7fffffff);

    } else {

        //
        // Insure that bytes per sector is a power of 2
        // This corrects a problem with the HP 4020i CDR where it
        // returns an incorrect number for bytes per sector.
        //

        lastBit = (ULONG) -1;
        while (bps) {
            lastBit++;
            bps = bps >> 1;
        }

        bps = 1 << lastBit;
    }
    fdoExtension->DiskGeometry.BytesPerSector = bps;
    DebugPrint((2, "CdRomInitDevice: Calc'd bps = %x\n", bps));

    ClassInitializeMediaChangeDetection(fdoExtension, "CdRom");


    //
    // test for audio read capabilities
    //

    DebugPrint((1, "Detecting XA_READ capabilities\n"));

    if (CdRomGetDeviceType(Fdo) == FILE_DEVICE_DVD) {

        DebugPrint((1, "CdRomInitDevice: DVD Devices require START_UNIT\n"));

        SET_FLAG(fdoExtension->DeviceFlags, DEV_SAFE_START_UNIT);

        //
        // all DVD devices must support the READ_CD command
        //

        DebugPrint((1, "CdRomDetermineRawReadCapabilities: DVD devices "
                    "support READ_CD command for FDO %p\n", Fdo));

        SET_FLAG(cddata->XAFlags, XA_USE_READ_CD);


        status = STATUS_SUCCESS;

    } else if ((fdoExtension->DeviceDescriptor->BusType != BusTypeScsi)  &&
               (fdoExtension->DeviceDescriptor->BusType != BusTypeAta)   &&
               (fdoExtension->DeviceDescriptor->BusType != BusTypeAtapi) &&
               (fdoExtension->DeviceDescriptor->BusType != BusTypeUnknown)
               ) {

        //
        // devices on the newer busses must support READ_CD command
        //

        DebugPrint((1, "CdRomDetermineRawReadCapabilities: Devices for newer "
                    "busses must support READ_CD command for FDO %p\n", Fdo));
        SET_FLAG(cddata->XAFlags, XA_USE_READ_CD);

    } else {

        SCSI_REQUEST_BLOCK srb;
        PCDB cdb;
        ULONG length;
        PUCHAR buffer = NULL;
        ULONG count;

        //
        // BUGBUG - use the mode page to determine READ_CD support, then fall
        //          back on the below (unreliable?) hack.
        //

        //
        // Build the MODE SENSE CDB. The data returned will be kept in the
        // device extension and used to set block size.
        //

        length = max(sizeof(ERROR_RECOVERY_DATA),sizeof(ERROR_RECOVERY_DATA10));

        buffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                       length,
                                       CDROM_TAG_MODE_DATA);

        if (!buffer) {
            DebugPrint((1, "CdRomDetermineRawReadCapabilities: cannot allocate "
                        "buffer, so leaving for FDO %p\n", Fdo));
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto CdRomInitDeviceExit;
        }

        for (count = 0; count < 2; count++) {

            if (count == 0) {
                length = sizeof(ERROR_RECOVERY_DATA);
            } else {
                length = sizeof(ERROR_RECOVERY_DATA10);
            }

            RtlZeroMemory(buffer, length);
            RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));
            cdb = (PCDB)srb.Cdb;

            srb.TimeOutValue = fdoExtension->TimeOutValue;

            if (count == 0) {
                srb.CdbLength = 6;
                cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
                cdb->MODE_SENSE.PageCode = 0x1;
                cdb->MODE_SENSE.AllocationLength = (UCHAR)length;
            } else {
                srb.CdbLength = 10;
                cdb->MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
                cdb->MODE_SENSE10.PageCode = 0x1;
                cdb->MODE_SENSE10.AllocationLength[0] = (UCHAR)(length >> 8);
                cdb->MODE_SENSE10.AllocationLength[1] = (UCHAR)(length & 0xFF);
            }

            status = ClassSendSrbSynchronous(Fdo,
                                             &srb,
                                             buffer,
                                             length,
                                             FALSE);


            if (NT_SUCCESS(status) || (status == STATUS_DATA_OVERRUN)) {

                //
                // STATUS_DATA_OVERRUN means it's a newer drive with more info
                // to tell us, so it's probably able to support READ_CD
                //

                RtlZeroMemory(cdb, CDB12GENERIC_LENGTH);

                srb.CdbLength = 12;
                cdb->READ_CD.OperationCode = SCSIOP_READ_CD;

                status = ClassSendSrbSynchronous(Fdo,
                                                 &srb,
                                                 NULL,
                                                 0,
                                                 FALSE);

                if (NT_SUCCESS(status) ||
                    (status == STATUS_NO_MEDIA_IN_DEVICE) ||
                    (status == STATUS_NONEXISTENT_SECTOR)
                    ) {

                    //
                    // READ_CD works
                    //

                    DebugPrint((1, "CdRomDetermineRawReadCapabilities: Using "
                                "READ_CD for FDO %p\n", Fdo));
                    SET_FLAG(cddata->XAFlags, XA_USE_READ_CD);
                    break; // out of the for loop

                }

                DebugPrint((1, "CdRomDetermineRawReadCapabilities: Using "
                            "%s-byte mode switching for FDO %p due to status "
                            "%x returned for READ_CD\n",
                            ((count == 0) ? "6" : "10"), Fdo, status));

                if (count == 0) {
                    SET_FLAG(cddata->XAFlags, XA_USE_6_BYTE);
                    RtlCopyMemory(&cddata->Header,
                                  buffer,
                                  sizeof(ERROR_RECOVERY_DATA));
                    cddata->Header.ModeDataLength = 0;
                } else {
                    SET_FLAG(cddata->XAFlags, XA_USE_10_BYTE);
                    RtlCopyMemory(&cddata->Header10,
                                  buffer,
                                  sizeof(ERROR_RECOVERY_DATA10));
                    cddata->Header10.ModeDataLength[0] = 0;
                    cddata->Header10.ModeDataLength[1] = 0;
                }
                break;  // out of for loop

            }

            //
            // mode sense failed
            //

        } // end of for loop to try 6 and 10-byte mode sense

        if (count == 2) {

            //
            // nothing worked.  we probably cannot support digital
            // audio extraction from this drive
            //

            DebugPrint((1, "CdRomDetermineRawReadCapabilities: FDO %p "
                        "cannot support READ_CD\n", Fdo));
            CLEAR_FLAG(cddata->XAFlags, XA_PLEXTOR_CDDA);
            CLEAR_FLAG(cddata->XAFlags, XA_NEC_CDDA);
            SET_FLAG(cddata->XAFlags, XA_NOT_SUPPORTED);

        } // end of count == 2

        //
        // free our resources
        //

        ExFreePool(buffer);

        //
        // set a successful status
        // (in case someone later checks this)
        //

        status = STATUS_SUCCESS;

    }

    //
    // Register interfaces for this device.
    //

    {
        UNICODE_STRING interfaceName;

        RtlInitUnicodeString(&interfaceName, NULL);

        status = IoRegisterDeviceInterface(fdoExtension->LowerPdo,
                                           (LPGUID) &CdRomClassGuid,
                                           NULL,
                                           &interfaceName);

        if(NT_SUCCESS(status)) {

            cddata->CdromInterfaceString = interfaceName;

            status = IoSetDeviceInterfaceState(
                        &interfaceName,
                        TRUE);

            if(!NT_SUCCESS(status)) {

                DebugPrint((1, "CdromInitDevice: Unable to register cdrom "
                               "DCA for fdo %p [%lx]\n",
                            Fdo, status));
            }
        }
    }

    return(STATUS_SUCCESS);

CdRomInitDeviceExit:

    return status;

}


NTSTATUS
CdRomStartDevice(
    IN PDEVICE_OBJECT Fdo
    )
/*++

Routine Description:

    This routine starts the timer for the cdrom

Arguments:

    Fdo - a pointer to the functional device object for this device

Return Value:

    status

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = Fdo->DeviceExtension;
    PCDROM_DATA cddata = (PCDROM_DATA)(commonExtension->DriverData);
    PDVD_COPY_PROTECT_KEY copyProtectKey;
    PDVD_RPC_KEY rpcKey;
    IO_STATUS_BLOCK ioStatus;
    ULONG bufferLen;

    // CdRomCreateWellKnownName(Fdo);

    //
    // if we have a DVD-ROM
    //    if we have a rpc0 device
    //        fake a rpc2 device
    //    if device does not have a dvd region set
    //        select a dvd region for the user
    //
    cddata->DvdRpc0Device = FALSE;

    //
    // check to see if we have a DVD device
    //

    if (CdRomGetDeviceType(Fdo) != FILE_DEVICE_DVD) {
        return STATUS_SUCCESS;
    }

    //
    // we got a DVD drive.
    // now, figure out if we have a RPC0 device
    //

    bufferLen = DVD_RPC_KEY_LENGTH;
    copyProtectKey =
        (PDVD_COPY_PROTECT_KEY)ExAllocatePoolWithTag(PagedPool,
                                                     bufferLen,
                                                     DVD_TAG_RPC2_CHECK);

    if (copyProtectKey == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // get the device region
    //
    RtlZeroMemory (copyProtectKey, bufferLen);
    copyProtectKey->KeyLength = DVD_RPC_KEY_LENGTH;
    copyProtectKey->KeyType = DvdGetRpcKey;

    //
    // Build a request for READ_KEY
    //
    ClassSendDeviceIoControlSynchronous(
        IOCTL_DVD_READ_KEY,
        Fdo,
        copyProtectKey,
        DVD_RPC_KEY_LENGTH,
        DVD_RPC_KEY_LENGTH,
        FALSE,
        &ioStatus
        );

    if (!NT_SUCCESS(ioStatus.Status)) {

        //
        // we have a rpc0 device
        //
        // NOTE: THIS MODIFIES THE BEHAVIOR OF THE IOCTL
        //

        cddata->DvdRpc0Device = TRUE;

        //
        // need this to be initialized for RPC Phase 1 drives (rpc0)
        //

        KeInitializeMutex(&cddata->Rpc0RegionMutex, 0);

        DebugPrint((1, "CdromStartDevice (%p): RPC Phase 1 drive detected\n",
                    Fdo));

        //
        // note: we could force this chosen now, but it's better to reduce
        // the number of code paths that could be taken.  always delay to
        // increase the percentage code coverage.
        //

        DebugPrint((1, "CdromStartDevice (%p): Delay DVD Region Selection\n"));

        cddata->Rpc0SystemRegion           = 0xff;
        cddata->Rpc0SystemRegionResetCount = DVD_MAX_REGION_RESET_COUNT;
        cddata->PickDvdRegion              = 1;
        cddata->Rpc0RetryRegistryCallback  = 1;
        ExFreePool(copyProtectKey);
        return STATUS_SUCCESS;

    } else {

        rpcKey = (PDVD_RPC_KEY) copyProtectKey->KeyData;

        //
        // TypeCode of zero means that no region has been set.
        //

        if (rpcKey->TypeCode == 0) {
            DebugPrint((1, "CdromStartDevice (%p): must choose DVD region\n",
                        Fdo));
            cddata->PickDvdRegion = 1;
            CdromPickDvdRegion(Fdo);
        }
    }

    ExFreePool (copyProtectKey);

    return STATUS_SUCCESS;
}


NTSTATUS
CdRomStopDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR Type
    )
{
    return STATUS_SUCCESS;
}


VOID
CdRomStartIo(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP Irp
    )
{

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;

    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  nextIrpStack = IoGetNextIrpStackLocation(Irp);
    PIO_STACK_LOCATION  irpStack;

    PIRP                irp2 = NULL;

    ULONG               transferPages;
    ULONG               transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER       startingOffset = currentIrpStack->Parameters.Read.ByteOffset;
    PCDROM_DATA         cdData;
    PSCSI_REQUEST_BLOCK srb = NULL;
    PCDB                cdb;
    PUCHAR              senseBuffer = NULL;
    PVOID               dataBuffer;
    NTSTATUS            status;
    BOOLEAN             use6Byte;

    //
    // Mark IRP with status pending.
    //

    IoMarkIrpPending(Irp);

    cdData = (PCDROM_DATA)(fdoExtension->CommonExtension.DriverData);
    use6Byte = TEST_FLAG(cdData->XAFlags, XA_USE_6_BYTE);

    //
    // If the flag is set in the device object, force a verify.
    //

    if (Fdo->Flags & DO_VERIFY_VOLUME) {
        DebugPrint((2, "CdRomStartIo: [%p] Volume needs verified\n", Irp));
        if (!(currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME)) {

            if (Irp->Tail.Overlay.Thread) {
                IoSetHardErrorOrVerifyDevice(Irp, Fdo);
            }

            Irp->IoStatus.Status = STATUS_VERIFY_REQUIRED;

            DebugPrint((2, "CdRomStartIo: [%p] Calling UpdateCapcity - "
                           "ioctl event = %lx\n",
                        Irp,
                        nextIrpStack->Parameters.Others.Argument1
                      ));

            //
            // our device control dispatch routine stores an event in the next
            // stack location to signal when startio has completed.  We need to
            // pass this in so that the update capacity completion routine can
            // set it rather than completing the Irp.
            //

            //
            // BUGBUG - what happens when this fails?
            //

            status = CdRomUpdateCapacity(fdoExtension,
                                         Irp,
                                         nextIrpStack->Parameters.Others.Argument1
                                         );

            DebugPrint((2, "CdRomStartIo: [%p] UpdateCapacity returned %lx\n", Irp, status));
            ASSERT(status == STATUS_PENDING);
            return;
        }
    }


    if (currentIrpStack->MajorFunction == IRP_MJ_READ) {

        ULONG maximumTransferLength = fdoExtension->AdapterDescriptor->MaximumTransferLength;

        //
        // Add partition byte offset to make starting byte relative to
        // beginning of disk. In addition, add in skew for DM Driver, if any.
        //

        currentIrpStack->Parameters.Read.ByteOffset.QuadPart += (fdoExtension->CommonExtension.StartingOffset.QuadPart);

        //
        // Calculate number of pages in this transfer.
        //

        transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(Irp->MdlAddress),
                                                       currentIrpStack->Parameters.Read.Length);

        //
        // Check if request length is greater than the maximum number of
        // bytes that the hardware can transfer.
        //

        if (cdData->RawAccess) {

            ASSERT(!TEST_FLAG(cdData->XAFlags, XA_USE_READ_CD));

            //
            // Fire off a mode select to switch back to cooked sectors.
            //

            irp2 = IoAllocateIrp((CCHAR)(Fdo->StackSize+1), FALSE);

            if (!irp2) {
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            srb = ExAllocatePoolWithTag(NonPagedPool,
                                        sizeof(SCSI_REQUEST_BLOCK),
                                        CDROM_TAG_SRB);
            if (!srb) {
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

            cdb = (PCDB)srb->Cdb;

            //
            // Allocate sense buffer.
            //

            senseBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                                SENSE_BUFFER_SIZE,
                                                CDROM_TAG_SENSE_INFO);

            if (!senseBuffer) {
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Set up the irp.
            //

            IoSetNextIrpStackLocation(irp2);
            irp2->IoStatus.Status = STATUS_SUCCESS;
            irp2->IoStatus.Information = 0;
            irp2->Flags = 0;
            irp2->UserBuffer = NULL;

            //
            // Save the device object and irp in a private stack location.
            //

            irpStack = IoGetCurrentIrpStackLocation(irp2);
            irpStack->DeviceObject = Fdo;
            irpStack->Parameters.Others.Argument2 = (PVOID) Irp;

            //
            // The retry count will be in the real Irp, as the retry logic will
            // recreate our private irp.
            //

            if (!(nextIrpStack->Parameters.Others.Argument1)) {

                //
                // Only jam this in if it doesn't exist. The completion routines can
                // call StartIo directly in the case of retries and resetting it will
                // cause infinite loops.
                //

                nextIrpStack->Parameters.Others.Argument1 = (PVOID) MAXIMUM_RETRIES;
            }

            //
            // Construct the IRP stack for the lower level driver.
            //

            irpStack = IoGetNextIrpStackLocation(irp2);
            irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
            irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_IN;
            irpStack->Parameters.Scsi.Srb = srb;

            srb->Length = SCSI_REQUEST_BLOCK_SIZE;
            srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
            srb->SrbStatus = srb->ScsiStatus = 0;
            srb->NextSrb = 0;
            srb->OriginalRequest = irp2;
            srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;
            srb->SenseInfoBuffer = senseBuffer;

            transferByteCount = (use6Byte) ? sizeof(ERROR_RECOVERY_DATA) : sizeof(ERROR_RECOVERY_DATA10);

            dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                               transferByteCount,
                                               CDROM_TAG_RAW);

            if (!dataBuffer) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;

            }

            irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                            transferByteCount,
                                            FALSE,
                                            FALSE,
                                            (PIRP) NULL);

            if (!irp2->MdlAddress) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                ExFreePool(dataBuffer);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Prepare the MDL
            //

            MmBuildMdlForNonPagedPool(irp2->MdlAddress);

            srb->DataBuffer = dataBuffer;

            //
            // Set the new block size in the descriptor.
            //

            if (use6Byte) {
                cdData->BlockDescriptor.BlockLength[0] = (UCHAR)(COOKED_SECTOR_SIZE >> 16) & 0xFF;
                cdData->BlockDescriptor.BlockLength[1] = (UCHAR)(COOKED_SECTOR_SIZE >>  8) & 0xFF;
                cdData->BlockDescriptor.BlockLength[2] = (UCHAR)(COOKED_SECTOR_SIZE & 0xFF);
            } else {
                cdData->BlockDescriptor10.BlockLength[0] = (UCHAR)(COOKED_SECTOR_SIZE >> 16) & 0xFF;
                cdData->BlockDescriptor10.BlockLength[1] = (UCHAR)(COOKED_SECTOR_SIZE >>  8) & 0xFF;
                cdData->BlockDescriptor10.BlockLength[2] = (UCHAR)(COOKED_SECTOR_SIZE & 0xFF);
            }

            //
            // Move error page into dataBuffer.
            //

            RtlCopyMemory(srb->DataBuffer, &cdData->Header, transferByteCount);

            //
            // Build and send a mode select to switch into raw mode.
            //

            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_OUT);
            srb->DataTransferLength = transferByteCount;
            srb->TimeOutValue = fdoExtension->TimeOutValue * 2;

            if (use6Byte) {
                srb->CdbLength = 6;
                cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
                cdb->MODE_SELECT.PFBit = 1;
                cdb->MODE_SELECT.ParameterListLength = (UCHAR)transferByteCount;
            } else {
                srb->CdbLength = 10;
                cdb->MODE_SELECT10.OperationCode = SCSIOP_MODE_SELECT10;
                cdb->MODE_SELECT10.PFBit = 1;
                cdb->MODE_SELECT10.ParameterListLength[0] = (UCHAR)(transferByteCount >> 8);
                cdb->MODE_SELECT10.ParameterListLength[1] = (UCHAR)(transferByteCount & 0xFF);
            }

            //
            // Update completion routine.
            //

            IoSetCompletionRoutine(irp2,
                                   CdRomSwitchModeCompletion,
                                   srb,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        if ((currentIrpStack->Parameters.Read.Length > maximumTransferLength) ||
            (transferPages >
                fdoExtension->AdapterDescriptor->MaximumPhysicalPages)) {

            //
            // Request needs to be split. Completion of each portion of the
            // request will fire off the next portion. The final request will
            // signal Io to send a new request.
            //

            transferPages =
                fdoExtension->AdapterDescriptor->MaximumPhysicalPages - 1;

            if(maximumTransferLength > (transferPages << PAGE_SHIFT)) {
                maximumTransferLength = transferPages << PAGE_SHIFT;
            }

            //
            // Check that the maximum transfer size is not zero
            //

            if(maximumTransferLength == 0) {
                maximumTransferLength = PAGE_SIZE;
            }

            ClassSplitRequest(Fdo, Irp, maximumTransferLength);
            return;

        } else {

            //
            // Build SRB and CDB for this IRP.
            //

            ClassBuildRequest(Fdo, Irp);

        }


    } else if (currentIrpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        //
        // Allocate an irp, srb and associated structures.
        //

        irp2 = IoAllocateIrp((CCHAR)(Fdo->StackSize+1),
                              FALSE);

        if (!irp2) {
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

            BAIL_OUT(Irp);
            CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
            return;
        }

        srb = ExAllocatePoolWithTag(NonPagedPool,
                                    sizeof(SCSI_REQUEST_BLOCK),
                                    CDROM_TAG_SRB);
        if (!srb) {
            IoFreeIrp(irp2);
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

            BAIL_OUT(Irp);
            CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
            return;
        }

        RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

        cdb = (PCDB)srb->Cdb;

        //
        // Allocate sense buffer.
        //

        senseBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                            SENSE_BUFFER_SIZE,
                                            CDROM_TAG_SENSE_INFO);

        if (!senseBuffer) {
            ExFreePool(srb);
            IoFreeIrp(irp2);
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

            BAIL_OUT(Irp);
            CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
            return;
        }

        RtlZeroMemory(senseBuffer, SENSE_BUFFER_SIZE);

        //
        // Set up the irp.
        //

        IoSetNextIrpStackLocation(irp2);
        irp2->IoStatus.Status = STATUS_SUCCESS;
        irp2->IoStatus.Information = 0;
        irp2->Flags = 0;
        irp2->UserBuffer = NULL;

        //
        // Save the device object and irp in a private stack location.
        //

        irpStack = IoGetCurrentIrpStackLocation(irp2);
        irpStack->DeviceObject = Fdo;
        irpStack->Parameters.Others.Argument2 = (PVOID) Irp;

        //
        // The retry count will be in the real Irp, as the retry logic will
        // recreate our private irp.
        //

        if (!(nextIrpStack->Parameters.Others.Argument1)) {

            //
            // Only jam this in if it doesn't exist. The completion routines can
            // call StartIo directly in the case of retries and resetting it will
            // cause infinite loops.
            //

            nextIrpStack->Parameters.Others.Argument1 = (PVOID) MAXIMUM_RETRIES;
        }

        //
        // keep track of the new irp as Argument3
        //

        nextIrpStack->Parameters.Others.Argument3 = irp2;


        //
        // Construct the IRP stack for the lower level driver.
        //

        irpStack = IoGetNextIrpStackLocation(irp2);
        irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
        irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_IN;
        irpStack->Parameters.Scsi.Srb = srb;

        IoSetCompletionRoutine(irp2,
                               CdRomDeviceControlCompletion,
                               srb,
                               TRUE,
                               TRUE,
                               TRUE);
        //
        // Setup those fields that are generic to all requests.
        //

        srb->Length = SCSI_REQUEST_BLOCK_SIZE;
        srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
        srb->SrbStatus = srb->ScsiStatus = 0;
        srb->NextSrb = 0;
        srb->OriginalRequest = irp2;
        srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;
        srb->SenseInfoBuffer = senseBuffer;

        switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {


        case IOCTL_CDROM_RAW_READ: {

            //
            // Determine whether the drive is currently in raw or cooked mode,
            // and which command to use to read the data.
            //

            if (!TEST_FLAG(cdData->XAFlags, XA_USE_READ_CD)) {

                PRAW_READ_INFO rawReadInfo =
                                   (PRAW_READ_INFO)currentIrpStack->Parameters.DeviceIoControl.Type3InputBuffer;
                ULONG          maximumTransferLength;
                ULONG          transferPages;

                if (cdData->RawAccess) {

                    ULONG  startingSector;
                    UCHAR  min, sec, frame;

                    //
                    // Free the recently allocated irp, as we don't need it.
                    //

                    IoFreeIrp(irp2);

                    cdb = (PCDB)srb->Cdb;
                    RtlZeroMemory(cdb, CDB12GENERIC_LENGTH);

                    //
                    // Calculate starting offset.
                    //

                    startingSector = (ULONG)(rawReadInfo->DiskOffset.QuadPart >> fdoExtension->SectorShift);
                    transferByteCount  = rawReadInfo->SectorCount * RAW_SECTOR_SIZE;
                    maximumTransferLength = fdoExtension->AdapterDescriptor->MaximumTransferLength;
                    transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(Irp->MdlAddress),
                                                                   transferByteCount);

                    //
                    // Determine if request is within limits imposed by miniport.
                    //
                    if (transferByteCount > maximumTransferLength ||
                        transferPages > fdoExtension->AdapterDescriptor->MaximumPhysicalPages) {

                        //
                        // The claim is that this won't happen, and is backed up by
                        // ActiveMovie usage, which does unbuffered XA reads of 0x18000, yet
                        // we get only 4 sector requests.
                        //

                        ExFreePool(senseBuffer);
                        ExFreePool(srb);

                        Irp->IoStatus.Information = 0;
                        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

                        BAIL_OUT(Irp);
                        CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                        return;

                    }

                    srb->OriginalRequest = Irp;
                    srb->SrbFlags = fdoExtension->SrbFlags;
                    srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);
                    srb->DataTransferLength = transferByteCount;
                    srb->TimeOutValue = fdoExtension->TimeOutValue;
                    srb->CdbLength = 10;
                    srb->DataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);

                    if (rawReadInfo->TrackMode == CDDA) {
                        if (TEST_FLAG(cdData->XAFlags, XA_PLEXTOR_CDDA)) {

                            srb->CdbLength = 12;

                            cdb->PLXTR_READ_CDDA.LogicalBlockByte3  = (UCHAR) (startingSector & 0xFF);
                            cdb->PLXTR_READ_CDDA.LogicalBlockByte2  = (UCHAR) ((startingSector >>  8) & 0xFF);
                            cdb->PLXTR_READ_CDDA.LogicalBlockByte1  = (UCHAR) ((startingSector >> 16) & 0xFF);
                            cdb->PLXTR_READ_CDDA.LogicalBlockByte0  = (UCHAR) ((startingSector >> 24) & 0xFF);

                            cdb->PLXTR_READ_CDDA.TransferBlockByte3 = (UCHAR) (rawReadInfo->SectorCount & 0xFF);
                            cdb->PLXTR_READ_CDDA.TransferBlockByte2 = (UCHAR) (rawReadInfo->SectorCount >> 8);
                            cdb->PLXTR_READ_CDDA.TransferBlockByte1 = 0;
                            cdb->PLXTR_READ_CDDA.TransferBlockByte0 = 0;

                            cdb->PLXTR_READ_CDDA.SubCode = 0;
                            cdb->PLXTR_READ_CDDA.OperationCode = 0xD8;

                        } else if (TEST_FLAG(cdData->XAFlags, XA_NEC_CDDA)) {

                            cdb->NEC_READ_CDDA.LogicalBlockByte3  = (UCHAR) (startingSector & 0xFF);
                            cdb->NEC_READ_CDDA.LogicalBlockByte2  = (UCHAR) ((startingSector >>  8) & 0xFF);
                            cdb->NEC_READ_CDDA.LogicalBlockByte1  = (UCHAR) ((startingSector >> 16) & 0xFF);
                            cdb->NEC_READ_CDDA.LogicalBlockByte0  = (UCHAR) ((startingSector >> 24) & 0xFF);

                            cdb->NEC_READ_CDDA.TransferBlockByte1 = (UCHAR) (rawReadInfo->SectorCount & 0xFF);
                            cdb->NEC_READ_CDDA.TransferBlockByte0 = (UCHAR) (rawReadInfo->SectorCount >> 8);

                            cdb->NEC_READ_CDDA.OperationCode = 0xD4;
                        }
                    } else {

                        cdb->CDB10.TransferBlocksMsb  = (UCHAR) (rawReadInfo->SectorCount >> 8);
                        cdb->CDB10.TransferBlocksLsb  = (UCHAR) (rawReadInfo->SectorCount & 0xFF);

                        cdb->CDB10.LogicalBlockByte3  = (UCHAR) (startingSector & 0xFF);
                        cdb->CDB10.LogicalBlockByte2  = (UCHAR) ((startingSector >>  8) & 0xFF);
                        cdb->CDB10.LogicalBlockByte1  = (UCHAR) ((startingSector >> 16) & 0xFF);
                        cdb->CDB10.LogicalBlockByte0  = (UCHAR) ((startingSector >> 24) & 0xFF);

                        cdb->CDB10.OperationCode = SCSIOP_READ;
                    }

                    srb->SrbStatus = srb->ScsiStatus = 0;

                    nextIrpStack->MajorFunction = IRP_MJ_SCSI;
                    nextIrpStack->Parameters.Scsi.Srb = srb;

                    // HACKHACK - REF #0001

                    //
                    // Set up IoCompletion routine address.
                    //

                    IoSetCompletionRoutine(Irp,
                                           CdRomXACompletion,
                                           srb,
                                           TRUE,
                                           TRUE,
                                           TRUE);

                    IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, Irp);
                    return;

                } else {

                    transferByteCount = (use6Byte) ? sizeof(ERROR_RECOVERY_DATA) : sizeof(ERROR_RECOVERY_DATA10);
                    dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                                       transferByteCount,
                                                       CDROM_TAG_RAW );
                    if (!dataBuffer) {
                        ExFreePool(senseBuffer);
                        ExFreePool(srb);
                        IoFreeIrp(irp2);
                        Irp->IoStatus.Information = 0;
                        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                        BAIL_OUT(Irp);
                        CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                        return;

                    }

                    irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                                    transferByteCount,
                                                    FALSE,
                                                    FALSE,
                                                    (PIRP) NULL);

                    if (!irp2->MdlAddress) {
                        ExFreePool(senseBuffer);
                        ExFreePool(srb);
                        ExFreePool(dataBuffer);
                        IoFreeIrp(irp2);
                        Irp->IoStatus.Information = 0;
                        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                        BAIL_OUT(Irp);
                        CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                        return;
                    }

                    //
                    // Prepare the MDL
                    //

                    MmBuildMdlForNonPagedPool(irp2->MdlAddress);

                    srb->DataBuffer = dataBuffer;

                    //
                    // Set the new block size in the descriptor.
                    // This will set the block read size to RAW_SECTOR_SIZE
                    // TODO: Set density code, based on operation
                    //

                    if (use6Byte) {
                        cdData->BlockDescriptor.BlockLength[0] = (UCHAR)(RAW_SECTOR_SIZE >> 16) & 0xFF;
                        cdData->BlockDescriptor.BlockLength[1] = (UCHAR)(RAW_SECTOR_SIZE >>  8) & 0xFF;
                        cdData->BlockDescriptor.BlockLength[2] = (UCHAR)(RAW_SECTOR_SIZE & 0xFF);
                        cdData->BlockDescriptor.DensityCode = 0;
                    } else {
                        cdData->BlockDescriptor10.BlockLength[0] = (UCHAR)(RAW_SECTOR_SIZE >> 16) & 0xFF;
                        cdData->BlockDescriptor10.BlockLength[1] = (UCHAR)(RAW_SECTOR_SIZE >>  8) & 0xFF;
                        cdData->BlockDescriptor10.BlockLength[2] = (UCHAR)(RAW_SECTOR_SIZE & 0xFF);
                        cdData->BlockDescriptor10.DensityCode = 0;
                    }

                    //
                    // Move error page into dataBuffer.
                    //

                    RtlCopyMemory(srb->DataBuffer, &cdData->Header, transferByteCount);


                    //
                    // Build and send a mode select to switch into raw mode.
                    //

                    srb->SrbFlags = fdoExtension->SrbFlags;
                    srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_OUT);
                    srb->DataTransferLength = transferByteCount;
                    srb->TimeOutValue = fdoExtension->TimeOutValue * 2;

                    if (use6Byte) {
                        srb->CdbLength = 6;
                        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
                        cdb->MODE_SELECT.PFBit = 1;
                        cdb->MODE_SELECT.ParameterListLength = (UCHAR)transferByteCount;
                    } else {

                        srb->CdbLength = 10;
                        cdb->MODE_SELECT10.OperationCode = SCSIOP_MODE_SELECT10;
                        cdb->MODE_SELECT10.PFBit = 1;
                        cdb->MODE_SELECT10.ParameterListLength[0] = (UCHAR)(transferByteCount >> 8);
                        cdb->MODE_SELECT10.ParameterListLength[1] = (UCHAR)(transferByteCount & 0xFF);
                    }

                    //
                    // Update completion routine.
                    //

                    IoSetCompletionRoutine(irp2,
                                           CdRomSwitchModeCompletion,
                                           srb,
                                           TRUE,
                                           TRUE,
                                           TRUE);

                }

            } else {

                PRAW_READ_INFO rawReadInfo =
                                   (PRAW_READ_INFO)currentIrpStack->Parameters.DeviceIoControl.Type3InputBuffer;
                ULONG  startingSector;

                //
                // Free the recently allocated irp, as we don't need it.
                //

                IoFreeIrp(irp2);

                cdb = (PCDB)srb->Cdb;
                RtlZeroMemory(cdb, CDB12GENERIC_LENGTH);


                //
                // Calculate starting offset.
                //

                startingSector = (ULONG)(rawReadInfo->DiskOffset.QuadPart >> fdoExtension->SectorShift);
                transferByteCount  = rawReadInfo->SectorCount * RAW_SECTOR_SIZE;


                srb->OriginalRequest = Irp;
                srb->SrbFlags = fdoExtension->SrbFlags;
                srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);
                srb->DataTransferLength = transferByteCount;
                srb->TimeOutValue = fdoExtension->TimeOutValue;
                srb->DataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
                srb->CdbLength = 12;
                srb->SrbStatus = srb->ScsiStatus = 0;

                //
                // Fill in CDB fields.
                //

                cdb = (PCDB)srb->Cdb;


                cdb->READ_CD.TransferBlocks[2]  = (UCHAR) (rawReadInfo->SectorCount & 0xFF);
                cdb->READ_CD.TransferBlocks[1]  = (UCHAR) (rawReadInfo->SectorCount >> 8 );
                cdb->READ_CD.TransferBlocks[0]  = (UCHAR) (rawReadInfo->SectorCount >> 16);


                cdb->READ_CD.StartingLBA[3]  = (UCHAR) (startingSector & 0xFF);
                cdb->READ_CD.StartingLBA[2]  = (UCHAR) ((startingSector >>  8));
                cdb->READ_CD.StartingLBA[1]  = (UCHAR) ((startingSector >> 16));
                cdb->READ_CD.StartingLBA[0]  = (UCHAR) ((startingSector >> 24));

                //
                // Setup cdb depending upon the sector type we want.
                //

                switch (rawReadInfo->TrackMode) {
                case CDDA:

                    cdb->READ_CD.ExpectedSectorType = CD_DA_SECTOR;
                    cdb->READ_CD.IncludeUserData = 1;
                    cdb->READ_CD.HeaderCode = 3;
                    cdb->READ_CD.IncludeSyncData = 1;
                    break;

                case YellowMode2:

                    cdb->READ_CD.ExpectedSectorType = YELLOW_MODE2_SECTOR;
                    cdb->READ_CD.IncludeUserData = 1;
                    cdb->READ_CD.HeaderCode = 1;
                    cdb->READ_CD.IncludeSyncData = 1;
                    break;

                case XAForm2:

                    cdb->READ_CD.ExpectedSectorType = FORM2_MODE2_SECTOR;
                    cdb->READ_CD.IncludeUserData = 1;
                    cdb->READ_CD.HeaderCode = 3;
                    cdb->READ_CD.IncludeSyncData = 1;
                    break;

                default:
                    ExFreePool(senseBuffer);
                    ExFreePool(srb);
                    Irp->IoStatus.Information = 0;
                    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

                    BAIL_OUT(Irp);
                    CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                    return;
                }

                cdb->READ_CD.OperationCode = SCSIOP_READ_CD;

                nextIrpStack->MajorFunction = IRP_MJ_SCSI;
                nextIrpStack->Parameters.Scsi.Srb = srb;

                // HACKHACK - REF #0001

                //
                // Set up IoCompletion routine address.
                //

                IoSetCompletionRoutine(Irp,
                                       CdRomXACompletion,
                                       srb,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, Irp);
                return;

            }

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        case IOCTL_CDROM_GET_DRIVE_GEOMETRY: {

            //
            // Issue ReadCapacity to update device extension
            // with information for current media.
            //

            DebugPrint((3, "CdRomStartIo: Get drive capacity\n"));

            //
            // setup remaining srb and cdb parameters.
            //

            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);
            srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);
            srb->CdbLength = 10;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                               sizeof(READ_CAPACITY_DATA),
                                               CDROM_TAG_READ_CAP);
            if (!dataBuffer) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;

            }

            irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                            sizeof(READ_CAPACITY_DATA),
                                            FALSE,
                                            FALSE,
                                            (PIRP) NULL);

            if (!irp2->MdlAddress) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                ExFreePool(dataBuffer);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Prepare the MDL
            //

            MmBuildMdlForNonPagedPool(irp2->MdlAddress);

            srb->DataBuffer = dataBuffer;
            cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        case IOCTL_CDROM_CHECK_VERIFY: {

            //
            // Since a test unit ready is about to be performed, reset the
            // timer value to decrease the opportunities for it to race with
            // this code.
            //

            ClassResetMediaChangeTimer(fdoExtension);

            //
            // Set up the SRB/CDB
            //

            srb->CdbLength = 6;
            cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
            srb->TimeOutValue = fdoExtension->TimeOutValue * 2;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
                              SRB_FLAGS_NO_DATA_TRANSFER);

            DebugPrint((2, "CdRomStartIo: [%p] Sending CHECK_VERIFY irp %p\n",
                        Irp, irp2));
            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        case IOCTL_DVD_READ_STRUCTURE: {

            CdRomDeviceControlDvdReadStructure(Fdo, Irp, irp2, srb);
            return;

        }

        case IOCTL_DVD_END_SESSION: {
            CdRomDeviceControlDvdEndSession(Fdo, Irp, irp2, srb);
            return;
        }

        case IOCTL_DVD_START_SESSION:
        case IOCTL_DVD_READ_KEY: {

            CdRomDeviceControlDvdStartSessionReadKey(Fdo, Irp, irp2, srb);
            return;

        }


        case IOCTL_DVD_SEND_KEY:
        case IOCTL_DVD_SEND_KEY2: {

            CdRomDeviceControlDvdSendKey (Fdo, Irp, irp2, srb);
            return;


        }

        case IOCTL_CDROM_GET_LAST_SESSION:

            //
            // Set format to return first and last session numbers.
            //

            cdb->READ_TOC.Format = GET_LAST_SESSION;

            //
            // Fall through to READ TOC code.
            //

        case IOCTL_CDROM_READ_TOC: {


            if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_READ_TOC) {

                //
                // Use MSF addressing if not request for session information.
                //

                cdb->READ_TOC.Msf = CDB_USE_MSF;
            }

            //
            // Set size of TOC structure.
            //

            transferByteCount =
                currentIrpStack->Parameters.Read.Length >
                    sizeof(CDROM_TOC) ? sizeof(CDROM_TOC):
                    currentIrpStack->Parameters.Read.Length;

            cdb->READ_TOC.AllocationLength[0] = (UCHAR) (transferByteCount >> 8);
            cdb->READ_TOC.AllocationLength[1] = (UCHAR) (transferByteCount & 0xFF);

            cdb->READ_TOC.Control = 0;

            //
            // Start at beginning of disc.
            //

            cdb->READ_TOC.StartingTrack = 0;

            //
            // setup remaining srb and cdb parameters.
            //

            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);
            srb->DataTransferLength = transferByteCount;
            srb->CdbLength = 10;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                               transferByteCount,
                                               CDROM_TAG_TOC);
            if (!dataBuffer) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;

            }

            irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                            transferByteCount,
                                            FALSE,
                                            FALSE,
                                            (PIRP) NULL);

            if (!irp2->MdlAddress) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                ExFreePool(dataBuffer);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Prepare the MDL
            //

            MmBuildMdlForNonPagedPool(irp2->MdlAddress);

            srb->DataBuffer = dataBuffer;
            cdb->READ_TOC.OperationCode = SCSIOP_READ_TOC;

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;

        }

        case IOCTL_CDROM_PLAY_AUDIO_MSF: {

            PCDROM_PLAY_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

            //
            // Set up the SRB/CDB
            //

            srb->CdbLength = 10;
            cdb->PLAY_AUDIO_MSF.OperationCode = SCSIOP_PLAY_AUDIO_MSF;

            cdb->PLAY_AUDIO_MSF.StartingM = inputBuffer->StartingM;
            cdb->PLAY_AUDIO_MSF.StartingS = inputBuffer->StartingS;
            cdb->PLAY_AUDIO_MSF.StartingF = inputBuffer->StartingF;

            cdb->PLAY_AUDIO_MSF.EndingM = inputBuffer->EndingM;
            cdb->PLAY_AUDIO_MSF.EndingS = inputBuffer->EndingS;
            cdb->PLAY_AUDIO_MSF.EndingF = inputBuffer->EndingF;

            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_NO_DATA_TRANSFER);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;

        }

        case IOCTL_CDROM_READ_Q_CHANNEL: {

            PSUB_Q_CHANNEL_DATA userChannelData =
                             Irp->AssociatedIrp.SystemBuffer;
            PCDROM_SUB_Q_DATA_FORMAT inputBuffer =
                             Irp->AssociatedIrp.SystemBuffer;

            //
            // Allocate buffer for subq channel information.
            //

            dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                               sizeof(SUB_Q_CHANNEL_DATA),
                                               CDROM_TAG_SUB_Q);

            if (!dataBuffer) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;

            }

            irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                             sizeof(SUB_Q_CHANNEL_DATA),
                                             FALSE,
                                             FALSE,
                                             (PIRP) NULL);

            if (!irp2->MdlAddress) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                ExFreePool(dataBuffer);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Prepare the MDL
            //

            MmBuildMdlForNonPagedPool(irp2->MdlAddress);

            srb->DataBuffer = dataBuffer;

            //
            // Always logical unit 0, but only use MSF addressing
            // for IOCTL_CDROM_CURRENT_POSITION
            //

            if (inputBuffer->Format==IOCTL_CDROM_CURRENT_POSITION)
                cdb->SUBCHANNEL.Msf = CDB_USE_MSF;

            //
            // Return subchannel data
            //

            cdb->SUBCHANNEL.SubQ = CDB_SUBCHANNEL_BLOCK;

            //
            // Specify format of informatin to return
            //

            cdb->SUBCHANNEL.Format = inputBuffer->Format;

            //
            // Specify which track to access (only used by Track ISRC reads)
            //

            if (inputBuffer->Format==IOCTL_CDROM_TRACK_ISRC) {
                cdb->SUBCHANNEL.TrackNumber = inputBuffer->Track;
            }

            //
            // Set size of channel data -- however, this is dependent on
            // what information we are requesting (which Format)
            //

            switch( inputBuffer->Format ) {

                case IOCTL_CDROM_CURRENT_POSITION:
                    transferByteCount = sizeof(SUB_Q_CURRENT_POSITION);
                    break;

                case IOCTL_CDROM_MEDIA_CATALOG:
                    transferByteCount = sizeof(SUB_Q_MEDIA_CATALOG_NUMBER);
                    break;

                case IOCTL_CDROM_TRACK_ISRC:
                    transferByteCount = sizeof(SUB_Q_TRACK_ISRC);
                    break;
            }

            cdb->SUBCHANNEL.AllocationLength[0] = (UCHAR) (transferByteCount >> 8);
            cdb->SUBCHANNEL.AllocationLength[1] = (UCHAR) (transferByteCount &  0xFF);
            cdb->SUBCHANNEL.OperationCode = SCSIOP_READ_SUB_CHANNEL;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);
            srb->DataTransferLength = transferByteCount;
            srb->CdbLength = 10;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;

        }

        case IOCTL_CDROM_PAUSE_AUDIO: {

            cdb->PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;
            cdb->PAUSE_RESUME.Action = CDB_AUDIO_PAUSE;

            srb->CdbLength = 10;
            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_NO_DATA_TRANSFER);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        case IOCTL_CDROM_RESUME_AUDIO: {

            cdb->PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;
            cdb->PAUSE_RESUME.Action = CDB_AUDIO_RESUME;

            srb->CdbLength = 10;
            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_NO_DATA_TRANSFER);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        case IOCTL_CDROM_SEEK_AUDIO_MSF: {

            PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;
            ULONG                 logicalBlockAddress;

            logicalBlockAddress = MSF_TO_LBA(inputBuffer->M, inputBuffer->S, inputBuffer->F);

            cdb->SEEK.OperationCode      = SCSIOP_SEEK;
            cdb->SEEK.LogicalBlockAddress[0] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte3;
            cdb->SEEK.LogicalBlockAddress[1] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte2;
            cdb->SEEK.LogicalBlockAddress[2] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte1;
            cdb->SEEK.LogicalBlockAddress[3] = ((PFOUR_BYTE)&logicalBlockAddress)->Byte0;

            srb->CdbLength = 10;
            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_NO_DATA_TRANSFER);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;

        }

        case IOCTL_CDROM_STOP_AUDIO: {

            cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
            cdb->START_STOP.Immediate = 1;
            cdb->START_STOP.Start = 0;
            cdb->START_STOP.LoadEject = 0;

            srb->CdbLength = 6;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_NO_DATA_TRANSFER);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        case IOCTL_CDROM_GET_CONTROL: {

            PAUDIO_OUTPUT audioOutput;
            PCDROM_AUDIO_CONTROL audioControl = Irp->AssociatedIrp.SystemBuffer;

            //
            // Allocate buffer for volume control information.
            //

            dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                               MODE_DATA_SIZE,
                                               CDROM_TAG_VOLUME);

            if (!dataBuffer) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;

            }

            irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                            MODE_DATA_SIZE,
                                            FALSE,
                                            FALSE,
                                            (PIRP) NULL);

            if (!irp2->MdlAddress) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                ExFreePool(dataBuffer);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Prepare the MDL
            //

            MmBuildMdlForNonPagedPool(irp2->MdlAddress);
            srb->DataBuffer = dataBuffer;

            RtlZeroMemory(dataBuffer, MODE_DATA_SIZE);

            //
            // Setup for either 6 or 10 byte CDBs.
            //

            if (use6Byte) {

                cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
                cdb->MODE_SENSE.PageCode = CDROM_AUDIO_CONTROL_PAGE;
                cdb->MODE_SENSE.AllocationLength = MODE_DATA_SIZE;

                //
                // Disable block descriptors.
                //

                cdb->MODE_SENSE.Dbd = TRUE;

                srb->CdbLength = 6;
            } else {

                cdb->MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
                cdb->MODE_SENSE10.PageCode = CDROM_AUDIO_CONTROL_PAGE;
                cdb->MODE_SENSE10.AllocationLength[0] = (UCHAR)(MODE_DATA_SIZE >> 8);
                cdb->MODE_SENSE10.AllocationLength[1] = (UCHAR)(MODE_DATA_SIZE & 0xFF);

                //
                // Disable block descriptors.
                //

                cdb->MODE_SENSE10.Dbd = TRUE;

                srb->CdbLength = 10;
            }

            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->DataTransferLength = MODE_DATA_SIZE;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;

        }

        case IOCTL_CDROM_GET_VOLUME:
        case IOCTL_CDROM_SET_VOLUME: {

            dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                        MODE_DATA_SIZE,
                                        CDROM_TAG_VOLUME);

            if (!dataBuffer) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            irp2->MdlAddress = IoAllocateMdl(dataBuffer,
                                            MODE_DATA_SIZE,
                                            FALSE,
                                            FALSE,
                                            (PIRP) NULL);

            if (!irp2->MdlAddress) {
                ExFreePool(senseBuffer);
                ExFreePool(srb);
                ExFreePool(dataBuffer);
                IoFreeIrp(irp2);
                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                BAIL_OUT(Irp);
                CdRomCompleteIrpAndStartNextPacketSafely(Fdo, Irp);
                return;
            }

            //
            // Prepare the MDL
            //

            MmBuildMdlForNonPagedPool(irp2->MdlAddress);
            srb->DataBuffer = dataBuffer;

            RtlZeroMemory(dataBuffer, MODE_DATA_SIZE);


            if (use6Byte) {
                cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
                cdb->MODE_SENSE.PageCode = CDROM_AUDIO_CONTROL_PAGE;
                cdb->MODE_SENSE.AllocationLength = MODE_DATA_SIZE;

                srb->CdbLength = 6;

            } else {

                cdb->MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
                cdb->MODE_SENSE10.PageCode = CDROM_AUDIO_CONTROL_PAGE;
                cdb->MODE_SENSE10.AllocationLength[0] = (UCHAR)(MODE_DATA_SIZE >> 8);
                cdb->MODE_SENSE10.AllocationLength[1] = (UCHAR)(MODE_DATA_SIZE & 0xFF);

                srb->CdbLength = 10;
            }

            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->DataTransferLength = MODE_DATA_SIZE;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);

            if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_SET_VOLUME) {

                //
                // Setup a different completion routine as the mode sense data is needed in order
                // to send the mode select.
                //

                IoSetCompletionRoutine(irp2,
                                       CdRomSetVolumeIntermediateCompletion,
                                       srb,
                                       TRUE,
                                       TRUE,
                                       TRUE);

            }

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;

        }

        case IOCTL_STORAGE_SET_READ_AHEAD: {

            PSTORAGE_SET_READ_AHEAD readAhead = Irp->AssociatedIrp.SystemBuffer;

            ULONG blockAddress;
            PFOUR_BYTE fourByte = (PFOUR_BYTE) &blockAddress;

            //
            // setup the SRB for a set readahead command
            //

            cdb->SET_READ_AHEAD.OperationCode = SCSIOP_SET_READ_AHEAD;

            blockAddress = (ULONG) (readAhead->TriggerAddress.QuadPart >>
                                    fdoExtension->SectorShift);

            cdb->SET_READ_AHEAD.TriggerLBA[0] = fourByte->Byte3;
            cdb->SET_READ_AHEAD.TriggerLBA[1] = fourByte->Byte2;
            cdb->SET_READ_AHEAD.TriggerLBA[2] = fourByte->Byte1;
            cdb->SET_READ_AHEAD.TriggerLBA[3] = fourByte->Byte0;

            blockAddress = (ULONG) (readAhead->TargetAddress.QuadPart >>
                                    fdoExtension->SectorShift);

            cdb->SET_READ_AHEAD.ReadAheadLBA[0] = fourByte->Byte3;
            cdb->SET_READ_AHEAD.ReadAheadLBA[1] = fourByte->Byte2;
            cdb->SET_READ_AHEAD.ReadAheadLBA[2] = fourByte->Byte1;
            cdb->SET_READ_AHEAD.ReadAheadLBA[3] = fourByte->Byte0;

            srb->CdbLength = 12;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
                              SRB_FLAGS_NO_DATA_TRANSFER);

            IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp2);
            return;
        }

        default: {

            UCHAR uniqueAddress;

            //
            // Just complete the request - CdRomClassIoctlCompletion will take
            // care of it for us
            //

            //
            // Acquire a new copy of the lock so that ClassCompleteRequest
            // doesn't freak when we complete the other request while holding
            // the lock.
            //

            //
            // NOTE: CdRomDeviceControl/CdRomDeviceControlCompletion
            //       wait for the event and eventually calls
            //       IoStartNextPacket()
            //

            ASSERT(irp2);
            ASSERT(senseBuffer);
            ASSERT(srb);

            ExFreePool(srb);
            ExFreePool(senseBuffer);
            IoFreeIrp(irp2);



            ClassAcquireRemoveLock(Fdo, (PIRP)&uniqueAddress);
            ClassReleaseRemoveLock(Fdo, Irp);
            ClassCompleteRequest(Fdo, Irp, IO_NO_INCREMENT);
            ClassReleaseRemoveLock(Fdo, (PIRP)&uniqueAddress);
            return;
        }

        } // end switch()
    }

    //
    // If a read or an unhandled IRP_MJ_XX, end up here. The unhandled IRP_MJ's
    // are expected and composed of AutoRun Irps, at present.
    //

    IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, Irp);
    return;
}


NTSTATUS
CdRomReadVerification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the entry called by the I/O system for read requests.
    It builds the SRB and sends it to the port driver.

Arguments:

    DeviceObject - the system object for the device.
    Irp - IRP involved.

Return Value:

    NT Status

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG               transferByteCount = currentIrpStack->Parameters.Read.Length;
    LARGE_INTEGER       startingOffset = currentIrpStack->Parameters.Read.ByteOffset;

    PCDROM_DATA         cdData = (PCDROM_DATA)(commonExtension->DriverData);

    SCSI_REQUEST_BLOCK  srb;
    PCDB                cdb = (PCDB)srb.Cdb;
    NTSTATUS            status;

    PAGED_CODE();

    //
    // if this is a write request then reject it.
    //

    if (currentIrpStack->MajorFunction != IRP_MJ_READ) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // If the cd is playing music then reject this request.
    //

    if (PLAY_ACTIVE(fdoExtension)) {
        Irp->IoStatus.Status = STATUS_DEVICE_BUSY;
        return STATUS_DEVICE_BUSY;
    }

    //
    // Verify parameters of this request.
    // Check that ending sector is on disc and
    // that number of bytes to transfer is a multiple of
    // the sector size.
    //

    startingOffset.QuadPart = currentIrpStack->Parameters.Read.ByteOffset.QuadPart +
                              transferByteCount;

    if (!fdoExtension->DiskGeometry.BytesPerSector) {
        fdoExtension->DiskGeometry.BytesPerSector = 2048;
    }

    if ((startingOffset.QuadPart > commonExtension->PartitionLength.QuadPart) ||
        (transferByteCount & fdoExtension->DiskGeometry.BytesPerSector - 1)) {

        //
        // Fail request with status of invalid parameters.
        //

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

        return STATUS_INVALID_PARAMETER;
    }


    return STATUS_SUCCESS;

} // end CdRomReadVerification()


NTSTATUS
CdRomDeviceControlCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PCDROM_DATA         cdData = (PCDROM_DATA)(commonExtension->DriverData);
    BOOLEAN             use6Byte = TEST_FLAG(cdData->XAFlags, XA_USE_6_BYTE);

    PIO_STACK_LOCATION  irpStack        = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION  realIrpStack;
    PIO_STACK_LOCATION  realIrpNextStack;

    PSCSI_REQUEST_BLOCK srb     = Context;

    PIRP                realIrp = NULL;

    NTSTATUS            status;
    BOOLEAN             retry;

    //
    // This code must run at DISPATCH_LEVEL to call IoStartNextPacket()
    // The storage stack guarantees this, but this explicitly checks
    // that assumption.
    //

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    //
    // Extract the 'real' irp from the irpstack.
    //

    realIrp = (PIRP) irpStack->Parameters.Others.Argument2;
    realIrpStack = IoGetCurrentIrpStackLocation(realIrp);
    realIrpNextStack = IoGetNextIrpStackLocation(realIrp);

    //
    // check that we've really got the correct irp
    //

    ASSERT(realIrpNextStack->Parameters.Others.Argument3 == Irp);

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        ULONG retryInterval;

        DebugPrint((2,
                    "CdRomDeviceControlCompletion: Irp %p, Srb %p Real Irp %p Status %lx\n",
                    Irp,
                    srb,
                    realIrp,
                    srb->SrbStatus));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            DebugPrint((2, "CdRomDeviceControlCompletion: Releasing Queue\n"));
            ClassReleaseQueue(DeviceObject);
        }


        retry = ClassInterpretSenseInfo(DeviceObject,
                                        srb,
                                        irpStack->MajorFunction,
                                        irpStack->Parameters.DeviceIoControl.IoControlCode,
                                        MAXIMUM_RETRIES - ((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1),
                                        &status,
                                        &retryInterval);

        DebugPrint((2, "CdRomDeviceControlCompletion: IRP will %sbe retried\n",
                    (retry ? "" : "not ")));

        //
        // Some of the Device Controls need special cases on non-Success status's.
        //

        if (realIrpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
            if ((realIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_GET_LAST_SESSION) ||
                (realIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_READ_TOC)         ||
                (realIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_GET_CONTROL)      ||
                (realIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_GET_VOLUME)) {

                if (status == STATUS_DATA_OVERRUN) {
                    status = STATUS_SUCCESS;
                    retry = FALSE;
                }
            }

            if (realIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_READ_Q_CHANNEL) {
                PLAY_ACTIVE(fdoExtension) = FALSE;
            }
        }

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (realIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            if (((realIrpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) ||
                 (realIrpStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL)
                 ) &&
                (realIrpStack->Parameters.DeviceIoControl.IoControlCode ==
                 IOCTL_CDROM_CHECK_VERIFY)
                ) {

                //
                // Update the geometry information, as the media could have
                // changed. The completion routine for this will complete
                // the real irp and start the next packet.
                //

                ExFreePool(srb->SenseInfoBuffer);
                if (srb->DataBuffer) {
                    ExFreePool(srb->DataBuffer);
                }
                ExFreePool(srb);
                if (Irp->MdlAddress) {
                    ExFreePool(Irp->MdlAddress);
                }

                status = CdRomUpdateCapacity(fdoExtension, realIrp, NULL);
                DebugPrint((2, "CdRomDeviceControlCompletion: [%p] "
                            "CdRomUpdateCapacity completed with status %lx\n",
                            realIrp, status));
                ASSERT(status == STATUS_PENDING);

                return STATUS_MORE_PROCESSING_REQUIRED;

            } else {

                status = STATUS_IO_DEVICE_ERROR;
                retry = TRUE;
            }

        }

        if (retry && ((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1)--) {


            if (((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1)) {

                //
                // Retry request.
                //

                DebugPrint((1, "Retry request %p - Calling StartIo\n", Irp));


                ExFreePool(srb->SenseInfoBuffer);
                if (srb->DataBuffer) {
                    ExFreePool(srb->DataBuffer);
                }
                ExFreePool(srb);
                if (Irp->MdlAddress) {
                    IoFreeMdl(Irp->MdlAddress);
                }

                realIrpNextStack->Parameters.Others.Argument3 = (PVOID)-1;
                IoFreeIrp(Irp);

                CdRomRetryRequest(fdoExtension, realIrp, retryInterval, FALSE);
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

            //
            // Exhausted retries. Fall through and complete the request with
            // the appropriate status.
            //

        }
    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {

        switch (realIrpStack->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_CDROM_GET_DRIVE_GEOMETRY: {

            PREAD_CAPACITY_DATA readCapacityBuffer = srb->DataBuffer;
            ULONG               lastSector;
            ULONG               bps;
            ULONG               lastBit;
            ULONG               tmp;

            //
            // Swizzle bytes from Read Capacity and translate into
            // the necessary geometry information in the device extension.
            //

            tmp = readCapacityBuffer->BytesPerBlock;
            ((PFOUR_BYTE)&bps)->Byte0 = ((PFOUR_BYTE)&tmp)->Byte3;
            ((PFOUR_BYTE)&bps)->Byte1 = ((PFOUR_BYTE)&tmp)->Byte2;
            ((PFOUR_BYTE)&bps)->Byte2 = ((PFOUR_BYTE)&tmp)->Byte1;
            ((PFOUR_BYTE)&bps)->Byte3 = ((PFOUR_BYTE)&tmp)->Byte0;

            //
            // Insure that bps is a power of 2.
            // This corrects a problem with the HP 4020i CDR where it
            // returns an incorrect number for bytes per sector.
            //

            if (!bps) {
                bps = 2048;
            } else {
                lastBit = (ULONG) -1;
                while (bps) {
                    lastBit++;
                    bps = bps >> 1;
                }

                bps = 1 << lastBit;
            }

            fdoExtension->DiskGeometry.BytesPerSector = bps;

            DebugPrint((2,
                        "CdRomDeviceControlCompletion: Calculated bps %#x\n",
                        fdoExtension->DiskGeometry.BytesPerSector));

            //
            // Copy last sector in reverse byte order.
            //

            tmp = readCapacityBuffer->LogicalBlockAddress;
            ((PFOUR_BYTE)&lastSector)->Byte0 = ((PFOUR_BYTE)&tmp)->Byte3;
            ((PFOUR_BYTE)&lastSector)->Byte1 = ((PFOUR_BYTE)&tmp)->Byte2;
            ((PFOUR_BYTE)&lastSector)->Byte2 = ((PFOUR_BYTE)&tmp)->Byte1;
            ((PFOUR_BYTE)&lastSector)->Byte3 = ((PFOUR_BYTE)&tmp)->Byte0;

            //
            // Calculate sector to byte shift.
            //

            WHICH_BIT(bps, fdoExtension->SectorShift);

            DebugPrint((2,"SCSI ClassReadDriveCapacity: Sector size is %d\n",
                fdoExtension->DiskGeometry.BytesPerSector));

            DebugPrint((2,"SCSI ClassReadDriveCapacity: Number of Sectors is %d\n",
                lastSector + 1));

            //
            // Calculate media capacity in bytes.
            //

            commonExtension->PartitionLength.QuadPart = (LONGLONG)(lastSector + 1);

            //
            // Calculate number of cylinders.
            //

            fdoExtension->DiskGeometry.Cylinders.QuadPart = (LONGLONG)((lastSector + 1)/(32 * 64));

            commonExtension->PartitionLength.QuadPart =
                (commonExtension->PartitionLength.QuadPart << fdoExtension->SectorShift);

            if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {

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

            //
            // Assume sectors per track are 32;
            //

            fdoExtension->DiskGeometry.SectorsPerTrack = 32;

            //
            // Assume tracks per cylinder (number of heads) is 64.
            //

            fdoExtension->DiskGeometry.TracksPerCylinder = 64;

            //
            // Copy the device extension's geometry info into the user buffer.
            //

            RtlMoveMemory(realIrp->AssociatedIrp.SystemBuffer,
                          &(fdoExtension->DiskGeometry),
                          sizeof(DISK_GEOMETRY));

            //
            // update information field.
            //

            realIrp->IoStatus.Information = sizeof(DISK_GEOMETRY);
            break;
        }

        case IOCTL_CDROM_CHECK_VERIFY: {

            if((realIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_CHECK_VERIFY) &&
               (realIrpStack->Parameters.DeviceIoControl.OutputBufferLength)) {

                *((PULONG)realIrp->AssociatedIrp.SystemBuffer) =
                    commonExtension->PartitionZeroExtension->MediaChangeCount;

                realIrp->IoStatus.Information = sizeof(ULONG);
            } else {
                realIrp->IoStatus.Information = 0;
            }

            DebugPrint((2, "CdRomDeviceControlCompletion: [%p] completing CHECK_VERIFY buddy irp %p\n", realIrp, Irp));
            break;
        }

        case IOCTL_CDROM_GET_LAST_SESSION:
        case IOCTL_CDROM_READ_TOC: {

                PCDROM_TOC toc = srb->DataBuffer;

                //
                // Copy the device extension's geometry info into the user buffer.
                //

                RtlMoveMemory(realIrp->AssociatedIrp.SystemBuffer,
                              toc,
                              srb->DataTransferLength);

                //
                // update information field.
                //

                realIrp->IoStatus.Information = srb->DataTransferLength;
                break;
            }

        case IOCTL_DVD_READ_STRUCTURE: {

            DVD_STRUCTURE_FORMAT format = ((PDVD_READ_STRUCTURE) realIrp->AssociatedIrp.SystemBuffer)->Format;

            PDVD_DESCRIPTOR_HEADER header = realIrp->AssociatedIrp.SystemBuffer;

            FOUR_BYTE fourByte;
            PTWO_BYTE twoByte;
            UCHAR tmp;

            DebugPrint((2, "DvdDeviceControlCompletion - IOCTL_DVD_READ_STRUCTURE: completing irp %p (buddy %p)\n",
                           Irp,
                           realIrp));

            DebugPrint((2, "DvdDCCompletion - READ_STRUCTURE: descriptor format of %d\n", format));

            RtlMoveMemory(header,
                          srb->DataBuffer,
                          srb->DataTransferLength);

            //
            // Cook the data.  There are a number of fields that really
            // should be byte-swapped for the caller.
            //

            DebugPrint((4, "DvdDCCompletion - READ_STRUCTURE:\n"
                        "\tHeader at %p\n"
                        "\tDvdDCCompletion - READ_STRUCTURE: data at %p\n"
                        "\tDataBuffer was at %p\n"
                        "\tDataTransferLength was %lx\n",
                        header,
                        header->Data,
                        srb->DataBuffer,
                        srb->DataTransferLength));

            //
            // First the fields in the header
            //

            DebugPrint((4, "READ_STRUCTURE: header->Length %lx -> ",
                           header->Length));
            REVERSE_SHORT(&header->Length);
            DebugPrint((4, "%lx\n", header->Length));

            //
            // Now the fields in the descriptor
            //

            if(format == DvdPhysicalDescriptor) {

                PDVD_LAYER_DESCRIPTOR layer = (PDVD_LAYER_DESCRIPTOR) &(header->Data[0]);

                DebugPrint((4, "READ_STRUCTURE: layer->Length %lx -> ",
                               layer->Length));
                REVERSE_SHORT(&layer->Length);
                DebugPrint((4, "%lx\n", layer->Length));

                DebugPrint((4, "READ_STRUCTURE: StartingDataSector %lx -> ",
                               layer->StartingDataSector));
                REVERSE_LONG(&(layer->StartingDataSector));
                DebugPrint((4, "%lx\n", layer->StartingDataSector));

                DebugPrint((4, "READ_STRUCTURE: EndDataSector %lx -> ",
                               layer->EndDataSector));
                REVERSE_LONG(&(layer->EndDataSector));
                DebugPrint((4, "%lx\n", layer->EndDataSector));

                DebugPrint((4, "READ_STRUCTURE: EndLayerZeroSector %lx -> ",
                               layer->EndLayerZeroSector));
                REVERSE_LONG(&(layer->EndLayerZeroSector));
                DebugPrint((4, "%lx\n", layer->EndLayerZeroSector));
            }

            DebugPrint((2, "Status is %lx\n", Irp->IoStatus.Status));
            DebugPrint((2, "DvdDeviceControlCompletion - "
                        "IOCTL_DVD_READ_STRUCTURE: data transfer length of %d\n",
                        srb->DataTransferLength));

            realIrp->IoStatus.Information = srb->DataTransferLength;
            break;
        }

        case IOCTL_DVD_READ_KEY: {

            PDVD_COPY_PROTECT_KEY copyProtectKey = realIrp->AssociatedIrp.SystemBuffer;

            PCDVD_KEY_HEADER keyHeader = srb->DataBuffer;
            ULONG dataLength;

            ULONG transferLength = srb->DataTransferLength - 2;

            //
            // Adjust the data length to ignore the two reserved bytes in the
            // header then check to make sure the drive didn't return too
            // large of a key.
            //

            dataLength = (keyHeader->DataLength[0] << 8) +
                         keyHeader->DataLength[1];
            dataLength -= 2;

            if(dataLength < transferLength) {
                transferLength = dataLength;
            }

            DebugPrint((2, "DvdDeviceControlCompletion: [%p] - READ_KEY with "
                        "transfer length of (%d or %d) bytes\n",
                        Irp,
                        dataLength,
                        srb->DataTransferLength - 2));

            //
            // Copy the key data into the return buffer
            //
            if(copyProtectKey->KeyType == DvdTitleKey) {

                RtlMoveMemory(copyProtectKey->KeyData,
                              keyHeader->Data + 1,
                              transferLength - 1);
                copyProtectKey->KeyData[transferLength - 1] = 0;

                //
                // If this is a title key then we need to copy the CGMS flags
                // as well.
                //
                copyProtectKey->KeyFlags = *(keyHeader->Data);

            } else {

                RtlMoveMemory(copyProtectKey->KeyData,
                              keyHeader->Data,
                              transferLength);
            }

            copyProtectKey->KeyLength = sizeof(DVD_COPY_PROTECT_KEY);
            copyProtectKey->KeyLength += transferLength;

            realIrp->IoStatus.Information = copyProtectKey->KeyLength;
            break;
        }

        case IOCTL_DVD_START_SESSION: {

            PDVD_SESSION_ID sessionId = realIrp->AssociatedIrp.SystemBuffer;

            PCDVD_KEY_HEADER keyHeader = srb->DataBuffer;
            PCDVD_REPORT_AGID_DATA keyData = (PCDVD_REPORT_AGID_DATA) keyHeader->Data;

            *sessionId = keyData->AGID;

            realIrp->IoStatus.Information = sizeof(DVD_SESSION_ID);

            break;
        }

        case IOCTL_DVD_END_SESSION:
        case IOCTL_DVD_SEND_KEY:
        case IOCTL_DVD_SEND_KEY2:

            //
            // nothing to return
            //
            realIrp->IoStatus.Information = 0;
            break;

        case IOCTL_CDROM_PLAY_AUDIO_MSF:

            PLAY_ACTIVE(fdoExtension) = TRUE;

            break;

        case IOCTL_CDROM_READ_Q_CHANNEL: {

            PSUB_Q_CHANNEL_DATA userChannelData = realIrp->AssociatedIrp.SystemBuffer;
            PCDROM_SUB_Q_DATA_FORMAT inputBuffer = realIrp->AssociatedIrp.SystemBuffer;
            PSUB_Q_CHANNEL_DATA subQPtr = srb->DataBuffer;

#if DBG
            switch( inputBuffer->Format ) {

            case IOCTL_CDROM_CURRENT_POSITION:
                DebugPrint((2,"CdRomDeviceControlCompletion: Audio Status is %u\n", subQPtr->CurrentPosition.Header.AudioStatus ));
                DebugPrint((2,"CdRomDeviceControlCompletion: ADR = 0x%x\n", subQPtr->CurrentPosition.ADR ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Control = 0x%x\n", subQPtr->CurrentPosition.Control ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Track = %u\n", subQPtr->CurrentPosition.TrackNumber ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Index = %u\n", subQPtr->CurrentPosition.IndexNumber ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Absolute Address = %x\n", *((PULONG)subQPtr->CurrentPosition.AbsoluteAddress) ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Relative Address = %x\n", *((PULONG)subQPtr->CurrentPosition.TrackRelativeAddress) ));
                break;

            case IOCTL_CDROM_MEDIA_CATALOG:
                DebugPrint((2,"CdRomDeviceControlCompletion: Audio Status is %u\n", subQPtr->MediaCatalog.Header.AudioStatus ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Mcval is %u\n", subQPtr->MediaCatalog.Mcval ));
                break;

            case IOCTL_CDROM_TRACK_ISRC:
                DebugPrint((2,"CdRomDeviceControlCompletion: Audio Status is %u\n", subQPtr->TrackIsrc.Header.AudioStatus ));
                DebugPrint((2,"CdRomDeviceControlCompletion: Tcval is %u\n", subQPtr->TrackIsrc.Tcval ));
                break;

            }
#endif

            //
            // Update the play active status.
            //

            if (subQPtr->CurrentPosition.Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS) {

                PLAY_ACTIVE(fdoExtension) = TRUE;

            } else {

                PLAY_ACTIVE(fdoExtension) = FALSE;

            }

            //
            // Check if output buffer is large enough to contain
            // the data.
            //

            if (realIrpStack->Parameters.DeviceIoControl.OutputBufferLength <
                srb->DataTransferLength) {

                srb->DataTransferLength =
                    realIrpStack->Parameters.DeviceIoControl.OutputBufferLength;
            }

            //
            // Copy our buffer into users.
            //

            RtlMoveMemory(userChannelData,
                          subQPtr,
                          srb->DataTransferLength);

            realIrp->IoStatus.Information = srb->DataTransferLength;
            break;
        }

        case IOCTL_CDROM_PAUSE_AUDIO:

            PLAY_ACTIVE(fdoExtension) = FALSE;
            realIrp->IoStatus.Information = 0;
            break;

        case IOCTL_CDROM_RESUME_AUDIO:

            realIrp->IoStatus.Information = 0;
            break;

        case IOCTL_CDROM_SEEK_AUDIO_MSF:

            realIrp->IoStatus.Information = 0;
            break;

        case IOCTL_CDROM_STOP_AUDIO:

            PLAY_ACTIVE(fdoExtension) = FALSE;

            realIrp->IoStatus.Information = 0;
            break;

        case IOCTL_CDROM_GET_CONTROL: {

            PCDROM_AUDIO_CONTROL audioControl = srb->DataBuffer;
            PAUDIO_OUTPUT        audioOutput;
            ULONG                bytesTransferred;

            audioOutput = ClassFindModePage((PCHAR)audioControl,
                                            srb->DataTransferLength,
                                            CDROM_AUDIO_CONTROL_PAGE,
                                            use6Byte);
            //
            // Verify the page is as big as expected.
            //

            bytesTransferred = (ULONG)((PCHAR) audioOutput - (PCHAR) audioControl) +
                               sizeof(AUDIO_OUTPUT);

            if (audioOutput != NULL &&
                srb->DataTransferLength >= bytesTransferred) {

                audioControl->LbaFormat = audioOutput->LbaFormat;

                audioControl->LogicalBlocksPerSecond =
                    (audioOutput->LogicalBlocksPerSecond[0] << (UCHAR)8) |
                    audioOutput->LogicalBlocksPerSecond[1];

                realIrp->IoStatus.Information = sizeof(CDROM_AUDIO_CONTROL);

            } else {
                realIrp->IoStatus.Information = 0;
                status = STATUS_INVALID_DEVICE_REQUEST;
            }
            break;
        }

        case IOCTL_CDROM_GET_VOLUME: {

            PAUDIO_OUTPUT audioOutput;
            PVOLUME_CONTROL volumeControl = srb->DataBuffer;
            ULONG i,bytesTransferred;

            audioOutput = ClassFindModePage((PCHAR)volumeControl,
                                                 srb->DataTransferLength,
                                                 CDROM_AUDIO_CONTROL_PAGE,
                                                 use6Byte);

            //
            // Verify the page is as big as expected.
            //

            bytesTransferred = (ULONG)((PCHAR) audioOutput - (PCHAR) volumeControl) +
                               sizeof(AUDIO_OUTPUT);

            if (audioOutput != NULL &&
                srb->DataTransferLength >= bytesTransferred) {

                for (i=0; i<4; i++) {
                    volumeControl->PortVolume[i] =
                        audioOutput->PortOutput[i].Volume;
                }

                //
                // Set bytes transferred in IRP.
                //

                realIrp->IoStatus.Information = sizeof(VOLUME_CONTROL);

            } else {
                realIrp->IoStatus.Information = 0;
                status = STATUS_INVALID_DEVICE_REQUEST;
            }

            break;
        }

        case IOCTL_CDROM_SET_VOLUME:

            realIrp->IoStatus.Information = 0;
            break;

        default:

            ASSERT(FALSE);
            realIrp->IoStatus.Information = 0;
            status = STATUS_INVALID_DEVICE_REQUEST;

        } // end switch()
    }

    //
    // Deallocate srb and sense buffer.
    //

    if (srb) {
        if (srb->DataBuffer) {
            ExFreePool(srb->DataBuffer);
        }
        if (srb->SenseInfoBuffer) {
            ExFreePool(srb->SenseInfoBuffer);
        }
        ExFreePool(srb);
    }

    if (realIrp->PendingReturned) {
        IoMarkIrpPending(realIrp);
    }

    if (Irp->MdlAddress) {
        IoFreeMdl(Irp->MdlAddress);
    }

    IoFreeIrp(Irp);

    //
    // Set status in completing IRP.
    //

    realIrp->IoStatus.Status = status;

    //
    // Set the hard error if necessary.
    //

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        //
        // Store DeviceObject for filesystem, and clear
        // in IoStatus.Information field.
        //

        DebugPrint((1, "CdRomDeviceCompletion - Setting Hard Error on realIrp %p\n",
                    realIrp));
        if (realIrp->Tail.Overlay.Thread) {
            IoSetHardErrorOrVerifyDevice(realIrp, DeviceObject);
        }

        realIrp->IoStatus.Information = 0;
    }

    CdRomCompleteIrpAndStartNextPacketSafely(DeviceObject, realIrp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
CdRomSetVolumeIntermediateCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION  irpStack = IoGetCurrentIrpStackLocation(Irp);
    PCDROM_DATA         cdData = (PCDROM_DATA)(commonExtension->DriverData);
    BOOLEAN             use6Byte = TEST_FLAG(cdData->XAFlags, XA_USE_6_BYTE);
    PIO_STACK_LOCATION  realIrpStack;
    PIO_STACK_LOCATION  realIrpNextStack;
    PSCSI_REQUEST_BLOCK srb     = Context;
    PIRP                realIrp = NULL;
    NTSTATUS            status;
    BOOLEAN             retry;

    //
    // This code must run at DISPATCH_LEVEL to call IoStartNextPacket()
    // The storage stack guarantees this, but this explicitly checks
    // that assumption.
    //

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    //
    // Extract the 'real' irp from the irpstack.
    //

    realIrp = (PIRP) irpStack->Parameters.Others.Argument2;
    realIrpStack = IoGetCurrentIrpStackLocation(realIrp);
    realIrpNextStack = IoGetNextIrpStackLocation(realIrp);

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        ULONG retryInterval;

        DebugPrint((2,
                    "CdRomSetVolumeIntermediateCompletion: Irp %p, Srb %p, Real Irp %p\n",
                    Irp,
                    srb,
                    realIrp));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ClassReleaseQueue(DeviceObject);
        }


        retry = ClassInterpretSenseInfo(DeviceObject,
                                            srb,
                                            irpStack->MajorFunction,
                                            irpStack->Parameters.DeviceIoControl.IoControlCode,
                                            MAXIMUM_RETRIES - ((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1),
                                            &status,
                                            &retryInterval);

        if (status == STATUS_DATA_OVERRUN) {
            status = STATUS_SUCCESS;
            retry = FALSE;
        }

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (realIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            status = STATUS_IO_DEVICE_ERROR;
            retry = TRUE;
        }

        if (retry && ((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1)--) {

            if (((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1)) {

                //
                // Retry request.
                //

                DebugPrint((1, "Retry request %p - Calling StartIo\n", Irp));


                ExFreePool(srb->SenseInfoBuffer);
                ExFreePool(srb->DataBuffer);
                ExFreePool(srb);
                if (Irp->MdlAddress) {
                    IoFreeMdl(Irp->MdlAddress);
                }

                IoFreeIrp(Irp);

                CdRomRetryRequest(deviceExtension,
                                  realIrp,
                                  retryInterval,
                                  FALSE);

                return STATUS_MORE_PROCESSING_REQUIRED;

            }

            //
            // Exhausted retries. Fall through and complete the request with the appropriate status.
            //

        }
    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {

        PAUDIO_OUTPUT   audioInput = NULL;
        PAUDIO_OUTPUT   audioOutput;
        PVOLUME_CONTROL volumeControl = realIrp->AssociatedIrp.SystemBuffer;
        ULONG           i,bytesTransferred,headerLength;
        PVOID           dataBuffer;
        PCDB            cdb;

        audioInput = ClassFindModePage((PCHAR)srb->DataBuffer,
                                             srb->DataTransferLength,
                                             CDROM_AUDIO_CONTROL_PAGE,
                                             use6Byte);

        //
        // Check to make sure the mode sense data is valid before we go on
        //

        if(audioInput == NULL) {

            DebugPrint((1, "Mode Sense Page %d not found\n",
                        CDROM_AUDIO_CONTROL_PAGE));

            realIrp->IoStatus.Information = 0;
            realIrp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
            ClassReleaseRemoveLock(DeviceObject, realIrp);
            ClassCompleteRequest(DeviceObject, realIrp, IO_DISK_INCREMENT);
            ExFreePool(srb->SenseInfoBuffer);
            ExFreePool(srb);
            IoFreeMdl(Irp->MdlAddress);
            IoFreeIrp(Irp);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        if (use6Byte) {
            headerLength = sizeof(MODE_PARAMETER_HEADER);
        } else {
            headerLength = sizeof(MODE_PARAMETER_HEADER10);
        }

        bytesTransferred = sizeof(AUDIO_OUTPUT) + headerLength;

        //
        // Allocate a new buffer for the mode select.
        //

        dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                    bytesTransferred,
                                    CDROM_TAG_VOLUME_INT);

        if (!dataBuffer) {
            realIrp->IoStatus.Information = 0;
            realIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            ClassReleaseRemoveLock(DeviceObject, realIrp);
            ClassCompleteRequest(DeviceObject, realIrp, IO_DISK_INCREMENT);
            ExFreePool(srb->SenseInfoBuffer);
            ExFreePool(srb);
            IoFreeMdl(Irp->MdlAddress);
            IoFreeIrp(Irp);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        RtlZeroMemory(dataBuffer, bytesTransferred);

        //
        // Rebuild the data buffer to include the user requested values.
        //

        audioOutput = (PAUDIO_OUTPUT) ((PCHAR) dataBuffer + headerLength);

        for (i=0; i<4; i++) {
            audioOutput->PortOutput[i].Volume =
                volumeControl->PortVolume[i];
            audioOutput->PortOutput[i].ChannelSelection =
                audioInput->PortOutput[i].ChannelSelection;
        }

        audioOutput->CodePage = CDROM_AUDIO_CONTROL_PAGE;
        audioOutput->ParameterLength = sizeof(AUDIO_OUTPUT) - 2;
        audioOutput->Immediate = MODE_SELECT_IMMEDIATE;

        //
        // Free the old data buffer, mdl.
        //

        ExFreePool(srb->DataBuffer);
        IoFreeMdl(Irp->MdlAddress);

        //
        // rebuild the srb.
        //

        cdb = (PCDB)srb->Cdb;
        RtlZeroMemory(cdb, CDB12GENERIC_LENGTH);

        srb->SrbStatus = srb->ScsiStatus = 0;
        srb->SrbFlags = deviceExtension->SrbFlags;
        srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_OUT);
        srb->DataTransferLength = bytesTransferred;

        if (use6Byte) {

            cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
            cdb->MODE_SELECT.ParameterListLength = (UCHAR) bytesTransferred;
            cdb->MODE_SELECT.PFBit = 1;
            srb->CdbLength = 6;
        } else {

            cdb->MODE_SELECT10.OperationCode = SCSIOP_MODE_SELECT10;
            cdb->MODE_SELECT10.ParameterListLength[0] = (UCHAR) (bytesTransferred >> 8);
            cdb->MODE_SELECT10.ParameterListLength[1] = (UCHAR) (bytesTransferred & 0xFF);
            cdb->MODE_SELECT10.PFBit = 1;
            srb->CdbLength = 10;
        }

        //
        // Prepare the MDL
        //

        Irp->MdlAddress = IoAllocateMdl(dataBuffer,
                                        bytesTransferred,
                                        FALSE,
                                        FALSE,
                                        (PIRP) NULL);

        if (!Irp->MdlAddress) {
            realIrp->IoStatus.Information = 0;
            realIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            ClassReleaseRemoveLock(DeviceObject, realIrp);
            ClassCompleteRequest(DeviceObject, realIrp, IO_DISK_INCREMENT);
            ExFreePool(srb->SenseInfoBuffer);
            ExFreePool(srb);
            ExFreePool(dataBuffer);
            IoFreeIrp(Irp);
            return STATUS_MORE_PROCESSING_REQUIRED;

        }

        MmBuildMdlForNonPagedPool(Irp->MdlAddress);
        srb->DataBuffer = dataBuffer;

        irpStack = IoGetNextIrpStackLocation(Irp);
        irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
        irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_IN;
        irpStack->Parameters.Scsi.Srb = srb;

        //
        // reset the irp completion.
        //

        IoSetCompletionRoutine(Irp,
                               CdRomDeviceControlCompletion,
                               srb,
                               TRUE,
                               TRUE,
                               TRUE);
        //
        // Call the port driver.
        //

        IoCallDriver(commonExtension->LowerDeviceObject, Irp);

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Deallocate srb and sense buffer.
    //

    if (srb) {
        if (srb->DataBuffer) {
            ExFreePool(srb->DataBuffer);
        }
        if (srb->SenseInfoBuffer) {
            ExFreePool(srb->SenseInfoBuffer);
        }
        ExFreePool(srb);
    }

    if (Irp->PendingReturned) {
      IoMarkIrpPending(Irp);
    }

    if (realIrp->PendingReturned) {
        IoMarkIrpPending(realIrp);
    }

    if (Irp->MdlAddress) {
        IoFreeMdl(Irp->MdlAddress);
    }

    IoFreeIrp(Irp);

    //
    // Set status in completing IRP.
    //

    realIrp->IoStatus.Status = status;

    //
    // Set the hard error if necessary.
    //

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        //
        // Store DeviceObject for filesystem, and clear
        // in IoStatus.Information field.
        //

        if (realIrp->Tail.Overlay.Thread) {
            IoSetHardErrorOrVerifyDevice(realIrp, DeviceObject);
        }
        realIrp->IoStatus.Information = 0;
    }

    CdRomCompleteIrpAndStartNextPacketSafely(DeviceObject, realIrp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}



NTSTATUS
CdRomSwitchModeCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION  irpStack = IoGetCurrentIrpStackLocation(Irp);
    PCDROM_DATA         cdData = (PCDROM_DATA)(commonExtension->DriverData);
    BOOLEAN             use6Byte = TEST_FLAG(cdData->XAFlags, XA_USE_6_BYTE);
    PIO_STACK_LOCATION  realIrpStack;
    PIO_STACK_LOCATION  realIrpNextStack;
    PSCSI_REQUEST_BLOCK srb     = Context;
    PIRP                realIrp = NULL;
    NTSTATUS            status;
    BOOLEAN             retry;

    //
    // This code must run at DISPATCH_LEVEL to call IoStartNextPacket()
    // The storage stack guarantees this, but this explicitly checks
    // that assumption.
    //

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    //
    // Extract the 'real' irp from the irpstack.
    //

    realIrp = (PIRP) irpStack->Parameters.Others.Argument2;
    realIrpStack = IoGetCurrentIrpStackLocation(realIrp);
    realIrpNextStack = IoGetNextIrpStackLocation(realIrp);

    //
    // Check SRB status for success of completing request.
    //

    if (SRB_STATUS(srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        ULONG retryInterval;

        DebugPrint((2,
                    "CdRomSetVolumeIntermediateCompletion: Irp %p, Srb %p, Real Irp %p\n",
                    Irp,
                    srb,
                    realIrp));

        //
        // Release the queue if it is frozen.
        //

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ClassReleaseQueue(DeviceObject);
        }


        retry = ClassInterpretSenseInfo(DeviceObject,
                                        srb,
                                        irpStack->MajorFunction,
                                        irpStack->Parameters.DeviceIoControl.IoControlCode,
                                        MAXIMUM_RETRIES - ((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1),
                                        &status,
                                        &retryInterval);

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (realIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME &&
            status == STATUS_VERIFY_REQUIRED) {

            status = STATUS_IO_DEVICE_ERROR;
            retry = TRUE;
        }

        if (retry && ((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1)--) {

            if (((ULONG)(ULONG_PTR)realIrpNextStack->Parameters.Others.Argument1)) {

                //
                // Retry request.
                //

                DebugPrint((1, "Retry request %p - Calling StartIo\n", Irp));


                ExFreePool(srb->SenseInfoBuffer);
                ExFreePool(srb->DataBuffer);
                ExFreePool(srb);
                if (Irp->MdlAddress) {
                    IoFreeMdl(Irp->MdlAddress);
                }

                IoFreeIrp(Irp);

                //
                // Call StartIo directly since IoStartNextPacket hasn't been called,
                // the serialisation is still intact.
                //

                CdRomRetryRequest(fdoExtension,
                                  realIrp,
                                  retryInterval,
                                  FALSE);

                return STATUS_MORE_PROCESSING_REQUIRED;

            }

            //
            // Exhausted retries. Fall through and complete the request with the appropriate status.
            //
        }
    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {

        ULONG sectorSize, startingSector, transferByteCount;
        PCDB cdb;

        //
        // Update device ext. to show which mode we are currently using.
        //

        sectorSize =  cdData->BlockDescriptor.BlockLength[0] << 16;
        sectorSize |= (cdData->BlockDescriptor.BlockLength[1] << 8);
        sectorSize |= (cdData->BlockDescriptor.BlockLength[2]);

        cdData->RawAccess = (sectorSize == RAW_SECTOR_SIZE) ? TRUE : FALSE;

        //
        // Free the old data buffer, mdl.
        // reuse the SenseInfoBuffer and Srb
        //

        ExFreePool(srb->DataBuffer);
        IoFreeMdl(Irp->MdlAddress);
        IoFreeIrp(Irp);

        //
        // rebuild the srb.
        //

        cdb = (PCDB)srb->Cdb;
        RtlZeroMemory(cdb, CDB12GENERIC_LENGTH);


        if (cdData->RawAccess) {

            PRAW_READ_INFO rawReadInfo =
                               (PRAW_READ_INFO)realIrpStack->Parameters.DeviceIoControl.Type3InputBuffer;

            ULONG maximumTransferLength;
            ULONG transferPages;
            UCHAR min, sec, frame;

            //
            // Calculate starting offset.
            //

            startingSector = (ULONG)(rawReadInfo->DiskOffset.QuadPart >> fdoExtension->SectorShift);
            transferByteCount  = rawReadInfo->SectorCount * RAW_SECTOR_SIZE;
            maximumTransferLength = fdoExtension->AdapterDescriptor->MaximumTransferLength;
            transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(realIrp->MdlAddress),
                                                           transferByteCount);

            //
            // Determine if request is within limits imposed by miniport.
            // If the request is larger than the miniport's capabilities, split it.
            //

            if (transferByteCount > maximumTransferLength ||
                transferPages > fdoExtension->AdapterDescriptor->MaximumPhysicalPages) {


                ExFreePool(srb->SenseInfoBuffer);
                ExFreePool(srb);
                realIrp->IoStatus.Information = 0;
                realIrp->IoStatus.Status = STATUS_INVALID_PARAMETER;

                BAIL_OUT(realIrp);
                CdRomCompleteIrpAndStartNextPacketSafely(DeviceObject, realIrp);
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

            srb->OriginalRequest = realIrp;
            srb->SrbFlags = fdoExtension->SrbFlags;
            srb->SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER | SRB_FLAGS_DATA_IN);
            srb->DataTransferLength = transferByteCount;
            srb->TimeOutValue = fdoExtension->TimeOutValue;
            srb->CdbLength = 10;
            srb->DataBuffer = MmGetMdlVirtualAddress(realIrp->MdlAddress);

            if (rawReadInfo->TrackMode == CDDA) {
                if (TEST_FLAG(cdData->XAFlags, XA_PLEXTOR_CDDA)) {

                    srb->CdbLength = 12;

                    cdb->PLXTR_READ_CDDA.LogicalBlockByte3  = (UCHAR) (startingSector & 0xFF);
                    cdb->PLXTR_READ_CDDA.LogicalBlockByte2  = (UCHAR) ((startingSector >>  8) & 0xFF);
                    cdb->PLXTR_READ_CDDA.LogicalBlockByte1  = (UCHAR) ((startingSector >> 16) & 0xFF);
                    cdb->PLXTR_READ_CDDA.LogicalBlockByte0  = (UCHAR) ((startingSector >> 24) & 0xFF);

                    cdb->PLXTR_READ_CDDA.TransferBlockByte3 = (UCHAR) (rawReadInfo->SectorCount & 0xFF);
                    cdb->PLXTR_READ_CDDA.TransferBlockByte2 = (UCHAR) (rawReadInfo->SectorCount >> 8);
                    cdb->PLXTR_READ_CDDA.TransferBlockByte1 = 0;
                    cdb->PLXTR_READ_CDDA.TransferBlockByte0 = 0;

                    cdb->PLXTR_READ_CDDA.SubCode = 0;
                    cdb->PLXTR_READ_CDDA.OperationCode = 0xD8;

                } else if (TEST_FLAG(cdData->XAFlags, XA_NEC_CDDA)) {

                    cdb->NEC_READ_CDDA.LogicalBlockByte3  = (UCHAR) (startingSector & 0xFF);
                    cdb->NEC_READ_CDDA.LogicalBlockByte2  = (UCHAR) ((startingSector >>  8) & 0xFF);
                    cdb->NEC_READ_CDDA.LogicalBlockByte1  = (UCHAR) ((startingSector >> 16) & 0xFF);
                    cdb->NEC_READ_CDDA.LogicalBlockByte0  = (UCHAR) ((startingSector >> 24) & 0xFF);

                    cdb->NEC_READ_CDDA.TransferBlockByte1 = (UCHAR) (rawReadInfo->SectorCount & 0xFF);
                    cdb->NEC_READ_CDDA.TransferBlockByte0 = (UCHAR) (rawReadInfo->SectorCount >> 8);

                    cdb->NEC_READ_CDDA.OperationCode = 0xD4;
                }
            } else {
                cdb->CDB10.TransferBlocksMsb  = (UCHAR) (rawReadInfo->SectorCount >> 8);
                cdb->CDB10.TransferBlocksLsb  = (UCHAR) (rawReadInfo->SectorCount & 0xFF);

                cdb->CDB10.LogicalBlockByte3  = (UCHAR) (startingSector & 0xFF);
                cdb->CDB10.LogicalBlockByte2  = (UCHAR) ((startingSector >>  8) & 0xFF);
                cdb->CDB10.LogicalBlockByte1  = (UCHAR) ((startingSector >> 16) & 0xFF);
                cdb->CDB10.LogicalBlockByte0  = (UCHAR) ((startingSector >> 24) & 0xFF);

                cdb->CDB10.OperationCode = SCSIOP_READ;
            }

            srb->SrbStatus = srb->ScsiStatus = 0;


            irpStack = IoGetNextIrpStackLocation(realIrp);
            irpStack->MajorFunction = IRP_MJ_SCSI;
            irpStack->Parameters.Scsi.Srb = srb;

            if (!(irpStack->Parameters.Others.Argument1)) {

                //
                // Only jam this in if it doesn't exist. The completion routines can
                // call StartIo directly in the case of retries and resetting it will
                // cause infinite loops.
                //

                irpStack->Parameters.Others.Argument1 = (PVOID) MAXIMUM_RETRIES;
            }

            //
            // Set up IoCompletion routine address.
            //

            IoSetCompletionRoutine(realIrp,
                                   CdRomXACompletion,
                                   srb,
                                   TRUE,
                                   TRUE,
                                   TRUE);
        } else {

            ULONG maximumTransferLength = fdoExtension->AdapterDescriptor->MaximumTransferLength;
            ULONG transferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(realIrp->MdlAddress),
                                                                 realIrpStack->Parameters.Read.Length);

            //
            // free the SRB and SenseInfoBuffer since they aren't used
            // by either ClassBuildRequest() nor ClassSplitRequest().
            //

            ExFreePool(srb->SenseInfoBuffer);
            ExFreePool(srb);

            //
            // Back to cooked sectors. Build and send a normal read.
            // The real work for setting offsets and checking for splitrequests was
            // done in startio
            //

            if ((realIrpStack->Parameters.Read.Length > maximumTransferLength) ||
                (transferPages > fdoExtension->AdapterDescriptor->MaximumPhysicalPages)) {

                //
                // Request needs to be split. Completion of each portion of the request will
                // fire off the next portion. The final request will signal Io to send a new request.
                //

                ClassSplitRequest(DeviceObject, realIrp, maximumTransferLength);
                return STATUS_MORE_PROCESSING_REQUIRED;

            } else {

                //
                // Build SRB and CDB for this IRP.
                //

                ClassBuildRequest(DeviceObject, realIrp);

            }
        }

        //
        // Call the port driver.
        //

        IoCallDriver(commonExtension->LowerDeviceObject, realIrp);

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Update device Extension flags to indicate that XA isn't supported.
    //

    DebugPrint((1, "Device Cannot Support CDDA (but tested positive) "
                "Now Clearing CDDA flags for FDO %p\n", DeviceObject));
    SET_FLAG(cdData->XAFlags, XA_NOT_SUPPORTED);
    CLEAR_FLAG(cdData->XAFlags, XA_PLEXTOR_CDDA);
    CLEAR_FLAG(cdData->XAFlags, XA_NEC_CDDA);

    //
    // Deallocate srb and sense buffer.
    //

    if (srb) {
        if (srb->DataBuffer) {
            ExFreePool(srb->DataBuffer);
        }
        if (srb->SenseInfoBuffer) {
            ExFreePool(srb->SenseInfoBuffer);
        }
        ExFreePool(srb);
    }

    if (Irp->PendingReturned) {
      IoMarkIrpPending(Irp);
    }

    if (realIrp->PendingReturned) {
        IoMarkIrpPending(realIrp);
    }

    if (Irp->MdlAddress) {
        IoFreeMdl(Irp->MdlAddress);
    }

    IoFreeIrp(Irp);

    //
    // Set status in completing IRP.
    //

    realIrp->IoStatus.Status = status;

    //
    // Set the hard error if necessary.
    //

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        //
        // Store DeviceObject for filesystem, and clear
        // in IoStatus.Information field.
        //

        if (realIrp->Tail.Overlay.Thread) {
            IoSetHardErrorOrVerifyDevice(realIrp, DeviceObject);
        }
        realIrp->IoStatus.Information = 0;
    }

    CdRomCompleteIrpAndStartNextPacketSafely(DeviceObject, realIrp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
CdRomXACompletion(
    IN PDEVICE_OBJECT DeviceObject,
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

Arguments:

    DeviceObject - Supplies the device object which represents the logical
        unit.

    Irp - Supplies the Irp which has completed.

    Context - Supplies a pointer to the SRB.

Return Value:

    NT status

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = Context;
    PFUNCTIONAL_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS status;
    BOOLEAN retry;

    //
    // This code must run at DISPATCH_LEVEL to call IoStartNextPacket()
    // The storage stack guarantees this, but this explicitly checks
    // that assumption.
    //

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

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
            ClassReleaseQueue(DeviceObject);
        }

        retry = ClassInterpretSenseInfo(
            DeviceObject,
            srb,
            irpStack->MajorFunction,
            irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ? irpStack->Parameters.DeviceIoControl.IoControlCode : 0,
            MAXIMUM_RETRIES - irpStack->MinorFunction, // HACKHACK - REF #0001
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

        if (retry) {

            if (irpStack->MinorFunction != 0) { // HACKHACK - REF #0001

                irpStack->MinorFunction--;      // HACKHACK - REF #0001

                //
                // Retry request.
                //

                DebugPrint((1, "CdRomXACompletion: Retry request %p (%x) - "
                            "Calling StartIo\n", Irp, irpStack->MinorFunction));


                ExFreePool(srb->SenseInfoBuffer);
                ExFreePool(srb);

                //
                // Call StartIo directly since IoStartNextPacket hasn't been called,
                // the serialisation is still intact.
                //

                CdRomRetryRequest(deviceExtension,
                                  Irp,
                                  retryInterval,
                                  FALSE);

                return STATUS_MORE_PROCESSING_REQUIRED;

            }

            //
            // Exhausted retries, fall through and complete the request
            // with the appropriate status
            //

            DebugPrint((1, "CdRomXACompletion: Retries exhausted for irp %p\n",
                        Irp));

        }

    } else {

        //
        // Set status for successful request.
        //

        status = STATUS_SUCCESS;

    } // end if (SRB_STATUS(srb->SrbStatus) ...

    //
    // Return SRB to nonpaged pool.
    //

    ExFreePool(srb->SenseInfoBuffer);
    ExFreePool(srb);

    //
    // Set status in completing IRP.
    //

    Irp->IoStatus.Status = status;

    //
    // Set the hard error if necessary.
    //

    if (!NT_SUCCESS(status) &&
        IoIsErrorUserInduced(status) &&
        Irp->Tail.Overlay.Thread != NULL ) {

        //
        // Store DeviceObject for filesystem, and clear
        // in IoStatus.Information field.
        //

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
        Irp->IoStatus.Information = 0;
    }

    //
    // If pending has be returned for this irp then mark the current stack as
    // pending.
    //

    if (Irp->PendingReturned) {
      IoMarkIrpPending(Irp);
    }

    IoStartNextPacket(DeviceObject, FALSE);
    ClassReleaseRemoveLock(DeviceObject, Irp);

    return status;
}


NTSTATUS
CdRomDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the NT device control handler for CDROMs.

Arguments:

    DeviceObject - for this CDROM

    Irp - IO Request packet

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION  fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack;
    PCDROM_DATA        cdData = (PCDROM_DATA)(commonExtension->DriverData);

    BOOLEAN            use6Byte = TEST_FLAG(cdData->XAFlags, XA_USE_6_BYTE);
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb = (PCDB)srb.Cdb;
    PVOID outputBuffer;
    ULONG bytesTransferred = 0;
    NTSTATUS status;
    NTSTATUS status2;
    KIRQL    irql;

    ULONG ioctlCode;
    ULONG baseCode;
    ULONG functionCode;

RetryControl:

    //
    // Zero the SRB on stack.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    Irp->IoStatus.Information = 0;

    //
    // if this is a class driver ioctl then we need to change the base code
    // to IOCTL_CDROM_BASE so that the switch statement can handle it.
    //
    // WARNING - currently the scsi class ioctl function codes are between
    // 0x200 & 0x300.  this routine depends on that fact
    //

    ioctlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    baseCode = ioctlCode >> 16;
    functionCode = (ioctlCode & (~0xffffc003)) >> 2;

    DebugPrint((2, "CdRomDeviceControl: Ioctl Code = %lx, Base Code = %lx,"
                   " Function Code = %lx\n",
                ioctlCode,
                baseCode,
                functionCode
              ));

    if((functionCode >= 0x200) && (functionCode <= 0x300)) {

        ioctlCode = (ioctlCode & 0x0000ffff) | CTL_CODE(IOCTL_CDROM_BASE, 0, 0, 0);

        DebugPrint((2, "CdRomDeviceControl: Class Code - new ioctl code is %lx\n",
                    ioctlCode));

        irpStack->Parameters.DeviceIoControl.IoControlCode = ioctlCode;

    }

    switch (ioctlCode) {

    case IOCTL_STORAGE_GET_MEDIA_TYPES_EX: {

        PGET_MEDIA_TYPES  mediaTypes = Irp->AssociatedIrp.SystemBuffer;
        PDEVICE_MEDIA_INFO mediaInfo = &mediaTypes->MediaInfo[0];

        //
        // Ensure that buffer is large enough.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(GET_MEDIA_TYPES)) {

            //
            // Buffer too small.
            //

            Irp->IoStatus.Information = 0;
            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        mediaTypes->DeviceType = CdRomGetDeviceType(DeviceObject);
        mediaTypes->MediaInfoCount = 1;


        mediaInfo->DeviceSpecific.RemovableDiskInfo.Cylinders.QuadPart = fdoExtension->DiskGeometry.Cylinders.QuadPart;
        mediaInfo->DeviceSpecific.RemovableDiskInfo.TracksPerCylinder = fdoExtension->DiskGeometry.TracksPerCylinder;
        mediaInfo->DeviceSpecific.RemovableDiskInfo.SectorsPerTrack = fdoExtension->DiskGeometry.SectorsPerTrack;
        mediaInfo->DeviceSpecific.RemovableDiskInfo.BytesPerSector = fdoExtension->DiskGeometry.BytesPerSector;

        //
        // Set the type.
        //

        mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaType = CD_ROM;
        mediaInfo->DeviceSpecific.RemovableDiskInfo.NumberMediaSides = 1;
        mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics = MEDIA_READ_ONLY;

        //
        // Status will either be success, if media is present, or no media.
        // It would be optimal to base from density code and medium type, but not all devices
        // have values for these fields.
        //

        //
        // Send a TUR to determine if media is present.
        //

        srb.CdbLength = 6;
        cdb = (PCDB)srb.Cdb;
        cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = fdoExtension->TimeOutValue;

        status = ClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         NULL,
                                         0,
                                         FALSE);


        DebugPrint((1,
                   "CdRomDeviceControl: GET_MEDIA_TYPES status of TUR - %lx\n",
                   status));

        if (NT_SUCCESS(status)) {
            mediaInfo->DeviceSpecific.RemovableDiskInfo.MediaCharacteristics |= MEDIA_CURRENTLY_MOUNTED;
        }

        Irp->IoStatus.Information = sizeof(GET_MEDIA_TYPES);
        status = STATUS_SUCCESS;
        break;
    }


    case IOCTL_CDROM_RAW_READ: {

        LARGE_INTEGER  startingOffset;
        ULONGLONG      transferBytes;
        ULONG          startingSector;
        PRAW_READ_INFO rawReadInfo = (PRAW_READ_INFO)irpStack->Parameters.DeviceIoControl.Type3InputBuffer;
        PUCHAR         userData = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;

        //
        // Ensure that XA reads are supported.
        //

        if (TEST_FLAG(cdData->XAFlags, XA_NOT_SUPPORTED)) {
            DebugPrint((1,
                        "CdRomDeviceControl: XA Reads not supported. Flags (%x)\n",
                        cdData->XAFlags));
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        //
        // Check that ending sector is on disc and buffers are there and of
        // correct size.
        //

        if (rawReadInfo == NULL) {

            //
            //  Called from user space. Validate the buffers.
            //

            rawReadInfo = (PRAW_READ_INFO)userData;
            irpStack->Parameters.DeviceIoControl.Type3InputBuffer =
                (PVOID)userData;

            if (rawReadInfo == NULL) {

                DebugPrint((1, "CdRomDeviceControl: Invalid I/O parameters for "
                            "XA Read (No extent info\n"));

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

                ClassReleaseRemoveLock(DeviceObject, Irp);
                ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
                return STATUS_INVALID_PARAMETER;
            }

            if (irpStack->Parameters.DeviceIoControl.InputBufferLength !=
                sizeof(RAW_READ_INFO)) {

                DebugPrint((1,"CdRomDeviceControl: Invalid I/O parameters for "
                            "XA Read (Invalid info buffer\n"));

                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

                ClassReleaseRemoveLock(DeviceObject, Irp);
                ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
                return STATUS_INVALID_PARAMETER;
            }
        }

        startingOffset.QuadPart = rawReadInfo->DiskOffset.QuadPart;
        startingSector = (ULONG)(rawReadInfo->DiskOffset.QuadPart >>
                                 fdoExtension->SectorShift);
        transferBytes = rawReadInfo->SectorCount * RAW_SECTOR_SIZE;

        if ((transferBytes < rawReadInfo->SectorCount ||
             transferBytes < RAW_SECTOR_SIZE) ||            // check for overflow
             (irpStack->Parameters.DeviceIoControl.OutputBufferLength < transferBytes)) {

            DebugPrint((1,"CdRomDeviceControl: Invalid I/O parameters for XA "
                        "Read (Bad buffer size)\n"));

            //
            // Fail request with status of invalid parameters.
            //

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            return STATUS_INVALID_PARAMETER;
        }

        if ((((ULONGLONG) startingOffset.QuadPart) + transferBytes) > (ULONGLONG) commonExtension->PartitionLength.QuadPart) {

            DebugPrint((1,"CdRomDeviceControl: Invalid I/O parameters for XA "
                        "Read (Request Out of Bounds)\n"));

            //
            // Fail request with status of invalid parameters.
            //

            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // HACKHACK - REF #0001
        // The retry count will be in this irp's IRP_MN function,
        // as the new irp was freed, and we therefore cannot use
        // this irp's next stack location for this function.
        // This may be a good location to store this info for
        // when we remove RAW_READ (mode switching), as we will
        // no longer have the nextIrpStackLocation to play with
        // when that occurs
        //
        // once XA_READ is removed, then this hack can also be
        // removed.
        //
        irpStack->MinorFunction = MAXIMUM_RETRIES; // HACKHACK - REF #0001

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_GET_DRIVE_GEOMETRY: {

        DebugPrint((2,"CdRomDeviceControl: Get drive geometry\n"));

        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof( DISK_GEOMETRY ) ) {

            status = STATUS_INFO_LENGTH_MISMATCH;
            break;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_GET_LAST_SESSION:
    case IOCTL_CDROM_READ_TOC:  {

        //
        // If the cd is playing music then reject this request.
        //

        if (CdRomIsPlayActive(DeviceObject)) {
            Irp->IoStatus.Status = STATUS_DEVICE_BUSY;
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            return STATUS_DEVICE_BUSY;
        }

        //
        // Make sure the caller is requesting enough data to make this worth
        // our while.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            FIELD_OFFSET(CDROM_TOC, FirstTrack)) {

            //
            // Not enough room for the length word.
            //

            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            ClassReleaseRemoveLock(DeviceObject, Irp);
            ClassCompleteRequest(DeviceObject, Irp, IO_NO_INCREMENT);
            return STATUS_BUFFER_TOO_SMALL;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_PLAY_AUDIO_MSF: {

        //
        // Play Audio MSF
        //

        DebugPrint((2,"CdRomDeviceControl: Play audio MSF\n"));

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(CDROM_PLAY_AUDIO_MSF)) {

            //
            // Indicate unsuccessful status.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_SEEK_AUDIO_MSF: {


        //
        // Seek Audio MSF
        //

        DebugPrint((2,"CdRomDeviceControl: Seek audio MSF\n"));

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(CDROM_SEEK_AUDIO_MSF)) {

            //
            // Indicate unsuccessful status.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            break;
        } else {
            IoMarkIrpPending(Irp);
            IoStartPacket(DeviceObject, Irp, NULL, NULL);

            return STATUS_PENDING;

        }
    }

    case IOCTL_CDROM_PAUSE_AUDIO: {

        //
        // Pause audio
        //

        DebugPrint((2, "CdRomDeviceControl: Pause audio\n"));

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;

        break;
    }

    case IOCTL_CDROM_RESUME_AUDIO: {

        //
        // Resume audio
        //

        DebugPrint((2, "CdRomDeviceControl: Resume audio\n"));

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_READ_Q_CHANNEL: {

        PCDROM_SUB_Q_DATA_FORMAT inputBuffer =
                         Irp->AssociatedIrp.SystemBuffer;

        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(CDROM_SUB_Q_DATA_FORMAT)) {

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            break;
        }

        //
        // check for all valid types of request
        //

        if (inputBuffer->Format != IOCTL_CDROM_CURRENT_POSITION &&
            inputBuffer->Format != IOCTL_CDROM_MEDIA_CATALOG &&
            inputBuffer->Format != IOCTL_CDROM_TRACK_ISRC ) {
            status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            break;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_GET_CONTROL: {

        DebugPrint((2, "CdRomDeviceControl: Get audio control\n"));

        //
        // Verify user buffer is large enough for the data.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(CDROM_AUDIO_CONTROL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            break;

        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_GET_VOLUME: {

        DebugPrint((2, "CdRomDeviceControl: Get volume control\n"));

        //
        // Verify user buffer is large enough for data.
        //

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(VOLUME_CONTROL)) {

            //
            // Indicate unsuccessful status and no data transferred.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            break;

        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_SET_VOLUME: {

        DebugPrint((2, "CdRomDeviceControl: Set volume control\n"));

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(VOLUME_CONTROL)) {

            //
            // Indicate unsuccessful status.
            //

            Irp->IoStatus.Information = 0;
            status = STATUS_BUFFER_TOO_SMALL;
            break;

        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_STOP_AUDIO: {

        //
        // Stop play.
        //

        DebugPrint((2, "CdRomDeviceControl: Stop audio\n"));

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_CDROM_CHECK_VERIFY: {

        DebugPrint((2, "CdRomDeviceControl: [%p] Check Verify\n", Irp));

        if((irpStack->Parameters.DeviceIoControl.OutputBufferLength) &&
           (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG))) {

           DebugPrint((1, "CdRomDeviceControl: Check Verify: media count "
                          "buffer too small\n"));

           status = STATUS_BUFFER_TOO_SMALL;
           break;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_DVD_READ_STRUCTURE: {

        DebugPrint((2, "DvdDeviceControl: [%p] IOCTL_DVD_READ_STRUCTURE\n", Irp));

        if (cdData->DvdRpc0Device && cdData->DvdRpc0LicenseFailure) {
            DebugPrint((1, "DvdDeviceControl: License Failure\n"));
            status = STATUS_LICENSE_VIOLATION;
            break;
        }

        if (cdData->DvdRpc0Device && cdData->Rpc0RetryRegistryCallback) {
            //
            // if currently in-progress, this will just return.
            // prevents looping by doing that interlockedExchange()
            //
            DebugPrint((1, "DvdDeviceControl: PickRegion() from "
                        "READ_STRUCTURE\n"));
            CdromPickDvdRegion(DeviceObject);
        }


        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
           sizeof(DVD_READ_STRUCTURE)) {

            DebugPrint((1, "DvdDeviceControl - READ_STRUCTURE: input buffer "
                           "length too small (was %d should be %d)\n",
                           irpStack->Parameters.DeviceIoControl.InputBufferLength,
                           sizeof(DVD_READ_STRUCTURE)));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength <
           sizeof(READ_DVD_STRUCTURES_HEADER)) {

            DebugPrint((1, "DvdDeviceControl - READ_STRUCTURE: output buffer "
                           "cannot hold header information\n"));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength >
           MAXUSHORT) {

            //
            // key length must fit in two bytes
            //
            DebugPrint((1, "DvdDeviceControl - READ_STRUCTURE: output buffer "
                           "too large\n"));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_DVD_START_SESSION: {

        DebugPrint((2, "DvdDeviceControl: [%p] IOCTL_DVD_START_SESSION\n", Irp));

        if (cdData->DvdRpc0Device && cdData->DvdRpc0LicenseFailure) {
            DebugPrint((1, "DvdDeviceControl: License Failure\n"));
            status = STATUS_LICENSE_VIOLATION;
            break;
        }

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength <
           sizeof(DVD_SESSION_ID)) {

            DebugPrint((1, "DvdDeviceControl: DVD_START_SESSION - output "
                           "buffer too small\n"));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_DVD_SEND_KEY:
    case IOCTL_DVD_SEND_KEY2: {

        PDVD_COPY_PROTECT_KEY key = Irp->AssociatedIrp.SystemBuffer;
        ULONG keyLength;

        DebugPrint((2, "DvdDeviceControl: [%p] IOCTL_DVD_SEND_KEY\n", Irp));

        if (cdData->DvdRpc0Device && cdData->DvdRpc0LicenseFailure) {
            DebugPrint((1, "DvdDeviceControl: License Failure\n"));
            status = STATUS_LICENSE_VIOLATION;
            break;
        }

        if((irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(DVD_COPY_PROTECT_KEY)) ||
           (irpStack->Parameters.DeviceIoControl.InputBufferLength !=
            key->KeyLength)) {

            //
            // Key is too small to have a header or the key length doesn't
            // match the input buffer length.  Key must be invalid
            //

            DebugPrint((1, "DvdDeviceControl: [%p] IOCTL_DVD_SEND_KEY - "
                           "key is too small or does not match KeyLength\n",
                           Irp));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // allow only certain key type (non-destructive) to go through
        // IOCTL_DVD_SEND_KEY (which only requires READ access to the device
        //
        if (ioctlCode == IOCTL_DVD_SEND_KEY) {

            if ((key->KeyType != DvdChallengeKey) &&
                (key->KeyType != DvdBusKey2) &&
                (key->KeyType != DvdInvalidateAGID)) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }
        }

        if (cdData->DvdRpc0Device) {

            if (key->KeyType == DvdSetRpcKey) {

                PDVD_SET_RPC_KEY rpcKey = (PDVD_SET_RPC_KEY) key->KeyData;

                if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                    DVD_SET_RPC_KEY_LENGTH) {

                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                //
                // we have a request to set region code
                // on a RPC0 device which doesn't support
                // region coding.
                //
                // we have to fake it.
                //

                KeWaitForMutexObject(
                    &cdData->Rpc0RegionMutex,
                    UserRequest,
                    KernelMode,
                    FALSE,
                    NULL
                    );

                if (cdData->DvdRpc0Device && cdData->Rpc0RetryRegistryCallback) {
                    //
                    // if currently in-progress, this will just return.
                    // prevents looping by doing that interlockedExchange()
                    //
                    DebugPrint((1, "DvdDeviceControl: PickRegion() from "
                                "SEND_KEY\n"));
                    CdromPickDvdRegion(DeviceObject);
                }

                if (cdData->Rpc0SystemRegion == rpcKey->PreferredDriveRegionCode) {

                    //
                    // nothing to change
                    //
                    DebugPrint((1, "DvdDeviceControl (%p) => not changing "
                                "regions -- requesting current region\n",
                                DeviceObject));
                    status = STATUS_SUCCESS;

                } else if (cdData->Rpc0SystemRegionResetCount == 0) {

                    //
                    // not allowed to change it again
                    //

                    DebugPrint((1, "DvdDeviceControl (%p) => no more region "
                                "changes are allowed for this device\n",
                                DeviceObject));
                    status = STATUS_INVALID_DEVICE_REQUEST;

                } else {

                    ULONG i;
                    UCHAR mask;
                    ULONG bufferLen;
                    PDVD_READ_STRUCTURE dvdReadStructure;
                    PDVD_COPYRIGHT_DESCRIPTOR dvdCopyRight;
                    IO_STATUS_BLOCK ioStatus;
                    UCHAR mediaRegionData;

                    mask = ~rpcKey->PreferredDriveRegionCode;

                    if (CountOfSetBitsUChar(mask) != 1) {

                        status = STATUS_INVALID_DEVICE_REQUEST;
                        break;
                    }

                    //
                    // this test will always be TRUE except during initial
                    // automatic selection of the first region.
                    //

                    if (cdData->Rpc0SystemRegion != 0xff) {

                        //
                        // make sure we have a media in the drive with the same
                        // region code if the drive is already has a region set
                        //

                        DebugPrint((2, "DvdDeviceControl (%p) => Checking "
                                    "media region\n",
                                    DeviceObject));

                        bufferLen = max(sizeof(DVD_DESCRIPTOR_HEADER) +
                                            sizeof(DVD_COPYRIGHT_DESCRIPTOR),
                                        sizeof(DVD_READ_STRUCTURE)
                                        );

                        dvdReadStructure = (PDVD_READ_STRUCTURE)
                            ExAllocatePoolWithTag(PagedPool,
                                                  bufferLen,
                                                  DVD_TAG_RPC2_CHECK);

                        if (dvdReadStructure == NULL) {
                            status = STATUS_INSUFFICIENT_RESOURCES;
                            KeReleaseMutex(&cdData->Rpc0RegionMutex,FALSE);
                            break;
                        }

                        dvdCopyRight = (PDVD_COPYRIGHT_DESCRIPTOR)
                            ((PDVD_DESCRIPTOR_HEADER) dvdReadStructure)->Data;

                        //
                        // check to see if we have a DVD device
                        //

                        RtlZeroMemory (dvdReadStructure, bufferLen);
                        dvdReadStructure->Format = DvdCopyrightDescriptor;

                        //
                        // Build a request for READ_KEY
                        //
                        ClassSendDeviceIoControlSynchronous(
                            IOCTL_DVD_READ_STRUCTURE,
                            DeviceObject,
                            dvdReadStructure,
                            sizeof(DVD_READ_STRUCTURE),
                            sizeof(DVD_DESCRIPTOR_HEADER) +
                                sizeof(DVD_COPYRIGHT_DESCRIPTOR),
                            FALSE,
                            &ioStatus);

                        //
                        // this is just to prevent bugs from creeping in
                        // if status is not set later in development
                        //

                        status = ioStatus.Status;

                        //
                        // handle errors
                        //

                        if (!NT_SUCCESS(status)) {
                            KeReleaseMutex(&cdData->Rpc0RegionMutex,FALSE);
                            ExFreePool(dvdReadStructure);
                            status = STATUS_INVALID_DEVICE_REQUEST;
                            break;
                        }

                        //
                        // save the mediaRegionData before freeing the
                        // allocated memory
                        //

                        //
                        // BUGBUG - this should not accept all-region discs
                        //          but should treat them non-regionalized
                        //          so that a non-regionalized disc will not
                        //          allow a region change setting to one that
                        //          the user really doesn't want.
                        //

                        mediaRegionData =
                            dvdCopyRight->RegionManagementInformation;
                        ExFreePool(dvdReadStructure);

                        DebugPrint((1, "DvdDeviceControl (%p) => new mask is %x"
                                    " MediaRegionData is %x\n", DeviceObject,
                                    rpcKey->PreferredDriveRegionCode,
                                    mediaRegionData));

                        if (((UCHAR)~(mediaRegionData | rpcKey->PreferredDriveRegionCode)) == 0) {
                            KeReleaseMutex(&cdData->Rpc0RegionMutex,FALSE);
                            status = STATUS_INVALID_DEVICE_REQUEST;
                            break;
                        }

                    }

                    //
                    // now try to set the region
                    //

                    DebugPrint((2, "DvdDeviceControl (%p) => Soft-Setting "
                                "region of RPC1 device to %x\n",
                                DeviceObject,
                                rpcKey->PreferredDriveRegionCode
                                ));

                    status = CdromSetRpc0Settings(DeviceObject,
                                                  rpcKey->PreferredDriveRegionCode);

                    if (!NT_SUCCESS(status)) {
                        DebugPrint((1, "DvdDeviceControl (%p) => Could not "
                                    "set region code (%x)\n",
                                    DeviceObject, status
                                    ));
                    } else {

                        DebugPrint((2, "DvdDeviceControl (%p) => New region set "
                                    " for RPC1 drive\n", DeviceObject));

                        //
                        // if it worked, our extension is already updated.
                        // release the mutex
                        //

                        DebugPrint ((4, "DvdDeviceControl (%p) => DVD current "
                                     "region bitmap  0x%x\n", DeviceObject,
                                     cdData->Rpc0SystemRegion));
                        DebugPrint ((4, "DvdDeviceControl (%p) => DVD region "
                                     " reset Count     0x%x\n", DeviceObject,
                                     cdData->Rpc0SystemRegionResetCount));
                    }

                }

                KeReleaseMutex(&cdData->Rpc0RegionMutex,FALSE);
                break;
            } // end of key->KeyType == DvdSetRpcKey
        } // end of Rpc0Device hacks

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);
        return STATUS_PENDING;
        break;
    }

    case IOCTL_DVD_READ_KEY: {

        PDVD_COPY_PROTECT_KEY keyParameters = Irp->AssociatedIrp.SystemBuffer;
        ULONG keyLength;

        DebugPrint((2, "DvdDeviceControl: [%p] IOCTL_DVD_READ_KEY\n", Irp));

        if (cdData->DvdRpc0Device && cdData->DvdRpc0LicenseFailure) {
            DebugPrint((1, "DvdDeviceControl: License Failure\n"));
            status = STATUS_LICENSE_VIOLATION;
            break;
        }

        if (cdData->DvdRpc0Device && cdData->Rpc0RetryRegistryCallback) {
            DebugPrint((1, "DvdDeviceControl: PickRegion() from READ_KEY\n"));
            CdromPickDvdRegion(DeviceObject);
        }


        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(DVD_COPY_PROTECT_KEY)) {

            DebugPrint((1, "DvdDeviceControl: EstablishDriveKey - challenge "
                           "key buffer too small\n"));

            status = STATUS_INVALID_PARAMETER;
            break;

        }


        switch(keyParameters->KeyType) {

            case DvdChallengeKey:
                keyLength = DVD_CHALLENGE_KEY_LENGTH;
                break;

            case DvdBusKey1:
            case DvdBusKey2:

                keyLength = DVD_BUS_KEY_LENGTH;
                break;

            case DvdTitleKey:
                keyLength = DVD_TITLE_KEY_LENGTH;
                break;

            case DvdAsf:
                keyLength = DVD_ASF_LENGTH;
                break;

            case DvdDiskKey:
                keyLength = DVD_DISK_KEY_LENGTH;
                break;

            case DvdGetRpcKey:
                keyLength = DVD_RPC_KEY_LENGTH;
                break;

            default:
                keyLength = sizeof(DVD_COPY_PROTECT_KEY);
                break;
        }

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            keyLength) {

            DebugPrint((1, "DvdDeviceControl: EstablishDriveKey - output "
                           "buffer too small\n"));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (keyParameters->KeyType == DvdGetRpcKey) {

            CdromPickDvdRegion(DeviceObject);
        }

        if ((keyParameters->KeyType == DvdGetRpcKey) &&
            (cdData->DvdRpc0Device)) {

            PDVD_RPC_KEY rpcKey;
            rpcKey = (PDVD_RPC_KEY)keyParameters->KeyData;
            RtlZeroMemory (rpcKey, sizeof (*rpcKey));

            KeWaitForMutexObject(
                &cdData->Rpc0RegionMutex,
                UserRequest,
                KernelMode,
                FALSE,
                NULL
                );

            //
            // make up the data
            //
            rpcKey->UserResetsAvailable = cdData->Rpc0SystemRegionResetCount;
            rpcKey->ManufacturerResetsAvailable = 0;
            if (cdData->Rpc0SystemRegion == 0xff) {
                rpcKey->TypeCode = 0;
            } else {
                rpcKey->TypeCode = 1;
            }
            rpcKey->RegionMask = (UCHAR) cdData->Rpc0SystemRegion;
            rpcKey->RpcScheme = 1;

            KeReleaseMutex(
                &cdData->Rpc0RegionMutex,
                FALSE
                );

            Irp->IoStatus.Information = DVD_RPC_KEY_LENGTH;
            status = STATUS_SUCCESS;
            break;

        } else if (keyParameters->KeyType == DvdDiskKey) {

            PDVD_COPY_PROTECT_KEY keyHeader;
            PDVD_READ_STRUCTURE readStructureRequest;

            //
            // Special case - build a request to get the dvd structure
            // so we can get the disk key.
            //

            //
            // save the key header so we can restore the interesting
            // parts later
            //

            keyHeader = ExAllocatePoolWithTag(NonPagedPool,
                                              sizeof(DVD_COPY_PROTECT_KEY),
                                              DVD_TAG_READ_KEY);

            if(keyHeader == NULL) {

                //
                // Can't save the context so return an error
                //

                DebugPrint((1, "DvdDeviceControl - READ_KEY: unable to "
                               "allocate context\n"));
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            RtlCopyMemory(keyHeader,
                          Irp->AssociatedIrp.SystemBuffer,
                          sizeof(DVD_COPY_PROTECT_KEY));

            IoCopyCurrentIrpStackLocationToNext(Irp);

            nextStack = IoGetNextIrpStackLocation(Irp);

            nextStack->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_DVD_READ_STRUCTURE;

            readStructureRequest = Irp->AssociatedIrp.SystemBuffer;
            readStructureRequest->Format = DvdDiskKeyDescriptor;
            readStructureRequest->BlockByteOffset.QuadPart = 0;
            readStructureRequest->LayerNumber = 0;
            readStructureRequest->SessionId = keyHeader->SessionId;

            nextStack->Parameters.DeviceIoControl.InputBufferLength =
                sizeof(DVD_READ_STRUCTURE);

            nextStack->Parameters.DeviceIoControl.OutputBufferLength =
                sizeof(READ_DVD_STRUCTURES_HEADER) + sizeof(DVD_DISK_KEY_DESCRIPTOR);

            IoSetCompletionRoutine(Irp,
                                   CdRomDvdReadDiskKeyCompletion,
                                   (PVOID) keyHeader,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            {
                UCHAR uniqueAddress;
                ClassAcquireRemoveLock(DeviceObject, (PIRP)&uniqueAddress);
                ClassReleaseRemoveLock(DeviceObject, Irp);

                IoMarkIrpPending(Irp);
                IoCallDriver(commonExtension->DeviceObject, Irp);
                status = STATUS_PENDING;

                ClassReleaseRemoveLock(DeviceObject, (PIRP)&uniqueAddress);
            }

            return STATUS_PENDING;

        } else {

            IoMarkIrpPending(Irp);
            IoStartPacket(DeviceObject, Irp, NULL, NULL);

        }
        return STATUS_PENDING;
    }

    case IOCTL_DVD_END_SESSION: {

        PDVD_SESSION_ID sessionId = Irp->AssociatedIrp.SystemBuffer;

        DebugPrint((2, "DvdDeviceControl: [%p] END_SESSION\n", Irp));

        if (cdData->DvdRpc0Device && cdData->DvdRpc0LicenseFailure) {
            DebugPrint((1, "DvdDeviceControl: License Failure\n"));
            status = STATUS_LICENSE_VIOLATION;
            break;
        }

        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(DVD_SESSION_ID)) {

            DebugPrint((1, "DvdDeviceControl: EndSession - input buffer too "
                           "small\n"));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        IoMarkIrpPending(Irp);

        if(*sessionId == DVD_END_ALL_SESSIONS) {

            status = CdRomDvdEndAllSessionsCompletion(DeviceObject, Irp, NULL);

            if(status == STATUS_SUCCESS) {

                //
                // Just complete the request - it was never issued to the
                // lower device
                //

                break;

            } else {

                return STATUS_PENDING;

            }
        }

        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    case IOCTL_DVD_GET_REGION: {

        PDVD_COPY_PROTECT_KEY copyProtectKey;
        ULONG keyLength;
        IO_STATUS_BLOCK ioStatus;
        PDVD_DESCRIPTOR_HEADER dvdHeader;
        PDVD_COPYRIGHT_DESCRIPTOR copyRightDescriptor;
        PDVD_REGION dvdRegion;
        PDVD_READ_STRUCTURE readStructure;
        PDVD_RPC_KEY rpcKey;

        DebugPrint((2, "DvdDeviceControl: [%p] IOCTL_DVD_GET_REGION\n", Irp));

        if (cdData->DvdRpc0Device && cdData->DvdRpc0LicenseFailure) {
            DebugPrint((1, "DvdDeviceControl: License Failure\n"));
            status = STATUS_LICENSE_VIOLATION;
            break;
        }

        if(irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(DVD_REGION)) {

            DebugPrint((1, "DvdDeviceControl: output buffer DVD_REGION too small\n"));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // figure out how much data buffer we need
        //

        keyLength = max(sizeof(DVD_DESCRIPTOR_HEADER) +
                            sizeof(DVD_COPYRIGHT_DESCRIPTOR),
                        sizeof(DVD_READ_STRUCTURE)
                        );
        keyLength = max(keyLength,
                        DVD_RPC_KEY_LENGTH
                        );

        //
        // round the size to nearest ULONGLONG -- why?
        //

        keyLength += sizeof(ULONGLONG) - (keyLength & (sizeof(ULONGLONG) - 1));

        readStructure = ExAllocatePoolWithTag(NonPagedPool,
                                              keyLength,
                                              DVD_TAG_READ_KEY);
        if (readStructure == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory (readStructure, keyLength);
        readStructure->Format = DvdCopyrightDescriptor;

        //
        // Build a request for READ_STRUCTURE
        //

        ClassSendDeviceIoControlSynchronous(
            IOCTL_DVD_READ_STRUCTURE,
            DeviceObject,
            readStructure,
            keyLength,
            sizeof(DVD_DESCRIPTOR_HEADER) +
                sizeof(DVD_COPYRIGHT_DESCRIPTOR),
            FALSE,
            &ioStatus);

        status = ioStatus.Status;

        if (!NT_SUCCESS(status)) {
            DebugPrint((1, "CdRomDvdGetRegion => read structure failed %x\n",
                        status));
            ExFreePool(readStructure);
            break;
        }

        //
        // we got the copyright descriptor, so now get the region if possible
        //

        dvdHeader = (PDVD_DESCRIPTOR_HEADER) readStructure;
        copyRightDescriptor = (PDVD_COPYRIGHT_DESCRIPTOR) dvdHeader->Data;

        //
        // the original irp's systembuffer has a copy of the info that
        // should be passed down in the request
        //

        dvdRegion = Irp->AssociatedIrp.SystemBuffer;

        dvdRegion->CopySystem = copyRightDescriptor->CopyrightProtectionType;
        dvdRegion->RegionData = copyRightDescriptor->RegionManagementInformation;

        //
        // now reuse the buffer to request the copy protection info
        //

        copyProtectKey = (PDVD_COPY_PROTECT_KEY) readStructure;
        RtlZeroMemory (copyProtectKey, DVD_RPC_KEY_LENGTH);
        copyProtectKey->KeyLength = DVD_RPC_KEY_LENGTH;
        copyProtectKey->KeyType = DvdGetRpcKey;

        //
        // send a request for READ_KEY
        //

        ClassSendDeviceIoControlSynchronous(
            IOCTL_DVD_READ_KEY,
            DeviceObject,
            copyProtectKey,
            DVD_RPC_KEY_LENGTH,
            DVD_RPC_KEY_LENGTH,
            FALSE,
            &ioStatus);
        status = ioStatus.Status;

        if (!NT_SUCCESS(status)) {
            DebugPrint((1, "CdRomDvdGetRegion => read key failed %x\n",
                        status));
            ExFreePool(readStructure);
            break;
        }

        //
        // the request succeeded.  if a supported scheme is returned,
        // then return the information to the caller
        //

        rpcKey = (PDVD_RPC_KEY) copyProtectKey->KeyData;

        if (rpcKey->RpcScheme == 1) {

            if (rpcKey->TypeCode) {

                dvdRegion->SystemRegion = ~rpcKey->RegionMask;
                dvdRegion->ResetCount = rpcKey->UserResetsAvailable;

            } else {

                //
                // the drive has not been set for any region
                //

                dvdRegion->SystemRegion = 0;
                dvdRegion->ResetCount = rpcKey->UserResetsAvailable;
            }
            Irp->IoStatus.Information = sizeof(DVD_REGION);

        } else {

            DebugPrint((1, "CdRomDvdGetRegion => rpcKey->RpcScheme != 1\n"));
            status = STATUS_INVALID_DEVICE_REQUEST;
        }

        ExFreePool(readStructure);
        break;
    }


    case IOCTL_STORAGE_SET_READ_AHEAD: {

        PSTORAGE_SET_READ_AHEAD readAhead = Irp->AssociatedIrp.SystemBuffer;


        if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
           sizeof(STORAGE_SET_READ_AHEAD)) {

            DebugPrint((1, "DvdDeviceControl: SetReadAhead buffer too small\n"));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // BUGBUG
        // Check the block addresses and make sure they aren't beyond the
        // end of the media.
        //

        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, NULL, NULL);

        return STATUS_PENDING;
    }

    default: {

        BOOLEAN synchronize = (KeGetCurrentIrql() == PASSIVE_LEVEL);
        PKEVENT deviceControlEvent;

        //
        // If the ioctl has come in at passive level then we will synchronize
        // with our start-io routine when sending the ioctl.  If the ioctl
        // has come in at a higher interrupt level and it was not handled
        // above then it's unlikely to be a request for the class DLL - however
        // we'll still use it's common code to forward the request through.
        //

        if(synchronize == TRUE) {

            deviceControlEvent = ExAllocatePoolWithTag(NonPagedPool,
                                                       sizeof(KEVENT),
                                                       CDROM_TAG_DC_EVENT);

            if(!deviceControlEvent) {

                //
                // must complete this irp unsuccessful here
                //
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;

            } else {

                PIO_STACK_LOCATION currentStack;

                KeInitializeEvent(deviceControlEvent, NotificationEvent, FALSE);

                currentStack = IoGetCurrentIrpStackLocation(Irp);
                nextStack = IoGetNextIrpStackLocation(Irp);

                //
                // Copy the stack down a notch
                //

                IoCopyCurrentIrpStackLocationToNext(Irp);

                IoSetCompletionRoutine(
                    Irp,
                    CdRomClassIoctlCompletion,
                    deviceControlEvent,
                    TRUE,
                    TRUE,
                    TRUE
                    );

                IoSetNextIrpStackLocation(Irp);

                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = 0;

                //
                // Override volume verifies on this stack location so that we
                // will be forced through the synchronization.  Once this
                // location goes away we get the old value back
                //

                nextStack->Flags |= SL_OVERRIDE_VERIFY_VOLUME;

                IoStartPacket(DeviceObject, Irp, NULL, NULL);

                //
                // Wait for CdRomClassIoctlCompletion to set the event. This
                // ensures serialization remains intact for these unhandled device
                // controls.
                //

                KeWaitForSingleObject(
                    deviceControlEvent,
                    Suspended,
                    KernelMode,
                    FALSE,
                    NULL);

                ExFreePool(deviceControlEvent);

                DebugPrint((2, "CdRomDeviceControl: irp %p synchronized\n", Irp));

                status = Irp->IoStatus.Status;
            }

        } else {
            status = STATUS_SUCCESS;
        }

        //
        // If an error occured then propagate that back up - we are no longer
        // guaranteed synchronization and the upper layers will have to
        // retry.
        //
        // If no error occured, call down to the class driver directly
        // then start up the next request.
        //

        if (NT_SUCCESS(status)) {

            UCHAR uniqueAddress;

            //
            // The class device control routine will release the remove
            // lock for this Irp.  We need to make sure we have one
            // available so that it's safe to call IoStartNextPacket
            //

            if(synchronize) {

                ClassAcquireRemoveLock(DeviceObject, (PIRP)&uniqueAddress);

            }

            status = ClassDeviceControl(DeviceObject, Irp);

            if(synchronize) {
                KeRaiseIrql(DISPATCH_LEVEL, &irql);
                IoStartNextPacket(DeviceObject, FALSE);
                KeLowerIrql(irql);
                ClassReleaseRemoveLock(DeviceObject, (PIRP)&uniqueAddress);
            }
            return status;

        }

        //
        // an error occurred (either STATUS_INSUFFICIENT_RESOURCES from
        // attempting to synchronize or  StartIo() error'd this one
        // out), so we need to finish the irp, which is
        // done at the end of this routine.
        //
        break;

    } // end default case

    } // end switch()

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // If the status is verified required and this request
        // should bypass verify required then retry the request.
        //

        if (irpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

            status = STATUS_IO_DEVICE_ERROR;
            goto RetryControl;

        }
    }

    if (IoIsErrorUserInduced(status)) {

        if (Irp->Tail.Overlay.Thread) {
            IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
        }

    }

    //
    // Update IRP with completion status.
    //

    Irp->IoStatus.Status = status;

    //
    // Complete the request.
    //

    ClassReleaseRemoveLock(DeviceObject, Irp);
    ClassCompleteRequest(DeviceObject, Irp, IO_DISK_INCREMENT);
    DebugPrint((2, "CdRomDeviceControl: Status is %lx\n", status));
    return status;

} // end CdRomDeviceControl()


VOID
ScanForSpecial(
    PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This function checks to see if an SCSI logical unit requires an special
    initialization or error processing.

Arguments:

    DeviceObject - Supplies the device object to be tested.

    InquiryData - Supplies the inquiry data returned by the device of interest.

    PortCapabilities - Supplies the capabilities of the device object.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PSTORAGE_ADAPTER_DESCRIPTOR adapterDescriptor = fdoExtension->AdapterDescriptor;
    PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor = fdoExtension->DeviceDescriptor;

    PUCHAR vendorId = (PUCHAR) deviceDescriptor + deviceDescriptor->VendorIdOffset;
    PUCHAR productId = (PUCHAR) deviceDescriptor + deviceDescriptor->ProductIdOffset;

    PCDROM_DATA       cdData = (PCDROM_DATA)(commonExtension->DriverData);

    PAGED_CODE();

    //
    // BUGBUG - the vendorId and/or productId offsets may be set to zero,
    //          which would access the beginning of the struct.  all the
    //          string compares would fail almost instantly, but it's still
    //          not the right behaviour.  should probably set to NULL if
    //          the offset is zero, and then check for NULL below in all
    //          cases.
    //

    //
    // All CDRom's can ignore the queue lock failure for power operations
    // and do not require handling the SpinUp case (unknown result of sending
    // a cdrom a START_UNIT command -- may eject disks?)
    //
    // We send the stop command mostly to stop outstanding asynch operations
    // (like audio playback) from running when the system is powered off.
    // Because of this and the unlikely chance that a PLAY command will be
    // sent in the window between the STOP and the time the machine powers down
    // we don't require queue locks.  This is important because without them
    // classpnp's power routines will send the START_STOP_UNIT command to the
    // device whether or not it supports locking (atapi does not support locking
    // and if we requested them we would end up not stopping audio on atapi
    // devices).
    //

    SET_FLAG(fdoExtension->ScanForSpecialFlags, CLASS_SPECIAL_DISABLE_SPIN_UP);
    SET_FLAG(fdoExtension->ScanForSpecialFlags, CLASS_SPECIAL_NO_QUEUE_LOCK);

    //
    // Look for a Hitachi CDR-1750. Read-ahead must be disabled in order
    // to get this cdrom drive to work on scsi adapters that use PIO.
    //

    if (( RtlCompareMemory(vendorId, "HITACHI ", 8) == 8 )&&
        ( ( RtlCompareMemory(productId, "CDR-1750S",       9) ==  9)||
          ( RtlCompareMemory(productId, "CDR-3650/1650S", 14) == 14)
         ) && ( adapterDescriptor->AdapterUsesPio )
        ) {

        DebugPrint((1, "CdRom ScanForSpecial:  Found Hitachi CDR-1750S.\n"));

        //
        // Setup an error handler to reinitialize the cd rom after it is reset.
        //

        commonExtension->DevInfo->ClassError = HitachiProcessError;

        //
        // Lock down the hitachi error processing code.
        //

        MmLockPagableCodeSection(HitachiProcessError);

    } else if ( (RtlCompareMemory(vendorId,  "TOSHIBA ", 8) == 8) &&
                (RtlCompareMemory(productId, "SD-W1101 DVD-RAM", 16) == 16)
                ) {

        DebugPrint((0, "CdRom ScanForSpecial: Found Toshiba SD-W1101 DVD-RAM "
                    "-- This drive will *NOT* support DVD-ROM playback.\n"));

    } else if ( (RtlCompareMemory(productId, "HITACHI GD-2000", 15) == 15 ) ||
                (RtlCompareMemory(productId, "HITACHI DVD-ROM GD-2000", 23) == 23)
                ) {

        //
        // NOTE: this drive has no VendorId, it's all in the ProductId
        //

        DebugPrint((1, "CdRom ScanForSpecial: Found Hitachi GD-2000\n"));

        //
        // Setup an error handler to spin up the drive when it idles out
        // since it seems to like to fail to spin itself back up on its
        // own for a REPORT_KEY command.  It may also lose the AGIDs that
        // it has given, which will result in DVD playback failures.
        // This routine will just do what it can...
        //

        commonExtension->DevInfo->ClassError = HitachiProcessErrorGD2000;

        //
        // this drive may require START_UNIT commands to spin
        // the drive up when it's spun itself down.
        //

        SET_FLAG(fdoExtension->DeviceFlags, DEV_SAFE_START_UNIT);

        //
        // Lock down the hitachi error processing code.
        //

        MmLockPagableCodeSection(HitachiProcessErrorGD2000);

    } else if (( RtlCompareMemory( vendorId, "FUJITSU ", 8) == 8 ) &&
              (( RtlCompareMemory( productId,"FMCD-101", 8) == 8 ) ||
               ( RtlCompareMemory( productId,"FMCD-102", 8) == 8 ))) {

        //
        // When Read command is issued to FMCD-101 or FMCD-102 and there is a music
        // cd in it. It takes longer time than SCSI_CDROM_TIMEOUT before returning
        // error status.
        //

        fdoExtension->TimeOutValue = 20;

    } else if ((RtlCompareMemory(vendorId, "DEC     ",    8)  ==  8) &&
               (RtlCompareMemory(productId,"RRD",         3)  ==  3)) {

        PMODE_PARM_READ_WRITE_DATA modeParameters;
        SCSI_REQUEST_BLOCK         srb;
        PCDB                       cdb;
        NTSTATUS                   status;


        DebugPrint((1, "CdRom ScanForSpecial:  Found DEC RRD.\n"));

        cdData->IsDecRrd = TRUE;

        //
        // Setup an error handler to reinitialize the cd rom after it is reset?
        //
        //commonExtension->DevInfo->ClassError = DecRrdProcessError;

        //
        // Found a DEC RRD cd-rom.  These devices do not pass MS HCT
        // multi-media tests because the DEC firmware modifieds the block
        // from the PC-standard 2K to 512.  Change the block transfer size
        // back to the PC-standard 2K by using a mode select command.
        //

        modeParameters = ExAllocatePoolWithTag(NonPagedPool,
                                               sizeof(MODE_PARM_READ_WRITE_DATA),
                                               CDROM_TAG_MODE_DATA
                                               );
        if (modeParameters == NULL) {
            return;
        }

        RtlZeroMemory(modeParameters, sizeof(MODE_PARM_READ_WRITE_DATA));
        RtlZeroMemory(&srb,           sizeof(SCSI_REQUEST_BLOCK));

        //
        // Set the block length to 2K.
        //

        modeParameters->ParameterListHeader.BlockDescriptorLength =
                sizeof(MODE_PARAMETER_BLOCK);

        //
        // Set block length to 2K (0x0800) in Parameter Block.
        //

        modeParameters->ParameterListBlock.BlockLength[0] = 0x00; //MSB
        modeParameters->ParameterListBlock.BlockLength[1] = 0x08;
        modeParameters->ParameterListBlock.BlockLength[2] = 0x00; //LSB

        //
        // Build the mode select CDB.
        //

        srb.CdbLength = 6;
        srb.TimeOutValue = fdoExtension->TimeOutValue;

        cdb = (PCDB)srb.Cdb;
        cdb->MODE_SELECT.PFBit               = 1;
        cdb->MODE_SELECT.OperationCode       = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.ParameterListLength = HITACHI_MODE_DATA_SIZE;

        //
        // Send the request to the device.
        //

        status = ClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         modeParameters,
                                         sizeof(MODE_PARM_READ_WRITE_DATA),
                                         TRUE);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1, "CdRom ScanForSpecial: Setting DEC RRD to 2K block"
                        "size failed [%x]\n", status));
        }
        ExFreePool(modeParameters);

    } else if (( RtlCompareMemory( vendorId, "TOSHIBA ",    8)  ==  8 ) &&
              (( RtlCompareMemory( productId,"CD-ROM XM-3", 11) == 11))) {

        SCSI_REQUEST_BLOCK srb;
        PCDB               cdb;
        ULONG              length;
        PUCHAR             buffer;
        NTSTATUS           status;

        //
        // Set the density code and the error handler.
        //

        length = (sizeof(MODE_READ_RECOVERY_PAGE) + MODE_BLOCK_DESC_LENGTH + MODE_HEADER_LENGTH);

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
        cdb->MODE_SENSE.PageCode = 0x1;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)length;

        buffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                (sizeof(MODE_READ_RECOVERY_PAGE) + MODE_BLOCK_DESC_LENGTH + MODE_HEADER_LENGTH),
                                CDROM_TAG_MODE_DATA);
        if (!buffer) {
            return;
        }

        status = ClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         buffer,
                                         length,
                                         FALSE);

        ((PERROR_RECOVERY_DATA)buffer)->BlockDescriptor.DensityCode = 0x83;
        ((PERROR_RECOVERY_DATA)buffer)->Header.ModeDataLength = 0x0;

        RtlCopyMemory(&cdData->Header, buffer, sizeof(ERROR_RECOVERY_DATA));

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

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = 1;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)length;

        status = ClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         buffer,
                                         length,
                                         TRUE);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1,
                        "Cdrom.ScanForSpecial: Setting density code on Toshiba failed [%x]\n",
                        status));
        }

        commonExtension->DevInfo->ClassError = ToshibaProcessError;

        //
        // Lock down the toshiba error section.
        //

        MmLockPagableCodeSection(ToshibaProcessError);

        ExFreePool(buffer);

    }

    //
    // Determine special CD-DA requirements.
    //

    if (RtlCompareMemory(vendorId, "PLEXTOR ", 8) == 8) {
        SET_FLAG(cdData->XAFlags, XA_PLEXTOR_CDDA);
    } else if (RtlCompareMemory(vendorId, "NEC", 3) == 3) {
        SET_FLAG(cdData->XAFlags, XA_NEC_CDDA);
    }

    return;
}


VOID
HitachiProcessErrorGD2000(
    PDEVICE_OBJECT Fdo,
    PSCSI_REQUEST_BLOCK OriginalSrb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )
/*++

Routine Description:

   This routine checks the type of error.  If the error suggests that the
   drive has spun down and cannot reinitialize itself, send a
   START_UNIT or READ to the device.  This will force the drive to spin
   up.  This drive also loses the AGIDs it has granted when it spins down,
   which may result in playback failure the first time around.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - return the final status for this command?

    Retry - return if the command should be retried.

Return Value:

    None.

--*/
{
    PSENSE_DATA         senseBuffer = OriginalSrb->SenseInfoBuffer;

    UNREFERENCED_PARAMETER(Status);
    UNREFERENCED_PARAMETER(Retry);

    if (!TEST_FLAG(OriginalSrb->SrbStatus, SRB_STATUS_AUTOSENSE_VALID)) {
        return;
    }

    if (((senseBuffer->SenseKey & 0xf) == SCSI_SENSE_HARDWARE_ERROR) &&
        (senseBuffer->AdditionalSenseCode == 0x44)) {

        PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
        PIRP                irp;
        PIO_STACK_LOCATION  irpStack;
        PCOMPLETION_CONTEXT context;
        PSCSI_REQUEST_BLOCK newSrb;
        PCDB                cdb;

        DebugPrint((1, "HitachiProcessErrorGD2000 (%p) => Internal Target "
                    "Failure Detected -- spinning up drive\n", Fdo));

        //
        // the request should be retried because the device isn't ready
        //

        *Retry = TRUE;
        *Status = STATUS_DEVICE_NOT_READY;

        //
        // send a START_STOP unit to spin up the drive
        // NOTE: this temporarily violates the StartIo serialization
        //       mechanism, but the completion routine on this will NOT
        //       call StartNextPacket(), so it's a temporary disruption
        //       of the serialization only.
        //

        ClassSendStartUnit(Fdo);

    }

    return;
}



VOID
HitachiProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )
/*++

Routine Description:

   This routine checks the type of error.  If the error indicates CD-ROM the
   CD-ROM needs to be reinitialized then a Mode sense command is sent to the
   device.  This command disables read-ahead for the device.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - Not used.

    Retry - Not used.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PSENSE_DATA         senseBuffer = Srb->SenseInfoBuffer;
    LARGE_INTEGER       largeInt;
    PUCHAR              modePage;
    PIO_STACK_LOCATION  irpStack;
    PIRP                irp;
    PSCSI_REQUEST_BLOCK srb;
    PCOMPLETION_CONTEXT context;
    PCDB                cdb;
    ULONG_PTR            alignment;

    UNREFERENCED_PARAMETER(Status);
    UNREFERENCED_PARAMETER(Retry);

    largeInt.QuadPart = (LONGLONG) 1;

    //
    // Check the status.  The initialization command only needs to be sent
    // if UNIT ATTENTION is returned.
    //

    if (!(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)) {

        //
        // The drive does not require reinitialization.
        //

        return;
    }

    //
    // Found an HITACHI cd-rom that does not work with PIO
    // adapters when read-ahead is enabled.  Read-ahead is disabled by
    // a mode select command.  The mode select page code is zero and the
    // length is 6 bytes.  All of the other bytes should be zero.
    //

    if ((senseBuffer->SenseKey & 0xf) == SCSI_SENSE_UNIT_ATTENTION) {

        DebugPrint((1, "HitachiProcessError: Reinitializing the CD-ROM.\n"));

        //
        // Send the special mode select command to disable read-ahead
        // on the CD-ROM reader.
        //

        alignment = DeviceObject->AlignmentRequirement ?
            DeviceObject->AlignmentRequirement : 1;

        context = ExAllocatePoolWithTag(
            NonPagedPool,
            sizeof(COMPLETION_CONTEXT) +  HITACHI_MODE_DATA_SIZE + (ULONG)alignment,
            CDROM_TAG_HITACHI_ERROR
            );

        if (context == NULL) {

            //
            // If there is not enough memory to fulfill this request,
            // simply return. A subsequent retry will fail and another
            // chance to start the unit.
            //

            return;
        }

        context->DeviceObject = DeviceObject;
        srb = &context->Srb;

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

        //
        // Write length to SRB.
        //

        srb->Length = SCSI_REQUEST_BLOCK_SIZE;

        //
        // Set up SCSI bus address.
        //

        srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
        srb->TimeOutValue = fdoExtension->TimeOutValue;

        //
        // Set the transfer length.
        //

        srb->DataTransferLength = HITACHI_MODE_DATA_SIZE;
        srb->SrbFlags = fdoExtension->SrbFlags;
        srb->SrbFlags |= SRB_FLAGS_DATA_OUT | SRB_FLAGS_DISABLE_AUTOSENSE | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

        //
        // The data buffer must be aligned.
        //

        srb->DataBuffer = (PVOID) (((ULONG_PTR) (context + 1) + (alignment - 1)) &
            ~(alignment - 1));


        //
        // Build the HITACHI read-ahead mode select CDB.
        //

        srb->CdbLength = 6;
        cdb = (PCDB)srb->Cdb;
        cdb->MODE_SENSE.LogicalUnitNumber = srb->Lun;
        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SENSE.AllocationLength = HITACHI_MODE_DATA_SIZE;

        //
        // Initialize the mode sense data.
        //

        modePage = srb->DataBuffer;

        RtlZeroMemory(modePage, HITACHI_MODE_DATA_SIZE);

        //
        // Set the page length field to 6.
        //

        modePage[5] = 6;

        //
        // Build the asynchronous request to be sent to the port driver.
        //

        irp = IoBuildAsynchronousFsdRequest(IRP_MJ_WRITE,
                                           DeviceObject,
                                           srb->DataBuffer,
                                           srb->DataTransferLength,
                                           &largeInt,
                                           NULL);

        if (irp == NULL) {

            //
            // If there is not enough memory to fulfill this request,
            // simply return. A subsequent retry will fail and another
            // chance to start the unit.
            //

            ExFreePool(context);
            return;
        }

        ClassAcquireRemoveLock(DeviceObject, irp);

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
        // Save SRB address in next stack for port driver.
        //

        irpStack->Parameters.Scsi.Srb = (PVOID)srb;

        //
        // Set up IRP Address.
        //

        (VOID)IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp);

    }
}


NTSTATUS
ToshibaProcessErrorCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )

/*++

Routine Description:

    Completion routine for the ClassError routine to handle older Toshiba units
    that require setting the density code.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Irp - Pointer to irp created to set the density code.

    Context - Supplies a pointer to the Mode Select Srb.


Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{

    PSCSI_REQUEST_BLOCK srb = Context;

    //
    // Check for a frozen queue.
    //

    if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

        //
        // Unfreeze the queue getting the device object from the context.
        //

        ClassReleaseQueue(DeviceObject);
    }

    //
    // Free all of the allocations.
    //

    ClassReleaseRemoveLock(DeviceObject, Irp);

    ExFreePool(srb->DataBuffer);
    ExFreePool(srb);
    IoFreeMdl(Irp->MdlAddress);
    IoFreeIrp(Irp);

    //
    // Indicate the I/O system should stop processing the Irp completion.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}


VOID
ToshibaProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )

/*++

Routine Description:

   This routine checks the type of error.  If the error indicates a unit attention,
   the density code needs to be set via a Mode select command.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - Not used.

    Retry - Not used.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PCDROM_DATA         cdData = (PCDROM_DATA)(commonExtension->DriverData);
    PSENSE_DATA         senseBuffer = Srb->SenseInfoBuffer;
    PIO_STACK_LOCATION  irpStack;
    PIRP                irp;
    PSCSI_REQUEST_BLOCK srb;
    ULONG               length;
    PCDB                cdb;
    PUCHAR              dataBuffer;


    if (!(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)) {
        return;
    }

    //
    // The Toshiba's require the density code to be set on power up and media changes.
    //

    if ((senseBuffer->SenseKey & 0xf) == SCSI_SENSE_UNIT_ATTENTION) {


        irp = IoAllocateIrp((CCHAR)(DeviceObject->StackSize+1),
                              FALSE);

        if (!irp) {
            return;
        }

        srb = ExAllocatePoolWithTag(NonPagedPool,
                                    sizeof(SCSI_REQUEST_BLOCK),
                                    CDROM_TAG_TOSHIBA_ERROR);
        if (!srb) {
            IoFreeIrp(irp);
            return;
        }


        length = sizeof(ERROR_RECOVERY_DATA);
        dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                           length,
                                           CDROM_TAG_TOSHIBA_ERROR);
        if (!dataBuffer) {
            ExFreePool(srb);
            IoFreeIrp(irp);
            return;
        }

        irp->MdlAddress = IoAllocateMdl(dataBuffer,
                                        length,
                                        FALSE,
                                        FALSE,
                                        (PIRP) NULL);

        if (!irp->MdlAddress) {
            ExFreePool(srb);
            ExFreePool(dataBuffer);
            IoFreeIrp(irp);
            return;
        }

        //
        // Prepare the MDL
        //

        MmBuildMdlForNonPagedPool(irp->MdlAddress);

        RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

        srb->DataBuffer = dataBuffer;
        cdb = (PCDB)srb->Cdb;

        //
        // Set up the irp.
        //

        IoSetNextIrpStackLocation(irp);
        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = 0;
        irp->Flags = 0;
        irp->UserBuffer = NULL;

        //
        // Save the device object and irp in a private stack location.
        //

        irpStack = IoGetCurrentIrpStackLocation(irp);
        irpStack->DeviceObject = DeviceObject;

        //
        // Construct the IRP stack for the lower level driver.
        //

        irpStack = IoGetNextIrpStackLocation(irp);
        irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
        irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_OUT;
        irpStack->Parameters.Scsi.Srb = srb;

        IoSetCompletionRoutine(irp,
                               ToshibaProcessErrorCompletion,
                               srb,
                               TRUE,
                               TRUE,
                               TRUE);

        ClassAcquireRemoveLock(DeviceObject, irp);

        srb->Length = SCSI_REQUEST_BLOCK_SIZE;
        srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
        srb->SrbStatus = srb->ScsiStatus = 0;
        srb->NextSrb = 0;
        srb->OriginalRequest = irp;
        srb->SenseInfoBufferLength = 0;

        //
        // Set the transfer length.
        //

        srb->DataTransferLength = length;
        srb->SrbFlags = fdoExtension->SrbFlags;
        srb->SrbFlags |= SRB_FLAGS_DATA_OUT | SRB_FLAGS_DISABLE_AUTOSENSE | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;


        srb->CdbLength = 6;
        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = 1;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)length;

        //
        // Copy the Mode page into the databuffer.
        //

        RtlCopyMemory(srb->DataBuffer, &cdData->Header, length);

        //
        // Set the density code.
        //

        ((PERROR_RECOVERY_DATA)srb->DataBuffer)->BlockDescriptor.DensityCode = 0x83;

        IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, irp);
    }
}


BOOLEAN
CdRomIsPlayActive(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine determines if the cd is currently playing music.

Arguments:

    DeviceObject - Device object to test.

Return Value:

    TRUE if the device is playing music.

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    IO_STATUS_BLOCK ioStatus;
    PSUB_Q_CURRENT_POSITION currentBuffer;

    PAGED_CODE();

    //
    // if we don't think it is playing audio, don't bother checking.
    //

    if (!PLAY_ACTIVE(fdoExtension)) {
        return(FALSE);
    }

    currentBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                          sizeof(SUB_Q_CURRENT_POSITION),
                                          CDROM_TAG_PLAY_ACTIVE);

    if (currentBuffer == NULL) {
        return(FALSE);
    }

    ((PCDROM_SUB_Q_DATA_FORMAT) currentBuffer)->Format = IOCTL_CDROM_CURRENT_POSITION;
    ((PCDROM_SUB_Q_DATA_FORMAT) currentBuffer)->Track = 0;

    //
    // Build the synchronous request to be sent to ourself
    // to perform the request.
    //

    ClassSendDeviceIoControlSynchronous(
        IOCTL_CDROM_READ_Q_CHANNEL,
        DeviceObject,
        currentBuffer,
        sizeof(CDROM_SUB_Q_DATA_FORMAT),
        sizeof(SUB_Q_CURRENT_POSITION),
        FALSE,
        &ioStatus);

    if (!NT_SUCCESS(ioStatus.Status)) {
        ExFreePool(currentBuffer);
        return FALSE;
    }

    //
    // should update the playactive flag here.
    //

    if (currentBuffer->Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS) {
        PLAY_ACTIVE(fdoExtension) = TRUE;
    } else {
        PLAY_ACTIVE(fdoExtension) = FALSE;
    }

    ExFreePool(currentBuffer);

    return(PLAY_ACTIVE(fdoExtension));

}



VOID
CdRomTickHandler(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine handles the once per second timer provided by the
    Io subsystem.  It is only used when the cdrom device itself is
    a candidate for autoplay support.  It should never be called if
    the cdrom device is a changer device.

Arguments:

    DeviceObject - what to check.

Return Value:

    None.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    ULONG isRemoved;

    KIRQL             oldIrql;

    PIRP              irp;
    PIRP              heldIrpList;
    PIRP              nextIrp;
    PLIST_ENTRY       listEntry;
    PCDROM_DATA       cddata;
    PIO_STACK_LOCATION irpStack;
    UCHAR             uniqueAddress;

    isRemoved = ClassAcquireRemoveLock(DeviceObject, (PIRP) &uniqueAddress);

    //
    // We stop the timer before deleting the device.  It's safe to keep going
    // if the flag value is REMOVE_PENDING because the removal thread will be
    // blocked trying to stop the timer.
    //

    ASSERT(isRemoved != REMOVE_COMPLETE);

    //
    // This routine is reasonably safe even if the device object has a pending
    // remove

    cddata = commonExtension->DriverData;

    //
    // Since cdrom is completely synchronized there can never be more than one
    // irp delayed for retry at any time.
    //

    KeAcquireSpinLock(&(cddata->DelayedRetrySpinLock), &oldIrql);

    if(cddata->DelayedRetryIrp != NULL) {

        PIRP irp = cddata->DelayedRetryIrp;

        //
        // If we've got a delayed retry at this point then there had beter
        // be an interval for it.
        //

        ASSERT(cddata->DelayedRetryInterval != 0);
        cddata->DelayedRetryInterval--;

        if(isRemoved) {

            //
            // This device is removed - flush the timer queue
            //

            cddata->DelayedRetryIrp = NULL;
            cddata->DelayedRetryInterval = 0;

            KeReleaseSpinLock(&(cddata->DelayedRetrySpinLock), oldIrql);

            ClassReleaseRemoveLock(DeviceObject, irp);
            ClassCompleteRequest(DeviceObject, irp, IO_CD_ROM_INCREMENT);

        } else if (cddata->DelayedRetryInterval == 0) {

            //
            // Submit this IRP to the lower driver.  This IRP does not
            // need to be remembered here.  It will be handled again when
            // it completes.
            //

            cddata->DelayedRetryIrp = NULL;

            KeReleaseSpinLock(&(cddata->DelayedRetrySpinLock), oldIrql);

            DebugPrint((1,
                        "CdRomTickHandler: Reissuing request %p (thread = %p)\n",
                        irp,
                        irp->Tail.Overlay.Thread));

            //
            // feed this to the appropriate port driver
            //

            CdRomRerunRequest(fdoExtension, irp, cddata->DelayedRetryResend);
        } else {
            KeReleaseSpinLock(&(cddata->DelayedRetrySpinLock), oldIrql);
        }
    } else {
        KeReleaseSpinLock(&(cddata->DelayedRetrySpinLock), oldIrql);
    }

    ClassReleaseRemoveLock(DeviceObject, (PIRP) &uniqueAddress);
}


NTSTATUS
CdRomUpdateGeometryCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )

/*++

Routine Description:

    This routine andles the completion of the test unit ready irps
    used to determine if the media has changed.  If the media has
    changed, this code signals the named event to wake up other
    system services that react to media change (aka AutoPlay).

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

    PSCSI_REQUEST_BLOCK srb = (PSCSI_REQUEST_BLOCK) Context;
    PREAD_CAPACITY_DATA readCapacityBuffer;
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    BOOLEAN             retry;
    ULONG               retryCount;
    ULONG               lastSector;
    PIRP                originalIrp;
    PCDROM_DATA         cddata;
    UCHAR               uniqueAddress;

    //
    // This code must run at DISPATCH_LEVEL to call IoStartNextPacket()
    // The storage stack guarantees this, but this explicitly checks
    // that assumption.
    //

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    //
    // Get items saved in the private IRP stack location.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    retryCount = (ULONG)(ULONG_PTR) irpStack->Parameters.Others.Argument1;
    originalIrp = (PIRP) irpStack->Parameters.Others.Argument2;

    if (!DeviceObject) {
        DeviceObject = irpStack->DeviceObject;
    }
    ASSERT(DeviceObject);

    fdoExtension = DeviceObject->DeviceExtension;
    commonExtension = DeviceObject->DeviceExtension;
    cddata = commonExtension->DriverData;
    readCapacityBuffer = srb->DataBuffer;

    if ((NT_SUCCESS(Irp->IoStatus.Status)) && (SRB_STATUS(srb->SrbStatus) == SRB_STATUS_SUCCESS)) {

        PFOUR_BYTE from;
        PFOUR_BYTE to;
        ULONG bps;

        DebugPrint((2, "CdRomUpdateGeometryCompletion: [%p] successful completion of buddy-irp %p\n", originalIrp, Irp));
        //
        // Copy sector size from read capacity buffer to device extension
        // in reverse byte order.
        //
        from = (PFOUR_BYTE) &readCapacityBuffer->BytesPerBlock;
        to = (PFOUR_BYTE) &bps;
        to->Byte0 = from->Byte3;
        to->Byte1 = from->Byte2;
        to->Byte2 = from->Byte1;
        to->Byte3 = from->Byte0;

        if (bps == 0) {

            fdoExtension->DiskGeometry.BytesPerSector = 2048;

        } else {

            //
            // Insure that bytes per sector is a power of 2
            // This corrects a problem with the HP 4020i CDR where it
            // returns an incorrect number for bytes per sector.
            //

            ULONG lastBit = (ULONG) -1;

            while (bps) {
                lastBit++;
                bps = bps >> 1;
            }
            bps = 1 << lastBit;
            fdoExtension->DiskGeometry.BytesPerSector = bps;

        }

        DebugPrint((2,
                    "CdRomUpdateGeometryCompletion: Calculated bps %#x\n",
                    fdoExtension->DiskGeometry.BytesPerSector));

        //
        // Copy last sector in reverse byte order.
        //

        from = (PFOUR_BYTE) &readCapacityBuffer->LogicalBlockAddress;
        to = (PFOUR_BYTE) &lastSector;
        to->Byte0 = from->Byte3;
        to->Byte1 = from->Byte2;
        to->Byte2 = from->Byte1;
        to->Byte3 = from->Byte0;

        commonExtension->PartitionLength.QuadPart = (LONGLONG)(lastSector + 1);

        //
        // Using the new BytesPerBlock, calculate and store the SectorShift.
        //

        WHICH_BIT(fdoExtension->DiskGeometry.BytesPerSector,
                  fdoExtension->SectorShift);

        DebugPrint((2,"SCSI ClassReadDriveCapacity: Sector size is %d\n",
            fdoExtension->DiskGeometry.BytesPerSector));

        DebugPrint((2,"SCSI ClassReadDriveCapacity: Number of Sectors is %d\n",
            lastSector + 1));

        //
        // Calculate number of cylinders.
        //

        fdoExtension->DiskGeometry.Cylinders.QuadPart = (LONGLONG)((lastSector + 1)/(32 * 64));
        commonExtension->PartitionLength.QuadPart =
            (commonExtension->PartitionLength.QuadPart << fdoExtension->SectorShift);
        fdoExtension->DiskGeometry.MediaType = RemovableMedia;

        //
        // Assume sectors per track are 32;
        //

        fdoExtension->DiskGeometry.SectorsPerTrack = 32;

        //
        // Assume tracks per cylinder (number of heads) is 64.
        //

        fdoExtension->DiskGeometry.TracksPerCylinder = 64;

    } else {

        ULONG retryInterval;

        DebugPrint((1, "CdRomUpdateGeometryCompletion: [%p] unsuccessful "
                    "completion of buddy-irp %p (status - %lx)\n",
                    originalIrp, Irp, Irp->IoStatus.Status));

        if (srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ClassReleaseQueue(DeviceObject);
        }

        retry = ClassInterpretSenseInfo(DeviceObject,
                                        srb,
                                        IRP_MJ_SCSI,
                                        0,
                                        retryCount,
                                        &status,
                                        &retryInterval);
        if (retry) {
            retryCount--;
            if ((retryCount) && (commonExtension->IsRemoved == NO_REMOVE)) {
                PCDB cdb;

                DebugPrint((1, "CdRomUpdateGeometryCompletion: [%p] Retrying "
                            "request %p .. thread is %p\n",
                            originalIrp, Irp, Irp->Tail.Overlay.Thread));

                //
                // set up a one shot timer to get this process started over
                //

                irpStack->Parameters.Others.Argument1 = ULongToPtr( retryCount );
                irpStack->Parameters.Others.Argument2 = (PVOID) originalIrp;
                irpStack->Parameters.Others.Argument3 = (PVOID) 2;

                //
                // Setup the IRP to be submitted again in the timer routine.
                //

                irpStack = IoGetNextIrpStackLocation(Irp);
                irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
                irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_IN;
                irpStack->Parameters.Scsi.Srb = srb;
                IoSetCompletionRoutine(Irp,
                                       CdRomUpdateGeometryCompletion,
                                       srb,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                //
                // Set up the SRB for read capacity.
                //

                srb->CdbLength = 10;
                srb->TimeOutValue = fdoExtension->TimeOutValue;
                srb->SrbStatus = srb->ScsiStatus = 0;
                srb->NextSrb = 0;
                srb->Length = SCSI_REQUEST_BLOCK_SIZE;
                srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
                srb->SrbFlags = fdoExtension->SrbFlags;
                srb->SrbFlags |= SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
                srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);

                //
                // Set up the CDB
                //

                cdb = (PCDB) &srb->Cdb[0];
                cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;

                //
                // Requests queued onto this list will be sent to the
                // lower level driver during CdRomTickHandler
                //

                CdRomRetryRequest(fdoExtension, Irp, retryInterval, TRUE);

                return STATUS_MORE_PROCESSING_REQUIRED;
            }

            if (commonExtension->IsRemoved != NO_REMOVE) {

                //
                // We cannot retry the request.  Fail it.
                //

                originalIrp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;

            } else {

                //
                // This has been bounced for a number of times.  Error the
                // original request.
                //

                originalIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                RtlZeroMemory(&(fdoExtension->DiskGeometry),
                              sizeof(DISK_GEOMETRY));
                fdoExtension->DiskGeometry.BytesPerSector = 2048;
                fdoExtension->SectorShift = 11;
                commonExtension->PartitionLength.QuadPart =
                    (LONGLONG)(0x7fffffff);
                fdoExtension->DiskGeometry.MediaType = RemovableMedia;
            }
        } else {

            //
            // Set up reasonable defaults
            //

            RtlZeroMemory(&(fdoExtension->DiskGeometry),
                          sizeof(DISK_GEOMETRY));
            fdoExtension->DiskGeometry.BytesPerSector = 2048;
            fdoExtension->SectorShift = 11;
            commonExtension->PartitionLength.QuadPart = (LONGLONG)(0x7fffffff);
            fdoExtension->DiskGeometry.MediaType = RemovableMedia;
        }
    }

    //
    // Free resources held.
    //

    ExFreePool(srb->SenseInfoBuffer);
    ExFreePool(srb->DataBuffer);
    ExFreePool(srb);
    if (Irp->MdlAddress) {
        IoFreeMdl(Irp->MdlAddress);
    }
    IoFreeIrp(Irp);

    //
    // Grab an extra copy of the lock so we can make sure it's safe to call
    // start next packet later.
    //


    if (originalIrp->Tail.Overlay.Thread) {

        DebugPrint((2, "CdRomUpdateGeometryCompletion: [%p] completing "
                    "original IRP\n", originalIrp));

    } else {

        DbgPrint("CdRomUpdateGeometryCompletion: completing irp %p which has "
                 "no thread\n", originalIrp);

    }
    CdRomCompleteIrpAndStartNextPacketSafely(DeviceObject, originalIrp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
CdRomUpdateCapacity(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension,
    IN PIRP IrpToComplete,
    IN OPTIONAL PKEVENT IoctlEvent
    )

/*++

Routine Description:

    This routine updates the capacity of the disk as recorded in the device extension.
    It also completes the IRP given with STATUS_VERIFY_REQUIRED.  This routine is called
    when a media change has occurred and it is necessary to determine the capacity of the
    new media prior to the next access.

Arguments:

    DeviceExtension - the device to update
    IrpToComplete - the request that needs to be completed when done.

Return Value:

    NTSTATUS

--*/

{
    PCOMMON_DEVICE_EXTENSION commonExtension = (PCOMMON_DEVICE_EXTENSION) DeviceExtension;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = (PFUNCTIONAL_DEVICE_EXTENSION) DeviceExtension;

    PCDB                cdb;
    PIRP                irp;
    PSCSI_REQUEST_BLOCK srb;
    PREAD_CAPACITY_DATA capacityBuffer;
    PIO_STACK_LOCATION  irpStack;
    PUCHAR              senseBuffer;
    NTSTATUS            status;

    irp = IoAllocateIrp((CCHAR)(commonExtension->DeviceObject->StackSize+1),
                        FALSE);

    if (irp) {

        //
        // BUGBUG - find a better way to deal with failure.
        //

        srb = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                    sizeof(SCSI_REQUEST_BLOCK),
                                    CDROM_TAG_UPDATE_CAP);
        if (srb) {
            capacityBuffer = ExAllocatePoolWithTag(
                                NonPagedPoolCacheAlignedMustS,
                                sizeof(READ_CAPACITY_DATA),
                                CDROM_TAG_UPDATE_CAP);

            if (capacityBuffer) {


                senseBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAlignedMustS,
                                                    SENSE_BUFFER_SIZE,
                                                    CDROM_TAG_UPDATE_CAP);

                if (senseBuffer) {

                    irp->MdlAddress = IoAllocateMdl(capacityBuffer,
                                                    sizeof(READ_CAPACITY_DATA),
                                                    FALSE,
                                                    FALSE,
                                                    (PIRP) NULL);

                    if (irp->MdlAddress) {

                        //
                        // Have all resources.  Set up the IRP to send for the capacity.
                        //

                        IoSetNextIrpStackLocation(irp);
                        irp->IoStatus.Status = STATUS_SUCCESS;
                        irp->IoStatus.Information = 0;
                        irp->Flags = 0;
                        irp->UserBuffer = NULL;

                        //
                        // Save the device object and retry count in a private stack location.
                        //

                        irpStack = IoGetCurrentIrpStackLocation(irp);
                        irpStack->DeviceObject = commonExtension->DeviceObject;
                        irpStack->Parameters.Others.Argument1 = (PVOID) MAXIMUM_RETRIES;
                        irpStack->Parameters.Others.Argument2 = (PVOID) IrpToComplete;

                        //
                        // Construct the IRP stack for the lower level driver.
                        //

                        irpStack = IoGetNextIrpStackLocation(irp);
                        irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
                        irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_SCSI_EXECUTE_IN;
                        irpStack->Parameters.Scsi.Srb = srb;
                        IoSetCompletionRoutine(irp,
                                               CdRomUpdateGeometryCompletion,
                                               srb,
                                               TRUE,
                                               TRUE,
                                               TRUE);
                        //
                        // Prepare the MDL
                        //

                        MmBuildMdlForNonPagedPool(irp->MdlAddress);


                        //
                        // Set up the SRB for read capacity.
                        //

                        RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));
                        RtlZeroMemory(senseBuffer, SENSE_BUFFER_SIZE);
                        srb->CdbLength = 10;
                        srb->TimeOutValue = DeviceExtension->TimeOutValue;
                        srb->SrbStatus = srb->ScsiStatus = 0;
                        srb->NextSrb = 0;
                        srb->Length = SCSI_REQUEST_BLOCK_SIZE;
                        srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
                        srb->SrbFlags = DeviceExtension->SrbFlags;
                        srb->SrbFlags |= SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
                        srb->DataBuffer = capacityBuffer;
                        srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);
                        srb->OriginalRequest = irp;
                        srb->SenseInfoBuffer = senseBuffer;
                        srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

                        //
                        // Set up the CDB
                        //

                        cdb = (PCDB) &srb->Cdb[0];
                        cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;

                        //
                        // Set the return value in the IRP that will be completed
                        // upon completion of the read capacity.
                        //

                        IrpToComplete->IoStatus.Status = STATUS_VERIFY_REQUIRED;
                        IoMarkIrpPending(IrpToComplete);

                        IoCallDriver(commonExtension->LowerDeviceObject, irp);

                        //
                        // status is not checked because the completion routine for this
                        // IRP will always get called and it will free the resources.
                        //

                        return STATUS_PENDING;

                    } else {
                        ExFreePool(senseBuffer);
                        ExFreePool(capacityBuffer);
                        ExFreePool(srb);
                        IoFreeIrp(irp);
                    }
                } else {
                    ExFreePool(capacityBuffer);
                    ExFreePool(srb);
                    IoFreeIrp(irp);
                }
            } else {
                ExFreePool(srb);
                IoFreeIrp(irp);
            }
        } else {
            IoFreeIrp(irp);
        }
    }

    //
    // complete the original irp with a failure.
    //

    RtlZeroMemory(&(fdoExtension->DiskGeometry),
                  sizeof(DISK_GEOMETRY));
    fdoExtension->DiskGeometry.BytesPerSector = 2048;
    fdoExtension->SectorShift = 11;
    commonExtension->PartitionLength.QuadPart =
        (LONGLONG)(0x7fffffff);
    fdoExtension->DiskGeometry.MediaType = RemovableMedia;

    IrpToComplete->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
    IrpToComplete->IoStatus.Information = 0;

    BAIL_OUT(IrpToComplete);
    CdRomCompleteIrpAndStartNextPacketSafely(commonExtension->DeviceObject,
                                             IrpToComplete);
    return STATUS_INSUFFICIENT_RESOURCES;
}


NTSTATUS
CdRomClassIoctlCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine signals the event used by CdRomDeviceControl to synchronize
    class driver (and lower level driver) ioctls with cdrom's startio routine.
    The irp completion is short-circuited so that CdRomDeviceControl can
    reissue it once it wakes up.

Arguments:

    DeviceObject - the device object
    Irp - the request we are synchronizing
    Context - a PKEVENT that we need to signal

Return Value:

    NTSTATUS

--*/

{
    PKEVENT syncEvent = (PKEVENT) Context;

    DebugPrint((2, "CdRomClassIoctlCompletion: setting event for irp %p\n", Irp));

    //
    // We released the lock when we completed this request.  Reacquire it.
    //

    ClassAcquireRemoveLock(DeviceObject, Irp);

    KeSetEvent(syncEvent, IO_DISK_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
CdRomRemoveDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR Type
    )

/*++

Routine Description:

    This routine is responsible for releasing any resources in use by the
    cdrom driver and shutting down it's timer routine.  This routine is called
    when all outstanding requests have been completed and the device has
    disappeared - no requests may be issued to the lower drivers.

Arguments:

    DeviceObject - the device object being removed

Return Value:

    none - this routine may not fail

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION deviceExtension =
        DeviceObject->DeviceExtension;

    PCDROM_DATA cdData = deviceExtension->CommonExtension.DriverData;

    PAGED_CODE();

    if((Type == IRP_MN_QUERY_REMOVE_DEVICE) ||
       (Type == IRP_MN_CANCEL_REMOVE_DEVICE)) {
        return STATUS_SUCCESS;
    }

    if(cdData->DelayedRetryIrp != NULL) {
        cdData->DelayedRetryInterval = 1;
        CdRomTickHandler(DeviceObject);
    }

    if (deviceExtension->DeviceDescriptor) {
        ExFreePool(deviceExtension->DeviceDescriptor);
        deviceExtension->DeviceDescriptor = NULL;
    }

    if (deviceExtension->AdapterDescriptor) {
        ExFreePool(deviceExtension->AdapterDescriptor);
        deviceExtension->AdapterDescriptor = NULL;
    }

    if (deviceExtension->SenseData) {
        ExFreePool(deviceExtension->SenseData);
        deviceExtension->SenseData = NULL;
    }

    ClassDeleteSrbLookasideList(&deviceExtension->CommonExtension);

    if(cdData->CdromInterfaceString.Buffer != NULL) {
        IoSetDeviceInterfaceState(
            &(cdData->CdromInterfaceString),
            FALSE);
        RtlFreeUnicodeString(&(cdData->CdromInterfaceString));
        RtlInitUnicodeString(&(cdData->CdromInterfaceString), NULL);
    }

    if(cdData->VolumeInterfaceString.Buffer != NULL) {
        IoSetDeviceInterfaceState(
            &(cdData->VolumeInterfaceString),
            FALSE);
        RtlFreeUnicodeString(&(cdData->VolumeInterfaceString));
        RtlInitUnicodeString(&(cdData->VolumeInterfaceString), NULL);
    }

    CdRomDeleteWellKnownName(DeviceObject);

    ASSERT(cdData->DelayedRetryIrp == NULL);

    if(Type == IRP_MN_REMOVE_DEVICE) {
        DebugPrint((2, "CDROM.SYS Remove #%x\n",CdRomCounter));
        IoGetConfigurationInformation()->CdRomCount--;
    }

    //
    // so long, and thanks for all the fish!
    //

    return STATUS_SUCCESS;
}

NTSTATUS
CdRomDvdEndAllSessionsCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine will setup the next stack location to issue an end session
    to the device.  It will increment the session id in the system buffer
    and issue an END_SESSION for that AGID if the AGID is valid.

    When the new AGID is > 3 this routine will complete the request.

Arguments:

    DeviceObject - the device object for this drive

    Irp - the request

    Context - done

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED if there is another AGID to clear
    status otherwise.

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(Irp);

    PDVD_SESSION_ID sessionId = Irp->AssociatedIrp.SystemBuffer;

    NTSTATUS status;

    if(++(*sessionId) > MAX_COPY_PROTECT_AGID) {

        //
        // We're done here - just return success and let the io system
        // continue to complete it.
        //

        return STATUS_SUCCESS;

    }

    IoCopyCurrentIrpStackLocationToNext(Irp);

    IoSetCompletionRoutine(Irp,
                           CdRomDvdEndAllSessionsCompletion,
                           NULL,
                           TRUE,
                           FALSE,
                           FALSE);

    IoMarkIrpPending(Irp);

    IoCallDriver(fdoExtension->CommonExtension.DeviceObject, Irp);

    //
    // At this point we have to assume the irp may have already been
    // completed.  Ignore the returned status and return.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
CdRomDvdReadDiskKeyCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine handles the completion of a request to obtain the disk
    key from the dvd media.  It will transform the raw 2K of key data into
    a DVD_COPY_PROTECT_KEY structure and copy back the saved key parameters
    from the context pointer before returning.

Arguments:

    DeviceObject -

    Irp -

    Context - a DVD_COPY_PROTECT_KEY pointer which contains the key
              parameters handed down by the caller.

Return Value:

    STATUS_SUCCESS;

--*/

{
    PDVD_COPY_PROTECT_KEY savedKey = Context;

    PREAD_DVD_STRUCTURES_HEADER rawKey = Irp->AssociatedIrp.SystemBuffer;
    PDVD_COPY_PROTECT_KEY outputKey = Irp->AssociatedIrp.SystemBuffer;

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        // Shift the data down to its new position.
        //

        RtlMoveMemory(outputKey->KeyData,
                      rawKey->Data,
                      sizeof(DVD_DISK_KEY_DESCRIPTOR));

        RtlCopyMemory(outputKey,
                      savedKey,
                      sizeof(DVD_COPY_PROTECT_KEY));

        outputKey->KeyLength = DVD_DISK_KEY_LENGTH;

        Irp->IoStatus.Information = DVD_DISK_KEY_LENGTH;

    } else {

        DebugPrint((1, "DiskKey Failed with status %x, %x (%x) bytes\n",
                    Irp->IoStatus.Status,
                    Irp->IoStatus.Information,
                    ((rawKey->Length[0] << 16) | rawKey->Length[1])
                    ));

    }

    //
    // release the context block
    //

    ExFreePool(Context);

    return STATUS_SUCCESS;
}

DEVICE_TYPE
CdRomGetDeviceType(
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine figures out the real device type
    by checking CDVD_CAPABILITIES_PAGE

Arguments:

    DeviceObject -

Return Value:

    FILE_DEVICE_CD_ROM or FILE_DEVICE_DVD


--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    PCDROM_DATA cdromExtension;
    ULONG bufLength;
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    PMODE_PARAMETER_HEADER10 modePageHeader;
    PCDVD_CAPABILITIES_PAGE capPage;
    ULONG capPageOffset;
    DEVICE_TYPE deviceType;
    NTSTATUS status;
    BOOLEAN use6Byte;

    PAGED_CODE();

    //
    // NOTE: don't cache this until understand how it affects GetMediaTypes()
    //

    //
    // default device type
    //

    deviceType = FILE_DEVICE_CD_ROM;

    fdoExtension = DeviceObject->DeviceExtension;

    cdromExtension = fdoExtension->CommonExtension.DriverData;

    use6Byte = TEST_FLAG(cdromExtension->XAFlags, XA_USE_6_BYTE);

    RtlZeroMemory(&srb, sizeof(srb));
    cdb = (PCDB)srb.Cdb;

    //
    // Build the MODE SENSE CDB. The data returned will be kept in the
    // device extension and used to set block size.
    //
    if (use6Byte) {

        bufLength = sizeof(CDVD_CAPABILITIES_PAGE) +
                    sizeof(MODE_PARAMETER_HEADER);

        capPageOffset = sizeof(MODE_PARAMETER_HEADER);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = 1;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_CAPABILITIES;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufLength;
        srb.CdbLength = 6;
    } else {

        bufLength = sizeof(CDVD_CAPABILITIES_PAGE) +
                    sizeof(MODE_PARAMETER_HEADER10);

        capPageOffset = sizeof(MODE_PARAMETER_HEADER10);

        cdb->MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
        cdb->MODE_SENSE10.Dbd = 1;
        cdb->MODE_SENSE10.PageCode = MODE_PAGE_CAPABILITIES;
        cdb->MODE_SENSE10.AllocationLength[0] = (UCHAR)(bufLength >> 8);
        cdb->MODE_SENSE10.AllocationLength[1] = (UCHAR)(bufLength >> 0);
        srb.CdbLength = 10;
    }

    //
    // Set timeout value from device extension.
    //
    srb.TimeOutValue = fdoExtension->TimeOutValue;

    modePageHeader = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                           bufLength,
                                           CDROM_TAG_MODE_DATA);
    if (modePageHeader) {

        RtlZeroMemory(modePageHeader, bufLength);

        status = ClassSendSrbSynchronous(
                     DeviceObject,
                     &srb,
                     modePageHeader,
                     bufLength,
                     FALSE);

        if (NT_SUCCESS(status)) {

            capPage = (PCDVD_CAPABILITIES_PAGE) (((PUCHAR) modePageHeader) + capPageOffset);

            if ((capPage->PageCode == MODE_PAGE_CAPABILITIES) &&
                (capPage->DVDROMRead || capPage->DVDRRead ||
                 capPage->DVDRAMRead || capPage->DVDRWrite ||
                 capPage->DVDRAMWrite)) {

                deviceType = FILE_DEVICE_DVD;
            }
        }
        ExFreePool (modePageHeader);
    }

    return deviceType;
}


NTSTATUS
CdRomCreateWellKnownName(
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine creates a symbolic link to the cdrom device object
    under \dosdevices.  The number of the cdrom device does not neccessarily
    match between \dosdevices and \device, but usually will be the same.

    Saves the buffer

Arguments:

    DeviceObject -

Return Value:

    NTSTATUS

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PCDROM_DATA cdromData = commonExtension->DriverData;

    UNICODE_STRING unicodeLinkName;
    WCHAR wideLinkName[64];
    PWCHAR savedName;

    LONG cdromNumber = fdoExtension->DeviceNumber;

    NTSTATUS status;

    //
    // if already linked, assert then return
    //

    if (cdromData->WellKnownName.Buffer != NULL) {

        DebugPrint((0, "CdRomCreateWellKnownName: link already exists %p\n",
                    cdromData->WellKnownName.Buffer));
        ASSERT(FALSE);
        return STATUS_UNSUCCESSFUL;

    }

    //
    // find an unused CdRomNN to link to
    //

    do {

        swprintf(wideLinkName, L"\\DosDevices\\CdRom%d", cdromNumber);
        RtlInitUnicodeString(&unicodeLinkName, wideLinkName);
        status = IoCreateSymbolicLink(&unicodeLinkName,
                                      &(commonExtension->DeviceName));

        cdromNumber++;

    } while((status == STATUS_OBJECT_NAME_COLLISION) ||
            (status == STATUS_OBJECT_NAME_EXISTS));

    if (!NT_SUCCESS(status)) {

        DebugPrint((1, "CdRomCreateWellKnownName: Error %lx linking %wZ to "
                       "device %wZ\n",
                    status,
                    &unicodeLinkName,
                    &(commonExtension->DeviceName)));
        return status;

    }

    DebugPrint((1, "CdRomCreateWellKnownName: successfully linked %wZ "
                "to device %wZ\n",
                &unicodeLinkName,
                &(commonExtension->DeviceName)));

    //
    // Save away the symbolic link name in the driver data block.  We need
    // it so we can delete the link when the device is removed.
    //

    savedName = ExAllocatePoolWithTag(PagedPool,
                                      unicodeLinkName.MaximumLength,
                                      CDROM_TAG_STRINGS);

    if (savedName == NULL) {
        IoDeleteSymbolicLink(&unicodeLinkName);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(savedName,
                  unicodeLinkName.Buffer,
                  unicodeLinkName.MaximumLength);

    RtlInitUnicodeString(&(cdromData->WellKnownName), savedName);

    //
    // the name was saved and the link created
    //

    return STATUS_SUCCESS;
}


VOID
CdRomDeleteWellKnownName(
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PCOMMON_DEVICE_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PCDROM_DATA cdromData = commonExtension->DriverData;

    if(cdromData->WellKnownName.Buffer != NULL) {

        IoDeleteSymbolicLink(&(cdromData->WellKnownName));
        ExFreePool(cdromData->WellKnownName.Buffer);
        cdromData->WellKnownName.Buffer = NULL;
        cdromData->WellKnownName.Length = 0;
        cdromData->WellKnownName.MaximumLength = 0;

    }
    return;
}

NTSTATUS
CdromGetDeviceParameter (
    IN     PDEVICE_OBJECT      Fdo,
    IN     PWSTR               ParameterName,
    IN OUT PULONG              ParameterValue
    )
/*++

Routine Description:

    retrieve a devnode registry parameter

Arguments:

    DeviceObject - Cdrom Device Object

    ParameterName - parameter name to look up

    ParameterValuse - default parameter value

Return Value:

    NT Status

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    NTSTATUS                 status;
    HANDLE                   deviceParameterHandle;
    RTL_QUERY_REGISTRY_TABLE queryTable[2];
    ULONG                    defaultParameterValue;

    PAGED_CODE();

    //
    // open the given parameter
    //
    status = IoOpenDeviceRegistryKey(fdoExtension->LowerPdo,
                                     PLUGPLAY_REGKEY_DRIVER,
                                     KEY_READ,
                                     &deviceParameterHandle);

    if(NT_SUCCESS(status)) {

        RtlZeroMemory(queryTable, sizeof(queryTable));

        defaultParameterValue = *ParameterValue;

        queryTable->Flags         = RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
        queryTable->Name          = ParameterName;
        queryTable->EntryContext  = ParameterValue;
        queryTable->DefaultType   = REG_NONE;
        queryTable->DefaultData   = NULL;
        queryTable->DefaultLength = 0;

        status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                        (PWSTR) deviceParameterHandle,
                                        queryTable,
                                        NULL,
                                        NULL);
        if (!NT_SUCCESS(status)) {

            *ParameterValue = defaultParameterValue;
        }

        //
        // close what we open
        //
        ZwClose(deviceParameterHandle);
    }

    return status;

} // CdromGetDeviceParameter

NTSTATUS
CdromSetDeviceParameter (
    IN PDEVICE_OBJECT Fdo,
    IN PWSTR          ParameterName,
    IN ULONG          ParameterValue
    )
/*++

Routine Description:

    save a devnode registry parameter

Arguments:

    DeviceObject - Cdrom Device Object

    ParameterName - parameter name

    ParameterValuse - parameter value

Return Value:

    NT Status

--*/
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    NTSTATUS                 status;
    HANDLE                   deviceParameterHandle;

    PAGED_CODE();

    //
    // open the given parameter
    //
    status = IoOpenDeviceRegistryKey(fdoExtension->LowerPdo,
                                     PLUGPLAY_REGKEY_DRIVER,
                                     KEY_READ | KEY_WRITE,
                                     &deviceParameterHandle);

    if(NT_SUCCESS(status)) {

        status = RtlWriteRegistryValue(
                    RTL_REGISTRY_HANDLE,
                    (PWSTR) deviceParameterHandle,
                    ParameterName,
                    REG_DWORD,
                    &ParameterValue,
                    sizeof (ParameterValue));

        //
        // close what we open
        //
        ZwClose(deviceParameterHandle);
    }

    return status;

} // CdromSetDeviceParameter


VOID
CdromPickDvdRegion(
    IN PDEVICE_OBJECT Fdo
)
/*++

Routine Description:

    pick a default dvd region

Arguments:

    DeviceObject - Cdrom Device Object

Return Value:

    NT Status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = Fdo->DeviceExtension;
    PCDROM_DATA cddata = (PCDROM_DATA)(commonExtension->DriverData);

    //
    // these five pointers all point to dvdReadStructure or part of
    // its data, so don't deallocate them more than once!
    //

    PDVD_READ_STRUCTURE dvdReadStructure;
    PDVD_COPY_PROTECT_KEY copyProtectKey;
    PDVD_COPYRIGHT_DESCRIPTOR dvdCopyRight;
    PDVD_RPC_KEY rpcKey;
    PDVD_SET_RPC_KEY dvdRpcKey;

    IO_STATUS_BLOCK ioStatus;
    ULONG bufferLen;
    UCHAR mediaRegion;
    ULONG pickDvdRegion;
    ULONG defaultDvdRegion;
    ULONG dvdRegion;

    PAGED_CODE();

    if ((pickDvdRegion = InterlockedExchange(&cddata->PickDvdRegion, 0)) == 0) {

        //
        // it was non-zero, so either another thread will do this, or
        // we no longer need to pick a region
        //

        return;
    }

    //
    // short-circuit if license agreement violated
    //

    if (cddata->DvdRpc0LicenseFailure) {
        DebugPrint((1, "DVD License failure.  Refusing to pick a region\n"));
        InterlockedExchange(&cddata->PickDvdRegion, 0);
        return;
    }


    bufferLen = max(
                    max(sizeof(DVD_DESCRIPTOR_HEADER) +
                            sizeof(DVD_COPYRIGHT_DESCRIPTOR),
                        sizeof(DVD_READ_STRUCTURE)
                        ),
                    max(DVD_RPC_KEY_LENGTH,
                        DVD_SET_RPC_KEY_LENGTH
                        ),
                    );

    dvdReadStructure = (PDVD_READ_STRUCTURE)
        ExAllocatePoolWithTag(PagedPool, bufferLen, DVD_TAG_DVD_REGION);

    if (dvdReadStructure == NULL) {
        InterlockedExchange(&cddata->PickDvdRegion, pickDvdRegion);
        return;
    }

    if (cddata->DvdRpc0Device && cddata->Rpc0RetryRegistryCallback) {

        DebugPrint((1, "CdromPickDvdRegion (%p): now retrying "
                    "RPC0 callback\n", Fdo));

        //
        // get the registry settings again
        //

        ioStatus.Status = CdromGetRpc0Settings(Fdo);

        if (ioStatus.Status == STATUS_LICENSE_VIOLATION) {

            //
            // if this is the returned error, then
            // the routine should have set this!
            //

            ASSERT(cddata->DvdRpc0LicenseFailure);
            cddata->DvdRpc0LicenseFailure = 1;
            DebugPrint((1, "CdromPickDvdRegion (%p): "
                        "setting to fail all dvd ioctls due to CSS licensing "
                        "failure.\n", Fdo));

            pickDvdRegion = 0;
            goto getout;

        }

        //
        // get the device region, again
        //

        copyProtectKey = (PDVD_COPY_PROTECT_KEY)dvdReadStructure;
        RtlZeroMemory(copyProtectKey, bufferLen);
        copyProtectKey->KeyLength = DVD_RPC_KEY_LENGTH;
        copyProtectKey->KeyType = DvdGetRpcKey;

        //
        // Build a request for READ_KEY
        //

        ClassSendDeviceIoControlSynchronous(
            IOCTL_DVD_READ_KEY,
            Fdo,
            copyProtectKey,
            DVD_RPC_KEY_LENGTH,
            DVD_RPC_KEY_LENGTH,
            FALSE,
            &ioStatus);

        if (!NT_SUCCESS(ioStatus.Status)) {

            ASSERT(!"CdromPickDvdRegion: Unable to get device RPC data\n");
            pickDvdRegion = 0;
            goto getout;

        }

        //
        // now that we have gotten the device's RPC data,
        // we have set the device extension to usable data.
        // no need to call back into this section of code again
        //

        cddata->Rpc0RetryRegistryCallback = 0;


        rpcKey = (PDVD_RPC_KEY) copyProtectKey->KeyData;

        //
        // TypeCode of zero means that no region has been set.
        //

        if (rpcKey->TypeCode != 0) {
            DebugPrint((1, "CdromPickDvdRegion (%p): DVD Region already "
                        "chosen\n", Fdo));
            pickDvdRegion = 0;
            goto getout;
        }

        DebugPrint((1, "CdromPickDvdRegion (%p): must choose initial DVD "
                    " Region\n", Fdo));
    }



    copyProtectKey = (PDVD_COPY_PROTECT_KEY) dvdReadStructure;

    dvdCopyRight = (PDVD_COPYRIGHT_DESCRIPTOR)
        ((PDVD_DESCRIPTOR_HEADER) dvdReadStructure)->Data;

    //
    // get the media region
    //

    RtlZeroMemory (dvdReadStructure, bufferLen);
    dvdReadStructure->Format = DvdCopyrightDescriptor;

    //
    // Build and send a request for READ_KEY
    //

    DebugPrint((2, "CdromPickDvdRegion (%p): Getting Copyright Descriptor\n",
                Fdo));

    ClassSendDeviceIoControlSynchronous(
        IOCTL_DVD_READ_STRUCTURE,
        Fdo,
        dvdReadStructure,
        sizeof(DVD_READ_STRUCTURE),
        sizeof (DVD_DESCRIPTOR_HEADER) +
        sizeof(DVD_COPYRIGHT_DESCRIPTOR),
        FALSE,
        &ioStatus
        );
    DebugPrint((2, "CdromPickDvdRegion (%p): Got Copyright Descriptor %x\n",
                Fdo, ioStatus.Status));

    if (NT_SUCCESS(ioStatus.Status)) {

        //
        // keep the media region bitmap around
        // a 1 mean ok to play
        //

        if (dvdCopyRight->RegionManagementInformation == 0xff) {
            DebugPrint((0, "CdromPickDvdRegion (%p): RegionManagementInformation "
                        "is set to dis-allow playback for all regions.  This is "
                        "most likely a poorly authored disc.  defaulting to all "
                        "region disc for purpose of choosing initial region\n"));
            dvdCopyRight->RegionManagementInformation = 0;
        }


        mediaRegion = ~dvdCopyRight->RegionManagementInformation;

    } else {

        //
        // could be media, can't set the device region
        //

        if (!cddata->DvdRpc0Device) {

            //
            // can't automatically pick a default region on a rpc2 drive
            // without media, so just exit
            //
            DebugPrint((1, "CdromPickDvdRegion (%p): failed to auto-choose "
                        "a region due to status %x getting copyright "
                        "descriptor\n", Fdo, ioStatus.Status));
            goto getout;

        } else {

            //
            // for an RPC0 drive, we can try to pick a region for
            // the drive
            //

            mediaRegion = 0x0;
        }

    }

    //
    // get the device region
    //

    RtlZeroMemory (copyProtectKey, bufferLen);
    copyProtectKey->KeyLength = DVD_RPC_KEY_LENGTH;
    copyProtectKey->KeyType = DvdGetRpcKey;

    //
    // Build and send a request for READ_KEY for RPC key
    //

    DebugPrint((2, "CdromPickDvdRegion (%p): Getting RpcKey\n",
                Fdo));
    ClassSendDeviceIoControlSynchronous(
        IOCTL_DVD_READ_KEY,
        Fdo,
        copyProtectKey,
        DVD_RPC_KEY_LENGTH,
        DVD_RPC_KEY_LENGTH,
        FALSE,
        &ioStatus
        );
    DebugPrint((2, "CdromPickDvdRegion (%p): Got RpcKey %x\n",
                Fdo, ioStatus.Status));

    if (!NT_SUCCESS(ioStatus.Status)) {

        DebugPrint((1, "CdromPickDvdRegion (%p): failed to get RpcKey from "
                    "a DVD Device\n", Fdo));
        goto getout;

    }

    //
    // so we now have what we can get for the media region and the
    // drive region.  we will not set a region if the drive has one
    // set already (mask is not all 1's), nor will we set a region
    // if there are no more user resets available.
    //

    rpcKey = (PDVD_RPC_KEY) copyProtectKey->KeyData;


    if (rpcKey->RegionMask != 0xff) {
        DebugPrint((1, "CdromPickDvdRegion (%p): not picking a region since "
                    "it is already chosen\n", Fdo));
        goto getout;
    }

    if (rpcKey->UserResetsAvailable <= 1) {
        DebugPrint((1, "CdromPickDvdRegion (%p): not picking a region since "
                    "only one change remains\n", Fdo));
        goto getout;
    }

    defaultDvdRegion = 0;

    //
    // the proppage dvd class installer sets
    // this key based upon the system locale
    //

    CdromGetDeviceParameter (
        Fdo,
        DVD_DEFAULT_REGION,
        &defaultDvdRegion
        );

    if (defaultDvdRegion > DVD_MAX_REGION) {

        //
        // the registry has a bogus default
        //

        DebugPrint((1, "CdromPickDvdRegion (%p): registry has a bogus default "
                    "region value of %x\n", Fdo, defaultDvdRegion));
        defaultDvdRegion = 0;

    }

    //
    // if defaultDvdRegion == 0, it means no default.
    //

    //
    // we will select the initial dvd region for the user
    //

    if ((defaultDvdRegion != 0) &&
        (mediaRegion &
         (1 << (defaultDvdRegion - 1))
         )
        ) {

        //
        // first choice:
        // the media has region that matches
        // the default dvd region.
        //

        dvdRegion = (1 << (defaultDvdRegion - 1));

        DebugPrint((1, "CdromPickDvdRegion (%p): Choice #1: media matches "
                    "drive's default, chose region %x\n", Fdo, dvdRegion));


    } else if (mediaRegion) {

        //
        // second choice:
        // pick the lowest region number
        // from the media
        //

        UCHAR mask;

        mask = 1;
        dvdRegion = 0;
        while (mediaRegion && !dvdRegion) {

            //
            // pick the lowest bit
            //
            dvdRegion = mediaRegion & mask;
            mask <<= 1;
        }

        DebugPrint((1, "CdromPickDvdRegion (%p): Choice #2: choosing lowest "
                    "media region %x\n", Fdo, dvdRegion));

    } else if (defaultDvdRegion) {

        //
        // third choice:
        // default dvd region from the dvd class installer
        //

        dvdRegion = (1 << (defaultDvdRegion - 1));
        DebugPrint((1, "CdromPickDvdRegion (%p): Choice #3: using default "
                    "region for this install %x\n", Fdo, dvdRegion));

    } else {

        //
        // unable to pick one for the user -- this should rarely
        // happen, since the proppage dvd class installer sets
        // the key based upon the system locale
        //
        DebugPrint((1, "CdromPickDvdRegion (%p): Choice #4: failed to choose "
                    "a media region\n", Fdo));
        goto getout;

    }

    //
    // now that we've chosen a region, set the region by sending the
    // appropriate request to the drive
    //

    RtlZeroMemory (copyProtectKey, bufferLen);
    copyProtectKey->KeyLength = DVD_SET_RPC_KEY_LENGTH;
    copyProtectKey->KeyType = DvdSetRpcKey;
    dvdRpcKey = (PDVD_SET_RPC_KEY) copyProtectKey->KeyData;
    dvdRpcKey->PreferredDriveRegionCode = (UCHAR) ~dvdRegion;

    //
    // Build and send request for SEND_KEY
    //
    DebugPrint((2, "CdromPickDvdRegion (%p): Sending new Rpc Key to region %x\n",
                Fdo, dvdRegion));

    ClassSendDeviceIoControlSynchronous(
        IOCTL_DVD_SEND_KEY2,
        Fdo,
        copyProtectKey,
        DVD_SET_RPC_KEY_LENGTH,
        0,
        FALSE,
        &ioStatus);
    DebugPrint((2, "CdromPickDvdRegion (%p): Sent new Rpc Key %x\n",
                Fdo, ioStatus.Status));

    if (!NT_SUCCESS(ioStatus.Status)) {
        DebugPrint ((1, "CdromPickDvdRegion (%p): unable to set dvd initial "
                     " region code (%p)\n", Fdo, ioStatus.Status));
    } else {
        DebugPrint ((1, "CdromPickDvdRegion (%p): Successfully set dvd "
                     "initial region\n", Fdo));
        pickDvdRegion = 0;
    }

getout:
    if (dvdReadStructure) {
        ExFreePool (dvdReadStructure);
    }

    //
    // update the new PickDvdRegion value
    //

    InterlockedExchange(&cddata->PickDvdRegion, pickDvdRegion);

    return;
}


NTSTATUS
CdRomRetryRequest(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PIRP Irp,
    IN ULONG Delay,
    IN BOOLEAN ResendIrp
    )
{
    PCDROM_DATA cdData;
    KIRQL oldIrql;

    if(Delay == 0) {
        return CdRomRerunRequest(FdoExtension, Irp, ResendIrp);
    }

    cdData = FdoExtension->CommonExtension.DriverData;

    KeAcquireSpinLock(&(cdData->DelayedRetrySpinLock), &oldIrql);

    ASSERT(cdData->DelayedRetryIrp == NULL);
    ASSERT(cdData->DelayedRetryInterval == 0);

    cdData->DelayedRetryIrp = Irp;
    cdData->DelayedRetryInterval = Delay;
    cdData->DelayedRetryResend = ResendIrp;

    KeReleaseSpinLock(&(cdData->DelayedRetrySpinLock), oldIrql);

    return STATUS_PENDING;
}


NTSTATUS
CdRomRerunRequest(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN OPTIONAL PIRP Irp,
    IN BOOLEAN ResendIrp
    )
{
    if(ResendIrp) {
        return IoCallDriver(FdoExtension->CommonExtension.LowerDeviceObject,
                            Irp);
    } else {
        CdRomStartIo(FdoExtension->DeviceObject, Irp);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }
}



