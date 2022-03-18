//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       ppa3x.c
//
//--------------------------------------------------------------------------

#include "pch.h"

VOID
PptLegacyZipClockDiskModeByte(
    PUCHAR  Controller,
    UCHAR   ModeByte
    )
{
    WRITE_PORT_UCHAR( Controller, ModeByte );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)DCR_NOT_INIT );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_NOT_INIT | DCR_AUTOFEED) );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)DCR_NOT_INIT );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_NOT_INIT | DCR_SELECT_IN) );

} // end PptLegacyZipClockDiskModeByte()

VOID
PptLegacyZipClockPrtModeByte(
    PUCHAR  Controller,
    UCHAR   ModeByte
    )
{
    WRITE_PORT_UCHAR( Controller, ModeByte );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_SELECT_IN | DCR_NOT_INIT) );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_SELECT_IN | DCR_NOT_INIT | DCR_AUTOFEED) );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_SELECT_IN | DCR_NOT_INIT) );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)DCR_NOT_INIT );
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_SELECT_IN | DCR_NOT_INIT) );

} // end PptLegacyZipClockPrtModeByte()

VOID
PptLegacyZipSetDiskMode(
    PUCHAR  Controller,
    UCHAR   Mode
    )
{
    ULONG i;

    for ( i = 0; i < LEGACYZIP_MODE_LEN; i++ ) {
        PptLegacyZipClockDiskModeByte( Controller, LegacyZipModeQualifier[i] );
    }

    PptLegacyZipClockDiskModeByte( Controller, Mode );

} // end of PptLegacyZipSetDiskMode()

BOOLEAN
PptLegacyZipCheckDevice(
    PUCHAR  Controller
    )
{
    WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)(DCR_NOT_INIT | DCR_AUTOFEED) );

    if ( (READ_PORT_UCHAR( Controller+DSR_OFFSET ) & DSR_NOT_FAULT) == DSR_NOT_FAULT ) {

        WRITE_PORT_UCHAR( Controller+DCR_OFFSET, (UCHAR)DCR_NOT_INIT );

        if ( (READ_PORT_UCHAR( Controller+DSR_OFFSET ) & DSR_NOT_FAULT) != DSR_NOT_FAULT ) {
            // A device was found
            return TRUE;
        }
    }

    // No device is there
    return FALSE;

} // end PptLegacyZipCheckDevice()

NTSTATUS
PptTrySelectLegacyZip(
    IN  PVOID   Context,
    IN  PVOID   TrySelectCommand
    )
{
    PDEVICE_EXTENSION           Extension   = Context;
    PPARALLEL_1284_COMMAND      Command     = TrySelectCommand;
    NTSTATUS                    Status      = STATUS_SUCCESS; // default success
    PUCHAR                      Controller  = Extension->PortInfo.Controller;
    SYNCHRONIZED_COUNT_CONTEXT  SyncContext;
    KIRQL                       CancelIrql;

    PptDump2( PARLGZIP, ("par12843::PptTrySelectLegacyZip - Enter\n") );

    // test to see if we need to grab port
    if( !(Command->CommandFlags & PAR_HAVE_PORT_KEEP_PORT) ) {
        // Don't have the port
        //
        // Try to acquire port and select device
        //
        PptDump2( PARLGZIP, ("par12843::PptTrySelectLegacyZip Try get port.\n") );

        IoAcquireCancelSpinLock(&CancelIrql);
                
        SyncContext.Count = &Extension->WorkQueueCount;
                    
        if (Extension->InterruptRefCount) {
            KeSynchronizeExecution(Extension->InterruptObject,
                                   PptSynchronizedIncrement,
                                   &SyncContext);
        } else {
            PptSynchronizedIncrement(&SyncContext);
        }
                    
        if (SyncContext.NewCount) {
            // Port is busy, queue request
            Status = STATUS_PENDING;
        }  // endif - test for port busy
                    
        IoReleaseCancelSpinLock(CancelIrql);

    } // endif - test if already have port


    //
    // If we have port select legacy Zip
    //
    if ( NT_SUCCESS( Status ) && (Status != STATUS_PENDING) ) {
        if ( Command->CommandFlags & PAR_LEGACY_ZIP_DRIVE_EPP_MODE ) {
            // Select in EPP mode
            PptLegacyZipSetDiskMode( Controller, (UCHAR)0xCF );
        } else {
            // Select in Nibble or Byte mode
            PptLegacyZipSetDiskMode( Controller, (UCHAR)0x8F );
        }

        if ( PptLegacyZipCheckDevice( Controller ) ) {
            PptDump2( PARLGZIP, ("par12843::PptTrySelectLegacyZip - SUCCESS\n") );

            //
            // Legacy Zip is selected - test for EPP if we haven't previously done the test
            //
            if( !Extension->CheckedForGenericEpp ) {
                // haven't done the test yet
                PARDD01(("-- Legacy Zip Selected - haven't done EPP test yet - try to do it now\n"));
                if( Extension->PnpInfo.HardwareCapabilities & PPT_ECP_PRESENT ) {
                    // we have an ECR - required for generic EPP
                    PARDD01(("-- we have an ECR\n"));
                    if( !Extension->NationalChipFound ) {
                        // we don't have a NationalSemi chipset - no generic EPP on NatSemi chips
                        PARDD01(("-- we don't have NatSemi chipset - do the test\n"));
                        PARDD01(("-- pre  PptDetectEppPort\n"));
                        PptDetectEppPort( Extension );
                        PARDD01(("-- post PptDetectEppPort\n"));
                    } else {
                        PARDD01(("-- have NatSemi chipset - abort test\n"));
                    }
                } else {
                    PARDD01(("-- don't have an ECR - abort test\n"));
                }
                Extension->CheckedForGenericEpp = TRUE; // check is complete
            }

        } else {
            PptDump2( PARLGZIP, ("par12843::PptTrySelectLegacyZip - FAIL\n") );
            PptDeselectLegacyZip( Context, TrySelectCommand );
            Status = STATUS_UNSUCCESSFUL;
        }
    }
    
    return( Status );

} // end PptTrySelectLegacyZip()

NTSTATUS
PptDeselectLegacyZip(
    IN  PVOID   Context,
    IN  PVOID   DeselectCommand
    )
{
    ULONG i;
    PDEVICE_EXTENSION       Extension   = Context;
    PUCHAR                  Controller  = Extension->PortInfo.Controller;
    PPARALLEL_1284_COMMAND  Command     = DeselectCommand;

    PptDump2( PARLGZIP, ("par12843::PptDeselectLegacyZip - Enter\n") );

    for ( i = 0; i < LEGACYZIP_MODE_LEN; i++ ) {
        PptLegacyZipClockPrtModeByte( Controller, LegacyZipModeQualifier[i] );
    }

    // set to printer pass thru mode
    PptLegacyZipClockPrtModeByte( Controller, (UCHAR)0x0F );

    // check if requester wants to keep port or free port
    if( !(Command->CommandFlags & PAR_HAVE_PORT_KEEP_PORT) ) {
        PptFreePort( Extension );
    }

    return STATUS_SUCCESS;

} // end  PptDeselectLegacyZip()

