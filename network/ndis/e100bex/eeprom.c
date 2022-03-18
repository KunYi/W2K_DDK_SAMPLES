/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    eeprom.c

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#include "precomp.h"
#pragma hdrstop
#pragma warning (disable: 4244 4514)

//*****************************************************************************
//
//            I/O based Read EEPROM Routines
//
//*****************************************************************************

//-----------------------------------------------------------------------------
// Procedure:   ReadEEprom
//
// Description: This routine serially reads one word out of the EEPROM.
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      Reg - EEPROM word to read.
//
// Returns:
//      Contents of EEPROM word (Reg).
//-----------------------------------------------------------------------------

USHORT
ReadEEprom(
           IN PD100_ADAPTER Adapter,
           IN USHORT Reg,
           IN ULONG CSRBaseIoAddress
           )

{
    USHORT x;
    USHORT data;

    DEBUGFUNC("ReadEEprom");
    INITSTR(("\n"));

    // select EEPROM, reset bits, set EECS
    D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),
        (char *)&x);

    x &= ~(EEDI | EEDO | EESK);
    x |= EECS;
    D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);

    // write the read opcode and register number in that order
    // The opcode is 3bits in length, reg is 6 bits long
    ShiftOutBits(Adapter, EEPROM_READ_OPCODE, 3, CSRBaseIoAddress);
    ShiftOutBits(Adapter, Reg, 6, CSRBaseIoAddress);

    // Now read the data (16 bits) in from the selected EEPROM word
    data = ShiftInBits(Adapter, CSRBaseIoAddress);

    EEpromCleanup(Adapter, CSRBaseIoAddress);
    return data;
}

//-----------------------------------------------------------------------------
// Procedure:   ShiftOutBits
//
// Description: This routine shifts data bits out to the EEPROM.
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      data - data to send to the EEPROM.
//      count - number of data bits to shift out.
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
ShiftOutBits(
             IN PD100_ADAPTER Adapter,
             IN USHORT data,
             IN USHORT count,
             IN ULONG CSRBaseIoAddress
             )

{
    USHORT x,mask;

    mask = 0x01 << (count - 1);
    D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),
        (char *)&x);
    x &= ~(EEDO | EEDI);

    do
    {
        x &= ~EEDI;
        if (data & mask)
            x |= EEDI;

        D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),
            x);
        NdisStallExecution(100);
        RaiseClock(Adapter, &x, CSRBaseIoAddress);
        LowerClock(Adapter, &x, CSRBaseIoAddress);
        mask = mask >> 1;
    } while (mask);

    x &= ~EEDI;
    D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);
}


//-----------------------------------------------------------------------------
// Procedure:   RaiseClock
//
// Description: This routine raises the EEPOM's clock input (EESK)
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      x - Ptr to the EEPROM control register's current value
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
RaiseClock(
           IN PD100_ADAPTER Adapter,
           IN OUT USHORT *x,
           IN ULONG CSRBaseIoAddress
           )

{
    *x = *x | EESK;
    D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), *x);
    NdisStallExecution(100);
}


//-----------------------------------------------------------------------------
// Procedure:   LowerClock
//
// Description: This routine lower's the EEPOM's clock input (EESK)
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      x - Ptr to the EEPROM control register's current value
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
LowerClock(
           IN PD100_ADAPTER Adapter,
           IN OUT USHORT *x,
           IN ULONG CSRBaseIoAddress
           )

{
    *x = *x & ~EESK;
    D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), *x);
    NdisStallExecution(100);
}


//-----------------------------------------------------------------------------
// Procedure:   ShiftInBits
//
// Description: This routine shifts data bits in from the EEPROM.
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//
// Returns:
//      The contents of that particular EEPROM word
//-----------------------------------------------------------------------------

USHORT
ShiftInBits(
            IN PD100_ADAPTER Adapter,
            IN ULONG CSRBaseIoAddress
            )

{
    USHORT x,d,i;
    D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),
        (char *)&x);
    x &= ~( EEDO | EEDI);
    d = 0;

    for (i=0; i<16; i++)
    {
        d = d << 1;
        RaiseClock(Adapter, &x, CSRBaseIoAddress);

        D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),
            (char *)&x);

        x &= ~(EEDI);
        if (x & EEDO)
            d |= 1;

        LowerClock(Adapter, &x, CSRBaseIoAddress);
    }

    return d;
}


//-----------------------------------------------------------------------------
// Procedure:   EEpromCleanup
//
// Description: This routine returns the EEPROM to an idle state
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
EEpromCleanup(
              IN PD100_ADAPTER Adapter,
              IN ULONG CSRBaseIoAddress
              )

{
    USHORT x;
    D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),
        (char *)&x);

    x &= ~(EECS | EEDI);
    D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);

    RaiseClock(Adapter, &x, CSRBaseIoAddress);
    LowerClock(Adapter, &x, CSRBaseIoAddress);
}

//*****************************************************************************
//
//            Memory Read EEPROM Routines
//
//*****************************************************************************

//-----------------------------------------------------------------------------
// Procedure:   MemReadEEprom
//
// Description: This routine serially reads one word out of the EEPROM.
//              The EEPROM control registers are accessed via virtual
//              memory address, so this function can ONLY be used after the
//              CSRs physical address has been vitually mapped.
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      Reg - EEPROM word to read.
//
// Returns:
//      Contents of EEPROM word (Reg).
//-----------------------------------------------------------------------------
USHORT
MemReadEEprom(
              IN PD100_ADAPTER Adapter,
              IN USHORT Reg,
              IN PCSR_STRUC CSRVirtAddress
              )

{
    USHORT x;
    USHORT data;

    DEBUGFUNC("ReadEEprom");
    INITSTR(("\n"));

    // select EEPROM, reset bits, set EECS
    // D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), (char *)&x);
    x = CSRVirtAddress->EepromControl;

    x &= ~(EEDI | EEDO | EESK);
    x |= EECS;

    // D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);
    CSRVirtAddress->EepromControl = x;

    // write the read opcode and register number in that order
    // The opcode is 3bits in length, reg is 6 bits long
    MemShiftOutBits(Adapter, EEPROM_READ_OPCODE, 3, CSRVirtAddress);
    MemShiftOutBits(Adapter, Reg, 6, CSRVirtAddress);

    // Now read the data (16 bits) in from the selected EEPROM word
    data = MemShiftInBits(Adapter, CSRVirtAddress);

    MemEEpromCleanup(Adapter, CSRVirtAddress);
    return data;
}

//-----------------------------------------------------------------------------
// Procedure:   MemShiftOutBits
//
// Description: This routine shifts data bits out to the EEPROM.
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      data - data to send to the EEPROM.
//      count - number of data bits to shift out.
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
MemShiftOutBits(
                IN PD100_ADAPTER Adapter,
                IN USHORT data,
                IN USHORT count,
                IN PCSR_STRUC CSRVirtAddress
                )

{
    USHORT x,mask;

    mask = 0x01 << (count - 1);
    // D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),(char *)&x);
    x = CSRVirtAddress->EepromControl;
    x &= ~(EEDO | EEDI);

    do
    {
        x &= ~EEDI;
        if (data & mask)
            x |= EEDI;

        // D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);
        CSRVirtAddress->EepromControl = x;

        NdisStallExecution(100);
        MemRaiseClock(Adapter, &x, CSRVirtAddress);
        MemLowerClock(Adapter, &x, CSRVirtAddress);
        mask = mask >> 1;
    } while (mask);

    x &= ~EEDI;

    // D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);
    CSRVirtAddress->EepromControl = x;
}


//-----------------------------------------------------------------------------
// Procedure:   MemRaiseClock
//
// Description: This routine raises the EEPOM's clock input (EESK)
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      x - Ptr to the EEPROM control register's current value
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
MemRaiseClock(
              IN PD100_ADAPTER Adapter,
              IN OUT USHORT *x,
              IN PCSR_STRUC CSRVirtAddress
              )

{
    *x = *x | EESK;

    // D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), *x);
    CSRVirtAddress->EepromControl = *x;

    NdisStallExecution(100);
}


//-----------------------------------------------------------------------------
// Procedure:   MemLowerClock
//
// Description: This routine lower's the EEPOM's clock input (EESK)
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//      x - Ptr to the EEPROM control register's current value
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
MemLowerClock(
              IN PD100_ADAPTER Adapter,
              IN OUT USHORT *x,
              IN PCSR_STRUC CSRVirtAddress
              )

{
    *x = *x & ~EESK;

    //D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), *x);
    CSRVirtAddress->EepromControl = *x;

    NdisStallExecution(100);
}


//-----------------------------------------------------------------------------
// Procedure:   MemShiftInBits
//
// Description: This routine shifts data bits in from the EEPROM.
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//
// Returns:
//      The contents of that particular EEPROM word
//-----------------------------------------------------------------------------

USHORT
MemShiftInBits(
               IN PD100_ADAPTER Adapter,
               IN PCSR_STRUC CSRVirtAddress
               )

{
    USHORT x,d,i;

    // D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), (char *)&x);
    x = CSRVirtAddress->EepromControl;

    x &= ~( EEDO | EEDI);
    d = 0;

    for (i=0; i<16; i++)
    {
        d = d << 1;
        MemRaiseClock(Adapter, &x, CSRVirtAddress);

        // D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG),(char *)&x);
        x = CSRVirtAddress->EepromControl;

        x &= ~(EEDI);
        if (x & EEDO)
            d |= 1;

        MemLowerClock(Adapter, &x, CSRVirtAddress);
    }

    return d;
}


//-----------------------------------------------------------------------------
// Procedure:   MemEEpromCleanup
//
// Description: This routine returns the EEPROM to an idle state
//
// Arguments:
//      Adapter - Ptr to this card's adapter data structure
//
// Returns: (none)
//-----------------------------------------------------------------------------

VOID
MemEEpromCleanup(
                 IN PD100_ADAPTER Adapter,
                 IN PCSR_STRUC CSRVirtAddress
                 )

{
    USHORT x;

    // D100_READ_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), (char *)&x);
    x = CSRVirtAddress->EepromControl;

    x &= ~(EECS | EEDI);
    // D100_WRITE_UCHAR((CSRBaseIoAddress + CSR_EEPROM_CONTROL_REG), x);
    CSRVirtAddress->EepromControl = x;

    MemRaiseClock(Adapter, &x, CSRVirtAddress);
    MemLowerClock(Adapter, &x, CSRVirtAddress);
}


