/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    pci.h

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/


#ifndef _PCI_H
#define _PCI_H


//-------------------------------------------------------------------------
// PCI configuration hardware ports
//-------------------------------------------------------------------------
#define CF1_CONFIG_ADDR_REGISTER    0x0CF8
#define CF1_CONFIG_DATA_REGISTER    0x0CFC
#define CF2_SPACE_ENABLE_REGISTER   0x0CF8
#define CF2_FORWARD_REGISTER        0x0CFA
#define CF2_BASE_ADDRESS            0xC000



//-------------------------------------------------------------------------
// Configuration Space Header
//-------------------------------------------------------------------------
typedef struct _PCI_CONFIG_STRUC {
    USHORT  PciVendorId;        // PCI Vendor ID
    USHORT  PciDeviceId;        // PCI Device ID
    USHORT  PciCommand;
    USHORT  PciStatus;
    UCHAR   PciRevisionId;
    UCHAR   PciClassCode[3];
    UCHAR   PciCacheLineSize;
    UCHAR   PciLatencyTimer;
    UCHAR   PciHeaderType;
    UCHAR   PciBIST;
    ULONG   PciBaseReg0;
    ULONG   PciBaseReg1;
    ULONG   PciBaseReg2;
    ULONG   PciBaseReg3;
    ULONG   PciBaseReg4;
    ULONG   PciBaseReg5;
    ULONG   PciReserved0;
    ULONG   PciReserved1;
    ULONG   PciExpROMAddress;
    ULONG   PciReserved2;
    ULONG   PciReserved3;
    UCHAR   PciInterruptLine;
    UCHAR   PciInterruptPin;
    UCHAR   PciMinGnt;
    UCHAR   PciMaxLat;
} PCI_CONFIG_STRUC, *PPCI_CONFIG_STRUC;

//-------------------------------------------------------------------------
// PCI Class Code Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define PCI_BASE_CLASS      0x02    // Base Class - Network Controller
#define PCI_SUB_CLASS       0x00    // Sub Class - Ethernet Controller
#define PCI_PROG_INTERFACE  0x00    // Prog I/F - Ethernet COntroller

//-------------------------------------------------------------------------
// PCI Command Register Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define CMD_IO_SPACE            BIT_0
#define CMD_MEMORY_SPACE        BIT_1
#define CMD_BUS_MASTER          BIT_2
#define CMD_SPECIAL_CYCLES      BIT_3
#define CMD_MEM_WRT_INVALIDATE  BIT_4
#define CMD_VGA_PALLETTE_SNOOP  BIT_5
#define CMD_PARITY_RESPONSE     BIT_6
#define CMD_WAIT_CYCLE_CONTROL  BIT_7
#define CMD_SERR_ENABLE         BIT_8
#define CMD_BACK_TO_BACK        BIT_9

//-------------------------------------------------------------------------
// PCI Status Register Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define STAT_BACK_TO_BACK           BIT_7
#define STAT_DATA_PARITY            BIT_8
#define STAT_DEVSEL_TIMING          BIT_9 OR BIT_10
#define STAT_SIGNAL_TARGET_ABORT    BIT_11
#define STAT_RCV_TARGET_ABORT       BIT_12
#define STAT_RCV_MASTER_ABORT       BIT_13
#define STAT_SIGNAL_MASTER_ABORT    BIT_14
#define STAT_DETECT_PARITY_ERROR    BIT_15

//-------------------------------------------------------------------------
// PCI Base Address Register For Memory (BARM) Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define BARM_LOCATE_BELOW_1_MEG     BIT_1
#define BARM_LOCATE_IN_64_SPACE     BIT_2
#define BARM_PREFETCHABLE           BIT_3

//-------------------------------------------------------------------------
// PCI Base Address Register For I/O (BARIO) Bit Definitions
// Configuration Space Header
//-------------------------------------------------------------------------
#define BARIO_SPACE_INDICATOR       BIT_0

//-------------------------------------------------------------------------
// PCI BIOS Definitions
// Refer To The PCI BIOS Specification
//-------------------------------------------------------------------------
//- Function Code List
#define PCI_FUNCTION_ID         0xB1    // AH Register
#define PCI_BIOS_PRESENT        0x01    // AL Register
#define FIND_PCI_DEVICE         0x02    // AL Register
#define FIND_PCI_CLASS_CODE     0x03    // AL Register
#define GENERATE_SPECIAL_CYCLE  0x06    // AL Register
#define READ_CONFIG_BYTE        0x08    // AL Register
#define READ_CONFIG_WORD        0x09    // AL Register
#define READ_CONFIG_DWORD       0x0A    // AL Register
#define WRITE_CONFIG_BYTE       0x0B    // AL Register
#define WRITE_CONFIG_WORD       0x0C    // AL Register
#define WRITE_CONFIG_DWORD      0x0D    // AL Register

//- Function Return Code List
#define SUCCESSFUL              0x00
#define FUNC_NOT_SUPPORTED      0x81
#define BAD_VENDOR_ID           0x83
#define DEVICE_NOT_FOUND        0x86
#define BAD_REGISTER_NUMBER     0x87

//- PCI BIOS Calls
#define PCI_BIOS_INTERRUPT      0x1A        // PCI BIOS Int 1Ah Function Call
#define PCI_PRESENT_CODE        0x20494350  // Hex Equivalent Of 'PCI '

#define PCI_SERVICE_IDENTIFIER  0x49435024  // ASCII Codes for 'ICP$'

//- Device and Vendor IDs
#define D100_DEVICE_ID          0x1229
#define D100_VENDOR_ID          0x8086

#endif      // PCI_H

