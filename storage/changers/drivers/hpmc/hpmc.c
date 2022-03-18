/*++

Copyright (C) Microsoft Corporation, 1996 - 1998

Module Name:

    hpmc.c

Abstract:

    This module contains device-specific routines for HP MO and DLT medium changers:
    HP 40FX, HP ..... TODO

Environment:

    kernel mode only

Revision History:


--*/

#include "ntddk.h"
#include "mcd.h"
#include "hpmc.h"


#define HP_MO  1
#define HP_DLT 2

#define HP1194   1
#define HP1100   2
#define HP1160   3
#define HP1718   4
#define HP5151   5
#define HP5153   6
#define HP418    7
#define PLASMON  8
#define PINNACLE 9


// Device features
#define DEVICE_DOOR (L"DeviceHasDoor")
#define DEVICE_IEPORT_USER_CLOSE (L"IEPortUserClose")

// Device names
#define HPMC_MEDIUM_CHANGER (L"HPMC")


typedef struct _CHANGER_ADDRESS_MAPPING {

    //
    // Indicates the first element for each element type.
    // Used to map device-specific values into the 0-based
    // values that layers above expect.
    //

    USHORT  FirstElement[ChangerMaxElement];

    //
    // Indicates the number of each element type.
    //

    USHORT  NumberOfElements[ChangerMaxElement];

    //
    // Indicates the lowest element address for the device.
    //

    USHORT LowAddress;

    //
    // Indicates that the address mapping has been
    // completed successfully.
    //

    BOOLEAN Initialized;

} CHANGER_ADDRESS_MAPPING, *PCHANGER_ADDRESS_MAPPING;

typedef struct _CHANGER_DATA {

    //
    // Size, in bytes, of the structure.
    //

    ULONG Size;

    //
    // Drive type, either optical or dlt.
    //

    ULONG DriveType;

    //
    // Drive Id. Based on inquiry.
    //

    ULONG DriveID;

    //
    // INTERLOCKED counter of the number of prevent/allows.
    // As the HP units lock the IEPort on these operations
    // MoveMedium/SetAccess might need to clear a prevent
    // to do the operation.
    //

    LONG LockCount;

    //
    // Indicate whether to worry about the IEPort getting locked
    // down when a Prevent is sent.
    //

    ULONG DeviceLocksPort;

    //
    // See Address mapping structure above.
    //

    CHANGER_ADDRESS_MAPPING AddressMapping;

    //
    // Cached inquiry data.
    //

    INQUIRYDATA InquiryData;

} CHANGER_DATA, *PCHANGER_DATA;



NTSTATUS
HpmoBuildAddressMapping(
    IN PDEVICE_OBJECT DeviceObject
    );

ULONG
MapExceptionCodes(
    IN PELEMENT_DESCRIPTOR ElementDescriptor
    );

BOOLEAN
ElementOutOfRange(
    IN PCHANGER_ADDRESS_MAPPING AddressMap,
    IN USHORT ElementOrdinal,
    IN ELEMENT_TYPE ElementType
    );

VOID ScanForSpecial(
    IN PDEVICE_OBJECT DeviceObject,
    IN PGET_CHANGER_PARAMETERS ChangerParameters
    );



ULONG
ChangerAdditionalExtensionSize(
    VOID
    )

/*++

Routine Description:

    This routine returns the additional device extension size
    needed by the HP DLT and MO changers.

Arguments:


Return Value:

    Size, in bytes.

--*/

{

    return sizeof(CHANGER_DATA);
}


NTSTATUS
ChangerInitialize(
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA  changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    NTSTATUS       status;
    PINQUIRYDATA   dataBuffer;
    PCDB           cdb;
    ULONG          length;
    SCSI_REQUEST_BLOCK srb;

    changerData->Size = sizeof(CHANGER_DATA);


    //
    // Get inquiry data.
    //

    dataBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(INQUIRYDATA));
    if (!dataBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now get the full inquiry information for the device.
    //

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // Set timeout value.
    //

    srb.TimeOutValue = 10;

    srb.CdbLength = 6;

    cdb = (PCDB)srb.Cdb;

    //
    // Set CDB operation code.
    //

    cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;

    //
    // Set allocation length to inquiry data buffer size.
    //

    cdb->CDB6INQUIRY.AllocationLength = sizeof(INQUIRYDATA);

    status = ClassSendSrbSynchronous(DeviceObject,
                                     &srb,
                                     dataBuffer,
                                     sizeof(INQUIRYDATA),
                                     FALSE);

    if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_SUCCESS ||
        SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

        //
        // Updated the length actually transfered.
        //

        length = dataBuffer->AdditionalLength + FIELD_OFFSET(INQUIRYDATA, Reserved);

        if (length > srb.DataTransferLength) {
            length = srb.DataTransferLength;
        }


        RtlMoveMemory(&changerData->InquiryData, dataBuffer, length);

    }

    changerData->DeviceLocksPort = 1;

    //
    // Determine drive type.
    //

    if (RtlCompareMemory(dataBuffer->ProductId,"C1160A",6) == 6) {
        changerData->DriveType = HP_MO;
        changerData->DriveID   = HP1160;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C1160F",6) == 6) {
        changerData->DriveType = HP_MO;
        changerData->DriveID = HP1160;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C1100F",6) == 6) {
        changerData->DriveType = HP_MO;
        changerData->DriveID = HP1100;
        changerData->DeviceLocksPort = 0;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5153F",6) == 6) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP5153;
    } else if (RtlCompareMemory(dataBuffer->ProductId,"C1718T",6) == 6) {
        changerData->DriveType = HP_MO;
        changerData->DriveID = HP1718;
        changerData->DeviceLocksPort = 0;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C1194F",6) == 6) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP1194;
    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5151-4000", 10) == 10) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP5151;
        changerData->DeviceLocksPort = 0;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5151-2000", 10) == 10) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP5151;
        changerData->DeviceLocksPort = 0;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5177-4000",10) == 10) {

        //
        // Fast Wide versions of 1194 with DLT4000 and DLT7000 drives
        //

        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP1194;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5177-7000",10) == 10) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP1194;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5173-4000",10) == 10) {

        //
        // Fast Wide versions of 5151 with DLT4000 and DLT7000 drives
        //

        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP5151;

    } else if (RtlCompareMemory(dataBuffer->ProductId,"C5173-7000",10) == 10) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP5151;
    } else if (RtlCompareMemory(dataBuffer->ProductId,"C6280-4000",10) == 10) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP418;
    } else if (RtlCompareMemory(dataBuffer->ProductId,"C6280-7000",10) == 10) {
        changerData->DriveType = HP_DLT;
        changerData->DriveID = HP418;
    }

    //
    // Check for Plasmon and Pinnacle.
    //

    if (RtlCompareMemory(dataBuffer->VendorId,"PINNACLE", 8) == 8) {
        if (RtlCompareMemory(dataBuffer->ProductId,"ALTA", 4) == 4) {

            //
            // Acts like an 1100
            //

            changerData->DriveType = HP_MO;
            changerData->DriveID = PINNACLE;
            changerData->DeviceLocksPort = 1;
        }
    } else if (RtlCompareMemory(dataBuffer->VendorId,"IDE     ", 8) == 8) {

        if (RtlCompareMemory(dataBuffer->ProductId,"MULTI", 5) == 5) {
            changerData->DriveType = HP_MO;
            changerData->DriveID = PLASMON;
            changerData->DeviceLocksPort = 0;
        }
    }

    DebugPrint((1,
               "ChangerInitialize: DriveType %x, DriveID %x\n",
               changerData->DriveType,
               changerData->DriveID));

    ChangerClassFreePool(dataBuffer);

    //
    // Build address mapping.
    //

    status = HpmoBuildAddressMapping(DeviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Send an allow to the unit to ensure that the LockCount and state of the unit
    // are in sync.
    //

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb.Cdb;
    srb.CdbLength = CDB6GENERIC_LENGTH;
    srb.DataTransferLength = 0;
    srb.TimeOutValue = 10;
    cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
    cdb->MEDIA_REMOVAL.Prevent = 0;

    status = ClassSendSrbSynchronous(DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE);
    return STATUS_SUCCESS;
}

VOID
ScanForSpecial(
     IN PDEVICE_OBJECT DeviceObject,
     IN PGET_CHANGER_PARAMETERS ChangerParameters
     )

/*
   Routine Description : 
      This routine reads from registry certain hardware features, 
      and overrides the features derived (incorrectly) from the device.
      
   Arguments:
      DeviceObject  Pointer to the functional device object
      changerParameters Pointer to GET_CHANGER_PARAMETERS struct
   
   Return Value:
      None
 */

{
   PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension;
   PCHANGER_DATA   changerData; 
   NTSTATUS status;
   HANDLE deviceHandle, keyHandle;
   ULONG DeviceHasDoor, IEPortUserClose;
   RTL_QUERY_REGISTRY_TABLE queryTable[3];
   PDEVICE_OBJECT physicalDeviceObject;
   OBJECT_ATTRIBUTES ObjAttributes;
   UNICODE_STRING DriverName;
   ULONG DeviceBit;

   DebugPrint((3, "Entered ScanForSpecial in HPMC.SYS.\n"));
   
   ASSERT(DeviceObject != NULL);
   fdoExtension = DeviceObject->DeviceExtension;
   
   ASSERT(fdoExtension != NULL);
   changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
   
   physicalDeviceObject = fdoExtension->LowerPdo;

   //
   // Open a handle to the device node.
   //
   status = IoOpenDeviceRegistryKey(physicalDeviceObject, 
				    PLUGPLAY_REGKEY_DEVICE, 
				    KEY_QUERY_VALUE, 
				    &deviceHandle);
   if (!NT_SUCCESS(status)) {
       DebugPrint((1, 
		   "IoOpenDeviceRegistryKey Failed in ScanForSpecial : %x.\n",
		   status));
       return;
   }

   DebugPrint((3, 
	     "IoOpenDeviceRegistryKey success in HPMC.SYS!ScanForSpecial.\n"));
   
   RtlInitUnicodeString(&DriverName, HPMC_MEDIUM_CHANGER);
   
   RtlZeroMemory(queryTable, sizeof(queryTable));

   InitializeObjectAttributes(&ObjAttributes, &DriverName, 
			       (OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE),
			       deviceHandle, NULL);
   status = ZwOpenKey(&keyHandle, KEY_READ, &ObjAttributes);
   if (!NT_SUCCESS(status)) {
      DebugPrint((1, "ZwOpenKey failed in hpmc.sys : %x.\n",
		  status));
      ZwClose(deviceHandle);
      return;
   }
   
   //
   // Read values for device specific features from registry.
   // If the device has door, DeviceHasDoor flag should
   // be set to 1. Otherwise 0. Similarly, if IEPort
   // should be closed by the user, IEPortUserClose flag
   // should be set to 1. Otherwise 0.
   //
   queryTable[0].Flags = (RTL_QUERY_REGISTRY_REQUIRED | 
			     RTL_QUERY_REGISTRY_DIRECT);
   queryTable[0].Name = (PWSTR) DEVICE_DOOR;
   queryTable[0].EntryContext = &DeviceHasDoor;
   queryTable[0].DefaultType = REG_DWORD;
   queryTable[0].DefaultData = NULL;
   queryTable[0].DefaultLength = 0;
   
   queryTable[1].Flags = (RTL_QUERY_REGISTRY_REQUIRED | 
			  RTL_QUERY_REGISTRY_DIRECT);
   queryTable[1].Name = (PWSTR) DEVICE_IEPORT_USER_CLOSE;
   queryTable[1].EntryContext = &IEPortUserClose;
   queryTable[1].DefaultType = REG_DWORD;
   queryTable[1].DefaultData = NULL;
   queryTable[1].DefaultLength = 0;

   status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE, 
				   (PWSTR)keyHandle, queryTable, 
				   NULL, NULL);
   if (!NT_SUCCESS(status)) {
      DebugPrint((1, 
		  "QueryRegistry failed for DEVICE_DOOR : %d.\n",
		  status));
      ZwClose(keyHandle);
      ZwClose(deviceHandle);
      return;
   }
      
   //
   // Check the bit corresponding to this device to determine if
   // the device has door and if IEPort should be closed by the user.
   //
   DeviceBit = (1 << ((changerData->DriveID) - 1));

   if ((DeviceHasDoor & DeviceBit) == 0) {
      DebugPrint((3, "Modifying LockUnlockCapabilities flag.\n"));
      ChangerParameters->LockUnlockCapabilities &= ~LOCK_UNLOCK_DOOR;
   }

   if ((IEPortUserClose & DeviceBit) != 0) {
      DebugPrint((3, "Modifying Features1 flag.\n"));
      ChangerParameters->Features1 |= CHANGER_IEPORT_USER_CONTROL_CLOSE;
   }

   // 
   // Close the handle to the registry subkey and devnode
   //
   ZwClose(keyHandle);
   ZwClose(deviceHandle);
   
   return;
}

BOOLEAN
ChangerVerifyInquiry(
    PINQUIRYDATA InquiryData
    )
/*++

Routine Description:

    This routine determines whether the device specified in InquiryData
    should be supported by this module.

Arguments:

    InquiryData - Pointer to inquiry data.

Return Value:

    TRUE - If this is a supported device.

--*/

{


    if (RtlCompareMemory(InquiryData->VendorId,"HP      ",8) == 8) {
        if (RtlCompareMemory(InquiryData->ProductId,"C1160A",6) == 6) {

            return TRUE;

        }
        if (RtlCompareMemory(InquiryData->ProductId,"C1160F",6) == 6) {

            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C1100F",6) == 6) {

            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5153F",6) == 6) {

            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C1718T",6) == 6) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C1194F",6) == 6) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5151-4000", 10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5151-2000", 10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5177-4000",10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5177-7000",10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5173-4000",10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C5173-7000",10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C6280-4000",10) == 10) {
            return TRUE;
        }

        if (RtlCompareMemory(InquiryData->ProductId,"C6280-7000",10) == 10) {
            return TRUE;
        }

    } else if (RtlCompareMemory(InquiryData->VendorId,"PINNACLE", 8) == 8) {
        if (RtlCompareMemory(InquiryData->ProductId,"ALTA", 4) == 4) {
            return TRUE;
        }
    } else if (RtlCompareMemory(InquiryData->VendorId,"IDE     ", 8) == 8) {

        if (RtlCompareMemory(InquiryData->ProductId,"MULTI", 5) == 5) {
            return TRUE;
        }
    }

    return FALSE;
}


VOID
ChangerError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )

/*++

Routine Description:

    This routine executes any device-specific error handling needed.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/
{

    PFUNCTIONAL_DEVICE_EXTENSION          fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA              changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        switch (senseBuffer->SenseKey & 0xf) {

            case SCSI_SENSE_ILLEGAL_REQUEST:

                //if (senseBuffer->AdditionalSenseCode == ?? sense data for 'the unit is locked') {
                //}
                break;

            case SCSI_SENSE_UNIT_ATTENTION:

                changerData->LockCount = 0;
                break;

        default:
            break;
        }
    }

    return;
}

NTSTATUS
ChangerGetParameters(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine determines and returns the "drive parameters" of the
    HP changers.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION          fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA              changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING   addressMapping = &(changerData->AddressMapping);
    PSCSI_REQUEST_BLOCK        srb;
    PGET_CHANGER_PARAMETERS    changerParameters;
    PMODE_ELEMENT_ADDRESS_PAGE elementAddressPage;
    PMODE_TRANSPORT_GEOMETRY_PAGE transportGeometryPage;
    PMODE_DEVICE_CAPABILITIES_PAGE capabilitiesPage;
    NTSTATUS status;
    ULONG    bufferLength;
    PVOID    modeBuffer;
    PCDB     cdb;
    ULONG    i;

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (srb == NULL) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    //
    // Build a mode sense - Element address assignment page.
    //

    if (changerData->DriveID == HP1718) {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE);
    } else {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE);
        cdb->MODE_SENSE.Dbd = 1;
    }

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, bufferLength);

    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, bufferLength);
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = bufferLength;
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_ELEMENT_ADDRESS;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);

    if (!NT_SUCCESS(status)) {
        ChangerClassFreePool(srb);
        ChangerClassFreePool(modeBuffer);
        return status;
    }

    //
    // Fill in values.
    //

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(changerParameters, sizeof(GET_CHANGER_PARAMETERS));

    elementAddressPage = modeBuffer;
    if (changerData->DriveID == HP1718) {
        (PCHAR)elementAddressPage += (sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK));
    } else {
        (PCHAR)elementAddressPage += sizeof(MODE_PARAMETER_HEADER);
    }

    changerParameters->Size = sizeof(GET_CHANGER_PARAMETERS);
    changerParameters->NumberTransportElements = elementAddressPage->NumberTransportElements[1];
    changerParameters->NumberTransportElements |= (elementAddressPage->NumberTransportElements[0] << 8);

    changerParameters->NumberStorageElements = elementAddressPage->NumberStorageElements[1];
    changerParameters->NumberStorageElements |= (elementAddressPage->NumberStorageElements[0] << 8);

    changerParameters->NumberIEElements = elementAddressPage->NumberIEPortElements[1];
    changerParameters->NumberIEElements |= (elementAddressPage->NumberIEPortElements[0] << 8);

    changerParameters->NumberDataTransferElements = elementAddressPage->NumberDataXFerElements[1];
    changerParameters->NumberDataTransferElements |= (elementAddressPage->NumberDataXFerElements[0] << 8);


    if (!addressMapping->Initialized) {

        //
        // Build address mapping.
        //

        addressMapping->FirstElement[ChangerTransport] = (elementAddressPage->MediumTransportElementAddress[0] << 8) |
                                                          elementAddressPage->MediumTransportElementAddress[1];
        addressMapping->FirstElement[ChangerDrive] = (elementAddressPage->FirstDataXFerElementAddress[0] << 8) |
                                                      elementAddressPage->FirstDataXFerElementAddress[1];
        addressMapping->FirstElement[ChangerIEPort] = (elementAddressPage->FirstIEPortElementAddress[0] << 8) |
                                                       elementAddressPage->FirstIEPortElementAddress[1];
        addressMapping->FirstElement[ChangerSlot] = (elementAddressPage->FirstStorageElementAddress[0] << 8) |
                                                     elementAddressPage->FirstStorageElementAddress[1];


        addressMapping->FirstElement[ChangerDoor] = 0;

        addressMapping->FirstElement[ChangerKeypad] = 0;

        addressMapping->NumberOfElements[ChangerTransport] = elementAddressPage->NumberTransportElements[1];
        addressMapping->NumberOfElements[ChangerTransport] |= (elementAddressPage->NumberTransportElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDrive] = elementAddressPage->NumberDataXFerElements[1];
        addressMapping->NumberOfElements[ChangerDrive] |= (elementAddressPage->NumberDataXFerElements[0] << 8);

        addressMapping->NumberOfElements[ChangerIEPort] = elementAddressPage->NumberIEPortElements[1];
        addressMapping->NumberOfElements[ChangerIEPort] |= (elementAddressPage->NumberIEPortElements[0] << 8);

        addressMapping->NumberOfElements[ChangerSlot] = elementAddressPage->NumberStorageElements[1];
        addressMapping->NumberOfElements[ChangerSlot] |= (elementAddressPage->NumberStorageElements[0] << 8);

        //
        // Determine lowest address of all elements.
        //

        addressMapping->LowAddress = HP_NO_ELEMENT;
        for (i = 0; i <= ChangerDrive; i++) {
            if (addressMapping->LowAddress > addressMapping->FirstElement[i]) {
                addressMapping->LowAddress = addressMapping->FirstElement[i];
            }
        }
    }
    DebugPrint((1,"GetParams: First addresses\n"));
    DebugPrint((1,"Transport: %x\n",
                elementAddressPage->MediumTransportElementAddress[1]));
    DebugPrint((1,"Slot: %x\n",
                elementAddressPage->FirstStorageElementAddress[1]));
    DebugPrint((1,"Ieport: %x\n",
                elementAddressPage->FirstIEPortElementAddress[1]));
    DebugPrint((1,"Drive: %x\n",
                elementAddressPage->FirstDataXFerElementAddress[1]));
    DebugPrint((1,"LowAddress: %x\n",
                addressMapping->LowAddress));

    if (changerData->DriveType == HP_DLT) {
        changerParameters->NumberOfDoors = 1;
    } else {
        changerParameters->NumberOfDoors = 0;
    }

    changerParameters->NumberCleanerSlots = 0;

    changerParameters->FirstSlotNumber = 1;
    changerParameters->FirstDriveNumber =  1;
    changerParameters->FirstTransportNumber = 0;
    changerParameters->FirstIEPortNumber = 0;


    if (changerData->DriveID == HP5153 || changerData->DriveID == HP5151) {
        changerParameters->MagazineSize = 5;
    }

    //
    // Free buffer.
    //

    ChangerClassFreePool(modeBuffer);

    //
    // build transport geometry mode sense.
    //


    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;


    if (changerData->DriveID == HP1718) {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PAGE_TRANSPORT_GEOMETRY);
    } else {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PAGE_TRANSPORT_GEOMETRY);
        cdb->MODE_SENSE.Dbd = 1;
    }
    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, bufferLength);

    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(modeBuffer, bufferLength);
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = bufferLength;
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_TRANSPORT_GEOMETRY;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);

    if (!NT_SUCCESS(status)) {
        ChangerClassFreePool(srb);
        ChangerClassFreePool(modeBuffer);
        return status;
    }

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    transportGeometryPage = modeBuffer;
    if (changerData->DriveID == HP1718) {
        (PCHAR)transportGeometryPage += (sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK));
    } else {
        (PCHAR)transportGeometryPage += sizeof(MODE_PARAMETER_HEADER);
    }

    //
    // Determine if mc has 2-sided media.
    //

    changerParameters->Features0 = transportGeometryPage->Flip ? CHANGER_MEDIUM_FLIP : 0;
    changerParameters->Features1 = 0;

    //
    // Features based on manual, nothing programatic.
    //

    changerParameters->DriveCleanTimeout = 300;
    changerParameters->LockUnlockCapabilities = (LOCK_UNLOCK_IEPORT | LOCK_UNLOCK_DOOR);
    changerParameters->PositionCapabilities = (CHANGER_TO_SLOT      |
                                               CHANGER_TO_IEPORT    |
                                               CHANGER_TO_DRIVE);

    if (changerData->DriveID == HP1194) {
        changerParameters->Features0 |= CHANGER_BAR_CODE_SCANNER_INSTALLED      |
                                        CHANGER_CLOSE_IEPORT                    |
                                        CHANGER_OPEN_IEPORT                     |
                                        CHANGER_EXCHANGE_MEDIA                  |
                                        CHANGER_LOCK_UNLOCK                     |
                                        CHANGER_POSITION_TO_ELEMENT             |
                                        CHANGER_REPORT_IEPORT_STATE             |
                                        CHANGER_DEVICE_REINITIALIZE_CAPABLE     |
                                        CHANGER_DRIVE_CLEANING_REQUIRED         |
                                        CHANGER_DRIVE_EMPTY_ON_DOOR_ACCESS;
    }

    if (changerData->DriveID == HP5151 || changerData->DriveID == HP5153) {
        changerParameters->Features0 |= CHANGER_BAR_CODE_SCANNER_INSTALLED  |
                                        CHANGER_LOCK_UNLOCK                 |
                                        CHANGER_POSITION_TO_ELEMENT         |
                                        CHANGER_DRIVE_CLEANING_REQUIRED     |
                                        CHANGER_DEVICE_REINITIALIZE_CAPABLE |
                                        CHANGER_DRIVE_EMPTY_ON_DOOR_ACCESS;
    } else if (changerData->DriveID == HP418) {
        changerParameters->Features0 |= CHANGER_LOCK_UNLOCK                 |
                                        CHANGER_POSITION_TO_ELEMENT         |
                                        CHANGER_DRIVE_CLEANING_REQUIRED     |
                                        CHANGER_DEVICE_REINITIALIZE_CAPABLE |
                                        CHANGER_DRIVE_EMPTY_ON_DOOR_ACCESS;

       changerParameters->Features1 |= CHANGER_RTN_MEDIA_TO_ORIGINAL_ADDR;
    }

    if (changerData->DriveType == HP_MO) {

        //
        // MO units
        //

        changerParameters->Features0 |= CHANGER_CLOSE_IEPORT                  |
                                        CHANGER_OPEN_IEPORT                   |
                                        CHANGER_EXCHANGE_MEDIA                |
                                        CHANGER_LOCK_UNLOCK                   |
                                        CHANGER_POSITION_TO_ELEMENT           |
                                        CHANGER_DEVICE_REINITIALIZE_CAPABLE   |
                                        CHANGER_REPORT_IEPORT_STATE;

        changerParameters->DriveCleanTimeout = 0;

        if (changerData->DriveID == PLASMON) {

            //
            // IEport can't retract and can't tell when media is inserted when open.
            //

            changerParameters->Features0 &= ~CHANGER_CLOSE_IEPORT;
            changerParameters->Features0 &= ~CHANGER_REPORT_IEPORT_STATE;

            //
            // Transport needs to be positioned in front of drive on dismount.
            //

            changerParameters->Features1 |= CHANGER_PREDISMOUNT_ALIGN_TO_DRIVE;
        }

        if (changerData->DriveID == PINNACLE) {

            changerParameters->Features0 &= ~(CHANGER_CLOSE_IEPORT | CHANGER_OPEN_IEPORT);
        }
    } else {

        //
        // DLT
        //

        changerParameters->Features0 |= CHANGER_CLEANER_ACCESS_NOT_VALID;
    }

    if ((changerData->DriveID == HP1100) || (changerData->DriveID == PINNACLE)) {
        changerParameters->Features0 &= ~CHANGER_EXCHANGE_MEDIA;
        changerParameters->LockUnlockCapabilities &= ~LOCK_UNLOCK_DOOR;
    }

    if (changerData->DriveID == HP5151 || changerData->DriveID == HP418) {
        changerParameters->PositionCapabilities &= ~CHANGER_TO_IEPORT;
        changerParameters->LockUnlockCapabilities &= ~LOCK_UNLOCK_IEPORT;

    }
    if (changerData->DriveID == HP1160) {
        changerParameters->LockUnlockCapabilities &= ~LOCK_UNLOCK_DOOR;
    }

    //
    // Free buffer.
    //

    ChangerClassFreePool(modeBuffer);

    //
    // build transport geometry mode sense.
    //


    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    if (changerData->DriveID == HP1718) {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_DEVICE_CAPABILITIES_PAGE);
    } else {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_DEVICE_CAPABILITIES_PAGE);
        cdb->MODE_SENSE.Dbd = 1;
    }
    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, bufferLength);

    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, bufferLength);
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = bufferLength;
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CAPABILITIES;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);

    if (!NT_SUCCESS(status)) {
        ChangerClassFreePool(srb);
        ChangerClassFreePool(modeBuffer);
        return status;
    }

    //
    // Get the systembuffer and by-pass the mode header for the mode sense data.
    //

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    capabilitiesPage = modeBuffer;
    if (changerData->DriveID == HP1718) {
        (PCHAR)capabilitiesPage += (sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK));
    } else {
        (PCHAR)capabilitiesPage += sizeof(MODE_PARAMETER_HEADER);
    }

    //
    // Fill in values in Features that are contained in this page.
    //

    changerParameters->Features0 |= capabilitiesPage->MediumTransport ? CHANGER_STORAGE_DRIVE : 0;
    changerParameters->Features0 |= capabilitiesPage->StorageLocation ? CHANGER_STORAGE_SLOT : 0;
    changerParameters->Features0 |= capabilitiesPage->IEPort ? CHANGER_STORAGE_IEPORT : 0;
    changerParameters->Features0 |= capabilitiesPage->DataXFer ? CHANGER_STORAGE_DRIVE : 0;

    //
    // Determine all the move from and exchange from capabilities of this device.
    //

    changerParameters->MoveFromTransport = capabilitiesPage->MTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromTransport |= capabilitiesPage->MTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromTransport |= capabilitiesPage->MTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromTransport |= capabilitiesPage->MTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromSlot = capabilitiesPage->STtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromSlot |= capabilitiesPage->STtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromSlot |= capabilitiesPage->STtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromSlot |= capabilitiesPage->STtoDT ? CHANGER_TO_DRIVE : 0;

    if (changerData->DriveID == HP418) {

        //
        // As slot->slot moves are only available on a subset of slots, claim no support.
        //

        changerParameters->MoveFromSlot &= ~CHANGER_TO_SLOT;
    }
    if (changerData->DriveType == HP_DLT) {

        //
        // This is to workaround a FW bug with the non-ieport version.
        //

        if (changerData->DriveID == HP1194) {
            changerParameters->MoveFromIePort = capabilitiesPage->IEtoMT ? CHANGER_TO_TRANSPORT : 0;
            changerParameters->MoveFromIePort |= capabilitiesPage->IEtoST ? CHANGER_TO_SLOT : 0;
            changerParameters->MoveFromIePort |= capabilitiesPage->IEtoIE ? CHANGER_TO_IEPORT : 0;
            changerParameters->MoveFromIePort |= capabilitiesPage->IEtoDT ? CHANGER_TO_DRIVE : 0;

        } else {
            changerParameters->MoveFromIePort = 0;
        }

    } else {
        changerParameters->MoveFromIePort = capabilitiesPage->IEtoMT ? CHANGER_TO_TRANSPORT : 0;
        changerParameters->MoveFromIePort |= capabilitiesPage->IEtoST ? CHANGER_TO_SLOT : 0;
        changerParameters->MoveFromIePort |= capabilitiesPage->IEtoIE ? CHANGER_TO_IEPORT : 0;
        changerParameters->MoveFromIePort |= capabilitiesPage->IEtoDT ? CHANGER_TO_DRIVE : 0;
    }

    changerParameters->MoveFromDrive = capabilitiesPage->DTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromDrive |= capabilitiesPage->DTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromDrive |= capabilitiesPage->DTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromDrive |= capabilitiesPage->DTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromTransport = capabilitiesPage->XMTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromTransport |= capabilitiesPage->XMTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromTransport |= capabilitiesPage->XMTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromTransport |= capabilitiesPage->XMTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromSlot = capabilitiesPage->XSTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromSlot |= capabilitiesPage->XSTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromSlot |= capabilitiesPage->XSTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromSlot |= capabilitiesPage->XSTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromIePort = capabilitiesPage->XIEtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromIePort |= capabilitiesPage->XIEtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromIePort |= capabilitiesPage->XIEtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromIePort |= capabilitiesPage->XIEtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromDrive = capabilitiesPage->XDTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromDrive |= capabilitiesPage->XDTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromDrive |= capabilitiesPage->XDTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromDrive |= capabilitiesPage->XDTtoDT ? CHANGER_TO_DRIVE : 0;



    ChangerClassFreePool(srb);
    ChangerClassFreePool(modeBuffer);

    Irp->IoStatus.Information = sizeof(GET_CHANGER_PARAMETERS);

    ScanForSpecial(DeviceObject, changerParameters);

    return STATUS_SUCCESS;
}


NTSTATUS
ChangerGetStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the status of the medium changer as determined through a TUR.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCDB     cdb;
    NTSTATUS status;

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    //
    // Build TUR.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB6GENERIC_LENGTH;
    cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
    srb->TimeOutValue = 20;

    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     FALSE);

    ChangerClassFreePool(srb);
    return status;
}


NTSTATUS
ChangerGetProductData(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns fields from the inquiry data useful for
    identifying the particular device.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_PRODUCT_DATA productData = Irp->AssociatedIrp.SystemBuffer;

    RtlZeroMemory(productData, sizeof(CHANGER_PRODUCT_DATA));

    //
    // Copy cached inquiry data fields into the system buffer.
    //

    RtlMoveMemory(productData->VendorId, changerData->InquiryData.VendorId, VENDOR_ID_LENGTH);
    RtlMoveMemory(productData->ProductId, changerData->InquiryData.ProductId, PRODUCT_ID_LENGTH);
    RtlMoveMemory(productData->Revision, changerData->InquiryData.ProductRevisionLevel, REVISION_LENGTH);
    RtlMoveMemory(productData->SerialNumber, changerData->InquiryData.VendorSpecific, SERIAL_NUMBER_LENGTH);

    //
    // Indicate drive type and whether media is two-sided.
    //

    productData->DeviceType = MEDIUM_CHANGER;

    Irp->IoStatus.Information = sizeof(CHANGER_PRODUCT_DATA);
    return STATUS_SUCCESS;
}



NTSTATUS
ChangerSetAccess(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine sets the state of the IEPort.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_SET_ACCESS setAccess = Irp->AssociatedIrp.SystemBuffer;
    ULONG               controlOperation = setAccess->Control;
    NTSTATUS            status = STATUS_SUCCESS;
    LONG                lockValue = 0;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;


    if (ElementOutOfRange(addressMapping, (USHORT)setAccess->Element.ElementAddress, setAccess->Element.ElementType)) {
        DebugPrint((1,
                   "ChangerSetAccess: Element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (changerData->DeviceLocksPort) {
        if (controlOperation == LOCK_ELEMENT) {

            //
            // Inc the lock count to indicate that a prevent is on the device.
            //

            InterlockedIncrement(&changerData->LockCount);

        } else if (controlOperation == UNLOCK_ELEMENT) {

            //
            // Dec the lock count to indicate that an unlock has been sent.
            //

            changerData->LockCount = 0;
        } else {

            //
            // Either an extend or retract.
            // Need to ensure that the unit isn't locked down.
            //

            lockValue = changerData->LockCount;
            DebugPrint((1,
                       "SetAccess: LockCount is %x\n",
                       changerData->LockCount));


            RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
            cdb = (PCDB)srb->Cdb;

            srb->CdbLength = CDB6GENERIC_LENGTH;

            srb->DataTransferLength = 0;
            srb->TimeOutValue = 10;

            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
            cdb->MEDIA_REMOVAL.Prevent = 0;

            status = ClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             NULL,
                                             0,
                                             FALSE);
        }
    }


    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->DataTransferLength = 0;
    srb->TimeOutValue = 10;

    if (setAccess->Element.ElementType == ChangerDoor) {

        if (controlOperation == LOCK_ELEMENT) {

            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

            //
            // Issue prevent media removal command to lock the door.
            //

            cdb->MEDIA_REMOVAL.Prevent = 1;

        } else if (controlOperation == UNLOCK_ELEMENT) {

            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

            //
            // Issue allow media removal.
            //

            cdb->MEDIA_REMOVAL.Prevent = 0;
        }

    } else if (setAccess->Element.ElementType == ChangerIEPort) {

        if (addressMapping->NumberOfElements[ChangerIEPort] == 0) {
            status = STATUS_INVALID_PARAMETER;
        } else {

            if (controlOperation == LOCK_ELEMENT) {

                cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

                //
                // Issue prevent media removal command to lock the ie port.
                //

                cdb->MEDIA_REMOVAL.Prevent = 1;

            } else if (controlOperation == UNLOCK_ELEMENT) {

                cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

                //
                // Issue allow media removal.
                //

                cdb->MEDIA_REMOVAL.Prevent = 0;

            } else if (controlOperation == EXTEND_IEPORT) {

                if (changerData->DriveID == PINNACLE) {

                    ChangerClassFreePool(srb);
                    return STATUS_INVALID_DEVICE_REQUEST;
                }

                srb->TimeOutValue = fdoExtension->TimeOutValue;

                //
                // Hp uses a vendor unique mailslot command.
                //

                cdb->CDB6GENERIC.OperationCode = SCSIOP_ROTATE_MAILSLOT;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = HP_MAILSLOT_OPEN;

            } else if (controlOperation == RETRACT_IEPORT) {

                if (changerData->DriveID == PINNACLE) {
                    ChangerClassFreePool(srb);
                    return STATUS_INVALID_DEVICE_REQUEST;
                }

                srb->TimeOutValue = fdoExtension->TimeOutValue;

                //
                // Hp uses a vendor unique mailslot command.
                //

                cdb->CDB6GENERIC.OperationCode = SCSIOP_ROTATE_MAILSLOT;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = HP_MAILSLOT_CLOSE;

            } else {
                status = STATUS_INVALID_PARAMETER;
            }
        }
    } else {

        //
        // No keypad selectivity programatically.
        //

        status = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(status)) {

        //
        // Issue the srb.
        //

        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);

    }

    if (lockValue) {
        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
        cdb = (PCDB)srb->Cdb;

        srb->CdbLength = CDB6GENERIC_LENGTH;

        srb->DataTransferLength = 0;
        srb->TimeOutValue = 10;

        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
        cdb->MEDIA_REMOVAL.Prevent = 1;

        status = ClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);
        status = STATUS_SUCCESS;
    }

    ChangerClassFreePool(srb);
    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_SET_ACCESS);
    }

    return status;
}



NTSTATUS
ChangerGetElementStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine builds and issues a read element status command for either all elements or the
    specified element type. The buffer returned is used to build the user buffer.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA     changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING     addressMapping = &(changerData->AddressMapping);
    PCHANGER_READ_ELEMENT_STATUS readElementStatus = Irp->AssociatedIrp.SystemBuffer;
    PCHANGER_ELEMENT_STATUS      elementStatus;
    PCHANGER_ELEMENT    element;
    ELEMENT_TYPE        elementType;
    PSCSI_REQUEST_BLOCK srb;
    PCDB     cdb;
    ULONG    length;
    ULONG    statusPages;
    ULONG    totalElements = 0;
    NTSTATUS status;
    PVOID    statusBuffer;

    //
    // Determine the element type.
    //

    elementType = readElementStatus->ElementList.Element.ElementType;
    element = &readElementStatus->ElementList.Element;


    if (readElementStatus->VolumeTagInfo) {

        if (changerData->DriveType == HP_MO || changerData->DriveID == HP418) {

            //
            // These units have no Volume tag capability. DLT have this capability
            //

            return STATUS_INVALID_PARAMETER;

        }
    }

    if (elementType == AllElements) {

        ULONG i;

        statusPages = 0;

        //
        // Run through and determine number of statuspages, based on
        // whether this device claims it supports an element type.
        // As everything past ChangerDrive is artificial, stop there.
        //

        for (i = 0; i <= ChangerDrive; i++) {
            statusPages += (addressMapping->NumberOfElements[i]) ? 1 : 0;
            totalElements += addressMapping->NumberOfElements[i];
        }

        if (totalElements != readElementStatus->ElementList.NumberOfElements) {
            DebugPrint((1,
                       "ChangerGetElementStatus: Bogus number of elements in list (%x) actual (%x) AllElements\n",
                       totalElements,
                       readElementStatus->ElementList.NumberOfElements));

            return STATUS_INVALID_PARAMETER;
        }

        //
        // Account for length of the descriptors expected for the drives.
        //


        if (readElementStatus->VolumeTagInfo) {

            length = sizeof(HPMO_DATA_XFER_ELEMENT_DESCRIPTOR_PLUS) *
                            addressMapping->NumberOfElements[ChangerDrive];

            //
            // Add in length of descriptors for transport and IEPort (if applicable).
            //

            length += sizeof(HPMO_ELEMENT_DESCRIPTOR) *
                          (totalElements - (addressMapping->NumberOfElements[ChangerDrive] +
                                            addressMapping->NumberOfElements[ChangerSlot]));
            //
            // Add in length for slots.
            //

            length += sizeof(HPMO_ELEMENT_DESCRIPTOR_PLUS) * addressMapping->NumberOfElements[ChangerSlot];

            //
            // Add in header and status pages.
            //

            length += sizeof(ELEMENT_STATUS_HEADER) + (sizeof(ELEMENT_STATUS_PAGE) * statusPages);

        } else {

            if (changerData->DriveID == PLASMON) {

                length = sizeof(PLASMON_ELEMENT_DESCRIPTOR) * totalElements;

            } else {

                length = sizeof(HPMO_DATA_XFER_ELEMENT_DESCRIPTOR) * addressMapping->NumberOfElements[ChangerDrive];

                //
                // Add in length of descriptors for the other element types.
                //

                length += sizeof(HPMO_ELEMENT_DESCRIPTOR) *
                              (totalElements -
                               addressMapping->NumberOfElements[ChangerDrive]);

            }
            //
            // Add in header and status pages.
            //

            length += sizeof(ELEMENT_STATUS_HEADER) + (sizeof(ELEMENT_STATUS_PAGE) * statusPages);

        }

    } else {

        if (ElementOutOfRange(addressMapping, (USHORT)element->ElementAddress, elementType)) {
            DebugPrint((1,
                       "ChangerGetElementStatus: Element out of range.\n"));

            return STATUS_ILLEGAL_ELEMENT_ADDRESS;
        }

        totalElements = readElementStatus->ElementList.NumberOfElements;
        if (totalElements > addressMapping->NumberOfElements[elementType]) {

            DebugPrint((1,
                       "ChangerGetElementStatus: Bogus number of elements in list (%x) actual (%x) for type (%x)\n",
                       totalElements,
                       readElementStatus->ElementList.NumberOfElements,
                       elementType));

            return STATUS_INVALID_PARAMETER;
        }

        if (readElementStatus->VolumeTagInfo) {

            if (elementType == ChangerSlot) {

                length = (sizeof(HPMO_ELEMENT_DESCRIPTOR_PLUS) * totalElements);

            } else if (elementType == ChangerDrive) {

                length = (sizeof(HPMO_DATA_XFER_ELEMENT_DESCRIPTOR_PLUS) * totalElements );

            } else {

                //
                // No vol tag info from the other types.
                //

                length = (sizeof(HPMO_ELEMENT_DESCRIPTOR) * totalElements);
            }

        } else {

            if (changerData->DriveID == PLASMON) {

                length = (sizeof(PLASMON_ELEMENT_DESCRIPTOR) * totalElements);

            } else {
                if (elementType == ChangerDrive) {
                    length = (sizeof(HPMO_DATA_XFER_ELEMENT_DESCRIPTOR) * totalElements);
                } else {
                    length = (sizeof(HPMO_ELEMENT_DESCRIPTOR) * totalElements);
                }
            }
        }

        //
        // Add in length of header and status page.
        //

        length += sizeof(ELEMENT_STATUS_HEADER) + sizeof(ELEMENT_STATUS_PAGE);

    }

    DebugPrint((1,
               "ChangerGetElementStatus: Allocation Length %x, for %x elements of type %x\n",
               length,
               totalElements,
               elementType));

    statusBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, length);

    if (!statusBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(statusBuffer, length);

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {
        ChangerClassFreePool(statusBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->DataBuffer = statusBuffer;
    srb->DataTransferLength = length;
    srb->TimeOutValue = 200;

    cdb->READ_ELEMENT_STATUS.OperationCode = SCSIOP_READ_ELEMENT_STATUS;
    cdb->READ_ELEMENT_STATUS.ElementType = (UCHAR)elementType;
    cdb->READ_ELEMENT_STATUS.VolTag = readElementStatus->VolumeTagInfo;

    //
    // Fill in element addressing info based on the mapping values.
    //

    if (elementType == AllElements) {

        //
        // The HP's may not have the low address as 0.
        //

        cdb->READ_ELEMENT_STATUS.StartingElementAddress[0] =
            (UCHAR)((element->ElementAddress + addressMapping->LowAddress) >> 8);

        cdb->READ_ELEMENT_STATUS.StartingElementAddress[1] =
            (UCHAR)((element->ElementAddress + addressMapping->LowAddress) & 0xFF);

    } else {

        cdb->READ_ELEMENT_STATUS.StartingElementAddress[0] =
            (UCHAR)((element->ElementAddress + addressMapping->FirstElement[element->ElementType]) >> 8);

        cdb->READ_ELEMENT_STATUS.StartingElementAddress[1] =
            (UCHAR)((element->ElementAddress + addressMapping->FirstElement[element->ElementType]) & 0xFF);
    }

    cdb->READ_ELEMENT_STATUS.NumberOfElements[0] = (UCHAR)(totalElements >> 8);
    cdb->READ_ELEMENT_STATUS.NumberOfElements[1] = (UCHAR)(totalElements & 0xFF);

    cdb->READ_ELEMENT_STATUS.AllocationLength[0] = (UCHAR)(length >> 16);
    cdb->READ_ELEMENT_STATUS.AllocationLength[1] = (UCHAR)(length >> 8);
    cdb->READ_ELEMENT_STATUS.AllocationLength[2] = (UCHAR)(length & 0xFF);

    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);

    if (NT_SUCCESS(status)) {

        PELEMENT_STATUS_HEADER statusHeader = statusBuffer;
        PELEMENT_STATUS_PAGE statusPage;
        PELEMENT_DESCRIPTOR elementDescriptor;
        LONG remainingElements;
        LONG typeCount;
        BOOLEAN tagInfo = readElementStatus->VolumeTagInfo;
        LONG i;
        ULONG descriptorLength;

        //
        // Determine total number elements returned.
        //

        remainingElements = statusHeader->NumberOfElements[1];
        remainingElements |= (statusHeader->NumberOfElements[0] << 8);

        if (remainingElements < 0 ) {
            DebugPrint((1,
                       "ChangerGetElementStatus: Returned elements less than zero - %x\n",
                       remainingElements));

            ChangerClassFreePool(srb);
            ChangerClassFreePool(statusBuffer);

            return STATUS_IO_DEVICE_ERROR;
        }

        //
        // The buffer is composed of a header, status page, and element descriptors.
        // Point each element to it's respective place in the buffer.
        //

        (PCHAR)statusPage = (PCHAR)statusHeader;
        (PCHAR)statusPage += sizeof(ELEMENT_STATUS_HEADER);

        elementType = statusPage->ElementType;

        (PCHAR)elementDescriptor = (PCHAR)statusPage;
        (PCHAR)elementDescriptor += sizeof(ELEMENT_STATUS_PAGE);

        descriptorLength = statusPage->ElementDescriptorLength[1];
        descriptorLength |= (statusPage->ElementDescriptorLength[0] << 8);

        // Determine the number of elements of this type reported.
        typeCount =  statusPage->DescriptorByteCount[2];
        typeCount |=  (statusPage->DescriptorByteCount[1] << 8);
        typeCount |=  (statusPage->DescriptorByteCount[0] << 16);

        if (descriptorLength == 0) {
           typeCount = 0;
        }
        else { 
            typeCount /= descriptorLength;
        }

        if (typeCount == 0) {
           if (remainingElements > 0) {
              remainingElements--;
           }
        }
        else if (typeCount < 0) {
            DebugPrint((1,
                       "ChangerGetElementStatus (1): Count of type %x less than zero - %x\n",
                       elementType,
                       typeCount));

            ChangerClassFreePool(srb);
            ChangerClassFreePool(statusBuffer);

            return STATUS_IO_DEVICE_ERROR;
        }

        //
        // Fill in user buffer.
        //

        elementStatus = Irp->AssociatedIrp.SystemBuffer;

        do {

            for (i = 0; i < typeCount && remainingElements > 0; i++, remainingElements--) {

                //
                // Get the address for this element.
                //

                elementStatus->Element.ElementAddress = elementDescriptor->ElementAddress[1];
                elementStatus->Element.ElementAddress |= (elementDescriptor->ElementAddress[0] << 8);

                //
                // Account for address mapping.
                //

                elementStatus->Element.ElementAddress -= addressMapping->FirstElement[elementType];

                //
                // Set the element type.
                //

                elementStatus->Element.ElementType = elementType;
                elementStatus->Flags = 0;

                if (tagInfo) {

                    //
                    // For drive and slot types, copy the additional fields (vol tag info).
                    //

                    if (elementType == ChangerDrive) {
                        PHPMO_DATA_XFER_ELEMENT_DESCRIPTOR_PLUS driveDescriptor =
                                                                (PHPMO_DATA_XFER_ELEMENT_DESCRIPTOR_PLUS)elementDescriptor;

                        if (statusPage->PVolTag) {
                            RtlMoveMemory(elementStatus->PrimaryVolumeID, driveDescriptor->VolumeTagInformation, MAX_VOLUME_ID_SIZE);

                            elementStatus->Flags |= ELEMENT_STATUS_PVOLTAG;
                        }


                        if (elementDescriptor->IdValid) {
                            elementStatus->Flags |= ELEMENT_STATUS_ID_VALID;
                            elementStatus->TargetId = elementDescriptor->BusAddress;
                        }

                        if (elementDescriptor->LunValid) {
                            elementStatus->Flags |= ELEMENT_STATUS_LUN_VALID;
                            elementStatus->Lun = elementDescriptor->Lun;
                        }

                        //
                        // Source address
                        //

                        if (elementDescriptor->SValid) {

                            ULONG  j;
                            USHORT tmpAddress;


                            //
                            // Source address is valid. Determine the device specific address.
                            //

                            tmpAddress = elementDescriptor->SourceStorageElementAddress[1];
                            tmpAddress |= (elementDescriptor->SourceStorageElementAddress[0] << 8);

                            //
                            // Now convert to 0-based values.
                            //

                            for (j = 1; j <= ChangerDrive; j++) {
                                if (addressMapping->FirstElement[j] <= tmpAddress) {
                                    if (tmpAddress < (addressMapping->NumberOfElements[j] + addressMapping->FirstElement[j])) {
                                        elementStatus->SrcElementAddress.ElementType = j;
                                        break;
                                    }
                                }
                            }

                            elementStatus->SrcElementAddress.ElementAddress = tmpAddress - addressMapping->FirstElement[j];

                            elementStatus->Flags |= ELEMENT_STATUS_SVALID;

                        }

                    } else if (elementType == ChangerSlot) {
                        PHPMO_ELEMENT_DESCRIPTOR_PLUS slotDescriptor =
                                                         (PHPMO_ELEMENT_DESCRIPTOR_PLUS)elementDescriptor;

                        if (statusPage->PVolTag) {
                            RtlMoveMemory(elementStatus->PrimaryVolumeID, slotDescriptor->VolumeTagInformation, MAX_VOLUME_ID_SIZE);
                            elementStatus->Flags |= ELEMENT_STATUS_PVOLTAG;
                        }

                        if (elementDescriptor->SValid) {

                            ULONG  j;
                            USHORT tmpAddress;


                            //
                            // Source address is valid. Determine the device specific address.
                            //

                            tmpAddress = elementDescriptor->SourceStorageElementAddress[1];
                            tmpAddress |= (elementDescriptor->SourceStorageElementAddress[0] << 8);

                            //
                            // Now convert to 0-based values.
                            //

                            for (j = 1; j <= ChangerDrive; j++) {
                                if (addressMapping->FirstElement[j] <= tmpAddress) {
                                    if (tmpAddress < (addressMapping->NumberOfElements[j] + addressMapping->FirstElement[j])) {
                                        elementStatus->SrcElementAddress.ElementType = j;
                                        break;
                                    }
                                }
                            }

                            elementStatus->SrcElementAddress.ElementAddress = tmpAddress - addressMapping->FirstElement[j];
                            elementStatus->Flags |= ELEMENT_STATUS_SVALID;
                        }
                    }

                } else {

                    if (elementType == ChangerDrive) {

                        //
                        // Source address
                        //

                        if (elementDescriptor->SValid) {
                            ULONG  j;
                            USHORT tmpAddress;


                            //
                            // Source address is valid. Determine the device specific address.
                            //

                            tmpAddress = elementDescriptor->SourceStorageElementAddress[1];
                            tmpAddress |= (elementDescriptor->SourceStorageElementAddress[0] << 8);

                            //
                            // Now convert to 0-based values.
                            //

                            for (j = 1; j <= ChangerDrive; j++) {
                                if (addressMapping->FirstElement[j] <= tmpAddress) {
                                    if (tmpAddress < (addressMapping->NumberOfElements[j] + addressMapping->FirstElement[j])) {
                                        elementStatus->SrcElementAddress.ElementType = j;
                                        break;
                                    }
                                }
                            }

                            elementStatus->SrcElementAddress.ElementAddress = tmpAddress - addressMapping->FirstElement[j];
                            elementStatus->Flags |= ELEMENT_STATUS_SVALID;
                        }

                        if (elementDescriptor->IdValid) {
                            elementStatus->TargetId = elementDescriptor->BusAddress;
                        }
                        if (elementDescriptor->LunValid) {
                            elementStatus->Lun = elementDescriptor->Lun;
                        }
                    }
                }

                //
                // Build Flags field.
                //

                elementStatus->Flags |= elementDescriptor->Full;
                elementStatus->Flags |= (elementDescriptor->Exception << 2);
                elementStatus->Flags |= (elementDescriptor->Accessible << 3);

                if (elementType == ChangerDrive) {
                    elementStatus->Flags |= (elementDescriptor->LunValid << 12);
                    elementStatus->Flags |= (elementDescriptor->IdValid << 13);
                    elementStatus->Flags |= (elementDescriptor->NotThisBus << 15);

                    elementStatus->Flags |= (elementDescriptor->Invert << 22);
                    elementStatus->Flags |= (elementDescriptor->SValid << 23);
                }

                //
                // Map any exceptions reported directly.
                // If there is volume info returned ensure that it's not all spaces
                // as this indicates that the label is missing or unreadable.
                //

                if (elementStatus->Flags & ELEMENT_STATUS_EXCEPT) {

                    //
                    // The HP units don't have the capability of reporting exceptions
                    // in this manner except for - DataTransferElements on Optical
                    // DataXfer, Slot on DLT
                    //

                    elementStatus->ExceptionCode = MapExceptionCodes(elementDescriptor);
                } else if (elementStatus->Flags & ELEMENT_STATUS_PVOLTAG) {

                    ULONG index;

                    //
                    // Ensure that the tag info isn't all spaces. This indicates an error.
                    //

                    for (index = 0; index < MAX_VOLUME_ID_SIZE; index++) {
                        if (elementStatus->PrimaryVolumeID[index] != ' ') {
                            break;
                        }
                    }

                    //
                    // Determine if the volume id was all spaces. Do an extra check to see if media is
                    // actually present, for the unit will set the PVOLTAG flag whether media is present or not.
                    //

                    if ((index == MAX_VOLUME_ID_SIZE) && (elementStatus->Flags & ELEMENT_STATUS_FULL)) {

                        DebugPrint((1,
                                   "Hpmc.GetElementStatus: Setting exception to LABEL_UNREADABLE\n"));

                        elementStatus->Flags &= ~ELEMENT_STATUS_PVOLTAG;
                        elementStatus->Flags |= ELEMENT_STATUS_EXCEPT;
                        elementStatus->ExceptionCode = ERROR_LABEL_UNREADABLE;
                    }
                }

                //
                // Get next descriptor.
                //

                (PCHAR)elementDescriptor += descriptorLength;

                //
                // Advance to the next entry in the user buffer.
                //

                elementStatus += 1;

            }

            if (remainingElements > 0) {

                //
                // Get next status page.
                //

                (PCHAR)statusPage = (PCHAR)elementDescriptor;

                elementType = statusPage->ElementType;

                //
                // Point to decriptors.
                //

                (PCHAR)elementDescriptor = (PCHAR)statusPage;
                (PCHAR)elementDescriptor += sizeof(ELEMENT_STATUS_PAGE);

                descriptorLength = statusPage->ElementDescriptorLength[1];
                descriptorLength |= (statusPage->ElementDescriptorLength[0] << 8);

                // Determine the number of this element type reported.
                typeCount = statusPage->DescriptorByteCount[2];
                typeCount |= (statusPage->DescriptorByteCount[1] << 8);
                typeCount |= (statusPage->DescriptorByteCount[0] << 16);

                if (descriptorLength == 0) {
                   typeCount = 0;
                } else {
                   typeCount /= descriptorLength;
                }

                if (typeCount == 0) {
                    if (remainingElements > 0) {
                        remainingElements--;
                    }
                }
                else if (typeCount < 0) {
                    DebugPrint((1,
                               "ChangerGetElementStatus(2): Count of type %x less than zero - %x\n",
                               elementType,
                               typeCount));
                }
            }

        } while (remainingElements > 0);

        Irp->IoStatus.Information = sizeof(CHANGER_ELEMENT_STATUS) * totalElements;

    }

    ChangerClassFreePool(srb);
    ChangerClassFreePool(statusBuffer);

    return status;
}


NTSTATUS
ChangerInitializeElementStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine issues the necessary command to either initialize all elements
    or the specified range of elements using the normal SCSI-2 command, or a vendor-unique
    range command.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_INITIALIZE_ELEMENT_STATUS initElementStatus = Irp->AssociatedIrp.SystemBuffer;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    if (initElementStatus->ElementList.Element.ElementType == AllElements) {

        //
        // Build the normal SCSI-2 command for all elements.
        //

        srb->CdbLength = CDB6GENERIC_LENGTH;
        cdb->INIT_ELEMENT_STATUS.OperationCode = SCSIOP_INIT_ELEMENT_STATUS;

        srb->TimeOutValue = fdoExtension->TimeOutValue;
        srb->DataTransferLength = 0;

    } else {

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_INITIALIZE_ELEMENT_STATUS);
    }

    ChangerClassFreePool(srb);
    return status;
}


NTSTATUS
ChangerSetPosition(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine issues the appropriate command to set the robotic mechanism to the specified
    element address. Normally used to optimize moves or exchanges by pre-positioning the picker.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_SET_POSITION setPosition = Irp->AssociatedIrp.SystemBuffer;
    USHORT              transport;
    USHORT              destination;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    transport = (USHORT)(setPosition->Transport.ElementAddress);

    if (ElementOutOfRange(addressMapping, transport, ChangerTransport)) {

        DebugPrint((1,
                   "ChangerSetPosition: Transport element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    destination = (USHORT)(setPosition->Destination.ElementAddress);

    if (ElementOutOfRange(addressMapping, destination, setPosition->Destination.ElementType)) {
        DebugPrint((1,
                   "ChangerSetPosition: Destination element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    //
    // Convert to device addresses.
    //

    transport += addressMapping->FirstElement[ChangerTransport];
    destination += addressMapping->FirstElement[setPosition->Destination.ElementType];

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB10GENERIC_LENGTH;
    cdb->POSITION_TO_ELEMENT.OperationCode = SCSIOP_POSITION_TO_ELEMENT;

    //
    // Build device-specific addressing.
    //

    cdb->POSITION_TO_ELEMENT.TransportElementAddress[0] = (UCHAR)(transport >> 8);
    cdb->POSITION_TO_ELEMENT.TransportElementAddress[1] = (UCHAR)(transport & 0xFF);

    cdb->POSITION_TO_ELEMENT.DestinationElementAddress[0] = (UCHAR)(destination >> 8);
    cdb->POSITION_TO_ELEMENT.DestinationElementAddress[1] = (UCHAR)(destination & 0xFF);

    //
    // Doesn't support two-sided media, but as a ref. source base, it should be noted.
    //

    cdb->POSITION_TO_ELEMENT.Flip = setPosition->Flip;


    srb->DataTransferLength = 0;
    srb->TimeOutValue = 200;

    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     TRUE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_SET_POSITION);
    }

    ChangerClassFreePool(srb);
    return status;
}


NTSTATUS
ChangerExchangeMedium(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    Moves the media at source to dest1 and dest1 to dest2.

Arguments:

    DeviceObject
    Irp

Return Value:

    STATUS_INVALID_DEVICE_REQUEST

--*/

{


    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_EXCHANGE_MEDIUM exchangeMedium = Irp->AssociatedIrp.SystemBuffer;
    USHORT              transport;
    USHORT              source;
    USHORT              destination1, destination2;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    LONG                lockValue = 0;
    NTSTATUS            status;

    if (changerData->DriveID == HP5151 || changerData->DriveID == HP5153 || changerData->DriveID == HP418) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Verify transport, source, and dest. are within range.
    // Convert from 0-based to device-specific addressing.
    //

    transport = (USHORT)(exchangeMedium->Transport.ElementAddress);

    if (ElementOutOfRange(addressMapping, transport, ChangerTransport)) {

        DebugPrint((1,
                   "ChangerExchangeMedium: Transport element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    source = (USHORT)(exchangeMedium->Source.ElementAddress);

    if (ElementOutOfRange(addressMapping, source, exchangeMedium->Source.ElementType)) {

        DebugPrint((1,
                   "ChangerExchangeMedium: Source element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    destination1 = (USHORT)(exchangeMedium->Destination1.ElementAddress);

    if (ElementOutOfRange(addressMapping, destination1, exchangeMedium->Destination1.ElementType)) {
        DebugPrint((1,
                   "ChangerExchangeMedium: Destination1 element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    destination2 = (USHORT)(exchangeMedium->Destination2.ElementAddress);

    if (ElementOutOfRange(addressMapping, destination2, exchangeMedium->Destination2.ElementType)) {
        DebugPrint((1,
                   "ChangerExchangeMedium: Destination1 element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    //
    // Convert to device addresses.
    //

    transport += addressMapping->FirstElement[ChangerTransport];
    source += addressMapping->FirstElement[exchangeMedium->Source.ElementType];
    destination1 += addressMapping->FirstElement[exchangeMedium->Destination1.ElementType];
    destination2 += addressMapping->FirstElement[exchangeMedium->Destination2.ElementType];

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // for HP 1194, if the source or destination is an IEPORT,
    // do an allow before the move and a prevent after the move.
    // This works around the behaviour of the device whereby a PreventMediumRemoval
    // inhibits a MoveMedium to/from the IEPORT.
    //

    if ((changerData->DeviceLocksPort) &&
       ((exchangeMedium->Destination1.ElementType == ChangerIEPort) ||
        (exchangeMedium->Destination2.ElementType == ChangerIEPort) ||
        (exchangeMedium->Source.ElementType == ChangerIEPort))) {


        //
        // Determine value of LockCount.
        // Note that if other functionality is added to this routine, EXTEND/RETRACT
        // will have to be split out from this else.
        //

        lockValue = changerData->LockCount;
        DebugPrint((1,
                   "ExchangeMedium: LockCount is %x\n",
                   lockValue));

        if (lockValue) {

            //
            // Send an allow to clear the prevent for IEPORT extend/retract.
            //

            cdb = (PCDB)srb->Cdb;
            srb->CdbLength = CDB6GENERIC_LENGTH;
            srb->DataTransferLength = 0;
            srb->TimeOutValue = 10;
            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
            cdb->MEDIA_REMOVAL.Prevent = 0;

            //
            // Ignore errors at this point. If this fails and the move doesn't happen, the LM will
            // clean things up.
            //

            status = ClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             NULL,
                                             0,
                                             FALSE);
            DebugPrint((1,
                       "ExchangeMedium: Allow sent. Status %x\n",
                       status));

            status = STATUS_SUCCESS;
        }
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;
    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    cdb->EXCHANGE_MEDIUM.OperationCode = SCSIOP_EXCHANGE_MEDIUM;

    //
    // Build addressing values based on address map.
    //

    cdb->EXCHANGE_MEDIUM.TransportElementAddress[0] = (UCHAR)(transport >> 8);
    cdb->EXCHANGE_MEDIUM.TransportElementAddress[1] = (UCHAR)(transport & 0xFF);

    cdb->EXCHANGE_MEDIUM.SourceElementAddress[0] = (UCHAR)(source >> 8);
    cdb->EXCHANGE_MEDIUM.SourceElementAddress[1] = (UCHAR)(source & 0xFF);

    cdb->EXCHANGE_MEDIUM.Destination1ElementAddress[0] = (UCHAR)(destination1 >> 8);
    cdb->EXCHANGE_MEDIUM.Destination1ElementAddress[1] = (UCHAR)(destination1 & 0xFF);

    cdb->EXCHANGE_MEDIUM.Destination2ElementAddress[0] = (UCHAR)(destination2 >> 8);
    cdb->EXCHANGE_MEDIUM.Destination2ElementAddress[1] = (UCHAR)(destination2 & 0xFF);

    cdb->EXCHANGE_MEDIUM.Flip1 = exchangeMedium->Flip1;
    cdb->EXCHANGE_MEDIUM.Flip2 = exchangeMedium->Flip2;

    srb->DataTransferLength = 0;

    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_EXCHANGE_MEDIUM);
    }

    if ((changerData->DeviceLocksPort) &&
       ((exchangeMedium->Destination1.ElementType == ChangerIEPort) ||
        (exchangeMedium->Destination2.ElementType == ChangerIEPort) ||
        (exchangeMedium->Source.ElementType == ChangerIEPort))) {

        if (lockValue) {

            NTSTATUS preventStatus;

            //
            // Send the prevent to re-lock down the unit.
            //

            RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
            cdb = (PCDB)srb->Cdb;
            srb->CdbLength = CDB6GENERIC_LENGTH;
            srb->DataTransferLength = 0;
            srb->TimeOutValue = 10;
            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
            cdb->MEDIA_REMOVAL.Prevent = 1;

            //
            // Ignore any errors at this point. The calling layer will need to fixup any problems with
            // prevent/allow.
            //

            preventStatus = ClassSendSrbSynchronous(DeviceObject,
                                                    srb,
                                                    NULL,
                                                    0,
                                                    FALSE);

            DebugPrint((1,
                       "ExchangeMedium: Prevent sent. Status %x\n",
                       preventStatus));
        }
    }

    ChangerClassFreePool(srb);
    return status;
}


NTSTATUS
ChangerMoveMedium(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/


{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_MOVE_MEDIUM moveMedium = Irp->AssociatedIrp.SystemBuffer;
    USHORT transport;
    USHORT source;
    USHORT destination;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    LONG                lockValue = 0;
    NTSTATUS            status;

    //
    // Verify transport, source, and dest. are within range.
    // Convert from 0-based to device-specific addressing.
    //

    transport = (USHORT)(moveMedium->Transport.ElementAddress);

    if (ElementOutOfRange(addressMapping, transport, ChangerTransport)) {

        DebugPrint((1,
                   "ChangerMoveMedium: Transport element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    source = (USHORT)(moveMedium->Source.ElementAddress);

    if (ElementOutOfRange(addressMapping, source, moveMedium->Source.ElementType)) {

        DebugPrint((1,
                   "ChangerMoveMedium: Source element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    destination = (USHORT)(moveMedium->Destination.ElementAddress);

    if (ElementOutOfRange(addressMapping, destination, moveMedium->Destination.ElementType)) {
        DebugPrint((1,
                   "ChangerMoveMedium: Destination element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    //
    // Convert to device addresses.
    //

    transport += addressMapping->FirstElement[ChangerTransport];
    source += addressMapping->FirstElement[moveMedium->Source.ElementType];
    destination += addressMapping->FirstElement[moveMedium->Destination.ElementType];

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // for many HP units, if the source or destination is an IEPORT,
    // do an allow before the move and a prevent after the move.
    // This works around the behaviour of the device whereby a PreventMediumRemoval
    // inhibits a MoveMedium to/from the IEPORT.
    //

    if ((changerData->DeviceLocksPort) &&
        ((moveMedium->Destination.ElementType == ChangerIEPort) ||
         (moveMedium->Source.ElementType == ChangerIEPort))) {


        //
        // Determine value of LockCount.
        // Note that if other functionality is added to this routine, EXTEND/RETRACT
        // will have to be split out from this else.
        //

        lockValue = changerData->LockCount;
        DebugPrint((1,
                   "MoveMedium: LockCount is %x\n",
                   lockValue));

        if (lockValue) {

            //
            // Send an allow to clear the prevent for IEPORT extend/retract.
            //

            cdb = (PCDB)srb->Cdb;
            srb->CdbLength = CDB6GENERIC_LENGTH;
            srb->DataTransferLength = 0;
            srb->TimeOutValue = 10;
            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
            cdb->MEDIA_REMOVAL.Prevent = 0;

            //
            // Ignore errors at this point. If this fails and the move doesn't happen, the LM will
            // clean things up.
            //

            status = ClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             NULL,
                                             0,
                                             FALSE);
            DebugPrint((1,
                       "MoveMedium: Allow sent. Status %x\n",
                       status));

            status = STATUS_SUCCESS;
        }
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;
    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    cdb->MOVE_MEDIUM.OperationCode = SCSIOP_MOVE_MEDIUM;

    //
    // Build addressing values based on address map.
    //

    cdb->MOVE_MEDIUM.TransportElementAddress[0] = (UCHAR)(transport >> 8);
    cdb->MOVE_MEDIUM.TransportElementAddress[1] = (UCHAR)(transport & 0xFF);

    cdb->MOVE_MEDIUM.SourceElementAddress[0] = (UCHAR)(source >> 8);
    cdb->MOVE_MEDIUM.SourceElementAddress[1] = (UCHAR)(source & 0xFF);

    cdb->MOVE_MEDIUM.DestinationElementAddress[0] = (UCHAR)(destination >> 8);
    cdb->MOVE_MEDIUM.DestinationElementAddress[1] = (UCHAR)(destination & 0xFF);

    cdb->MOVE_MEDIUM.Flip = moveMedium->Flip;

    srb->DataTransferLength = 0;

    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_MOVE_MEDIUM);
    } else {
        DebugPrint((1,
                   "MoveMedium: Status of Move %x\n",
                   status));
    }

    if ((changerData->DeviceLocksPort) &&
        ((moveMedium->Destination.ElementType == ChangerIEPort) ||
         (moveMedium->Source.ElementType == ChangerIEPort))) {

        if (lockValue) {

            NTSTATUS preventStatus;

            //
            // Send the prevent to re-lock down the unit.
            //

            RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
            cdb = (PCDB)srb->Cdb;
            srb->CdbLength = CDB6GENERIC_LENGTH;
            srb->DataTransferLength = 0;
            srb->TimeOutValue = 10;
            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
            cdb->MEDIA_REMOVAL.Prevent = 1;

            //
            // Ignore any errors at this point. The LM will fixup any problems with
            // prevent/allow
            //

            preventStatus = ClassSendSrbSynchronous(DeviceObject,
                                    srb,
                                    NULL,
                                    0,
                                    FALSE);
            DebugPrint((1,
                       "MoveMedium: Prevent sent. Status %x\n",
                       preventStatus));
        }
    }

    ChangerClassFreePool(srb);

    DebugPrint((1,
               "MoveMedium: Returning %x\n",
               status));
    return status;
}


NTSTATUS
ChangerReinitializeUnit(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;



    //
    // Issue a rezero unit to the device.
    //

    srb->CdbLength = CDB6GENERIC_LENGTH;
    cdb->CDB6GENERIC.OperationCode = SCSIOP_REZERO_UNIT;
    srb->DataTransferLength = 0;
    srb->TimeOutValue = fdoExtension->TimeOutValue;


    //
    // Send SCSI command (CDB) to device
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_ELEMENT);
    }

    //
    // Clear locks.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb->Cdb;
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->DataTransferLength = 0;
    srb->TimeOutValue = 10;
    cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
    cdb->MEDIA_REMOVAL.Prevent = 0;

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     NULL,
                                     0,
                                     FALSE);
    //
    // Set LockCount to zero.
    //

    changerData->LockCount = 0;

    ChangerClassFreePool(srb);
    return status;
}


NTSTATUS
ChangerQueryVolumeTags(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    return STATUS_INVALID_DEVICE_REQUEST;
}


NTSTATUS
HpmoBuildAddressMapping(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine issues the appropriate mode sense commands and builds an
    array of element addresses. These are used to translate between the device-specific
    addresses and the zero-based addresses of the API.

Arguments:

    DeviceObject

Return Value:

    NTSTATUS

--*/
{

    PFUNCTIONAL_DEVICE_EXTENSION      fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA          changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &changerData->AddressMapping;
    PSCSI_REQUEST_BLOCK    srb;
    PCDB                   cdb;
    NTSTATUS               status;
    ULONG                  bufferLength;
    PMODE_ELEMENT_ADDRESS_PAGE elementAddressPage;
    PVOID modeBuffer;
    ULONG i;

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);


    //
    // Set all FirstElements to NO_ELEMENT.
    //

    for (i = 0; i < ChangerMaxElement; i++) {
        addressMapping->FirstElement[i] = HP_NO_ELEMENT;
    }

    cdb = (PCDB)srb->Cdb;

    //
    // Build a mode sense - Element address assignment page.
    //

    if (changerData->DriveID == HP1718) {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE);
    } else {
        bufferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE);
        cdb->MODE_SENSE.Dbd = 1;
    }

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, bufferLength);

    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(modeBuffer, bufferLength);
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = bufferLength;
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_ELEMENT_ADDRESS;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);

    elementAddressPage = modeBuffer;
    if (changerData->DriveID == HP1718) {
        (PCHAR)elementAddressPage += (sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK));
    } else {
        (PCHAR)elementAddressPage += sizeof(MODE_PARAMETER_HEADER);
    }

    if (NT_SUCCESS(status)) {

        //
        // Build address mapping.
        //

        addressMapping->FirstElement[ChangerTransport] = (elementAddressPage->MediumTransportElementAddress[0] << 8) |
                                                          elementAddressPage->MediumTransportElementAddress[1];
        addressMapping->FirstElement[ChangerDrive] = (elementAddressPage->FirstDataXFerElementAddress[0] << 8) |
                                                      elementAddressPage->FirstDataXFerElementAddress[1];
        addressMapping->FirstElement[ChangerIEPort] = (elementAddressPage->FirstIEPortElementAddress[0] << 8) |
                                                       elementAddressPage->FirstIEPortElementAddress[1];
        addressMapping->FirstElement[ChangerSlot] = (elementAddressPage->FirstStorageElementAddress[0] << 8) |
                                                     elementAddressPage->FirstStorageElementAddress[1];

        //
        // Determine lowest address of all elements.
        //


        addressMapping->LowAddress = HP_NO_ELEMENT;
        for (i = 0; i <= ChangerDrive; i++) {
            if (addressMapping->LowAddress > addressMapping->FirstElement[i]) {
                addressMapping->LowAddress = addressMapping->FirstElement[i];
            }
        }

        addressMapping->FirstElement[ChangerDoor] = 0;
        addressMapping->FirstElement[ChangerKeypad] = 0;

        addressMapping->NumberOfElements[ChangerTransport] = elementAddressPage->NumberTransportElements[1];
        addressMapping->NumberOfElements[ChangerTransport] |= (elementAddressPage->NumberTransportElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDrive] = elementAddressPage->NumberDataXFerElements[1];
        addressMapping->NumberOfElements[ChangerDrive] |= (elementAddressPage->NumberDataXFerElements[0] << 8);

        addressMapping->NumberOfElements[ChangerIEPort] = elementAddressPage->NumberIEPortElements[1];
        addressMapping->NumberOfElements[ChangerIEPort] |= (elementAddressPage->NumberIEPortElements[0] << 8);

        addressMapping->NumberOfElements[ChangerSlot] = elementAddressPage->NumberStorageElements[1];
        addressMapping->NumberOfElements[ChangerSlot] |= (elementAddressPage->NumberStorageElements[0] << 8);


        if (changerData->DriveType == HP_MO) {
            addressMapping->NumberOfElements[ChangerDoor] = 0;
        } else {
            addressMapping->NumberOfElements[ChangerDoor] = 1;
        }
        addressMapping->NumberOfElements[ChangerKeypad] = 0;

        addressMapping->Initialized = TRUE;

    }




    //
    // Free buffer.
    //

    ChangerClassFreePool(modeBuffer);
    ChangerClassFreePool(srb);

    return status;
}


ULONG
MapExceptionCodes(
    IN PELEMENT_DESCRIPTOR ElementDescriptor
    )

/*++

Routine Description:

    This routine takes the sense data from the elementDescriptor and creates
    the appropriate bitmap of values.

Arguments:

   ElementDescriptor - pointer to the descriptor page.

Return Value:

    Bit-map of exception codes.

--*/

{

    ULONG exceptionCode = 0;
    UCHAR asc = ElementDescriptor->AdditionalSenseCode;
    UCHAR ascq = ElementDescriptor->AddSenseCodeQualifier;


    switch (asc) {
        case 0x0:
            break;

        default:
            exceptionCode = ERROR_UNHANDLED_ERROR;
    }

    DebugPrint((1,
               "Hpmc.MapExceptionCode: ASC %x, ASCQ %x, exceptionCode %x\n",
               asc,
               ascq,
               exceptionCode));

    return exceptionCode;

}


BOOLEAN
ElementOutOfRange(
    IN PCHANGER_ADDRESS_MAPPING AddressMap,
    IN USHORT ElementOrdinal,
    IN ELEMENT_TYPE ElementType
    )
/*++

Routine Description:

    This routine determines whether the element address passed in is within legal range for
    the device.

Arguments:

    AddressMap - The dds' address map array
    ElementOrdinal - Zero-based address of the element to check.
    ElementType

Return Value:

    TRUE if out of range

--*/
{

    if (ElementOrdinal >= AddressMap->NumberOfElements[ElementType]) {

        DebugPrint((0,
                   "ElementOutOfRange: Type %x, Ordinal %x, Max %x\n",
                   ElementType,
                   ElementOrdinal,
                   AddressMap->NumberOfElements[ElementType]));
        return TRUE;
    } else if (AddressMap->FirstElement[ElementType] == HP_NO_ELEMENT) {

        DebugPrint((1,
                   "ElementOutOfRange: No Type %x present\n",
                   ElementType));

        return TRUE;
    }

    return FALSE;
}

