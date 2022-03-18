/*--

Copyright (C) Microsoft Corporation, 1999 - 1999

Module Name:

    ioctl.c

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

#if DBG
    PUCHAR READ_DVD_STRUCTURE_FORMAT_STRINGS[DvdMaxDescriptor+1] = {
        "Physical",
        "Copyright",
        "DiskKey",
        "BCA",
        "Manufacturer",
        "Unknown"
    };
#endif // DBG


VOID
CdRomDeviceControlDvdReadStructure(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    )
{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(OriginalIrp);
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCDB cdb = (PCDB)Srb->Cdb;
    PVOID dataBuffer;

    PDVD_READ_STRUCTURE request;
    USHORT dataLength;
    ULONG blockNumber;
    PFOUR_BYTE fourByte;

    dataLength =
        (USHORT)currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

    request = OriginalIrp->AssociatedIrp.SystemBuffer;
    blockNumber =
        (ULONG)(request->BlockByteOffset.QuadPart >> fdoExtension->SectorShift);
    fourByte = (PFOUR_BYTE) &blockNumber;

    Srb->CdbLength = 12;
    Srb->TimeOutValue = fdoExtension->TimeOutValue;
    Srb->SrbFlags = fdoExtension->SrbFlags;
    SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DATA_IN);

    cdb->READ_DVD_STRUCTURE.OperationCode = SCSIOP_READ_DVD_STRUCTURE;
    cdb->READ_DVD_STRUCTURE.RMDBlockNumber[0] = fourByte->Byte3;
    cdb->READ_DVD_STRUCTURE.RMDBlockNumber[1] = fourByte->Byte2;
    cdb->READ_DVD_STRUCTURE.RMDBlockNumber[2] = fourByte->Byte1;
    cdb->READ_DVD_STRUCTURE.RMDBlockNumber[3] = fourByte->Byte0;
    cdb->READ_DVD_STRUCTURE.LayerNumber   = request->LayerNumber;
    cdb->READ_DVD_STRUCTURE.Format        = (UCHAR)request->Format;

#if DBG
    {
        if ((UCHAR)request->Format > DvdMaxDescriptor) {
            DebugPrint((1, "READ_DVD_STRUCTURE format %x = %s (%x bytes)\n",
                        (UCHAR)request->Format,
                        READ_DVD_STRUCTURE_FORMAT_STRINGS[DvdMaxDescriptor],
                        dataLength
                        ));
        } else {
            DebugPrint((1, "READ_DVD_STRUCTURE format %x = %s (%x bytes)\n",
                        (UCHAR)request->Format,
                        READ_DVD_STRUCTURE_FORMAT_STRINGS[(UCHAR)request->Format],
                        dataLength
                        ));
        }
    }
#endif // DBG

    if (request->Format == DvdDiskKeyDescriptor) {

        cdb->READ_DVD_STRUCTURE.AGID = (UCHAR) request->SessionId;

    }

    cdb->READ_DVD_STRUCTURE.AllocationLength[0] = (UCHAR)(dataLength >> 8);
    cdb->READ_DVD_STRUCTURE.AllocationLength[1] = (UCHAR)(dataLength & 0xff);
    Srb->DataTransferLength = dataLength;



    dataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                       dataLength,
                                       DVD_TAG_READ_STRUCTURE);

    if (!dataBuffer) {
        ExFreePool(Srb->SenseInfoBuffer);
        ExFreePool(Srb);
        IoFreeIrp(NewIrp);
        OriginalIrp->IoStatus.Information = 0;
        OriginalIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

        BAIL_OUT(OriginalIrp);
        CdRomCompleteIrpAndStartNextPacketSafely(Fdo, OriginalIrp);
        return;
    }
    RtlZeroMemory(dataBuffer, dataLength);

    NewIrp->MdlAddress = IoAllocateMdl(dataBuffer,
                                       currentIrpStack->Parameters.Read.Length,
                                       FALSE,
                                       FALSE,
                                       (PIRP) NULL);

    if (NewIrp->MdlAddress == NULL) {
        ExFreePool(dataBuffer);
        ExFreePool(Srb->SenseInfoBuffer);
        ExFreePool(Srb);
        IoFreeIrp(NewIrp);
        OriginalIrp->IoStatus.Information = 0;
        OriginalIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

        BAIL_OUT(OriginalIrp);
        CdRomCompleteIrpAndStartNextPacketSafely(Fdo, OriginalIrp);
        return;
    }

    //
    // Prepare the MDL
    //

    MmBuildMdlForNonPagedPool(NewIrp->MdlAddress);

    Srb->DataBuffer = dataBuffer;

    IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, NewIrp);

    return;
}

VOID
CdRomDeviceControlDvdEndSession(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    )
{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(OriginalIrp);
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCDB cdb = (PCDB)Srb->Cdb;

    PDVD_SESSION_ID sessionId = OriginalIrp->AssociatedIrp.SystemBuffer;

    Srb->CdbLength = 12;
    Srb->TimeOutValue = fdoExtension->TimeOutValue;
    Srb->SrbFlags = fdoExtension->SrbFlags;
    SET_FLAG(Srb->SrbFlags, SRB_FLAGS_NO_DATA_TRANSFER);

    cdb->SEND_KEY.OperationCode = SCSIOP_SEND_KEY;
    cdb->SEND_KEY.AGID = (UCHAR) (*sessionId);
    cdb->SEND_KEY.KeyFormat = DVD_INVALIDATE_AGID;

    IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, NewIrp);
    return;

}


VOID
CdRomDeviceControlDvdStartSessionReadKey(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    )
{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(OriginalIrp);
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCDB cdb = (PCDB)Srb->Cdb;
    NTSTATUS status;

    PDVD_COPY_PROTECT_KEY keyParameters;
    PCDVD_KEY_HEADER keyBuffer = NULL;

    ULONG keyLength;

    ULONG allocationLength;
    PFOUR_BYTE fourByte;

    //
    // Both of these use REPORT_KEY commands.
    // Determine the size of the input buffer
    //

    if(currentIrpStack->Parameters.DeviceIoControl.IoControlCode ==
       IOCTL_DVD_READ_KEY) {

        keyParameters = OriginalIrp->AssociatedIrp.SystemBuffer;

        keyLength = sizeof(CDVD_KEY_HEADER) +
                    (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength -
                     sizeof(DVD_COPY_PROTECT_KEY));
    } else {

        keyParameters = NULL;
        keyLength = sizeof(CDVD_KEY_HEADER) +
                    sizeof(CDVD_REPORT_AGID_DATA);
    }

    TRY {

        keyBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                          keyLength,
                                          DVD_TAG_READ_KEY);

        if(keyBuffer == NULL) {

            DebugPrint((1, "IOCTL_DVD_READ_KEY - couldn't allocate "
                           "%d byte buffer for key\n",
                           keyLength));
            status = STATUS_INSUFFICIENT_RESOURCES;
            LEAVE;
        }


        NewIrp->MdlAddress = IoAllocateMdl(keyBuffer,
                                           keyLength,
                                           FALSE,
                                           FALSE,
                                           (PIRP) NULL);

        if(NewIrp->MdlAddress == NULL) {

            DebugPrint((1, "IOCTL_DVD_READ_KEY - couldn't create mdl\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            LEAVE;
        }

        MmBuildMdlForNonPagedPool(NewIrp->MdlAddress);

        Srb->DataBuffer = keyBuffer;
        Srb->CdbLength = 12;

        cdb->REPORT_KEY.OperationCode = SCSIOP_REPORT_KEY;

        allocationLength = keyLength;
        fourByte = (PFOUR_BYTE) &allocationLength;
        cdb->REPORT_KEY.AllocationLength[0] = fourByte->Byte1;
        cdb->REPORT_KEY.AllocationLength[1] = fourByte->Byte0;

        Srb->DataTransferLength = keyLength;

        //
        // set the specific parameters....
        //

        if(currentIrpStack->Parameters.DeviceIoControl.IoControlCode ==
           IOCTL_DVD_READ_KEY) {

            if(keyParameters->KeyType == DvdTitleKey) {

                ULONG logicalBlockAddress;

                logicalBlockAddress = (ULONG)
                    (keyParameters->Parameters.TitleOffset.QuadPart >>
                     fdoExtension->SectorShift);

                fourByte = (PFOUR_BYTE) &(logicalBlockAddress);

                cdb->REPORT_KEY.LogicalBlockAddress[0] = fourByte->Byte3;
                cdb->REPORT_KEY.LogicalBlockAddress[1] = fourByte->Byte2;
                cdb->REPORT_KEY.LogicalBlockAddress[2] = fourByte->Byte1;
                cdb->REPORT_KEY.LogicalBlockAddress[3] = fourByte->Byte0;
            }

            cdb->REPORT_KEY.KeyFormat = (UCHAR)keyParameters->KeyType;
            cdb->REPORT_KEY.AGID = (UCHAR) keyParameters->SessionId;
            DebugPrint((1, "CdRomDvdReadKey => sending irp %p for irp %p (%s)\n",
                        NewIrp, OriginalIrp, "READ_KEY"));

        } else {

            cdb->REPORT_KEY.KeyFormat = DVD_REPORT_AGID;
            cdb->REPORT_KEY.AGID = 0;
            DebugPrint((1, "CdRomDvdReadKey => sending irp %p for irp %p (%s)\n",
                        NewIrp, OriginalIrp, "START_SESSION"));
        }

        Srb->TimeOutValue = fdoExtension->TimeOutValue;
        Srb->SrbFlags = fdoExtension->SrbFlags;
        SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DATA_IN);

        IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, NewIrp);

        status = STATUS_SUCCESS;

    } FINALLY {

        if (!NT_SUCCESS(status)) {

            //
            // An error occured during setup - free resources and
            // complete this request.
            //
            if (NewIrp->MdlAddress != NULL) {
                IoFreeMdl(NewIrp->MdlAddress);
            }

            if (keyBuffer != NULL) {
                ExFreePool(keyBuffer);
            }
            ExFreePool(Srb->SenseInfoBuffer);
            ExFreePool(Srb);
            IoFreeIrp(NewIrp);

            OriginalIrp->IoStatus.Information = 0;
            OriginalIrp->IoStatus.Status = status;

            BAIL_OUT(OriginalIrp);
            CdRomCompleteIrpAndStartNextPacketSafely(Fdo, OriginalIrp);

        } // end !NT_SUCCESS
    }
    return;
}



VOID
CdRomDeviceControlDvdSendKey(
    IN PDEVICE_OBJECT Fdo,
    IN PIRP OriginalIrp,
    IN PIRP NewIrp,
    IN PSCSI_REQUEST_BLOCK Srb
    )
{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(OriginalIrp);
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = Fdo->DeviceExtension;
    PCDB cdb = (PCDB)Srb->Cdb;

    PDVD_COPY_PROTECT_KEY key;
    PCDVD_KEY_HEADER keyBuffer = NULL;

    NTSTATUS status;
    ULONG keyLength;
    PFOUR_BYTE fourByte;

    key = OriginalIrp->AssociatedIrp.SystemBuffer;
    keyLength = (key->KeyLength - sizeof(DVD_COPY_PROTECT_KEY)) +
                sizeof(PCDVD_KEY_HEADER);

    TRY {

        keyBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                          keyLength,
                                          DVD_TAG_SEND_KEY);

        if(keyBuffer == NULL) {

            DebugPrint((1, "IOCTL_DVD_SEND_KEY - couldn't allocate "
                           "%d byte buffer for key\n",
                           keyLength));
            status = STATUS_INSUFFICIENT_RESOURCES;
            LEAVE;
        }

        RtlZeroMemory(keyBuffer, keyLength);

        //
        // keylength is decremented here by two because the
        // datalength does not include the header, which is two
        // bytes.  keylength is immediately incremented later
        // by the same amount.
        //

        keyLength -= 2;
        fourByte = (PFOUR_BYTE) &keyLength;
        keyBuffer->DataLength[0] = fourByte->Byte1;
        keyBuffer->DataLength[1] = fourByte->Byte0;
        keyLength += 2;

        //
        // copy the user's buffer to our own allocated buffer
        //

        RtlMoveMemory(keyBuffer->Data,
                      key->KeyData,
                      key->KeyLength - sizeof(DVD_COPY_PROTECT_KEY));


        NewIrp->MdlAddress = IoAllocateMdl(keyBuffer,
                                           keyLength,
                                           FALSE,
                                           FALSE,
                                           (PIRP) NULL);

        if(NewIrp->MdlAddress == NULL) {
            DebugPrint((1, "IOCTL_DVD_SEND_KEY - couldn't create mdl\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            LEAVE;
        }


        MmBuildMdlForNonPagedPool(NewIrp->MdlAddress);

        Srb->CdbLength = 12;
        Srb->DataBuffer = keyBuffer;
        Srb->DataTransferLength = keyLength;

        Srb->TimeOutValue = fdoExtension->TimeOutValue;
        Srb->SrbFlags = fdoExtension->SrbFlags;
        SET_FLAG(Srb->SrbFlags, SRB_FLAGS_DATA_OUT);

        cdb->REPORT_KEY.OperationCode = SCSIOP_SEND_KEY;

        fourByte = (PFOUR_BYTE) &keyLength;
        cdb->REPORT_KEY.AllocationLength[0] = fourByte->Byte1;
        cdb->REPORT_KEY.AllocationLength[1] = fourByte->Byte0;

        if (key->KeyType == DvdSetRpcKey) {
            DebugPrint((1, "IOCTL_DVD_SEND_KEY - Setting RPC2 drive region\n"));
        } else {
            DebugPrint((1, "IOCTL_DVD_SEND_KEY - key type %x\n", key->KeyType));
        }

        cdb->REPORT_KEY.KeyFormat = (UCHAR)key->KeyType;
        cdb->REPORT_KEY.AGID = (UCHAR) key->SessionId;

        IoCallDriver(fdoExtension->CommonExtension.LowerDeviceObject, NewIrp);

        status = STATUS_SUCCESS;

    } FINALLY {

        if (!NT_SUCCESS(status)) {

            //
            // An error occured during setup - free resources and
            // complete this request.
            //

            if (NewIrp->MdlAddress != NULL) {
                IoFreeMdl(NewIrp->MdlAddress);
            }

            if (keyBuffer != NULL) {
                ExFreePool(keyBuffer);
            }

            ExFreePool(Srb->SenseInfoBuffer);
            ExFreePool(Srb);
            IoFreeIrp(NewIrp);

            OriginalIrp->IoStatus.Information = 0;
            OriginalIrp->IoStatus.Status = status;

            BAIL_OUT(OriginalIrp);
            CdRomCompleteIrpAndStartNextPacketSafely(Fdo, OriginalIrp);

        }
    }

    return;
}


