    /*++
    
    Copyright (C) Microsoft Corporation, 1991 - 1999
    
    Module Name:
    
        floppy.c
    
    Abstract:
    
        This is the NEC PD756 (aka AT, aka ISA, aka ix86) and Intel 82077
        (aka MIPS) floppy diskette driver for NT.
    
    Environment:
    
        Kernel mode only.
    
    --*/
    
    //
    // Include files.
    //

    #include "stdio.h"
    #include "ntddk.h"                       // various NT definitions
    #include "ntdddisk.h"                    // disk device driver I/O control codes
    #include "ntddfdc.h"                     // fdc I/O control codes and parameters
    #include <flo_data.h>                    // this driver's data declarations
    
    
    //
    // This is the actual definition of FloppyDebugLevel.
    // Note that it is only defined if this is a "debug"
    // build.
    //
    #if DBG
    extern ULONG FloppyDebugLevel = 0;
    #endif
    
#ifdef TOSHIBAJ
    ULONG   NotConfigurable = 0;
#endif
    
    #ifdef ALLOC_PRAGMA
    #pragma alloc_text(INIT,DriverEntry)
    
    #pragma alloc_text(PAGE,FloppyAddDevice)
    #pragma alloc_text(PAGE,FloppyPnp)
    #pragma alloc_text(PAGE,FloppyPower)
    #pragma alloc_text(PAGE,FlConfigCallBack)
    #pragma alloc_text(PAGE,FlInitializeControllerHardware)
    #pragma alloc_text(PAGE,FlInterpretError)
    #pragma alloc_text(PAGE,FlDatarateSpecifyConfigure)
    #pragma alloc_text(PAGE,FlRecalibrateDrive)
    #pragma alloc_text(PAGE,FlDetermineMediaType)
    #pragma alloc_text(PAGE,FlCheckBootSector)
    #pragma alloc_text(PAGE,FlConsolidateMediaTypeWithBootSector)
    #pragma alloc_text(PAGE,FlIssueCommand)
    #pragma alloc_text(PAGE,FlReadWriteTrack)
    #pragma alloc_text(PAGE,FlReadWrite)
    #pragma alloc_text(PAGE,FlFormat)
    #pragma alloc_text(PAGE,FlFinishOperation)
    #pragma alloc_text(PAGE,FlStartDrive)
    #pragma alloc_text(PAGE,FloppyThread)
    #pragma alloc_text(PAGE,FlAllocateIoBuffer)
    #pragma alloc_text(PAGE,FlFreeIoBuffer)
    #pragma alloc_text(PAGE,FloppyCreateClose)
    #pragma alloc_text(PAGE,FloppyDeviceControl)
    #pragma alloc_text(PAGE,FloppyReadWrite)
    #pragma alloc_text(PAGE,FlCheckFormatParameters)
#ifdef TOSHIBA
    #pragma alloc_text(PAGE,FlFdcDeviceIoPhys)
#endif
    #pragma alloc_text(PAGE,FlFdcDeviceIo)
    #pragma alloc_text(PAGE,FlHdbit)
    #endif
    
    #ifdef POOL_TAGGING
    #ifdef ExAllocatePool
    #undef ExAllocatePool
    #endif
    #define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'polF')
    #endif
    
    // #define KEEP_COUNTERS 1
    
    #ifdef KEEP_COUNTERS
    ULONG FloppyUsedSeek   = 0;
    ULONG FloppyNoSeek     = 0;
    #endif
    
    //
    // Used for paging the driver.
    //
    
    ULONG PagingReferenceCount = 0;
    PFAST_MUTEX PagingMutex = NULL;
    
    
    NTSTATUS
    DriverEntry(
        IN PDRIVER_OBJECT DriverObject,
        IN PUNICODE_STRING RegistryPath
        )
    
    /*++
    
    Routine Description:
    
        This routine is the driver's entry point, called by the I/O system
        to load the driver.  This routine can be called any number of times,
        as long as the IO system and the configuration manager conspire to
        give it an unmanaged controller to support at each call.  It could
        also be called a single time and given all of the controllers at
        once.
    
        It initializes the passed-in driver object, calls the configuration
        manager to learn about the devices that it is to support, and for
        each controller to be supported it calls a routine to initialize the
        controller (and all drives attached to it).
    
    Arguments:
    
        DriverObject - a pointer to the object that represents this device
        driver.
    
    Return Value:
    
        If we successfully initialize at least one drive, STATUS_SUCCESS is
        returned.
    
        If we don't (because the configuration manager returns an error, or
        the configuration manager says that there are no controllers or
        drives to support, or no controllers or drives can be successfully
        initialized), then the last error encountered is propogated.
    
    --*/
    
    {
        NTSTATUS ntStatus = STATUS_SUCCESS;
        UCHAR disketteNumber;
        BOOLEAN partlySuccessful = FALSE;  // TRUE if any controller init'd properly
    
        //
        // We use this to query into the registry as to whether we
        // should break at driver entry.
        //
        RTL_QUERY_REGISTRY_TABLE paramTable[3];
        ULONG zero = 0;
        ULONG one = 1;
        ULONG debugLevel = 0;
        ULONG shouldBreak = 0;
        ULONG notConfigurable = 0;
        PWCHAR path;
        UNICODE_STRING parameters;
        UNICODE_STRING systemPath;
        UNICODE_STRING identifier;
        UNICODE_STRING thinkpad, ps2e;
        ULONG pathLength;
    
    
        RtlInitUnicodeString(&parameters, L"\\Parameters");
        RtlInitUnicodeString(&systemPath,
            L"\\REGISTRY\\MACHINE\\HARDWARE\\DESCRIPTION\\System");
    
        pathLength = RegistryPath->Length + parameters.Length + sizeof(WCHAR);
        if (pathLength < systemPath.Length + sizeof(WCHAR)) {
            pathLength = systemPath.Length + sizeof(WCHAR);
        }
    
        //
        // Since the registry path parameter is a "counted" UNICODE string, it
        // might not be zero terminated.  For a very short time allocate memory
        // to hold the registry path zero terminated so that we can use it to
        // delve into the registry.
        //
        // NOTE NOTE!!!! This is not an architected way of breaking into
        // a driver.  It happens to work for this driver because the author
        // likes to do things this way.
        //
    
        if (path = ExAllocatePool(PagedPool, pathLength)) {
    
            RtlZeroMemory(&paramTable[0],
                          sizeof(paramTable));
            RtlZeroMemory(path, pathLength);
            RtlMoveMemory(path,
                          RegistryPath->Buffer,
                          RegistryPath->Length);
    
            paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
            paramTable[0].Name = L"BreakOnEntry";
            paramTable[0].EntryContext = &shouldBreak;
            paramTable[0].DefaultType = REG_DWORD;
            paramTable[0].DefaultData = &zero;
            paramTable[0].DefaultLength = sizeof(ULONG);
            paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
            paramTable[1].Name = L"DebugLevel";
            paramTable[1].EntryContext = &debugLevel;
            paramTable[1].DefaultType = REG_DWORD;
            paramTable[1].DefaultData = &zero;
            paramTable[1].DefaultLength = sizeof(ULONG);
    
    
            if (!NT_SUCCESS(RtlQueryRegistryValues(
                                RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                path,
                                &paramTable[0],
                                NULL,
                                NULL))) {
    
                shouldBreak = 0;
                debugLevel = 0;
    
            }
    
#ifdef TOSHIBAJ
            // If the controller does not support CONFIGURE command
            // and it returns no error for CONFIGURE command,
            // we tell to the driver using an value in registry
            // that CONFIGURE command is not supported.
    
            RtlZeroMemory(&paramTable[0],
                          sizeof(paramTable));
            RtlZeroMemory(path,
                          RegistryPath->Length + parameters.Length + sizeof(WCHAR));
            RtlMoveMemory(path,
                          RegistryPath->Buffer,
                          RegistryPath->Length);
            RtlMoveMemory((PCHAR) path + RegistryPath->Length,
                          parameters.Buffer,
                          parameters.Length);
            paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
            paramTable[0].Name = L"NotConfigurable";
            paramTable[0].EntryContext = &NotConfigurable;
            paramTable[0].DefaultType = REG_DWORD;
            paramTable[0].DefaultData = &zero;
            paramTable[0].DefaultLength = sizeof(ULONG);
    
            if (!NT_SUCCESS(RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                                   path,
                                                   &paramTable[0],
                                                   NULL,
                                                   NULL))) {
    
                NotConfigurable = 0;
            }
#endif
        }
    
        //
        // We don't need that path anymore.
        //
    
        if (path) {
    
            ExFreePool(path);
    
        }
    
    #if DBG
        FloppyDebugLevel = debugLevel;
    #endif
    
        if (shouldBreak) {
    
            DbgBreakPoint();
    
        }
    
        FloppyDump(FLOPSHOW,
                   ("Floppy: DriverEntry...\n"));
    
        //
        // Initialize the driver object with this driver's entry points.
        //
    
        DriverObject->MajorFunction[IRP_MJ_CREATE] =
            FloppyCreateClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] =
            FloppyCreateClose;
        DriverObject->MajorFunction[IRP_MJ_READ] = FloppyReadWrite;
        DriverObject->MajorFunction[IRP_MJ_WRITE] = FloppyReadWrite;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
            FloppyDeviceControl;
        DriverObject->MajorFunction[IRP_MJ_PNP] = FloppyPnp;
        DriverObject->MajorFunction[IRP_MJ_POWER] = FloppyPower;
    
        DriverObject->DriverExtension->AddDevice = FloppyAddDevice;
    
        PagingMutex = ExAllocatePool(NonPagedPool, sizeof(FAST_MUTEX));
        if (!PagingMutex) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        ExInitializeFastMutex(PagingMutex);
    
        MmPageEntireDriver(DriverEntry);
    
        DriveMediaLimits =
            (IsNEC_98 ? (PDRIVE_MEDIA_LIMITS)&_DriveMediaLimits_NEC98[0] : &_DriveMediaLimits[0]);
    
        DriveMediaConstants =
            (IsNEC_98 ? (PDRIVE_MEDIA_CONSTANTS)&_DriveMediaConstants_NEC98[0] : &_DriveMediaConstants[0]);
    
        return ntStatus;
    }
    
    NTSTATUS
    FloppyAddDevice(
        IN      PDRIVER_OBJECT DriverObject,
        IN OUT  PDEVICE_OBJECT PhysicalDeviceObject
        )
    /*++
    
    Routine Description:
    
        This routine is the driver's pnp add device entry point.  It is
        called by the pnp manager to initialize the driver.
    
        Add device creates and initializes a device object for this FDO and 
        attaches to the underlying PDO.
    
    Arguments:
    
        DriverObject - a pointer to the object that represents this device
        driver.
        PhysicalDeviceObject - a pointer to the underlying PDO to which this
        new device will attach.
    
    Return Value:
    
        If we successfully create a device object, STATUS_SUCCESS is
        returned.  Otherwise, return the appropriate error code.
    
    --*/
    
    {
        NTSTATUS            ntStatus;
        PDEVICE_OBJECT      deviceObject;
        PDISKETTE_EXTENSION disketteExtension;
        FDC_INFO            fdcInfo;
        UNICODE_STRING      deviceName;
        UCHAR               arcNameBuffer[256];
        STRING              arcNameString;
        UNICODE_STRING      arcUnicodeString;
        USHORT              instance = 0;
    
    
        ntStatus = STATUS_SUCCESS;
    
        FloppyDump( FLOPSHOW, ("FloppyAddDevice:  CreateDeviceObject\n"));
    
        //
        //  Get some device information from the underlying PDO.
        //
        fdcInfo.BufferCount = 0;
        fdcInfo.BufferSize = 0;
    
#ifdef TOSHIBA
        ntStatus = FlFdcDeviceIoPhys( PhysicalDeviceObject,
                                      IOCTL_DISK_INTERNAL_GET_FDC_INFO,
                                      &fdcInfo );
#else
        ntStatus = FlFdcDeviceIo( PhysicalDeviceObject,
                                  IOCTL_DISK_INTERNAL_GET_FDC_INFO,
                                  &fdcInfo );
#endif
    
        if ( NT_SUCCESS(ntStatus) ) {
            WCHAR   deviceNameBuffer[32];

            do {
                //
                // Register the next possible floppy
                //
                swprintf(deviceNameBuffer, L"\\Device\\Floppy%d", instance++);
                RtlInitUnicodeString(&deviceName, deviceNameBuffer);

                ntStatus = IoCreateDevice( DriverObject,
                                           sizeof( DISKETTE_EXTENSION ),
                                           &deviceName,
                                           FILE_DEVICE_DISK,
                                           FILE_REMOVABLE_MEDIA | FILE_FLOPPY_DISKETTE,
                                           FALSE,
                                           &deviceObject );

            } while ( ntStatus == STATUS_OBJECT_NAME_COLLISION );
        }

#ifdef TOSHIBAJ
        if( ntStatus != STATUS_SUCCESS )
        {
            FloppyDump( FLOPSHOW, ("FloppyAddDevice: IoCreateDevice(\\Device\\FloppyFDO-%d) FAIL with Status=%08x\n",
                        fdcInfo.PeripheralNumber, ntStatus)
                        );
        }
#endif
    
        if (NT_SUCCESS(ntStatus)) {
    
            //
            // Create a symbolic link from the disk name to the corresponding
            // ARC name, to be used if we're booting off the disk.  This will
            // if it's not system initialization time; that's fine.  The ARC
            // name looks something like \ArcName\multi(0)disk(0)rdisk(0).
            //
            sprintf( arcNameBuffer,
                     "%s(%d)disk(%d)fdisk(%d)",
                     "\\ArcName\\multi",
                     fdcInfo.BusNumber,
                     fdcInfo.PeripheralNumber,
                     fdcInfo.PeripheralNumber );
    
            RtlInitString( &arcNameString, arcNameBuffer );
    
            ntStatus = RtlAnsiStringToUnicodeString( &arcUnicodeString,
                                                     &arcNameString,
                                                     TRUE );
    
            if ( NT_SUCCESS( ntStatus ) ) {
    
                IoAssignArcName( &arcUnicodeString, &deviceName );
                RtlFreeUnicodeString( &arcUnicodeString );
            }
    
#ifdef TOSHIBA
    /*
     * Create the drive letter based on InstanceNumber
	 * Libretto FDC always return the max-xfer size as 32K bytes
     */
            if ( NT_SUCCESS( ntStatus ) 
             && (fdcInfo.MaxTransferSize == 32*1024) )
            {   UCHAR   driveLetter;

                driveLetter = 'A' + instance;
    
                do
                {
                    sprintf( arcNameBuffer, "\\DosDevices\\%c:", driveLetter );
                    RtlInitString( &arcNameString, arcNameBuffer );
                    ntStatus = RtlAnsiStringToUnicodeString( &arcUnicodeString,
                                                             &arcNameString,
                                                             TRUE );
                    ntStatus = IoCreateSymbolicLink( &arcUnicodeString, &deviceName );
                    RtlFreeUnicodeString( &arcUnicodeString );
                    driveLetter++;
                }while( !NT_SUCCESS( ntStatus ) );
                        
                disketteExtension = (PDISKETTE_EXTENSION)deviceObject->DeviceExtension;
                disketteExtension->DriveAssignedLetter = driveLetter-1;
            }
#endif
        
            disketteExtension = (PDISKETTE_EXTENSION)deviceObject->DeviceExtension;
    
            deviceObject->Flags |= DO_DIRECT_IO | DO_POWER_PAGABLE;
    
            if ( deviceObject->AlignmentRequirement < FILE_WORD_ALIGNMENT ) {
    
                deviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
            }
    
            deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
            disketteExtension->DriverObject = DriverObject;
    
            // Set the PDO for use with PlugPlay functions
            disketteExtension->UnderlyingPDO = PhysicalDeviceObject;
    
            FloppyDump( FLOPSHOW, ("FloppyAddDevice: Attaching %p to %p\n", deviceObject, PhysicalDeviceObject));
            disketteExtension->TargetObject = IoAttachDeviceToDeviceStack( deviceObject,
                                                                    PhysicalDeviceObject );
    
            FloppyDump( FLOPSHOW, ("FloppyAddDevice: TargetObject = %p\n",disketteExtension->TargetObject) );
    
            KeInitializeSemaphore(
                &disketteExtension->RequestSemaphore,
                0L,
                MAXLONG );
    
            KeInitializeSpinLock( &disketteExtension->ListSpinLock );
    
            ExInitializeFastMutex(
                &disketteExtension->ThreadReferenceMutex );
    
            InitializeListHead( &disketteExtension->ListEntry );
    
            disketteExtension->ThreadReferenceCount = -1;
    
            disketteExtension->FloppyControllerAllocated = FALSE;
            disketteExtension->DeviceObject = deviceObject;
    
            disketteExtension->IsReadOnly = FALSE;
    
            disketteExtension->MediaType = Undetermined;
#ifdef TOSHIBA
            disketteExtension->Removed = FALSE;
#endif

#ifdef TOSHIBAJ
            // for 3 mode support
            if( FlFdcDeviceIo ( PhysicalDeviceObject,
                        IOCTL_DISK_INTERNAL_AVAILABLE_3_MODE,
                        NULL) == STATUS_SUCCESS) {
                disketteExtension->Available3Mode = TRUE;
            } else {
                disketteExtension->Available3Mode = FALSE;
            }
    
            // No CONFIGURE command
            disketteExtension->ControllerConfigurable = NotConfigurable ? FALSE : TRUE;
#else
            disketteExtension->ControllerConfigurable = (IsNEC_98) ? FALSE : TRUE;
#endif
        }
    
        return ntStatus;
    }
    
    NTSTATUS
    FlConfigCallBack(
        IN PVOID Context,
        IN PUNICODE_STRING PathName,
        IN INTERFACE_TYPE BusType,
        IN ULONG BusNumber,
        IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
        IN CONFIGURATION_TYPE ControllerType,
        IN ULONG ControllerNumber,
        IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
        IN CONFIGURATION_TYPE PeripheralType,
        IN ULONG PeripheralNumber,
        IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
        )
    
    /*++
    
    Routine Description:
    
        This routine is used to acquire all of the configuration
        information for each floppy disk controller and the
        peripheral driver attached to that controller.
    
    Arguments:
    
        Context - Pointer to the confuration information we are building
                  up.
    
        PathName - unicode registry path.  Not Used.
    
        BusType - Internal, Isa, ...
    
        BusNumber - Which bus if we are on a multibus system.
    
        BusInformation - Configuration information about the bus. Not Used.
    
        ControllerType - Should always be DiskController.
    
        ControllerNumber - Which controller if there is more than one
                           controller in the system.
    
        ControllerInformation - Array of pointers to the three pieces of
                                registry information.
    
        PeripheralType - Should always be FloppyDiskPeripheral.
    
        PeripheralNumber - Which floppy if this controller is maintaining
                           more than one.
    
        PeripheralInformation - Arrya of pointers to the three pieces of
                                registry information.
    
    Return Value:
    
        STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
        if it couldn't map the base csr or acquire the adapter object, or
        all of the resource information couldn't be acquired.
    
    --*/
    
    {
    
        //
        // So we don't have to typecast the context.
        //
        PDISKETTE_EXTENSION disketteExtension = Context;
    
        //
        // Simple iteration variable.
        //
        ULONG i;
    
        //
        // This boolean will be used to denote whether we've seen this
        // diskette before.
        //
        BOOLEAN newDiskette;
    
        //
        // This will be used to denote whether we even have room
        // for a new controller.
        //
        BOOLEAN outOfRoom;
    
        //
        // Iteration variable that will end up indexing to where
        // the device information should be placed.
        //
        ULONG DeviceSlot;
    
        PCM_FULL_RESOURCE_DESCRIPTOR peripheralData;
    
        NTSTATUS ntStatus;
    
        ASSERT(ControllerType == DiskController);
        ASSERT(PeripheralType == FloppyDiskPeripheral);
    
        //
        // Check if the infprmation from the registry for this device
        // is valid.
        //
    
        if (!(((PUCHAR)PeripheralInformation[IoQueryDeviceConfigurationData]) +
            PeripheralInformation[IoQueryDeviceConfigurationData]->DataLength)) {
    
            ASSERT(FALSE);
            return STATUS_INVALID_PARAMETER;
    
        }
    
        peripheralData = (PCM_FULL_RESOURCE_DESCRIPTOR)
            (((PUCHAR)PeripheralInformation[IoQueryDeviceConfigurationData]) +
            PeripheralInformation[IoQueryDeviceConfigurationData]->DataOffset);
    
        //
        // With Version 2.0 or greater for this resource list, we will get
        // the full int13 information for the drive. So get that if available.
        //
        // Otherwise, the only thing that we want out of the peripheral information
        // is the maximum drive capacity.
        //
        // Drop any information on the floor other than the
        // device specfic floppy information.
        //
    
        for ( i = 0; i < peripheralData->PartialResourceList.Count; i++ ) {
    
            PCM_PARTIAL_RESOURCE_DESCRIPTOR partial =
                &peripheralData->PartialResourceList.PartialDescriptors[i];
    
            if ( partial->Type == CmResourceTypeDeviceSpecific ) {
    
                //
                // Point to right after this partial.  This will take
                // us to the beginning of the "real" device specific.
                //
    
                PCM_FLOPPY_DEVICE_DATA fDeviceData;
                UCHAR driveType;
                PDRIVE_MEDIA_CONSTANTS biosDriveMediaConstants =
                    &(disketteExtension->BiosDriveMediaConstants);
    
    
                fDeviceData = (PCM_FLOPPY_DEVICE_DATA)(partial + 1);
    
                //
                // Get the driver density
                //
    
                switch ( fDeviceData->MaxDensity ) {
    
                    case 360:   driveType = DRIVE_TYPE_0360;    break;
                    case 1200:  driveType = DRIVE_TYPE_1200;    break;
                    case 1185:  driveType = DRIVE_TYPE_1200;    break;
                    case 1423:  driveType = DRIVE_TYPE_1440;    break;
                    case 1440:  driveType = DRIVE_TYPE_1440;    break;
                    case 2880:  driveType = DRIVE_TYPE_2880;    break;
                    case 1201:  if (IsNEC_98) {
                                    driveType = DRIVE_TYPE_1200_E;  break;
                                } // (IsNEC_98)
    
                    default:
    
                        FloppyDump(
                            FLOPDBGP,
                            ("Floppy: Bad DriveCapacity!\n"
                            "------  density is %d\n",
                            fDeviceData->MaxDensity)
                            );
    
                        driveType = DRIVE_TYPE_1200;
    
                        FloppyDump(
                            FLOPDBGP,
                            ("Floppy: run a setup program to set the floppy\n"
                            "------  drive type; assuming 1.2mb\n"
                            "------  (type is %x)\n",fDeviceData->MaxDensity)
                            );
    
                        break;
    
                }
    
                disketteExtension->DriveType = driveType;
    
                //
                // Pick up all the default from our own table and override
                // with the BIOS information
                //
    
                *biosDriveMediaConstants = DriveMediaConstants[
                    DriveMediaLimits[driveType].HighestDriveMediaType];
    
                //
                // If the version is high enough, get the rest of the
                // information.  DeviceSpecific information with a version >= 2
                // should have this information
                //
    
                if ( fDeviceData->Version >= 2 ) {
    
    
                    // biosDriveMediaConstants->MediaType =
    
                    biosDriveMediaConstants->StepRateHeadUnloadTime =
                        fDeviceData->StepRateHeadUnloadTime;
    
                    biosDriveMediaConstants->HeadLoadTime =
                        fDeviceData->HeadLoadTime;
    
                    biosDriveMediaConstants->MotorOffTime =
                        fDeviceData->MotorOffTime;
    
                    biosDriveMediaConstants->SectorLengthCode =
                        fDeviceData->SectorLengthCode;
    
                    // biosDriveMediaConstants->BytesPerSector =
    
                    if (fDeviceData->SectorPerTrack == 0) {
                        // This is not a valid sector per track value.
                        // We don't recognize this drive.  This bogus
                        // value is often returned by SCSI floppies.
                        return STATUS_SUCCESS;
                    }
    
                    if (fDeviceData->MaxDensity == 0 ) {
                        //
                        // This values are returned by the LS-120 atapi drive.
                        // BIOS function 8, in int 13 is returned in bl, which
                        // is mapped to this field. The LS-120 returns 0x10
                        // which is mapped to 0.  Thats why we wont pick it up
                        // as a normal floppy.
                        //
                        return STATUS_SUCCESS;
                    }
    
    
                    biosDriveMediaConstants->SectorsPerTrack =
                        fDeviceData->SectorPerTrack;
    
                    biosDriveMediaConstants->ReadWriteGapLength =
                        fDeviceData->ReadWriteGapLength;
    
                    biosDriveMediaConstants->FormatGapLength =
                        fDeviceData->FormatGapLength;
    
                    biosDriveMediaConstants->FormatFillCharacter =
                        fDeviceData->FormatFillCharacter;
    
                    biosDriveMediaConstants->HeadSettleTime =
                        fDeviceData->HeadSettleTime;
    
                    biosDriveMediaConstants->MotorSettleTimeRead =
                        fDeviceData->MotorSettleTime * 1000 / 8;
    
                    biosDriveMediaConstants->MotorSettleTimeWrite =
                        fDeviceData->MotorSettleTime * 1000 / 8;
    
                    if (fDeviceData->MaximumTrackValue == 0) {
                        // This is not a valid maximum track value.
                        // We don't recognize this drive.  This bogus
                        // value is often returned by SCSI floppies.
                        return STATUS_SUCCESS;
                    }
    
                    biosDriveMediaConstants->MaximumTrack =
                        fDeviceData->MaximumTrackValue;
    
                    biosDriveMediaConstants->DataLength =
                        fDeviceData->DataTransferLength;
                }
            }
        }
    
        return STATUS_SUCCESS;
    }
    
    NTSTATUS
    FlQueueIrpToThread(
        IN OUT  PIRP                Irp,
        IN OUT  PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine queues the given irp to be serviced by the controller's
        thread.  If the thread is down then this routine creates the thread.
    
    Arguments:
    
        Irp             - Supplies the IRP to queue to the controller's thread.
    
        ControllerData  - Supplies the controller data.
    
    Return Value:
    
        May return an error if PsCreateSystemThread fails.
        Otherwise returns STATUS_PENDING and marks the IRP pending.
    
    --*/
    
    {
        KIRQL       oldIrql;
        NTSTATUS    status;
        HANDLE      threadHandle;
    
        ExAcquireFastMutex(&DisketteExtension->ThreadReferenceMutex);
    
        if (++(DisketteExtension->ThreadReferenceCount) == 0) {
            DisketteExtension->ThreadReferenceCount++;
    
            ExAcquireFastMutex(PagingMutex);
            if (++PagingReferenceCount == 1) {
    
                // Lock down the driver.
    
                MmResetDriverPaging(DriverEntry);
            }
            ExReleaseFastMutex(PagingMutex);
    
    
            // Create the thread.
    
            status = PsCreateSystemThread(&threadHandle,
                                          (ACCESS_MASK) 0L,
                                          NULL,
                                          (HANDLE) 0L,
                                          NULL,
                                          FloppyThread,
                                          DisketteExtension);
    
            if (!NT_SUCCESS(status)) {
                DisketteExtension->ThreadReferenceCount = -1;
    
                ExAcquireFastMutex(PagingMutex);
                if (--PagingReferenceCount == 0) {
                    MmPageEntireDriver(DriverEntry);
                }
                ExReleaseFastMutex(PagingMutex);
    
                ExReleaseFastMutex(&DisketteExtension->ThreadReferenceMutex);
                return status;
            }
    
            ExReleaseFastMutex(&DisketteExtension->ThreadReferenceMutex);
    
            ZwClose(threadHandle);
    
        } else {
            ExReleaseFastMutex(&DisketteExtension->ThreadReferenceMutex);
        }
    
        IoMarkIrpPending(Irp);
    
        ExInterlockedInsertTailList(
            &DisketteExtension->ListEntry,
            &Irp->Tail.Overlay.ListEntry,
            &DisketteExtension->ListSpinLock );
    
        KeReleaseSemaphore(
            &DisketteExtension->RequestSemaphore,
            (KPRIORITY) 0,
            1,
            FALSE );
    
        return STATUS_PENDING;
    }
    
    NTSTATUS
    FloppyCreateClose(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
    
    /*++
    
    Routine Description:
    
        This routine is called only rarely by the I/O system; it's mainly
        for layered drivers to call.  All it does is complete the IRP
        successfully.
    
    Arguments:
    
        DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.
    
        Irp - a pointer to the I/O Request Packet for this request.
    
    Return Value:
    
        Always returns STATUS_SUCCESS, since this is a null operation.
    
    --*/
    
    {
#ifdef TOSHIBA
        PDISKETTE_EXTENSION disketteExtension;
#endif
        UNREFERENCED_PARAMETER( DeviceObject );
    
        FloppyDump(
            FLOPSHOW,
            ("FloppyCreateClose...\n")
            );
    
        //
        // Null operation.  Do not give an I/O boost since
        // no I/O was actually done.  IoStatus.Information should be
        // FILE_OPENED for an open; it's undefined for a close.
        //
    
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = FILE_OPENED;
    
        IoCompleteRequest( Irp, IO_NO_INCREMENT );
    
        return STATUS_SUCCESS;
    }
    
    NTSTATUS
    FloppyDeviceControl(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
    
    /*++
    
    Routine Description:
    
        This routine is called by the I/O system to perform a device I/O
        control function.
    
    Arguments:
    
        DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.
    
        Irp - a pointer to the I/O Request Packet for this request.
    
    Return Value:
    
        STATUS_SUCCESS or STATUS_PENDING if recognized I/O control code,
        STATUS_INVALID_DEVICE_REQUEST otherwise.
    
    --*/
    
    {
        PIO_STACK_LOCATION irpSp;
        PDISKETTE_EXTENSION disketteExtension;
        PDISK_GEOMETRY outputBuffer;
        NTSTATUS ntStatus;
        ULONG outputBufferLength;
        UCHAR i;
        DRIVE_MEDIA_TYPE lowestDriveMediaType;
        DRIVE_MEDIA_TYPE highestDriveMediaType;
        ULONG formatExParametersSize;
        PFORMAT_EX_PARAMETERS formatExParameters;
    
        FloppyDump(
            FLOPSHOW,
            ("FloppyDeviceControl...\n")
            );
    
        disketteExtension = DeviceObject->DeviceExtension;
        irpSp = IoGetCurrentIrpStackLocation( Irp );
    
#ifdef TOSHIBA
        
        /* 98/9/3
         *  When the removal is happened, we never do any more.
         */
        if( disketteExtension->Removed )
        {
            ntStatus = Irp->IoStatus.Status = STATUS_CANCELLED;
            if (!NT_SUCCESS( ntStatus ) &&
                IoIsErrorUserInduced( ntStatus )) {
        
                IoSetHardErrorOrVerifyDevice( Irp, DeviceObject );
        
            }
            IoCompleteRequest( Irp, IO_NO_INCREMENT );
            return  STATUS_CANCELLED;
        }
#endif
        
        switch( irpSp->Parameters.DeviceIoControl.IoControlCode ) {
    
            case IOCTL_DISK_FORMAT_TRACKS:
            case IOCTL_DISK_FORMAT_TRACKS_EX:
    
                //
                // Make sure that we got all the necessary format parameters.
                //
    
                if ( irpSp->Parameters.DeviceIoControl.InputBufferLength <
                    sizeof( FORMAT_PARAMETERS ) ) {
    
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: invalid FORMAT buffer length\n")
                        );
    
                    ntStatus = STATUS_INVALID_PARAMETER;
                    break;
                }
    
                //
                // Make sure the parameters we got are reasonable.
                //
    
                if ( !FlCheckFormatParameters(
                    disketteExtension,
                    (PFORMAT_PARAMETERS) Irp->AssociatedIrp.SystemBuffer ) ) {
    
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: invalid FORMAT parameters\n")
                        );
    
                    ntStatus = STATUS_INVALID_PARAMETER;
                    break;
                }
    
                //
                // If this is an EX request then make a couple of extra checks
                //
    
                if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
                    IOCTL_DISK_FORMAT_TRACKS_EX) {
    
                    if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                        sizeof(FORMAT_EX_PARAMETERS)) {
    
                        ntStatus = STATUS_INVALID_PARAMETER;
                        break;
                    }
    
                    formatExParameters = (PFORMAT_EX_PARAMETERS)
                                         Irp->AssociatedIrp.SystemBuffer;
                    formatExParametersSize =
                            FIELD_OFFSET(FORMAT_EX_PARAMETERS, SectorNumber) +
                            formatExParameters->SectorsPerTrack*sizeof(USHORT);
    
                    if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                        formatExParametersSize ||
                        formatExParameters->FormatGapLength >= 0x100 ||
                        formatExParameters->SectorsPerTrack >= 0x100) {
    
                        ntStatus = STATUS_INVALID_PARAMETER;
                        break;
                    }
                }
    
                //
                // Fall through to queue the request.
                //
    
            case IOCTL_DISK_CHECK_VERIFY:
            case IOCTL_STORAGE_CHECK_VERIFY:
            case IOCTL_DISK_GET_DRIVE_GEOMETRY:
            case IOCTL_DISK_IS_WRITABLE:
    
                //
                // The thread must know which diskette to operate on, but the
                // request list only passes the IRP.  So we'll stick a pointer
                // to the diskette extension in Type3InputBuffer, which is
                // a field that isn't used for floppy ioctls.
                //
    
                //
                // Add the request to the queue, and wake up the thread to
                // process it.
                //
    
                irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)
                    disketteExtension;
    
                FloppyDump(
                    FLOPIRPPATH,
                    ("Floppy: Enqueing  up IRP: %p\n",Irp)
                    );
    
                ntStatus = FlQueueIrpToThread(Irp, disketteExtension);
    
                break;
    
            case IOCTL_DISK_GET_MEDIA_TYPES:
            case IOCTL_STORAGE_GET_MEDIA_TYPES: {
    
                FloppyDump(
                    FLOPSHOW,
                    ("Floppy: IOCTL_DISK_GET_MEDIA_TYPES called\n")
                    );
    
                lowestDriveMediaType = DriveMediaLimits[
                    disketteExtension->DriveType].LowestDriveMediaType;
                highestDriveMediaType = DriveMediaLimits[
                    disketteExtension->DriveType].HighestDriveMediaType;
    
                outputBufferLength =
                    irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    
                //
                // Make sure that the input buffer has enough room to return
                // at least one descriptions of a supported media type.
                //
    
                if ( outputBufferLength < ( sizeof( DISK_GEOMETRY ) ) ) {
    
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: invalid GET_MEDIA_TYPES buffer size\n")
                        );
    
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                    break;
                }
    
                //
                // Assume success, although we might modify it to a buffer
                // overflow warning below (if the buffer isn't big enough
                // to hold ALL of the media descriptions).
                //
    
                ntStatus = STATUS_SUCCESS;
    
                if ( outputBufferLength < ( sizeof( DISK_GEOMETRY ) *
                    ( highestDriveMediaType - lowestDriveMediaType + 1 ) ) ) {
    
                    //
                    // The buffer is too small for all of the descriptions;
                    // calculate what CAN fit in the buffer.
                    //
    
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: GET_MEDIA_TYPES buffer size too small\n")
                        );
    
                    ntStatus = STATUS_BUFFER_OVERFLOW;
    
                    highestDriveMediaType =
                        (DRIVE_MEDIA_TYPE)( ( lowestDriveMediaType - 1 ) +
                        ( outputBufferLength /
                        sizeof( DISK_GEOMETRY ) ) );
                }
    
                outputBuffer = (PDISK_GEOMETRY) Irp->AssociatedIrp.SystemBuffer;
    
                for (
                    i = (UCHAR)lowestDriveMediaType;
                    i <= (UCHAR)highestDriveMediaType;
                    i++ ) {
#ifdef TOSHIBAJ
					// Skip over 1.23MB and 1.2MB if drive is 3.5" 2mode.
					if ( (disketteExtension->DriveType == DRIVE_TYPE_1440)
					  && (!disketteExtension->Available3Mode) ) {
						if (i == Drive144Media120) {
							i = Drive144Media144;

							FloppyDump(
									FLOPINFO,
									("Floppy: Skip over 1.23MB and 1.2MB\n")
									);
						} else if (i == Drive144Media640) {
							i = Drive144Media720;

							FloppyDump(
								FLOPINFO,
									("Floppy: Skip over 640KB\n")
									);
						}
					}
#endif
    
                    outputBuffer->MediaType = DriveMediaConstants[i].MediaType;
                    outputBuffer->Cylinders.LowPart =
                        DriveMediaConstants[i].MaximumTrack + 1;
                    outputBuffer->Cylinders.HighPart = 0;
                    outputBuffer->TracksPerCylinder =
                        DriveMediaConstants[i].NumberOfHeads;
                    outputBuffer->SectorsPerTrack =
                        DriveMediaConstants[i].SectorsPerTrack;
                    outputBuffer->BytesPerSector =
                        DriveMediaConstants[i].BytesPerSector;
                    FloppyDump(
                        FLOPSHOW,
                        ("Floppy: media types supported [%d]\n"
                         "------- Cylinders low:  0x%x\n"
                         "------- Cylinders high: 0x%x\n"
                         "------- Track/Cyl:      0x%x\n"
                         "------- Sectors/Track:  0x%x\n"
                         "------- Bytes/Sector:   0x%x\n"
                         "------- Media Type:       %d\n",
                         i,
                         outputBuffer->Cylinders.LowPart,
                         outputBuffer->Cylinders.HighPart,
                         outputBuffer->TracksPerCylinder,
                         outputBuffer->SectorsPerTrack,
                         outputBuffer->BytesPerSector,
                         outputBuffer->MediaType)
                         );
                    outputBuffer++;
    
                    Irp->IoStatus.Information += sizeof( DISK_GEOMETRY );
                }
    
                break;
            }
    
           case IOCTL_DISK_SENSE_DEVICE: {
    
                if (IsNEC_98) {
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: SENSE_DEVISE_STATUS \n")
                        );
    
                    //
                    // Make sure that we got all the necessary IOCTL read write parameters.
                    //
    
                    if ( irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                        sizeof( SENSE_DEVISE_STATUS_PTOS ) ) {
    
                        FloppyDump(
                            FLOPDBGP,
                            ("Floppy: invalid SENSE_DEVISE_STATUS buffer length\n")
                          );
    
                        ntStatus = STATUS_INVALID_PARAMETER;
                        break;
                    }
    
    
                    irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID)
                        disketteExtension;
    
                    FloppyDump(
                        FLOPIRPPATH,
                        ("Floppy: Enqueing  up IRP: %p\n",Irp)
                        );
    
    
                    ntStatus = FlQueueIrpToThread(Irp, disketteExtension);
    
                    break;
                } // (IsNEC_98)
            }
    
            default: {
    
                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: invalid device request %x\n",
                     irpSp->Parameters.DeviceIoControl.IoControlCode)
                    );
    
                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }
        }
    
        if ( ntStatus != STATUS_PENDING ) {
    
            Irp->IoStatus.Status = ntStatus;
            if (!NT_SUCCESS( ntStatus ) &&
                IoIsErrorUserInduced( ntStatus )) {
    
                IoSetHardErrorOrVerifyDevice( Irp, DeviceObject );
    
            }
            IoCompleteRequest( Irp, IO_NO_INCREMENT );
        }
    
        return ntStatus;
    }
    
    NTSTATUS
    FloppyPnp(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
    
    /*++
    
    Routine Description:
    
    Arguments:
    
        DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.
    
        Irp - a pointer to the I/O Request Packet for this request.
    
    Return Value:
    
    --*/
    
    {
        PIO_STACK_LOCATION irpSp;
        PDISKETTE_EXTENSION disketteExtension;
        NTSTATUS ntStatus = STATUS_SUCCESS;
        ULONG i;
    
        CONFIGURATION_TYPE Dc = DiskController;
        CONFIGURATION_TYPE Fp = FloppyDiskPeripheral;
    
        FloppyDump( FLOPSHOW, ("FloppyPnp:\n") );
    
        //
        //  Lock down the driver if it is not already locked.
        //
        ExAcquireFastMutex(PagingMutex);
        if (++PagingReferenceCount == 1) {
            MmResetDriverPaging(DriverEntry);
        }
        ExReleaseFastMutex(PagingMutex);
    
        disketteExtension = DeviceObject->DeviceExtension;
        
#ifdef TOSHIBA
        /* 98/9/3
         *  When the removal is happened, we never do any more.
         */
        if( disketteExtension->Removed )
        {
            Irp->IoStatus.Status = STATUS_CANCELLED;
            if (!NT_SUCCESS( ntStatus ) &&
                IoIsErrorUserInduced( ntStatus )) {
        
                IoSetHardErrorOrVerifyDevice( Irp, DeviceObject );
        
            }
            IoCompleteRequest( Irp, IO_NO_INCREMENT );
            return  STATUS_CANCELLED;
        }
#endif
        irpSp = IoGetCurrentIrpStackLocation( Irp );
    
        switch ( irpSp->MinorFunction ) {
    
        case IRP_MN_START_DEVICE: {
    
            KEVENT doneEvent;
            FDC_INFO fdcInfo;
    
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_START_DEVICE - Irp: %p\n", Irp) );
            FloppyDump( FLOPSHOW, ("  AllocatedResources = %08x\n",irpSp->Parameters.StartDevice.AllocatedResources));
            FloppyDump( FLOPSHOW, ("  AllocatedResourcesTranslated = %08x\n",irpSp->Parameters.StartDevice.AllocatedResourcesTranslated));
    
            //
            // This must do all of the startup processing that was previously done
            // in DriverEntry.
            //
            // First we must pass this Irp on to the PDO.
            //
    
            KeInitializeEvent( &doneEvent, NotificationEvent, FALSE );
    
            IoCopyCurrentIrpStackLocationToNext( Irp );
    
            IoSetCompletionRoutine( Irp,
                                    FloppyPnpComplete,
                                    &doneEvent,
                                    TRUE,
                                    TRUE,
                                    TRUE );
    
            ntStatus = IoCallDriver( disketteExtension->TargetObject, Irp );
    
            if ( ntStatus == STATUS_PENDING ) {
    
                ntStatus = KeWaitForSingleObject( &doneEvent,
                                                  Executive,
                                                  KernelMode,
                                                  FALSE,
                                                  NULL );
    
                ASSERT( ntStatus == STATUS_SUCCESS );
    
                ntStatus = Irp->IoStatus.Status;
            }
    
            fdcInfo.BufferCount = 0;
            fdcInfo.BufferSize = 0;
    
            ntStatus = FlFdcDeviceIo( disketteExtension->TargetObject,
                                      IOCTL_DISK_INTERNAL_GET_FDC_INFO,
                                      &fdcInfo );
    
            if ( NT_SUCCESS(ntStatus) ) {
    
                disketteExtension->MaxTransferSize =
                    fdcInfo.MaxTransferSize;
    
                if ( disketteExtension->DriveType == DRIVE_TYPE_2880 ) {
                    disketteExtension->PerpendicularMode |=
                        1 << fdcInfo.PeripheralNumber;
                }
    
                ntStatus = IoQueryDeviceDescription( &fdcInfo.BusType,
                                                     &fdcInfo.BusNumber,
                                                     &Dc,
                                                     &fdcInfo.ControllerNumber,
                                                     &Fp,
                                                     &fdcInfo.PeripheralNumber,
                                                     FlConfigCallBack,
                                                     disketteExtension );
    
                if (IsNEC_98) {
                    disketteExtension->DeviceUnit = (UCHAR)fdcInfo.UnitNumber;
                    disketteExtension->DriveOnValue = (UCHAR)fdcInfo.UnitNumber;
                } else { // (IsNEC_98)
                    disketteExtension->DeviceUnit = (UCHAR)fdcInfo.PeripheralNumber;
                    disketteExtension->DriveOnValue =
                        (UCHAR)(fdcInfo.PeripheralNumber | ( DRVCTL_DRIVE_0 << fdcInfo.PeripheralNumber ));
                } // (IsNEC_98)
            }
    
        #if 0
                if ( NT_SUCCESS(ntStatus) /* && we are on an ACPI machine */) {
        
                    LARGE_INTEGER acquireWait;
        
                    acquireWait.QuadPart = -(15 * 1000 * 10000);
        
                    DriveMediaConstants[DriveMediaLimits[disketteExtension->DriveType].
                        HighestDriveMediaType] = disketteExtension->BiosDriveMediaConstants;
        
                    ntStatus = FlFdcDeviceIo( disketteExtension->TargetObject,
                                              IOCTL_DISK_INTERNAL_ACQUIRE_FDC,
                                              &acquireWait );
        
                    if ( NT_SUCCESS(ntStatus) ) {
        
                        ntStatus = FlDatarateSpecifyConfigure( disketteExtension );
        
                        if ( NT_SUCCESS(ntStatus) ) {
        
                            ntStatus = FlRecalibrateDrive( disketteExtension );
                        }
        
                        FlFdcDeviceIo( disketteExtension->TargetObject,
                                       IOCTL_DISK_INTERNAL_RELEASE_FDC,
                                       disketteExtension->DeviceObject );
                    }
                    FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_START_DEVICE - Recalibrate status = %08x\n", ntStatus) );
                }
        #endif
            Irp->IoStatus.Status = ntStatus;
            IoCompleteRequest( Irp, IO_NO_INCREMENT );
    
            return ntStatus;
        }
    
        case IRP_MN_QUERY_REMOVE_DEVICE:
    
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_QUERY_REMOVE_DEVICE - Irp: %p\n", Irp));
    
            //
            //  Determine if the floppy thread is running.  If it is then
            //  reject this request.  Otherwise pass it on as usual.
            //
            ExAcquireFastMutex( &disketteExtension->ThreadReferenceMutex );
            if ( disketteExtension->ThreadReferenceCount != -1 ) {
    
                ExReleaseFastMutex( &disketteExtension->ThreadReferenceMutex );
                ntStatus = STATUS_DEVICE_BUSY;
                Irp->IoStatus.Status = ntStatus;
                IoCompleteRequest( Irp, IO_NO_INCREMENT );
                return ntStatus;
            }
    
            ExReleaseFastMutex( &disketteExtension->ThreadReferenceMutex );
    
            disketteExtension->StopPending = TRUE;
    
#ifdef TOSHIBA
        /* 98/9/23  5.5
         *  To avoid the problem , which remove from Systray, we check the reference count
         *  of the DeviceObject. And if the count is 2, we force to decrement by ZERO.
         *  I don't know why FastFat never decrements this counter after access was done.
         */
            
            {   ULONG   refcount;
        
                refcount = (ULONG)*(((ULONG *)DeviceObject)+1);
                if(refcount==2)
                    (ULONG)*(((ULONG *)DeviceObject)+1) = 0;
        
            }
        
#endif
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
    
        case IRP_MN_CANCEL_REMOVE_DEVICE:
    
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_CANCEL_REMOVE_DEVICE - Irp: %p\n", Irp));
    
            disketteExtension->StopPending = FALSE;
    
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
    
        case IRP_MN_REMOVE_DEVICE:
    
#ifdef TOSHIBA
        /*  98/8/28
         *  User will remove directly without using the systray selection
         *  So, this IRP_MN_SURPRISE_REMOVE is needed to handle.
         *  
         */
        case IRP_MN_SURPRISE_REMOVAL:
            disketteExtension->Removed = TRUE;
        
#endif
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_REMOVE_DEVICE - Irp: %p\n", Irp) );
#ifdef TOSHIBA
        /*  98/8/31
         *  This logic was created on 98/8/27, BUT the location was BAD!!
         *  Change the location before calling the LowerDriver.
         *  Delete the DosDevice name when the REMOVE is coming
         */
            {
                UCHAR               arcNameBuffer[256];
                STRING              arcNameString;
                UNICODE_STRING      arcUnicodeString;
        
        // _asm int 3
        
                sprintf( arcNameBuffer,
                         "\\DosDevices\\%c:",
                         disketteExtension->DriveAssignedLetter
                );
        
                FloppyDump( FLOPSHOW,("FloppyPnp: IoDeleteSymbolicLink(%s)\n", arcNameBuffer) );
        
                RtlInitString( &arcNameString, arcNameBuffer );
                ntStatus = RtlAnsiStringToUnicodeString( &arcUnicodeString,
                                                         &arcNameString,
                                                         TRUE );
        
                ntStatus = IoDeleteSymbolicLink( &arcUnicodeString );
                RtlFreeUnicodeString( &arcUnicodeString );

            }
#endif

				// 1998.11.11
				// Deassign an ARC name.
			{
				PDEVICE_OBJECT		physicalDeviceObject;
		        FDC_INFO            fdcInfo;
                UCHAR               arcNameBuffer[256];
                STRING              arcNameString;
                UNICODE_STRING      arcUnicodeString;

				physicalDeviceObject = disketteExtension->UnderlyingPDO;

#ifdef TOSHIBA
		        ntStatus = FlFdcDeviceIoPhys( physicalDeviceObject,
		                                      IOCTL_DISK_INTERNAL_GET_FDC_INFO,
		                                      &fdcInfo );
#else
        		ntStatus = FlFdcDeviceIo( physicalDeviceObject,
		                                  IOCTL_DISK_INTERNAL_GET_FDC_INFO,
		                                  &fdcInfo );
#endif
        		if (NT_SUCCESS(ntStatus)) {
            		sprintf( arcNameBuffer,
                    		 "%s(%d)disk(%d)fdisk(%d)",
                    		 "\\ArcName\\multi",
                    		 fdcInfo.BusNumber,
                    		 fdcInfo.PeripheralNumber,
                    		 fdcInfo.PeripheralNumber );

                	FloppyDump( FLOPSHOW,
                		("FloppyPnp: IoDeassignArcName(%s)\n", arcNameBuffer)
                	);

            		RtlInitString( &arcNameString, arcNameBuffer );
            		ntStatus = RtlAnsiStringToUnicodeString( &arcUnicodeString,
                                                     &arcNameString,
                                                     TRUE );
		            if ( NT_SUCCESS( ntStatus ) ) {
		                IoDeassignArcName( &arcUnicodeString );
        		        RtlFreeUnicodeString( &arcUnicodeString );
            		}
            	}
			}

            //
            //  We already know that the floppy thread is dead (there are no
            //  outstanding requests to the floppy device) so we don't need to
            //  wait for them here.
            //
            //  Forward this Irp to the underlying PDO
            //
            IoSkipCurrentIrpStackLocation( Irp );
            Irp->IoStatus.Status = STATUS_SUCCESS;
            ntStatus = IoCallDriver( disketteExtension->TargetObject, Irp );
    
            //
            //  Detatch from the undelying device.
            //
            IoDetachDevice( disketteExtension->TargetObject );
    
            //
            //  And delete the device.
            //
            IoDeleteDevice( DeviceObject );
    
            return ntStatus;
    
        case IRP_MN_QUERY_STOP_DEVICE:
    
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_QUERY_STOP_DEVICE - Irp: %p\n", Irp) );
    
            //
            //  Determine if the floppy thread is running.  If it is then
            //  reject this request.  Otherwise pass it on as usual.
            //
            ExAcquireFastMutex( &disketteExtension->ThreadReferenceMutex );
            if ( disketteExtension->ThreadReferenceCount != -1 ) {
    
                ExReleaseFastMutex( &disketteExtension->ThreadReferenceMutex );
                ntStatus = STATUS_DEVICE_BUSY;
                Irp->IoStatus.Status = ntStatus;
                IoCompleteRequest( Irp, IO_NO_INCREMENT );
                return ntStatus;
            }
    
            ExReleaseFastMutex( &disketteExtension->ThreadReferenceMutex );
    
            disketteExtension->StopPending = TRUE;
    
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
    
        case IRP_MN_CANCEL_STOP_DEVICE:
    
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_CANCEL_STOP_DEVICE - Irp: %p\n", Irp) );
    
            disketteExtension->StopPending = FALSE;
    
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
    
        case IRP_MN_STOP_DEVICE:
            FloppyDump( FLOPSHOW,("FloppyPnp: IRP_MN_STOP_DEVICE - Irp: %p\n", Irp) );
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
    
        default:
            FloppyDump( FLOPSHOW, ("FloppyPnp: Unsupported PNP Request %x - Irp: %p\n",irpSp->MinorFunction, Irp) );
        }
    
        IoSkipCurrentIrpStackLocation( Irp );
        ntStatus = IoCallDriver( disketteExtension->TargetObject, Irp );
    
#ifdef TOSHIBA  
        //98/8/28
        FloppyDump( FLOPSHOW, ("FloppyPnp: Request Return Status  %08x - Irp: %p\n",ntStatus,Irp) );
    
#endif
        //
        //  Page out the driver if it is not busy elsewhere.
        //
        ExAcquireFastMutex(PagingMutex);
        if (--PagingReferenceCount == 0) {
            MmPageEntireDriver(DriverEntry);
        }
        ExReleaseFastMutex(PagingMutex);
    
        return ntStatus;
    }
    
    NTSTATUS
    FloppyPnpComplete (
        IN PDEVICE_OBJECT   DeviceObject,
        IN PIRP             Irp,
        IN PVOID            Context
        )
    /*++
    Routine Description:
        A completion routine for use when calling the lower device objects to
        which our bus (FDO) is attached.
    
    --*/
    {
    
#ifdef TOSHIBA
        if ( Irp->PendingReturned ) {
    
            IoMarkIrpPending( Irp );
        }
#endif
    
        KeSetEvent ((PKEVENT) Context, 1, FALSE);
        // No special priority
        // No Wait
    
        return STATUS_MORE_PROCESSING_REQUIRED; // Keep this IRP
    }
    
    NTSTATUS
    FloppyPower(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
    /*++
    
    Routine Description:
    
    Arguments:
    
        DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.
    
        Irp - a pointer to the I/O Request Packet for this request.
    
    Return Value:
    
    --*/
    {
        PDISKETTE_EXTENSION disketteExtension;
        NTSTATUS ntStatus = STATUS_SUCCESS;
        PIO_STACK_LOCATION irpSp;
    
        disketteExtension = DeviceObject->DeviceExtension;
        irpSp = IoGetCurrentIrpStackLocation( Irp );
    
#ifdef TOSHIBA
        
        /* 98/9/3
         *  When the removal is happened, we never do any more.
         */
        if( disketteExtension->Removed )
        {
            Irp->IoStatus.Status = STATUS_CANCELLED;
            if (!NT_SUCCESS( ntStatus ) &&
                IoIsErrorUserInduced( ntStatus )) {
        
                IoSetHardErrorOrVerifyDevice( Irp, DeviceObject );
        
            }
            IoCompleteRequest( Irp, IO_NO_INCREMENT );
            return  STATUS_CANCELLED;
        }
#endif
        FloppyDump( FLOPSHOW, ("FloppyPower:\n"));
    
        PoStartNextPowerIrp( Irp );
        IoSkipCurrentIrpStackLocation( Irp );
        ntStatus = PoCallDriver( disketteExtension->TargetObject, Irp );
    
        return ntStatus;
    }
    
    NTSTATUS
    FloppyReadWrite(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
    
    /*++
    
    Routine Description:
    
        This routine is called by the I/O system to read or write to a
        device that we control.
    
    Arguments:
    
        DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.
    
        Irp - a pointer to the I/O Request Packet for this request.
    
    Return Value:
    
        STATUS_INVALID_PARAMETER if parameters are invalid,
        STATUS_PENDING otherwise.
    
    --*/
    
    {
        PIO_STACK_LOCATION irpSp;
        NTSTATUS ntStatus;
        PDISKETTE_EXTENSION disketteExtension;
    
        FloppyDump(
            FLOPSHOW,
            ("FloppyReadWrite...\n")
            );
    
        disketteExtension = DeviceObject->DeviceExtension;
    
#ifdef TOSHIBA
        
        /* 98/9/3
         *  When the removal is happened, we never do any more.
         */
        if( disketteExtension->Removed )
        {
            ntStatus = Irp->IoStatus.Status = STATUS_CANCELLED;
            if (!NT_SUCCESS( ntStatus ) &&
                IoIsErrorUserInduced( ntStatus )) {
        
                IoSetHardErrorOrVerifyDevice( Irp, DeviceObject );
        
            }
            IoCompleteRequest( Irp, IO_NO_INCREMENT );
            return  STATUS_CANCELLED;
        }
#endif
        
        irpSp = IoGetCurrentIrpStackLocation( Irp );
    
        if ( ( disketteExtension->MediaType > Unknown ) &&
            ( ( ( irpSp->Parameters.Read.ByteOffset ).LowPart +
                irpSp->Parameters.Read.Length > disketteExtension->ByteCapacity ) ||
            ( ( irpSp->Parameters.Read.Length &
                ( disketteExtension->BytesPerSector - 1 ) ) != 0 ) ) ) {
    
            FloppyDump(
                FLOPDBGP,
                ("Floppy: Invalid Parameter, rejecting request\n")
                );
    
            FloppyDump(
                FLOPWARN,
                ("Floppy: Starting offset = %lx\n"
                 "------  I/O Length = %lx\n"
                 "------  ByteCapacity = %lx\n"
                 "------  BytesPerSector = %lx\n",
                 irpSp->Parameters.Read.ByteOffset.LowPart,
                 irpSp->Parameters.Read.Length,
                 disketteExtension->ByteCapacity,
                 disketteExtension->BytesPerSector)
                );
    
            ntStatus = STATUS_INVALID_PARAMETER;
    
        } else {
    
    
            //
            // We need to pass the disketteExtension somewhere in the irp.
            // The "Key" field in our stack location should be unused.
            //
            // (fcf) Unfortunately, "Key" is only 32-bits, so we'll use
            //       Others.Argument4 instead.
            //
    
            irpSp->Parameters.Others.Argument4 = disketteExtension;
    
            FloppyDump(
                FLOPIRPPATH,
                ("Floppy: Enqueing  up IRP: %p\n",Irp)
                );
    
            ntStatus = FlQueueIrpToThread(Irp, disketteExtension);
        }
    
        if (ntStatus != STATUS_PENDING) {
            Irp->IoStatus.Status = ntStatus;
            IoCompleteRequest(Irp, 0);
        }
    
        return ntStatus;
    }
    
    NTSTATUS
    FlInterpretError(
        IN UCHAR StatusRegister1,
        IN UCHAR StatusRegister2
        )
    
    /*++
    
    Routine Description:
    
        This routine is called when the floppy controller returns an error.
        Status registers 1 and 2 are passed in, and this returns an appropriate
        error status.
    
    Arguments:
    
        StatusRegister1 - the controller's status register #1.
    
        StatusRegister2 - the controller's status register #2.
    
    Return Value:
    
        An NTSTATUS error determined from the status registers.
    
    --*/
    
    {
        if ( ( StatusRegister1 & STREG1_CRC_ERROR ) ||
            ( StatusRegister2 & STREG2_CRC_ERROR ) ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_CRC_ERROR\n")
                );
            return STATUS_CRC_ERROR;
        }
    
        if ( StatusRegister1 & STREG1_DATA_OVERRUN ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_DATA_OVERRUN\n")
                );
            return STATUS_DATA_OVERRUN;
        }
    
        if ( ( StatusRegister1 & STREG1_SECTOR_NOT_FOUND ) ||
            ( StatusRegister1 & STREG1_END_OF_DISKETTE ) ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_NONEXISTENT_SECTOR\n")
                );
            return STATUS_NONEXISTENT_SECTOR;
        }
    
        if ( ( StatusRegister2 & STREG2_DATA_NOT_FOUND ) ||
            ( StatusRegister2 & STREG2_BAD_CYLINDER ) ||
            ( StatusRegister2 & STREG2_DELETED_DATA ) ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_DEVICE_DATA_ERROR\n")
                );
            return STATUS_DEVICE_DATA_ERROR;
        }
    
        if ( StatusRegister1 & STREG1_WRITE_PROTECTED ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_MEDIA_WRITE_PROTECTED\n")
                );
            return STATUS_MEDIA_WRITE_PROTECTED;
        }
    
        if ( StatusRegister1 & STREG1_ID_NOT_FOUND ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_FLOPPY_ID_MARK_NOT_FOUND\n")
                );
            return STATUS_FLOPPY_ID_MARK_NOT_FOUND;
    
        }
    
        if ( StatusRegister2 & STREG2_WRONG_CYLINDER ) {
    
            FloppyDump(
                FLOPSHOW,
                ("FlInterpretError: STATUS_FLOPPY_WRONG_CYLINDER\n")
                );
            return STATUS_FLOPPY_WRONG_CYLINDER;
    
        }
    
        //
        // There's other error bits, but no good status values to map them
        // to.  Just return a generic one.
        //
    
        FloppyDump(
            FLOPSHOW,
            ("FlInterpretError: STATUS_FLOPPY_UNKNOWN_ERROR\n")
            );
        return STATUS_FLOPPY_UNKNOWN_ERROR;
    }
    
    VOID
    FlFinishOperation(
        IN OUT PIRP Irp,
        IN PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine is called by FloppyThread at the end of any operation
        whether it succeeded or not.
    
        If the packet is failing due to a hardware error, this routine will
        reinitialize the hardware and retry once.
    
        When the packet is done, this routine will start the timer to turn
        off the motor, and complete the IRP.
    
    Arguments:
    
        Irp - a pointer to the IO Request Packet being processed.
    
        DisketteExtension - a pointer to the diskette extension for the
        diskette on which the operation occurred.
    
    Return Value:
    
        None.
    
    --*/
    
    {
        NTSTATUS ntStatus;
    
        FloppyDump(
            FLOPSHOW,
            ("Floppy: FloppyFinishOperation...\n")
            );
    
        //
        // See if this packet is being failed due to a hardware error.
        //
    
        if ( ( Irp->IoStatus.Status != STATUS_SUCCESS ) &&
            ( DisketteExtension->HardwareFailed ) ) {
    
            DisketteExtension->HardwareFailCount++;
    
            if ( DisketteExtension->HardwareFailCount <
                 HARDWARE_RESET_RETRY_COUNT ) {
    
                //
                // This is our first time through (that is, we're not retrying
                // the packet after a hardware failure).  If it failed this first
                // time because of a hardware problem, set the HardwareFailed flag
                // and put the IRP at the beginning of the request queue.
                //
    
                ntStatus = FlInitializeControllerHardware( DisketteExtension );
    
                if ( NT_SUCCESS( ntStatus ) ) {
    
                    FloppyDump(
                        FLOPINFO,
                        ("Floppy: packet failed; hardware reset.  Retry.\n")
                        );
    
                    //
                    // Force media to be redetermined, in case we messed up
                    // and to make sure FlDatarateSpecifyConfigure() gets
                    // called.
                    //
    
                    DisketteExtension->MediaType = Undetermined;
    
                    FloppyDump(
                        FLOPIRPPATH,
                        ("Floppy: irp %p failed - back on the queue with it\n",
                         Irp)
                        );
    
                    ExAcquireFastMutex(&DisketteExtension->ThreadReferenceMutex);
                    ASSERT(DisketteExtension->ThreadReferenceCount >= 0);
                    (DisketteExtension->ThreadReferenceCount)++;
                    ExReleaseFastMutex(&DisketteExtension->ThreadReferenceMutex);
    
                    ExInterlockedInsertHeadList(
                        &DisketteExtension->ListEntry,
                        &Irp->Tail.Overlay.ListEntry,
                        &DisketteExtension->ListSpinLock );
    
                    return;
                }
    
                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: packet AND hardware reset failed.\n")
                    );
            }
    
        }
    
        //
        // If we didn't already RETURN, we're done with this packet so
        // reset the HardwareFailCount for the next packet.
        //
    
        DisketteExtension->HardwareFailCount = 0;
    
        //
        // If this request was unsuccessful and the error is one that can be
        // remedied by the user, save the Device Object so that the file system,
        // after reaching its original entry point, can know the real device.
        //
    
        if ( !NT_SUCCESS( Irp->IoStatus.Status ) &&
             IoIsErrorUserInduced( Irp->IoStatus.Status ) ) {
    
            IoSetHardErrorOrVerifyDevice( Irp, DisketteExtension->DeviceObject );
        }
    
        //
        // Even if the operation failed, it probably had to wait for the drive
        // to spin up or somesuch so we'll always complete the request with the
        // standard priority boost.
        //
    
        if ( ( Irp->IoStatus.Status != STATUS_SUCCESS ) &&
            ( Irp->IoStatus.Status != STATUS_VERIFY_REQUIRED ) &&
            ( Irp->IoStatus.Status != STATUS_NO_MEDIA_IN_DEVICE ) ) {
    
            FloppyDump(
                FLOPDBGP,
                ("Floppy: IRP failed with error %lx\n", Irp->IoStatus.Status)
                );
    
        } else {
    
            FloppyDump(
                FLOPINFO,
                ("Floppy: IoStatus.Status = %x\n", Irp->IoStatus.Status)
                );
        }
    
        FloppyDump(
            FLOPINFO,
            ("Floppy: IoStatus.Information = %x\n", Irp->IoStatus.Information)
            );
    
        FloppyDump(
            FLOPIRPPATH,
            ("Floppy: Finishing up IRP: %p\n",Irp)
            );
    
        //
        //  In order to get explorer to request a format of unformatted media
        //  the STATUS_UNRECOGNIZED_MEDIA error must be translated to a generic
        //  STATUS_UNSUCCESSFUL error.
        //
    //    if ( Irp->IoStatus.Status == STATUS_UNRECOGNIZED_MEDIA ) {
    //        Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
    //    }
        IoCompleteRequest( Irp, IO_DISK_INCREMENT );
    }
    
    NTSTATUS
    FlStartDrive(
        IN OUT PDISKETTE_EXTENSION DisketteExtension,
        IN PIRP Irp,
        IN BOOLEAN WriteOperation,
        IN BOOLEAN SetUpMedia,
        IN BOOLEAN IgnoreChange
        )
    
    /*++
    
    Routine Description:
    
        This routine is called at the beginning of every operation.  It cancels
        the motor timer if it's on, turns the motor on and waits for it to
        spin up if it was off, resets the disk change line and returns
        VERIFY_REQUIRED if the disk has been changed, determines the diskette
        media type if it's not known and SetUpMedia=TRUE, and makes sure that
        the disk isn't write protected if WriteOperation = TRUE.
    
    Arguments:
    
        DisketteExtension - a pointer to our data area for the drive being
        started.
    
        Irp - Supplies the I/O request packet.
    
        WriteOperation - TRUE if the diskette will be written to, FALSE
        otherwise.
    
        SetUpMedia - TRUE if the media type of the diskette in the drive
        should be determined.
    
        IgnoreChange - Do not return VERIFY_REQUIRED eventhough we are mounting
        for the first time.
    
    Return Value:
    
        STATUS_SUCCESS if the drive is started properly; appropriate error
        propogated otherwise.
    
    --*/
    
    {
        LARGE_INTEGER    delay;
        BOOLEAN  motorStarted;
        BOOLEAN  diskChanged;
        UCHAR    driveStatus;
        NTSTATUS ntStatus = STATUS_SUCCESS;
        FDC_ENABLE_PARMS    fdcEnableParms;
        FDC_DISK_CHANGE_PARMS fdcDiskChangeParms;
    
        FloppyDump(
            FLOPSHOW,
            ("Floppy: FloppyStartDrive...\n")
            );
    
        //
        // IMPORTANT
        // NOTE
        // COMMENT
        //
        // Here we will copy the BIOS floppy configuration on top of the
        // highest media value in our global array so that any type of processing
        // that will recalibrate the drive can have it done here.
        // An optimization would be to only do it when we will try to recalibrate
        // the driver or media in it.
        // At this point, we ensure that on any processing of a command we
        // are going to have the real values inthe first entry of the array for
        // driver constants.
        //
    
        DriveMediaConstants[DriveMediaLimits[DisketteExtension->DriveType].
            HighestDriveMediaType] = DisketteExtension->BiosDriveMediaConstants;
    
        if ((DisketteExtension->MediaType == Undetermined) ||
            (DisketteExtension->MediaType == Unknown)) {
            DisketteExtension->DriveMediaConstants = DriveMediaConstants[0];
        }
    
        //
        // Grab the timer spin lock and cancel the timer, since we want the
        // motor to run for the whole operation.  If the proper drive is
        // already running, great; if not, start the motor and wait for it
        // to spin up.
        //
    
        fdcEnableParms.DriveOnValue = DisketteExtension->DriveOnValue;
        if ( WriteOperation ) {
            fdcEnableParms.TimeToWait =
                DisketteExtension->DriveMediaConstants.MotorSettleTimeWrite;
        } else {
            fdcEnableParms.TimeToWait =
                DisketteExtension->DriveMediaConstants.MotorSettleTimeRead;
        }
    
        ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                  IOCTL_DISK_INTERNAL_ENABLE_FDC_DEVICE,
                                  &fdcEnableParms );
    
        motorStarted = fdcEnableParms.MotorStarted;
    
        if (NT_SUCCESS(ntStatus)) {
    
            fdcDiskChangeParms.DriveOnValue = DisketteExtension->DriveOnValue;
    
            ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                      IOCTL_DISK_INTERNAL_GET_FDC_DISK_CHANGE,
                                      &fdcDiskChangeParms );
    
            driveStatus = fdcDiskChangeParms.DriveStatus;
        }
    
        if (!NT_SUCCESS(ntStatus)) {
            return ntStatus;
        }
    
        //
        // Support for 360K drives:
        // They have no change line, so we will assume a power up of the motor
        // to be equivalent to a change of floppy (we assume noone will
        // change the floppy while it is turning.
        // So force a VERIFY here (unless the file system explicitly turned
        // it off).
        //
    
        if ( ((DisketteExtension->DriveType == DRIVE_TYPE_0360) &&
                  motorStarted) ||
             ((DisketteExtension->DriveType != DRIVE_TYPE_0360) &&
                  driveStatus & DSKCHG_DISKETTE_REMOVED) ) {
    
            FloppyDump(
                FLOPSHOW,
                ("Floppy: disk changed...\n")
                );
    
            DisketteExtension->MediaType = Undetermined;
    
            //
            // If the volume is mounted, we must tell the filesystem to
            // verify that the media in the drive is the same volume.
            //
    
            if ( DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED ) {
    
                if (Irp) {
                    IoSetHardErrorOrVerifyDevice( Irp,
                                                  DisketteExtension->DeviceObject );
                }
                DisketteExtension->DeviceObject->Flags |= DO_VERIFY_VOLUME;
            }
    
            //
            // Only go through the device reset if we did get the flag set
            // We really only want to go throught here if the diskette changed,
            // but on 360 it will always say the diskette has changed.
            // So based on our previous test, only proceed if it is NOT
            // a 360K driver
    
            if (DisketteExtension->DriveType != DRIVE_TYPE_0360) {
    
                if (IsNEC_98) {
    
                    //
                    // Before seek, make sure that disk has been removed.
                    //
    
                    DisketteExtension->FifoBuffer[0] = COMMND_SENSE_DRIVE_STATUS;
                    DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
    
                    ntStatus = FlIssueCommand( DisketteExtension,
                                               DisketteExtension->FifoBuffer,
                                               DisketteExtension->FifoBuffer,
                                               NULL,
                                               0,
                                               0 );
    
                    if ( !NT_SUCCESS( ntStatus ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: SENSE_DRIVE (1) returned%x\n", ntStatus)
                            );
    
                        return ntStatus;
                    }
    
                    if ( DisketteExtension->FifoBuffer[0] & STREG3_DRIVE_READY ) {
    
                        driveStatus = DSKCHG_RESERVED;
    
                    } else {
    
                        driveStatus = DSKCHG_DISKETTE_REMOVED;
                    }
    
                    if ( driveStatus & DSKCHG_DISKETTE_REMOVED ) {
    
                        //
                        // If "disk changed" is still set after the double seek, the
                        // drive door must be opened.
                        //
    
                        FloppyDump(
                            FLOPINFO,
                            ("Floppy: close the door! (1)\n")
                            );
    
                        //
                        // Turn off the flag for now so that we will not get so many
                        // gratuitous verifys.  It will be set again the next time.
                        //
    
                        if(DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED) {
    
                            DisketteExtension->DeviceObject->Flags &= ~DO_VERIFY_VOLUME;
    
                        }
    
                        return STATUS_NO_MEDIA_IN_DEVICE;
                    }
                } // (IsNEC_98)
    
                //
                // Now seek twice to reset the "disk changed" line.  First
                // seek to 1.
                //
                // Normally we'd do a READ ID after a seek.  However, we don't
                // even know if this disk is formatted.  We're not really
                // trying to get anywhere; we're just doing this to reset the
                // "disk changed" line so we'll skip the READ ID.
                //
    
                DisketteExtension->FifoBuffer[0] = COMMND_SEEK;
                DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
                DisketteExtension->FifoBuffer[2] = 1;
    
                ntStatus = FlIssueCommand( DisketteExtension,
                                           DisketteExtension->FifoBuffer,
                                           DisketteExtension->FifoBuffer,
                                           NULL,
                                           0,
                                           0 );
    
                if ( !NT_SUCCESS( ntStatus ) ) {
    
                    FloppyDump( FLOPWARN, 
                                ("Floppy: seek to 1 returned %x\n", ntStatus) );
    
                    return ntStatus;
    
                } else {
    
                    if (!( DisketteExtension->FifoBuffer[0] & STREG0_SEEK_COMPLETE)
                        || ( DisketteExtension->FifoBuffer[1] != 1 ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: Seek to 1 had bad return registers\n")
                            );
    
                        DisketteExtension->HardwareFailed = TRUE;
    
                        return STATUS_FLOPPY_BAD_REGISTERS;
                    }
                }
    
                //
                // Seek back to 0.  We can once again skip the READ ID.
                //
    
                DisketteExtension->FifoBuffer[0] = COMMND_SEEK;
                DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
                DisketteExtension->FifoBuffer[2] = 0;
    
                //
                // Floppy drives use by Toshiba systems require a delay
                // when this operation is performed.
                //
    
                delay.LowPart = (ULONG) -900;
                delay.HighPart = -1;
                KeDelayExecutionThread( KernelMode, FALSE, &delay );
                ntStatus = FlIssueCommand( DisketteExtension,
                                           DisketteExtension->FifoBuffer,
                                           DisketteExtension->FifoBuffer,
                                           NULL,
                                           0,
                                           0 );
                //
                // Again, for Toshiba floppy drives, a delay is required.
                //
    
                delay.LowPart = (ULONG) -5;
                delay.HighPart = -1;
                KeDelayExecutionThread( KernelMode, FALSE, &delay );
    
                if ( !NT_SUCCESS( ntStatus ) ) {
    
                    FloppyDump( FLOPWARN,
                                ("Floppy: seek to 0 returned %x\n", ntStatus) );
    
                    return ntStatus;
    
                } else {
    
                    if (!(DisketteExtension->FifoBuffer[0] & STREG0_SEEK_COMPLETE)
                        || ( DisketteExtension->FifoBuffer[1] != 0 ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: Seek to 0 had bad return registers\n")
                            );
    
                        DisketteExtension->HardwareFailed = TRUE;
    
                        return STATUS_FLOPPY_BAD_REGISTERS;
                    }
                }
    
    
                if (IsNEC_98) {
    
                    //
                    // Before seek, make sure that disk has been removed.
                    //
    
                    DisketteExtension->FifoBuffer[0] = COMMND_SENSE_DRIVE_STATUS;
                    DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
    
                    ntStatus = FlIssueCommand( DisketteExtension,
                                               DisketteExtension->FifoBuffer,
                                               DisketteExtension->FifoBuffer,
                                               NULL,
                                               0,
                                               0 );
    
                    if ( !NT_SUCCESS( ntStatus ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: SENSE_DRIVE (1) returned%x\n", ntStatus)
                            );
    
                        return ntStatus;
                    }
    
                    if ( DisketteExtension->FifoBuffer[0] & STREG3_DRIVE_READY ) {
    
                        driveStatus = DSKCHG_RESERVED;
    
                    } else {
    
                        driveStatus = DSKCHG_DISKETTE_REMOVED;
                    }
                } else { // (IsNEC_98)
    
                    ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                              IOCTL_DISK_INTERNAL_GET_FDC_DISK_CHANGE,
                                              &fdcDiskChangeParms );
    
                    driveStatus = fdcDiskChangeParms.DriveStatus;
    
                    if (!NT_SUCCESS(ntStatus)) {
                        return ntStatus;
                    }
                } // (IsNEC_98)
    
                if ( driveStatus & DSKCHG_DISKETTE_REMOVED ) {
    
                    //
                    // If "disk changed" is still set after the double seek, the
                    // drive door must be opened.
                    //
    
                    FloppyDump(
                        FLOPINFO,
                        ("Floppy: close the door!\n")
                        );
    
                    //
                    // Turn off the flag for now so that we will not get so many
                    // gratuitous verifys.  It will be set again the next time.
                    //
    
                    if(DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED) {
    
                        DisketteExtension->DeviceObject->Flags &= ~DO_VERIFY_VOLUME;
    
                    }
    
                    return STATUS_NO_MEDIA_IN_DEVICE;
                }
            }
    
            //
            // IgnoreChange indicates the file system is in the process
            // of performing a verify so do not return verify required.
            //
    
            if (( IgnoreChange == FALSE ) &&
                ( DisketteExtension->DeviceObject->Vpb->Flags & VPB_MOUNTED )) {
    
                //
                // Drive WAS mounted, but door was opened since the last time
                // we checked so tell the file system to verify the diskette.
                //
    
                FloppyDump(
                    FLOPSHOW,
                    ("Floppy: start drive - verify required because door opened\n")
                    );
    
                return STATUS_VERIFY_REQUIRED;
            }
        }else{
            if (IsNEC_98) {
    
                FlHdbit(DisketteExtension);
    
            } // (IsNEC_98)
        }
    
        if ( SetUpMedia ) {
#ifdef TOSHIBAJ
    //
    // By MS original code, Error never recover when media type necomes UNKNOWN state.
    //
            if ( (DisketteExtension->MediaType == Undetermined) ||
                 (DisketteExtension->MediaType == Unknown)
                ){
                ntStatus = FlDetermineMediaType( DisketteExtension );
            }
#endif
    
            if ( DisketteExtension->MediaType == Undetermined ) {
    
                ntStatus = FlDetermineMediaType( DisketteExtension );
    
            } else {
    
                if ( DisketteExtension->MediaType == Unknown ) {
    
                    //
                    // We've already tried to determine the media type and
                    // failed.  It's probably not formatted.
                    //
    
                    FloppyDump(
                        FLOPSHOW,
                        ("Floppy - start drive - media type was unknown\n")
                        );
                    return STATUS_UNRECOGNIZED_MEDIA;
    
                } else {
    
                    if ( DisketteExtension->DriveMediaType !=
                        DisketteExtension->LastDriveMediaType ) {
    
                        //
                        // Last drive/media combination accessed by the
                        // controller was different, so set up the controller.
                        //
    
                        ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );
                        if (!NT_SUCCESS(ntStatus)) {
    
                            FloppyDump(
                                FLOPWARN,
                                ("Floppy: start drive - bad status from datarate"
                                 "------  specify %x\n",
                                 ntStatus)
                                );
    
                        }
                    }
                }
            }
        }
    
        //
        // If this is a WRITE, check the drive to make sure it's not write
        // protected.  If so, return an error.
        //
    
        if ( ( WriteOperation ) && ( NT_SUCCESS( ntStatus ) ) ) {
    
            DisketteExtension->FifoBuffer[0] = COMMND_SENSE_DRIVE_STATUS;
            DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
    
            ntStatus = FlIssueCommand( DisketteExtension,
                                       DisketteExtension->FifoBuffer,
                                       DisketteExtension->FifoBuffer,
                                       NULL,
                                       0,
                                       0 );
    
            if ( !NT_SUCCESS( ntStatus ) ) {
    
                FloppyDump(
                    FLOPWARN,
                    ("Floppy: SENSE_DRIVE returned %x\n", ntStatus)
                    );
    
                return ntStatus;
            }
    
            if (IsNEC_98) {
                //
                // Check if media has be ejected.
                //
                if (!(DisketteExtension->FifoBuffer[0] & STREG3_DRIVE_READY)) {
    
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: start drive - media is not ready\n")
                        );
                    return STATUS_NO_MEDIA_IN_DEVICE;
                }
            } // (IsNEC_98)
    
            if ( DisketteExtension->FifoBuffer[0] & STREG3_WRITE_PROTECTED ) {
    
                FloppyDump(
                    FLOPSHOW,
                    ("Floppy: start drive - media is write protected\n")
                    );
                return STATUS_MEDIA_WRITE_PROTECTED;
            }
        }
    
        return ntStatus;
    }
    
    NTSTATUS
    FlDatarateSpecifyConfigure(
        IN PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine is called to set up the controller every time a new type
        of diskette is to be accessed.  It issues the CONFIGURE command if
        it's available, does a SPECIFY, sets the data rate, and RECALIBRATEs
        the drive.
    
        The caller must set DisketteExtension->DriveMediaType before calling
        this routine.
    
    Arguments:
    
        DisketteExtension - pointer to our data area for the drive to be
        prepared.
    
    Return Value:
    
        STATUS_SUCCESS if the controller is properly prepared; appropriate
        error propogated otherwise.
    
    --*/
    
    {
        NTSTATUS ntStatus = STATUS_SUCCESS;
#ifdef TOSHIBAJ
        BOOLEAN ChangeResult;
    
        // for 3 mode support
        if( DisketteExtension->Available3Mode ) {
            ChangeResult = FlEnable3Mode( DisketteExtension );
        }
#endif
    
        //
        // If the controller has a CONFIGURE command, use it to enable implied
        // seeks.  If it doesn't, we'll find out here the first time through.
        //
        if ( DisketteExtension->ControllerConfigurable ) {
    
            DisketteExtension->FifoBuffer[0] = COMMND_CONFIGURE;
            DisketteExtension->FifoBuffer[1] = 0;
    
            DisketteExtension->FifoBuffer[2] = COMMND_CONFIGURE_FIFO_THRESHOLD;
            DisketteExtension->FifoBuffer[2] += COMMND_CONFIGURE_DISABLE_POLLING;
    
            if (!DisketteExtension->DriveMediaConstants.CylinderShift) {
                DisketteExtension->FifoBuffer[2] += COMMND_CONFIGURE_IMPLIED_SEEKS;
            }
    
            DisketteExtension->FifoBuffer[3] = 0;
    
            ntStatus = FlIssueCommand( DisketteExtension,
                                       DisketteExtension->FifoBuffer,
                                       DisketteExtension->FifoBuffer,
                                       NULL,
                                       0,
                                       0 );
    
            if ( ntStatus == STATUS_DEVICE_NOT_READY ) {
    
                DisketteExtension->ControllerConfigurable = FALSE;
                ntStatus = STATUS_SUCCESS;
            }
        }
    
        //
        // Issue SPECIFY command to program the head load and unload
        // rates, the drive step rate, and the DMA data transfer mode.
        //
    
        if ( NT_SUCCESS( ntStatus ) ||
             ntStatus == STATUS_DEVICE_NOT_READY ) {
    
            DisketteExtension->FifoBuffer[0] = COMMND_SPECIFY;
            DisketteExtension->FifoBuffer[1] =
                DisketteExtension->DriveMediaConstants.StepRateHeadUnloadTime;
    
            DisketteExtension->FifoBuffer[2] =
                DisketteExtension->DriveMediaConstants.HeadLoadTime;
    
            ntStatus = FlIssueCommand( DisketteExtension,
                                       DisketteExtension->FifoBuffer,
                                       DisketteExtension->FifoBuffer,
                                       NULL,
                                       0,
                                       0 );
    
            if ( NT_SUCCESS( ntStatus ) ) {
    
                //
                // Program the data rate
                //
    
                ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                          IOCTL_DISK_INTERNAL_SET_FDC_DATA_RATE,
                                          &DisketteExtension->
                                            DriveMediaConstants.DataTransferRate );
    
                //
                // Recalibrate the drive, now that we've changed all its
                // parameters.
                //
    
                if (NT_SUCCESS(ntStatus)) {
    
                    ntStatus = FlRecalibrateDrive( DisketteExtension );
                }
            } else {
                FloppyDump(
                    FLOPINFO,
                    ("Floppy: Failed specify %x\n", ntStatus)
                    );
            }
        } else {
            FloppyDump(
                FLOPINFO,
                ("Floppy: Failed configuration %x\n", ntStatus)
                );
        }
    
        if ( NT_SUCCESS( ntStatus ) ) {
    
            DisketteExtension->LastDriveMediaType =
                DisketteExtension->DriveMediaType;
    
        } else {
    
            DisketteExtension->LastDriveMediaType = Unknown;
            FloppyDump(
                FLOPINFO,
                ("Floppy: Failed recalibrate %x\n", ntStatus)
                );
        }
    
        return ntStatus;
    }
    
    NTSTATUS
    FlRecalibrateDrive(
        IN PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine recalibrates a drive.  It is called whenever we're
        setting up to access a new diskette, and after certain errors.  It
        will actually recalibrate twice, since many controllers stop after
        77 steps and many disks have 80 tracks.
    
    Arguments:
    
        DisketteExtension - pointer to our data area for the drive to be
        recalibrated.
    
    Return Value:
    
        STATUS_SUCCESS if the drive is successfully recalibrated; appropriate
        error is propogated otherwise.
    
    --*/
    
    {
        NTSTATUS ntStatus;
        UCHAR recalibrateCount;
    
        recalibrateCount = 0;
    
        do {
    
            //
            // Issue the recalibrate command
            //
    
            DisketteExtension->FifoBuffer[0] = COMMND_RECALIBRATE;
            DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
    
            ntStatus = FlIssueCommand( DisketteExtension,
                                       DisketteExtension->FifoBuffer,
                                       DisketteExtension->FifoBuffer,
                                       NULL,
                                       0,
                                       0 );
    
            if ( !NT_SUCCESS( ntStatus ) ) {
    
                FloppyDump(
                    FLOPWARN,
                    ("Floppy: recalibrate returned %x\n", ntStatus)
                    );
    
            }
    
            if ( NT_SUCCESS( ntStatus ) ) {
    
                if (IsNEC_98) {
                    UCHAR       fifoBuffer[2];
    
                    //
                    // Procedure for media is not ready
                    //
                    fifoBuffer[0] = DisketteExtension->FifoBuffer[0];
                    fifoBuffer[1] = DisketteExtension->FifoBuffer[1];
    
                    //
                    // Sense target drive and get all data at transition of condistion.
                    //
                    DisketteExtension->FifoBuffer[0] = COMMND_SENSE_DRIVE_STATUS;
                    DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
    
                    ntStatus = FlIssueCommand( DisketteExtension,
                                               DisketteExtension->FifoBuffer,
                                               DisketteExtension->FifoBuffer,
                                               NULL,
                                               0,
                                               0 );
    
                    if ( !NT_SUCCESS( ntStatus ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: SENSE_DRIVE returned %x\n", ntStatus)
                            );
    
                        return ntStatus;
                    }
    
                    DisketteExtension->FifoBuffer[0] = fifoBuffer[0];
                    DisketteExtension->FifoBuffer[1] = fifoBuffer[1];
    
                } // (IsNEC_98)
    
                if ( !( DisketteExtension->FifoBuffer[0] & STREG0_SEEK_COMPLETE ) ||
                    ( DisketteExtension->FifoBuffer[1] != 0 ) ) {
    
                    FloppyDump(
                        FLOPWARN,
                        ("Floppy: recalibrate had bad registers\n")
                        );
    
                    DisketteExtension->HardwareFailed = TRUE;
    
                    ntStatus = STATUS_FLOPPY_BAD_REGISTERS;
                }
            }
    
            recalibrateCount++;
    
        } while ( ( !NT_SUCCESS( ntStatus ) ) && ( recalibrateCount < 2 ) );
    
        FloppyDump( FLOPSHOW,
                    ("Floppy: FloppyRecalibrateDrive: status %x, count %d\n",
                    ntStatus, recalibrateCount)
                    );
    
        return ntStatus;
    }
    
    NTSTATUS
    FlDetermineMediaType(
        IN OUT PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine is called by FlStartDrive() when the media type is
        unknown.  It assumes the largest media supported by the drive is
        available, and keeps trying lower values until it finds one that
        works.
    
    Arguments:
    
        DisketteExtension - pointer to our data area for the drive whose
        media is to checked.
    
    Return Value:
    
        STATUS_SUCCESS if the type of the media is determined; appropriate
        error propogated otherwise.
    
    --*/
    
    {
        NTSTATUS ntStatus;
        PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
#ifdef TOSHIBA
        BOOLEAN mediaTypesExhausted = FALSE;
#else
        BOOLEAN mediaTypesExhausted;
#endif
        ULONG retries = 0;
    
        USHORT sectorLengthCode;
        PBOOT_SECTOR_INFO bootSector;
        LARGE_INTEGER offset;
        PIRP irp;
    
        FloppyDump(
            FLOPSHOW,
            ("FlDetermineMediaType...\n")
            );
    
        DisketteExtension->IsReadOnly = FALSE;
    
        //
        // Try up to three times for read the media id.
        //
    
        for ( retries = 0; retries < 3; retries++ ) {
    
            if (retries) {
    
                //
                // We're retrying the media determination because
                // some silly controllers don't always want to work
                // at setup.  First we'll reset the device to give
                // it a better chance of working.
                //
    
                FloppyDump(
                    FLOPINFO,
                    ("FlDetermineMediaType: Resetting controller\n")
                    );
                FlInitializeControllerHardware( DisketteExtension );
            }
    
#ifdef TOSHIBAJ

        // For 3 mode, 1.2MB(512bytesSector) media CAN misunderstand even
        // though READ_ID command is used in this entry because 1.2MB media
        // is very lose to 1.4MB one.
        // To avoid this misunderstanding, we will try this first step by
        // 1.23MB(1024bytesSector). By this way, 1.4MB is slightly slow down
        // to be recognized.
        // There is magic. when FD mode is in 1.4MB, 1.2MB media maybe read
        // successfully. However, in 1.2MB mode, 1.4MB media CANNOT read
        // correctly.

    TryAgainLowerMedia:

        if( retries == 0 ) {
            DisketteExtension->DriveMediaType =
                DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType-1;
        } else {
        //
        // Assume that the largest supported media is in the drive.  If that
        // turns out to be untrue, we'll try successively smaller media types
        // until we find what's really in there (or we run out and decide
        // that the media isn't formatted).
        //
            DisketteExtension->DriveMediaType =
                DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
        }

#else

        //
        // Assume that the largest supported media is in the drive.  If that
        // turns out to be untrue, we'll try successively smaller media types
        // until we find what's really in there (or we run out and decide
        // that the media isn't formatted).
        //

        DisketteExtension->DriveMediaType =
           DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
#endif

        DisketteExtension->DriveMediaConstants =
            DriveMediaConstants[DisketteExtension->DriveMediaType];
    
#ifdef TOSHIBA
#else
            mediaTypesExhausted = FALSE;
#endif
            do {
    
                if (IsNEC_98) {
                    sectorLengthCode = DriveMediaConstants[DisketteExtension->DriveMediaType].SectorLengthCode;
    
                    FlHdbit(DisketteExtension);
    
                } // (IsNEC_98)
    
                ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );
    
                if ( !NT_SUCCESS( ntStatus ) ) {
    
                    //
                    // The SPECIFY or CONFIGURE commands resulted in an error.
                    // Force ourselves out of this loop and return error.
                    //
    
                    FloppyDump(
                        FLOPINFO,
                        ("FlDetermineMediaType: DatarateSpecify failed %x\n", ntStatus)
                        );
                    mediaTypesExhausted = TRUE;
    
                } else {
    
                    //
                    // Use the media constants table when trying to determine
                    // media type.
                    //
    
                    driveMediaConstants =
                        &DriveMediaConstants[DisketteExtension->DriveMediaType];
    
                    //
                    // Now try to read the ID from wherever we're at.
                    //
    
                    DisketteExtension->FifoBuffer[1] = (UCHAR)
                        ( DisketteExtension->DeviceUnit |
                        ( ( driveMediaConstants->NumberOfHeads - 1 ) << 2 ) );
    
                    DisketteExtension->FifoBuffer[0] =
                        COMMND_READ_ID + COMMND_OPTION_MFM;
    
                    ntStatus = FlIssueCommand( DisketteExtension,
                                               DisketteExtension->FifoBuffer,
                                               DisketteExtension->FifoBuffer,
                                               NULL,
                                               0,
                                               0 );
    
                    if ( (!NT_SUCCESS(ntStatus)) ||
                        ( (DisketteExtension->FifoBuffer[0]&(~STREG0_SEEK_COMPLETE)) !=
                            (UCHAR)( (DisketteExtension->DeviceUnit) |
                            ((driveMediaConstants->NumberOfHeads - 1) << 2 ) )) ||
                        (DisketteExtension->FifoBuffer[1] != 0) ||
                        (DisketteExtension->FifoBuffer[2] != 0) ||
#ifdef TOSHIBAJ
                        // Check the sector length, comes from READ-ID/resultPhase
                        // To get a sector length, figure out
                        //   (0x80 << ResultBytes[6]).
                        ((128 << (DisketteExtension->FifoBuffer[6])) != driveMediaConstants->BytesPerSector)
#else
                        (IsNEC_98 && (DisketteExtension->FifoBuffer[6] != sectorLengthCode))
#endif
                        ) {
    
                        FloppyDump(
                            FLOPINFO,
                            ("Floppy: READID failed trying lower media\n"
                             "------  status = %x\n"
                             "------  SR0 = %x\n"
                             "------  SR1 = %x\n"
                             "------  SR2 = %x\n",
                             ntStatus,
                             DisketteExtension->FifoBuffer[0],
                             DisketteExtension->FifoBuffer[1],
                             DisketteExtension->FifoBuffer[2])
                            );
    
                        DisketteExtension->DriveMediaType--;
#ifdef TOSHIBAJ
						// Skip over 1.23MB and 1.2MB if drive is 3.5" 2mode.
						if ( (DisketteExtension->DriveType == DRIVE_TYPE_1440)
						  && (!DisketteExtension->Available3Mode) ) {
							if (DisketteExtension->DriveMediaType == Drive144Media123) {
								DisketteExtension->DriveMediaType = Drive144Media720;

								FloppyDump(
										FLOPINFO,
										("Floppy: Skip over 1.23MB and 1.2MB\n")
										);
							} else if (DisketteExtension->DriveMediaType == Drive144Media640) {
								--DisketteExtension->DriveMediaType;

								FloppyDump(
									FLOPINFO,
										("Floppy: Skip over 640KB\n")
										);
							}
						}
#endif
                        DisketteExtension->DriveMediaConstants =
                            DriveMediaConstants[DisketteExtension->DriveMediaType];
    
                        if (ntStatus != STATUS_DEVICE_NOT_READY) {
    
                            ntStatus = STATUS_UNRECOGNIZED_MEDIA;
                        }
    
                        //
                        // Next comparison must be signed, for when
                        // LowestDriveMediaType = 0.
                        //
    
                        if ( (CHAR)( DisketteExtension->DriveMediaType ) <
                            (CHAR)( DriveMediaLimits[DisketteExtension->DriveType].
                            LowestDriveMediaType ) ) {
    
                            DisketteExtension->MediaType = Unknown;
                            mediaTypesExhausted = TRUE;
    
                            FloppyDump(
                                FLOPINFO,
                                ("Floppy: Unrecognized media.\n")
                                );
                        }
    
                    } else {
    
                        if (IsNEC_98) {
                            //
                            // Read boot sector by current media type's parameters to determine.
                            //
    
                            DisketteExtension->MediaType = driveMediaConstants->MediaType;
    
                            DisketteExtension->BytesPerSector =
                                driveMediaConstants->BytesPerSector;
    
                            FloppyDump(
                                FLOPINFO,
                                ("Floppy: MediaType is %x ---\n", DisketteExtension->MediaType)
                                );
    
                            DisketteExtension->ByteCapacity =
                                ( driveMediaConstants->BytesPerSector ) *
                                driveMediaConstants->SectorsPerTrack *
                                ( 1 + driveMediaConstants->MaximumTrack ) *
                                driveMediaConstants->NumberOfHeads;
    
                            //
                            // Structure copy the media constants into the diskette extension.
                            //
    
                            DisketteExtension->DriveMediaConstants =
                                DriveMediaConstants[DisketteExtension->DriveMediaType];
    
                            //
                            // Check the boot sector for any overriding geometry information.
                            //
                            //FlCheckBootSector(DisketteExtension);
    
                            // Set up the IRP to read the boot sector.
    
                            bootSector = ExAllocatePool(NonPagedPoolCacheAligned, BOOT_SECTOR_SIZE);
                            if (!bootSector) {
                                return STATUS_INSUFFICIENT_RESOURCES;
                            }
    
                            offset.LowPart = offset.HighPart = 0;
                            irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                                                                DisketteExtension->DeviceObject,
                                                                bootSector,
                                                                BOOT_SECTOR_SIZE,
                                                                &offset,
                                                                NULL);
                            if (!irp) {
                                FloppyDump(
                                    FLOPWARN,
                                    ( "Floppy: Returned from IoBuildAsynchronousFsdRequest with error\n" ));
                                ExFreePool(bootSector);
                                return STATUS_INSUFFICIENT_RESOURCES;
                            }
                            irp->CurrentLocation--;
                            irp->Tail.Overlay.CurrentStackLocation = IoGetNextIrpStackLocation(irp);
    
    
                            // Allocate an adapter channel, do read, free adapter channel.
    
                            ntStatus = FlReadWrite(DisketteExtension, irp, TRUE);
    
                            FloppyDump(
                                FLOPSHOW,
                                ( "Floppy: Read boot sector (called FlReadWrite) with status=%x\n" ,
                                ntStatus ));
    
                            MmUnlockPages(irp->MdlAddress);
                            IoFreeMdl(irp->MdlAddress);
                            IoFreeIrp(irp);
                            ExFreePool(bootSector);
    
                            if ( !NT_SUCCESS( ntStatus ) ) {
    
                                FloppyDump(
                                    FLOPINFO,
                                    ("Floppy: READID failed trying lower media\n"
                                     "------  Status = %x\n"
                                     "------  SecLen = %x\n"
                                     "------  SR0 = %x\n"
                                     "------  SR1 = %x\n"
                                     "------  SR2 = %x\n"
                                     "------  SR6 = %x\n",
                                     ntStatus,
                                     sectorLengthCode,
                                     DisketteExtension->FifoBuffer[0],
                                     DisketteExtension->FifoBuffer[1],
                                     DisketteExtension->FifoBuffer[2],
                                     DisketteExtension->FifoBuffer[6])
                                    );
    
                                DisketteExtension->DriveMediaType--;
    
                                DisketteExtension->DriveMediaConstants =
                                    DriveMediaConstants[DisketteExtension->DriveMediaType];
    
                                if (ntStatus != STATUS_DEVICE_NOT_READY) {
    
                                    ntStatus = STATUS_UNRECOGNIZED_MEDIA;
    
                                }
    
                                //
                                // Next comparison must be signed, for when
                                // LowestDriveMediaType = 0.
                                //
    
                                if ( (CHAR)( DisketteExtension->DriveMediaType ) <
                                    (CHAR)( DriveMediaLimits[DisketteExtension->DriveType].
                                    LowestDriveMediaType ) ) {
    
                                    DisketteExtension->MediaType = Unknown;
                                    mediaTypesExhausted = TRUE;
    
                                    FloppyDump(
                                        FLOPINFO,
                                        ("Floppy: Unrecognized media. 2\n")
                                        );
    
                                }
                            }
                        } // (IsNEC_98)
                    }
                }
    
            } while ( ( !NT_SUCCESS( ntStatus ) ) && !( mediaTypesExhausted ) );
    
            if (NT_SUCCESS(ntStatus)) {
    
                //
                // We determined the media type.  Time to move on.
                //
    
                FloppyDump(
                    FLOPINFO,
                    ("Floppy: Determined media type %d\n", retries)
                    );
                break;
            }
        }
    
#ifdef TOSHIBAJ
    //
    // Toshiba 3 mode floppy driver may recognize the 1.4MB media under next 
    // conditions.
    //      (1)retries is 1
    //      (2)mediaTypeExhausted is TRUE
    //      (3)Last ntStatus is SUCCESSFULL
    // Then we MUST DO this procedure to recognize the 1.4MB media type.
    //
    if(NT_SUCCESS( ntStatus ) && mediaTypesExhausted) {
        ;
    } else {
#endif
        if ( (!NT_SUCCESS( ntStatus )) || mediaTypesExhausted) {
    
            FloppyDump(
                FLOPINFO,
                ("Floppy: failed determine types status = %x %s\n",
                 ntStatus,
                 mediaTypesExhausted ? "media types exhausted" : "")
                );
            return ntStatus;
        }
#ifdef TOSHIBAJ  // for above Toshiba IF-statement
    }
#endif
    
        DisketteExtension->MediaType = driveMediaConstants->MediaType;
        DisketteExtension->BytesPerSector = driveMediaConstants->BytesPerSector;
    
        DisketteExtension->ByteCapacity =
            ( driveMediaConstants->BytesPerSector ) *
            driveMediaConstants->SectorsPerTrack *
            ( 1 + driveMediaConstants->MaximumTrack ) *
            driveMediaConstants->NumberOfHeads;
    
        FloppyDump(
            FLOPINFO,
            ("FlDetermineMediaType: MediaType is %x, bytes per sector %d, capacity %d\n",
             DisketteExtension->MediaType,
             DisketteExtension->BytesPerSector,
             DisketteExtension->ByteCapacity)
            );
        //
        // Structure copy the media constants into the diskette extension.
        //
    
        DisketteExtension->DriveMediaConstants =
            DriveMediaConstants[DisketteExtension->DriveMediaType];
    
#ifdef TOSHIBAJ
        {   NTSTATUS    sectorReadStatus;
            //
            // Check the boot sector for any overriding geometry information.
            // if boot sector cannot read by this settings, retry again.
            //
            sectorReadStatus = FlCheckBootSector(DisketteExtension);
            if( !NT_SUCCESS(sectorReadStatus) ) {
                retries++;
                mediaTypesExhausted = FALSE;
                goto    TryAgainLowerMedia;
            }
        }
#else

        //
        // Check the boot sector for any overriding geometry information.
        //
        FlCheckBootSector(DisketteExtension);
    
#endif

        return ntStatus;
    }
    
    VOID
    FlAllocateIoBuffer(
        IN OUT  PDISKETTE_EXTENSION DisketteExtension,
        IN      ULONG               BufferSize
        )
    
    /*++
    
    Routine Description:
    
        This routine allocates a PAGE_SIZE io buffer.
    
    Arguments:
    
        ControllerData      - Supplies the controller data.
    
        BufferSize          - Supplies the number of bytes to allocate.
    
    Return Value:
    
        None.
    
    --*/
    
    {
        BOOLEAN         allocateContiguous;
        LARGE_INTEGER   maxDmaAddress;
    
        if (DisketteExtension->IoBuffer) {
            if (DisketteExtension->IoBufferSize >= BufferSize) {
                return;
            }
            FlFreeIoBuffer(DisketteExtension);
        }
    
        if (BufferSize > DisketteExtension->MaxTransferSize ) {
            allocateContiguous = TRUE;
        } else {
            allocateContiguous = FALSE;
        }
    
        if (allocateContiguous) {
            maxDmaAddress.QuadPart = MAXIMUM_DMA_ADDRESS;
            DisketteExtension->IoBuffer = MmAllocateContiguousMemory(BufferSize,
                                                                  maxDmaAddress);
        } else {
            DisketteExtension->IoBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                                      BufferSize);
        }
    
        if (!DisketteExtension->IoBuffer) {
            return;
        }
    
        DisketteExtension->IoBufferMdl = IoAllocateMdl(DisketteExtension->IoBuffer,
                                                    BufferSize, FALSE, FALSE, NULL);
        if (!DisketteExtension->IoBufferMdl) {
            if (allocateContiguous) {
                MmFreeContiguousMemory(DisketteExtension->IoBuffer);
            } else {
                ExFreePool(DisketteExtension->IoBuffer);
            }
            DisketteExtension->IoBuffer = NULL;
            return;
        }
    
        MmProbeAndLockPages(DisketteExtension->IoBufferMdl, KernelMode,
                            IoModifyAccess);
    
        DisketteExtension->IoBufferSize = BufferSize;
    }
    
    VOID
    FlFreeIoBuffer(
        IN OUT  PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine free's the controller's IoBuffer.
    
    Arguments:
    
        DisketteExtension      - Supplies the controller data.
    
    Return Value:
    
        None.
    
    --*/
    
    {
        BOOLEAN contiguousBuffer;
    
        if (!DisketteExtension->IoBuffer) {
            return;
        }
    
        if (DisketteExtension->IoBufferSize >
            DisketteExtension->MaxTransferSize) {
    
            contiguousBuffer = TRUE;
        } else {
            contiguousBuffer = FALSE;
        }
    
        DisketteExtension->IoBufferSize = 0;
    
        MmUnlockPages(DisketteExtension->IoBufferMdl);
        IoFreeMdl(DisketteExtension->IoBufferMdl);
        DisketteExtension->IoBufferMdl = NULL;
        if (contiguousBuffer) {
            MmFreeContiguousMemory(DisketteExtension->IoBuffer);
        } else {
            ExFreePool(DisketteExtension->IoBuffer);
        }
        DisketteExtension->IoBuffer = NULL;
    }
    
    VOID
    FloppyThread(
        PVOID Context
        )
    
    /*++
    
    Routine Description:
    
        This is the code executed by the system thread created when the
        floppy driver initializes.  This thread loops forever (or until a
        flag is set telling the thread to kill itself) processing packets
        put into the queue by the dispatch routines.
    
        For each packet, this thread calls appropriate routines to process
        the request, and then calls FlFinishOperation() to complete the
        packet.
    
    Arguments:
    
        Context - a pointer to our data area for the controller being
        supported (there is one thread per controller).
    
    Return Value:
    
        None.
    
    --*/
    
    {
        PIRP irp;
        PIO_STACK_LOCATION irpSp;
        PLIST_ENTRY request;
        PDISKETTE_EXTENSION disketteExtension = Context;
        NTSTATUS ntStatus = STATUS_SUCCESS;
        NTSTATUS waitStatus;
        LARGE_INTEGER queueWait;
        LARGE_INTEGER acquireWait;
    
    
        //
        // Set thread priority to lowest realtime level.
        //
    
        KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
    
        queueWait.QuadPart = -(3 * 1000 * 10000);
        acquireWait.QuadPart = -(15 * 1000 * 10000);
    
        do {
    
            //
            // Wait for a request from the dispatch routines.
            // KeWaitForSingleObject won't return error here - this thread
            // isn't alertable and won't take APCs, and we're not passing in
            // a timeout.
            //
    
            waitStatus = KeWaitForSingleObject(
                (PVOID) &disketteExtension->RequestSemaphore,
                Executive,
                KernelMode,
                FALSE,
                &queueWait );
    
            if (waitStatus == STATUS_TIMEOUT) {
    
#ifdef TOSHIBA
        /* 98/9/3   Removed flag control
         */
                if( disketteExtension->Removed == FALSE )
                {
#endif
                if (disketteExtension->FloppyControllerAllocated) {
    
                   FloppyDump(FLOPSHOW,
                             ("Floppy: Timed Out - Turning off the motor\n")
                             );
                    FlFdcDeviceIo( disketteExtension->TargetObject,
                                   IOCTL_DISK_INTERNAL_DISABLE_FDC_DEVICE,
                                   NULL );
    
                    FlFdcDeviceIo( disketteExtension->TargetObject,
                                   IOCTL_DISK_INTERNAL_RELEASE_FDC,
                                   disketteExtension->DeviceObject );
    
                    disketteExtension->FloppyControllerAllocated = FALSE;
    
                }
    
#ifdef TOSHIBA
                }
#endif
                ExAcquireFastMutex(&disketteExtension->ThreadReferenceMutex);
    
                if (disketteExtension->ThreadReferenceCount == 0) {
                    disketteExtension->ThreadReferenceCount = -1;
    
                    ExAcquireFastMutex(PagingMutex);
                    if (--PagingReferenceCount == 0) {
                        MmPageEntireDriver(DriverEntry);
                    }
                    ExReleaseFastMutex(PagingMutex);
    
                    ExReleaseFastMutex(&disketteExtension->ThreadReferenceMutex);
                    PsTerminateSystemThread( STATUS_SUCCESS );
                }
    
                ExReleaseFastMutex(&disketteExtension->ThreadReferenceMutex);
                continue;
            }
    
            while (request = ExInterlockedRemoveHeadList(
                    &disketteExtension->ListEntry,
                    &disketteExtension->ListSpinLock)) {
    
                ExAcquireFastMutex(&disketteExtension->ThreadReferenceMutex);
                ASSERT(disketteExtension->ThreadReferenceCount > 0);
                (disketteExtension->ThreadReferenceCount)--;
                ExReleaseFastMutex(&disketteExtension->ThreadReferenceMutex);
    
                disketteExtension->HardwareFailed = FALSE;
    
                irp = CONTAINING_RECORD( request, IRP, Tail.Overlay.ListEntry );
    
                irpSp = IoGetCurrentIrpStackLocation( irp );
#ifdef TOSHIBA
    /* 98/9/3   Removed flag control
     */
                if( disketteExtension->Removed )
                {
                    ntStatus = STATUS_CANCELLED;
                    goto    Removed_flag_set;
                }
#endif
    
                FloppyDump(
                    FLOPIRPPATH,
                    ("Floppy: Starting  up IRP: %p for extension %p\n",
                      irp,irpSp->Parameters.Others.Argument4)
                    );
                switch ( irpSp->MajorFunction ) {
    
                    case IRP_MJ_READ:
                    case IRP_MJ_WRITE: {
    
                        //
                        // Get the diskette extension from where it was hidden
                        // in the IRP.
                        //
    
                        disketteExtension = (PDISKETTE_EXTENSION)
                            irpSp->Parameters.Others.Argument4;
    
                        if (!disketteExtension->FloppyControllerAllocated) {
    
                            ntStatus = FlFdcDeviceIo(
                                            disketteExtension->TargetObject,
                                            IOCTL_DISK_INTERNAL_ACQUIRE_FDC,
                                            &acquireWait );
    
                            if (NT_SUCCESS(ntStatus)) {
                                disketteExtension->FloppyControllerAllocated = TRUE;
                            } else {
                                break;
                            }
                        }
    
                        //
                        // Until the file system clears the DO_VERIFY_VOLUME
                        // flag, we should return all requests with error.
                        //
    
                        if (( disketteExtension->DeviceObject->Flags &
                                DO_VERIFY_VOLUME )  &&
                             !(irpSp->Flags & SL_OVERRIDE_VERIFY_VOLUME))
                                    {
    
                            FloppyDump(
                                FLOPINFO,
                                ("Floppy: clearing queue; verify required\n")
                                );
    
                            //
                            // The disk changed, and we set this bit.  Fail
                            // all current IRPs for this device; when all are
                            // returned, the file system will clear
                            // DO_VERIFY_VOLUME.
                            //
    
                            ntStatus = STATUS_VERIFY_REQUIRED;
    
                        } else {
    
                            ntStatus = FlReadWrite( disketteExtension, irp, FALSE );
    
                        }
    
                        break;
                    }
    
                    case IRP_MJ_DEVICE_CONTROL: {
    
                        disketteExtension = (PDISKETTE_EXTENSION)
                            irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    
                        if (!disketteExtension->FloppyControllerAllocated) {
    
                            ntStatus = FlFdcDeviceIo(
                                            disketteExtension->TargetObject,
                                            IOCTL_DISK_INTERNAL_ACQUIRE_FDC,
                                            &acquireWait );
    
                            if (NT_SUCCESS(ntStatus)) {
                                disketteExtension->FloppyControllerAllocated = TRUE;
                            } else {
                                break;
                            }
                        }
                        //
                        // Until the file system clears the DO_VERIFY_VOLUME
                        // flag, we should return all requests with error.
                        //
    
                        if (( disketteExtension->DeviceObject->Flags &
                                DO_VERIFY_VOLUME )  &&
                             !(irpSp->Flags & SL_OVERRIDE_VERIFY_VOLUME))
                                    {
    
                            FloppyDump(
                                FLOPINFO,
                                ("Floppy: clearing queue; verify required\n")
                                );
    
                            //
                            // The disk changed, and we set this bit.  Fail
                            // all current IRPs; when all are returned, the
                            // file system will clear DO_VERIFY_VOLUME.
                            //
    
                            ntStatus = STATUS_VERIFY_REQUIRED;
    
                        } else {
    
                            switch (
                                irpSp->Parameters.DeviceIoControl.IoControlCode ) {
    
                                case IOCTL_STORAGE_CHECK_VERIFY:
                                case IOCTL_DISK_CHECK_VERIFY: {
    
                                    //
                                    // Just start the drive; it will
                                    // automatically check whether or not the
                                    // disk has been changed.
                                    //
    
                                    FloppyDump(
                                        FLOPSHOW,
                                        ("Floppy: IOCTL_DISK_CHECK_VERIFY called\n")
                                        );
    
                                    ntStatus = FlStartDrive(
                                        disketteExtension,
                                        irp,
                                        FALSE,
                                        FALSE,
                                        FALSE);
    
                                    break;
                                }
    
                                case IOCTL_DISK_IS_WRITABLE: {
    
                                    //
                                    // Start the drive with the WriteOperation
                                    // flag set to TRUE.
                                    //
    
                                    FloppyDump(
                                        FLOPSHOW,
                                        ("Floppy: IOCTL_DISK_IS_WRITABLE called\n")
                                        );
    
                                    if (disketteExtension->IsReadOnly) {
    
                                        ntStatus = STATUS_INVALID_PARAMETER;
    
                                    } else {
    
                                        ntStatus = FlStartDrive(
                                            disketteExtension,
                                            irp,
                                            TRUE,
                                            FALSE,
                                            TRUE);
                                    }
    
                                    break;
                                }
    
                                case IOCTL_DISK_GET_DRIVE_GEOMETRY: {
    
                                    FloppyDump(
                                        FLOPSHOW,
                                        ("Floppy: IOCTL_DISK_GET_DRIVE_GEOMETRY\n")
                                        );
    
                                    //
                                    // If there's enough room to write the
                                    // data, start the drive to make sure we
                                    // know what type of media is in the drive.
                                    //
    
                                    if ( irpSp->Parameters.DeviceIoControl.
                                        OutputBufferLength <
                                        sizeof( DISK_GEOMETRY ) ) {
    
                                        ntStatus = STATUS_INVALID_PARAMETER;
    
                                    } else {
    
                                        ntStatus = FlStartDrive(
                                            disketteExtension,
                                            irp,
                                            FALSE,
                                            TRUE,
                                            (BOOLEAN)!!(irpSp->Flags &
                                                SL_OVERRIDE_VERIFY_VOLUME));
    
                                    }
    
                                    //
                                    // If the media wasn't formatted, FlStartDrive
                                    // returned STATUS_UNRECOGNIZED_MEDIA.
                                    //
    
                                    if ( NT_SUCCESS( ntStatus ) ||
                                        ( ntStatus == STATUS_UNRECOGNIZED_MEDIA )) {
    
                                        PDISK_GEOMETRY outputBuffer =
                                            (PDISK_GEOMETRY)
                                            irp->AssociatedIrp.SystemBuffer;
    
                                        // Always return the media type, even if
                                        // it's unknown.
                                        //
    
                                        ntStatus = STATUS_SUCCESS;
    
                                        outputBuffer->MediaType =
                                            disketteExtension->MediaType;
    
                                        //
                                        // The rest of the fields only have meaning
                                        // if the media type is known.
                                        //
    
                                        if ( disketteExtension->MediaType ==
                                            Unknown ) {
    
                                            FloppyDump(
                                                FLOPSHOW,
                                                ("Floppy: geometry unknown\n")
                                                );
    
                                            //
                                            // Just zero out everything.  The
                                            // caller shouldn't look at it.
                                            //
    
                                            outputBuffer->Cylinders.LowPart = 0;
                                            outputBuffer->Cylinders.HighPart = 0;
                                            outputBuffer->TracksPerCylinder = 0;
                                            outputBuffer->SectorsPerTrack = 0;
                                            outputBuffer->BytesPerSector = 0;
    
                                        } else {
    
                                            //
                                            // Return the geometry of the current
                                            // media.
                                            //
    
                                            FloppyDump(
                                                FLOPSHOW,
                                                ("Floppy: geomentry is known\n")
                                                );
                                            outputBuffer->Cylinders.LowPart =
                                                disketteExtension->
                                                DriveMediaConstants.MaximumTrack + 1;
    
                                            outputBuffer->Cylinders.HighPart = 0;
    
                                            outputBuffer->TracksPerCylinder =
                                                disketteExtension->
                                                DriveMediaConstants.NumberOfHeads;
    
                                            outputBuffer->SectorsPerTrack =
                                                disketteExtension->
                                                DriveMediaConstants.SectorsPerTrack;
    
                                            outputBuffer->BytesPerSector =
                                                disketteExtension->
                                                DriveMediaConstants.BytesPerSector;
                                        }
    
                                        FloppyDump(
                                            FLOPSHOW,
                                            ("Floppy: Geometry\n"
                                             "------- Cylinders low:  0x%x\n"
                                             "------- Cylinders high: 0x%x\n"
                                             "------- Track/Cyl:      0x%x\n"
                                             "------- Sectors/Track:  0x%x\n"
                                             "------- Bytes/Sector:   0x%x\n"
                                             "------- Media Type:       %d\n",
                                             outputBuffer->Cylinders.LowPart,
                                             outputBuffer->Cylinders.HighPart,
                                             outputBuffer->TracksPerCylinder,
                                             outputBuffer->SectorsPerTrack,
                                             outputBuffer->BytesPerSector,
                                             outputBuffer->MediaType)
                                             );
    
                                    }
    
                                    irp->IoStatus.Information =
                                        sizeof( DISK_GEOMETRY );
    
                                    break;
                                }
    
                                case IOCTL_DISK_FORMAT_TRACKS_EX:
                                case IOCTL_DISK_FORMAT_TRACKS: {
    
                                    FloppyDump(
                                        FLOPSHOW,
                                        ("Floppy: IOCTL_DISK_FORMAT_TRACKS\n")
                                        );
    
                                    //
                                    // Start the drive, and make sure it's not
                                    // write protected.
                                    //
    
                                    ntStatus = FlStartDrive(
                                        disketteExtension,
                                        irp,
                                        TRUE,
                                        FALSE,
                                        FALSE );
    
                                    //
                                    // Note that FlStartDrive could have returned
                                    // STATUS_UNRECOGNIZED_MEDIA if the drive
                                    // wasn't formatted.
                                    //
    
                                    if ( NT_SUCCESS( ntStatus ) ||
                                        ( ntStatus == STATUS_UNRECOGNIZED_MEDIA)) {
    
                                        //
                                        // We need a single page to do FORMATs.
                                        // If we already allocated a buffer,
                                        // we'll use that.  If not, let's
                                        // allocate a single page.  Note that
                                        // we'd have to do this anyway if there's
                                        // not enough map registers.
                                        //
    
                                        FlAllocateIoBuffer( disketteExtension,
                                                            PAGE_SIZE);
    
                                        if (disketteExtension->IoBuffer) {
                                            ntStatus = FlFormat(disketteExtension,
                                                                irp);
                                        } else {
                                            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                                        }
                                    }
    
                                    break;
                                }                              //end of case format
    
                               case IOCTL_DISK_SENSE_DEVICE: {
    
                                    if (IsNEC_98) {
                                        disketteExtension->FifoBuffer[0] = COMMND_SENSE_DRIVE_STATUS;
                                        disketteExtension->FifoBuffer[1] = disketteExtension->DeviceUnit;
    
                                        ntStatus = FlIssueCommand( disketteExtension,
                                                                   disketteExtension->FifoBuffer,
                                                                   disketteExtension->FifoBuffer,
                                                                   NULL,
                                                                   0,
                                                                   0 );
    
                                       if ( NT_SUCCESS( ntStatus ) ) {
    
                                           PSENSE_DEVISE_STATUS_PTOS outputBuffer
                                                =(PSENSE_DEVISE_STATUS_PTOS)irp->AssociatedIrp.SystemBuffer;
                                           ((PSENSE_DEVISE_STATUS_PTOS)outputBuffer)->ST3_PTOS
                                                =Result_Status3_PTOS[0].ST3_PTOS;
    
                                           irp->IoStatus.Information = sizeof( SENSE_DEVISE_STATUS_PTOS );
                                       }
                                       break;
                                    }
                               }                        //end of case sense device
    
                            }                           //end of switch controlcode
                        }
    
                        break;
                    }                                           //end of case IOCTL
    
                    default: {
    
                        FloppyDump(
                            FLOPDBGP,
                            ("Floppy: bad majorfunction %x\n",irpSp->MajorFunction)
                            );
    
                        ntStatus = STATUS_NOT_IMPLEMENTED;
                    }
    
                }                                  //end of switch on majorfunction
    
#ifdef TOSHIBA
            Removed_flag_set:
#endif
    
                if (ntStatus == STATUS_DEVICE_BUSY) {
    
                    // If the status is DEVICE_BUSY then this indicates that the
                    // qic117 has control of the controller.  Therefore complete
                    // all remaining requests with STATUS_DEVICE_BUSY.
    
                    for (;;) {
    
                        disketteExtension->HardwareFailed = FALSE;
    
                        irp->IoStatus.Status = STATUS_DEVICE_BUSY;
    
                        IoCompleteRequest(irp, IO_DISK_INCREMENT);
    
                        request = ExInterlockedRemoveHeadList(
                            &disketteExtension->ListEntry,
                            &disketteExtension->ListSpinLock );
    
                        if (!request) {
                            break;
                        }
    
                        ExAcquireFastMutex(
                            &disketteExtension->ThreadReferenceMutex);
                        ASSERT(disketteExtension->ThreadReferenceCount > 0);
                        (disketteExtension->ThreadReferenceCount)--;
                        ExReleaseFastMutex(
                            &disketteExtension->ThreadReferenceMutex);
    
                        irp = CONTAINING_RECORD( request,
                                                 IRP,
                                                 Tail.Overlay.ListEntry);
                    }
    
                } else {
    
                    //
                    // All operations leave a final status in ntStatus.  Copy it
                    // to the IRP, and then complete the operation.
                    //
    
                    irp->IoStatus.Status = ntStatus;
    
                    FlFinishOperation( irp, disketteExtension );
    
                }
    
            } // while there are packets to process
    
        } while ( TRUE );
    }
    
    VOID
    FlConsolidateMediaTypeWithBootSector(
        IN OUT  PDISKETTE_EXTENSION DisketteExtension,
        IN      PBOOT_SECTOR_INFO   BootSector
        )
    
    /*++
    
    Routine Description:
    
        This routine adjusts the DisketteExtension data according
        to the BPB values if this is appropriate.
    
    Arguments:
    
        DisketteExtension   - Supplies the diskette extension.
    
        BootSector          - Supplies the boot sector information.
    
    Return Value:
    
        None.
    
    --*/
    
    {
        USHORT                  bpbNumberOfSectors, bpbNumberOfHeads;
        USHORT                  bpbSectorsPerTrack, bpbBytesPerSector;
        USHORT                  bpbMediaByte, bpbMaximumTrack;
        MEDIA_TYPE              bpbMediaType;
        ULONG                   i, n;
        PDRIVE_MEDIA_CONSTANTS  readidDriveMediaConstants;
        BOOLEAN                 changeToBpbMedia;
    
        FloppyDump(
            FLOPSHOW,
            ("Floppy: First sector read: media descriptor is: 0x%x\n",
             BootSector->MediaByte[0])
            );
    
        if (BootSector->JumpByte[0] != 0xeb &&
            BootSector->JumpByte[0] != 0xe9) {
    
            // This is not a formatted floppy so ignore the BPB.
            return;
        }
    
        bpbNumberOfSectors = BootSector->NumberOfSectors[1]*0x100 +
                             BootSector->NumberOfSectors[0];
        bpbNumberOfHeads = BootSector->NumberOfHeads[1]*0x100 +
                           BootSector->NumberOfHeads[0];
        bpbSectorsPerTrack = BootSector->SectorsPerTrack[1]*0x100 +
                             BootSector->SectorsPerTrack[0];
        bpbBytesPerSector = BootSector->BytesPerSector[1]*0x100 +
                            BootSector->BytesPerSector[0];
        bpbMediaByte = BootSector->MediaByte[0];
    
        if (!bpbNumberOfHeads || !bpbSectorsPerTrack) {
            // Invalid BPB, avoid dividing by zero.
            return;
        }
    
        bpbMaximumTrack =
            bpbNumberOfSectors/bpbNumberOfHeads/bpbSectorsPerTrack - 1;
    
#ifdef TOSHIBAJ
        //
        // First compare ReadIdMedia with BPB.
        // If the parameters are the same, use ReadIdMedia type.
        //
        // When 3.5" 1.2M disk is checked,
        // the original source code mistakes the media as 5" 1.2M disk.
        //
        readidDriveMediaConstants = &(DisketteExtension->DriveMediaConstants);
    
        if (bpbBytesPerSector == readidDriveMediaConstants->BytesPerSector &&
            bpbSectorsPerTrack == readidDriveMediaConstants->SectorsPerTrack &&
            bpbMaximumTrack == readidDriveMediaConstants->MaximumTrack &&
            bpbNumberOfHeads == readidDriveMediaConstants->NumberOfHeads &&
            bpbMediaByte == readidDriveMediaConstants->MediaByte) {
                bpbMediaType = readidDriveMediaConstants->MediaType;
        } else {
    
#endif

        // First figure out if this BPB specifies a known media type
        // independantly of the current drive type.
    
        bpbMediaType = Unknown;
        for (i = 0; i < NUMBER_OF_DRIVE_MEDIA_COMBINATIONS; i++) {
    
            if (bpbBytesPerSector == DriveMediaConstants[i].BytesPerSector &&
                bpbSectorsPerTrack == DriveMediaConstants[i].SectorsPerTrack &&
                bpbMaximumTrack == DriveMediaConstants[i].MaximumTrack &&
                bpbNumberOfHeads == DriveMediaConstants[i].NumberOfHeads &&
                bpbMediaByte == DriveMediaConstants[i].MediaByte) {
    
                bpbMediaType = DriveMediaConstants[i].MediaType;
    
    // Sep.19.1996 KIADP005 bpbMediaType is not suitable for DriveType
#ifdef TOSHIBAJ
                if (DisketteExtension->DriveType == DRIVE_TYPE_1440) {
                    FloppyDump(
                        FLOPSHOW,
                        ("FLOPPY: bpbMediaType is %x, DriveType is %x\n",
                         bpbMediaType, DRIVE_TYPE_1440)
                    );
    
                    if (bpbMediaType == F5_1Pt2_512)
                        bpbMediaType = F3_1Pt2_512;
                    else if (bpbMediaType == F5_1Pt23_1024)
                        bpbMediaType = F3_1Pt23_1024;
                }
#endif // TOSHIBAJ

                break;
            }
        }
#ifdef TOSHIBAJ
        }   // For above comparison
#endif

#ifdef TOSHIBAJ
        // skomi 1998-03-12  bpbMediaType is not suitable for DriveType
        if (bpbMediaType == F5_640_512){
            bpbMediaType = F3_640_512;
            FloppyDump(
                FLOPSHOW,
                ("FLOPPY: replace bpbMediaType from F5_640_512 to F3_640_512\n")
            );
        } else if (bpbMediaType == F5_720_512) {
            bpbMediaType = F3_720_512;
            FloppyDump(
                FLOPSHOW,
                ("FLOPPY: replace bpbMediaType from F5_720_512 to F3_720_512\n")
            );
        }
#endif
    
        FloppyDump(
            FLOPSHOW,
            ("FLOPPY: After switch media type is: %x\n",bpbMediaType)
            );
    
        FloppyDump(
            FLOPINFO,
            ("FloppyBpb: Media type ")
            );
        if (bpbMediaType == DisketteExtension->MediaType) {
    
            // No conflict between BPB and readId result.
    
            changeToBpbMedia = FALSE;
            FloppyDump(
                FLOPINFO,
                ("is same\n")
                );
    
        } else {
    
            // There is a conflict between the BPB and the readId
            // media type.  If the new parameters are acceptable
            // then go with them.
    
            readidDriveMediaConstants = &(DisketteExtension->DriveMediaConstants);
    
            if (bpbBytesPerSector == readidDriveMediaConstants->BytesPerSector &&
                bpbSectorsPerTrack < 0x100 &&
                bpbMaximumTrack == readidDriveMediaConstants->MaximumTrack &&
                bpbNumberOfHeads <= readidDriveMediaConstants->NumberOfHeads) {
    
                changeToBpbMedia = TRUE;
    
            } else {
                changeToBpbMedia = FALSE;
            }
    
            FloppyDump( FLOPINFO,
                        ("%s",
                        changeToBpbMedia ?
                        "will change to Bpb\n" : "will not change\n")
                        );
    
            // If we didn't derive a new media type from the BPB then
            // just use the one from readId.  Also override any
            // skew compensation since we don't really know anything
            // about this new media type.
    
            if (bpbMediaType == Unknown) {
                bpbMediaType = readidDriveMediaConstants->MediaType;
                DisketteExtension->DriveMediaConstants.SkewDelta = 0;
            }
        }
    
        if (changeToBpbMedia) {
    
            // Change the DriveMediaType only if this new media type
            // falls in line with what is supported by the drive.
    
            i = DriveMediaLimits[DisketteExtension->DriveType].LowestDriveMediaType;
            n = DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
            for (; i <= n; i++) {
    
                if (bpbMediaType == DriveMediaConstants[i].MediaType) {
                    DisketteExtension->DriveMediaType = i;
    
    // Sep.19.1996 KIADP004 DriveMediaType is changed by BPB information.
#ifdef TOSHIBAJ
                    FlDatarateSpecifyConfigure(DisketteExtension);
#endif // TOSHIBAJ
    
                    break;
                }
            }
    
            DisketteExtension->MediaType = bpbMediaType;
            DisketteExtension->ByteCapacity = bpbNumberOfSectors*bpbBytesPerSector;
            DisketteExtension->DriveMediaConstants.SectorsPerTrack =
                (UCHAR) bpbSectorsPerTrack;
            DisketteExtension->DriveMediaConstants.NumberOfHeads =
                (UCHAR) bpbNumberOfHeads;
    
            // If the MSDMF3. signature is there then make this floppy
            // read-only.
    
            if (RtlCompareMemory(BootSector->OemData, "MSDMF3.", 7) == 7) {
                DisketteExtension->IsReadOnly = TRUE;
            }
        }
    }
    
#ifdef TOSHIBAJ
    // for 3 mode support
    NTSTATUS
#else
    VOID
#endif
    FlCheckBootSector(
        IN OUT  PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine reads the boot sector and then figures
        out whether or not the boot sector contains new geometry
        information.
    
    Arguments:
    
        DisketteExtension   - Supplies the diskette extension.
    
    Return Value:
    
        None.
    
    --*/
    
    {
        PBOOT_SECTOR_INFO   bootSector;
        LARGE_INTEGER       offset;
        PIRP                irp;
        NTSTATUS            status;
    
    
        // Set up the IRP to read the boot sector.
    
        bootSector = ExAllocatePool(NonPagedPoolCacheAligned, BOOT_SECTOR_SIZE);
        if (!bootSector) {

#ifdef TOSHIBAJ
            return STATUS_INSUFFICIENT_RESOURCES;
#else
            return;
#endif
        }
    
        offset.LowPart = offset.HighPart = 0;
        irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                                            DisketteExtension->DeviceObject,
                                            bootSector,
                                            BOOT_SECTOR_SIZE,
                                            &offset,
                                            NULL);
        if (!irp) {
            ExFreePool(bootSector);

#ifdef TOSHIBAJ
            return STATUS_INSUFFICIENT_RESOURCES;
#else
            return;
#endif
        }
        irp->CurrentLocation--;
        irp->Tail.Overlay.CurrentStackLocation = IoGetNextIrpStackLocation(irp);
    
    
        // Allocate an adapter channel, do read, free adapter channel.
    
        status = FlReadWrite(DisketteExtension, irp, TRUE);
    
        MmUnlockPages(irp->MdlAddress);
        IoFreeMdl(irp->MdlAddress);
        IoFreeIrp(irp);
    
#ifdef TOSHIBAJ
        // BUG-DUMP
        // Check the Boot_Sector_Info if the current media type is suitable or not.
        // Because sometimes READ-ID command get the succesful even if the media
        // type is mismatched,
        // We should check the medeia type withboot sector information.
        {
            USHORT  *sectorLength;
            USHORT  *trackLength;
            PDRIVE_MEDIA_CONSTANTS  driveMediaConstants;
    
            sectorLength = (USHORT *)&bootSector->BytesPerSector;
            trackLength  = (USHORT *)&bootSector->SectorsPerTrack;
            driveMediaConstants = &DisketteExtension->DriveMediaConstants;
            FloppyDump(FLOPINFO,
                ("FLOPPY: FlCheckbootsector BootSector(BytesPerSector=%d SectorsPerTrack:%d)\n",
                         *sectorLength,
                         *trackLength)
            );
            FloppyDump(FLOPINFO,
                ("FLOPPY: FlCheckbootsector MediaContants(BytesPerSector=%d SectorsPerTrack:%d)\n",
                         driveMediaConstants->BytesPerSector,
                         driveMediaConstants->SectorsPerTrack)
            );
    
        }
#endif

        ExFreePool(bootSector);
    
#ifdef TOSHIBAJ
        // for 3 mode support
        return status;
#endif
    }
    
    NTSTATUS
    FlReadWriteTrack(
        IN OUT  PDISKETTE_EXTENSION DisketteExtension,
        IN OUT  PMDL                IoMdl,
        IN OUT  ULONG               IoOffset,
        IN      BOOLEAN             WriteOperation,
        IN      UCHAR               Cylinder,
        IN      UCHAR               Head,
        IN      UCHAR               Sector,
        IN      UCHAR               NumberOfSectors,
        IN      BOOLEAN             NeedSeek
        )
    
    /*++
    
    Routine Description:
    
        This routine reads a portion of a track.  It transfers the to or from the
        device from or to the given IoBuffer and IoMdl.
    
    Arguments:
    
        DisketteExtension   - Supplies the diskette extension.
    
        IoMdl               - Supplies the Mdl for transfering from/to the device.
    
        IoBuffer            - Supplies the buffer to transfer from/to the device.
    
        WriteOperation      - Supplies whether or not this is a write operation.
    
        Cylinder            - Supplies the cylinder number for this track.
    
        Head                - Supplies the head number for this track.
    
        Sector              - Supplies the starting sector of the transfer.
    
        NumberOfSectors     - Supplies the number of sectors to transfer.
    
        NeedSeek            - Supplies whether or not we need to do a seek.
    
    Return Value:
    
        An NTSTATUS code.
    
    --*/
    
    {
        PDRIVE_MEDIA_CONSTANTS  driveMediaConstants;
        ULONG                   byteToSectorShift;
        ULONG                   transferBytes;
        LARGE_INTEGER           headSettleTime;
        NTSTATUS                status;
        ULONG                   seekRetry, ioRetry;
        BOOLEAN                 recalibrateDrive = FALSE;
        UCHAR                   i;
    
        FloppyDump( FLOPSHOW,
                    ("\nFlReadWriteTrack:%sseek for %s at chs %d/%d/%d for %d sectors\n",
                    NeedSeek ? " need " : " ",
                    WriteOperation ? "write" : "read",
                    Cylinder,
                    Head,
                    Sector,
                    NumberOfSectors) );
    
        driveMediaConstants = &DisketteExtension->DriveMediaConstants;
        byteToSectorShift = SECTORLENGTHCODE_TO_BYTESHIFT +
                            driveMediaConstants->SectorLengthCode;
        transferBytes = ((ULONG) NumberOfSectors)<<byteToSectorShift;
    
        headSettleTime.LowPart = -(10*1000*driveMediaConstants->HeadSettleTime);
        headSettleTime.HighPart = -1;
    
        for (seekRetry = 0, ioRetry = 0; seekRetry < 3; seekRetry++) {
    
            if (recalibrateDrive) {
    
                // Something failed, so recalibrate the drive.
    
                FloppyDump(
                    FLOPINFO,
                    ("FlReadWriteTrack: performing recalibrate\n")
                    );
                FlRecalibrateDrive(DisketteExtension);
            }
    
            // Do a seek if we have to.
    
            if (recalibrateDrive ||
                (NeedSeek &&
                 (!DisketteExtension->ControllerConfigurable ||
                  driveMediaConstants->CylinderShift != 0))) {
    
                DisketteExtension->FifoBuffer[0] = COMMND_SEEK;
                DisketteExtension->FifoBuffer[1] = (Head<<2) |
                                                DisketteExtension->DeviceUnit;
                DisketteExtension->FifoBuffer[2] = Cylinder<<
                                                driveMediaConstants->CylinderShift;
    
                status = FlIssueCommand( DisketteExtension,
                                         DisketteExtension->FifoBuffer,
                                         DisketteExtension->FifoBuffer,
                                         NULL,
                                         0,
                                         0 );
    
                if (NT_SUCCESS(status)) {
    
                    // Check the completion state of the controller.
    
                    if (!(DisketteExtension->FifoBuffer[0]&STREG0_SEEK_COMPLETE) ||
                        DisketteExtension->FifoBuffer[1] !=
                                Cylinder<<driveMediaConstants->CylinderShift) {
    
                        DisketteExtension->HardwareFailed = TRUE;
                        status = STATUS_FLOPPY_BAD_REGISTERS;
                    }
    
                    if (NT_SUCCESS(status)) {
    
                        // Delay after doing seek.
    
                        KeDelayExecutionThread(KernelMode, FALSE, &headSettleTime);
    
                        // SEEKs should always be followed by a READID.
    
                        DisketteExtension->FifoBuffer[0] =
                            COMMND_READ_ID + COMMND_OPTION_MFM;
                        DisketteExtension->FifoBuffer[1] =
                            (Head<<2) | DisketteExtension->DeviceUnit;
    
                        status = FlIssueCommand( DisketteExtension,
                                                 DisketteExtension->FifoBuffer,
                                                 DisketteExtension->FifoBuffer,
                                                 NULL,
                                                 0,
                                                 0 );
    
                        if (NT_SUCCESS(status)) {
    
                            if (IsNEC_98) {
                                if(DisketteExtension->FifoBuffer[0] & STREG0_DRIVE_NOT_READY) {
                                     return STATUS_DEVICE_NOT_READY;
                                }
                            } // (IsNEC_98)
    
                            if (DisketteExtension->FifoBuffer[0] !=
                                    ((Head<<2) | DisketteExtension->DeviceUnit) ||
                                DisketteExtension->FifoBuffer[1] != 0 ||
                                DisketteExtension->FifoBuffer[2] != 0 ||
                                DisketteExtension->FifoBuffer[3] != Cylinder) {
    
                                DisketteExtension->HardwareFailed = TRUE;
    
                                status = FlInterpretError(
                                            DisketteExtension->FifoBuffer[1],
                                            DisketteExtension->FifoBuffer[2]);
                            }
                        } else {
                            FloppyDump(
                                FLOPINFO,
                                ("FlReadWriteTrack: Read ID failed %x\n", status)
                                );
                        }
                    }
                } else {
                    FloppyDump(
                        FLOPINFO,
                        ("FlReadWriteTrack: SEEK failed %x\n", status)
                        );
                }
    
    
            } else {
                status = STATUS_SUCCESS;
            }
    
            if (!NT_SUCCESS(status)) {
    
                // The seek failed so try again.
    
                FloppyDump(
                    FLOPINFO,
                    ("FlReadWriteTrack: setup failure %x - recalibrating\n", status)
                    );
                recalibrateDrive = TRUE;
                continue;
            }
    
            for (;; ioRetry++) {
    
                //
                // Issue the READ or WRITE command
                //
    
                DisketteExtension->FifoBuffer[1] = (Head<<2) |
                                                DisketteExtension->DeviceUnit;
                DisketteExtension->FifoBuffer[2] = Cylinder;
                DisketteExtension->FifoBuffer[3] = Head;
                DisketteExtension->FifoBuffer[4] = Sector + 1;
                DisketteExtension->FifoBuffer[5] =
                        driveMediaConstants->SectorLengthCode;
                DisketteExtension->FifoBuffer[6] = Sector + NumberOfSectors;
                DisketteExtension->FifoBuffer[7] =
                        driveMediaConstants->ReadWriteGapLength;
                DisketteExtension->FifoBuffer[8] = driveMediaConstants->DataLength;
    
                if (WriteOperation) {
                    DisketteExtension->FifoBuffer[0] =
                        COMMND_WRITE_DATA + COMMND_OPTION_MFM;
                } else {
                    DisketteExtension->FifoBuffer[0] =
                        COMMND_READ_DATA + COMMND_OPTION_MFM;
                }
    
                status = FlIssueCommand( DisketteExtension,
                                         DisketteExtension->FifoBuffer,
                                         DisketteExtension->FifoBuffer,
                                         IoMdl,
                                         IoOffset,
                                         transferBytes );
    
                if (NT_SUCCESS(status)) {
    
                    if (IsNEC_98) {
                        if(DisketteExtension->FifoBuffer[0] & STREG0_DRIVE_NOT_READY) {
                             return STATUS_DEVICE_NOT_READY;
                        }
                    } // (IsNEC_98)
    
                    if ((DisketteExtension->FifoBuffer[0] & STREG0_END_MASK) !=
                            STREG0_END_NORMAL &&
                        ((DisketteExtension->FifoBuffer[0] & STREG0_END_MASK) !=
                            STREG0_END_ERROR ||
                         DisketteExtension->FifoBuffer[1] !=
                            STREG1_END_OF_DISKETTE ||
                         DisketteExtension->FifoBuffer[2] != STREG2_SUCCESS)) {
    
                        DisketteExtension->HardwareFailed = TRUE;
    
                        status = FlInterpretError(DisketteExtension->FifoBuffer[1],
                                                  DisketteExtension->FifoBuffer[2]);
                    } else {
                        //
                        // The floppy controller may return no errors but not have
                        // read all of the requested data.  If this is the case,
                        // record it as an error and retru the operation.
                        //
                        if (DisketteExtension->FifoBuffer[5] != 1) {
    
                            DisketteExtension->HardwareFailed = TRUE;
                            status = STATUS_FLOPPY_UNKNOWN_ERROR;
                        }
                    }
                } else {
                    FloppyDump( FLOPINFO,
                                ("FlReadWriteTrack: %s command failed %x\n",
                                WriteOperation ? "write" : "read",
                                status) );
                }
    
                if (NT_SUCCESS(status)) {
                    break;
                }
    
                if (ioRetry >= 2) {
                    FloppyDump(FLOPINFO,
                               ("FlReadWriteTrack: too many retries - failing\n"));
                    break;
                }
            }
    
            if (NT_SUCCESS(status)) {
                break;
            }
    
            // We failed quite a bit so make seeks mandatory.
            recalibrateDrive = TRUE;
        }
    
        if (!NT_SUCCESS(status) && NumberOfSectors > 1) {
    
            // Retry one sector at a time.
    
            FloppyDump( FLOPINFO,
                        ("FlReadWriteTrack: Attempting sector at a time\n") );
    
            for (i = 0; i < NumberOfSectors; i++) {
                status = FlReadWriteTrack( DisketteExtension,
                                           IoMdl,
                                           IoOffset+(((ULONG)i)<<byteToSectorShift),
                                           WriteOperation,
                                           Cylinder,
                                           Head,
                                           (UCHAR) (Sector + i),
                                           1,
                                           FALSE );
    
                if (!NT_SUCCESS(status)) {
                    FloppyDump( FLOPINFO,
                                ("FlReadWriteTrack: failed sector %d status %x\n",
                                i,
                                status) );
    
                    DisketteExtension->HardwareFailed = TRUE;
                    break;
                }
            }
        }
    
        return status;
    }
    
    NTSTATUS
    FlReadWrite(
        IN OUT PDISKETTE_EXTENSION DisketteExtension,
        IN OUT PIRP Irp,
        IN BOOLEAN DriveStarted
        )
    
    /*++
    
    Routine Description:
    
        This routine is called by the floppy thread to read/write data
        to/from the diskette.  It breaks the request into pieces called
        "transfers" (their size depends on the buffer size, where the end of
        the track is, etc) and retries each transfer until it succeeds or
        the retry count is exceeded.
    
    Arguments:
    
        DisketteExtension - a pointer to our data area for the drive to be
        accessed.
    
        Irp - a pointer to the IO Request Packet.
    
        DriveStarted - indicated whether or not the drive has been started.
    
    Return Value:
    
        STATUS_SUCCESS if the packet was successfully read or written; the
        appropriate error is propogated otherwise.
    
    --*/
    
    {
        PIO_STACK_LOCATION      irpSp;
        BOOLEAN                 writeOperation;
        NTSTATUS                status;
        PDRIVE_MEDIA_CONSTANTS  driveMediaConstants;
        ULONG                   byteToSectorShift;
        ULONG                   currentSector, firstSector, lastSector;
        ULONG                   trackSize;
        UCHAR                   sectorsPerTrack, numberOfHeads;
        UCHAR                   currentHead, currentCylinder, trackSector;
        PCHAR                   userBuffer;
        UCHAR                   skew, skewDelta;
        UCHAR                   numTransferSectors;
        PMDL                    mdl;
        PCHAR                   ioBuffer;
        ULONG                   ioOffset;
    
        irpSp = IoGetCurrentIrpStackLocation(Irp);
    
        FloppyDump(
            FLOPSHOW,
            ("FlReadWrite: for %s at offset %x size %x ",
             irpSp->MajorFunction == IRP_MJ_WRITE ? "write" : "read",
             irpSp->Parameters.Read.ByteOffset.LowPart,
             irpSp->Parameters.Read.Length)
            );
    
        // Check for valid operation on this device.
    
        if (irpSp->MajorFunction == IRP_MJ_WRITE) {
            if (DisketteExtension->IsReadOnly) {
                FloppyDump( FLOPSHOW, ("is read-only\n") );
                return STATUS_INVALID_PARAMETER;
            }
            writeOperation = TRUE;
        } else {
            writeOperation = FALSE;
        }
    
        FloppyDump( FLOPSHOW, ("\n") );
    
        // Start up the drive.
    
        if (DriveStarted) {
            status = STATUS_SUCCESS;
        } else {
#ifdef TOSHIBA
        /* 98/9/3 Removed flag set
         *
         */
            if( DisketteExtension->Removed)
                status = STATUS_CANCELLED;
            else
#endif
                
            status = FlStartDrive( DisketteExtension,
                                   Irp,
                                   writeOperation,
                                   TRUE,
                                   (BOOLEAN)
                                        !!(irpSp->Flags&SL_OVERRIDE_VERIFY_VOLUME));
        }
    
        if (!NT_SUCCESS(status)) {
            FloppyDump(
                FLOPSHOW,
                ("FlReadWrite: error on start %x\n", status)
                );
            return status;
        }
    
        if (IsNEC_98) {
    
            FlHdbit(DisketteExtension);
    
        } // (IsNEC_98)
    
        if (DisketteExtension->MediaType == Unknown) {
            FloppyDump( FLOPSHOW, ("not recognized\n") );
            return STATUS_UNRECOGNIZED_MEDIA;
        }
    
        // The drive has started up with a recognized media.
        // Gather some relavant parameters.
    
        driveMediaConstants = &DisketteExtension->DriveMediaConstants;
    
        byteToSectorShift = SECTORLENGTHCODE_TO_BYTESHIFT +
                            driveMediaConstants->SectorLengthCode;
        firstSector = irpSp->Parameters.Read.ByteOffset.LowPart>>
                      byteToSectorShift;
        lastSector = firstSector + (irpSp->Parameters.Read.Length>>
                                    byteToSectorShift);
        sectorsPerTrack = driveMediaConstants->SectorsPerTrack;
        numberOfHeads = driveMediaConstants->NumberOfHeads;
        userBuffer = MmGetSystemAddressForMdl(Irp->MdlAddress);
        trackSize = ((ULONG) sectorsPerTrack)<<byteToSectorShift;
    
        skew = 0;
        skewDelta = driveMediaConstants->SkewDelta;
        for (currentSector = firstSector;
             currentSector < lastSector;
             currentSector += numTransferSectors) {
    
            // Compute cylinder, head and sector from absolute sector.
    
            currentCylinder = (UCHAR) (currentSector/sectorsPerTrack/numberOfHeads);
            trackSector = (UCHAR) (currentSector%sectorsPerTrack);
            currentHead = (UCHAR) (currentSector/sectorsPerTrack%numberOfHeads);
            numTransferSectors = sectorsPerTrack - trackSector;
            if (lastSector - currentSector < numTransferSectors) {
                numTransferSectors = (UCHAR) (lastSector - currentSector);
            }
    
            //
            // If we're using a temporary IO buffer because of
            // insufficient registers in the DMA and we're
            // doing a write then copy the write buffer to
            // the contiguous buffer.
            //
    
            if (trackSize > DisketteExtension->MaxTransferSize) {
    
                FloppyDump(FLOPSHOW,
                          ("FlReadWrite allocating an IoBuffer\n")
                          );
                FlAllocateIoBuffer(DisketteExtension, trackSize);
                if (!DisketteExtension->IoBuffer) {
                    FloppyDump(
                        FLOPSHOW,
                        ("FlReadWrite: no resources\n")
                        );
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
                mdl = DisketteExtension->IoBufferMdl;
                ioBuffer = DisketteExtension->IoBuffer;
                ioOffset = 0;
                if (writeOperation) {
                    RtlMoveMemory(ioBuffer,
                                  userBuffer + ((currentSector - firstSector)<<
                                                byteToSectorShift),
                                  ((ULONG) numTransferSectors)<<byteToSectorShift);
                }
            } else {
                mdl = Irp->MdlAddress;
                ioOffset = (currentSector - firstSector) << byteToSectorShift;
            }
    
            //
            // Transfer the track.
            // Do what we can to avoid missing revs.
            //
    
            // Alter the skew to be in the range of what
            // we're transfering.
    
            if (skew >= numTransferSectors + trackSector) {
                skew = 0;
            }
    
            if (skew < trackSector) {
                skew = trackSector;
            }
    
            // Go from skew to the end of the irp.
    
            status = FlReadWriteTrack(
                      DisketteExtension,
                      mdl,
                      ioOffset + (((ULONG) skew - trackSector)<<byteToSectorShift),
                      writeOperation,
                      currentCylinder,
                      currentHead,
                      skew,
                      (UCHAR) (numTransferSectors + trackSector - skew),
                      TRUE);
    
            // Go from start of irp to skew.
    
            if (NT_SUCCESS(status) && skew > trackSector) {
                status = FlReadWriteTrack( DisketteExtension,
                                           mdl,
                                           ioOffset,
                                           writeOperation,
                                           currentCylinder,
                                           currentHead,
                                           trackSector,
                                           (UCHAR) (skew - trackSector),
                                           FALSE);
            } else {
                skew = (numTransferSectors + trackSector)%sectorsPerTrack;
            }
    
            if (!NT_SUCCESS(status)) {
                break;
            }
    
            //
            // If we used the temporary IO buffer to do the
            // read then copy the contents back to the IRPs buffer.
            //
    
            if (!writeOperation &&
                trackSize > DisketteExtension->MaxTransferSize) {
    
                RtlMoveMemory( userBuffer + ((currentSector - firstSector) <<
                                    byteToSectorShift),
                              ioBuffer,
                              ((ULONG) numTransferSectors)<<byteToSectorShift);
            }
    
            //
            // Increment the skew.  Do this even if just switching sides
            // for National Super I/O chips.
            //
    
            skew = (skew + skewDelta)%sectorsPerTrack;
        }
    
        Irp->IoStatus.Information =
            (currentSector - firstSector) << byteToSectorShift;
    
    
        // If the read was successful then consolidate the
        // boot sector with the determined density.
    
        if (NT_SUCCESS(status) && firstSector == 0) {
            FlConsolidateMediaTypeWithBootSector(DisketteExtension,
                                                 (PBOOT_SECTOR_INFO) userBuffer);
        }
    
        FloppyDump( FLOPSHOW,
                    ("FlReadWrite: completed status %x information %d\n",
                    status, Irp->IoStatus.Information)
                    );
        return status;
    }
    
    NTSTATUS
    FlFormat(
        IN PDISKETTE_EXTENSION DisketteExtension,
        IN PIRP Irp
        )
    
    /*++
    
    Routine Description:
    
        This routine is called by the floppy thread to format some tracks on
        the diskette.  This won't take TOO long because the FORMAT utility
        is written to only format a few tracks at a time so that it can keep
        a display of what percentage of the disk has been formatted.
    
    Arguments:
    
        DisketteExtension - pointer to our data area for the diskette to be
        formatted.
    
        Irp - pointer to the IO Request Packet.
    
    Return Value:
    
        STATUS_SUCCESS if the tracks were formatted; appropriate error
        propogated otherwise.
    
    --*/
    
    {
        LARGE_INTEGER headSettleTime;
        PIO_STACK_LOCATION irpSp;
        PBAD_TRACK_NUMBER badTrackBuffer;
        PFORMAT_PARAMETERS formatParameters;
        PFORMAT_EX_PARAMETERS formatExParameters;
        PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
        NTSTATUS ntStatus;
        ULONG badTrackBufferLength;
        DRIVE_MEDIA_TYPE driveMediaType;
        UCHAR driveStatus;
        UCHAR numberOfBadTracks = 0;
        UCHAR currentTrack;
        UCHAR endTrack;
        UCHAR whichSector;
        UCHAR retryCount;
        BOOLEAN bufferOverflow = FALSE;
        FDC_DISK_CHANGE_PARMS fdcDiskChangeParms;
    
        FloppyDump(
            FLOPSHOW,
            ("Floppy: FlFormat...\n")
            );
    
        irpSp = IoGetCurrentIrpStackLocation( Irp );
        formatParameters = (PFORMAT_PARAMETERS) Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_DISK_FORMAT_TRACKS_EX) {
            formatExParameters =
                    (PFORMAT_EX_PARAMETERS) Irp->AssociatedIrp.SystemBuffer;
        } else {
            formatExParameters = NULL;
        }
    
        FloppyDump(
            FLOPFORMAT,
            ("Floppy: Format Params - MediaType: %d\n"
             "------                  Start Cyl: %x\n"
             "------                  End   Cyl: %x\n"
             "------                  Start  Hd: %d\n"
             "------                  End    Hd: %d\n",
             formatParameters->MediaType,
             formatParameters->StartCylinderNumber,
             formatParameters->EndCylinderNumber,
             formatParameters->StartHeadNumber,
             formatParameters->EndHeadNumber)
             );
    
        badTrackBufferLength =
                        irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    
        //
        // Figure out which entry in the DriveMediaConstants table to use.
        // We know we'll find one, or FlCheckFormatParameters() would have
        // rejected the request.
        //
    
        driveMediaType =
            DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
    
        while ( ( DriveMediaConstants[driveMediaType].MediaType !=
                formatParameters->MediaType ) &&
            ( driveMediaType > DriveMediaLimits[DisketteExtension->DriveType].
                LowestDriveMediaType ) ) {
    
            driveMediaType--;

#ifdef TOSHIBAJ
			// Skip over 1.23MB and 1.2MB if drive is 3.5" 2mode.
			if ( (DisketteExtension->DriveType == DRIVE_TYPE_1440)
			  && (!DisketteExtension->Available3Mode) ) {
				if (driveMediaType == Drive144Media123) {
					driveMediaType = Drive144Media720;

					FloppyDump(
							FLOPINFO,
							("Floppy: Skip over 1.23MB and 1.2MB\n")
							);
				} else if (driveMediaType == Drive144Media640) {
					--driveMediaType;

						FloppyDump(
						FLOPINFO,
							("Floppy: Skip over 640KB\n")
							);
				}
			}
#endif
        }
    
        driveMediaConstants = &DriveMediaConstants[driveMediaType];
    
        //
        // Set some values in the diskette extension to indicate what we
        // know about the media type.
        //
    
        DisketteExtension->MediaType = formatParameters->MediaType;
        DisketteExtension->DriveMediaType = driveMediaType;
        DisketteExtension->DriveMediaConstants =
            DriveMediaConstants[driveMediaType];
    
        if (formatExParameters) {
            DisketteExtension->DriveMediaConstants.SectorsPerTrack =
                    (UCHAR) formatExParameters->SectorsPerTrack;
            DisketteExtension->DriveMediaConstants.FormatGapLength =
                    (UCHAR) formatExParameters->FormatGapLength;
        }
    
        driveMediaConstants = &(DisketteExtension->DriveMediaConstants);
    
        DisketteExtension->BytesPerSector = driveMediaConstants->BytesPerSector;
    
        DisketteExtension->ByteCapacity =
            ( driveMediaConstants->BytesPerSector ) *
            driveMediaConstants->SectorsPerTrack *
            ( 1 + driveMediaConstants->MaximumTrack ) *
            driveMediaConstants->NumberOfHeads;
    
        currentTrack = (UCHAR)( ( formatParameters->StartCylinderNumber *
            driveMediaConstants->NumberOfHeads ) +
            formatParameters->StartHeadNumber );
    
        endTrack = (UCHAR)( ( formatParameters->EndCylinderNumber *
            driveMediaConstants->NumberOfHeads ) +
            formatParameters->EndHeadNumber );
    
        FloppyDump(
            FLOPFORMAT,
            ("Floppy: Format - Starting/ending tracks: %x/%x\n",
             currentTrack,
             endTrack)
            );
    
        //
        // Set the data rate (which depends on the drive/media
        // type).
        //
    
        if (IsNEC_98) {
    
            FlHdbit(DisketteExtension);
    
        } // (IsNEC_98)
    
        if ( DisketteExtension->LastDriveMediaType != driveMediaType ) {
    
            ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );
    
            if ( !NT_SUCCESS( ntStatus ) ) {
    
                return ntStatus;
            }
        }
    
        //
        // Since we're doing a format, make this drive writable.
        //
    
        DisketteExtension->IsReadOnly = FALSE;
    
        //
        // Format each track.
        //
    
        do {
    
            //
            // Seek to proper cylinder
            //
    
            DisketteExtension->FifoBuffer[0] = COMMND_SEEK;
            DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
            DisketteExtension->FifoBuffer[2] = (UCHAR)( ( currentTrack /
                driveMediaConstants->NumberOfHeads ) <<
                driveMediaConstants->CylinderShift );
    
            FloppyDump(
                FLOPFORMAT,
                ("Floppy: Format seek to cylinder: %x\n",
                  DisketteExtension->FifoBuffer[1])
                );
    
            ntStatus = FlIssueCommand( DisketteExtension,
                                       DisketteExtension->FifoBuffer,
                                       DisketteExtension->FifoBuffer,
                                       NULL,
                                       0,
                                       0 );
    
            if ( NT_SUCCESS( ntStatus ) ) {
    
                if ( ( DisketteExtension->FifoBuffer[0] & STREG0_SEEK_COMPLETE ) &&
                    ( DisketteExtension->FifoBuffer[1] == (UCHAR)( ( currentTrack /
                        driveMediaConstants->NumberOfHeads ) <<
                        driveMediaConstants->CylinderShift ) ) ) {
    
                    //
                    // Must delay HeadSettleTime milliseconds before
                    // doing anything after a SEEK.
                    //
    
                    headSettleTime.LowPart = - ( 10 * 1000 *
                        driveMediaConstants->HeadSettleTime );
                    headSettleTime.HighPart = -1;
    
                    KeDelayExecutionThread(
                        KernelMode,
                        FALSE,
                        &headSettleTime );
    
                    if (IsNEC_98) {
                        //
                        // We don't need READ ID at format.
                        //
                    } else { // (IsNEC_98)
    
                        //
                        // Read ID.  Note that we don't bother checking the return
                        // registers, because if this media wasn't formatted we'd
                        // get an error.
                        //
    
                        DisketteExtension->FifoBuffer[0] =
                            COMMND_READ_ID + COMMND_OPTION_MFM;
                        DisketteExtension->FifoBuffer[1] =
                            DisketteExtension->DeviceUnit;
    
                        ntStatus = FlIssueCommand( DisketteExtension,
                                                   DisketteExtension->FifoBuffer,
                                                   DisketteExtension->FifoBuffer,
                                                   NULL,
                                                   0,
                                                   0 );
                    } // (IsNEC_98)
    
                } else {
    
                    FloppyDump(
                        FLOPWARN,
                        ("Floppy: format's seek returned bad registers\n"
                         "------  Statusreg0 = %x\n"
                         "------  Statusreg1 = %x\n",
                         DisketteExtension->FifoBuffer[0],
                         DisketteExtension->FifoBuffer[1])
                        );
    
                    DisketteExtension->HardwareFailed = TRUE;
    
                    ntStatus = STATUS_FLOPPY_BAD_REGISTERS;
                }
            }
    
            if ( !NT_SUCCESS( ntStatus ) ) {
    
                FloppyDump(
                    FLOPWARN,
                    ("Floppy: format's seek/readid returned %x\n", ntStatus)
                    );
    
                return ntStatus;
            }
    
            //
            // Fill the buffer with the format of this track.
            //
    
            for (whichSector = 0;
                 whichSector < driveMediaConstants->SectorsPerTrack;
                 whichSector++) {
    
                DisketteExtension->IoBuffer[whichSector*4] =
                        currentTrack/driveMediaConstants->NumberOfHeads;
                DisketteExtension->IoBuffer[whichSector*4 + 1] =
                        currentTrack%driveMediaConstants->NumberOfHeads;
                if (formatExParameters) {
                    DisketteExtension->IoBuffer[whichSector*4 + 2] =
                            (UCHAR) formatExParameters->SectorNumber[whichSector];
                } else {
                    DisketteExtension->IoBuffer[whichSector*4 + 2] =
                        whichSector + 1;
                }
                DisketteExtension->IoBuffer[whichSector*4 + 3] =
                        driveMediaConstants->SectorLengthCode;
    
                FloppyDump(
                    FLOPFORMAT,
                    ("Floppy - Format table entry %x - %x/%x/%x/%x\n",
                     whichSector,
                     DisketteExtension->IoBuffer[whichSector*4],
                     DisketteExtension->IoBuffer[whichSector*4 + 1],
                     DisketteExtension->IoBuffer[whichSector*4 + 2],
                     DisketteExtension->IoBuffer[whichSector*4 + 3])
                    );
            }
    
            //
            // Retry until success or too many retries.
            //
    
            retryCount = 0;
    
            do {
    
                ULONG length;
    
                length = driveMediaConstants->BytesPerSector;
    
                //
                // Issue command to format track
                //
    
                DisketteExtension->FifoBuffer[0] =
                    COMMND_FORMAT_TRACK + COMMND_OPTION_MFM;
                DisketteExtension->FifoBuffer[1] = (UCHAR)
                    ( ( ( currentTrack % driveMediaConstants->NumberOfHeads ) << 2 )
                    | DisketteExtension->DeviceUnit );
                DisketteExtension->FifoBuffer[2] =
                    driveMediaConstants->SectorLengthCode;
                DisketteExtension->FifoBuffer[3] =
                    driveMediaConstants->SectorsPerTrack;
                DisketteExtension->FifoBuffer[4] =
                    driveMediaConstants->FormatGapLength;
                DisketteExtension->FifoBuffer[5] =
                    driveMediaConstants->FormatFillCharacter;
    
                FloppyDump(
                    FLOPFORMAT,
                    ("Floppy: format command parameters\n"
                     "------  Head/Unit:        %x\n"
                     "------  Bytes/Sector:     %x\n"
                     "------  Sectors/Cylinder: %x\n"
                     "------  Gap 3:            %x\n"
                     "------  Filler Byte:      %x\n",
                     DisketteExtension->FifoBuffer[0],
                     DisketteExtension->FifoBuffer[1],
                     DisketteExtension->FifoBuffer[2],
                     DisketteExtension->FifoBuffer[3],
                     DisketteExtension->FifoBuffer[4])
                    );
                ntStatus = FlIssueCommand( DisketteExtension,
                                           DisketteExtension->FifoBuffer,
                                           DisketteExtension->FifoBuffer,
                                           DisketteExtension->IoBufferMdl,
                                           0,
                                           length );
    
                if ( !NT_SUCCESS( ntStatus ) ) {
    
                    FloppyDump(
                        FLOPDBGP,
                        ("Floppy: format returned %x\n", ntStatus)
                        );
                }
    
                if ( NT_SUCCESS( ntStatus ) ) {
    
                    //
                    // Check the return bytes from the controller.
                    //
    
                    if ( ( DisketteExtension->FifoBuffer[0] &
                            ( STREG0_DRIVE_FAULT |
                            STREG0_END_INVALID_COMMAND ) )
                        || ( DisketteExtension->FifoBuffer[1] &
                            STREG1_DATA_OVERRUN ) ||
                        ( DisketteExtension->FifoBuffer[2] != 0 ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: format had bad registers\n"
                             "------  Streg0 = %x\n"
                             "------  Streg1 = %x\n"
                             "------  Streg2 = %x\n",
                             DisketteExtension->FifoBuffer[0],
                             DisketteExtension->FifoBuffer[1],
                             DisketteExtension->FifoBuffer[2])
                            );
    
                        DisketteExtension->HardwareFailed = TRUE;
    
                        ntStatus = FlInterpretError(
                            DisketteExtension->FifoBuffer[1],
                            DisketteExtension->FifoBuffer[2] );
                    }
                }
    
            } while ( ( !NT_SUCCESS( ntStatus ) ) &&
                      ( retryCount++ < RECALIBRATE_RETRY_COUNT ) );
    
            if ( !NT_SUCCESS( ntStatus ) ) {
    
                if (IsNEC_98) {
                    DisketteExtension->FifoBuffer[0] = COMMND_SENSE_DRIVE_STATUS;
                    DisketteExtension->FifoBuffer[1] = DisketteExtension->DeviceUnit;
    
                    ntStatus = FlIssueCommand( DisketteExtension,
                                               DisketteExtension->FifoBuffer,
                                               DisketteExtension->FifoBuffer,
                                               NULL,
                                               0,
                                               0 );
    
                    if ( !NT_SUCCESS( ntStatus ) ) {
    
                        FloppyDump(
                            FLOPWARN,
                            ("Floppy: SENSE_DRIVE returned %x\n", ntStatus)
                            );
    
                        return ntStatus;
                    }
    
                    if ( DisketteExtension->FifoBuffer[0] & STREG3_DRIVE_READY ) {
    
                        driveStatus = DSKCHG_RESERVED;
    
                    } else {
    
                        driveStatus = DSKCHG_DISKETTE_REMOVED;
                    }
    
                } else { // (IsNEC_98)
    
                    ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                              IOCTL_DISK_INTERNAL_GET_FDC_DISK_CHANGE,
                                              &fdcDiskChangeParms );
    
                    driveStatus = fdcDiskChangeParms.DriveStatus;
                } // (IsNEC_98)
    
                if ( (DisketteExtension->DriveType != DRIVE_TYPE_0360) &&
                     driveStatus & DSKCHG_DISKETTE_REMOVED ) {
    
                    //
                    // The user apparently popped the floppy.  Return error
                    // rather than logging bad track.
                    //
    
                    return ntStatus;
                }
    
                //
                // Log the bad track.
                //
    
                FloppyDump(
                    FLOPDBGP,
                    ("Floppy: track %x is bad\n", currentTrack)
                    );
    
                if ( badTrackBufferLength >= (ULONG)
                    ( ( numberOfBadTracks + 1 ) * sizeof( BAD_TRACK_NUMBER ) ) ) {
    
                    badTrackBuffer = (PBAD_TRACK_NUMBER)
                                     Irp->AssociatedIrp.SystemBuffer;
    
                    badTrackBuffer[numberOfBadTracks] = ( BAD_TRACK_NUMBER )
                        currentTrack;
    
                } else {
    
                    bufferOverflow = TRUE;
                }
    
                numberOfBadTracks++;
            }
    
            currentTrack++;
    
        } while ( currentTrack <= endTrack );
    
        if ( ( NT_SUCCESS( ntStatus ) ) && ( bufferOverflow ) ) {
    
            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
    
        return ntStatus;
    }
    
    BOOLEAN
    FlCheckFormatParameters(
        IN PDISKETTE_EXTENSION DisketteExtension,
        IN PFORMAT_PARAMETERS FormatParameters
        )
    
    /*++
    
    Routine Description:
    
        This routine checks the supplied format parameters to make sure that
        they'll work on the drive to be formatted.
    
    Arguments:
    
        DisketteExtension - a pointer to our data area for the diskette to
        be formatted.
    
        FormatParameters - a pointer to the caller's parameters for the FORMAT.
    
    Return Value:
    
        TRUE if parameters are OK.
        FALSE if the parameters are bad.
    
    --*/
    
    {
        PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
        DRIVE_MEDIA_TYPE driveMediaType;
    
        //
        // Figure out which entry in the DriveMediaConstants table to use.
        //
    
        driveMediaType =
            DriveMediaLimits[DisketteExtension->DriveType].HighestDriveMediaType;
    
        while ( ( DriveMediaConstants[driveMediaType].MediaType !=
                FormatParameters->MediaType ) &&
            ( driveMediaType > DriveMediaLimits[DisketteExtension->DriveType].
                LowestDriveMediaType ) ) {
    
            driveMediaType--;
        }
    
        if ( DriveMediaConstants[driveMediaType].MediaType !=
            FormatParameters->MediaType ) {
    
            return FALSE;
    
        } else {
    
            driveMediaConstants = &DriveMediaConstants[driveMediaType];
    
            if ( ( FormatParameters->StartHeadNumber >
                    (ULONG)( driveMediaConstants->NumberOfHeads - 1 ) ) ||
                ( FormatParameters->EndHeadNumber >
                    (ULONG)( driveMediaConstants->NumberOfHeads - 1 ) ) ||
                ( FormatParameters->StartCylinderNumber >
                    driveMediaConstants->MaximumTrack ) ||
                ( FormatParameters->EndCylinderNumber >
                    driveMediaConstants->MaximumTrack ) ||
                ( FormatParameters->EndCylinderNumber <
                    FormatParameters->StartCylinderNumber ) ) {
    
                return FALSE;
    
            } else {
    
                if (IsNEC_98) {
                    if((FormatParameters->MediaType == F5_360_512)||
                       (FormatParameters->MediaType == F5_320_512)||
                       (FormatParameters->MediaType == F5_320_1024)||
                       (FormatParameters->MediaType == F5_180_512)||
                       (FormatParameters->MediaType == F5_160_512)){
    
                        return FALSE;
                    }
                } // (IsNEC_98)
    
                return TRUE;
            }
        }
    }
    
    NTSTATUS
    FlIssueCommand(
        IN OUT PDISKETTE_EXTENSION DisketteExtension,
        IN     PUCHAR FifoInBuffer,
        OUT    PUCHAR FifoOutBuffer,
        IN     PMDL   IoMdl,
        IN OUT ULONG  IoOffset,
        IN     ULONG  TransferBytes
        )
    
    /*++
    
    Routine Description:
    
        This routine sends the command and all parameters to the controller,
        waits for the command to interrupt if necessary, and reads the result
        bytes from the controller, if any.
    
        Before calling this routine, the caller should put the parameters for
        the command in ControllerData->FifoBuffer[].  The result bytes will
        be returned in the same place.
    
        This routine runs off the CommandTable.  For each command, this says
        how many parameters there are, whether or not there is an interrupt
        to wait for, and how many result bytes there are.  Note that commands
        without result bytes actually have two, since the ISR will issue a
        SENSE INTERRUPT STATUS command on their behalf.
    
    Arguments:
    
        Command - a byte specifying the command to be sent to the controller.
    
        FloppyExtension - a pointer to our data area for the drive being
        accessed (any drive if a controller command is being given).
    
    Return Value:
    
        STATUS_SUCCESS if the command was sent and bytes received properly;
        appropriate error propogated otherwise.
    
    --*/
    
    {
        NTSTATUS ntStatus;
        UCHAR i;
        PIRP irp;
        KEVENT DoneEvent;
        IO_STATUS_BLOCK IoStatus;
        PIO_STACK_LOCATION irpSp;
        ISSUE_FDC_COMMAND_PARMS issueCommandParms;
    
        //
        //  Set the command parameters
        //
        issueCommandParms.FifoInBuffer = FifoInBuffer;
        issueCommandParms.FifoOutBuffer = FifoOutBuffer;
        issueCommandParms.IoHandle = (PVOID)IoMdl;
        issueCommandParms.IoOffset = IoOffset;
        issueCommandParms.TransferBytes = TransferBytes;
        issueCommandParms.TimeOut = FDC_TIMEOUT;
    
        FloppyDump( FLOPSHOW,
                    ("Floppy: FloppyIssueCommand %2x...\n",
                    DisketteExtension->FifoBuffer[0])
                    );
    
        ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                  IOCTL_DISK_INTERNAL_ISSUE_FDC_COMMAND,
                                  &issueCommandParms );
    
        //
        //  If it appears like the floppy controller is not responding
        //  set the HardwareFailed flag which will force a reset.
        //
        if ( ntStatus == STATUS_DEVICE_NOT_READY ||
             ntStatus == STATUS_FLOPPY_BAD_REGISTERS ) {
    
            DisketteExtension->HardwareFailed = TRUE;
        }
    
        return ntStatus;
    }
    
    NTSTATUS
    FlInitializeControllerHardware(
        IN OUT  PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
       This routine is called to reset and initialize the floppy controller device.
    
    Arguments:
    
        disketteExtension   - Supplies the diskette extension.
    
    Return Value:
    
    --*/
    
    {
        NTSTATUS ntStatus;
    
        ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                  IOCTL_DISK_INTERNAL_RESET_FDC,
                                  NULL );
    
        if (NT_SUCCESS(ntStatus)) {
    
            if ( DisketteExtension->PerpendicularMode != 0 ) {
    
                DisketteExtension->FifoBuffer[0] = COMMND_PERPENDICULAR_MODE;
                DisketteExtension->FifoBuffer[1] =
                    (UCHAR) (COMMND_PERPENDICULAR_MODE_OW |
                            (DisketteExtension->PerpendicularMode << 2));
    
                ntStatus = FlIssueCommand( DisketteExtension,
                                           DisketteExtension->FifoBuffer,
                                           DisketteExtension->FifoBuffer,
                                           NULL,
                                           0,
                                           0 );
            }
        }
    
    
        return ntStatus;
    }
    
#ifdef TOSHIBA
        NTSTATUS
        FlFdcDeviceIoPhys(
            IN      PDEVICE_OBJECT DeviceObject,
            IN      ULONG Ioctl,
            IN OUT  PVOID Data
            )
        {
            NTSTATUS ntStatus;
            PIRP irp;
            PIO_STACK_LOCATION irpStack;
            KEVENT doneEvent;
            IO_STATUS_BLOCK ioStatus;
        
            FloppyDump(FLOPINFO,("Calling Fdc Device with %x\n", Ioctl));
        
            KeInitializeEvent( &doneEvent,
                               NotificationEvent,
                               FALSE);
        
            //
            // Create an IRP for enabler
            //
            irp = IoBuildDeviceIoControlRequest( Ioctl,
                                                 DeviceObject,
                                                 NULL,
                                                 0,
                                                 NULL,
                                                 0,
                                                 TRUE,
                                                 &doneEvent,
                                                 &ioStatus );
        
            if (irp == NULL) {
        
                FloppyDump(FLOPDBGP,("FlFloppyDeviceIo: Can't allocate Irp\n"));
                //
                // If an Irp can't be allocated, then this call will
                // simply return. This will leave the queue frozen for
                // this device, which means it can no longer be accessed.
                //
                return STATUS_INSUFFICIENT_RESOURCES;
            }
        
            irpStack = IoGetNextIrpStackLocation(irp);
            irpStack->Parameters.DeviceIoControl.Type3InputBuffer = Data;
        
            //
            // Call the driver and request the operation
            //
            ntStatus = IoCallDriver(DeviceObject, irp);
        
            if ( ntStatus == STATUS_PENDING ) {
        
                //
                // Now wait for operation to complete (should already be done,  but
                // maybe not)
                //
                KeWaitForSingleObject( &doneEvent, 
                                       Suspended, 
                                       KernelMode, 
                                       FALSE, 
                                       NULL );
        
                ntStatus = ioStatus.Status;
            }
        
            return ntStatus;
        }
#endif
    
    NTSTATUS
    FlFdcDeviceIo(
        IN      PDEVICE_OBJECT DeviceObject,
        IN      ULONG Ioctl,
        IN OUT  PVOID Data
        )
    {
        NTSTATUS ntStatus;
        PIRP irp;
        PIO_STACK_LOCATION irpStack;
        KEVENT doneEvent;
        IO_STATUS_BLOCK ioStatus;
    
        FloppyDump(FLOPINFO,("Calling Fdc Device with %x\n", Ioctl));
#ifdef TOSHIBA
        
        /* 98/9/3   Removed flag control
         */
        /*
        if( disketteExtension->Removed )
        {
            return  STATUS_CANCELLED;
        }
        */
#endif
    
        KeInitializeEvent( &doneEvent,
                           NotificationEvent,
                           FALSE);
    
        //
        // Create an IRP for enabler
        //
        irp = IoBuildDeviceIoControlRequest( Ioctl,
                                             DeviceObject,
                                             NULL,
                                             0,
                                             NULL,
                                             0,
                                             TRUE,
                                             &doneEvent,
                                             &ioStatus );
    
        if (irp == NULL) {
    
            FloppyDump(FLOPDBGP,("FlFloppyDeviceIo: Can't allocate Irp\n"));
            //
            // If an Irp can't be allocated, then this call will
            // simply return. This will leave the queue frozen for
            // this device, which means it can no longer be accessed.
            //
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    
        irpStack = IoGetNextIrpStackLocation(irp);
        irpStack->Parameters.DeviceIoControl.Type3InputBuffer = Data;
    
        //
        // Call the driver and request the operation
        //
        ntStatus = IoCallDriver(DeviceObject, irp);
    
        if ( ntStatus == STATUS_PENDING ) {
    
            //
            // Now wait for operation to complete (should already be done,  but
            // maybe not)
            //
            KeWaitForSingleObject( &doneEvent, 
                                   Suspended, 
                                   KernelMode, 
                                   FALSE, 
                                   NULL );
    
            ntStatus = ioStatus.Status;
        }
    
        return ntStatus;
    }
    
    
    NTSTATUS
    FlHdbit(
        IN OUT PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        Set a Hd bit or a FDD EXC bit.
    
    Arguments:
    
        DisketteExtension - a pointer to our data area for the device extension.
    
    
    Return Value:
    
            0: Success
            1: Error
    
    --*/
    
    {
        NTSTATUS ntStatus;
        USHORT   st;                // State of HD bit
        USHORT   st2;               // Set on/off HD bit
        USHORT   st3;               // When set HD bit, then st3=1
        USHORT   st4;               // 1.44MB bit for 1.44MB media
        SHORT    sel;               // 1.44MB Selector No for 1.44MB media
        SHORT    st5=0;             // 1.44MB on: wait for spin for 1.44MB media
        LARGE_INTEGER motorOnDelay;
    
        USHORT      lpc;
        UCHAR       resultStatus0Save[4];
        USHORT      resultStatus0;
        ULONG       getStatusRetryCount;
        ULONG       rqmReadyRetryCount;
    
        BOOLEAN     media144MB;
        BOOLEAN     mediaUpTo120MB;
        BOOLEAN     supportDrive;
    
        SET_HD_BIT_PARMS setHdBitParameter;
    
        media144MB      = FALSE;
        mediaUpTo120MB  = FALSE;
        supportDrive    = TRUE;
    
        FloppyDump(
                FLOPSTATUS,
               ("Flpydisk : HdBit Media Type = %d \n", DisketteExtension->DriveMediaType)
        );
    
        switch(DisketteExtension->DriveMediaType){
    
            //
            // 1.44MB drive
            //
    
            case    Drive144Media144Nec98:          // 3.5"   1.44Mb  drive; 1.44Mb  media
                media144MB     = TRUE;
    
            case    Drive144Media120Nec98:          // 3.5"   1.44Mb  drive; 1.2Mb   media
            case    Drive144Media123Nec98:          // 3.5"   1.44Mb  drive; 1.23Mb   media
            case    Drive120Media120Nec98:          // 5.25"  1.2Mb drive; 1.2Mb   media
            case    Drive120Media123Nec98:          // 5.25"  1.2Mb drive; 1.23Mb   media
            case    Drive12EMedia120Nec98:          // 5.25"  1.2Mb extension drive; 1.2Mb   media
            case    Drive12EMedia123Nec98:          // 5.25"  1.2Mb extension drive; 1.23Mb   media
                mediaUpTo120MB = TRUE;
    
            case    Drive360Media160Nec98:          // 5.25"  360k  drive;  160k   media
            case    Drive360Media180Nec98:          // 5.25"  360k  drive;  180k   media
            case    Drive360Media320Nec98:          // 5.25"  360k  drive;  320k   media
            case    Drive360Media32XNec98:          // 5.25"  360k  drive;  320k 1k secs
            case    Drive360Media360Nec98:          // 5.25"  360k  drive;  360k   media
    
            case    Drive120Media160Nec98:          // 5.25"  720k  drive;  160k   media
            case    Drive120Media180Nec98:          // 5.25"  720k  drive;  180k   media
            case    Drive120Media320Nec98:          // 5.25"  720k  drive;  320k   media
            case    Drive120Media32XNec98:          // 5.25"  720k  drive;  320k 1k secs
            case    Drive120Media360Nec98:          // 5.25"  720k  drive;  360k   media
            case    Drive120Media640Nec98:          // 5.25"  720k  drive;  640k   media
            case    Drive120Media720Nec98:          // 5.25"  720k  drive;  720k   media
    
            case    Drive144Media640Nec98:          // 3.5"   1.44Mb  drive;  640k   media
            case    Drive144Media720Nec98:          // 3.5"   1.44Mb  drive;  720k   media
    
                break;
    
            default:
    
                //
                // As 2HD
                //
                mediaUpTo120MB = TRUE;
    
                break;
        }
    
        setHdBitParameter.Media144MB = media144MB;
        setHdBitParameter.More120MB  = mediaUpTo120MB;
        setHdBitParameter.DeviceUnit = DisketteExtension->DeviceUnit;
        setHdBitParameter.DriveType144MB  = (DisketteExtension->DriveType == DRIVE_TYPE_1440) ? TRUE:FALSE;
    
    
        ntStatus = FlFdcDeviceIo( DisketteExtension->TargetObject,
                                  IOCTL_DISK_INTERNAL_SET_HD_BIT,
                                  &setHdBitParameter );
    
        if (!NT_SUCCESS(ntStatus)) {
            return ntStatus;
        }
    
        if (setHdBitParameter.ChangedHdBit) {
    
            ntStatus = FlDatarateSpecifyConfigure( DisketteExtension );
        }
    
        return ntStatus;
    }
    
#ifdef TOSHIBAJ
    /* for 3 mode support */
    BOOLEAN
    FlEnable3Mode(
        IN PDISKETTE_EXTENSION DisketteExtension
        )
    
    /*++
    
    Routine Description:
    
        This routine sets motor speed of the device.
    
    Arguments:
    
        DisketteExtension - a pointer to our data area for the diskette to
        be accessed.  The motor speed is changed to so that the specified
        media can be read/written.
    
        Context -  tells FlEnable3Mode() in which context it has been
        called.
    
             FALSE - floppy.sys has not issued the SPECIFY/CONFIGURE
             command yet.
             TRUE -  floppy.sys has already issued the SPECIFY/CONFIGURE
             command.
    
        How to switch floppy device motor speed is not standardized.  On some
        implementation change motor speed operation should be done before
        SPECIFY/CONFIGURE command is issued by floppy.sys.  On other systems it
        should be done after SPECIFY/CONFIGURE.  Hence this cntext flag.
    
    Return Value:
    
        TRUE if specified motor speed is set.
        FALSE if specified motor speed is not set.
    
    --*/
    
    #define IS_3_MODE_MEDIA_TYPE( MediaType ) \
                 (DriveMediaConstants[MediaType].Enable3Mode)
    
    {   NTSTATUS    ntStatus;
        ENABLE_3_MODE   enablemode;
        UCHAR       enable;
    
        if (DisketteExtension->DriveType != DRIVE_TYPE_1440)
            return TRUE;
    
        if (IS_3_MODE_MEDIA_TYPE(DisketteExtension->DriveMediaType))
        {
            if (IS_3_MODE_MEDIA_TYPE(DisketteExtension->LastDriveMediaType))
                return TRUE;
            enable = TRUE;  //Enable 3 mode
        } else
        {
            if (!IS_3_MODE_MEDIA_TYPE(DisketteExtension->LastDriveMediaType))
                return TRUE;
            enable = FALSE; //Disable 3 mode
        }
    
        // DEVICE SPECIFIC CODE SHOULD BE PLACED ONLY HERE
        // TO SUPPORT JAPANESE "3 MODE" FLOPPY DEVICES
    
        /* New IOCTL
    
        IOCTL_ENABLE_3_MODE
        input: irpStack->Parameters.DeviceIoControl.Type3InputBuffer points to
            the drive number and 3 mode enable/disable
        output  NONE
        return  NtStatus
        */
    
        enablemode.DeviceUnit = DisketteExtension->DeviceUnit;
        enablemode.Enable3Mode = enable;
    
        ntStatus = FlFdcDeviceIo (
                DisketteExtension->TargetObject,
                IOCTL_DISK_INTERNAL_ENABLE_3_MODE,
                &enablemode);
    
        if( ntStatus == STATUS_SUCCESS )
            return  TRUE;
        else
            return  FALSE;
    }
    
#endif

