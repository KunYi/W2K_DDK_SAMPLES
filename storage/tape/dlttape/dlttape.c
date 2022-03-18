//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       dlttape.c
//
//--------------------------------------------------------------------------

/*++

Copyright (c) Microsoft 1998

Module Name:

    dlttape.c

Abstract:

    This module contains device specific routines for DLT drives.

Environment:

    kernel mode only

Revision History:


--*/

#include "minitape.h"

//
//  Internal (module wide) defines that symbolize
//  the Archive QIC drives supported by this module.
//
#define CIP_T860DLT     1  // aka the Cipher T860 DLT
                           // Vendor  ID - CIPHER
                           // Product ID - T860s
                           // Revision   - 430A

#define DEC_TZ85        2  // aka the DEC TZ85
                           // Vendor  ID - DEC
                           // Product ID - THZ02  (C)DEC
                           // Revision   - 4314

#define CIP_DLT2000     3  // aka the Cipher DLT 2000
                           // Vendor  ID - CIPHER
                           // Product ID - DLT2000
                           // Revision   - 8E01

#define DEC_TZ87        4  // aka the DEC TZ87
                           // Vendor  ID - DEC
                           // Product ID - (C)DEC
                           // Revision   - 9104

#define QUANTUM_7000    5
#define QUANTUM_8000    6

#define COMPAQ_8000     7


#define DLT_SUPPORTED_TYPES 2

STORAGE_MEDIA_TYPE DLTMedia[DLT_SUPPORTED_TYPES] = {DLT,CLEANER_CARTRIDGE};

#define DLT_ASCQ_CLEANING_REQUIRED 0x01
#define DLT_ASCQ_CLEANING_REQUEST  0x02
//
//  Function prototype(s) for internal function(s)
//
//
// Minitape extension definition.
//
typedef struct _MINITAPE_EXTENSION {
          ULONG DriveID;
          ULONG Capacity;
          BOOLEAN CompressionOn;
} MINITAPE_EXTENSION, *PMINITAPE_EXTENSION ;

//
// Command extension definition.
//

typedef struct _COMMAND_EXTENSION {

    ULONG   CurrentState;

} COMMAND_EXTENSION, *PCOMMAND_EXTENSION;

//
// DLT Sense data - including addition length
// for remaining tape capacity fields.
//


typedef struct _DLT_SENSE_DATA {
    UCHAR ErrorCode:7;
    UCHAR Valid:1;
    UCHAR SegmentNumber;
    UCHAR SenseKey:4;
    UCHAR Reserved:1;
    UCHAR IncorrectLength:1;
    UCHAR EndOfMedia:1;
    UCHAR FileMark:1;
    UCHAR Information[4];
    UCHAR AdditionalSenseLength;
    UCHAR CommandSpecificInformation[4];
    UCHAR AdditionalSenseCode;
    UCHAR AdditionalSenseCodeQualifier;
    UCHAR SubAssemblyCode;
    UCHAR SenseKeySpecific[3];
    UCHAR InternalStatusCode;
    UCHAR TapeMotionHrs[2];
    UCHAR PowerOnHrs[4];
    UCHAR Remaining[4];
    //UCHAR Reserved3; -- 7000 only
} DLT_SENSE_DATA, *PDLT_SENSE_DATA;

//
// Bitfield def. of InternalStatusCode
//

#define BIT_FLAG_FORMAT   0x80
#define CLEANING_LIGHT_ON 0x01

//
//  Function Prototype(s) for internal function(s)
//
static  ULONG  WhichIsIt(IN PINQUIRYDATA InquiryData);


BOOLEAN
VerifyInquiry(
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    );

VOID
ExtensionInit(
    OUT PVOID                   MinitapeExtension,
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    );

TAPE_STATUS
CreatePartition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
Erase(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetStatus(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
Prepare(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
SetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
SetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
SetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
WriteMarks(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

TAPE_STATUS
GetMediaTypes(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    );

VOID
TapeError(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN OUT  TAPE_STATUS         *LastError
    );


ULONG
DriverEntry(
    IN PVOID Argument1,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Driver entry point for tape minitape driver.

Arguments:

    Argument1   - Supplies the first argument.

    Argument2   - Supplies the second argument.

Return Value:

    Status from TapeClassInitialize()

--*/

{
    TAPE_INIT_DATA_EX  tapeInitData;

    TapeClassZeroMemory( &tapeInitData, sizeof(TAPE_INIT_DATA_EX));

    tapeInitData.InitDataSize = sizeof(TAPE_INIT_DATA_EX);
    tapeInitData.VerifyInquiry = VerifyInquiry;
    tapeInitData.QueryModeCapabilitiesPage = FALSE ;
    tapeInitData.MinitapeExtensionSize = sizeof(MINITAPE_EXTENSION);
    tapeInitData.ExtensionInit = ExtensionInit;
    tapeInitData.DefaultTimeOutValue = 360;
    tapeInitData.TapeError = TapeError ;
    tapeInitData.CommandExtensionSize = sizeof(COMMAND_EXTENSION);
    tapeInitData.CreatePartition = CreatePartition;
    tapeInitData.Erase = Erase;
    tapeInitData.GetDriveParameters = GetDriveParameters;
    tapeInitData.GetMediaParameters = GetMediaParameters;
    tapeInitData.GetPosition = GetPosition;
    tapeInitData.GetStatus = GetStatus;
    tapeInitData.Prepare = Prepare;
    tapeInitData.SetDriveParameters = SetDriveParameters;
    tapeInitData.SetMediaParameters = SetMediaParameters;
    tapeInitData.SetPosition = SetPosition;
    tapeInitData.WriteMarks = WriteMarks;
    tapeInitData.TapeGetMediaTypes = GetMediaTypes;
    tapeInitData.MediaTypesSupported = DLT_SUPPORTED_TYPES;

    return TapeClassInitialize(Argument1, Argument2, &tapeInitData);
}

BOOLEAN
VerifyInquiry(
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    )

/*++

Routine Description:

    This routine examines the given inquiry data to determine whether
    or not the given device is one that may be controller by this driver.

Arguments:

    InquiryData - Supplies the SCSI inquiry data.

Return Value:

    FALSE   - This driver does not recognize the given device.

    TRUE    - This driver recognizes the given device.

--*/

{
    return WhichIsIt(InquiryData) ? TRUE : FALSE;
}

VOID
ExtensionInit(
    OUT PVOID                   MinitapeExtension,
    IN  PINQUIRYDATA            InquiryData,
    IN  PMODE_CAPABILITIES_PAGE ModeCapabilitiesPage
    )

/*++

Routine Description:

    This routine is called at driver initialization time to
    initialize the minitape extension.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

Return Value:

    None.

--*/

{
    PMINITAPE_EXTENSION     extension = MinitapeExtension;

    extension->DriveID = WhichIsIt(InquiryData);
    extension->CompressionOn = FALSE ;
}

TAPE_STATUS
CreatePartition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Create Partition requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{

    DebugPrint((3,"TapeCreatePartition: Enter routine\n"));

    DebugPrint((1,"TapeCreatePartition: operation not supported\n"));

    return TAPE_STATUS_NOT_IMPLEMENTED;

} // end TapeCreatePartition()


TAPE_STATUS
Erase(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for an Erase requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_ERASE        tapeErase = CommandParameters;
    PCDB               cdb = (PCDB) Srb->Cdb;

    DebugPrint((3,"TapeErase: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeErase->Immediate) {
            switch (tapeErase->Type) {
                case TAPE_ERASE_LONG:
                    DebugPrint((3,"TapeErase: immediate\n"));
                    break;

                case TAPE_ERASE_SHORT:
                default:
                    DebugPrint((1,"TapeErase: EraseType, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        switch (tapeErase->Type) {
            case TAPE_ERASE_LONG:
                DebugPrint((3,"TapeErase: long\n"));
                break;

            case TAPE_ERASE_SHORT:
            default:
                DebugPrint((1,"TapeErase: EraseType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->ERASE.OperationCode = SCSIOP_ERASE;
        cdb->ERASE.Immediate = tapeErase->Immediate;
        cdb->ERASE.Long = SETBITON;

        //
        // Set timeout value.
        //

        Srb->TimeOutValue = 23760;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeErase: SendSrb (erase)\n"));

        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
    ASSERT( CallNumber == 1 ) ;

    return TAPE_STATUS_SUCCESS ;

} // end TapeErase()

VOID
TapeError(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      TAPE_STATUS         *LastError
    )

/*++

Routine Description:

    This routine is called for tape requests, to handle tape
    specific errors: it may/can update the status.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    Srb                 - Supplies the SCSI request block.

    LastError           - Status used to set the IRP's completion status.

    Retry - Indicates that this request should be retried.

Return Value:

    None.

--*/

{
    PSENSE_DATA        senseBuffer = Srb->SenseInfoBuffer;
    UCHAR              sensekey = senseBuffer->SenseKey & 0x0F;
    UCHAR              adsenseq = senseBuffer->AdditionalSenseCodeQualifier;
    UCHAR              adsense = senseBuffer->AdditionalSenseCode;

    DebugPrint((3,"TapeError: Enter routine\n"));
    DebugPrint((1,"TapeError: Status 0x%.8X\n", *LastError));


    if (sensekey == SCSI_SENSE_RECOVERED_ERROR) {
        if ((adsense == SCSI_ADSENSE_VENDOR_UNIQUE) && (adsenseq == DLT_ASCQ_CLEANING_REQUEST)) {
            *LastError = TAPE_STATUS_REQUIRES_CLEANING;
        }
    }
    if (sensekey == SCSI_SENSE_MEDIUM_ERROR) {
        if ((adsense == SCSI_ADSENSE_VENDOR_UNIQUE) && (adsenseq == DLT_ASCQ_CLEANING_REQUIRED)) {
            *LastError = TAPE_STATUS_REQUIRES_CLEANING;
        }
    }

    return;
} // end TapeError()


TAPE_STATUS
GetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Drive Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PCOMMAND_EXTENSION          commandExtension = CommandExtension;
    PTAPE_GET_DRIVE_PARAMETERS  tapeGetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PINQUIRYDATA                inquiryBuffer;
    PMODE_DEVICE_CONFIG_PAGE    deviceConfigModeSenseBuffer;
    PMODE_DATA_COMPRESS_PAGE    compressionModeSenseBuffer;
    PREAD_BLOCK_LIMITS_DATA     blockLimitsBuffer;

    DebugPrint((3,"TapeGetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetDriveParams, sizeof(TAPE_GET_DRIVE_PARAMETERS));

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DEVICE_CONFIG_PAGE)) ) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (deviceConfigModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        deviceConfigModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));
        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    if ( CallNumber == 1 ) {

        deviceConfigModeSenseBuffer = Srb->DataBuffer ;

        tapeGetDriveParams->ReportSetmarks =
            (deviceConfigModeSenseBuffer->DeviceConfigPage.RSmk? 1 : 0 );


        if ( !TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DATA_COMPRESS_PAGE)) ) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (compressionModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        compressionModeSenseBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DATA_COMPRESS_PAGE);


        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_DATA_COMPRESS_PAGE) ;
        *RetryFlags |= RETURN_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if ( CallNumber == 2 ) {

        compressionModeSenseBuffer = Srb->DataBuffer ;

        if ((LastError == TAPE_STATUS_SUCCESS) &&
           compressionModeSenseBuffer->DataCompressPage.DCC) {

            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_COMPRESSION;
            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_SET_COMPRESSION;
            tapeGetDriveParams->Compression =
                (compressionModeSenseBuffer->DataCompressPage.DCE? TRUE : FALSE);

        }

        if ( LastError != TAPE_STATUS_SUCCESS ) {
            switch (extension->DriveID) {
                case CIP_T860DLT:
                case DEC_TZ85:
                case DEC_TZ87:
                    if (LastError == TAPE_STATUS_INVALID_DEVICE_REQUEST) {
                        LastError = TAPE_STATUS_SUCCESS;
                        break;
                    }
                    // else: fall through to next case!

                default:
                    DebugPrint((1,"TapeGetDriveParameters: mode sense, SendSrb unsuccessful\n"));
                    return LastError;
            }
        }

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(READ_BLOCK_LIMITS_DATA) ) ) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (blockLimitsBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        blockLimitsBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->CDB6GENERIC.OperationCode = SCSIOP_READ_BLOCK_LIMITS;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (read block limits)\n"));

        Srb->DataTransferLength = sizeof(READ_BLOCK_LIMITS_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

   ASSERT ( CallNumber == 3 ) ;

   blockLimitsBuffer = Srb->DataBuffer ;

   tapeGetDriveParams->MaximumBlockSize = blockLimitsBuffer->BlockMaximumSize[2];
   tapeGetDriveParams->MaximumBlockSize += (blockLimitsBuffer->BlockMaximumSize[1] << 8);
   tapeGetDriveParams->MaximumBlockSize += (blockLimitsBuffer->BlockMaximumSize[0] << 16);

   tapeGetDriveParams->MinimumBlockSize = blockLimitsBuffer->BlockMinimumSize[1];
   tapeGetDriveParams->MinimumBlockSize += (blockLimitsBuffer->BlockMinimumSize[0] << 8);

   tapeGetDriveParams->DefaultBlockSize = 32768;
   if ((extension->DriveID == QUANTUM_7000) ||
       (extension->DriveID == QUANTUM_8000) ||
       (extension->DriveID == COMPAQ_8000)) {

       //
       // All wide adapters should be able to handle this.
       //

       tapeGetDriveParams->DefaultBlockSize = 65536;
   }

   tapeGetDriveParams->FeaturesLow |=
       TAPE_DRIVE_ERASE_LONG       |
       TAPE_DRIVE_ERASE_BOP_ONLY   |
       TAPE_DRIVE_ERASE_IMMEDIATE  |
       TAPE_DRIVE_FIXED_BLOCK      |
       TAPE_DRIVE_VARIABLE_BLOCK   |
       TAPE_DRIVE_WRITE_PROTECT    |
       TAPE_DRIVE_GET_ABSOLUTE_BLK |
       TAPE_DRIVE_GET_LOGICAL_BLK  |
       TAPE_DRIVE_TAPE_CAPACITY    |
       TAPE_DRIVE_TAPE_REMAINING   |
       TAPE_DRIVE_CLEAN_REQUESTS;

   tapeGetDriveParams->FeaturesHigh |=
       TAPE_DRIVE_LOAD_UNLOAD       |
       TAPE_DRIVE_REWIND_IMMEDIATE  |
       TAPE_DRIVE_SET_BLOCK_SIZE    |
       TAPE_DRIVE_LOAD_UNLD_IMMED   |
       TAPE_DRIVE_ABSOLUTE_BLK      |
       TAPE_DRIVE_LOGICAL_BLK       |
       TAPE_DRIVE_END_OF_DATA       |
       TAPE_DRIVE_RELATIVE_BLKS     |
       TAPE_DRIVE_FILEMARKS         |
       TAPE_DRIVE_SEQUENTIAL_FMKS   |
       TAPE_DRIVE_REVERSE_POSITION  |
       TAPE_DRIVE_WRITE_FILEMARKS   |
       TAPE_DRIVE_WRITE_MARK_IMMED;

   if ((extension->DriveID == CIP_T860DLT) ||
       (extension->DriveID == CIP_DLT2000) ||
       (extension->DriveID == QUANTUM_7000)||
       (extension->DriveID == QUANTUM_8000) ||
       (extension->DriveID == COMPAQ_8000)) {

       tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_LOCK_UNLOCK;
   }


   tapeGetDriveParams->FeaturesHigh &= ~TAPE_DRIVE_HIGH_FEATURES;

   DebugPrint((3,"TapeGetDriveParameters: FeaturesLow == 0x%.8X\n",
       tapeGetDriveParams->FeaturesLow));
   DebugPrint((3,"TapeGetDriveParameters: FeaturesHigh == 0x%.8X\n",
       tapeGetDriveParams->FeaturesHigh));

   return TAPE_STATUS_SUCCESS;

} // end TapeGetDriveParameters()

TAPE_STATUS
GetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Media Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_GET_MEDIA_PARAMETERS  tapeGetMediaParams = CommandParameters;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;
    PDLT_SENSE_DATA             senseData;
    ULONG                       remaining;
    ULONG                       temp ;
    PCDB                        cdb = (PCDB)Srb->Cdb;

    DebugPrint((3,"TapeGetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        return TAPE_STATUS_CHECK_TEST_UNIT_READY;
    }

    if (CallNumber == 1) {

        TapeClassZeroMemory(tapeGetMediaParams, sizeof(TAPE_GET_MEDIA_PARAMETERS));

        if ( !TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_PARM_READ_WRITE_DATA)) ) {

            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (modeBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer ;


        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_PARM_READ_WRITE_DATA);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));
        Srb->DataTransferLength = sizeof(MODE_PARM_READ_WRITE_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 2) {

        UCHAR senseDataSize;

        modeBuffer = Srb->DataBuffer ;

        tapeGetMediaParams->PartitionCount = 0 ;

        tapeGetMediaParams->BlockSize = modeBuffer->ParameterListBlock.BlockLength[2];
        tapeGetMediaParams->BlockSize += (modeBuffer->ParameterListBlock.BlockLength[1] << 8);
        tapeGetMediaParams->BlockSize += (modeBuffer->ParameterListBlock.BlockLength[0] << 16);

        tapeGetMediaParams->WriteProtected =
            ((modeBuffer->ParameterListHeader.DeviceSpecificParameter >> 7) & 0x01);

        senseDataSize = sizeof(DLT_SENSE_DATA);
        if ((extension->DriveID == QUANTUM_7000) ||
            (extension->DriveID == QUANTUM_8000) ||
            (extension->DriveID == COMPAQ_8000)){

            //
            // The 7000 & 8000 have one additional 'reserved' byte.
            //

            senseDataSize += 1;
        }

        //
        // Build a request sense to get remaining values.
        //

        if (!TapeClassAllocateSrbBuffer( Srb, senseDataSize)) {
            DebugPrint((1,
                       "GetRemaining Size: insufficient resources (SenseData)\n"));
            return TAPE_STATUS_SUCCESS;
        }

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->ScsiStatus = Srb->SrbStatus = 0;
        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_REQUEST_SENSE;
        cdb->CDB6GENERIC.CommandUniqueBytes[2] = senseDataSize;
        Srb->DataTransferLength = senseDataSize;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"GetRemainingSize: SendSrb (request sense)\n"));

        *RetryFlags |= RETURN_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 3) {
        if (LastError == TAPE_STATUS_SUCCESS) {

            //
            // As a rewind was sent, get the remaining capacity from BOP.
            //

            senseData = Srb->DataBuffer;

            DebugPrint((1,
                       "GetMediaParameters: remaining Bytes %x %x %x %x\n",
                       senseData->Remaining[0],
                       senseData->Remaining[1],
                       senseData->Remaining[2],
                       senseData->Remaining[3]));

            remaining =  (senseData->Remaining[0] << 24);
            remaining += (senseData->Remaining[1] << 16);
            remaining += (senseData->Remaining[2] << 8);
            remaining += (senseData->Remaining[3]);


            DebugPrint((1,
                       "GetMediaParameters: remaining Ulong %x\n",
                       remaining));

            tapeGetMediaParams->Capacity.LowPart = extension->Capacity;
            tapeGetMediaParams->Capacity.QuadPart <<= 12;

            DebugPrint((1,
                       "Capacity returned %x %x\n",
                       tapeGetMediaParams->Capacity.HighPart,
                       tapeGetMediaParams->Capacity.LowPart));

            tapeGetMediaParams->Remaining.LowPart = remaining;
            tapeGetMediaParams->Remaining.QuadPart <<= 12;


            DebugPrint((1,
                       "Remaining returned %x %x\n",
                       tapeGetMediaParams->Remaining.HighPart,
                       tapeGetMediaParams->Remaining.LowPart));

        }
    }


    return TAPE_STATUS_SUCCESS ;

} // end TapeGetMediaParameters()


TAPE_STATUS
GetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Position requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_GET_POSITION          tapeGetPosition = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PTAPE_POSITION_DATA         logicalBuffer;
    ULONG                       type;

    DebugPrint((3,"TapeGetPosition: Enter routine\n"));

    if (CallNumber == 0) {


        type = tapeGetPosition->Type;
        TapeClassZeroMemory(tapeGetPosition, sizeof(TAPE_GET_POSITION));
        tapeGetPosition->Type = type;

        return TAPE_STATUS_CHECK_TEST_UNIT_READY;

    }
    if ( CallNumber == 1 ) {

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->WRITE_TAPE_MARKS.OperationCode = SCSIOP_WRITE_FILEMARKS;
        cdb->WRITE_TAPE_MARKS.Immediate = FALSE ;

        cdb->WRITE_TAPE_MARKS.TransferLength[0] = 0 ;
        cdb->WRITE_TAPE_MARKS.TransferLength[1] = 0 ;
        cdb->WRITE_TAPE_MARKS.TransferLength[2] = 0 ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetPosition: flushing TapeWriteMarks: SendSrb\n"));
        Srb->DataTransferLength = 0 ;
        *RetryFlags |= IGNORE_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
    if ( CallNumber == 2 ) {

        switch (tapeGetPosition->Type) {

            case TAPE_ABSOLUTE_POSITION:
            case TAPE_LOGICAL_POSITION:

                //
                // Zero CDB in SRB on stack.
                //

                TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

                if (!TapeClassAllocateSrbBuffer( Srb, sizeof(TAPE_POSITION_DATA)) ) {
                    DebugPrint((1,"TapeGetPosition: insufficient resources (logicalBuffer)\n"));
                    return TAPE_STATUS_INSUFFICIENT_RESOURCES;
                }

                logicalBuffer = Srb->DataBuffer ;

                //
                // Prepare SCSI command (CDB)
                //

                Srb->CdbLength = CDB10GENERIC_LENGTH;

                cdb->READ_POSITION.Operation = SCSIOP_READ_POSITION;

                //
                // Send SCSI command (CDB) to device
                //

                DebugPrint((3,"TapeGetPosition: SendSrb (read position)\n"));
                Srb->DataTransferLength = sizeof(TAPE_POSITION_DATA) ;

                return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
                break ;

            default:
                DebugPrint((1,"TapeGetPosition: PositionType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;

        }
    }
    ASSERT( CallNumber == 3 ) ;

    logicalBuffer = Srb->DataBuffer ;

    tapeGetPosition->Offset.HighPart = 0;

    REVERSE_BYTES((PFOUR_BYTE)&tapeGetPosition->Offset.LowPart,
                          (PFOUR_BYTE)logicalBuffer->FirstBlock);

    return TAPE_STATUS_SUCCESS ;

} // end TapeGetPosition()


TAPE_STATUS
GetStatus(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Get Status requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PCOMMAND_EXTENSION commandExtension = CommandExtension;
    PCDB            cdb = (PCDB)Srb->Cdb;
    PDLT_SENSE_DATA senseData;

    DebugPrint((3,"TapeGetStatus: Enter routine\n"));

    if (CallNumber == 0) {

        *RetryFlags |= RETURN_ERRORS;
        return TAPE_STATUS_CHECK_TEST_UNIT_READY;
    }

    if (CallNumber == 1) {

        commandExtension->CurrentState = LastError;

        //
        // Issue a request sense to get the cleaning info bits.
        //

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(DLT_SENSE_DATA))) {
            DebugPrint((1,
                        "GetStatus: Insufficient resources (SenseData)\n"));
            return TAPE_STATUS_SUCCESS;
        }

        //
        // Prepare SCSI command (CDB)
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        Srb->ScsiStatus = Srb->SrbStatus = 0;
        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.OperationCode = SCSIOP_REQUEST_SENSE;
        cdb->CDB6GENERIC.CommandUniqueBytes[2] = sizeof(DLT_SENSE_DATA);

        //
        // Send SCSI command (CDB) to device
        //

        Srb->DataTransferLength = sizeof(DLT_SENSE_DATA);
        *RetryFlags |= RETURN_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    } else if (CallNumber == 2) {
        if ((commandExtension->CurrentState == TAPE_STATUS_SUCCESS)  ||
            (commandExtension->CurrentState == TAPE_STATUS_NO_MEDIA)) {

            //
            // Return needs cleaning status if necessary, but only if
            // no other errors are present (with the exception of no media as the
            // drive will spit out AME media when in this state).
            //

            senseData = Srb->DataBuffer;

            //
            // Determine if the clean bit is set.
            //

            if (senseData->InternalStatusCode & BIT_FLAG_FORMAT) {
                if (senseData->InternalStatusCode & CLEANING_LIGHT_ON) {
                    DebugPrint((1,
                               "Drive reports needs cleaning - InternalStatusCode %x\n",
                               senseData->InternalStatusCode));

                    return TAPE_STATUS_REQUIRES_CLEANING;
                }
            }

            return (commandExtension->CurrentState);
        } else {

            //
            // Return the saved error status.
            //

            return commandExtension->CurrentState;
        }
    }

    return TAPE_STATUS_SUCCESS;
}

TAPE_STATUS
Prepare(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Prepare requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_PREPARE               tapePrepare = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PDLT_SENSE_DATA             senseData;
    ULONG                       remaining;
    ULONG                       temp ;

    DebugPrint((3,"TapePrepare: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapePrepare->Immediate) {
            switch (tapePrepare->Operation) {
                case TAPE_LOAD:
                case TAPE_UNLOAD:
                    DebugPrint((3,"TapePrepare: immediate\n"));
                    break;

                case TAPE_LOCK:
                case TAPE_UNLOCK:
                    if ((extension->DriveID == DEC_TZ85) || (extension->DriveID == DEC_TZ87) ){
                        DebugPrint((1,"TapePrepare: Operation, immediate -- operation not supported\n"));
                        return TAPE_STATUS_NOT_IMPLEMENTED;
                    }
                    DebugPrint((3,"TapePrepare: immediate\n"));
                    break;

                case TAPE_TENSION:
                default:
                    DebugPrint((1,"TapePrepare: Operation, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->CDB6GENERIC.Immediate = tapePrepare->Immediate;

        switch (tapePrepare->Operation) {
            case TAPE_LOAD:
                DebugPrint((3,"TapePrepare: Operation == load\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                break;

            case TAPE_UNLOAD:
                DebugPrint((3,"TapePrepare: Operation == unload\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                break;

            case TAPE_LOCK:
                if ((extension->DriveID == DEC_TZ85) || (extension->DriveID == DEC_TZ87) ){
                    DebugPrint((1,"TapePrepare: Operation -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
                }

                DebugPrint((3,"TapePrepare: Operation == lock\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                break;

            case TAPE_UNLOCK:
                if ((extension->DriveID == DEC_TZ85) || (extension->DriveID == DEC_TZ87) ){
                    DebugPrint((1,"TapePrepare: Operation -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
                }

                DebugPrint((3,"TapePrepare: Operation == unlock\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                break;

            default:
                DebugPrint((1,"TapePrepare: Operation -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapePrepare: SendSrb (Operation)\n"));
        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 1) {
        if (tapePrepare->Operation == TAPE_LOAD) {

            //
            // Issue a request sense to get the cleaning info bits.
            //

            if (!TapeClassAllocateSrbBuffer( Srb, sizeof(DLT_SENSE_DATA))) {
                DebugPrint((1,
                            "GetStatus: Insufficient resources (SenseData)\n"));
                return TAPE_STATUS_SUCCESS;
            }

            //
            // Prepare SCSI command (CDB)
            //

            TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

            Srb->ScsiStatus = Srb->SrbStatus = 0;
            Srb->CdbLength = CDB6GENERIC_LENGTH;

            cdb->CDB6GENERIC.OperationCode = SCSIOP_REQUEST_SENSE;
            cdb->CDB6GENERIC.CommandUniqueBytes[2] = sizeof(DLT_SENSE_DATA);

            //
            // Send SCSI command (CDB) to device
            //

            Srb->DataTransferLength = sizeof(DLT_SENSE_DATA);
            *RetryFlags |= RETURN_ERRORS;

            return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

        } else {

            return TAPE_STATUS_CALLBACK ;
        }

    }

    if (LastError == TAPE_STATUS_SUCCESS) {

        senseData = Srb->DataBuffer;

        remaining =  (senseData->Remaining[0] << 24);
        remaining += (senseData->Remaining[1] << 16);
        remaining += (senseData->Remaining[2] << 8);
        remaining += (senseData->Remaining[3]);

        extension->Capacity = remaining;
    }

    return TAPE_STATUS_SUCCESS;

} // end TapePrepare()


TAPE_STATUS
SetDriveParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Set Drive Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PCOMMAND_EXTENSION          commandExtension = CommandExtension;
    PTAPE_SET_DRIVE_PARAMETERS  tapeSetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_DATA_COMPRESS_PAGE    compressionBuffer;
    PMODE_DEVICE_CONFIG_PAGE    configBuffer;

    DebugPrint((3,"TapeSetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DEVICE_CONFIG_PAGE)) ) {

            DebugPrint((1,"TapeSetDriveParameters: insufficient resources (configBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        configBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));
        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if ( CallNumber == 1 ) {

        configBuffer = Srb->DataBuffer ;

        configBuffer->ParameterListHeader.ModeDataLength = 0;
        configBuffer->ParameterListHeader.MediumType = 0;
        configBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        configBuffer->ParameterListHeader.BlockDescriptorLength = 0;

        configBuffer->DeviceConfigPage.PageCode = MODE_PAGE_DEVICE_CONFIG;
        configBuffer->DeviceConfigPage.PageLength = 0x0E;

        if (tapeSetDriveParams->ReportSetmarks) {
            configBuffer->DeviceConfigPage.RSmk = SETBITON;
        } else {
            configBuffer->DeviceConfigPage.RSmk = SETBITOFF;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_DEVICE_CONFIG_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 2 ) {

        if ( !TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_DATA_COMPRESS_PAGE)) ) {
            DebugPrint((1,"TapeSetDriveParameters: insufficient resources (compressionBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        compressionBuffer = Srb->DataBuffer ;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.Dbd = SETBITON;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));
        Srb->DataTransferLength = sizeof(MODE_DATA_COMPRESS_PAGE) ;

        *RetryFlags |= RETURN_ERRORS;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 3 ) {

        if ( LastError != TAPE_STATUS_SUCCESS ) {

            switch (extension->DriveID) {
                case CIP_T860DLT:
                case DEC_TZ85:
                case DEC_TZ87:
                    if (LastError == TAPE_STATUS_INVALID_DEVICE_REQUEST) {
                        return TAPE_STATUS_SUCCESS ;
                        break;
                    }
                    // else: fall through to next case!

                default:
                    DebugPrint((1,"TapeSetDriveParameters: mode sense, SendSrb unsuccessful\n"));
            }

            return LastError;

        }

        compressionBuffer = Srb->DataBuffer ;

        compressionBuffer->ParameterListHeader.ModeDataLength = 0;
        compressionBuffer->ParameterListHeader.MediumType = 0;
        compressionBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        compressionBuffer->ParameterListHeader.BlockDescriptorLength = 0;

        compressionBuffer->DataCompressPage.PageCode = MODE_PAGE_DATA_COMPRESS;
        compressionBuffer->DataCompressPage.PageLength = 0x0E;

        if (tapeSetDriveParams->Compression) {
            compressionBuffer->DataCompressPage.DCE = SETBITON;
            compressionBuffer->DataCompressPage.CompressionAlgorithm[3]= 0x10;
        } else {
            compressionBuffer->DataCompressPage.DCE = SETBITOFF;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_DATA_COMPRESS_PAGE);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));

        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = sizeof(MODE_DATA_COMPRESS_PAGE) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT(CallNumber == 4 ) ;
    return TAPE_STATUS_SUCCESS ;

} // end TapeSetDriveParameters()



TAPE_STATUS
SetMediaParameters(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Set Media Parameters requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_SET_MEDIA_PARAMETERS  tapeSetMediaParams = CommandParameters;
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;

    DebugPrint((3,"TapeSetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        if ((extension->DriveID == QUANTUM_7000) ||
            (extension->DriveID == QUANTUM_8000) ||
            (extension->DriveID == COMPAQ_8000)) {

            //
            // Ensure that the new block size isn't 'odd'.
            //

            if (tapeSetMediaParams->BlockSize & 0x01) {
                DebugPrint((1,
                           "SetMediaParameters: Attempting to set odd block size - %x\n",
                           tapeSetMediaParams->BlockSize));

                return TAPE_STATUS_INVALID_DEVICE_REQUEST;
            }
        }
        return TAPE_STATUS_CHECK_TEST_UNIT_READY ;
    }

    if (CallNumber == 1) {
        if (!TapeClassAllocateSrbBuffer( Srb, sizeof(MODE_PARM_READ_WRITE_DATA)) ) {
            DebugPrint((1,"TapeSetMediaParameters: insufficient resources (modeBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer ;

        modeBuffer->ParameterListHeader.ModeDataLength = 0;
        modeBuffer->ParameterListHeader.MediumType = 0;
        modeBuffer->ParameterListHeader.DeviceSpecificParameter = 0x10;
        modeBuffer->ParameterListHeader.BlockDescriptorLength =
            MODE_BLOCK_DESC_LENGTH;

        modeBuffer->ParameterListBlock.BlockLength[0] =
            (UCHAR)((tapeSetMediaParams->BlockSize >> 16) & 0xFF);
        modeBuffer->ParameterListBlock.BlockLength[1] =
            (UCHAR)((tapeSetMediaParams->BlockSize >> 8) & 0xFF);
        modeBuffer->ParameterListBlock.BlockLength[2] =
            (UCHAR)(tapeSetMediaParams->BlockSize & 0xFF);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.ParameterListLength = sizeof(MODE_PARM_READ_WRITE_DATA);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetMediaParameters: SendSrb (mode select)\n"));
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
        Srb->DataTransferLength = sizeof(MODE_PARM_READ_WRITE_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT(CallNumber == 2 ) ;

    return TAPE_STATUS_SUCCESS;

} // end TapeSetMediaParameters()


TAPE_STATUS
SetPosition(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Set Position requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PMINITAPE_EXTENSION         extension = MinitapeExtension;
    PTAPE_SET_POSITION          tapeSetPosition = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    ULONG                       tapePositionVector;
    PDLT_SENSE_DATA             senseData;
    ULONG                       remaining;

    DebugPrint((3,"TapeSetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeSetPosition->Immediate) {
           switch (tapeSetPosition->Method) {
        case TAPE_REWIND:
        case TAPE_LOGICAL_BLOCK:
        case TAPE_ABSOLUTE_BLOCK: {     
           DebugPrint((3,"TapeSetPosition: immediate\n"));
           break;
        }

                case TAPE_SPACE_END_OF_DATA:
                case TAPE_SPACE_RELATIVE_BLOCKS:
                case TAPE_SPACE_FILEMARKS:
                case TAPE_SPACE_SEQUENTIAL_FMKS: {
           DebugPrint((3, 
                   "TapeSetPosition-Operation Immediate : %x.\n",
                   tapeSetPosition->Method));
           break;
        }

                case TAPE_SPACE_SETMARKS:
                case TAPE_SPACE_SEQUENTIAL_SMKS:
                default:
                    DebugPrint((1,
                "TapeSetPosition: PositionMethod, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        tapePositionVector = tapeSetPosition->Offset.LowPart;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->CDB6GENERIC.Immediate = tapeSetPosition->Immediate;

        switch (tapeSetPosition->Method) {
            case TAPE_REWIND:
                DebugPrint((3,"TapeSetPosition: method == rewind\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;
                break;

            case TAPE_ABSOLUTE_BLOCK:
            case TAPE_LOGICAL_BLOCK:
                DebugPrint((3,"TapeSetPosition: method == locate (logical)\n"));
                Srb->CdbLength = CDB10GENERIC_LENGTH;
                cdb->LOCATE.OperationCode = SCSIOP_LOCATE;
                cdb->LOCATE.LogicalBlockAddress[0] =
                    (UCHAR)((tapePositionVector >> 24) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[1] =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[2] =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[3] =
                    (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 11800;
                break;

            case TAPE_SPACE_END_OF_DATA:
                DebugPrint((3,"TapeSetPosition: method == space to end-of-data\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 3;
                Srb->TimeOutValue = 11800;
                break;

            case TAPE_SPACE_RELATIVE_BLOCKS:

                DebugPrint((3,"TapeSetPosition: method == space blocks\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 0;

                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                    (UCHAR)(tapePositionVector & 0xFF);

                Srb->TimeOutValue = 11800;
                break;

            case TAPE_SPACE_FILEMARKS:
                DebugPrint((3,"TapeSetPosition: method == space filemarks\n"));
                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 1;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                    (UCHAR)((tapePositionVector >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =
                    (UCHAR)((tapePositionVector >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                   (UCHAR)(tapePositionVector & 0xFF);
                Srb->TimeOutValue = 11800;
               break;

           case TAPE_SPACE_SEQUENTIAL_FMKS:
               DebugPrint((3,"TapeSetPosition: method == space sequential filemarks\n"));
               cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
               cdb->SPACE_TAPE_MARKS.Code = 2;
               cdb->SPACE_TAPE_MARKS.NumMarksMSB =
                   (UCHAR)((tapePositionVector >> 16) & 0xFF);
               cdb->SPACE_TAPE_MARKS.NumMarks =
                   (UCHAR)((tapePositionVector >> 8) & 0xFF);
               cdb->SPACE_TAPE_MARKS.NumMarksLSB =
                   (UCHAR)(tapePositionVector & 0xFF);
               Srb->TimeOutValue = 11800;
               break;

            default:
                DebugPrint((1,"TapeSetPosition: PositionMethod -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
         }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetPosition: SendSrb (method)\n"));
        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 1) {

        if (tapeSetPosition->Method == TAPE_REWIND) {

            //
            // Build a request sense to get remaining values.
            //

            if (!TapeClassAllocateSrbBuffer( Srb, sizeof(DLT_SENSE_DATA)) ) {
                DebugPrint((1,
                           "GetRemaining Size: insufficient resources (SenseData)\n"));
                return TAPE_STATUS_SUCCESS;
            }

            //
            // Prepare SCSI command (CDB)
            //

            TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

            Srb->ScsiStatus = Srb->SrbStatus = 0;
            Srb->CdbLength = CDB6GENERIC_LENGTH;

            cdb->CDB6GENERIC.OperationCode = SCSIOP_REQUEST_SENSE;
            cdb->CDB6GENERIC.CommandUniqueBytes[2] = sizeof(DLT_SENSE_DATA);

            //
            // Send SCSI command (CDB) to device
            //

            DebugPrint((3,"GetRemainingSize: SendSrb (request sense)\n"));

            Srb->DataTransferLength = sizeof(DLT_SENSE_DATA);
            *RetryFlags |= RETURN_ERRORS;

            return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
        } else if (tapeSetPosition->Method == TAPE_SPACE_SEQUENTIAL_FMKS) {
       DebugPrint((3, "TAPE_SPACE_SEQUENTIAL_FMKS Success.\n"));
    }
    }

    if (CallNumber == 2) {
        if (LastError == TAPE_STATUS_SUCCESS) {

            //
            // As a rewind was sent, get the remaining capacity from BOP.
            //

            senseData = Srb->DataBuffer;

            remaining =  (senseData->Remaining[0] << 24);
            remaining += (senseData->Remaining[1] << 16);
            remaining += (senseData->Remaining[2] << 8);
            remaining += (senseData->Remaining[3]);

            extension->Capacity = remaining;
        }
    }
    return TAPE_STATUS_SUCCESS ;

} // end TapeSetPosition()


TAPE_STATUS
WriteMarks(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for a Write Marks requests.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/

{
    PTAPE_WRITE_MARKS  tapeWriteMarks = CommandParameters;
    PCDB               cdb = (PCDB)Srb->Cdb;
    LARGE_INTEGER      timeout ;

    DebugPrint((3,"TapeWriteMarks: Enter routine\n"));

    if (CallNumber == 0) {

        switch (tapeWriteMarks->Type) {
            case TAPE_FILEMARKS:
                DebugPrint((3,"TapeWriteMarks: immediate\n"));
                break;

            case TAPE_SETMARKS:
            case TAPE_SHORT_FILEMARKS:
            case TAPE_LONG_FILEMARKS:
            default:
                DebugPrint((1,"TapeWriteMarks: TapemarkType, immediate -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->WRITE_TAPE_MARKS.OperationCode = SCSIOP_WRITE_FILEMARKS;

        cdb->WRITE_TAPE_MARKS.Immediate = tapeWriteMarks->Immediate;


        cdb->WRITE_TAPE_MARKS.TransferLength[0] =
            (UCHAR)((tapeWriteMarks->Count >> 16) & 0xFF);
        cdb->WRITE_TAPE_MARKS.TransferLength[1] =
            (UCHAR)((tapeWriteMarks->Count >> 8) & 0xFF);
        cdb->WRITE_TAPE_MARKS.TransferLength[2] =
            (UCHAR)(tapeWriteMarks->Count & 0xFF);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeWriteMarks: SendSrb (TapemarkType)\n"));
        Srb->DataTransferLength = 0 ;

        if ( tapeWriteMarks->Count == 0 ) {
            *RetryFlags |= IGNORE_ERRORS;
        }

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT (CallNumber == 1 ) ;
    /* we may need to add a delay for the tz87 */

    return TAPE_STATUS_SUCCESS ;

} // end TapeWriteMarks()



TAPE_STATUS
GetMediaTypes(
    IN OUT  PVOID               MinitapeExtension,
    IN OUT  PVOID               CommandExtension,
    IN OUT  PVOID               CommandParameters,
    IN OUT  PSCSI_REQUEST_BLOCK Srb,
    IN      ULONG               CallNumber,
    IN      TAPE_STATUS         LastError,
    IN OUT  PULONG              RetryFlags
    )

/*++

Routine Description:

    This is the TAPE COMMAND routine for TapeGetMediaTypes.

Arguments:

    MinitapeExtension   - Supplies the minitape extension.

    CommandExtension    - Supplies the ioctl extension.

    CommandParameters   - Supplies the command parameters.

    Srb                 - Supplies the SCSI request block.

    CallNumber          - Supplies the call number.

    RetryFlags          - Supplies the retry flags.

Return Value:

    TAPE_STATUS_SEND_SRB_AND_CALLBACK   - The SRB is ready to be sent
                                            (a callback is requested.)

    TAPE_STATUS_SUCCESS                 - The command is complete and
                                            successful.

    Otherwise                           - An error occurred.

--*/
{
    PGET_MEDIA_TYPES  mediaTypes = CommandParameters;
    PDEVICE_MEDIA_INFO mediaInfo = &mediaTypes->MediaInfo[0];
    PCDB               cdb = (PCDB)Srb->Cdb;

    DebugPrint((3,"GetMediaTypes: Enter routine\n"));

    if (CallNumber == 0) {

        *RetryFlags = RETURN_ERRORS;
        return TAPE_STATUS_CHECK_TEST_UNIT_READY ;
    }

    if ((LastError == TAPE_STATUS_BUS_RESET) || (LastError == TAPE_STATUS_MEDIA_CHANGED) || (LastError == TAPE_STATUS_SUCCESS)) {
        if (CallNumber == 1) {

            //
            // Zero the buffer, including the first media info.
            //

            TapeClassZeroMemory(mediaTypes, sizeof(GET_MEDIA_TYPES));

            //
            // Build mode sense for medium partition page.
            //

            if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_DEVICE_CONFIG_PAGE_PLUS))) {

                DebugPrint((1,
                            "Dlttape.GetMediaTypes: Couldn't allocate Srb Buffer\n"));
                return TAPE_STATUS_INSUFFICIENT_RESOURCES;
            }

            TapeClassZeroMemory(Srb->DataBuffer, sizeof(MODE_DEVICE_CONFIG_PAGE_PLUS));
            Srb->CdbLength = CDB6GENERIC_LENGTH;
            Srb->DataTransferLength = sizeof(MODE_DEVICE_CONFIG_PAGE_PLUS);

            cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
            cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
            cdb->MODE_SENSE.AllocationLength = (UCHAR)Srb->DataTransferLength;

            return TAPE_STATUS_SEND_SRB_AND_CALLBACK;

        }
    }

    if ((CallNumber == 2) || ((CallNumber == 1) && (LastError != TAPE_STATUS_SUCCESS))) {

        ULONG i;
        ULONG currentMedia;
        ULONG blockSize;
        UCHAR mediaType;
        PMODE_DEVICE_CONFIG_PAGE_PLUS configInformation = Srb->DataBuffer;

        mediaTypes->DeviceType = 0x0000001f; // FILE_DEVICE_TAPE;

        //
        // Currently, two types (either known dlt/cleaner or unknown) are returned.
        //

        mediaTypes->MediaInfoCount = 2;

        //
        // Determine the media type currently loaded.
        //

        if ( LastError == TAPE_STATUS_SUCCESS ) {

            mediaType = configInformation->ParameterListHeader.MediumType;
            blockSize = configInformation->ParameterListBlock.BlockLength[2];
            blockSize |= (configInformation->ParameterListBlock.BlockLength[1] << 8);
            blockSize |= (configInformation->ParameterListBlock.BlockLength[0] << 16);

            DebugPrint((1,
                        "GetMediaTypes: MediaType %x, Current Block Size %x\n",
                        mediaType,
                        blockSize));

            switch (mediaType) {
                case 0:

                    //
                    // Check density code, just in case - DEC TLZ86/87
                    //

                    if ((configInformation->ParameterListBlock.DensityCode) != 0) {

                        currentMedia = DLT;
                    } else {
                        currentMedia = 0;
                    }

                    break;

                case 0x81:

                    //
                    // Cleaning cartridge.
                    //

                    currentMedia = CLEANER_CARTRIDGE;
                    break;

                case 0x82:
                case 0x83:
                case 0x84:
                case 0x85:

                    //
                    // DLT 1,2,3,or 4.
                    //

                    currentMedia = DLT;
                    break;

                default:

                    //
                    // Unknown
                    //

                    currentMedia = 0;
                    break;
            }
        } else {
            currentMedia = 0;
        }

        //
        // fill in buffer based on spec. values
        //

        for (i = 0; i < DLT_SUPPORTED_TYPES; i++) {

            TapeClassZeroMemory(mediaInfo, sizeof(DEVICE_MEDIA_INFO));

            mediaInfo->DeviceSpecific.TapeInfo.MediaType = DLTMedia[i];

            //
            // Indicate that the media potentially is read/write
            //

            mediaInfo->DeviceSpecific.TapeInfo.MediaCharacteristics = MEDIA_READ_WRITE;

            if (DLTMedia[i] == (STORAGE_MEDIA_TYPE)currentMedia) {

                mediaInfo->DeviceSpecific.TapeInfo.BusSpecificData.ScsiInformation.MediumType = mediaType;
                mediaInfo->DeviceSpecific.TapeInfo.BusSpecificData.ScsiInformation.DensityCode =
                    configInformation->ParameterListBlock.DensityCode;

                mediaInfo->DeviceSpecific.TapeInfo.BusType = 0x01;

                //
                // This media type is currently mounted.
                //

                mediaInfo->DeviceSpecific.TapeInfo.MediaCharacteristics |= MEDIA_CURRENTLY_MOUNTED;

                //
                // Indicate whether the media is write protected.
                //

                mediaInfo->DeviceSpecific.TapeInfo.MediaCharacteristics |=
                    ((configInformation->ParameterListHeader.DeviceSpecificParameter >> 7) & 0x01) ? MEDIA_WRITE_PROTECTED : 0;

                //
                // Fill in current blocksize.
                //

                mediaInfo->DeviceSpecific.TapeInfo.CurrentBlockSize = blockSize;
            }

            //
            // Advance to next array entry.
            //

            mediaInfo++;
        }
    }

    return TAPE_STATUS_SUCCESS;
}


static
ULONG
WhichIsIt(
    IN PINQUIRYDATA InquiryData
    )

/*++
Routine Description:

    This routine determines a drive's identity from the Product ID field
    in its inquiry data.

Arguments:

    InquiryData (from an Inquiry command)

Return Value:

    driveID

--*/

{
    if (TapeClassCompareMemory(InquiryData->VendorId,"CIPHER  ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"T860",4) == 4) {
            return CIP_T860DLT;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"L860",4) == 4) {
            return CIP_T860DLT;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TZ86",4) == 4) {
            return DEC_TZ85;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2000",7) == 7) {
            return CIP_DLT2000;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"DEC",3) == 3) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"THZ02",5) == 5) {
            return DEC_TZ85;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TZ86",4) == 4) {
            return DEC_TZ85;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TZ87",4) == 4) {
            return DEC_TZ87;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TZ88",4) == 4) {
            return DEC_TZ87;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TZ89",4) == 4) {
            return QUANTUM_7000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2000",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2500",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2700",7) == 7) {
            return CIP_DLT2000;
        }
        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT4000",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT4500",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT4700",7) == 7) {
            return CIP_DLT2000;
        }
    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"Quantum",7) == 7) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2000",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2500",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT2700",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT4000",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT4500",7) == 7) {
            return CIP_DLT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT4700",7) == 7) {
            return CIP_DLT2000;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"QUANTUM",7) == 7) {
        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT7000",7) == 7) {
            return QUANTUM_7000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT8000",7) == 7) {
            return QUANTUM_8000;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"COMPAQ",6) == 6) {
        if (TapeClassCompareMemory(InquiryData->ProductId,"DLT8000",7) == 7) {
            return COMPAQ_8000;
        }
    }

    return 0;
}

