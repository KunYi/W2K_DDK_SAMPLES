/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    routines.c

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
#pragma warning (disable: 4514 4706)

//-----------------------------------------------------------------------------
// Procedure:   MdiWrite
//
// Description: This routine will write a value to the specified MII register
//              of an external MDI compliant device (e.g. PHY 100).  The
//              command will execute in polled mode.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      RegAddress - The MII register that we are writing to
//      PhyAddress - The MDI address of the Phy component.
//      DataValue - The value that we are writing to the MII register.
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
VOID
MdiWrite(
         IN PD100_ADAPTER Adapter,
         IN ULONG RegAddress,
         IN ULONG PhyAddress,
         IN USHORT DataValue
         )

{
    UINT    counter;
    DEBUGFUNC("MdiWrite");
    TRACE3(Adapter, ("\n"));

    // Issue the write command to the MDI control register.
    Adapter->CSRAddress->MDIControl = (((ULONG) DataValue) |
        (RegAddress << 16) |
        (PhyAddress << 21) |
        (MDI_WRITE << 26));

    // wait 20usec before checking status
    NdisStallExecution(20);

    // poll for the mdi write to complete
    for (counter = 100000; counter != 0; counter--)
    {
        if (Adapter->CSRAddress->MDIControl & MDI_PHY_READY)
            break;
        NdisStallExecution(20);
    }
    if (!counter)
    {
        HARDWARE_NOT_RESPONDING (Adapter);
    }
}


//-----------------------------------------------------------------------------
// Procedure:   MdiRead
//
// Description: This routine will read a value from the specified MII register
//              of an external MDI compliant device (e.g. PHY 100), and return
//              it to the calling routine.  The command will execute in polled
//              mode.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      RegAddress - The MII register that we are reading from
//      PhyAddress - The MDI address of the Phy component.
//
// Results:
//      DataValue - The value that we read from the MII register.
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
VOID
MdiRead(
        IN PD100_ADAPTER Adapter,
        IN ULONG RegAddress,
        IN ULONG PhyAddress,
        IN OUT PUSHORT DataValue
        )

{
    UINT    counter;
    DEBUGFUNC("MdiRead");
    TRACE3(Adapter, ("\n"));


    // Issue the read command to the MDI control register.
    Adapter->CSRAddress->MDIControl = ((RegAddress << 16) |
        (PhyAddress << 21) |
        (MDI_READ << 26));

    // wait 20usec before checking status
    NdisStallExecution(20);

    // poll for the mdi read to complete
    for (counter = 100000; counter != 0; counter--)
    {
        if (Adapter->CSRAddress->MDIControl & MDI_PHY_READY)
            break;
        NdisStallExecution(20);
    }
    if (!counter)
    {
        HARDWARE_NOT_RESPONDING (Adapter);
        return;
    }

    *DataValue = (USHORT) Adapter->CSRAddress->MDIControl;
}


//-----------------------------------------------------------------------------
// Procedure:   DumpStatsCounters
//
// Description: This routine will dump and reset the 82557's internal
//              Statistics counters.  The current stats dump values will be
//              added to the "Adapter's" overall statistics.
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
VOID
DumpStatsCounters(
                  IN PD100_ADAPTER Adapter
                  )

{
    UINT i;
    DEBUGFUNC("DumpStatsCounters");

    TRACE3(Adapter, ("\n"));
    DEBUGCHAR(Adapter,'D');
    // The query is for a driver statistic, so we need to first
    // update our statistics in software.

    // clear the dump counters complete DWORD
    Adapter->StatsCounters->CommandComplete = 0;

    // Acquire a spinlock here because this is a static command


    // Dump and reset the hardware's statistic counters
    D100IssueScbCommand(Adapter, SCB_CUC_DUMP_RST_STAT, TRUE);

    // Restore the resume transmit software flag.  After the dump counters
    // command is issued, we should do a WaitSCB before issuing the next send.
    Adapter->ResumeWait = TRUE;

    // Release SCB spinlock


    // Now wait for the dump/reset to complete
    for (i=100000; i != 0; i--)
    {
        if (Adapter->StatsCounters->CommandComplete == 0xA007)
            break;
        NdisStallExecution(20);
    }
    if (!i)
    {
        HARDWARE_NOT_RESPONDING (Adapter);
        return;
    }

    // Output the debug counters to the debug terminal.
    STATS(Adapter, ("Good Transmits %d\n", Adapter->StatsCounters->XmtGoodFrames));
    STATS(Adapter, ("Good Receives %d\n", Adapter->StatsCounters->RcvGoodFrames));
    STATS(Adapter, ("Max Collisions %d\n", Adapter->StatsCounters->XmtMaxCollisions));
    STATS(Adapter, ("Late Collisions %d\n", Adapter->StatsCounters->XmtLateCollisions));
    STATS(Adapter, ("Transmit Underruns %d\n", Adapter->StatsCounters->XmtUnderruns));
    STATS(Adapter, ("Transmit Lost CRS %d\n", Adapter->StatsCounters->XmtLostCRS));
    STATS(Adapter, ("Transmits Deferred %d\n", Adapter->StatsCounters->XmtDeferred));
    STATS(Adapter, ("One Collision xmits %d\n", Adapter->StatsCounters->XmtSingleCollision));
    STATS(Adapter, ("Mult Collision xmits %d\n", Adapter->StatsCounters->XmtMultCollisions));
    STATS(Adapter, ("Total Collisions %d\n", Adapter->StatsCounters->XmtTotalCollisions));

    STATS(Adapter, ("Receive CRC errors %d\n", Adapter->StatsCounters->RcvCrcErrors));
    STATS(Adapter, ("Receive Alignment errors %d\n", Adapter->StatsCounters->RcvAlignmentErrors));
    STATS(Adapter, ("Receive no resources %d\n", Adapter->StatsCounters->RcvResourceErrors));
    STATS(Adapter, ("Receive overrun errors %d\n", Adapter->StatsCounters->RcvOverrunErrors));
    STATS(Adapter, ("Receive CDT errors %d\n", Adapter->StatsCounters->RcvCdtErrors));
    STATS(Adapter, ("Receive short frames %d\n", Adapter->StatsCounters->RcvShortFrames));

    // update packet counts
    Adapter->GoodTransmits += Adapter->StatsCounters->XmtGoodFrames;
    Adapter->GoodReceives += Adapter->StatsCounters->RcvGoodFrames;

    // update transmit error counts
    Adapter->TxAbortExcessCollisions += Adapter->StatsCounters->XmtMaxCollisions;
    Adapter->TxLateCollisions += Adapter->StatsCounters->XmtLateCollisions;
    Adapter->TxDmaUnderrun += Adapter->StatsCounters->XmtUnderruns;
    Adapter->TxLostCRS += Adapter->StatsCounters->XmtLostCRS;
    Adapter->TxOKButDeferred += Adapter->StatsCounters->XmtDeferred;
    Adapter->OneRetry += Adapter->StatsCounters->XmtSingleCollision;
    Adapter->MoreThanOneRetry += Adapter->StatsCounters->XmtMultCollisions;
    Adapter->TotalRetries += Adapter->StatsCounters->XmtTotalCollisions;

    // update receive error counts
    Adapter->RcvCrcErrors += Adapter->StatsCounters->RcvCrcErrors;
    Adapter->RcvAlignmentErrors += Adapter->StatsCounters->RcvAlignmentErrors;
    Adapter->RcvResourceErrors += Adapter->StatsCounters->RcvResourceErrors;
    Adapter->RcvDmaOverrunErrors += Adapter->StatsCounters->RcvOverrunErrors;
    Adapter->RcvCdtFrames += Adapter->StatsCounters->RcvCdtErrors;
    Adapter->RcvRuntErrors += Adapter->StatsCounters->RcvShortFrames;
}


//-----------------------------------------------------------------------------
// Procedure:   DoBogusMulticast
//
// Description: This routine will issue a multicast command to adapter.  If the
//              Adapter's receive unit is "locked-up", a multicast command will
//              will reset and "un-lock" the receive unit.  The multicast
//              command that is issued here will be chained into the transmit
//              list.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------
VOID
DoBogusMulticast(
                 IN PD100_ADAPTER Adapter
                 )

{
    PD100SwTcb          SwTcb;
    PUCHAR              McAddress;
    UINT                i, j;
#if DBG
    USHORT x;
#endif

    DEBUGFUNC("DoBogusMulticast");
    TRACE3(Adapter, ("\n"));

    DEBUGCHAR(Adapter,'B');
    // Attempt to acquire a Software TCB for the packet

    SwTcb = (PD100SwTcb) QueuePopHead(&Adapter->TxCBList);

    if (SwTcb)
    {

        // Setup the pointer to the first MC Address
        McAddress = (PUCHAR) &SwTcb->Tcb->TxCbTbdPointer;
        McAddress += 2;

// the cast on an l-value is wierd, but needed because we are using
// a transmit structure to build a multicast add command
#pragma warning (disable: 4213)

        // Setup the multicast address count
        (USHORT) SwTcb->Tcb->TxCbTbdPointer =
            (USHORT) (Adapter->NumberOfMcAddresses * ETH_LENGTH_OF_ADDRESS);

#pragma warning (default: 4213)

        // Copy the current multicast addresses to the command block
        for (i = 0;(i < Adapter->NumberOfMcAddresses) &&
            (i < MAX_MULTICAST_ADDRESSES); i++)
            for (j = 0; j < ETH_LENGTH_OF_ADDRESS; j++)
                *(McAddress++) = Adapter->PrivateMulticastBuffer[i][j];

            // Setup the command and status words of the command block
            SwTcb->Tcb->TxCbHeader.CbStatus = 0;
            SwTcb->Tcb->TxCbHeader.CbCommand = CB_S_BIT | CB_MULTICAST;

            // If CU is idle (very first command) then we must
            // setup the general pointer and issue a full CU-start
            if (Adapter->TransmitIdle)
            {
                TRACE2(Adapter, ("CU is idle -- First MC added to Active List\n"));

                QueuePushHead(&Adapter->ActiveChainList, &SwTcb->Link);

                // Wait for the SCB to clear before we set the general pointer
                WaitScb(Adapter);

                // Don't try to start the transmitter if the command unit is not
                // idle ((not idle) == (Cu-Suspended or Cu-Active)).
                if ((Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) != SCB_CUS_IDLE)
                {
                    TRACESTR(Adapter, ("CU Not IDLE\n"));
                    ASSERT(0);
                    NdisStallExecution(25);     // hack to wait 25us
                }

                Adapter->CSRAddress->ScbGeneralPointer = SwTcb->TcbPhys;

                D100IssueScbCommand(Adapter, SCB_CUC_START, FALSE);

                Adapter->TransmitIdle = FALSE;
                Adapter->ResumeWait = TRUE;


            }

            // If the CU has already been started, then append this
            // TCB onto the end of the transmit chain, and issue a CU-Resume.
            else
            {
                TRACE2(Adapter, ("adding MCB to Active chain\n"));
                QueuePutTail(&Adapter->ActiveChainList, &SwTcb->Link);

                // Clear the suspend bit on the previous packet.
                SwTcb->PreviousTcb->TxCbHeader.CbCommand &= ~CB_S_BIT;

                // We need to wait for the SCB to clear when inserting a non-
                // transmit command dynamically into the CBL (command block list).
                Adapter->ResumeWait = TRUE;

                // Issue a CU-Resume command to the device -- and wait for the
                // SCB command word to clear first.
                if (!D100IssueScbCommand(Adapter, SCB_CUC_RESUME, TRUE))
                {
                    TRACESTR(Adapter, ("CU-resume failed\n"));
                }

            }
    }
    DEBUGCHAR(Adapter,'b');

}


//-----------------------------------------------------------------------------
// Procedure:   D100IssueSelectiveReset
//
// Description: This routine will issue a selective reset, forcing the adapter
//              the CU and RU back into their idle states.  The receive unit
//              will then be re-enabled if it was previously enabled, because
//              an RNR interrupt will be generated when we abort the RU.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------

VOID
D100IssueSelectiveReset(
                        PD100_ADAPTER Adapter
                        )

{
    //    BOOLEAN     EnableInts = TRUE;
#if DBG
    UINT counter=0;
    USHORT x;
#endif
    UINT i;
    DEBUGFUNC("D100IssueSelectiveReset");
    TRACE2(Adapter, ("Entered D100IssueSelectiveReset\n"));

    INITSTR(("\n"));
    // Wait for the SCB to clear before we check the CU status.
    WaitScb(Adapter);

    // If we have issued any transmits, then the CU will either be active, or
    // in the suspended state.  If the CU is active, then we wait for it to be
    // suspended.  If the the CU is suspended, then we need to put the CU back
    // into the idle state by issuing a selective reset.
    if (Adapter->TransmitIdle == FALSE)
    {
        // Wait for suspended state
        for (i=1000; (i != 0) && (Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) == SCB_CUS_ACTIVE; i--)
            //        while ((Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) == SCB_CUS_ACTIVE)
        {
            TRACESTR(Adapter, ("CU active -- wait for it to suspend. ScbStatus=%04x\n",
                Adapter->CSRAddress->ScbStatus));
            NdisStallExecution(20);
        }
        if (!i)
        {
            HARDWARE_NOT_RESPONDING (Adapter);
            return;
        }

        // Check the current status of the receive unit
        if ((Adapter->CSRAddress->ScbStatus & SCB_RUS_MASK) != SCB_RUS_IDLE)
        {
            // Issue an RU abort.  Since an interrupt will be issued, the
            // RU will be started by the DPC.
            D100IssueScbCommand(Adapter, SCB_RUC_ABORT, TRUE);
        }

        // Issue a selective reset.
        TRACESTR(Adapter, ("CU suspended. ScbStatus=%04x Issue selective reset\n",
            Adapter->CSRAddress->ScbStatus));
        Adapter->CSRAddress->Port = PORT_SELECTIVE_RESET;

        // stall 20 us (only need 10) after a port sel-reset command
        NdisStallExecution(20);

        for (i=100; i != 0; i--)
        {
            if (Adapter->CSRAddress->Port == 0)
                break;
            NdisStallExecution(10);
        }
        if (!i)
        {
            HARDWARE_NOT_RESPONDING (Adapter);
            return;
        }

        // disable interrupts after issuing reset, because the int
        // line gets raised when reset completes.
        D100DisableInterrupt(Adapter);

        // Restore the transmit software flags.
        Adapter->TransmitIdle = TRUE;
        Adapter->ResumeWait = TRUE;

    }
}


//-----------------------------------------------------------------------------
// Procedure:   D100SubmitCommandBlockAndWait
//
// Description: This routine will submit a command block to be executed, and
//              then it will wait for that command block to be executed.  Since
//              board ints will be disabled, we will ack the interrupt in
//              this routine.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      TRUE - If we successfully submitted and completed the command.
//      FALSE - If we didn't successfully submit and complete the command.
//-----------------------------------------------------------------------------

BOOLEAN
D100SubmitCommandBlockAndWait(
                              IN PD100_ADAPTER Adapter
                              )

{
    UINT            Delay;
    BOOLEAN         Status;

    // Points to the Non Tx Command Block.
    volatile PNON_TRANSMIT_CB CommandBlock = Adapter->NonTxCmdBlock;

    DEBUGFUNC("D100SubmitCommandBlockAndWait");
    TRACE2(Adapter, ("Entered D100SubmitCommandBlockAndWait\n"));

    // Set the Command Block to be the last command block
    CommandBlock->NonTxCb.Config.ConfigCBHeader.CbCommand |= CB_EL_BIT;

    // Clear the status of the command block
    CommandBlock->NonTxCb.Config.ConfigCBHeader.CbStatus = 0;

#if DBG
    // Don't try to start the CU if the command unit is active.
    if ((Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) == SCB_CUS_ACTIVE)
    {
        TRACESTR(Adapter, ("Scb %08x ScbStatus %04x\n", Adapter->CSRAddress, Adapter->CSRAddress->ScbStatus));

        ASSERT(0);

        return (FALSE);
    }
#endif


    // Start the command unit.
    D100IssueScbCommand(Adapter, SCB_CUC_START, FALSE);

    // Wait for the SCB to clear, indicating the completion of the command.
    if (WaitScb(Adapter) == FALSE)
    {
        TRACESTR(Adapter, ("WaitScb failed\n"));

        D100LogError(Adapter, EVENT_6, NDIS_ERROR_CODE_TIMEOUT, 0);

        return (FALSE);
    }

    // Wait for some status
    Delay = 300000;
    while ((!(CommandBlock->NonTxCb.Config.ConfigCBHeader.CbStatus & CB_STATUS_COMPLETE)) && Delay)
    {
        NdisStallExecution(10);
        Delay--;
    }

    if (!Delay)
    {
        HARDWARE_NOT_RESPONDING (Adapter);
        return (FALSE);
    }

    // Ack any interrupts
    if (Adapter->CSRAddress->ScbStatus & SCB_ACK_MASK)
    {
        // Ack all pending interrupts now
        Adapter->CSRAddress->ScbStatus &= SCB_ACK_MASK;
    }

    // Check the status of the command, and if the command failed return FALSE,
    // otherwise return TRUE.
    if (!(CommandBlock->NonTxCb.Config.ConfigCBHeader.CbStatus & CB_STATUS_OK))
    {
        TRACESTR(Adapter, ("Command failed\n"));
        Status = FALSE;
    }
    else
        Status = TRUE;

    return (Status);
}

//-----------------------------------------------------------------------------
// Procedure: GetConnectionStatus
//
// Description: This function returns the connection status that is
//              a required indication for PC 97 specification from MS
//              the value we are looking for is if there is link to the
//              wire or not.
//
// Arguments: IN Adapter structure pointer
//
// Returns:   NdisMediaStateConnected
//            NdisMediaStateDisconnected
//-----------------------------------------------------------------------------
NDIS_MEDIA_STATE
GetConnectionStatus( IN PD100_ADAPTER Adapter )
{
    USHORT  MdiStatusReg;

    // Read the status register at phy 1
    MdiRead(Adapter, MDI_STATUS_REG, Adapter->PhyAddress, &MdiStatusReg);
    MdiRead(Adapter, MDI_STATUS_REG, Adapter->PhyAddress, &MdiStatusReg);
    if (MdiStatusReg & MDI_SR_LINK_STATUS)
        return(NdisMediaStateConnected);
    else
        return(NdisMediaStateDisconnected);

}

