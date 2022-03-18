/*++

Copyright (C) Microsoft Corporation, 1993 - 1999

Module Name:

    parmode.c

Abstract:

    This is the main module for Extended Parallel Port (ECP) and
    Enhanced Parallel Port (EPP) detection.  This module 
    will detect for invalid chipshets and do ECR detection 
    for ECP and EPP hardware support if the invalid chipset
    is not found.

Author:

    Don Redford (v-donred) 4-Mar-1998

Environment:

    Kernel mode

Revision History :

--*/

#include "pch.h"
#include "parmode.h"

#define USE_PARCHIP_ECRCONTROLLER 1


NTSTATUS
PptDetectChipFilter(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine is called once per DeviceObject to see if the filter driver 
    for detecting parallel chip capabilities is there and to get the chip
    capabilities if there of the port in question.
    
Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if we were able detect the chip and modes possible.
   !STATUS_SUCCESS  - otherwise.

--*/

{
    NTSTATUS                    Status = STATUS_NO_SUCH_DEVICE;
    PIRP                        Irp;
    KEVENT                      Event;
    IO_STATUS_BLOCK             IoStatus;
    UCHAR                       ecrLast;
    PUCHAR                      Controller, EcpController;
            
    Controller = Extension->PortInfo.Controller;
    EcpController = Extension->PnpInfo.EcpController;
    
    PptDumpV( ("PptDetectChipFilter(...)\n") );

    // Setting variable to FALSE to make sure we do not acidentally succeed
    Extension->ChipInfo.success = FALSE;

    // Setting the Address to send to the filter driver to check the chips
    Extension->ChipInfo.Controller = Controller;

    // Setting the Address to send to the filter driver to check the chips
    Extension->ChipInfo.EcrController = EcpController;

#ifndef USE_PARCHIP_ECRCONTROLLER
    // if there is not value in the ECR controller then PARCHIP and PARPORT
    // will conflict and PARCHIP will not work with PARPORT unless we
    // use the ECR controller found by PARCHIP.
    if ( !EcpController ) {
         return Status;
    }
#endif    
    //
    // Initialize
    //
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    // Send a Pointer to the ChipInfo structure to and from the filter
    Irp = IoBuildDeviceIoControlRequest( IOCTL_INTERNAL_PARCHIP_CONNECT,
                                         Extension->ParentDeviceObject, 
                                         &Extension->ChipInfo,
                                         sizeof(PARALLEL_PARCHIP_INFO),
                                         &Extension->ChipInfo,
                                         sizeof(PARALLEL_PARCHIP_INFO),
                                         TRUE, &Event, &IoStatus);

    if (!Irp) { 
        // couldn't create an IRP
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Call down to our parent and see if Filter is present
    //
    PptDumpV( ("PptDetectChipFilter: ready to call IoCallDriver\n") );

    Status = IoCallDriver(Extension->ParentDeviceObject, Irp);
            
    if (Status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
        Status = Irp->IoStatus.Status;
    }
            
    //
    // If successful then we have a filter driver and we need to get the modes supported
    //
    if ( NT_SUCCESS(Status) ) {

        PptDumpV( ("PptDetectChipFilter(...) Found A Filter Driver\n") );
        //
        // check to see if the filter driver was able to determine the I/O chip
        //
        if ( Extension->ChipInfo.success ) {
            PptDumpV( ("PptDetectChipFilter(...) Found A Filter Driver and detected the chip.\n") );
            Extension->PnpInfo.HardwareCapabilities = Extension->ChipInfo.HardwareModes;
#ifdef USE_PARCHIP_ECRCONTROLLER
            // only replace it if defined
            PptDumpV( ("PptDetectChipFilter(...) BEFORE CHIP FILTER ECR address is %x.\n", Extension->PnpInfo.EcpController) );
            if ( Extension->PnpInfo.EcpController != Extension->ChipInfo.EcrController ) {
                Extension->PnpInfo.EcpController = Extension->ChipInfo.EcrController;
                EcpController = Extension->PnpInfo.EcpController;
                PptDumpV( ("PptDetectChipFilter(...) AFTER CHIP FILTER ECR address is %x.\n", Extension->PnpInfo.EcpController) );
            }
#endif
            // Set variable to say we have a filter driver
            Extension->FilterMode = TRUE;
        } else {
            PptDumpV( ("PptDetectChipFilter(...) Found A Filter Driver NOT able to detect the chip.\n") );
        }
    }

    // if there is a filter and ECP capable we need to get the Fifo Size
    if ( Extension->FilterMode && Extension->PnpInfo.HardwareCapabilities & PPT_ECP_PRESENT ) {

        PptDumpV( ("PptDetectChipFilter(...) ECP is present Determining Fifo size.\n") );

        Status = Extension->ChipInfo.ParChipSetMode ( Extension->ChipInfo.Context, ECR_ECP_MODE );

        // if able to set ECP mode
        if ( NT_SUCCESS( Status ) ) {
            PUCHAR wPortECR;
            PptDumpV( ("PptDetectChipFilter(...) Able to set chip into ECP mode.\n") );

#if (0 == DVRH_USE_PARPORT_ECP_ADDR)
            wPortECR = Controller + ECR_OFFSET;
#else
            wPortECR = EcpController + ECR_OFFSET;
#endif

            // get value from ECR reg & save it
            ecrLast = READ_PORT_UCHAR( wPortECR );

            // Determining Fifo Size
            PptDetermineFifoWidth(Extension);    
            PptDetermineFifoDepth(Extension);

            // return ecr to original
            WRITE_PORT_UCHAR( wPortECR, ecrLast);

            Status = Extension->ChipInfo.ParChipClearMode ( Extension->ChipInfo.Context, ECR_ECP_MODE );
        }    
    
    }    

    PptDumpV( ("Leaving PptDetectChipFilter(...) with Status 0x%x\n", Status) );

    return Status;
}

NTSTATUS
PptDetectPortType(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine is called once per DeviceObject to detect the type of 
    parallel chip capabilities of the port in question.
    
Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if we were able detect the chip and modes possible.
   !STATUS_SUCCESS  - otherwise.

--*/

{
    NTSTATUS                    Status;
    UNICODE_STRING              ParportPath;
    RTL_QUERY_REGISTRY_TABLE    RegTable[2];
    ULONG                       IdentifierHex = 12169;
    ULONG                       zero = 0;

    PptDumpV( ("PARPORT: Enter PptDetectPortType().\n") );

    //
    // -- May want to get detection order from Registry.
    // -- May also want to store/retrieve last known good configuration in/from registry.
    // -- Finally we should set a registry flag during dection so that we'll know
    //    if we crashed while attempting to detect and not try it again.
    //
    RtlInitUnicodeString(&ParportPath, (PWSTR)L"Parport");

    // Setting up to get the Parport info
    RtlZeroMemory( RegTable, sizeof(RegTable) );

    RegTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT |
                        RTL_QUERY_REGISTRY_REQUIRED;
    RegTable[0].Name = (PWSTR)L"ModeCheckedStalled";
    RegTable[0].EntryContext = &IdentifierHex;
    RegTable[0].DefaultType = REG_DWORD;
    RegTable[0].DefaultData = &zero;
    RegTable[0].DefaultLength = sizeof(ULONG);

    //
    // Querying the registry for Parport to see if we tried to go check mode and we crashed
    // the registry key would still be there 
    //
    Status = RtlQueryRegistryValues(
                                RTL_REGISTRY_SERVICES,
                                ParportPath.Buffer,
                                RegTable,
                                NULL,
                                NULL );

    PptDump2(PARINITDEV, ("PptDetectPortType: Query Register returned : %x.\n", Status) );

    PptDump2(PARINITDEV, ("PptDetectPortType: IdentifierHex is : %x.\n", IdentifierHex) );

    //
    // if registry key is there then we will just check ECP and Byte
    //
//    if ( !NT_SUCCESS( Status ) || (IdentifierHex == zero)) {
    if ( !(NT_SUCCESS( Status ) && IdentifierHex == 0) && (Status != STATUS_OBJECT_NAME_NOT_FOUND) ) {
        PptDump2(PARINITDEV, ("PptDetectPortType: Found Registry value so we locked up last time.\n") );

        // dvtw, Check for ECP anyway!  We just won't turn it on
        PptDump2(PARINITDEV, ("PptDetectPortType: Only check ECP and Byte.\n") );

        PptDetectEcpPort(Extension);
        PptDetectBytePort(Extension);

        if (Extension->PnpInfo.HardwareCapabilities & 
            (PPT_ECP_PRESENT | PPT_BYTE_PRESENT) ) {
            return ( STATUS_SUCCESS );
        } else {
            return ( STATUS_NO_SUCH_DEVICE );
        }
    }
    
    IdentifierHex = 12169;
    // Write the registry key out there just in case we crash
    Status = RtlWriteRegistryValue(
                                RTL_REGISTRY_SERVICES,
                                ParportPath.Buffer,
                                (PWSTR)L"ModeCheckedStalled",
                                REG_DWORD,
                                &IdentifierHex,
                                sizeof(ULONG) );
            
    //
    // Now we can start detecting the parallel port chip capabilities
    //
    PptDumpV( ("PARPORT: PptDetectPortType() Calling PptDetectPortCapablities().\n") );
    
    Status = PptDetectPortCapabilities( Extension );

    PptDumpV( ("PARPORT: PptDetectPortType() PptDetectPortCapablities returned %x.\n", Status) );

    // Delete the registry key out there since we finished
    Status = RtlDeleteRegistryValue(
                                RTL_REGISTRY_SERVICES,
                                ParportPath.Buffer,
                                (PWSTR)L"ModeCheckedStalled"
                                ); 

    PptDumpV( ("PARPORT: Leave PptDetectPortType() with Status %x.\n", Status) );

    return Status;

}

NTSTATUS
PptDetectPortCapabilities(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This is the "default" detection code, which looks for an ECR.  If the ECR
    is present it tries to set mode 100b in <7:5>. If it sticks we'll call it
    EPP.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if the port type was detected.
   !STATUS_SUCCESS  - otherwise.

--*/

{
    NTSTATUS    Status;

    PptDump2(PARINITDEV, ("Entering PptDetectPortCapablilities\n") );

    PptDump2(PARINITDEV, ("PptDetectPortCapablilities(): Detecting ECP\n") );
    PptDetectEcpPort( Extension );
    
    // dvdr 
    // 
    // if we did not detect an ECR for ECP mode and ECP mode failed
    // EPP mode would fail also
    // Also cannot have EPP mode at an address that ends with a "C"
    // 
    if ( (Extension->PnpInfo.HardwareCapabilities & PPT_ECP_PRESENT) 
         && (((ULONG_PTR)Extension->PortInfo.Controller & 0x0F) != 0x0C) ) {
        PptDump2(PARINITDEV, ("PptDetectPortCapablilities ECP Found Detecting for National Chipset\n") );
        // Need to check for National chipsets before trying EPP mode
        // dvdr - need to add detection for old Winbond
        Status = PptFindNatChip( Extension );
        if ( NT_SUCCESS( Status ) ) {
            if ( !Extension->NationalChipFound ) {
                // National chipset was NOT found so we can see if generic EPP is supported
                PptDump2(PARINITDEV, ("National chipset not found on %lx\n", Extension->PortInfo.Controller) );
                PptDump2(PARINITDEV, ("PptDetectPortCapablilities Detecting EPP\n") );
                PARDD01(("-- !NationalChipFound\n"));
                PARDD01(("-- pre  PptDetectEppPortIfDot3DevicePresent\n"));
                PptDetectEppPortIfDot3DevicePresent( Extension );
                PARDD01(("-- post PptDetectEppPortIfDot3DevicePresent\n"));
                if( !Extension->CheckedForGenericEpp ) {
                    // we didn't have a dot3 device to use for screening, do check anyway
                    //   if user has explicitly requested EPP detection
                    PARDD01(("-- no dot3 device - perhaps user requested that we check for EPP anyway?\n"));
                    PARDD01(("-- pre  PptDetectEppPortIfUserRequested\n"));
                    PptDetectEppPortIfUserRequested( Extension );
                    PARDD01(("-- post PptDetectEppPortIfUserRequested\n"));
                }
            } else {
                // National chipset was found so can't do generic EPP
                PptDump2(PARINITDEV, ("National chipset found\n") );
                PARDD01(("-- NationalChipFound\n"));
                Extension->CheckedForGenericEpp = TRUE; // check is complete - generic EPP is unsafe
            }
        }
    } else {
        // ECP failed no check for Generic EPP
        PptDump2(PARINITDEV, ("ECP on port %lx Failed Not Checking EPP\n", Extension->PortInfo.Controller) );
        PARDD01(("-- !ECP\n"));
        Extension->CheckedForGenericEpp = TRUE; // check is complete - generic EPP is unsafe
    }

    PptDump2(PARINITDEV, ("PptDetectPortCapablilities Detecting BYTE\n") );
    PptDetectBytePort( Extension );
    
    if (Extension->PnpInfo.HardwareCapabilities & 
            (PPT_ECP_PRESENT | PPT_EPP_PRESENT | PPT_BYTE_PRESENT) ) {
        PptDump2(PARINITDEV, ("Leaving PptDetectPortCapablilities was successful\n") );
        return STATUS_SUCCESS;
    }

    PptDump2(PARINITDEV, ("Leaving PptDetectPortCapablilities no Device found\n") );
    return STATUS_NO_SUCH_DEVICE;    
}

VOID
PptDetectEcpPort(
    IN  PDEVICE_EXTENSION   Extension
    )
    
/*++
      
Routine Description:
      
    This routine looks for the presence of an ECR register to determine that
      it has ECP.
      
Arguments:
      
    Extension           - Supplies the device extension of the device we are
                            reporting resources for.
      
Return Value:
      
    None.
      
--*/
    
{
    PUCHAR  Controller;
    PUCHAR  wPortDCR;       // IO address of Device Control Register (DCR)
    PUCHAR  wPortECR;       // IO address of Extended Control Register (ECR)
    UCHAR   ecrLast, ecr, dcr;
    
    Controller = Extension->PortInfo.Controller;
    wPortDCR = Controller + DCR_OFFSET;

#if (0 == DVRH_USE_PARPORT_ECP_ADDR)
    wPortECR = Controller + OFFSET_ECR;
#else
    if( 0 == Extension->PnpInfo.EcpController ) {
        //
        // PnP didn't give us an ECP Register set - we're done here
        //
        PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort:  Start\n"));
        return;
    }
    wPortECR = Extension->PnpInfo.EcpController + ECR_OFFSET;
#endif

    PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort:  Start\n"));

    PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort: Ecr port : %x\n", wPortECR));

    ecrLast = ecr = READ_PORT_UCHAR(wPortECR);
    PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort: Ecr Value before DCR write: %x\n", ecr));

    // Initialize the DCR's nAutoFeed and nStrobe to a harmless combination
    // that could be returned by the ECR, but is not likely to be returned if
    // the ECR isn't present.  Depending on the host's address decode logic,
    // reading a non-existant ECR could have one of two results:  the ECR address
    // could decode on top of the DCR, so we'll read the value we are about to set.
    // Alternately, we might just read a floating bus and get a random value.
    dcr = SET_DCR( DIR_WRITE, IRQEN_DISABLE, INACTIVE, ACTIVE, INACTIVE, ACTIVE );
    WRITE_PORT_UCHAR( wPortDCR, dcr );

    ecrLast = ecr = READ_PORT_UCHAR(wPortECR);
    
    PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort: Ecr Value After DCR write: %x\n", ecr));
    
    // Attempt to read the ECR.  If ECP hardware is present, the ECR register's
    // bit 1 and bit 0 should read a 00 (some data in the FIFO), 01 (FIFO empty),
    // or 10 (FIFO full).  If we read a 11 (illegal combination) then we know for
    // sure that no ECP hardware is present.  Also, a valid ECR should never return
    // 0xFF (but a nonexistant register probably would), so we'll test for that 
    // specific value also.
    if ( ( TEST_ECR_FIFO( ecr, ECR_FIFO_MASK ) ) || ( ecrLast == 0xFF ) )
    {
        // ECR[1:0] returned a value of 11, so this can't be hardware ECP.
        PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort:  illegal FIFO status\n"));

        // Restore the DCR so that all lines are inactive.
        dcr = SET_DCR( DIR_WRITE, IRQEN_DISABLE, INACTIVE, ACTIVE, ACTIVE, ACTIVE );
        WRITE_PORT_UCHAR( wPortDCR, dcr );
        return;
    }

    // OK, so we got either a 00, 01, or 10 for ECR[1:0].  If it was 10, the
    if ( TEST_ECR_FIFO( ecr, ECR_FIFO_FULL ) )  // Looking for ECR[1:0] of 10...
    {
        // The ECR[1:0] returned 10.  This is a legal value, but possibly the
        // hardware might have just decoded the DCR and we merely read back the
        // DCR value we set earlier.  Or, we might have just read back a value
        // that was hanging on the bus due to bus capacitance.  So, we'll change 
        // the DCR, read the ECR again, and see if the two registers continue to 
        // track each other.  If they do track, we'll conclude that there is no
        // ECP hardware.

        // Put the DCR's nAutoFeed and nStrobe register bits back to zero.
        dcr = SET_DCR( DIR_WRITE, IRQEN_DISABLE, INACTIVE, ACTIVE, ACTIVE, ACTIVE );
        WRITE_PORT_UCHAR( wPortDCR, dcr );

        // Read the ECR again
        ecr = READ_PORT_UCHAR( wPortECR );

        if ( TEST_ECR_FIFO( ecr, ECR_FIFO_SOME_DATA ) )
        {
            // ECR[1:0] is tracking DCR[1:0], so this can't be hardware ECP.
            PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort: FIFO status follows DCR\r\n"));
            // Restore the DCR so that all lines are inactive.
            dcr = SET_DCR( DIR_WRITE, IRQEN_DISABLE, INACTIVE, ACTIVE, ACTIVE, ACTIVE );
            WRITE_PORT_UCHAR( wPortDCR, dcr );
            return;
        }
    }
    
    // If we get this far, then the ECR appears to be returning something valid that
    // doesn't track the DCR.  It is beginning to look promising.  We're going
    // to take a chance, and write the ECR to put the chip in compatiblity
    // mode.  Doing so will reset the FIFO, so when we read FIFO status it should
    // come back empty.  However, if we're wrong and this isn't ECP hardware, the
    // value we're about to write will turn on 1284Active (nSelectIn) and this might
    // cause headaches for the peripheral.
    WRITE_PORT_UCHAR( wPortECR, DEFAULT_ECR_COMPATIBILITY );

    // Read the ECR again
    ecr = READ_PORT_UCHAR( wPortECR );

    // Now test the ECR snapshot to see if the FIFO status is correct.  The FIFO
    // should test empty.
    if (!TEST_ECR_FIFO( ecr, ECR_FIFO_EMPTY ) )
    {
        PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort: FIFO shows full\n"));
        // Restore the DCR so that all lines are inactive.
        dcr = SET_DCR( DIR_WRITE, IRQEN_DISABLE, INACTIVE, ACTIVE, ACTIVE, ACTIVE );
        WRITE_PORT_UCHAR( wPortDCR, dcr );
        return;
    }

    // OK, it looks very promising.  Perform a couple of additional tests that
    // will give us a lot of confidence, as well as providing some information
    // we need about the ECP chip.
    
    // return ecr to original
    WRITE_PORT_UCHAR(wPortECR, ecrLast);

    //
    // Test here for ECP capable
    //

    // get value from ECR reg & save it
    ecrLast = READ_PORT_UCHAR( wPortECR );
    ecr     = (UCHAR)(ecrLast & ECR_MODE_MASK);

    // Put the chip into test mode; the FIFO should start out empty
    WRITE_PORT_UCHAR(wPortECR, (UCHAR)(ecr | ECR_TEST_MODE) );

    PptDetermineFifoWidth(Extension);    
    if (0 != Extension->PnpInfo.FifoWidth)
    {
        Extension->PnpInfo.HardwareCapabilities |= PPT_ECP_PRESENT;
 
        PptDetermineFifoDepth(Extension);

        PptDump2(PARINITDEV, ("ParMode::PptDetectEcpPort: ECP present\n") );
    }
    
    // return ecr to original
    WRITE_PORT_UCHAR( wPortECR, ecrLast);
    return;
}

VOID
PptDetectEppPortIfDot3DevicePresent(
    IN  PDEVICE_EXTENSION   Extension
    )
    
/*++
      
Routine Description:
      
    If a 1284.3 daisy chain device is present, use the dot3 device to screen
    any printer from signal leakage while doing EPP detection. Otherwise
    abort detection.
      
Arguments:
      
    Extension           - Supplies the device extension of the device we are
                            reporting resources for.
      
Return Value:
      
    None.
      
--*/
    
{
    NTSTATUS status;
    PUCHAR   Controller = Extension->PortInfo.Controller;
    UCHAR    Reverse = (UCHAR)(DCR_DIRECTION | DCR_NOT_INIT | DCR_AUTOFEED | DCR_DSTRB);
    UCHAR    Forward = (UCHAR)(DCR_NOT_INIT | DCR_AUTOFEED | DCR_DSTRB);
    BOOLEAN  daisyChainDevicePresent = FALSE;
    // ULONG    DisableEppTest = 0; // any value != 0 means disable the test
    PARALLEL_1284_COMMAND Command;

    if( 0 == Extension->PnpInfo.Ieee1284_3DeviceCount ) {
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPortIfDot3DevicePresent "
                              "- No dot3 device present - aborting - Controller=%x\n", Controller) );
        PARDD01(("-- No DOT3 Device Present\n"));
        return;
    }
        
    //
    // 1284.3 daisy chain device is present. Use device to screen printer from
    //   possible signal leakage.
    //

    //
    // Select 1284.3 daisy chain  device
    //
    Command.ID           = 0;
    Command.Port         = 0;
    Command.CommandFlags = PAR_HAVE_PORT_KEEP_PORT;
    status = PptTrySelectDevice( Extension, &Command );
    if( !NT_SUCCESS( status ) ) {
        // unable to select device - something is wrong - just bail out
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPortIfDot3DevicePresent: "
                              "unable to select 1284.3 device - bailing out - Controller=%x\n",
                              Controller) );
        return;
    }

    //
    // do the detection for chipset EPP capability
    //
    PARDD01(("-- DOT3 Device Present and selected\n"));
    PptDetectEppPort( Extension );

    //
    // Deselect 1284.3 daisy chain device
    //
    Command.ID           = 0;
    Command.Port         = 0;
    Command.CommandFlags = PAR_HAVE_PORT_KEEP_PORT;
    status = PptDeselectDevice( Extension, &Command );
    if( !NT_SUCCESS( status ) ) {
        // deselect failed??? - this shouldn't happen - our daisy chain interface is likely hung
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: deselect 1284.3 device FAILED - Controller=%x\n", Controller) );
    } else {
        // 1284.3 daisy chain device deselected
        PARDD01(("-- DOT3 Device deselected\n"));
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: 1284.3 device deselected - Controller=%x\n", Controller) );
    }
    
#if DBG
    if( Extension->PnpInfo.HardwareCapabilities & PPT_EPP_PRESENT ) {
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: EPP present - Controller=%x\n", Controller) );
    } else {
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: EPP NOT present - Controller=%x\n", Controller) );        
    }
#endif

    return;
}

VOID
PptDetectEppPortIfUserRequested(
    IN  PDEVICE_EXTENSION   Extension
    )
    
/*++
      
Routine Description:
      
    If user explicitly requested Generic EPP detection then do the check.
      
Arguments:
      
    Extension           - Supplies the device extension of the device we are
                            reporting resources for.
      
Return Value:
      
    None.
      
--*/
    
{
    ULONG RequestEppTest = 0;
    PptRegGetDeviceParameterDword( Extension->PhysicalDeviceObject, (PWSTR)L"RequestEppTest", &RequestEppTest );
    if( RequestEppTest ) {
        PARDD01(("-- User Requested EPP detection - %x\n", RequestEppTest));
        PptDetectEppPort( Extension );
    } else {
        PARDD01(("-- User did not request EPP detection\n"));
    }
    return;
}

VOID
PptDetectEppPort(
    IN  PDEVICE_EXTENSION   Extension
    )
    
/*++
      
Routine Description:
      
    This routine checks for EPP capable port after ECP was found.
      
Arguments:
      
    Extension           - Supplies the device extension of the device we are
                            reporting resources for.
      
Return Value:
      
    None.
      
--*/
    
{
    PUCHAR   Controller;
    UCHAR    dcr, i;
    UCHAR    Reverse = (UCHAR)(DCR_DIRECTION | DCR_NOT_INIT | DCR_AUTOFEED | DCR_DSTRB);
    UCHAR    Forward = (UCHAR)(DCR_NOT_INIT | DCR_AUTOFEED | DCR_DSTRB);
    BOOLEAN  daisyChainDevicePresent = FALSE;
    ULONG    DisableEppTest = 0; // any value != 0 means disable the test

    PARDD01(("-- PptDetectEppPort - Enter\n"));
    PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: Enter\n") );

    Controller = Extension->PortInfo.Controller;
    
    // Get current DCR
    dcr = READ_PORT_UCHAR( Controller + DCR_OFFSET );

    //
    // Temporarily set capability to true to bypass PptEcrSetMode validity
    //   check. We'll clear the flag before we return if EPP test fails.
    //
    Extension->PnpInfo.HardwareCapabilities |= PPT_EPP_PRESENT;

    // Setting EPP mode
    PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: Setting EPP Mode\n") );
    PptEcrSetMode( Extension, ECR_EPP_PIO_MODE );

    //
    // Testing the hardware for EPP capable
    //
    for ( i = 0x01; i <= 0x02; i++ ) {
        // Put it into reverse phase so it doesn't talk to a device
        WRITE_PORT_UCHAR( Controller + DCR_OFFSET, Reverse );
        KeStallExecutionProcessor( 5 );
        WRITE_PORT_UCHAR( Controller + EPP_OFFSET, (UCHAR)i );

        // put it back into forward phase to read the byte we put out there
        WRITE_PORT_UCHAR( Controller + DCR_OFFSET, Forward );
        KeStallExecutionProcessor( 5 );
        if ( READ_PORT_UCHAR( Controller ) != i ) {
            // failure so clear EPP flag
            Extension->PnpInfo.HardwareCapabilities &= ~PPT_EPP_PRESENT;
            break;
        }
    }

    // Clearing EPP Mode
    PptEcrClearMode( Extension );
    // Restore DCR
    WRITE_PORT_UCHAR( Controller + DCR_OFFSET, dcr );

    Extension->CheckedForGenericEpp = TRUE; // check is complete

    if( Extension->PnpInfo.HardwareCapabilities & PPT_EPP_PRESENT ) {
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: EPP present - Controller=%x\n", Controller) );
        PARDD01(("-- PptDetectEppPort - HAVE Generic EPP\n"));
    } else {
        PptDump2(PARINITDEV, ("ParMode::PptDetectEppPort: EPP NOT present - Controller=%x\n", Controller) );        
        PARDD01(("-- PptDetectEppPort - DON'T HAVE Generic EPP\n"));
    }

    PARDD01(("-- PptDetectEppPort - Exit\n"));
    return;
}

VOID
PptDetectBytePort(
    IN  PDEVICE_EXTENSION   Extension
    )
    
/*++
      
Routine Description:
      
    This routine check to see if the port is Byte capable.
      
Arguments:
      
    Extension           - Supplies the device extension of the device we are
                            reporting resources for.
      
Return Value:
      
    None.
      
--*/
    
{
    NTSTATUS    Status = STATUS_SUCCESS;
    
    PptDump2(PARINITDEV, ("ParMode::PptDetectBytePort Enter.\n" ) );

    Status = PptSetByteMode( Extension, ECR_BYTE_PIO_MODE );

    if ( NT_SUCCESS(Status) ) {
        // Byte Mode found
        PptDump2(PARINITDEV, ("ParMode::PptDetectBytePort: Byte Found\n") );
        Extension->PnpInfo.HardwareCapabilities |= PPT_BYTE_PRESENT;
    } else {
        // Byte Mode Not Found
        PptDump2(PARINITDEV, ("ParMode::PptDetectBytePort: Byte Not Found\n"));
    }    
    
    (VOID)PptClearByteMode( Extension );

}

VOID PptDetermineFifoDepth(
    IN PDEVICE_EXTENSION   Extension
    )
{
    PUCHAR  Controller;
    PUCHAR  wPortECR;       // IO address of Extended Control Register (ECR)
    PUCHAR  wPortDFIFO;
    UCHAR   ecr, ecrLast;
    ULONG   wFifoDepth;
    UCHAR   writeFifoDepth;     // Depth calculated while writing FIFO
    UCHAR   readFifoDepth;      // Depth calculated while reading FIFO
    ULONG   limitCount;         // Prevents infinite looping on FIFO status
    UCHAR   testData;
    
    Controller = Extension->PortInfo.Controller;
    #if (0 == DVRH_USE_PARPORT_ECP_ADDR)
        wPortECR = Controller + ECR_OFFSET;
        wPortDFIFO = Controller + ECP_DFIFO_OFFSET;
    #else
        wPortECR =  Extension->PnpInfo.EcpController+ ECR_OFFSET;
        wPortDFIFO = Extension->PnpInfo.EcpController;
    #endif
    wFifoDepth = 0;

    ecrLast = READ_PORT_UCHAR(wPortECR );

    WRITE_PORT_UCHAR(wPortECR, DEFAULT_ECR_TEST );

    ecr = READ_PORT_UCHAR(wPortECR );
    
    if ( TEST_ECR_FIFO( ecr, ECR_FIFO_EMPTY ) ) {
    
        // Write bytes into the FIFO until it indicates full.
        writeFifoDepth = 0;
        limitCount     = 0;
        
        while (((READ_PORT_UCHAR (wPortECR) & ECR_FIFO_MASK) != ECR_FIFO_FULL ) &&
                    (limitCount <= ECP_MAX_FIFO_DEPTH)) {
                    
            WRITE_PORT_UCHAR( wPortDFIFO, (UCHAR)(writeFifoDepth & 0xFF));
            writeFifoDepth++;
            limitCount++;
        }
        
        PptDump2(PARINITDEV, ("ParMode::PptDetermineFifoDepth::  write fifo depth = %d\r\n", writeFifoDepth ));

        // Now read the bytes back, comparing what comes back.
        readFifoDepth = 0;
        limitCount    = 0;
        
        while (((READ_PORT_UCHAR( wPortECR ) & ECR_FIFO_MASK ) != ECR_FIFO_EMPTY ) &&
                    (limitCount <= ECP_MAX_FIFO_DEPTH)) {
                    
            testData = READ_PORT_UCHAR( wPortDFIFO );
            if ( testData != (readFifoDepth & (UCHAR)0xFF )) {
            
                // Data mismatch indicates problems...
                // FIFO status didn't pan out, may not be an ECP chip after all
                WRITE_PORT_UCHAR( wPortECR, ecrLast);
                PptDump2(PARINITDEV, ("ParMode::PptDetermineFifoDepth:::  data mismatch\n"));
                return;
            }
            
            readFifoDepth++;
            limitCount++;
        }

        PptDump2(PARINITDEV, ("ParMode::PptDetermineFifoDepth:::  read fifo depth = %d\r\n", readFifoDepth ));

        // The write depth should match the read depth...
        if ( writeFifoDepth == readFifoDepth ) {
        
            wFifoDepth = readFifoDepth;
            
        } else {
        
            // Assume no FIFO
            WRITE_PORT_UCHAR( wPortECR, ecrLast);
            PptDump2(PARINITDEV, ("ParMode::PptDetermineFifoDepth:::  No Fifo\n"));
            return;
        }
                
    } else {
    
        // FIFO status didn't pan out, may not be an ECP chip after all
        PptDump2(PARINITDEV, ("ParMode::PptDetermineFifoDepth::  Bad Fifo\n"));
        WRITE_PORT_UCHAR(wPortECR, ecrLast);
        return;
    }

    // put chip into spp mode
    WRITE_PORT_UCHAR( wPortECR, ecrLast );
    Extension->PnpInfo.FifoDepth = wFifoDepth;
}

VOID
PptDetermineFifoWidth(
    IN PDEVICE_EXTENSION   Extension
    )
{
    PUCHAR Controller;
    UCHAR   bConfigA;
    PUCHAR wPortECR;

    PptDump2(PARINITDEV, ("ParMode::PptDetermineFifoWidth: Start\n"));
    Controller = Extension->PortInfo.Controller;

#if (0 == DVRH_USE_PARPORT_ECP_ADDR)
    wPortECR = Controller + ECR_OFFSET;
#else
    wPortECR = Extension->PnpInfo.EcpController + ECR_OFFSET;
#endif

    // Put chip into configuration mode so we can access the ConfigA register
    WRITE_PORT_UCHAR( wPortECR, DEFAULT_ECR_CONFIGURATION );

    // The FIFO width is bits <6:4> of the ConfigA register.
    #if (0 == DVRH_USE_PARPORT_ECP_ADDR)
        bConfigA = READ_PORT_UCHAR( Controller + CNFGA_OFFSET );
    #else
        bConfigA = READ_PORT_UCHAR( Extension->PnpInfo.EcpController );
    #endif
    Extension->PnpInfo.FifoWidth = (ULONG)(( bConfigA & CNFGA_IMPID_MASK ) >> CNFGA_IMPID_SHIFT);

    // Put the chip back in compatibility mode.
    WRITE_PORT_UCHAR(wPortECR, DEFAULT_ECR_COMPATIBILITY );
    return;
}

NTSTATUS
PptSetChipMode (
    IN  PDEVICE_EXTENSION  Extension,
    IN  UCHAR              ChipMode
    )

/*++

Routine Description:

    This function will put the current parallel chip into the
    given mode if supported.  The determination of supported mode 
    was in the PptDetectPortType function.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if the port type was detected.
   !STATUS_SUCCESS  - otherwise.

--*/

{
    NTSTATUS    Status = STATUS_SUCCESS;
    UCHAR EcrMode = (UCHAR)( ChipMode & ~ECR_MODE_MASK );

    PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: Start\n"));

    // Also allow PptSetChipMode from PS/2 mode - we need this for HWECP
    //   bus flip from Forward to Reverse in order to meet the required
    //   sequence specified in the Microsoft ECP Port Spec version 1.06,
    //   July 14, 1993, to switch directly from PS/2 mode with output 
    //   drivers disabled (direction bit set to "read") to HWECP via 
    //   the ECR. Changed 2000-02-11.
    if ( Extension->PnpInfo.CurrentMode != INITIAL_MODE &&
         Extension->PnpInfo.CurrentMode != ECR_BYTE_MODE ) {

        PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: CurrentMode != INITIAL_MODE.\n"));

        // Current mode is not valid to put in EPP or ECP mode
        Status = STATUS_INVALID_DEVICE_STATE;

        goto ExitSetChipModeNoChange;
    }

    // need to find out what mode it was and try to take it out of it
    
    // Check to see if we need to use the filter to set the mode
    if ( Extension->FilterMode ) {
        PptDumpV(("PptSetChipMode: Chip Filter Present.\n"));
        Status = Extension->ChipInfo.ParChipSetMode ( Extension->ChipInfo.Context, ChipMode );
    } else {
        PptDumpV(("PptSetChipMode: Chip Filter NOT Present.\n"));

        // If asked for ECP check to see if we can do it
        if ( EcrMode == ECR_ECP_MODE ) {
            if ((Extension->PnpInfo.HardwareCapabilities & PPT_ECP_PRESENT) ^ PPT_ECP_PRESENT) {
                PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: ECP Not Present.\n"));
                return STATUS_NO_SUCH_DEVICE;
            }
            Status = PptEcrSetMode ( Extension, ChipMode );
            goto ExitSetChipModeWithChanges;
        }
        
        // If asked for EPP check to see if we can do it
        if ( EcrMode == ECR_EPP_MODE ) {
            if ((Extension->PnpInfo.HardwareCapabilities & PPT_EPP_PRESENT) ^ PPT_EPP_PRESENT) {
                PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: EPP Not Present.\n"));
                return STATUS_NO_SUCH_DEVICE;
            }
            Status = PptEcrSetMode ( Extension, ChipMode );
            goto ExitSetChipModeWithChanges;
        }

        // If asked for Byte Mode check to see if it is still enabled
        if ( EcrMode == ECR_BYTE_MODE ) {
            if ((Extension->PnpInfo.HardwareCapabilities & PPT_BYTE_PRESENT) ^ PPT_BYTE_PRESENT) {
                PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: BYTE Not Present.\n"));
                return STATUS_NO_SUCH_DEVICE;
            }
            Status = PptSetByteMode ( Extension, ChipMode );
            goto ExitSetChipModeWithChanges;
        }
    }
    
ExitSetChipModeWithChanges:
    if ( NT_SUCCESS(Status) ) {
        PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: Mode Set.\n"));
        Extension->PnpInfo.CurrentMode = EcrMode;
    } else {
        PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: Mode Not Set.\n"));
    }

ExitSetChipModeNoChange:

    PptDump2(PARINITDEV, ("ParMode::PptSetChipMode: Exit with Status - %x\n", Status ));

    return Status;

}

NTSTATUS
PptClearChipMode (
    IN  PDEVICE_EXTENSION  Extension,
    IN  UCHAR              ChipMode
    )

/*++

Routine Description:

    This routine Clears the Given chip mode.

Arguments:

    Extension   - Supplies the device extension.
    ChipMode    - The given mode to clear from the Chip

Return Value:

    STATUS_SUCCESS  - if the port type was detected.
   !STATUS_SUCCESS  - otherwise.

--*/

{
    NTSTATUS    Status = STATUS_UNSUCCESSFUL;
    ULONG EcrMode = ChipMode & ~ECR_MODE_MASK;

    PptDump2(PARINITDEV, ("ParMode::PptClearChipMode: Start\n"));

    // make sure we have a mode to clear
    if ( EcrMode != Extension->PnpInfo.CurrentMode ) {
                
        PptDump2(PARINITDEV, ("ParMode::PptClearChipMode: Mode to Clear != CurrentMode.\n"));

        // Current mode is not the same as requested to take it out of
        Status = STATUS_INVALID_DEVICE_STATE;

        goto ExitClearChipModeNoChange;
    }

    // need to find out what mode it was and try to take it out of it
    
    // check to see if we used the filter to set the mode
    if ( Extension->FilterMode ) {
        PptDumpV(("PptClearChipMode: Chip Filter Present.\n"));
        Status = Extension->ChipInfo.ParChipClearMode ( Extension->ChipInfo.Context, ChipMode );
    } else {
        PptDumpV(("PptClearChipMode: Chip Filter NOT Present.\n"));
        // If ECP mode check to see if we can clear it
        if ( EcrMode == ECR_ECP_MODE ) {
            Status = PptEcrClearMode( Extension );
            goto ExitClearChipModeWithChanges;
        }
    
        // If EPP mode check to see if we can clear it
        if ( EcrMode == ECR_EPP_MODE ) {
            Status = PptEcrClearMode( Extension );
            goto ExitClearChipModeWithChanges;
        }

        // If BYTE mode clear it if use ECR register
        if ( EcrMode == ECR_BYTE_MODE ) {
            Status = PptClearByteMode( Extension );
            goto ExitClearChipModeWithChanges;
        }    
    }
    
ExitClearChipModeWithChanges:
    if ( NT_SUCCESS(Status) ) {
        PptDump2(PARINITDEV, ("ParMode::PptClearChipMode: Clearing Mode Returned Success.\n"));

        Extension->PnpInfo.CurrentMode = INITIAL_MODE;
    }

ExitClearChipModeNoChange:

    PptDump2(PARINITDEV, ("ParMode::PptClearChipMode: Exit with Status - %x\n", Status ));

    return Status;
}

NTSTATUS
PptEcrSetMode(
    IN  PDEVICE_EXTENSION   Extension,
    IN  UCHAR               ChipMode
    )

/*++

Routine Description:

    This routine enables EPP mode through the ECR register.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if the port type was detected.
   !STATUS_SUCCESS  - otherwise.

--*/

{

    UCHAR   ecr;
    PUCHAR  Controller;
    PUCHAR  wPortECR;
            
    PptDump2(PARINITDEV, ("ParMode::PptEcrSetMode: Start.\n"));

    Controller = Extension->PortInfo.Controller;
    
    //
    // Store the prior mode.
    //
#if (0 == DVRH_USE_PARPORT_ECP_ADDR)
    wPortECR = Controller + ECR_OFFSET;
#else
    wPortECR = Extension->PnpInfo.EcpController + ECR_OFFSET;
#endif

    PptDump2(PARINFO, ("PptEcrSetMode:: wPortECR : %p.\n", wPortECR ));

    ecr = READ_PORT_UCHAR( wPortECR );
    Extension->EcrPortData = ecr;
    
    // get rid of prior mode which is the top three bits
    ecr &= ECR_MODE_MASK;

    // Write out SPP mode first to the chip
    WRITE_PORT_UCHAR( wPortECR, (UCHAR)(ecr | ECR_BYTE_MODE) );

    // Write new mode to ECR register    
    WRITE_PORT_UCHAR( wPortECR, ChipMode );
    
    PptDump2(PARINITDEV, ("ParMode::PptEcrSetMode: Exit.\n"));

    return STATUS_SUCCESS;

}

NTSTATUS
PptSetByteMode( 
    IN  PDEVICE_EXTENSION   Extension,
    IN  UCHAR               ChipMode
    )

/*++

Routine Description:

    This routine enables Byte mode either through the ECR register 
    (if available).  Or just checks it to see if it works

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if the port type was detected.
   !STATUS_SUCCESS  - otherwise.

--*/
{
    NTSTATUS    Status;
    
    // Checking to see if ECR register is there and if there use it
    if ( Extension->PnpInfo.HardwareCapabilities & PPT_ECP_PRESENT ) {
        Status = PptEcrSetMode( Extension, ChipMode );    
    }
    
    Status = PptCheckByteMode( Extension );

    return Status;

}    

NTSTATUS
PptClearByteMode( 
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine Clears Byte mode through the ECR register if there otherwise
    just returns success because nothing needs to be done.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if the port type was detected.
   !STATUS_SUCCESS  - otherwise.

--*/

{
    NTSTATUS    Status = STATUS_SUCCESS;
    
    // Put ECR register back to original if it was there
    if ( Extension->PnpInfo.HardwareCapabilities & PPT_ECP_PRESENT ) {
        Status = PptEcrClearMode( Extension );    
    }
    
    return Status;
}    

NTSTATUS
PptCheckByteMode(
    IN  PDEVICE_EXTENSION   Extension
    )
    
/*++
      
Routine Description:
      
    This routine checks to make sure we are still Byte capable before doing
    any transfering of data.
      
Arguments:
      
    Extension           - Supplies the device extension of the device we are
                            reporting resources for.
      
Return Value:
      
    None.
      
--*/
    
{
    PUCHAR  Controller;
    UCHAR   dcr;
    
    Controller = Extension->PortInfo.Controller;

    //
    // run the test again to make sure somebody didn't take us out of a
    // bi-directional capable port.
    //
    // 1. put in extended read mode.
    // 2. write data pattern
    // 3. read data pattern
    // 4. if bi-directional capable, then data patterns will be different.
    // 5. if patterns are the same, then check one more pattern.
    // 6. if patterns are still the same, then port is NOT bi-directional.
    //

    // get the current control port value for later restoration
    dcr = READ_PORT_UCHAR( Controller + DCR_OFFSET );

    // put port into extended read mode
    WRITE_PORT_UCHAR( Controller + DCR_OFFSET, (UCHAR)(dcr | DCR_DIRECTION) );

    // write the first pattern to the port
    WRITE_PORT_UCHAR( Controller, (UCHAR)0x55 );
    if ( READ_PORT_UCHAR( Controller ) == (UCHAR)0x55 ) {
        // same pattern, try the second pattern
        WRITE_PORT_UCHAR( Controller, (UCHAR)0xaa );
        if ( READ_PORT_UCHAR( Controller ) == (UCHAR)0xaa ) {
            // the port is NOT bi-directional capable
            return STATUS_UNSUCCESSFUL;
        }
    }

    // restore the control port to its original value
    WRITE_PORT_UCHAR( Controller + DCR_OFFSET, (UCHAR)dcr );

    return STATUS_SUCCESS;

}

NTSTATUS
PptEcrClearMode(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine disables EPP or ECP mode whichever one the chip
    was in through the ECR register.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - if it was successful.
   !STATUS_SUCCESS  - otherwise.

--*/

{

    UCHAR   ecr;
    PUCHAR  Controller;
    PUCHAR  wPortECR;
    
    Controller = Extension->PortInfo.Controller;
    
    //
    // Restore the prior mode.
    //

    // Get original ECR register
    ecr = Extension->EcrPortData;
    Extension->EcrPortData = 0;

    // some chips require to change modes only after 
    // you put it into spp mode

#if (0 == DVRH_USE_PARPORT_ECP_ADDR)
    wPortECR = Controller + ECR_OFFSET;
#else
    wPortECR = Extension->PnpInfo.EcpController + ECR_OFFSET;
#endif

    WRITE_PORT_UCHAR( wPortECR, (UCHAR)(ecr & ECR_MODE_MASK) );

    // Back to original mode
    WRITE_PORT_UCHAR( wPortECR, ecr );
    
    return STATUS_SUCCESS;

}

NTSTATUS
PptFindNatChip(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine finds out if there is a National Semiconductor IO chip on
    this machine.  If it finds a National chip it then determines if this 
    instance of Parport is using this chips paralle port IO address.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS       - if we were able to check for the parallel chip.
    STATUS_UNSUCCESSFUL  - if we were not able to check for the parallel chip.

Updates:
    
    Extension->
            NationalChecked
            NationalChipFound

--*/

{
    BOOLEAN             found = FALSE;              // return code, assumed value
    BOOLEAN             OkToLook = FALSE;
    BOOLEAN             Conflict;
    PUCHAR              ChipAddr[4] = { (PUCHAR)0x398, (PUCHAR)0x26e, (PUCHAR)0x15c, (PUCHAR)0x2e };  // list of valid chip addresses
    PUCHAR              AddrList[4] = { (PUCHAR)0x378, (PUCHAR)0x3bc, (PUCHAR)0x278, (PUCHAR)0x00 };  // List of valid Parallel Port addresses
    PUCHAR              PortAddr;                   // Chip Port Address
    ULONG_PTR           Port;                       // Chip Port Read Value
    UCHAR               SaveIdx;                    // Save the index register value
    UCHAR               cr;                         // config register value
    UCHAR               ii;                         // loop index
    NTSTATUS            Status;                     // Status of success
    ULONG               ResourceDescriptorCount;
    ULONG               ResourcesSize;
    PCM_RESOURCE_LIST   Resources;
    ULONG               NationalChecked   = 0;
    ULONG               NationalChipFound = 0;

    
    //
    // Quick exit if we already know the answer
    //
    if ( Extension->NationalChecked == TRUE ) {
        PptDump2(PARINITDEV, ("ParMode::PptFindNatChip: Already found NatSemi\n") );
        return STATUS_SUCCESS;
    }

    //
    // Mark extension so that we can quick exit the next time we are asked this question
    //
    Extension->NationalChecked = TRUE; 

    //
    // Check the registry - we should only need to check this once per installation
    //
    PptRegGetDeviceParameterDword(Extension->PhysicalDeviceObject, (PWSTR)L"NationalChecked", &NationalChecked);
    if( NationalChecked ) {
        //
        // We previously performed the NatSemi Check - extract result from registry
        //
        PptRegGetDeviceParameterDword(Extension->PhysicalDeviceObject, (PWSTR)L"NationalChipFound", &NationalChipFound);
        if( NationalChipFound ) {
            Extension->NationalChipFound = TRUE;
        } else {
            Extension->NationalChipFound = FALSE;
        }
        return STATUS_SUCCESS;
    }

    //
    // This is our first, and hopefully last time that we need to make this check
    //   for this installation
    //

    //
    // Allocate a block of memory for constructing a resource descriptor
    //

    // number of partial descriptors 
    ResourceDescriptorCount = sizeof(ChipAddr)/sizeof(ULONG);

    // size of resource descriptor list + space for (n-1) more partial descriptors
    //   (resource descriptor list includes one partial descriptor)
    ResourcesSize =  sizeof(CM_RESOURCE_LIST) +
        (ResourceDescriptorCount - 1) * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);

    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(NonPagedPool, ResourcesSize);

    if (Resources == NULL) {
        PptDump2(PARINITDEV, ("ParMode::PptFindNatChip: No Resources Punting\n"));
        // Error out
        return(STATUS_UNSUCCESSFUL);
    }

    // zero out memory block as a precaution
    RtlZeroMemory(Resources, ResourcesSize);

    //
    // Build the Resource List
    //
    Status = PptBuildResourceList( Extension,
                                   sizeof(ChipAddr)/sizeof(ULONG),
                                   &ChipAddr[0],
                                   Resources
                                   );
    
    // Check to see if it was successful    
    if ( !NT_SUCCESS( Status ) )
    {
        PptDump2(PARINITDEV, ("ParMode::PptFindNatChip: Couldn't build Resources list. Punting\n") );
        ExFreePool( Resources );
        return ( Status );
    }

    // 
    // check to see if we can use the io addresses where
    // national chipsets are located
    //
    Status = IoReportResourceUsage( NULL,
                                    Extension->DriverObject,
                                    Resources,
                                    sizeof(Resources),
                                    Extension->DeviceObject,
                                    NULL,
                                    0,
                                    FALSE,
                                    &Conflict
                                    ); 

    // done with resource list
    ExFreePool( Resources );

    // Check to see if IoReportResourceUsage was successful    
    if ( !NT_SUCCESS( Status ) )
    {
        PptDump2(PARINITDEV, ("ParMode::PptFindNatChip: Couldn't report usage. Punting\n") );
        return ( Status );
    }

    // Check to see if it was successful    
    if ( Conflict )
    {
        PptDump2(PARINITDEV, ("ParMode::PptFindNatChip: Conflict. Punting\n"));
        return ( STATUS_UNSUCCESSFUL );
    }


    // Was successful so now we check each of the addresses that we have
    // the resources for
    //
    // the following for loop is a state machine that checks modes and
    // port addresses.
    //
    // state 0: check for Pc873 at primary port address
    // state 1: check for Pc873 at secondary port address
    // state 2: check for Pc873 at Ter port address
    // state 3: check for Pc873 at Quad port address
    
    for ( ii = 0; !found && ii < 4; ii++ ) {

        PortAddr = (PUCHAR)ChipAddr[ii];

        // After power up the index register will read back an 0xAA one time only.
        // So we'll check for that first.
        // Then it should read back a 0 or a valid register number
            
        if(( READ_PORT_UCHAR( PortAddr ) == 0x88 )
           && ( READ_PORT_UCHAR( PortAddr ) < 0x20 )) {

            OkToLook = TRUE;

        } else {

            // Or it could read back a 0 or a valid register number
            READ_PORT_UCHAR( PortAddr );        // may read back 0 here
            cr = READ_PORT_UCHAR( PortAddr );   // valid register no.
  
            // is it really valid?
            // if( cr < 0x20 ) { - dvdr
            if( cr != 0xff ) {
                // does it read back the same?
                if( READ_PORT_UCHAR( PortAddr ) == cr)
                    OkToLook = TRUE;
            }

        } // end else
            
        // take a closer look by writing to the chip
        if ( OkToLook ) {

            OkToLook = FALSE;
                    
            // setup for ID reg
            WRITE_PORT_UCHAR( PortAddr, REG_CR8 );
                            
            // read it back
            cr = READ_PORT_UCHAR( PortAddr );
                            
            // does it read back the same?
            if( cr  == REG_CR8 ) {

                // get the ID number.
                cr = (UCHAR)( READ_PORT_UCHAR( PortAddr + 1 ) & 0xf0 );
                                    
                // if the up. nib. is 1,3,5,6,7,9,A,B,C
                if( cr == PC87332 || cr == PC87334 || cr == PC87306 || cr == PC87303 || 
                   cr == PC87323 || cr == PC87336 || cr == PC87338 || cr == PC873xx ) {

                    // we found a national chip
                    found = TRUE;

                    // setup for Address reg
                    WRITE_PORT_UCHAR( PortAddr, REG_CR1 );
                    
                    // read it back
                    Port = READ_PORT_UCHAR( PortAddr + 1 ) & 0x03;
                    
                    // Check the base address
                    if ( Extension->PortInfo.Controller == (PUCHAR)AddrList[ Port ] ) {

                        //
                        // it is using the same address that Parport is using
                        // so we set the flag to not use generic ECP and EPP
                        //
                        Extension->NationalChipFound = TRUE;

                    }
                            
                }

            } // reads back ok
                            
        } // end OkToLook

        // check to see if we found it
        if ( !found ) {

            // Check for the 307/308 chips
            SaveIdx = READ_PORT_UCHAR( PortAddr );

            // Setup for SID Register
            WRITE_PORT_UCHAR( PortAddr, PC873_DEVICE_ID );
                    
            // Zero the ID register to start and because it is read only it will
            // let us know whether it is this chip
            WRITE_PORT_UCHAR( PortAddr + 1, REG_CR0 );
                    
            // get the ID number.
            cr = (UCHAR)( READ_PORT_UCHAR( PortAddr + 1 ) & 0xf8 );
                    
            if ( (cr == PC87307) || (cr == PC87308) ) {

                // we found a new national chip
                found = TRUE;

                // Set the logical device
                WRITE_PORT_UCHAR( PortAddr, PC873_LOGICAL_DEV_REG );
                WRITE_PORT_UCHAR( PortAddr+1, PC873_PP_LDN );

                // set up for the base address MSB register
                WRITE_PORT_UCHAR( PortAddr, PC873_BASE_IO_ADD_MSB );
                            
                // get the MSB of the base address
                Port = (ULONG_PTR)((READ_PORT_UCHAR( PortAddr + 1 ) << 8) & 0xff00);
                            
                // Set up for the base address LSB register
                WRITE_PORT_UCHAR( PortAddr, PC873_BASE_IO_ADD_LSB );
                            
                // Get the LSBs of the base address
                Port |= READ_PORT_UCHAR( PortAddr + 1 );
                            
                // Check the base address
                if ( Extension->PortInfo.Controller == (PUCHAR)Port ) {
                    //
                    // it is using the same address that Parport is using
                    // so we set the flag to not use generic ECP and EPP
                    //
                    Extension->NationalChipFound = TRUE;
                }

            } else {

                WRITE_PORT_UCHAR( PortAddr, SaveIdx );
            }
        }

    } // end of for ii...
    

    //
    // Check for NatSemi chip is complete - save results in registry so that we never
    //   have to make this check again for this port
    //
    {
        PDEVICE_OBJECT pdo = Extension->PhysicalDeviceObject;
        NationalChecked    = 1;
        NationalChipFound  = Extension->NationalChipFound ? 1 : 0;
        
        // we ignore status here because there is nothing we can do if the calls fail
        PptRegSetDeviceParameterDword(pdo, (PWSTR)L"NationalChecked",   &NationalChecked);
        PptRegSetDeviceParameterDword(pdo, (PWSTR)L"NationalChipFound", &NationalChipFound);
    }


    // 
    // release the io addresses where we checked for the national chipsets
    // we do this by calling IoReportResourceUsage with all NULL parameters
    //
    Status = IoReportResourceUsage( NULL,
                                    Extension->DriverObject,
                                    NULL,
                                    0,
                                    Extension->DeviceObject,
                                    NULL,
                                    0,
                                    FALSE,
                                    &Conflict
                                    ); 

    PptDump2(PARINITDEV, ("ParMode::PptFindNatChip: return isFound [%x]\n", Extension->NationalChipFound));
    return ( Status );
    
} // end of ParFindNat()

NTSTATUS
PptBuildResourceList(
    IN  PDEVICE_EXTENSION   Extension,
    IN  ULONG               Partial,
    IN  PULONG              Addresses,
    OUT PCM_RESOURCE_LIST   Resources
    )

/*++

Routine Description:

    This routine Builds a CM_RESOURCE_LIST with 1 Full Resource
    Descriptor and as many Partial resource descriptors as you want
    with the same parameters for the Full.  No Interrupts or anything
    else just IO addresses.

Arguments:

    Extension   - Supplies the device extension.
    Partial     - Number (array size) of partial descriptors in Addresses[]
    Addresses   - Pointer to an Array of addresses of the partial descriptors
    Resources   - The returned CM_RESOURCE_LIST

Return Value:

    STATUS_SUCCESS       - if the building of the list was successful.
    STATUS_UNSUCCESSFUL  - otherwise.

--*/

{

    UCHAR       i;

    //
    // Number of Full Resource descriptors
    //
    Resources->Count = 1;
    
    Resources->List[0].InterfaceType = Extension->InterfaceType;
    Resources->List[0].BusNumber = Extension->BusNumber;
    Resources->List[0].PartialResourceList.Version = 0;
    Resources->List[0].PartialResourceList.Revision = 0;
    Resources->List[0].PartialResourceList.Count = Partial;

    //
    // Going through the loop for each partial descriptor
    //
    for ( i = 0; i < Partial ; i++ ) {

        //
        // Setup port
        //
        Resources->List[0].PartialResourceList.PartialDescriptors[i].Type = CmResourceTypePort;
        Resources->List[0].PartialResourceList.PartialDescriptors[i].ShareDisposition = CmResourceShareDriverExclusive;
        Resources->List[0].PartialResourceList.PartialDescriptors[i].Flags = CM_RESOURCE_PORT_IO;
        Resources->List[0].PartialResourceList.PartialDescriptors[i].u.Port.Start.QuadPart = Addresses[i];
        Resources->List[0].PartialResourceList.PartialDescriptors[i].u.Port.Length = (ULONG)2;

    }


    return ( STATUS_SUCCESS );

}

