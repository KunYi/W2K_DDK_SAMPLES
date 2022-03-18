/*++

Copyright (C) Microsoft Corporation, 1991 - 1999

Module Name:

    geometry.c

Abstract:

    SCSI disk class driver - this module contains all the code for generating 
    disk geometries.

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "disk.h"
#include "ntddstor.h"

#if defined(_X86_)

DISK_GEOMETRY_SOURCE
DiskUpdateGeometry(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
DiskUpdateRemovableGeometry (
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    );

VOID
DiskScanBusDetectInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN HANDLE BusKey
    );

NTSTATUS
DiskSaveBusDetectInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN HANDLE TargetKey,
    IN ULONG DiskNumber
    );

NTSTATUS
DiskSaveGeometryDetectInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN HANDLE HardwareKey
    );

NTSTATUS
DiskGetPortGeometry(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    OUT PDISK_GEOMETRY Geometry
    );

NTSTATUS
DiskGetNEC98Geometry(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    IN PDISK_GEOMETRY DiskGeometry
    );

BOOLEAN
DiskIsFormatMediaTypeNEC_98(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    );

typedef struct _DISK_DETECT_INFO {
    BOOLEAN Initialized;
    ULONG Signature;
    ULONG MbrCheckSum;
    PDEVICE_OBJECT Device;
    CM_INT13_DRIVE_PARAMETER DriveParameters;
} DISK_DETECT_INFO, *PDISK_DETECT_INFO;

//
// Information about the disk geometries collected and saved into the registry 
// by NTDETECT.COM or the system firmware.
//

PDISK_DETECT_INFO DetectInfoList = NULL;
ULONG DetectInfoCount = 0;
ULONG DetectInfoUsedCount = 0;

#define IPL1_OFFSET             4
#define BOOT_SIGNATURE_OFFSET   ((0x200 / 2) -1)
#define BOOT_RECORD_SIGNATURE   (0xAA55)
#define SECTORS_DISK_8GB        0x1000000

#define PARTITION_TABLE_OFFSET         (0x1be / 2)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DiskSaveDetectInfo)
#pragma alloc_text(INIT, DiskScanBusDetectInfo)
#pragma alloc_text(INIT, DiskSaveBusDetectInfo)
#pragma alloc_text(INIT, DiskSaveGeometryDetectInfo)

#pragma alloc_text(PAGE, DiskReadSignature)
#pragma alloc_text(PAGE, DiskUpdateGeometry)
#pragma alloc_text(PAGE, DiskUpdateRemovableGeometry)
#pragma alloc_text(PAGE, DiskGetPortGeometry)
#pragma alloc_text(PAGE, DiskGetNEC98Geometry)
#pragma alloc_text(PAGE, DiskIsFormatMediaTypeNEC_98)
#endif


NTSTATUS
DiskReadSignature(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )

/*++

Routine Description:

    This routine will read the disk signature and calculate a checksum of the 
    contents of the MBR.

    The signature and MBR checksum will be saved in the disk data block.

Arguments:

    DeviceExtension - Supplies a pointer to the device information for disk.

Return Value:

    status

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = &(FdoExtension->CommonExtension);
    PDISK_DATA diskData = commonExtension->DriverData;

    KEVENT event;

    LARGE_INTEGER sectorZero;
    PIRP irp;
    IO_STATUS_BLOCK ioStatus;
    ULONG sectorSize;
    PULONG mbr;

    ULONG checksum;
    ULONG i;

    NTSTATUS status;

    BOOLEAN diskIsNec98Format = FALSE;

    PAGED_CODE();

    ASSERT_FDO(FdoExtension->DeviceObject);

    sectorZero.QuadPart = (LONGLONG) 0;

    //
    // Create notification event object to be used to signal the inquiry
    // request completion.
    //

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    //
    // Get sector size.
    //

    sectorSize = FdoExtension->DiskGeometry.BytesPerSector;

    //
    // Make sure sector size is at least 512 bytes.
    //

    if (sectorSize < 512) {
        sectorSize = 512;
    }

    if ( IsNEC_98 ) {
        diskIsNec98Format = DiskIsFormatMediaTypeNEC_98(FdoExtension);

        if(diskIsNec98Format) {
            //
            // Target sector is 1 for checksum.
            //

            sectorZero.QuadPart = (LONGLONG)sectorSize;
        }
    }

    //
    // Allocate buffer for sector read.
    //

    mbr = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                sectorSize,
                                DISK_CACHE_MBR_CHECK);

    if (!mbr) {
        return FALSE;
    }

    //
    // Build IRP to read MBR.
    //

    irp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                                       FdoExtension->DeviceObject,
                                       mbr,
                                       sectorSize,
                                       &sectorZero,
                                       &event,
                                       &ioStatus );

    if (!irp) {
        ExFreePool(mbr);
        return FALSE;
    }

    //
    // Pass request to port driver and wait for request to complete.
    //

    status = IoCallDriver(FdoExtension->DeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        ExFreePool(mbr);
        return status;
    }

    //
    // Calculate MBR checksum.
    //

    diskData->MbrCheckSum = 0;

    for (i = 0; i < 128; i++) {
        diskData->MbrCheckSum += mbr[i];
    }

    diskData->MbrCheckSum = ~(diskData->MbrCheckSum) + 1;

    //
    // Get the signature out of the sector and save it in the disk data block.
    //

    if (!diskIsNec98Format) {

        diskData->Signature = mbr[PARTITION_TABLE_OFFSET/2-1];

    } else {

        //
        // The signature for NEC98 format hard disk is in 17th sector, first 4 bytes.
        // Build IRP to read this sector.
        //

        RtlZeroMemory(mbr, sectorSize);

        sectorZero.QuadPart = (LONGLONG) sectorSize * 16;

        irp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                                           FdoExtension->DeviceObject,
                                           mbr,
                                           sectorSize,
                                           &sectorZero,
                                           &event,
                                           &ioStatus );

        if (!irp) {
            ExFreePool(mbr);
            return FALSE;
        }

        //
        // Pass request to port driver and wait for request to complete.
        //

        status = IoCallDriver(FdoExtension->DeviceObject, irp);

        if (status == STATUS_PENDING) {
            KeWaitForSingleObject(&event,
                                  Suspended,
                                  KernelMode,
                                  FALSE,
                                  NULL);
            status = ioStatus.Status;
        }

        if (!NT_SUCCESS(status)) {
            ExFreePool(mbr);
            return status;
        }

        diskData->Signature = 0;

        if(((PUSHORT)mbr)[BOOT_SIGNATURE_OFFSET] == BOOT_RECORD_SIGNATURE) {
            diskData->Signature = mbr[0];
        }
    }

    ExFreePool(mbr);
    return status;
}


BOOLEAN
EnumerateBusKey(
    IN PFUNCTIONAL_DEVICE_EXTENSION DeviceExtension,
    HANDLE BusKey,
    PULONG DiskNumber
    )

/*++

Routine Description:

    The routine queries the registry to determine if this disk is visible to
    the BIOS.  If the disk is visable to the BIOS, then the geometry information
    is updated.

Arguments:

    DeviceExtension - Supplies a pointer to the device information for disk.
    Signature - Unique identifier recorded in MBR.
    BusKey - Handle of bus key.
    DiskNumber - Returns ordinal of disk as BIOS sees it.

Return Value:

    TRUE is disk signature matched.

--*/
{
    PDISK_DATA        diskData = (PDISK_DATA)(DeviceExtension->CommonExtension.DriverData);
    BOOLEAN           diskFound = FALSE;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING    unicodeString;
    UNICODE_STRING    identifier;
    ULONG             busNumber;
    ULONG             adapterNumber;
    ULONG             diskNumber;
    HANDLE            adapterKey;
    HANDLE            spareKey;
    HANDLE            diskKey;
    HANDLE            targetKey;
    NTSTATUS          status;
    STRING            string;
    STRING            anotherString;
    ULONG             length;
    UCHAR             buffer[20];

    PAGED_CODE();

    ASSERT(DeviceExtension->CommonExtension.IsFdo);

    for (busNumber = 0; ; busNumber++) {

        //
        // Open controller name key.
        //

        sprintf(buffer,
                "%d",
                busNumber);

        RtlInitString(&string,
                      buffer);

        status = RtlAnsiStringToUnicodeString(&unicodeString,
                                              &string,
                                              TRUE);

        if (!NT_SUCCESS(status)){
            break;
        }

        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   BusKey,
                                   (PSECURITY_DESCRIPTOR)NULL);

        status = ZwOpenKey(&spareKey,
                           KEY_READ,
                           &objectAttributes);

        RtlFreeUnicodeString(&unicodeString);

        if (!NT_SUCCESS(status)) {
            break;
        }

        //
        // Open up controller ordinal key.
        //

        RtlInitUnicodeString(&unicodeString, L"DiskController");
        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   spareKey,
                                   (PSECURITY_DESCRIPTOR)NULL);

        status = ZwOpenKey(&adapterKey,
                           KEY_READ,
                           &objectAttributes);

        ZwClose(spareKey);

        //
        // This could fail even with additional adapters of this type
        // to search.
        //

        if (!NT_SUCCESS(status)) {
            continue;
        }

        for (adapterNumber = 0; ; adapterNumber++) {

            //
            // Open disk key.
            //

            sprintf(buffer,
                    "%d\\DiskPeripheral",
                    adapterNumber);

            RtlInitString(&string,
                          buffer);

            status = RtlAnsiStringToUnicodeString(&unicodeString,
                                                  &string,
                                                  TRUE);

            if (!NT_SUCCESS(status)){
                break;
            }

            InitializeObjectAttributes(&objectAttributes,
                                       &unicodeString,
                                       OBJ_CASE_INSENSITIVE,
                                       adapterKey,
                                       (PSECURITY_DESCRIPTOR)NULL);

            status = ZwOpenKey(&diskKey,
                               KEY_READ,
                               &objectAttributes);

            RtlFreeUnicodeString(&unicodeString);

            if (!NT_SUCCESS(status)) {
                break;
            }

            for (diskNumber = 0; ; diskNumber++) {

                PKEY_VALUE_FULL_INFORMATION keyData;

                sprintf(buffer,
                        "%d",
                        diskNumber);

                RtlInitString(&string,
                              buffer);

                status = RtlAnsiStringToUnicodeString(&unicodeString,
                                                      &string,
                                                      TRUE);

                if (!NT_SUCCESS(status)){
                    break;
                }

                InitializeObjectAttributes(&objectAttributes,
                                           &unicodeString,
                                           OBJ_CASE_INSENSITIVE,
                                           diskKey,
                                           (PSECURITY_DESCRIPTOR)NULL);

                status = ZwOpenKey(&targetKey,
                                   KEY_READ,
                                   &objectAttributes);

                RtlFreeUnicodeString(&unicodeString);

                if (!NT_SUCCESS(status)) {
                    break;
                }

                //
                // Allocate buffer for registry query.
                //

                keyData = ExAllocatePoolWithTag(PagedPool,
                                                VALUE_BUFFER_SIZE,
                                                DISK_TAG_GENERAL);

                if (keyData == NULL) {
                    ZwClose(targetKey);
                    continue;
                }

                //
                // Get disk peripheral identifier.
                //

                RtlInitUnicodeString(&unicodeString, L"Identifier");
                status = ZwQueryValueKey(targetKey,
                                         &unicodeString,
                                         KeyValueFullInformation,
                                         keyData,
                                         VALUE_BUFFER_SIZE,
                                         &length);

                ZwClose(targetKey);

                if (!NT_SUCCESS(status)) {
                    ExFreePool(keyData);
                    continue;
                }

                //
                // Complete unicode string.
                //

                identifier.Buffer =
                    (PWSTR)((PUCHAR)keyData + keyData->DataOffset);
                identifier.Length = (USHORT)keyData->DataLength;
                identifier.MaximumLength = (USHORT)keyData->DataLength;

                //
                // Convert unicode identifier to ansi string.
                //

                status =
                    RtlUnicodeStringToAnsiString(&anotherString,
                                                 &identifier,
                                                 TRUE);

                if (!NT_SUCCESS(status)) {
                    ExFreePool(keyData);
                    continue;
                }

                //
                // If checksum is zero, then the MBR is valid and
                // the signature is meaningful.
                //

                if (diskData->MbrCheckSum) {

                    //
                    // Convert checksum to ansi string.
                    //

                    sprintf(buffer, "%08x", diskData->MbrCheckSum);

                } else {

                    if(anotherString.Length <= 9) {

                        ExFreePool(keyData);
                        RtlFreeAnsiString(&anotherString);
                        continue;
                    }
                    if ( !IsNEC_98 ) {

                        //
                        // Convert signature to ansi string.
                        //

                        sprintf(buffer, "%08x", diskData->Signature);

                        //
                        // Make string point at signature. Can't use scan
                        // functions because they are not exported for driver use.
                        //

                        anotherString.Buffer+=9;

                    } else {
                        //
                        // Aid there is no partition on the disk and exist Disk Signature on the one.
                        // When there is no partition, MbrCheckSum of PC-9800 is zero.
                        // Signature which hal.dll return is null.
                        // at this time, even if MbrCheckSum is zero, PC-9800 must use MbrCheckSum.
                        //
                        if (diskData->Signature) {

                            //
                            // Convert signature to ansi string.
                            //

                            sprintf(buffer, "%08x", diskData->Signature);

                            //
                            // Make string point at signature. Can't use scan
                            // functions because they are not exported for driver use.
                            //

                            anotherString.Buffer+=9;

                        }else{

                            sprintf(buffer, "%08x", diskData->MbrCheckSum);
                        }
                    }
                }

                //
                // Convert to ansi string.
                //

                RtlInitString(&string,
                              buffer);


                //
                // Make string lengths equal.
                //

                anotherString.Length = string.Length;

                //
                // Check if strings match.
                //

                if (RtlCompareString(&string,
                                     &anotherString,
                                     TRUE) == 0)  {

                    diskFound = TRUE;
                    *DiskNumber = diskNumber;
                }

                ExFreePool(keyData);

                //
                // Readjust indentifier string if necessary.
                //

                if (!diskData->MbrCheckSum) {
                    if ( !IsNEC_98 ) {

                        anotherString.Buffer-=9;
                    } else {
                        if (diskData->Signature) {

                            anotherString.Buffer-=9;
                        }
                    }
                }

                RtlFreeAnsiString(&anotherString);

                if (diskFound) {
                    break;
                }
            }

            ZwClose(diskKey);
        }

        ZwClose(adapterKey);
    }

    ZwClose(BusKey);
    return diskFound;

} // end EnumerateBusKey()


NTSTATUS
DiskSaveDetectInfo(
    PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    This routine saves away the firmware information about the disks which has 
    been saved in the registry.  It generates a list (DetectInfoList) which 
    contains the disk geometries, signatures & checksums of all drives which 
    were examined by NtDetect.  This list is later used to assign geometries 
    to disks as they are initialized.

Arguments:

    DriverObject - the driver being initialized.  This is used to get to the 
                   hardware database.
    
Return Value:

    status.
    
--*/        
    
{
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE hardwareKey;

    UNICODE_STRING unicodeString;
    HANDLE busKey;

    NTSTATUS status;

    PAGED_CODE();

    InitializeObjectAttributes(
        &objectAttributes,
        DriverObject->HardwareDatabase,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    //
    // Create the hardware base key.
    //

    status = ZwOpenKey(&hardwareKey, KEY_READ, &objectAttributes);

    if(!NT_SUCCESS(status)) {
        DebugPrint((1, "DiskSaveDetectInfo: Cannot open hardware data. "
                       "Name: %wZ\n", 
                    DriverObject->HardwareDatabase));
        return status;
    }

    status = DiskSaveGeometryDetectInfo(DriverObject, hardwareKey);

    if(!NT_SUCCESS(status)) {
        DebugPrint((1, "DiskSaveDetectInfo: Can't query configuration data "
                       "(%#08lx)\n",
                    status));
        ZwClose(hardwareKey);
        return status;
    }

    //
    // Open EISA bus key.
    //

    RtlInitUnicodeString(&unicodeString, L"EisaAdapter");
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeString,
                               OBJ_CASE_INSENSITIVE,
                               hardwareKey,
                               NULL);

    status = ZwOpenKey(&busKey,
                       KEY_READ,
                       &objectAttributes);

    if(NT_SUCCESS(status)) {
        DebugPrint((1, "DiskSaveDetectInfo: Opened EisaAdapter key\n"));
        DiskScanBusDetectInfo(DriverObject, busKey);
        ZwClose(busKey);
    }

    //
    // Open MultiFunction bus key.
    // 

    RtlInitUnicodeString(&unicodeString, L"MultifunctionAdapter");
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeString,
                               OBJ_CASE_INSENSITIVE,
                               hardwareKey,
                               NULL);

    status = ZwOpenKey(&busKey,
                       KEY_READ,
                       &objectAttributes);

    if(NT_SUCCESS(status)) {
        DebugPrint((1, "DiskSaveDetectInfo: Opened MultifunctionAdapter key\n"));
        DiskScanBusDetectInfo(DriverObject, busKey);
        ZwClose(busKey);
    }

    ZwClose(hardwareKey);

    return STATUS_SUCCESS;
}


VOID
DiskCleanupDetectInfo(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:
    
    This routine will cleanup the data structure built by DiskSaveDetectInfo.
    
Arguments:

    DriverObject - a pointer to the kernel object for this driver.
    
Return Value:

    none
    
--*/            

{
    if(DetectInfoList != NULL) {
        ExFreePool(DetectInfoList);
        DetectInfoList = NULL;
    }
    return;
}


NTSTATUS
DiskSaveGeometryDetectInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN HANDLE HardwareKey
    )
{
    UNICODE_STRING unicodeString;
    PKEY_VALUE_FULL_INFORMATION keyData;
    ULONG length;

    PCM_FULL_RESOURCE_DESCRIPTOR fullDescriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialDescriptor;

    PCM_INT13_DRIVE_PARAMETER driveParameters;
    ULONG numberOfDrives;

    ULONG i;

    NTSTATUS status;

    PAGED_CODE();

    //
    // Get disk BIOS geometry information.
    //

    RtlInitUnicodeString(&unicodeString, L"Configuration Data");

    keyData = ExAllocatePoolWithTag(PagedPool,
                                    VALUE_BUFFER_SIZE,
                                    DISK_TAG_UPDATE_GEOM);
                                   
    if(keyData == NULL) {
        DebugPrint((1, "DiskSaveGeometryDetectInfo: Can't allocate config "
                       "data buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = ZwQueryValueKey(HardwareKey,
                             &unicodeString,
                             KeyValueFullInformation,
                             keyData,
                             VALUE_BUFFER_SIZE,
                             &length);

    if(!NT_SUCCESS(status)) {
        DebugPrint((1, "DiskSaveGeometryDetectInfo: Can't query configuration "
                       "data (%#08lx)\n",
                    status));
        ExFreePool(keyData);
        return status;
    }

    //
    // Extract the resource list out of the key data.
    // 

    fullDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR)
                      (((PUCHAR) keyData) + keyData->DataOffset);
    partialDescriptor = 
        fullDescriptor->PartialResourceList.PartialDescriptors;
    length = partialDescriptor->u.DeviceSpecificData.DataSize;

    if((keyData->DataLength < sizeof(CM_FULL_RESOURCE_DESCRIPTOR)) ||
       (fullDescriptor->PartialResourceList.Count == 0) ||
       (partialDescriptor->Type != CmResourceTypeDeviceSpecific) ||
       (length < sizeof(ULONG))) {

        DebugPrint((1, "DiskSaveGeometryDetectInfo: BIOS header data too small "
                       "or invalid\n"));
        ExFreePool(keyData);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Point to the BIOS data.  THe BIOS data is located after the first 
    // partial Resource list which should be device specific data.
    //

    {
        PUCHAR buffer = (PUCHAR) keyData;
        buffer += keyData->DataOffset;
        buffer += sizeof(CM_FULL_RESOURCE_DESCRIPTOR);
        driveParameters = (PCM_INT13_DRIVE_PARAMETER) buffer;
    }

    numberOfDrives = length / sizeof(CM_INT13_DRIVE_PARAMETER);

    //
    // Allocate our detect info list now that we know how many entries there 
    // are going to be.  No other routine allocates detect info and this is 
    // done out of DriverEntry so we don't need to synchronize it's creation.
    //

    length = sizeof(DISK_DETECT_INFO) * numberOfDrives;
    DetectInfoList = ExAllocatePoolWithTag(PagedPool,
                                           length,
                                           DISK_TAG_UPDATE_GEOM);

    if(DetectInfoList == NULL) {
        DebugPrint((1, "DiskSaveGeometryDetectInfo: Couldn't allocate %x bytes "
                       "for DetectInfoList\n", 
                    length));

        ExFreePool(keyData);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DetectInfoCount = numberOfDrives;

    RtlZeroMemory(DetectInfoList, length);

    //
    // Copy the information out of the key data and into the list we've 
    // allocated.
    //

    for(i = 0; i < numberOfDrives; i++) {
        DetectInfoList[i].DriveParameters = driveParameters[i];
    }

    ExFreePool(keyData);
    return STATUS_SUCCESS;
}


VOID
DiskScanBusDetectInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN HANDLE BusKey
    )
/*++

Routine Description:

    The routine queries the registry to determine which disks are visible to
    the BIOS.  If a disk is visable to the BIOS then the geometry information
    is updated with the disk's signature and MBR checksum.

Arguments:

    DriverObject - the object for this driver.
    BusKey - handle to the bus key to be enumerated.

Return Value:

    status
    
--*/
{
    ULONG busNumber;

    NTSTATUS status;

    for(busNumber = 0; ; busNumber++) {

        WCHAR buffer[32];
        UNICODE_STRING unicodeString;

        OBJECT_ATTRIBUTES objectAttributes;

        HANDLE spareKey;
        HANDLE adapterKey;

        ULONG adapterNumber;

        DebugPrint((1, "DiskScanBusDetectInfo: Scanning bus %d\n", busNumber));

        //
        // Open controller name key.
        //

        swprintf(buffer, L"%d", busNumber);
        RtlInitUnicodeString(&unicodeString, buffer);

        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   BusKey,
                                   NULL);

        status = ZwOpenKey(&spareKey, KEY_READ, &objectAttributes);

        if(!NT_SUCCESS(status)) {
            DebugPrint((1, "DiskScanBusDetectInfo: Error %#08lx opening bus "
                           "key %#x\n", 
                        status, busNumber));
            break;
        }

        //
        // Open up a controller ordinal key.
        //

        RtlInitUnicodeString(&unicodeString, L"DiskController");
        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   spareKey,
                                   NULL);

        status = ZwOpenKey(&adapterKey, KEY_READ, &objectAttributes);
        ZwClose(spareKey);

        if(!NT_SUCCESS(status)) {
            DebugPrint((1, "DiskScanBusDetectInfo: Error %#08lx opening "
                           "DiskController key\n", 
                           status));
            continue;
        }

        for(adapterNumber = 0; ; adapterNumber++) {

            HANDLE diskKey;
            ULONG diskNumber;
            
            //
            // Open disk key.
            //

            DebugPrint((1, "DiskScanBusDetectInfo: Scanning disk key "
                           "%d\\DiskController\\%d\\DiskPeripheral\n",
                           busNumber, adapterNumber));

            swprintf(buffer, L"%d\\DiskPeripheral", adapterNumber);
            RtlInitUnicodeString(&unicodeString, buffer);

            InitializeObjectAttributes(&objectAttributes,
                                       &unicodeString,
                                       OBJ_CASE_INSENSITIVE,
                                       adapterKey,
                                       NULL);

            status = ZwOpenKey(&diskKey, KEY_READ, &objectAttributes);

            if(!NT_SUCCESS(status)) {
                DebugPrint((1, "DiskScanBusDetectInfo: Error %#08lx opening "
                               "disk key\n",
                               status));
                break;
            }

            for(diskNumber = 0; ; diskNumber++) {

                HANDLE targetKey;

                DebugPrint((1, "DiskScanBusDetectInfo: Scanning target key "
                               "%d\\DiskController\\%d\\DiskPeripheral\\%d\n",
                               busNumber, adapterNumber, diskNumber));

                swprintf(buffer, L"%d", diskNumber);
                RtlInitUnicodeString(&unicodeString, buffer);

                InitializeObjectAttributes(&objectAttributes,
                                           &unicodeString,
                                           OBJ_CASE_INSENSITIVE,
                                           diskKey,
                                           NULL);

                status = ZwOpenKey(&targetKey, KEY_READ, &objectAttributes);

                if(!NT_SUCCESS(status)) {
                    DebugPrint((1, "DiskScanBusDetectInfo: Error %#08lx "
                                   "opening target key\n", 
                                status));
                    break;
                }

                status = DiskSaveBusDetectInfo(DriverObject, 
                                               targetKey,
                                               diskNumber);

                ZwClose(targetKey);
            }

            ZwClose(diskKey);
        }
        ZwClose(adapterKey);
    }
    return;
}


NTSTATUS
DiskSaveBusDetectInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN HANDLE TargetKey,
    IN ULONG DiskNumber
    )
/*++

Routine Description:

    This routine will transfer the firmware/ntdetect reported information 
    in the specified target key into the appropriate entry in the 
    DetectInfoList.

Arguments:

    DriverObject - the object for this driver.
    
    TargetKey - the key for the disk being saved.    

    DiskNumber - the ordinal of the entry in the DiskPeripheral tree for this 
                 entry

Return Value:

    status    

--*/
{
    PDISK_DETECT_INFO diskInfo;

    UNICODE_STRING unicodeString;

    PKEY_VALUE_FULL_INFORMATION keyData;
    ULONG length;

    NTSTATUS status;

    PAGED_CODE();

    diskInfo = &(DetectInfoList[DiskNumber]);

    if(diskInfo->Initialized) {
        ASSERT(FALSE);
        DebugPrint((1, "DiskSaveBusDetectInfo: disk entry %#x already has a "
                       "signature of %#08lx and mbr checksum of %#08lx\n", 
                    DiskNumber,
                    diskInfo->Signature,
                    diskInfo->MbrCheckSum));
        return STATUS_UNSUCCESSFUL;
    }

    RtlInitUnicodeString(&unicodeString, L"Identifier");

    keyData = ExAllocatePoolWithTag(PagedPool,
                                    VALUE_BUFFER_SIZE,
                                    DISK_TAG_UPDATE_GEOM);

    if(keyData == NULL) {
        DebugPrint((1, "DiskSaveBusDetectInfo: Couldn't allocate space for "
                       "registry data\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Get disk peripheral identifier.
    //

    status = ZwQueryValueKey(TargetKey,
                             &unicodeString,
                             KeyValueFullInformation,
                             keyData,
                             VALUE_BUFFER_SIZE,
                             &length);

    if(!NT_SUCCESS(status)) {
        DebugPrint((1, "DiskSaveBusDetectInfo: Error %#08lx getting "
                       "Identifier\n",
                    status));
        ExFreePool(keyData);
        return status;

    } else if (keyData->DataLength < 9*sizeof(WCHAR)) {

        //
        // the data is too short to use (we subtract 9 chars in normal path)
        //
        DebugPrint((1, "DiskSaveBusDetectInfo: Saved data was invalid, "
                    "not enough data in registry!\n"));
        ExFreePool(keyData);
        return STATUS_UNSUCCESSFUL;
        
    } else {
        UNICODE_STRING identifier;
        ULONG value;

        //
        // Complete unicode string.
        //
    
        identifier.Buffer = (PWSTR) ((PUCHAR)keyData + keyData->DataOffset);
        identifier.Length = (USHORT) keyData->DataLength;
        identifier.MaximumLength = (USHORT) keyData->DataLength;
    
        //
        // Get the first value out of the identifier - this will be the MBR 
        // checksum.
        //
    
        status = RtlUnicodeStringToInteger(&identifier, 16, &value);
    
        if(!NT_SUCCESS(status)) {
            DebugPrint((1, "DiskSaveBusDetectInfo: Error %#08lx converting "
                           "identifier %wZ into MBR xsum\n", 
                           status,
                           &identifier));
            ExFreePool(keyData);
            return status;
        }
    
        diskInfo->MbrCheckSum = value;
    
        //
        // Shift the string over to get the disk signature
        //
    
        identifier.Buffer += 9;
        identifier.Length -= 9 * sizeof(WCHAR);
        identifier.MaximumLength -= 9 * sizeof(WCHAR);
    
        status = RtlUnicodeStringToInteger(&identifier, 16, &value);
    
        if(!NT_SUCCESS(status)) {
            DebugPrint((1, "DiskSaveBusDetectInfo: Error %#08lx converting "
                           "identifier %wZ into disk signature\n", 
                           status,
                           &identifier));
            ExFreePool(keyData);
            value = 0;
        }

        diskInfo->Signature = value;
    }

    //
    // Here is where we would save away the extended int13 data.
    //

    //
    // Mark this entry as initialized so we can make sure not to do it again.
    //

    diskInfo->Initialized = TRUE;


    return STATUS_SUCCESS;
}


DISK_GEOMETRY_SOURCE
DiskUpdateGeometry(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
/*++

Routine Description:

    This routine checks the DetectInfoList saved away during disk driver init
    to see if any geometry information was reported for this drive.  If the 
    geometry data exists (determined by matching non-zero signatures or 
    non-zero MBR checksums) then it will be saved in the RealGeometry member
    of the disk data block.

    ClassReadDriveCapacity MUST be called after calling this routine to update
    the cylinder count based on the size of the disk and the presence of any 
    disk management software.

Arguments:

    DeviceExtension - Supplies a pointer to the device information for disk.

Return Value:

    Inidicates whether the "RealGeometry" in the data block is now valid.

--*/

{
    PDISK_DATA diskData = FdoExtension->CommonExtension.DriverData;

    ULONG i;
    PDISK_DETECT_INFO diskInfo;

    BOOLEAN found = FALSE;

    NTSTATUS status;

    PAGED_CODE();

    ASSERT(FdoExtension->CommonExtension.IsFdo);
    ASSERT((FdoExtension->DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) == 0);

    //
    // If we've already set a non-default geometry for this drive then there's 
    // no need to try and update again.
    //

    if(diskData->GeometrySource != DiskGeometryUnknown) {
        return diskData->GeometrySource;
    }

    //
    // Scan through the saved detect info to see if we can find a match 
    // for this device.
    //

    for(i = 0; i < DetectInfoCount; i++) {

        ASSERT(DetectInfoList != NULL);

        diskInfo = &(DetectInfoList[i]);

        if((diskData->Signature != 0) && 
           (diskInfo->Signature == diskData->Signature)) { 
            DebugPrint((1, "DiskUpdateGeometry: found match for signature "
                           "%#08lx\n",
                        diskData->Signature));
            found = TRUE;
            break;
        } else if((diskData->Signature == 0) && 
                  (diskData->MbrCheckSum != 0) &&
                  (diskInfo->MbrCheckSum == diskData->MbrCheckSum)) {
            DebugPrint((1, "DiskUpdateGeometry: found match for xsum %#08lx\n",
                        diskData->MbrCheckSum));
            found = TRUE;
            break;
        }
    }

    if(found) {

        ULONG cylinders;
        ULONG sectorsPerTrack;
        ULONG tracksPerCylinder;

        ULONG sectors;
        ULONG length;

        //
        // Point to the array of drive parameters.
        //
        
        cylinders = diskInfo->DriveParameters.MaxCylinders + 1;
        sectorsPerTrack = diskInfo->DriveParameters.SectorsPerTrack;
        tracksPerCylinder = diskInfo->DriveParameters.MaxHeads + 1;

        //
        // Since the BIOS may not report the full drive, recalculate the drive
        // size based on the volume size and the BIOS values for tracks per
        // cylinder and sectors per track..
        //
    
        length = tracksPerCylinder * sectorsPerTrack;
    
        if (length == 0) {
    
            //
            // The BIOS information is bogus.
            //
    
            DebugPrint((1, "DiskUpdateGeometry: H (%d) or S(%d) is zero\n",
                        tracksPerCylinder, sectorsPerTrack));
            return FALSE;
        }

	//
        // since we are copying the structure RealGeometry here, we should
        // really initialize all the fields, especially since a zero'd
        // BytesPerSector field would cause a trap in xHalReadPartitionTable()
        //

        diskData->RealGeometry = FdoExtension->DiskGeometry;

        //
        // Save the geometry information away in the disk data block and 
        // set the bit indicating that we found a valid one.
        //
    
        diskData->RealGeometry.SectorsPerTrack = sectorsPerTrack;
        diskData->RealGeometry.TracksPerCylinder = tracksPerCylinder;
        diskData->RealGeometry.Cylinders.QuadPart = (LONGLONG)cylinders;
    
        DebugPrint((1, "DiskUpdateGeometry: BIOS spt %#x, #heads %#x, "
                       "#cylinders %#x\n",
                   sectorsPerTrack, tracksPerCylinder, cylinders));

        diskData->GeometrySource = DiskGeometryFromBios;
        diskInfo->Device = FdoExtension->DeviceObject;
    } else {
        DebugPrint((1, "DiskUpdateGeometry: no match found for signature %#08lx\n", diskData->Signature));
    }


    //
    // NEC-98 machines have their own geometry which we can calcuate.  If this
    // disk has the NEC-98 format on it and it's an NEC-98 machine then
    // derive the geometry rather than asking the port driver.
    //

    if((diskData->GeometrySource == DiskGeometryUnknown) &&
       (IsNEC_98 == TRUE) && 
       (DiskIsFormatMediaTypeNEC_98(FdoExtension) == TRUE)) {

        status = DiskGetNEC98Geometry(FdoExtension, &(diskData->RealGeometry));

        if(NT_SUCCESS(status)) {
            DebugPrint((1, "DiskUpdateGeometry: using NEC98 geometry for disk %#p\n", FdoExtension));
            diskData->GeometrySource = DiskGeometryFromNec98;
        }
    }

    if(diskData->GeometrySource == DiskGeometryUnknown) {

        //
        // We couldn't find a geometry from the BIOS.  Check with the port 
        // driver and see if it can provide one.
        //

        status = DiskGetPortGeometry(FdoExtension, &(diskData->RealGeometry));

        if(NT_SUCCESS(status)) {

            //
            // Check the geometry to make sure it's valid.
            //

            if((diskData->RealGeometry.TracksPerCylinder * 
                diskData->RealGeometry.SectorsPerTrack) != 0) {

                diskData->GeometrySource = DiskGeometryFromPort;
                DebugPrint((1, "DiskUpdateGeometry: using Port geometry for disk %#p\n", FdoExtension));
            }
        }
    }

    //
    // If we came up with a "real" geometry for this drive then set it in the 
    // device extension.
    //

    if(diskData->GeometrySource != DiskGeometryUnknown) {
        FdoExtension->DiskGeometry = diskData->RealGeometry;

        //
        // Increment the count of used geometry entries.
        //

        InterlockedIncrement(&DetectInfoUsedCount);
    }

    return diskData->GeometrySource;
}


NTSTATUS
DiskUpdateRemovableGeometry (
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )

/*++

Routine Description:

    This routine updates the geometry of the disk.  It will query the port 
    driver to see if it can provide any geometry info.  If not it will use 
    the current head & sector count.
    
    Based on these values & the capacity of the drive as reported by 
    ClassReadDriveCapacity it will determine a new cylinder count for the 
    device.

Arguments:

    Fdo - Supplies the functional device object whos size needs to be updated.

Return Value:

    Returns the status of the opertion.

--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = &(FdoExtension->CommonExtension);
    PDISK_DATA diskData = commonExtension->DriverData;
    PDISK_GEOMETRY geometry = &(diskData->RealGeometry);

    NTSTATUS status;

    PAGED_CODE();

    ASSERT_FDO(commonExtension->DeviceObject);
    ASSERT(FdoExtension->DeviceDescriptor->RemovableMedia);
    ASSERT(TEST_FLAG(FdoExtension->DeviceObject->Characteristics, 
                     FILE_REMOVABLE_MEDIA));

    //
    // Attempt to determine the disk geometry.  First we'll check with the 
    // port driver to see what it suggests for a value.
    //

    status = DiskGetPortGeometry(FdoExtension, geometry);

    if(NT_SUCCESS(status) && 
       ((geometry->TracksPerCylinder * geometry->SectorsPerTrack) != 0)) {
        FdoExtension->DiskGeometry = (*geometry);
    }

    return status;
}


NTSTATUS
DiskGetPortGeometry(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    OUT PDISK_GEOMETRY Geometry
    )
/*++

Routine Description:

    This routine will query the port driver for disk geometry.  Some port 
    drivers (in particular IDEPORT) may be able to provide geometry for the 
    device.
    
Arguments:

    FdoExtension - the device object for the disk.
    
    Geometry - a structure to save the geometry information into (if any is 
               available)
               
Return Value:
    
    STATUS_SUCCESS if geometry information can be provided or 
    error status indicating why it can't.
    
--*/
{
    PCOMMON_DEVICE_EXTENSION commonExtension = &(FdoExtension->CommonExtension);
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT event;

    NTSTATUS status;

    PAGED_CODE();

    //
    // Build an irp to send IOCTL_DISK_GET_DRIVE_GEOMETRY to the lower driver.
    //

    irp = IoAllocateIrp(commonExtension->LowerDeviceObject->StackSize, FALSE);

    if(irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_DEVICE_CONTROL;

    irpStack->Parameters.DeviceIoControl.IoControlCode = 
        IOCTL_DISK_GET_DRIVE_GEOMETRY;
    irpStack->Parameters.DeviceIoControl.OutputBufferLength = 
        sizeof(DISK_GEOMETRY);

    irp->AssociatedIrp.SystemBuffer = Geometry;

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    IoSetCompletionRoutine(irp, 
                           ClassSignalCompletion,
                           &event,
                           TRUE,
                           TRUE,
                           TRUE);

    status = IoCallDriver(commonExtension->LowerDeviceObject, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);

    ASSERT((status == STATUS_PENDING) || (status == irp->IoStatus.Status));
    status = irp->IoStatus.Status;

    IoFreeIrp(irp);

    return status;
}

NTSTATUS
DiskGetNEC98Geometry(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension,
    OUT PDISK_GEOMETRY DiskGeometry
    )
/*++

Routine Description:

    This routine will retrieve the geometry mode page from the device (if 
    supported) and will return a disk geometry structure which contains the 
    reported geometry.

    This routine should not be called on a removable disk, nor should it be 
    called on a non NEC98 system.

Arguments:

    FdoExtension - the disk to be interrogated

    DiskGeometry - a location to store the geometry info (provided by the 
                   caller)

Return Value:

    status

--*/
{
    PMODE_PARAMETER_HEADER modeData;
    ULONG length;

    PMODE_FORMAT_PAGE formatPage;
    PMODE_RIGID_GEOMETRY_PAGE geometryPage;

    ULONG bytesPerSector;
    ULONG sectorsPerTrack;
    ULONG tracksPerCylinder;
    ULONG numberOfCylinders;
    
    BOOLEAN useGeometryFromModeSense;
    PUCHAR pages;

    ULONG totalSectors;

    PAGED_CODE();

    ASSERT(IsNEC_98);
    ASSERT(!FdoExtension->DeviceDescriptor->RemovableMedia);
    ASSERT(!TEST_FLAG(FdoExtension->DeviceObject->Characteristics, 
                      FILE_REMOVABLE_MEDIA));

    modeData = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                     MODE_DATA_SIZE,
                                     DISK_TAG_MODE_DATA);

    if (modeData == NULL) {
        DebugPrint((1,"DiskGetModeSenseGeometry - couldn't allocate mode "
                      "sense buffer\n"));
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

            DebugPrint((1,"DiskGetModeSenseGeometry - length %d is short.\n",
                        length));
            ExFreePool(modeData);
            return STATUS_UNSUCCESSFUL;
        }
    }

    //
    // If the length is greater than length indicated by the mode data reset
    // the data to the mode data.
    //

    if (length > (ULONG) modeData->ModeDataLength + 1) {
        length = modeData->ModeDataLength + 1;
    }

    //
    // Get SectorsPerTrack value.
    //

    formatPage = (PMODE_FORMAT_PAGE) ClassFindModePage(
                                        (PUCHAR) modeData, 
                                        length, 
                                        MODE_PAGE_FORMAT_DEVICE, 
                                        TRUE);

    //
    // Check if valid MODE_PAGE_FORMAT_DEVICE page exists.
    //

    if (formatPage != NULL) {
        PFOUR_BYTE tmp = (PFOUR_BYTE) &sectorsPerTrack;

        sectorsPerTrack = 0;
        tmp->Byte0 = formatPage->SectorsPerTrack[1];
        tmp->Byte1 = formatPage->SectorsPerTrack[0];

        tmp = (PFOUR_BYTE) &bytesPerSector;
        bytesPerSector = 0;
        tmp->Byte0 = formatPage->BytesPerPhysicalSector[1];
        tmp->Byte1 = formatPage->BytesPerPhysicalSector[0];

    } else {

        DebugPrint((1,"DiskGetModeSenseGeometry - MODE_PAGE_FORMAT_DEVICE "
                      "is FALSE\n"));
        ExFreePool(modeData);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Get values of NumberOfCylinders and NumberOfHeads.
    //

    geometryPage = (PMODE_RIGID_GEOMETRY_PAGE) ClassFindModePage(
                                                (PUCHAR) modeData, 
                                                length, 
                                                MODE_PAGE_RIGID_GEOMETRY, 
                                                TRUE);

    //
    // Check if MODE_PAGE_RIGID_GEOMETRY page exists.
    //

    if (geometryPage != NULL) {

        PFOUR_BYTE tmp = (PFOUR_BYTE) &numberOfCylinders;

        //
        // Update NumberOfHeads
        //

        tracksPerCylinder = (ULONG) geometryPage->NumberOfHeads;

        //
        // Update NumberOfCylinders.
        //

        tmp->Byte0 = geometryPage->NumberOfCylinders[2];
        tmp->Byte1 = geometryPage->NumberOfCylinders[1];
        tmp->Byte2 = geometryPage->NumberOfCylinders[0];
        tmp->Byte3 = 0;

    } else {

        DebugPrint((1,"DiskGetModePageGeometry - MODE_PAGE_RIGID_GEOMETRY is "
                      "FALSE\n"));
        ExFreePool(modeData);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Check scsi type.
    //

    useGeometryFromModeSense = FALSE;

    pages = (PUCHAR) modeData;

    if (length > sizeof(MODE_PARAMETER_HEADER)) {

        pages += sizeof(MODE_PARAMETER_HEADER) +
            ((PMODE_PARAMETER_HEADER) pages)->BlockDescriptorLength;

        if ((((PMODE_DISCONNECT_PAGE) pages)->PageCode == MODE_PAGE_ERROR_RECOVERY) &&
            (((PMODE_DISCONNECT_PAGE) pages)->PageLength == 6 )){

            pages += ((PMODE_DISCONNECT_PAGE) pages)->PageLength + 2;

            if ((((PMODE_DISCONNECT_PAGE) pages)->PageCode == MODE_PAGE_FORMAT_DEVICE) &&
                (((PMODE_DISCONNECT_PAGE) pages)->PageLength == 22 )){

                pages += ((PMODE_DISCONNECT_PAGE) pages)->PageLength + 2;

                if ((((PMODE_DISCONNECT_PAGE) pages)->PageCode == MODE_PAGE_RIGID_GEOMETRY) &&
                    (((PMODE_DISCONNECT_PAGE) pages)->PageLength == 18 )){

                    //
                    // nec-scsi or ide device.
                    //

                    useGeometryFromModeSense = TRUE;

                }
            }
        }
    }

    numberOfCylinders --;

    if ( !useGeometryFromModeSense ) {

        //
        // We can't use the geometry from mode sense on standerd scsi device.
        // Because it's geometry is vendor specific,
        // and is different from one of BIOS on PC-9800 series.
        // So, we use the following geometry.
        // The determination logic is the same as PC-9800's BIOS.
        //

        DebugPrint((1,"DiskGetModeSenseGeometry - not use Geometry from mode sense.\n"));

        totalSectors = (ULONG)(FdoExtension->CommonExtension.PartitionLength.QuadPart >> FdoExtension->SectorShift);

        tracksPerCylinder = 0x08;

        if ( totalSectors < SECTORS_DISK_8GB ){
            sectorsPerTrack = 0x20;

        } else {
            sectorsPerTrack = 0x80;

        }

        numberOfCylinders = totalSectors / (tracksPerCylinder * sectorsPerTrack) - 1;
    }

    //
    // Set geometry
    //

    DiskGeometry->Cylinders.QuadPart   = numberOfCylinders;
    DiskGeometry->TracksPerCylinder    = tracksPerCylinder;
    DiskGeometry->SectorsPerTrack      = sectorsPerTrack;

    DiskGeometry->BytesPerSector = 0x200;

    ExFreePool(modeData);
    return STATUS_SUCCESS;
}


BOOLEAN
DiskIsFormatMediaTypeNEC_98(
    IN PFUNCTIONAL_DEVICE_EXTENSION FdoExtension
    )
/*++

Routine Description:

    Determine format media type, PC-9800 architecture or PC/AT architecture
    Logic of determination:
      [fixed] and [0x55AA exist] and [non "IPL1"]    - PC/AT architecture
      [fixed] and [0x55AA exist] and ["IPL1" exist]  - PC-9800 architecture
      [fixed] and [non 0x55AA  ]                     - PC-9800 architecture

    This routine should never be called for a removable disk.

Arguments:

    Fdo - Supplies the device object to be tested.

Return Value:

    Return format type
        TRUE   - The media was formated by PC-9800 architecture or it was not 
                 formated
        FALSE  - The media was formated by PC/AT architecture
--*/
{
    LARGE_INTEGER       sectorZero;
    PUCHAR              readBuffer  = NULL;
    KEVENT              event;
    IO_STATUS_BLOCK     ioStatus;
    PIRP                irp;
    NTSTATUS            status;
    ULONG               readSize;

    PAGED_CODE();

    ASSERT(!FdoExtension->DeviceDescriptor->RemovableMedia);
    ASSERT(!TEST_FLAG(FdoExtension->DeviceObject->Characteristics, 
                      FILE_REMOVABLE_MEDIA));

    //
    // Start at sector 0 of the device.
    //

    sectorZero.QuadPart = (LONGLONG) 0;
    readSize = FdoExtension->DiskGeometry.BytesPerSector;

    if (readSize < 512) {
        readSize = 512;
    }

    //
    // Allocate a buffer that will hold the reads.
    //

    readBuffer = ExAllocatePoolWithTag( NonPagedPoolCacheAligned,
                                        readSize,
                                        DISK_TAG_NEC_98 );

    if (readBuffer == NULL) {

        //
        // Assume NEC98 architecture.
        //

        return TRUE;
    }

    RtlZeroMemory(readBuffer, readSize);

    //
    // Read record containing partition table.
    //
    // Create a notification event object to be used while waiting for
    // the read request to complete.
    //

    KeInitializeEvent( &event, NotificationEvent, FALSE );

    irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                        FdoExtension->DeviceObject,
                                        readBuffer,
                                        readSize,
                                        &sectorZero,
                                        &event,
                                        &ioStatus );

    status = IoCallDriver( FdoExtension->DeviceObject, irp );

    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS( status )) {

        //
        // If SectorsPerTrack and TracksPerCylinder has not been determined,
        // then this irp should be failed.
        // Assume it's NEC98 format disk.
        //

        ExFreePool(readBuffer);
        return TRUE; 
    }

    //
    // Check for Boot Record signature.
    //

    if (((PUSHORT) readBuffer)[BOOT_SIGNATURE_OFFSET] == BOOT_RECORD_SIGNATURE) {

        if (!strncmp( readBuffer+IPL1_OFFSET, "IPL1", sizeof("IPL1")-1 )){

            //
            // It's PC-9800 Architecture.
            //

            ExFreePool(readBuffer);
            return TRUE; 
        }
    } else {
        //
        // It's PC-9800 Architecture.
        //

        ExFreePool(readBuffer);
        return TRUE; 
    }
    ExFreePool(readBuffer);
    return FALSE; 
}


NTSTATUS
DiskReadDriveCapacity(
    IN PDEVICE_OBJECT Fdo
    )
/*++

Routine Description:

    This routine is used by disk.sys as a wrapper for the classpnp API 
    ClassReadDriveCapacity.  It will perform some additional operations to 
    attempt to determine drive geometry before it calls the classpnp version 
    of the routine.
    
    For fixed disks this involves calling DiskUpdateGeometry which will check 
    various sources (the BIOS, the port driver) for geometry information.
    
    For removable disks this will involve calling DiskGetPortGeometry to 
    get geometry information from the lower drivers or using the geometry 
    mode page to determine the geometry of the media (NEC98 only).

Arguments:

    Fdo - a pointer to the device object to be checked.    

Return Value:

    status of ClassReadDriveCapacity.
    
--*/        

{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PDISK_DATA diskData = fdoExtension->CommonExtension.DriverData;
    DISK_GEOMETRY_SOURCE diskGeometrySource = DiskGeometryUnknown;
    NTSTATUS status;

    ASSERT_FDO(Fdo);

    if(fdoExtension->DeviceDescriptor->RemovableMedia) {
        DiskUpdateRemovableGeometry(fdoExtension);
    } else {
        diskGeometrySource = DiskUpdateGeometry(fdoExtension);
    }

    status = ClassReadDriveCapacity(Fdo);

    if (IsNEC_98) {

        //
        // We don't use the last cylinder of NEC98 format disk on any OS.
        // If we found the correct geometry in DiskUpdateGeometry(), then use it.
        //

        if ((diskGeometrySource == DiskGeometryFromBios ) ||
            (diskGeometrySource == DiskGeometryFromNec98)) {

            fdoExtension->DiskGeometry.Cylinders.QuadPart
                    = diskData->RealGeometry.Cylinders.QuadPart;
        }
    }

    return status;
}


VOID
DiskDriverReinitialization(
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID Nothing,
    IN ULONG Count
    )
/*++

Routine Description:

    This routine will scan through the current list of disks and attempt to 
    match them to any remaining geometry information.  This will only be done 
    on the first call to the routine.

    Note: This routine assumes that the system will not be adding or removing
          devices during this phase of the init process.  This is very likely
          a bad assumption but it greatly simplifies the code.

Arguments:

    DriverObject - a pointer to the object for the disk driver.
    
    Nothing - unused
    
    Count - an indication of how many times this routine has been called.
    
Return Value:

    none

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
    PDISK_DATA diskData;

    ULONG unmatchedDiskCount;
    PDEVICE_OBJECT unmatchedDisk = NULL;

    ULONG i;
    PDISK_DETECT_INFO diskInfo = NULL;

    if(Count != 1) {
        DebugPrint((1, "DiskDriverReinitialization: ignoring call %d\n", 
                    Count));
        return;
    }

    //
    // Check to see how many entries in the detect info list have been matched.
    // If there's only one remaining we'll see if we can find a disk to go with
    // it.
    //

    if(DetectInfoCount == 0) {
        DebugPrint((1, "DiskDriverReinitialization: no detect info saved\n"));
        return;
    }

    if((DetectInfoCount - DetectInfoUsedCount) != 1) {
        DebugPrint((1, "DiskDriverReinitialization: %d of %d geometry entries "
                       "used - will not attempt match\n"));
        return;
    }

    //
    // Scan through the list of disks and see if any of them are missing
    // geometry information.  If there is only one such disk we'll try to 
    // match it to the unmatched geometry.
    //
    // BUGBUG - figure out if there's a way to keep removals from happening
    //          while doing this.
    //

    for(deviceObject = DriverObject->DeviceObject, unmatchedDiskCount = 0;
        deviceObject != NULL;
        deviceObject = deviceObject->NextDevice) {

        //
        // Make sure this is a disk and not a partition.
        //

        fdoExtension = deviceObject->DeviceExtension;
        if(fdoExtension->CommonExtension.IsFdo == FALSE) {
            DebugPrint((1, "DiskDriverReinit: DO %#p is not an FDO\n", 
                           deviceObject));
            continue;
        }

        //
        // If the geometry for this one is already known then skip it.
        //

        diskData = fdoExtension->CommonExtension.DriverData;
        if(diskData->GeometrySource != DiskGeometryUnknown) {
            DebugPrint((1, "DiskDriverReinit: FDO %#p has a geometry\n", 
                           deviceObject));
            continue;
        }

        DebugPrint((1, "DiskDriverReinit: FDO %#p has no geometry\n",
                       deviceObject));

        //
        // Mark this one as using the default.  It's past the time when disk
        // might blunder across the geometry info.  If we set the geometry 
        // from the bios we'll reset this field down below.
        //

        diskData->GeometrySource = DiskGeometryFromDefault;

        //
        // As long as we've only got one unmatched disk we're fine.
        //

        unmatchedDiskCount++;
        if(unmatchedDiskCount > 1) {
            ASSERT(unmatchedDisk != NULL);
            DebugPrint((1, "DiskDriverReinit: FDO %#p also has no geometry\n",
                           unmatchedDisk));
            unmatchedDisk = NULL;
            break;
        }

        unmatchedDisk = deviceObject;
    }

    //
    // If there's more or less than one ungeometried disk then we can't do 
    // anything about the geometry.
    //

    if(unmatchedDiskCount != 1) {
        DebugPrint((1, "DiskDriverReinit: Unable to match geometry\n"));
        return;

    }

    fdoExtension = unmatchedDisk->DeviceExtension;
    diskData = fdoExtension->CommonExtension.DriverData;

    DebugPrint((1, "DiskDriverReinit: Found possible match\n"));

    //
    // Find the geometry which wasn't assigned.
    //

    for(i = 0; i < DetectInfoCount; i++) {
        if(DetectInfoList[i].Device == NULL) {
            diskInfo = &(DetectInfoList[i]);
            break;
        }
    }

    ASSERT(diskInfo != NULL);

    {
        //
        // Save the geometry information away in the disk data block and 
        // set the bit indicating that we found a valid one.
        //

        ULONG cylinders;
        ULONG sectorsPerTrack;
        ULONG tracksPerCylinder;

        ULONG sectors;
        ULONG length;

        //
        // Point to the array of drive parameters.
        //
        
        cylinders = diskInfo->DriveParameters.MaxCylinders + 1;
        sectorsPerTrack = diskInfo->DriveParameters.SectorsPerTrack;
        tracksPerCylinder = diskInfo->DriveParameters.MaxHeads + 1;

        //
        // Since the BIOS may not report the full drive, recalculate the drive
        // size based on the volume size and the BIOS values for tracks per
        // cylinder and sectors per track..
        //
    
        length = tracksPerCylinder * sectorsPerTrack;
    
        if (length == 0) {
    
            //
            // The BIOS information is bogus.
            //
    
            DebugPrint((1, "DiskDriverReinit: H (%d) or S(%d) is zero\n",
                        tracksPerCylinder, sectorsPerTrack));
            return;
        }

	//
        // since we are copying the structure RealGeometry here, we should
        // really initialize all the fields, especially since a zero'd
        // BytesPerSector field would cause a trap in xHalReadPartitionTable()
        //
        
	diskData->RealGeometry = fdoExtension->DiskGeometry;

        //
        // Save the geometry information away in the disk data block and 
        // set the bit indicating that we found a valid one.
        //
    
        diskData->RealGeometry.SectorsPerTrack = sectorsPerTrack;
        diskData->RealGeometry.TracksPerCylinder = tracksPerCylinder;
        diskData->RealGeometry.Cylinders.QuadPart = (LONGLONG)cylinders;
    
        DebugPrint((1, "DiskDriverReinit: BIOS spt %#x, #heads %#x, "
                       "#cylinders %#x\n",
                   sectorsPerTrack, tracksPerCylinder, cylinders));

        diskData->GeometrySource = DiskGeometryGuessedFromBios;
        diskInfo->Device = unmatchedDisk;

        //
        // Now copy the geometry over to the fdo extension and call 
        // classpnp to redetermine the disk size and cylinder count.
        //

        fdoExtension->DiskGeometry = diskData->RealGeometry;
        ClassReadDriveCapacity(unmatchedDisk);
    }

    return;
}

#endif

