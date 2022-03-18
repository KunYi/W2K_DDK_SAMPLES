/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    inlinef.h

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

//-----------------------------------------------------------------------------
// Procedure:   WaitScb
//
// Description: This routine checks to see if the D100 has accepted a command.
//              It does so by checking the command field in the SCB, which will
//              be zeroed by the D100 upon accepting a command.  The loop waits
//              for up to 600 milliseconds for command acceptance.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE if the SCB cleared within 600 milliseconds.
//      FALSE if it didn't clear within 600 milliseconds
//-----------------------------------------------------------------------------
__inline
BOOLEAN
WaitScb(
        IN PD100_ADAPTER Adapter
)

{
    UINT        wait_count = 60000;

    DEBUGFUNC("WaitScb");
    do
    {
        if (!Adapter->CSRAddress->ScbCommandLow)
            return TRUE;

        NdisStallExecution(10);
    } while (wait_count--);

    // If we fall through, we have a problem.
    HARDWARE_NOT_RESPONDING (Adapter);

        return FALSE;
}

//-----------------------------------------------------------------------------
// Procedure:   D100DisableInterrupt
//
// Description: This routine disables interrupts at the hardware, by setting
//              the M (mask) bit in the adapter's CSR SCB command word.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
__inline
VOID
D100DisableInterrupt(
                     IN PD100_ADAPTER Adapter
)

{
    DEBUGFUNC(("D100DisableInterrupt"));
    TRACE2(Adapter, ("Entered D100DisableInterrupt\n"));

    DEBUGCHAR(Adapter,'\\');
    // Disable interrupts on our PCI board by setting the mask bit
    Adapter->CSRAddress->ScbCommandHigh = SCB_INT_MASK;
}






//-----------------------------------------------------------------------------
// Procedure:   D100EnableInterrupt
//
// Description: This routine enables interrupts at the hardware, by resetting
//              the M (mask) bit in the adapter's CSR SCB command word
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
__inline
VOID
D100EnableInterrupt(
                    IN PD100_ADAPTER Adapter
)

{
    DEBUGFUNC("D100EnableInterrupt");
    TRACE2(Adapter, ("Entered D100EnableInterrupt\n"));

    DEBUGCHAR(Adapter,'/');

    // Enable interrupts on our PCI board by clearing the mask bit

    Adapter->CSRAddress->ScbCommandHigh = 0;
}

//-----------------------------------------------------------------------------
// Procedure:   D100StallExecution
//
// Description: This routine cause a delay for the number of milliseconds that
//              it is passed.
//
// Arguments:
//      MsecDelay - How many milliseconds to delay for
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
__inline
VOID
D100StallExecution(
                   IN UINT MsecDelay
)

{
    // Delay in 100 usec increments
    MsecDelay *= 10;
    while (MsecDelay)
    {
        NdisStallExecution(100);
                MsecDelay--;
    }

}


//-----------------------------------------------------------------------------
// Procedure:   D100IssueScbCommand
//
// Description: This general routine will issue a command to the D100.
//
// Arguments:
//      Adapter - ptr to Adapter object instance.
//      ScbCommand - The command that is to be issued
//      WaitForSCB - A boolean value indicating whether or not a wait for SCB
//                   must be done before the command is issued to the chip
//
// Returns:
//      TRUE if the command was issued to the chip successfully
//      FALSE if the command was not issued to the chip
//-----------------------------------------------------------------------------
__inline
BOOLEAN
D100IssueScbCommand(
                    IN PD100_ADAPTER Adapter,
                    IN UCHAR ScbCommandLow,
                    IN BOOLEAN WaitForScb
)

{
    DEBUGFUNC("D100IssueScbCommand");
    TRACE2(Adapter, ("Entered D100IssueScbCommand\n"));

    if (WaitForScb == TRUE)
    {
        if (WaitScb(Adapter) != TRUE)
        {
            TRACESTR(Adapter, ("First D100WaitScb failed\n"));

            D100LogError(Adapter, EVENT_4, NDIS_ERROR_CODE_TIMEOUT, 0);

            return (FALSE);
        }
    }

    Adapter->CSRAddress->ScbCommandLow = ScbCommandLow;

    return (TRUE);
}


