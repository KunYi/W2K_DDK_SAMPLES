/*++

Copyright (C) Microsoft Corporation, 1993 - 1999

Module Name:

    par12843.c

Abstract:

    This is the main module for 1284.3 functionality.  These
      function enable the selection and deselection of 1284.3
      compatable devices on the parallel port.

    The devices can be selected and deselected IRQL <= DISPATCH_LEVEL 
    by calling IOCTL_INTERNAL_SELECT_DEVICE, or 'TrySelectDevice'.
    The first call is the simplest:  the IRP will be queued in the
    parallel port driver until the port is free and then it will 
    try to select the device with the given ID from the structure
    PARALLEL_1284_COMMAND.  If successful it will with a successful 
    status, otherwise it will return with an unsuccessful status.
    The class driver may cancel this IRP at any time which serves 
    as a mechanism to timeout an allocate request.

    The 'TrySelectDevice' call returns immediately from the port
    driver with a TRUE status if the port was allocated and the
    device was able to be selected or a FALSE status if the port 
    was either busy or the device was not able to be selected.

    Once the device is selected, the port is owned by the selecting class
    driver until a 'DeselectDevice' call is made.  This deselects the
    device and also releases the port and wakes up the next caller.

Author:

    Don E. Redford  3-Mar-1998

Environment:

    Kernel mode

Revision History :


--*/

#include "pch.h"

ULONG
PptInitiate1284_3(
    IN  PVOID   Extension
    );

NTSTATUS
PptTrySelectDevice(
    IN  PVOID   Context,
    IN  PVOID   TrySelectCommand
    );

NTSTATUS
PptDeselectDevice(
    IN  PVOID   Context,
    IN  PVOID   DeselectCommand
    );

ULONG
Ppt1284_3AssignAddress(
    IN  PDEVICE_EXTENSION    DeviceExtension
    );

BOOLEAN
PptSend1284_3Command(
    IN  PDEVICE_EXTENSION  DeviceExtension,
    IN  UCHAR              Command
    );

BOOLEAN
PptCheckIfStl1284_3(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN ULONG    ulDaisyIndex,
    IN BOOLEAN  bNoStrobe
    );

BOOLEAN
PptCheckIfNon1284_3Present(
    IN PDEVICE_EXTENSION    Extension
    );

BOOLEAN
PptCheckIfStlProductId(
    IN PDEVICE_EXTENSION    Extension,
    IN ULONG   ulDaisyIndex
    );
//
// Beginning of functions
//

ULONG
PptInitiate1284_3(
    IN  PVOID   Extension
    )

/*++

Routine Description:

    This routine initializes all of the 1284.3 devices out on the
    given parallel port.  It does this by assigning 1284.3 addresses to
    each device on the port.

Arguments:

    Extensioon    - Device extension structure.

Return Value:

    None.

--*/

{
    ULONG deviceCount1 = 0;
    ULONG deviceCount2 = 0;
    ULONG loopCount    = 0;
    ULONG maxTries     = 3; // picked 3 out of thin air as a "reasonable" value

    // Send command to assign addresses and count number of 1284.3 daisy chain devices 
    // Try multiple times to make sure we get the same count
    do {

        KeStallExecutionProcessor( 5 );
        deviceCount1 = Ppt1284_3AssignAddress( Extension );

        KeStallExecutionProcessor( 5 );
        deviceCount2 = Ppt1284_3AssignAddress( Extension );

    } while( (deviceCount1 != deviceCount2) && (++loopCount < maxTries) );

    return deviceCount2;
}

NTSTATUS
PptTrySelectDevice(
    IN  PVOID   Context,
    IN  PVOID   TrySelectCommand
    )
/*++

Routine Description:

    This routine first tries to allocate the port.  If successful
      it will then try to select  the device with the ID given.

Arguments:

    Extension   -   Driver extension.
    Device      -   1284.3 Device Id.
    Command     -   Command to know whether to allocate the port

Return Value:

    TRUE            -  Able to allocate the port and select the device
    FALSE           -  1: Invalid ID    2: Not able to allocate port    3: Not able to select device

--*/
{
    NTSTATUS                    Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION           Extension = Context;
    PPARALLEL_1284_COMMAND      Command = TrySelectCommand;
    BOOLEAN                     success = FALSE;
    SYNCHRONIZED_COUNT_CONTEXT  SyncContext;
    KIRQL                       CancelIrql;
    UCHAR                       i, DeviceID;

    PptDump2( PARENTRY, ("Enter PptTrySelectDevice()\n") );

    if( ( Command->CommandFlags & PAR_LEGACY_ZIP_DRIVE ) ||
        ( Command->ID == DOT3_LEGACY_ZIP_ID )) {
        return PptTrySelectLegacyZip(Context, TrySelectCommand);
    }

    // get device ID to select
    DeviceID = Command->ID;
            
    // validate parameters - we will accept:
    //   - a Dot3 device with a valid DeviceID
    //   - an End-of-Chain device indicated by the PAR_END_OF_CHAIN_DEVICE flag, or
    //   - an End-of-Chain device indicated by a DeviceID value one past the last Dot3 device

    if ( !(Command->CommandFlags & PAR_END_OF_CHAIN_DEVICE) && 
            DeviceID > Extension->PnpInfo.Ieee1284_3DeviceCount ) {
                
        // Requested device is not flagged as End-of-Chain device and DeviceID
        //   is more than one past the end of the Dot3 Devices, so FAIL the IRP
        PptDump2( PARERRORS, ("Dot3 SELECT_DEVICE IRP %S ID=%d - FAIL - invalid ID\n",
                                Extension->PnpInfo.PortName, DeviceID) );
        Status = STATUS_INVALID_PARAMETER;
                
    } else {
                
        //
        // Request appears valid
        //

        // test to see if we need to grab port
        if( Command->CommandFlags & PAR_HAVE_PORT_KEEP_PORT ) {

            //
            // requester has already acquired port, just do a SELECT
            //
            if ( !(Command->CommandFlags & PAR_END_OF_CHAIN_DEVICE) &&
                    DeviceID < Extension->PnpInfo.Ieee1284_3DeviceCount ) {

                // SELECT the device
                for ( i = 0; i < PptDot3Retries && !success; i++ ) {
                    // Send command to to select device in compatability mode
                    success = PptSend1284_3Command( Extension, (UCHAR)(CPP_SELECT | DeviceID) );
                    // Stall a little in case we have to retry
                    KeStallExecutionProcessor( 5 );
                }                

                if ( success ) {
                    PptDump2( PARINFO, ("DOT3 SELECT_DEVICE IRP - SUCCESS\n") );
                    Status = STATUS_SUCCESS;
                } else {
                    PptDump2( PARERRORS, ("DOT3 SELECT_DEVICE IRP - FAIL\n") );
                    Status = STATUS_UNSUCCESSFUL;
                }
            } else {
                // End-of-Chain device, no SELECT required, SUCCEED the request
                PptDumpV( ("DOT3 SELECT_DEVICE IRP - End-of-Chain - SUCCESS\n") );
                Status = STATUS_SUCCESS;
            }

        } else {

            // Don't have the port

            //
            // Try to acquire port and select device
            //
            PptDump2( PARINFO, ("Dot3 SELECT_DEVICE IRP %S ID=%d - attempting to SELECT...\n",
                            Extension->PnpInfo.PortName, DeviceID) );

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
                IoReleaseCancelSpinLock(CancelIrql);
                Status = STATUS_PENDING;

            } else {

                IoReleaseCancelSpinLock(CancelIrql);
                        
                // Port is acquired
                PptDump2( PARINFO, ("DOT3 SELECT_DEVICE IRP - Port ALLOCATE (ACQUIRED) - SUCCESS\n") );

                Extension->WmiPortAllocFreeCounts.PortAllocates++;

                if ( !(Command->CommandFlags & PAR_END_OF_CHAIN_DEVICE) &&
                        DeviceID < Extension->PnpInfo.Ieee1284_3DeviceCount ) {
                            
                    // SELECT the device
                    for ( i = 0; i < PptDot3Retries && !success; i++ ) {
                        // Send command to to select device in compatability mode
                        success = PptSend1284_3Command( Extension, (UCHAR)(CPP_SELECT | DeviceID) );
                        // Stall a little in case we have to retry
                        KeStallExecutionProcessor( 5 );
                    }                

                    if ( success ) {
                        PptDump2( PARINFO, ("DOT3 SELECT_DEVICE IRP - SUCCESS\n") );
                        Status = STATUS_SUCCESS;
                    } else {
                        PptDump2( PARERRORS, ("DOT3 SELECT_DEVICE IRP - FAIL\n") );
                        Status = STATUS_UNSUCCESSFUL;
                    }

                } else {
                    // End-of-Chain device, no SELECT required, SUCCEED the request
                    PptDump2( PARINFO, ("DOT3 SELECT_DEVICE IRP - End-of-Chain device - no select required\n") );
                    Status = STATUS_SUCCESS;
                }

            }  // endif - test for port busy
                    
        } // endif - test if already have port

    } // endif - test for valid parameters

    return( Status );

}

NTSTATUS
PptDeselectDevice(
    IN  PVOID   Context,
    IN  PVOID   DeselectCommand
    )
    
/*++

Routine Description:

    This routine deselects the current device and then frees the port

Arguments:

    DriverObject    - Supplies the driver object controlling all of the
                        devices.

Return Value:

    TRUE            -  Able to deselect the device and free the port
    FALSE           -  1: Invalid ID    2: Not able to deselect the drive

--*/

{
    NTSTATUS                Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION       Extension = Context;
    PPARALLEL_1284_COMMAND  Command = DeselectCommand;
    BOOLEAN                 success = FALSE;
    UCHAR                   i, DeviceID;

    PptDump2( PARENTRY, ("Enter PptDeselectDevice()\n") );

    if( ( Command->CommandFlags & PAR_LEGACY_ZIP_DRIVE ) ||
        ( Command->ID == DOT3_LEGACY_ZIP_ID ) ) {
        return PptDeselectLegacyZip( Context, DeselectCommand );
    }

    // get device ID to deselect
    DeviceID = Command->ID;


    // validate ID
    if ( !(Command->CommandFlags & PAR_END_OF_CHAIN_DEVICE) &&
            DeviceID > Extension->PnpInfo.Ieee1284_3DeviceCount ) {

        // not End-of-Chain device and Dot3 DeviceID is invalid
        PptDump2( PARERRORS, ("DOT3 DESELECT_DEVICE %S - ID=%d - FAIL - invalid parameter\n",
                           Extension->PnpInfo.PortName, DeviceID) );
        Status = STATUS_INVALID_PARAMETER;
                
    } else {
                
        // Check for End-of-Chain device
        if ( !(Command->CommandFlags & PAR_END_OF_CHAIN_DEVICE) &&
                DeviceID < Extension->PnpInfo.Ieee1284_3DeviceCount ) {
                    
            // first deselect the device 
            for ( i = 0; i < PptDot3Retries && !success; i++ ) {
                success = PptSend1284_3Command( Extension, (UCHAR)CPP_DESELECT );
                // Stall a little in case we have to retry
                KeStallExecutionProcessor( 5 );
            }

            if ( success ) {
                // Deselecting device was a success
                PptDumpV( ("DOT3 DESELECT_DEVICE %S - ID=%d - SUCCESS\n",
                            Extension->PnpInfo.PortName, DeviceID) );

                // check if requester wants to keep port or free port
                if( !(Command->CommandFlags & PAR_HAVE_PORT_KEEP_PORT) ) {
                    PptFreePort( Extension );
                }
                Status = STATUS_SUCCESS;
                        
            } else {
                // Unable to deselect device, something went very wrong,
                //   port is now in an unknown/blocked state
                PptDump2( PARERRORS, ("DOT3 DESELECT_DEVICE %S - ID=%d - SERIOUS FAILURE\n",
                                Extension->PnpInfo.PortName, DeviceID) );
                // ASSERT(FALSE);
                Status = STATUS_UNSUCCESSFUL;
            }
                    
        } else {

            // this is End-of-Chain device so no deselect neccessary
            PptDump2( PARINFO, ("DOT3 DESELECT_DEVICE %S - ID=%d - End-of-Chain - SUCCESS\n",
                        Extension->PnpInfo.PortName, DeviceID) );

            // check if requester wants to keep port or free port
            if( !(Command->CommandFlags & PAR_HAVE_PORT_KEEP_PORT) ) {
                PptFreePort( Extension );
            }
            Status = STATUS_SUCCESS;

        }  // endif - Check if End Of Chain

    } // endif - Validate ID

    return ( Status );
    
}


ULONG
Ppt1284_3AssignAddress(
    IN  PDEVICE_EXTENSION    DeviceExtension
    )

/*++

Routine Description:

    This routine initializes the 1284_3 bus.

Arguments:

    DeviceExtension    - Supplies Device Extension structure of the driver.

Return Value:

    Number of 1284.3 devices out there at the given address.

--*/

{

    //UCHAR  i, ii, value, newvalue, status;
    UCHAR  i, value, newvalue, status;
    PUCHAR CurrentPort, CurrentStatus, CurrentControl;
    ULONG  Delay = 5;
    UCHAR  number = 0;
    BOOLEAN lastdevice = FALSE;
    UCHAR   idx;

    CurrentPort = DeviceExtension->PortInfo.Controller;
    CurrentStatus  = CurrentPort + 1;
    CurrentControl = CurrentPort + 2;

    // get current ctl reg
    value = READ_PORT_UCHAR( CurrentControl );

    // make sure 1284.3 devices do not get reseted
    newvalue = (UCHAR)((value & ~DCR_SELECT_IN) | DCR_NOT_INIT);

    // make sure we can write
    newvalue = (UCHAR)(newvalue & ~DCR_DIRECTION);
    WRITE_PORT_UCHAR( CurrentControl, newvalue );    // make sure we can write 

    // bring nStrobe high
    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );

    // send first four bytes of the 1284.3 mode qualifier sequence out
    for ( i = 0; i < MODE_LEN_1284_3 - 3; i++ ) {
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[i] );
        KeStallExecutionProcessor( Delay );
    }

    // check for correct status
    status = READ_PORT_UCHAR( CurrentStatus );

    if ( (status & (UCHAR)0xb8 ) 
         == ( DSR_NOT_BUSY | DSR_PERROR | DSR_SELECT | DSR_NOT_FAULT )) {

        // continue with fifth byte of mode qualifier
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[4] );
        KeStallExecutionProcessor( Delay );

        // check for correct status
        status = READ_PORT_UCHAR( CurrentStatus );

        // note busy is high too but is opposite so we see it as a low
        if (( status & (UCHAR) 0xb8 ) == (DSR_SELECT | DSR_NOT_FAULT)) {

            // continue with sixth byte
            WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[5] );
            KeStallExecutionProcessor( Delay );

            // check for correct status
            status = READ_PORT_UCHAR( CurrentStatus );

            // if status is valid there is a device out there responding
            if ((status & (UCHAR) 0x30 ) == ( DSR_PERROR | DSR_SELECT )) {        

                // Device is out there
                KeStallExecutionProcessor( Delay );

                while ( number < 4 && !lastdevice ) {

                    // Asssign address byte
                    WRITE_PORT_UCHAR( CurrentPort, number );
                    number = (UCHAR)(number + 1);

                    KeStallExecutionProcessor( Delay );                    // wait a bit
                    if ( (READ_PORT_UCHAR( CurrentStatus ) & (UCHAR)DSR_NOT_BUSY ) == 0 ) {
                        // we saw last device
                        lastdevice = TRUE;    
                    }

                    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                    KeStallExecutionProcessor( Delay );        // wait a bit
                    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                    KeStallExecutionProcessor( Delay );        // wait a bit
                }

                // last byte
                WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[6] );

                if ( number ) {
                    BOOLEAN bStlNon1284_3Found ;
                    BOOLEAN bStlNon1284_3Valid ;
                    bStlNon1284_3Found = PptCheckIfNon1284_3Present(DeviceExtension);
                    bStlNon1284_3Valid = FALSE ;
                    // as the earlier 1284 spec does not give the
                    // lastdevice status is BSY, number needs to
                    // be corrected in such cases
                    for ( idx = 0 ; idx < number ; idx++ ) {
                        if ( TRUE == PptCheckIfStl1284_3(DeviceExtension, idx, bStlNon1284_3Found ) ) {
                            continue ;
                        }
                        if ( TRUE == bStlNon1284_3Found ) {
                            if ( TRUE == PptCheckIfStlProductId(DeviceExtension, idx) ) {
                                bStlNon1284_3Valid = TRUE ;
                                continue ;
                            }
                        }
                        break ;
                    }
                    if ( TRUE == bStlNon1284_3Valid ) {
                        // we alter the count only if old adapters
                        // are in the chain
                        number = idx;
                    }
                }

            } // Third status

        } // Second status

    } // First status

    WRITE_PORT_UCHAR( CurrentControl, value );    // restore everything

    // returns last device ID + 1 or number of devices out there
    return ( (ULONG)number );

}

BOOLEAN
PptCheckIfNon1284_3Present(
    IN PDEVICE_EXTENSION    Extension
    )
/*++

Routine Description:

    Indicates whether one of the devices of the earlier
    specification is present in the chain.


Arguments:

    Extension   - Device Extension structure


Return Value:

    TRUE    : Atleast one of the adapters are of earlier spec.
    FALSE   : None of the adapters of the earlier spec.

--*/
{
    BOOLEAN bReturnValue = FALSE ;
    UCHAR   i, value, newvalue, status;
    ULONG   Delay = 3;
    PUCHAR  CurrentPort, CurrentStatus, CurrentControl;
    UCHAR   ucAckStatus ;

    CurrentPort = Extension->PortInfo.Controller;
    CurrentStatus  = CurrentPort + 1;
    CurrentControl = CurrentPort + 2;

    // get current ctl reg
    value = READ_PORT_UCHAR( CurrentControl );

    // make sure 1284.3 devices do not get reseted
    newvalue = (UCHAR)((value & ~DCR_SELECT_IN) | DCR_NOT_INIT);

    // make sure we can write
    newvalue = (UCHAR)(newvalue & ~DCR_DIRECTION);
    WRITE_PORT_UCHAR( CurrentControl, newvalue );    // make sure we can write 

    // bring nStrobe high
    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );

    // send first four bytes of the 1284.3 mode qualifier sequence out
    for ( i = 0; i < MODE_LEN_1284_3 - 3; i++ ) {
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[i] );
        KeStallExecutionProcessor( Delay );
    }

    // check for correct status
    status = READ_PORT_UCHAR( CurrentStatus );

    if ( (status & (UCHAR)0xb8 ) 
         == ( DSR_NOT_BUSY | DSR_PERROR | DSR_SELECT | DSR_NOT_FAULT )) {

        ucAckStatus = status & 0x40 ;

        // continue with fifth byte of mode qualifier
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[4] );
        KeStallExecutionProcessor( Delay );

        // check for correct status
        status = READ_PORT_UCHAR( CurrentStatus );

        // note busy is high too but is opposite so we see it as a low
        if (( status & (UCHAR) 0xb8 ) == (DSR_SELECT | DSR_NOT_FAULT)) {

            if ( ucAckStatus != ( status & 0x40 ) ) {

                // save current ack status
                ucAckStatus = status & 0x40 ;

                // continue with sixth byte
                WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[5] );
                KeStallExecutionProcessor( Delay );

                // check for correct status
                status = READ_PORT_UCHAR( CurrentStatus );

                // if status is valid there is a device out there responding
                if ((status & (UCHAR) 0x30 ) == ( DSR_PERROR | DSR_SELECT )) {        

                    bReturnValue = TRUE ;

                } // Third status

            } // ack of earlier adapters not seen

            // last byte
            WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[6] );

        } // Second status

    } // First status

    WRITE_PORT_UCHAR( CurrentControl, value );    // restore everything

    return bReturnValue ;
} // PptCheckIfNon1284_3Present


// Define 1284 Commands
#define CPP_QUERY_PRODID    0x10

// 1284 related SHTL prod id equates
#define SHTL_EPAT_PRODID    0xAAFF
#define SHTL_EPST_PRODID    0xA8FF

BOOLEAN
PptCheckIfStl1284_3(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN ULONG    ulDaisyIndex,
    IN BOOLEAN  bNoStrobe
    )
/*++

Routine Description:

    This function checks to see whether the device indicated
    is a Shuttle 1284_3 type of device. 

Arguments:

    Extension       - Device extension structure.

    ulDaisyIndex    - The daisy chain id of the device that
                      this function will check on.

    bNoStrobe       - If set, indicates that the query
                      Ep1284 command issued by this function
                      need not assert strobe to latch the
                      command.

Return Value:

    TRUE            - Yes. Device is Shuttle 1284_3 type of device.
    FALSE           - No. This may mean that this device is either
                      non-shuttle or Shuttle non-1284_3 type of
                      device.

--*/
{
    BOOLEAN bReturnValue = FALSE ;
    UCHAR   i, value, newvalue, status;
    ULONG   Delay = 3;
    UCHAR   ucExpectedPattern ;
    UCHAR   ucReadValue, ucReadPattern;
    PUCHAR  CurrentPort, CurrentStatus, CurrentControl;

    CurrentPort = DeviceExtension->PortInfo.Controller;
    CurrentStatus  = CurrentPort + 1;
    CurrentControl = CurrentPort + 2;

    // get current ctl reg
    value = READ_PORT_UCHAR( CurrentControl );

    // make sure 1284.3 devices do not get reseted
    newvalue = (UCHAR)((value & ~DCR_SELECT_IN) | DCR_NOT_INIT);

    // make sure we can write
    newvalue = (UCHAR)(newvalue & ~DCR_DIRECTION);
    WRITE_PORT_UCHAR( CurrentControl, newvalue );    // make sure we can write 

    // bring nStrobe high
    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );

    // send first four bytes of the 1284.3 mode qualifier sequence out
    for ( i = 0; i < MODE_LEN_1284_3 - 3; i++ ) {
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[i] );
        KeStallExecutionProcessor( Delay );
    }

    // check for correct status
    status = READ_PORT_UCHAR( CurrentStatus );

    if ( (status & (UCHAR)0xb8 ) 
         == ( DSR_NOT_BUSY | DSR_PERROR | DSR_SELECT | DSR_NOT_FAULT )) {

        // continue with fifth byte of mode qualifier
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[4] );
        KeStallExecutionProcessor( Delay );

        // check for correct status
        status = READ_PORT_UCHAR( CurrentStatus );

        // note busy is high too but is opposite so we see it as a low
        if (( status & (UCHAR) 0xb8 ) == (DSR_SELECT | DSR_NOT_FAULT)) {

            // continue with sixth byte
            WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[5] );
            KeStallExecutionProcessor( Delay );

            // check for correct status
            status = READ_PORT_UCHAR( CurrentStatus );

            // if status is valid there is a device out there responding
            if ((status & (UCHAR) 0x30 ) == ( DSR_PERROR | DSR_SELECT )) {        

                // Device is out there
                KeStallExecutionProcessor( Delay );

                // issue shuttle specific CPP command
                WRITE_PORT_UCHAR( CurrentPort, (UCHAR) ( 0x88 | ulDaisyIndex ) );
                KeStallExecutionProcessor( Delay );        // wait a bit

                if ( ulDaisyIndex && ( bNoStrobe == FALSE ) ) {

                    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                    KeStallExecutionProcessor( Delay );        // wait a bit
                    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                    KeStallExecutionProcessor( Delay );        // wait a bit

                }

                ucExpectedPattern = 0xF0 ;
                bReturnValue = TRUE ;

                while ( ucExpectedPattern ) {

                    KeStallExecutionProcessor( Delay );        // wait a bit
                    WRITE_PORT_UCHAR( CurrentPort, (UCHAR) (0x80 | ulDaisyIndex )) ;

                    KeStallExecutionProcessor( Delay );        // wait a bit
                    WRITE_PORT_UCHAR( CurrentPort, (UCHAR) (0x88 | ulDaisyIndex )) ;

                    KeStallExecutionProcessor( Delay );        // wait a bit
                    ucReadValue = READ_PORT_UCHAR( CurrentStatus ) ;
                    ucReadPattern = ( ucReadValue << 1 ) & 0x70 ;
                    ucReadPattern |= ( ucReadValue & 0x80 ) ;

                    if ( ucReadPattern != ucExpectedPattern ) {
                        // not Shuttle 1284_3 behaviour
                        bReturnValue = FALSE ;
                        break ;
                    }

                    ucExpectedPattern -= 0x10 ;
                }


                // last byte
                WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[6] );

            } // Third status

        } // Second status

    } // First status

    WRITE_PORT_UCHAR( CurrentControl, value );    // restore everything

    return bReturnValue ;
} // end  PptCheckIfStl1284_3()

BOOLEAN
PptCheckIfStlProductId(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN ULONG   ulDaisyIndex
    )
/*++

Routine Description:

    This function checks to see whether the device indicated
    is a Shuttle non-1284_3 type of device. 

Arguments:

    Extension       - Device extension structure.

    ulDaisyIndex    - The daisy chain id of the device that
                      this function will check on.

Return Value:

    TRUE            - Yes. Device is Shuttle non-1284_3 type of device.
    FALSE           - No. This may mean that this device is 
                      non-shuttle.

--*/
{
    BOOLEAN bReturnValue = FALSE ;
    UCHAR   i, value, newvalue, status;
    ULONG   Delay = 3;
    UCHAR   ucProdIdHiByteHiNibble, ucProdIdHiByteLoNibble ;
    UCHAR   ucProdIdLoByteHiNibble, ucProdIdLoByteLoNibble ;
    UCHAR   ucProdIdHiByte, ucProdIdLoByte ;
    USHORT  usProdId ;
    PUCHAR  CurrentPort, CurrentStatus, CurrentControl;

    CurrentPort = DeviceExtension->PortInfo.Controller;
    CurrentStatus  = CurrentPort + 1;
    CurrentControl = CurrentPort + 2;

    // get current ctl reg
    value = READ_PORT_UCHAR( CurrentControl );

    // make sure 1284.3 devices do not get reseted
    newvalue = (UCHAR)((value & ~DCR_SELECT_IN) | DCR_NOT_INIT);

    // make sure we can write
    newvalue = (UCHAR)(newvalue & ~DCR_DIRECTION);
    WRITE_PORT_UCHAR( CurrentControl, newvalue );    // make sure we can write 

    // bring nStrobe high
    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );

    // send first four bytes of the 1284.3 mode qualifier sequence out
    for ( i = 0; i < MODE_LEN_1284_3 - 3; i++ ) {
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[i] );
        KeStallExecutionProcessor( Delay );
    }

    // check for correct status
    status = READ_PORT_UCHAR( CurrentStatus );

    if ( (status & (UCHAR)0xb8 ) 
         == ( DSR_NOT_BUSY | DSR_PERROR | DSR_SELECT | DSR_NOT_FAULT )) {

        // continue with fifth byte of mode qualifier
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[4] );
        KeStallExecutionProcessor( Delay );

        // check for correct status
        status = READ_PORT_UCHAR( CurrentStatus );

        // note busy is high too but is opposite so we see it as a low
        if (( status & (UCHAR) 0xb8 ) == (DSR_SELECT | DSR_NOT_FAULT)) {

            // continue with sixth byte
            WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[5] );
            KeStallExecutionProcessor( Delay );

            // check for correct status
            status = READ_PORT_UCHAR( CurrentStatus );

            // if status is valid there is a device out there responding
            if ((status & (UCHAR) 0x30 ) == ( DSR_PERROR | DSR_SELECT )) {

                WRITE_PORT_UCHAR ( CurrentPort, (UCHAR) (CPP_QUERY_PRODID | ulDaisyIndex )) ;
                KeStallExecutionProcessor( Delay );

                // Device is out there
                KeStallExecutionProcessor( Delay );
                ucProdIdLoByteHiNibble = READ_PORT_UCHAR( CurrentStatus ) ;
                ucProdIdLoByteHiNibble &= 0xF0 ;

                KeStallExecutionProcessor( Delay );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                KeStallExecutionProcessor( Delay );        // wait a bit
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                KeStallExecutionProcessor( Delay );        // wait a bit

                ucProdIdLoByteLoNibble = READ_PORT_UCHAR( CurrentStatus ) ;
                ucProdIdLoByteLoNibble >>= 4 ;
                ucProdIdLoByte = ucProdIdLoByteHiNibble | ucProdIdLoByteLoNibble ;

                KeStallExecutionProcessor( Delay );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                KeStallExecutionProcessor( Delay );        // wait a bit
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                KeStallExecutionProcessor( Delay );        // wait a bit

                ucProdIdHiByteHiNibble = READ_PORT_UCHAR( CurrentStatus ) ;
                ucProdIdHiByteHiNibble &= 0xF0 ;

                KeStallExecutionProcessor( Delay );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                KeStallExecutionProcessor( Delay );        // wait a bit
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                KeStallExecutionProcessor( Delay );        // wait a bit

                ucProdIdHiByteLoNibble = READ_PORT_UCHAR( CurrentStatus ) ;
                ucProdIdHiByteLoNibble >>= 4 ;
                ucProdIdHiByte = ucProdIdHiByteHiNibble | ucProdIdHiByteLoNibble ;

                // issue the last strobe
                KeStallExecutionProcessor( Delay );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                KeStallExecutionProcessor( Delay );        // wait a bit
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                KeStallExecutionProcessor( Delay );        // wait a bit

                usProdId = ( ucProdIdHiByte << 8 ) | ucProdIdLoByte ;

                if ( ( SHTL_EPAT_PRODID == usProdId ) ||\
                     ( SHTL_EPST_PRODID == usProdId ) ) {
                    // one of the devices that conform to the earlier
                    // draft is found
                    bReturnValue = TRUE ;
                }

                // last byte
                WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[6] );

            } // Third status

        } // Second status

    } // First status

    WRITE_PORT_UCHAR( CurrentControl, value );    // restore everything

    return bReturnValue ;
} // end  PptCheckIfStlProductId()

BOOLEAN
PptSend1284_3Command(
    IN  PDEVICE_EXTENSION    DeviceExtension,
    IN  UCHAR                Command
    )

/*++

Routine Description:

    This routine sends the 1284_3 Command given to it
    down the parallel bus.

Arguments:

    DriverObject    - Supplies the driver object controlling all of the
                        devices.

Return Value:

    None.

--*/

{

    UCHAR  i, value, newvalue, test;//, status;
    ULONG  ii;
    PUCHAR CurrentPort, CurrentStatus, CurrentControl;
    ULONG  Delay = 3;
    BOOLEAN success = FALSE;

    CurrentPort = DeviceExtension->PortInfo.Controller;
    CurrentStatus  = CurrentPort + 1;
    CurrentControl = CurrentPort + 2;

    // Get Upper 4 bits to see what Command it is
    test = (UCHAR)(Command & (UCHAR)CPP_COMMAND_FILTER);

    PptDumpV( ("PptSend1284_3Command - test = %x\n", test) );
    
    // get current ctl reg
    value = READ_PORT_UCHAR( CurrentControl );
    
    // make sure 1284.3 devices do not get reseted
    newvalue = (UCHAR)((value & ~DCR_SELECT_IN) | DCR_NOT_INIT);
    
    // make sure we can write
    newvalue = (UCHAR)(newvalue & ~DCR_DIRECTION);
    WRITE_PORT_UCHAR( CurrentControl, newvalue );       // make sure we can write 
    
    // bring nStrobe high
    WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );
    KeStallExecutionProcessor( Delay );
    
    // send first four bytes of the 1284.3 mode qualifier sequence out
    for ( i = 0; i < MODE_LEN_1284_3 - 3; i++ ) {
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[i] );
        KeStallExecutionProcessor( Delay );
    }
    
    // check for correct status
//    status = READ_PORT_UCHAR( CurrentStatus );
    
    // wait up to 5 us : Spec says about 2 but we will be lienient
    if (CHECK_DSR(CurrentPort,
                  INACTIVE, DONT_CARE, ACTIVE, ACTIVE, ACTIVE, 5 )) {

//    if ( (status & (UCHAR)0xb8 ) 
//         == ( DSR_NOT_BUSY | DSR_PERROR | DSR_SELECT | DSR_NOT_FAULT )) {
    
        // continue with fifth byte of mode qualifier
        WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[4] );
        KeStallExecutionProcessor( Delay );
        
        // check for correct status
//        status = READ_PORT_UCHAR( CurrentStatus );
        
        // note busy is high too but is opposite so we see it as a low
//        if (( status & (UCHAR) 0xb8 ) == (DSR_SELECT | DSR_NOT_FAULT)) {
            
        // wait up to 5 us : Spec says about 2 but we will be lienient
        if (CHECK_DSR(CurrentPort,
                      ACTIVE, DONT_CARE, INACTIVE, ACTIVE, ACTIVE, 5 )) {

            // continue with sixth byte
            WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[5] );
            KeStallExecutionProcessor( Delay );
            
            // check for correct status
//            status = READ_PORT_UCHAR( CurrentStatus );
            
            // if status is valid there is a device out there responding
//            if ((status & (UCHAR) 0x30 ) == ( DSR_PERROR | DSR_SELECT )) {
                
            // wait up to 5 us : Spec says about 2 but we will be lienient
            if (CHECK_DSR(CurrentPort,
                          DONT_CARE, DONT_CARE, ACTIVE, ACTIVE, DONT_CARE, 5 )) {

                // Device is out there
                
                KeStallExecutionProcessor( Delay );

                // Command byte
                WRITE_PORT_UCHAR( CurrentPort, Command );
                KeStallExecutionProcessor( Delay );        // wait a bit

                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue | DCR_STROBE) );    // bring nStrobe low
                KeStallExecutionProcessor( Delay );        // wait a bit
// NOTE NOTE NOTE
// Assertion of strobe to be done ONLY after checking for the
// FAULT feedback, as per the 1284_3 specification. The following lines
// have been moved to after checking for the FAULT feedback
//                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
//                KeStallExecutionProcessor( Delay );        // wait a bit


                // Selection does not work correctly yet to be able to check for lines
                switch ( test ) {
                    
                // Check to make sure we are selected
                case CPP_SELECT:

                    PptDumpV( ("PptSend1284_3Command - Enter CPP_SELECT case\n") );

                    // wait for upto 250 micro Secs for for selection time out.
                    for ( ii = 25000; ii > 0; ii-- ) {

                        if ( ( READ_PORT_UCHAR( CurrentStatus ) & DSR_NOT_FAULT ) == DSR_NOT_FAULT ) {
                            // selection...
                            success = TRUE;
                            PptDumpV( ("PptSend1284_3Command - SUCCESS - Selected device\n") );
                            break;
                        }
                    }

                    if ( !success ) {
                        PptDumpV( ("PptSend1284_3Command - was not able to select device\n") );
                    }

                    break;

                // Check to make sure we are deselected
                case CPP_DESELECT:

                    // wait for upto 250 micro Secs for for deselection time out.
                    for ( ii = 25000; ii > 0; ii-- ) {

                        if ( (READ_PORT_UCHAR( CurrentStatus ) & DSR_NOT_FAULT) != DSR_NOT_FAULT ) {
                            // deselection...
                            success = TRUE;
                            PptDumpV( ("PptSend1284_3Command - SUCCESS - Deselected device\n") );
                            break;
                        }
                    }

                    if ( !success ) {
                        PptDumpV( ("PptSend1284_3Command - was not able to deselect device\n") );
                    }

                    break;

                default :

                    PptDumpV( ("PptSend1284_3Command - Enter default case\n") );

                    // there is a device out there and Command completed sucessfully
                    KeStallExecutionProcessor( Delay );        // wait a bit
                    success = TRUE;

                    break;


                } // End Switch

// NOTE NOTE NOTE
// the strobe is de-asserted now and the command is completed here
                WRITE_PORT_UCHAR( CurrentControl, (UCHAR)(newvalue & ~DCR_STROBE) );    // bring nStrobe high
                KeStallExecutionProcessor( Delay );        // wait a bit

                // last byte
                WRITE_PORT_UCHAR( CurrentPort, ModeQualifier[6] );

            } // Third status
            
        } // Second status
        
    } // First status

    WRITE_PORT_UCHAR( CurrentControl, value );    // restore everything

    // returns TRUE if command succedded FALSE otherwise
    return ( success );

}

