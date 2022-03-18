/*
 ************************************************************************
 *
 *	INIT.C
 *
 *
 * Portions Copyright (C) 1996-1998 National Semiconductor Corp.
 * All rights reserved.
 * Copyright (C) 1996-1998 Microsoft Corporation. All Rights Reserved.
 *
 *
 *
 *************************************************************************
 */


    #include "nsc.h"
    #include "newdong.h"


    DongleParam  *IrDongle;
    UIR		 IrDongleResource;

    #define  SIR      0
    #define  MIR      1
    #define  FIR      2


    extern UINT rcvDMAchannel;
    extern UINT xmitDMAchannel;



    #define NSC_DEMO_IRDA_SPEEDS ( NDIS_IRDA_SPEED_2400    |	   \
				   NDIS_IRDA_SPEED_2400    |	   \
				   NDIS_IRDA_SPEED_9600    |	   \
				   NDIS_IRDA_SPEED_19200   |	   \
				   NDIS_IRDA_SPEED_38400   |	   \
				   NDIS_IRDA_SPEED_57600   |	   \
				   NDIS_IRDA_SPEED_115200  |	   \
				   NDIS_IRDA_SPEED_1152K   |	   \
				   NDIS_IRDA_SPEED_4M )


    //	NSC PC87108 index registers.  See the spec for more info.
    //
    enum indexRegs {
	    BAIC_REG	    = 0,
	    CSRT_REG	    = 1,
	    MCTL_REG	    = 2,
	    GPDIR_REG	    = 3,
	    GPDAT_REG	    = 4
    };

#define CS_MODE_CONFIG_OFFSET 0x8

    const UCHAR bankCode[] = { 0x03, 0x08, 0xE0, 0xE4, 0xE8, 0xEC, 0xF0, 0xF4 };

//////////////////////////////////////////////////////////////////////////
//									//
// Function :	   NSC_WriteBankReg					//
//									//
// Description: 							//
//  Write a value to the specified register of the specified register	//
//  bank.								//
//									//
//////////////////////////////////////////////////////////////////////////

void NSC_WriteBankReg(UINT comBase, UINT bankNum, UINT regNum, UCHAR val)
{
    NdisRawWritePortUchar(comBase+3, bankCode[bankNum]);
    NdisRawWritePortUchar(comBase+regNum, val);

    // Always switch back to reg 0
    NdisRawWritePortUchar(comBase+3, bankCode[0]);
}

//////////////////////////////////////////////////////////////////////////
//									//
// Function :	   NSC_ReadBankReg					//
//									//
// Description: 							//
//  Write the value from the specified register of the specified	//
//  register bank.							//
//									//
//////////////////////////////////////////////////////////////////////////


UCHAR NSC_ReadBankReg(UINT comBase, UINT bankNum, UINT regNum)
{
    UCHAR result;

    NdisRawWritePortUchar(comBase+3, bankCode[bankNum]);
    NdisRawReadPortUchar(comBase+regNum, &result);

    // Always switch back to reg 0
    NdisRawWritePortUchar(comBase+3, bankCode[0]);
		
    return result;
}

//////////////////////////////////////////////////////////////////////////
//									//
// Function :	   Ir108ConfigWrite					//
//									//
// Description: 							//
//  Write the data in the indexed register of the configuration I/O.	//
//									//
//////////////////////////////////////////////////////////////////////////

void Ir108ConfigWrite(UINT configIOBase, UCHAR indexReg, UCHAR data, BOOLEAN CSMode)
{
    UCHAR IndexStore;

    if (CSMode)
    {
        NdisRawWritePortUchar(configIOBase+indexReg, data);
        NdisRawWritePortUchar(configIOBase+indexReg, data);
    }
    else
    {
        NdisRawReadPortUchar(configIOBase, &IndexStore);
        NdisRawWritePortUchar(configIOBase, indexReg);
        NdisRawWritePortUchar(configIOBase+1, data);
        NdisRawWritePortUchar(configIOBase+1, data);
        NdisRawWritePortUchar(configIOBase, IndexStore);
    }
}

//////////////////////////////////////////////////////////////////////////
//									//
// Function :	   Ir108ConfigRead					//
//									//
// Description: 							//
//  Read the data in the indexed register of the configuration I/O.	//
//									//
//////////////////////////////////////////////////////////////////////////

UCHAR Ir108ConfigRead(UINT  configIOBase, UCHAR indexReg, BOOLEAN CSMode)
{
    UCHAR data,IndexStore;

    if (CSMode)
    {
        NdisRawReadPortUchar(configIOBase+indexReg, &data);
    }
    else
    {
        NdisRawReadPortUchar(configIOBase, &IndexStore);
        NdisRawWritePortUchar(configIOBase, indexReg);
        NdisRawReadPortUchar(configIOBase+1, &data);
        NdisRawWritePortUchar(configIOBase, IndexStore);
    }
    return (data);
}


//////////////////////////////////////////////////////////////////////////
//									//
// Function :	   PCIConfig560WriteWord				//
//									//
// Description: 							//
//  Write a word to the PCI configuration space for PC87560.		//
//									//
//////////////////////////////////////////////////////////////////////////

VOID PCIConfig560WriteWord(UINT pciCfgAdr, UCHAR RegisterNumber, UINT data)
{
    ULONG ConfigAddressIndex;

    ConfigAddressIndex = (0x80002900| (ULONG)RegisterNumber);
    NdisRawWritePortUlong(pciCfgAdr,
			  ConfigAddressIndex);
    NdisRawWritePortUshort(pciCfgAdr+4, data);

}

//////////////////////////////////////////////////////////////////////////
//									//
// Function :	   NSC_DEMO_Init					//
//									//
// Description: 							//
//  Set up configuration registers for NSC evaluation board.		//
//									//
// NOTE:								//
//  Assumes configuration registers are at I/O addr 0x398.		//
//  This function configures the demo board to make the SIR UART appear //
//  at <comBase>.							//
//									//
//  Called By:								//
//  OpenCom								//
//////////////////////////////////////////////////////////////////////////

BOOLEAN NSC_DEMO_Init(IrDevice *thisDev)
{
    UCHAR val;
    UCHAR FifoClear;
    BOOLEAN CSMode = FALSE;
    switch(thisDev->CardType){
    case PUMA108:
        CSMode = TRUE;
        thisDev->portInfo.ConfigIoBaseAddr = thisDev->portInfo.ioBase + CS_MODE_CONFIG_OFFSET;

	case PC87108:
	    // Look for id at startup.
        if (!CSMode)
        {
            NdisRawReadPortUchar(thisDev->portInfo.ConfigIoBaseAddr, &val);
            if (val != 0x5A){
                if (val == (UCHAR)0xff){
                    DBGERR(("didn't see PC87108 id (0x5A); got ffh."));
                    return FALSE;
                }
                else {
                    //	ID only appears once, so in case we're resetting,
                    //	don't fail if we don't see it.
                    DBGOUT(("WARNING: didn't see PC87108 id (0x5A); got %xh.",
                         (UINT)val));
                }
            }
        }

        if (CSMode)
        {
            // base address ignored.
            val = 0;
        }
        else
        {
            // Select the base address for the UART
            switch (thisDev->portInfo.ioBase){
            case 0x3E8:	    val = 0;	    break;
            case 0x2E8:	    val = 1;	    break;
            case 0x3F8:	    val = 2;	    break;
            case 0x2F8:	    val = 3;	    break;
            default:	    return FALSE;
            }
        }
	    val |= 0x04;	// enable register banks
	    val |= 0x10;	// Set the interrupt line to Totempole output.
        Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, BAIC_REG, val, CSMode);

	    //	 Select interrupt level according to base address,
	    //	 following COM port mapping.
	    //	 Also select MIR/FIR DMA channels for rcv and xmit.
	    //
	    switch (thisDev->portInfo.irq){
		case 3:     val = 1;	    break;
		case 4:     val = 2;	    break;
		case 5:     val = 3;	    break;
		case 7:     val = 4;	    break;
		case 9:     val = 5;	    break;
		case 11:    val = 6;	    break;
		case 15:    val = 7;	    break;
		default:    return FALSE;
	    }

	    switch (thisDev->portInfo.DMAChannel){
		case 0: 		    val |= 0x08;    break;
		case 1: 		    val |= 0x10;    break;
		case 3: 		    val |= 0x18;    break;
		default:
		    DBGERR(("Bad rcv dma channel in NSC_DEMO_Init"));
		    return FALSE;
	    }

	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, CSRT_REG, val, CSMode);

	    // Select device-enable and normal-operating-mode.
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, MCTL_REG, (UCHAR)3, CSMode);
	    break;

/*
	case PC87307:
	    //
	    //	Select Logical Device 5
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x7, 0x5);

	    // Disable IO check
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0x31,0x0);

	    // Config Base address low and high.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x61,(UCHAR)(thisDev->portInfo.ioBase));
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x60,(UCHAR)(thisDev->portInfo.ioBase >> 8));

	    // Set IRQ
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x70,(UCHAR)thisDev->portInfo.irq);
			
	    // Enable Bank Select
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0xF0,0x82);

	    // Enable UIR
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0x30,0x1);
	    break;

*/
	case PC87308:

	    //	Select Logical Device 5
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x7, 0x5, FALSE);
			
	    // Disable IO check
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0x31,0x0, FALSE);

	    // Config Base address low and high.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x61,(UCHAR)(thisDev->portInfo.ioBase), FALSE);
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x60,(UCHAR)(thisDev->portInfo.ioBase >> 8), FALSE);

	    // Set IRQ
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x70,(UCHAR)thisDev->portInfo.irq, FALSE);
			
	    // Select DMA Channel
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,
			     0x74,thisDev->portInfo.DMAChannel, FALSE);

	    // DeSelect TXDMA Channel
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0x75,0x4, FALSE);

	    // Enable Bank Select
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0xF0,0x82, FALSE);


	    // Enable UIR
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr,0x30,0x1, FALSE);
	    break;

	case PC87338:
	    // Select Plug and Play mode.
	    val = Ir108ConfigRead(thisDev->portInfo.ConfigIoBaseAddr, 0x1B, FALSE);
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x1B,
			     (UCHAR)(val | 0x08), FALSE);

	    // Write the new Plug and Play UART IOBASE register.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x46,
			     (UCHAR)((thisDev->portInfo.ioBase>>2) & 0xfe), FALSE);
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x47,
			     (UCHAR)((thisDev->portInfo.ioBase>>8) & 0xfc), FALSE);

	    // Enable 14 Mhz clock + Clk Multiplier
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x51, 0x04, FALSE);

	    // Get Interrup line and shift it four bits;
	    //
	    val = thisDev->portInfo.irq << 4;

	    // Read the Current Plug and Play Configuration 1 register.
	    //
	    val |= Ir108ConfigRead(thisDev->portInfo.ConfigIoBaseAddr,0x1C, FALSE);
		
	    // Write the New Plug and Play Configuration 1 register.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x1C, val, FALSE);
			
	    // Setup 338 DMA.
	    //
	    switch (thisDev->portInfo.DMAChannel){
		case 0: 		val = 0x01;	break;
		case 1: 		val = 0x02;	break;
		case 2: 		val = 0x03;	break;
		case 3:

		    // Read the Current Plug and Play Configuration 3 register.
		    //
		    val = Ir108ConfigRead(
				thisDev->portInfo.ConfigIoBaseAddr,0x50, FALSE) | 0x01;

		    // Write the new Plug and Play Configuration 3 register.
		    //
		    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x50,
				     val, FALSE);

		    // Read the Current Plug and Play Configuration 3 register.
		    //
		    val = Ir108ConfigRead(
			       thisDev->portInfo.ConfigIoBaseAddr,0x4C, FALSE) | 0x80;

		    // Write the new Plug and Play Configuration 3 register.
		    //
		    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x4C,
				     val, FALSE);
		    val = 0x04;
		    break;

		default:
		    DBGERR(("Bad rcv dma channel in NSC_DEMO_Init"));
		    return FALSE;
	    }

	    // Write the new Plug and Play Configuration 3 register.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x4F, val, FALSE);

	    // Read the Current SuperI/O Configuration Register 2 register.
	    //
	    val = Ir108ConfigRead(thisDev->portInfo.ConfigIoBaseAddr,0x40, FALSE);

	    // Set up UIR/UART2 for Normal Power Mode and Bank select enable.
	    //
	    val |= 0xE0;

	    // Write the New SuperI/O Configuration Register 2 register.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x40, val, FALSE);


	    // Read the Current SuperI/O Configuration Register 3 register.
	    //
	    val = Ir108ConfigRead(thisDev->portInfo.ConfigIoBaseAddr,0x50, FALSE);

	    // Set up UIR/UART2 IRX line
	    //
	    val |= 0x0C;

	    // Write the New SuperI/O Configuration Register 3 register.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x50, val, FALSE);
		
	    // Set the SIRQ1 int to DRQ3 ??? only for EB
	    //val = Ir108ConfigRead(thisDev->portInfo.ConfigIoBaseAddr,0x4c) & 0x3f;
	    //Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x4c, val | 0x80);


	    // Read the Current Function Enable register.
	    //
	    val = Ir108ConfigRead(thisDev->portInfo.ConfigIoBaseAddr,0x00, FALSE);

	    // Enable UIR/UART2.
	    //
	    val |= 0x04;

	    // Write the New Function Enable register.
	    //
	    Ir108ConfigWrite(thisDev->portInfo.ConfigIoBaseAddr, 0x00, val, FALSE);
	    break;

	// PCI configuration space settings for handling MIR, FIR transfers
	// through the legacy DMA.
#ifdef LEGACY
	case PNPFIR:	    // Use legacy DMA to handle MIR, FIR transfer

	    // turn on Bank register accessing...
	    PCIConfig560WriteWord(thisDev->pciCfgAddress, 0x5C, 0x0240);
	    // rcv and xmt are in the different DMA channel
	    PCIConfig560WriteWord(thisDev->pciCfgAddress, 0x64, 0x1000);

	    break;
#endif

    } // End of Evaluation board configuration setction.

    thisDev->UIR_Mid = NSC_ReadBankReg(thisDev->portInfo.ioBase, 3, 0);

    if (thisDev->UIR_Mid<0x20)
    {
        // Older revs of the NSC hardware seem to handle 1MB really poorly.
        thisDev->AllowedSpeedMask &= ~NDIS_IRDA_SPEED_1152K;
    }

    // The UART doesn't appear until we clear and set the FIFO control
    // register.

    NdisRawWritePortUchar(thisDev->portInfo.ioBase+2, (UCHAR)0x00);
    NdisRawWritePortUchar(thisDev->portInfo.ioBase+2, (UCHAR)0x07);

    // Set FIR CRC to 32 bits.
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 6, 0, 0x20);

    // Switch to bank 5
    // clear the status FIFO
    //
    NdisRawWritePortUchar(thisDev->portInfo.ioBase+3, (UCHAR)0xEC);
    FifoClear = 8;
    do {
	NdisRawReadPortUchar(thisDev->portInfo.ioBase+6, &val);
	NdisRawReadPortUchar(thisDev->portInfo.ioBase+7, &val);
	NdisRawReadPortUchar(thisDev->portInfo.ioBase+5, &val);
	FifoClear--;
    } while( (val & 0x80) && (FifoClear > 0) );

    // Test for newer silicon for support of Frame stop mode

#if 0
    if (thisDev->UIR_Mid < 0x16)
	// Change Bit 1 to Default 1
	//  0x40 -> 0x42
#endif
	NSC_WriteBankReg(thisDev->portInfo.ioBase, 5, 4, 0x40);
#if 0  // Since we're not currently using the multi-packet send, we don't use frame stop mode.
    else
	//
	// Set FIFO threshold and TX_MS Tx frame end stop mode.
	//
	// Change Bit 1 to Default 1
	//	0x68 -> 0x6a
	NSC_WriteBankReg(thisDev->portInfo.ioBase, 5, 4, 0x60);
#endif

    // Set SIR mode in IRCR1.
    // Enable SIR infrared mode in the Non-Extended mode of operation
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 2, 0x0C);

    // Set max xmit frame size.
    // Need to set value slightly larger so that counter never
    // reaches 0.
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 4,
			 (UCHAR)(MAX_NDIS_DATA_SIZE+1));
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 5,
			 (UCHAR)((MAX_NDIS_DATA_SIZE+1) >> 8));

    // Set max rcv frame size.
    // Need to set value slightly larger so that counter never
    // reaches 0.
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 6,
			 (UCHAR)(MAX_RCV_DATA_SIZE+FAST_IR_FCS_SIZE));
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 4, 7,
			 (UCHAR)((MAX_RCV_DATA_SIZE+FAST_IR_FCS_SIZE) >> 8));

    // Set extended mode
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 2, 2, 0x03);

    // Set 32-bit FIFOs
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 2, 4, 0x05);

    // Enable and reset FIFO's and set the receive FIF0
    // equal to the receive DMA threshold. See if DMA
    // is fast enough for device.
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 0, 2, 0x07);

    // Restore to Non-Extended mode
    //
    NSC_WriteBankReg(thisDev->portInfo.ioBase, 2, 2, 0x02);


    thisDev->portInfo.hwCaps.supportedSpeedsMask = NSC_DEMO_IRDA_SPEEDS;
    thisDev->portInfo.hwCaps.turnAroundTime_usec = DEFAULT_TURNAROUND_usec;
    thisDev->portInfo.hwCaps.extraBOFsRequired = 0;
		
    // Initialize thedongle structure before calling
    // GetDongleCapabilities and SetDongleCapabilities for dongle 1.
    //
    thisDev->currentDongle = 1;
    IrDongleResource.Signature =
			thisDev->DongleTypes[thisDev->currentDongle];
    IrDongleResource.ComPort = thisDev->portInfo.ioBase;
    IrDongleResource.ModeReq = SIR;
    IrDongleResource.XcvrNum = thisDev->currentDongle;

    IrDongle = GetDongleCapabilities(IrDongleResource);
		
    // Initialize thedongle structure before calling
    // GetDongleCapabilities and SetDongleCapabilities for dongle 0.
    //
    thisDev->currentDongle = 0;
    IrDongleResource.Signature = thisDev->DongleTypes[thisDev->currentDongle];
    IrDongleResource.ComPort = thisDev->portInfo.ioBase;
    IrDongleResource.ModeReq = SIR;
    IrDongleResource.XcvrNum = thisDev->currentDongle;

    IrDongle = GetDongleCapabilities(IrDongleResource);

    SetDongleCapabilities(IrDongleResource);
	
    return TRUE;
}


//////////////////////////////////////////////////////////////////////////
//									//
// Function:	NSC_DEMO_Deinit 					//
//									//
//  DUMMY ROUTINE							//
//////////////////////////////////////////////////////////////////////////

VOID NSC_DEMO_Deinit(UINT comBase, UINT context)
{
		
}

//////////////////////////////////////////////////////////////////////////
//									//
// Function:	NSC_DEMO_SetSpeed					//
//									//
// Description: 							//
//  Set up the size of FCB, the timer, FIFO, DMA and the IR mode/dongle //
//  speed based on the negotiated speed.				//
//									//
//////////////////////////////////////////////////////////////////////////

BOOLEAN NSC_DEMO_SetSpeed(
    IrDevice *thisDev,
    UINT comBase,
    UINT bitsPerSec,
    UINT context)
{
    NDIS_STATUS stat;
    UINT fcsSize;

    LOG("==>NSC_DEMO_SetSpeed",bitsPerSec);

    // Make sure the previous packet completely sent out(Not in the TX FIFO)
    // and Txmitter is empty
    // before the bandwidth control


    while((NSC_ReadBankReg(comBase, 0, 5)& 0x60) != 0x60);

    //

    if (bitsPerSec > 115200){

	fcsSize = (bitsPerSec >= MIN_FIR_SPEED) ?
		   FAST_IR_FCS_SIZE : MEDIUM_IR_FCS_SIZE;

	if(bitsPerSec >= MIN_FIR_SPEED)
	    IrDongleResource.ModeReq = FIR;
	else
	    IrDongleResource.ModeReq = MIR;
	SetDongleCapabilities(IrDongleResource);
	

	// Set extended mode and set DMA fairness.
	//
	NSC_WriteBankReg(comBase, 2, 2, 0x03);

	if (thisDev->UIR_Mid < 0x16){

	    //	Set Timer registers.
	    //
	    NSC_WriteBankReg(comBase, 4, 0, (UCHAR)0x2);
	    NSC_WriteBankReg(comBase, 4, 1, (UCHAR)0x0);
	}
	else {

	    //	Set Timer registers timer has 8 times finer
	    //	resolution.
	    //
	    NSC_WriteBankReg(comBase, 4, 0, (UCHAR)0xA);
	    NSC_WriteBankReg(comBase, 4, 1, (UCHAR)0x0);
	}

	// Set max rcv frame size.
	// Need to set value slightly larger so that counter never reaches 0.
	//
	DBGERR(("Programming Max Receive Size registers with %d Bytes ",
						 MAX_RCV_DATA_SIZE+fcsSize));
	NSC_WriteBankReg(comBase, 4, 6, (UCHAR)(MAX_RCV_DATA_SIZE+fcsSize));
	NSC_WriteBankReg(comBase, 4, 7,
				 (UCHAR)((MAX_RCV_DATA_SIZE+fcsSize) >> 8));


	// Reset Timer Enable bit.
	//
	NSC_WriteBankReg(comBase, 4, 2, 0x00);

	// Set MIR/FIR mode and DMA enable
	//
	NSC_WriteBankReg(comBase, 0, 4,
			 (UCHAR)((bitsPerSec >= 4000000) ? 0xA4 : 0x84));

	DBGERR(("EXCR2= 0x%x",NSC_ReadBankReg(thisDev->portInfo.ioBase, 2, 4)));
			
	// Set 32-bit FIFOs
	//
	NSC_WriteBankReg(comBase, 2, 4, 0x05);
	DBGERR(("EXCR2= 0x%x",NSC_ReadBankReg(thisDev->portInfo.ioBase, 2, 4)));

	//
	// We may start receiving immediately so setup the
	// receive DMA
	//

#if 1
	// First, tear down any existing DMA
	if (thisDev->AdapterState==ADAPTER_RX) {
	    NdisMCompleteDmaTransfer(&stat, thisDev->DmaHandle,
                                         thisDev->rcvDmaBuffer,
                                         (ULONG)thisDev->rcvDmaOffset,
                                         (ULONG)thisDev->rcvDmaSize, FALSE);
	}

    FindLargestSpace(thisDev, &thisDev->rcvDmaOffset, &thisDev->rcvDmaSize);

	SetupRecv(thisDev);

#else
	thisDev->rcvDmaOffset = 0;
	thisDev->rcvPktOffset = 0;
	thisDev->rcvDmaSize = RCV_DMA_SIZE
	NdisMSetupDmaTransfer(
			      &stat,
			      thisDev->DmaHandle,
			      thisDev->rcvDmaBuffer,
			      thisDev->rcvDmaOffset,
			      thisDev->rcvDmaSize, FALSE);

	if (stat != NDIS_STATUS_SUCCESS){
	    DBGERR(("NdisMSetupDmaTransfer failed in FIR_DeliverFrames (%xh)", (UINT)stat));
	}
#endif

	// Set the interrupt mask to interrupt on the
	// first packet received.
	//
	thisDev->IntMask = 0x04;
	DBGOUT(("RxDMA = ON"));
    }
    else {

	// Set SIR mode in UART before setting the timing of transciever
	//

	// Set SIR mode
	//
	NSC_WriteBankReg(comBase, 4, 2, 0x0C);

	// Must set SIR Pulse Width Register to 0 (3/16) as default
	// Bug in 338/108
	NSC_WriteBankReg(comBase, 6, 2, 0x0);

	// Clear extended mode
	//
	NSC_WriteBankReg(comBase, 2, 2, 0x00);


	IrDongleResource.ModeReq = SIR;
	SetDongleCapabilities(IrDongleResource);


	// Clear Line and Auxiluary status registers.
	//
	NSC_ReadBankReg(comBase, 0, 5);
	NSC_ReadBankReg(comBase, 0, 7);

    }
    LOG("<==NSC_DEMO_SetSpeed",0);
    return TRUE;
}

