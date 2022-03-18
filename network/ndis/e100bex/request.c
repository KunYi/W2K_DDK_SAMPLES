/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    request.c

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#include "precomp.h"
#include "d100.rc"
#pragma hdrstop
#pragma warning (disable: 4514)

//-----------------------------------------------------------------------------
// Procedure:   D100SetPacketFilter
//
// Description: This routine will set up the adapter so that it accepts packets
//              that match the specified packet filter.  The only filter bits
//              that can truly be toggled are for broadcast and promiscuous
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      NewPacketFilter - A bit-mask which contains the new packet filter.
//
// Returns:
//      NDIS_STATUS_SUCCESS
//      NDIS_STATUS_NOT_ACCEPTED
//      NDIS_STATUS_NOT_SUPPORTED
//-----------------------------------------------------------------------------

NDIS_STATUS
D100SetPacketFilter(
                    IN PD100_ADAPTER Adapter,
                    IN ULONG NewPacketFilter
                    )

{

    UCHAR           NewParameterField;
    UINT            i;
    NDIS_STATUS     Status;
    DEBUGFUNC("D100SetPacketFilter");

    // the filtering changes need to result in the hardware being
    // reset.
    if (!((NewPacketFilter ^ Adapter->PacketFilter) &
        (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST |
        NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_PROMISCUOUS |
        NDIS_PACKET_TYPE_ALL_MULTICAST)))
    {
        REQUEST(Adapter, ("Filter Type %08x didn't change significantly, so return\n", NewPacketFilter));

        // save the new packet filter in our adapter object
        Adapter->PacketFilter = NewPacketFilter;

        return (NDIS_STATUS_SUCCESS);
    }

    // save the new packet filter in our adapter object
    Adapter->PacketFilter = NewPacketFilter;

    // Need to enable or disable broadcast and promiscuous support depending
    // on the new filter
    NewParameterField = CB_557_CFIG_DEFAULT_PARM15;

    if (NewPacketFilter & NDIS_PACKET_TYPE_BROADCAST)
        NewParameterField &= ~CB_CFIG_BROADCAST_DIS;
    else
        NewParameterField |= CB_CFIG_BROADCAST_DIS;

    if (NewPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
        NewParameterField |= CB_CFIG_PROMISCUOUS;
    else
        NewParameterField &= ~CB_CFIG_PROMISCUOUS;

    // Only need to do something to the HW if the filter bits have changed.
    if ((Adapter->OldParameterField != NewParameterField ) ||
        (NewPacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST))
    {
        REQUEST(Adapter, ("Filter Type %08x changed -- doing re-config\n", NewPacketFilter));

        Adapter->OldParameterField = NewParameterField;
        Adapter->NonTxCmdBlockHdr->CbCommand = CB_CONFIGURE;
        Adapter->NonTxCmdBlockHdr->CbStatus = 0;
        Adapter->NonTxCmdBlockHdr->CbLinkPointer = DRIVER_NULL;

        // First fill in the static (end user can't change) config bytes
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[0] = CB_557_CFIG_DEFAULT_PARM0;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[2] = CB_557_CFIG_DEFAULT_PARM2;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[3] = CB_557_CFIG_DEFAULT_PARM3;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[9] = CB_557_CFIG_DEFAULT_PARM9;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[10] = CB_557_CFIG_DEFAULT_PARM10;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[11] = CB_557_CFIG_DEFAULT_PARM11;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[12] = CB_557_CFIG_DEFAULT_PARM12;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[13] = CB_557_CFIG_DEFAULT_PARM13;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[14] = CB_557_CFIG_DEFAULT_PARM14;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[15] = CB_557_CFIG_DEFAULT_PARM15;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[16] = CB_557_CFIG_DEFAULT_PARM16;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[17] = CB_557_CFIG_DEFAULT_PARM17;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[18] = CB_557_CFIG_DEFAULT_PARM18;
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[20] = CB_557_CFIG_DEFAULT_PARM20;

        // next toggle in or out of promiscuous mode

        if (NewPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
        {
            // turn on save bad frames
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[6] =
                (CB_557_CFIG_DEFAULT_PARM6 | CB_CFIG_SAVE_BAD_FRAMES);

            // turn on save short frames
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[7] =
                (UCHAR) ((CB_557_CFIG_DEFAULT_PARM7 &
                (~(CB_CFIG_URUN_RETRY | CB_CFIG_DISC_SHORT_FRAMES))) |
                (Adapter->AiUnderrunRetry << 1)
                );

        }

        else
        {
            // turn off save bad frames
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[6] =
                CB_557_CFIG_DEFAULT_PARM6;

            // turn off save short frames
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[7] =
                (UCHAR) ((CB_557_CFIG_DEFAULT_PARM7 &
                (~CB_CFIG_URUN_RETRY)) |
                (Adapter->AiUnderrunRetry << 1)
                );
        }


        // Set the Tx and Rx Fifo limits
        Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[1] =
            (UCHAR) ((Adapter->AiTxFifo << 4) | Adapter->AiRxFifo);

        // set the MWI enable bit if needed
        if (Adapter->MWIEnable)
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[3] |= CB_CFIG_B3_MWI_ENABLE;

        // Set the Tx and Rx DMA maximum byte count fields.
        if ((Adapter->AiRxDmaCount) || (Adapter->AiTxDmaCount))
        {
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[4] =
                Adapter->AiRxDmaCount;
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[5] =
                (UCHAR) (Adapter->AiTxDmaCount | CB_CFIG_DMBC_EN);
        }

        else
        {
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[4] =
                CB_557_CFIG_DEFAULT_PARM4;
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[5] =
                CB_557_CFIG_DEFAULT_PARM5;
        }

        // Setup for MII or 503 operation.  The CRS+CDT bit should only be
        // set when operating in 503 mode.
        if (Adapter->PhyAddress == 32)
        {
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[8] =
                (CB_557_CFIG_DEFAULT_PARM8 & (~CB_CFIG_503_MII));
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[15] =
                (UCHAR) (NewParameterField | CB_CFIG_CRS_OR_CDT);
        }
        else
        {
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[8] =
                (CB_557_CFIG_DEFAULT_PARM8 | CB_CFIG_503_MII);
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[15] =
                (UCHAR) (NewParameterField & (~CB_CFIG_CRS_OR_CDT));
        }

        // Setup Full duplex stuff

        // If forced to half duplex
        if (Adapter->AiForceDpx == 1)
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
            (CB_557_CFIG_DEFAULT_PARM19 &
            (~(CB_CFIG_FORCE_FDX| CB_CFIG_FDX_ENABLE)));

        // If forced to full duplex
        else if (Adapter->AiForceDpx == 2)
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
            (CB_557_CFIG_DEFAULT_PARM19 | CB_CFIG_FORCE_FDX);

        // If auto-duplex
        else
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[19] =
            CB_557_CFIG_DEFAULT_PARM19;

        // if multicast all is being turned on, set the bit
        if (NewPacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST)
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[21] =
            (CB_557_CFIG_DEFAULT_PARM21 | CB_CFIG_MULTICAST_ALL);
        else
            Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[21] =
            CB_557_CFIG_DEFAULT_PARM21;


        // Wait for the SCB to clear before we check the CU status.
        WaitScb(Adapter);

        // If we have issued any transmits, then the CU will either be active,
        // or in the suspended state.  If the CU is active, then we wait for
        // it to be suspended.
        if (Adapter->TransmitIdle == FALSE)
        {
            // Wait for suspended state
            for (i = 200000; i != 0; i--)
            {
                if ((Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) != SCB_CUS_ACTIVE)
                    break;
                NdisStallExecution(20);
            }
            if (!i)
            {
                HARDWARE_NOT_RESPONDING (Adapter);
                Status = (NDIS_STATUS_FAILURE);
            }

            // Restore the transmit software flags.  After the multicast
            // command is issued, the command unit will be idle, because the
            // EL bit will be set in the multicast commmand block.
            Adapter->TransmitIdle = TRUE;
            Adapter->ResumeWait = TRUE;
        }


        // display the config info to the debugger
        REQUEST(Adapter, ("Re-Issuing Configure command for filter change\n"));
        REQUEST(Adapter, ("Config Block at virt addr %x, phys address %x\n",
            &Adapter->NonTxCmdBlockHdr->CbStatus, Adapter->NonTxCmdBlockPhys));

        for (i=0; i < CB_CFIG_BYTE_COUNT; i++)
            REQUEST(Adapter, ("  Config byte %x = %.2x\n", i, Adapter->NonTxCmdBlock->NonTxCb.Config.ConfigBytes[i]));


        Adapter->CSRAddress->ScbGeneralPointer = Adapter->NonTxCmdBlockPhys;

        // Submit the configure command to the chip, and wait for it to complete.
        if (!D100SubmitCommandBlockAndWait(Adapter))
            Status = NDIS_STATUS_NOT_ACCEPTED;
        else
            Status = NDIS_STATUS_SUCCESS;

        // Release the spinlock


        return(Status);
    }

    REQUEST(Adapter, ("Filter Type %08x did not change config\n", NewPacketFilter));
    return (NDIS_STATUS_SUCCESS);
}



//-----------------------------------------------------------------------------
// Procedure:   D100ChangeMCAddresses
//
// Description: This routine will set up the adapter for a specified multicast
//              address list.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      AddressCount - The new number of Multicast addresses.
//
// Returns:
//      NDIS_STATUS_SUCCESS
//      NDIS_STATUS_NOT_ACCEPTED
//-----------------------------------------------------------------------------

NDIS_STATUS
D100ChangeMCAddresses(
                      IN PD100_ADAPTER Adapter,
                      IN UINT AddressCount
                      )

{

    // Holds the change that should be returned to the filtering package.
    PUCHAR              McAddress;
    UINT                i, j;
    NDIS_STATUS         Status;

    DEBUGFUNC("D100ChangeMCAddresses");

    TRACE1(Adapter,("\n"));

    TRACE1(Adapter,("Configuring for %x mc addresses\n", AddressCount));

    DEBUGCHAR(Adapter,'M');




    // Setup the command block for the multicast command.
    for (i = 0;(i < AddressCount) && (i < MAX_MULTICAST_ADDRESSES);i++)
    {
        TRACE1(Adapter,(" mc %d =%.2x %.2x %.2x %.2x %.2x %.2x\n", i,
            Adapter->PrivateMulticastBuffer[i][0],
            Adapter->PrivateMulticastBuffer[i][1],
            Adapter->PrivateMulticastBuffer[i][2],
            Adapter->PrivateMulticastBuffer[i][3],
            Adapter->PrivateMulticastBuffer[i][4],
            Adapter->PrivateMulticastBuffer[i][5]));

        McAddress = &Adapter->NonTxCmdBlock->NonTxCb.Multicast.McAddress[i*ETHERNET_ADDRESS_LENGTH];

        for (j = 0; j < ETH_LENGTH_OF_ADDRESS; j++)
            *(McAddress++) = Adapter->PrivateMulticastBuffer[i][j];
    }


    Adapter -> NonTxCmdBlock -> NonTxCb.Multicast.McCount =
        (USHORT) (AddressCount * ETH_LENGTH_OF_ADDRESS);
    Adapter->NonTxCmdBlockHdr->CbStatus = 0;
    Adapter->NonTxCmdBlockHdr->CbCommand = CB_MULTICAST;

    // Wait for the SCB to clear before we check the CU status.
    WaitScb(Adapter);

    // If we have issued any transmits, then the CU will either be active, or
    // in the suspended state.  If the CU is active, then we wait for it to be
    // suspended.
    if (Adapter->TransmitIdle == FALSE)
    {
        // Wait for suspended state
        for (i = 200000; i != 0; i--)
        {
            if ((Adapter->CSRAddress->ScbStatus & SCB_CUS_MASK) != SCB_CUS_ACTIVE)
                break;
            NdisStallExecution(20);
        }
        if (!i)
        {
            HARDWARE_NOT_RESPONDING (Adapter);
            Status = (NDIS_STATUS_FAILURE);
        }

        // Restore the transmit software flags.  After the multicast command is
        // issued, the command unit will be idle, because the EL bit will be
        // set in the multicast commmand block.
        Adapter->TransmitIdle = TRUE;
        Adapter->ResumeWait = TRUE;
    }

    // Update the command list pointer.
    Adapter->CSRAddress->ScbGeneralPointer = Adapter->NonTxCmdBlockPhys;

    // Submit the multicast command to the adapter and wait for it to complete.
    if (!D100SubmitCommandBlockAndWait(Adapter))
        Status = (NDIS_STATUS_NOT_ACCEPTED);
    else
        Status = (NDIS_STATUS_SUCCESS);

    DEBUGCHAR(Adapter,'m');

    return(Status);

}


//-----------------------------------------------------------------------------
// Procedure:   D100SetInformation
//
// Description: D100SetInformation handles a set operation for a single OID.
//              The only operations that really change the configuration of
//              the adapter are set PACKET_FILTER, and SET_MULTICAST.
//
// Arguments:
//      MiniportAdapterContext - The context value returned by the Miniport
//                               when the adapter was initialized (see the call
//                               NdisMSetAttributes. In reality, it is a
//                               pointer to D100_ADAPTER.
//      Oid - The Oid that is to be set.
//      InformationBuffer -  Holds the data to be set.
//      InformationBufferLength - The length of InformationBuffer.
//
// Result:
//      BytesRead - If the call is successful, returns the number of bytes
//                  read from InformationBuffer.
//      BytesNeeded - If there is not enough data in OvbBuffer to satisfy the
//                    OID, returns the amount of storage needed.
//
// Returns:
//      NDIS_STATUS_SUCCESS
//      NDIS_STATUS_INVALID_LENGTH
//      NDIS_STATUS_INVALID_OID
//      NDIS_STATUS_NOT_SUPPORTED
//      NDIS_STATUS_NOT_ACCEPTED
//-----------------------------------------------------------------------------

NDIS_STATUS
D100SetInformation(
                   IN NDIS_HANDLE MiniportAdapterContext,
                   IN NDIS_OID Oid,
                   IN PVOID InformationBuffer,
                   IN ULONG InformationBufferLength,
                   OUT PULONG BytesRead,
                   OUT PULONG BytesNeeded
                   )

{
    NDIS_STATUS         Status;
    ULONG               PacketFilter;
    PD100_ADAPTER       Adapter;

    DEBUGFUNC("D100SetInformation");

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    REQUEST(Adapter,("\n"));

    DEBUGCHAR(Adapter,'N');

    *BytesRead = 0;
    *BytesNeeded = 0;

    switch (Oid)
    {

    case OID_802_3_MULTICAST_LIST:

        // The data must be a multiple of the Ethernet address size.
        if (InformationBufferLength % ETH_LENGTH_OF_ADDRESS != 0)
            return (NDIS_STATUS_INVALID_LENGTH);

        // Save these new MC addresses to our adapter object
        NdisMoveMemory(Adapter->PrivateMulticastBuffer,
            InformationBuffer,
            InformationBufferLength);

        // Save the number of MC address in our adapter object
        Adapter->NumberOfMcAddresses =
            (InformationBufferLength / ETH_LENGTH_OF_ADDRESS);

        // If this is a miniport driver, then we'll directly call our
        // D100ChangeMCAddress routine, to have the hardware change its mulicast
        // address filter
        Status = D100ChangeMCAddresses(
            Adapter,
            InformationBufferLength /
            ETH_LENGTH_OF_ADDRESS);

        *BytesRead = InformationBufferLength;
        break;


    case OID_GEN_CURRENT_PACKET_FILTER:

        // Verify the Length
        if (InformationBufferLength != 4)
            return (NDIS_STATUS_INVALID_LENGTH);

        // Now call the filter package to set the packet filter.
        NdisMoveMemory((PVOID)&PacketFilter, InformationBuffer, sizeof(ULONG));

        // Verify bits, if any bits are set that we don't support, leave
        if (PacketFilter &
            ~(NDIS_PACKET_TYPE_DIRECTED |
             NDIS_PACKET_TYPE_MULTICAST |
             NDIS_PACKET_TYPE_BROADCAST |
             NDIS_PACKET_TYPE_PROMISCUOUS |
             NDIS_PACKET_TYPE_ALL_MULTICAST))
        {
            REQUEST(Adapter, ("Filter Type %08x not supported\n", PacketFilter));
            Status = NDIS_STATUS_NOT_SUPPORTED;
            *BytesRead = 4;
            break;
        }

        // If this is a miniport driver, the submit the packet filter change
        // to our hardware, by directly calling D100SetPacketFilter.
        Status = D100SetPacketFilter(
            Adapter,
            PacketFilter);

        *BytesRead = InformationBufferLength;
        break;


    case OID_GEN_CURRENT_LOOKAHEAD:

        // Verify the Length
        if (InformationBufferLength != 4)
            return (NDIS_STATUS_INVALID_LENGTH);

        *BytesRead = 4;
        Status = NDIS_STATUS_SUCCESS;
        break;


        // stub the next four cases to set up power management stuff
        // if the adapter had supported power management, the commented lines
        // should replace the stub lines. See the power management spec

    case OID_PNP_SET_POWER:
        TRACEPOWER(("SET: Power State change to 0x%08X!!!\n",InformationBuffer));

        //        *BytesRead = InformationBufferLength;
        //        Status = NDIS_STATUS_SUCCESS;
        *BytesRead = 0;                     //stub
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub
        break;

    case OID_PNP_ADD_WAKE_UP_PATTERN:
        TRACEPOWER(("SET: Got a WakeUpPattern SET    Call\n"));
        // call a function that would program the adapter's wake
        // up pattern, return success
        //*BytesRead = InformationBufferLength;
        *BytesRead = 0;                     //stub
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub
        break;

    case OID_PNP_REMOVE_WAKE_UP_PATTERN:
        TRACEPOWER(("SET: Got a WakeUpPattern REMOVE Call\n"));
        // call a function that would remove the adapter's wake
        // up pattern, return success
        //*BytesRead = InformationBufferLength;
        *BytesRead = 0;                     //stub
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub
        break;

    case OID_PNP_ENABLE_WAKE_UP:
        TRACEPOWER(("SET: Got a EnableWakeUp Call of %08X\n",InformationBuffer));
        // call a function that would enable wake up on the adapter
        // return success
        //*BytesRead = InformationBufferLength;
        *BytesRead = 0;                     //stub
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub
        break;

        /* this OID is for showing how to work with driver specific (custom)
        OIDs and the NDIS 5 WMI interface using GUIDs
        */
    case OID_CUSTOM_DRIVER_SET:
        REQUEST(Adapter,("CUSTOM_DRIVER_SET got a SET\n"));
        *BytesRead = 4;
        Adapter->CustomDriverSet = (ULONG) PtrToUlong(InformationBuffer);
        break;

        /*
        case OID_TCP_TASK_OFFLOAD:
        REQUEST(Adapter,("Task Offload request received\n"));
        // here we would enable whatever features the protocol requested
        // only on a per packet basis though.
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub
        break;
        */
    default:
        Status = NDIS_STATUS_INVALID_OID;
        break;

    }  // end of switch construct
    DEBUGCHAR(Adapter,'n');
    return Status;
}


//-----------------------------------------------------------------------------
// Procedure:   D100QueryInformation
//
// Description: D100QueryInformation handles a query operation for a single
//              OID. Basically we return information about the current state of
//              the OID in question.
//
// Arguments:
//      MiniportAdapterContext - The context value returned by the Miniport
//                               when the adapter was initialized (see the call
//                               NdisMSetAttributes. In reality, it is a
//                               pointer to D100_ADAPTER.
//      Oid - The Oid that is to be queried.
//      InformationBuffer -  A pointer to the buffer that holds the result of
//                           the query.
//      InformationBufferLength - The length of InformationBuffer.
//
// Result:
//      BytesWritten - If the call is successful, returns the number of bytes
//                     written into InformationBuffer.
//      BytesNeeded - If there is not enough data in OvbBuffer to satisfy the
//                    OID, returns the amount of storage needed.
//
// Returns:
//      NDIS_STATUS_SUCCESS
//      NDIS_STATUS_NOT_SUPPORTED
//      NDIS_STATUS_BUFFER_TO_SHORT
//-----------------------------------------------------------------------------

NDIS_STATUS
D100QueryInformation(
                     IN NDIS_HANDLE MiniportAdapterContext,
                     IN NDIS_OID Oid,
                     IN PVOID InformationBuffer,
                     IN ULONG InformationBufferLength,
                     OUT PULONG BytesWritten,
                     OUT PULONG BytesNeeded
                     )

{
    // order is important here because the OIDs should be in order
    // of increasing value
    static
    NDIS_OID        D100GlobalSupportedOids[] =
    {
        OID_GEN_SUPPORTED_LIST,
        OID_GEN_HARDWARE_STATUS,
        OID_GEN_MEDIA_SUPPORTED,
        OID_GEN_MEDIA_IN_USE,
        OID_GEN_MAXIMUM_LOOKAHEAD,
        OID_GEN_MAXIMUM_FRAME_SIZE,
        OID_GEN_LINK_SPEED,
        OID_GEN_TRANSMIT_BUFFER_SPACE,
        OID_GEN_RECEIVE_BUFFER_SPACE,
        OID_GEN_TRANSMIT_BLOCK_SIZE,
        OID_GEN_RECEIVE_BLOCK_SIZE,
        OID_GEN_VENDOR_ID,
        OID_GEN_VENDOR_DESCRIPTION,
        OID_GEN_CURRENT_PACKET_FILTER,
        OID_GEN_CURRENT_LOOKAHEAD,
        OID_GEN_DRIVER_VERSION,
        OID_GEN_MAXIMUM_TOTAL_SIZE,
        OID_GEN_PROTOCOL_OPTIONS,
        OID_GEN_MAC_OPTIONS,
        OID_GEN_MEDIA_CONNECT_STATUS,
        OID_GEN_MAXIMUM_SEND_PACKETS,
        OID_GEN_SUPPORTED_GUIDS,
        OID_GEN_XMIT_OK,
        OID_GEN_RCV_OK,
        OID_GEN_XMIT_ERROR,
        OID_GEN_RCV_ERROR,
        OID_GEN_RCV_NO_BUFFER,
        OID_GEN_RCV_CRC_ERROR,
        OID_GEN_TRANSMIT_QUEUE_LENGTH,
        OID_802_3_PERMANENT_ADDRESS,
        OID_802_3_CURRENT_ADDRESS,
        OID_802_3_MULTICAST_LIST,
        OID_802_3_MAXIMUM_LIST_SIZE,
        /* for ndis 4 packet priority
        OID_802_3_MAC_OPTIONS,
        */
        OID_802_3_RCV_ERROR_ALIGNMENT,
        OID_802_3_XMIT_ONE_COLLISION,
        OID_802_3_XMIT_MORE_COLLISIONS,
        OID_802_3_XMIT_DEFERRED,
        OID_802_3_XMIT_MAX_COLLISIONS,
        OID_802_3_RCV_OVERRUN,
        OID_802_3_XMIT_UNDERRUN,
        OID_802_3_XMIT_HEARTBEAT_FAILURE,
        OID_802_3_XMIT_TIMES_CRS_LOST,
        OID_802_3_XMIT_LATE_COLLISIONS,
        /* tcp/ip checksum offload */
        /*
        OID_TCP_TASK_OFFLOAD,
        */
        /* powermanagement */
        OID_PNP_CAPABILITIES,
        OID_PNP_SET_POWER,
        OID_PNP_QUERY_POWER,
        OID_PNP_ADD_WAKE_UP_PATTERN,
        OID_PNP_REMOVE_WAKE_UP_PATTERN,
        OID_PNP_ENABLE_WAKE_UP,
        /* end powermanagement */
        /* custom oid WMI support */
        OID_CUSTOM_DRIVER_SET,
        OID_CUSTOM_DRIVER_QUERY,
        OID_CUSTOM_ARRAY,
        OID_CUSTOM_STRING
    };


    // String describing our adapter
    char VendorDescriptor[] = VENDORDESCRIPTOR;


    PD100_ADAPTER   Adapter;

    UCHAR           VendorId[4];

    NDIS_MEDIUM     Medium = NdisMedium802_3;
    NDIS_HARDWARE_STATUS HardwareStatus = NdisHardwareStatusReady;

    UINT            GenericUlong;
    USHORT          GenericUShort;
    UCHAR           GenericArray[6];

    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;

    NDIS_PNP_CAPABILITIES    Power_Management_Capabilities;

    // Common variables for pointing to result of query
    PVOID           MoveSource = (PVOID) (&GenericUlong);
    ULONG           MoveBytes = sizeof(GenericUlong);


    // WMI support
    // check out the e100b.mof file for examples of how the below
    // maps into a .mof file for external advertisement of GUIDs
#define NUM_CUSTOM_GUIDS 4

    static const NDIS_GUID GuidList[NUM_CUSTOM_GUIDS] =
    { { // {F4A80276-23B7-11d1-9ED9-00A0C9010057} example of a uint set
            E100BExampleSetUINT_OIDGuid,
            OID_CUSTOM_DRIVER_SET,
            sizeof(ULONG),
            (fNDIS_GUID_TO_OID)
    },
    { // {F4A80277-23B7-11d1-9ED9-00A0C9010057} example of a uint query
            E100BExampleQueryUINT_OIDGuid,
            OID_CUSTOM_DRIVER_QUERY,
            sizeof(ULONG),
            (fNDIS_GUID_TO_OID)
        },
    { // {F4A80278-23B7-11d1-9ED9-00A0C9010057} example of an array query
            E100BExampleQueryArrayOIDGuid,
            OID_CUSTOM_ARRAY,
            sizeof(UCHAR), // size is size of each element in the array
            (fNDIS_GUID_TO_OID|fNDIS_GUID_ARRAY)
        },
    { // {F4A80279-23B7-11d1-9ED9-00A0C9010057} example of a string query
            E100BExampleQueryStringOIDGuid,
            OID_CUSTOM_STRING,
            (ULONG) -1, // size is -1 for ANSI or NDIS_STRING string types
            (fNDIS_GUID_TO_OID|fNDIS_GUID_ANSI_STRING)
        }
    };


    DEBUGFUNC("D100QueryInformation");
    INITSTR(("\n"));

    Adapter = PD100_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);
    DEBUGCHAR(Adapter,'Q');

    // Initialize the result
    *BytesWritten = 0;
    *BytesNeeded = 0;

    // Switch on request type
    switch (Oid)
    {

    case OID_GEN_MAC_OPTIONS:
        // NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA is set to indicate to the
        // protocol that it can access the lookahead data by any means that
        // it wishes.  On some systems there are fast copy routines that
        // may have trouble accessing shared memory.  Netcard drivers that
        // indicate data out of shared memory, should not have this flag
        // set on these troublesome systems  For the time being this driver
        // will set this flag.  This should be safe because the data area
        // of the RFDs is contained in uncached memory.

        // NOTE: Don't set NDIS_MAC_OPTION_RECEIVE_SERIALIZED if we
        // are doing multipacket (ndis4) style receives.

        GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND   |
            NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA  |
            NDIS_MAC_OPTION_NO_LOOPBACK
            );
        break;


    case OID_GEN_SUPPORTED_LIST:

        MoveSource = (PVOID) (D100GlobalSupportedOids);
        MoveBytes = sizeof(D100GlobalSupportedOids);
        break;


    case OID_GEN_HARDWARE_STATUS:

        MoveSource = (PVOID)(&HardwareStatus);
        MoveBytes = sizeof(NDIS_HARDWARE_STATUS);
        break;


    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:

        MoveSource = (PVOID) (&Medium);
        MoveBytes = sizeof(NDIS_MEDIUM);
        break;

    case OID_GEN_MAXIMUM_LOOKAHEAD:
    case OID_GEN_CURRENT_LOOKAHEAD:
    case OID_GEN_MAXIMUM_FRAME_SIZE:

        GenericUlong = MAXIMUM_ETHERNET_PACKET_SIZE - ENET_HEADER_SIZE;
        break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
    case OID_GEN_TRANSMIT_BLOCK_SIZE:
    case OID_GEN_RECEIVE_BLOCK_SIZE:

        GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE;
        break;


    case OID_GEN_LINK_SPEED:

        // We won't always know what line speed we are actually running at, but
        // report the value that the we detected when the driver loaded.
        GenericUlong =  (((ULONG) Adapter->AiLineSpeedCur) * 10000);
        break;


    case OID_GEN_TRANSMIT_BUFFER_SPACE:

        GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE *
            Adapter -> RegNumTcb;
        break;


    case OID_GEN_RECEIVE_BUFFER_SPACE:

        GenericUlong = (ULONG) MAXIMUM_ETHERNET_PACKET_SIZE *
            Adapter -> NumRfd;
        break;


    case OID_GEN_VENDOR_ID:

        NdisMoveMemory(VendorId, Adapter->AiNodeAddress, 3);
        VendorId[3] = 0x0;
        MoveSource = (PVOID) VendorId;
        MoveBytes = sizeof(VendorId);
        break;


    case OID_GEN_VENDOR_DESCRIPTION:

        MoveSource = (PVOID) VendorDescriptor;
        MoveBytes = sizeof(VendorDescriptor);
        break;


    case OID_GEN_DRIVER_VERSION:

        GenericUShort = (USHORT) D100_DRIVER_VERSION;
        MoveSource = (PVOID)(&GenericUShort);
        MoveBytes = sizeof(GenericUShort);
        break;

        // WMI support
    case OID_GEN_SUPPORTED_GUIDS:
        MoveSource = (PUCHAR) &GuidList;
        MoveBytes =  sizeof(GuidList);
        break;

        /*
        ******************************************************************************
        // Task Offload
        case OID_GEN_TASK_OFFLOAD:
        // here we set up the capabilities that we support
        // later the protocol will come back with a set requesting
        // some subset of the  specific capabilities we support.
        // this is all commented out because it is just a stub
        //


        //             TaskOffload.Size = sizeof(NDIS_TASK_OFFLOAD);
        //             TaskOffload.Task = NDIS_TASK_TCP_IP_CHECKSUM;
        //             // offsetnext should be 0 if this is the last task supported.
        //             TaskOffload.OffsetNextTask = (ULONG) (&TaskOffload + sizeof(NDIS_TASK_OFFLOAD));
        //             TaskOffload.TaskBufferLength = sizeof(NDIS_TASK_TCP_IP_CHECKSUM);
        //
        //             //set up our TaskBuffer
        //             TaskBufferDescriptor.Receive.TcpChecksum = TRUE;
        //             TaskOffload.TaskBuffer = &TaskBufferDescriptor;
        //
        //             MoveSource = (PVOID) &TaskOffload;
        //             MoveBytes = sizeof(NDIS_TASK_OFFLOAD) * NUM_TASKS_SUPPORTED;

          Status = NDIS_STATUS_NOT_SUPPORTED; // stub
          break;

        ******************************************************************************
        */
    case OID_802_3_PERMANENT_ADDRESS:

        ETH_COPY_NETWORK_ADDRESS(
            (PCHAR) GenericArray,
            Adapter->AiPermanentNodeAddress);

        MoveSource = (PVOID) (GenericArray);
        MoveBytes = ETH_LENGTH_OF_ADDRESS;
        break;


    case OID_802_3_CURRENT_ADDRESS:

        ETH_COPY_NETWORK_ADDRESS(
            GenericArray,
            Adapter->AiNodeAddress);

        MoveSource = (PVOID) (GenericArray);
        MoveBytes = ETH_LENGTH_OF_ADDRESS;
        break;


    case OID_802_3_MAXIMUM_LIST_SIZE:

        GenericUlong = (ULONG) MAX_MULTICAST_ADDRESSES;
        break;

    case OID_GEN_MAXIMUM_SEND_PACKETS:
        GenericUlong = (ULONG) MAX_ARRAY_SEND_PACKETS;
        break;

    case OID_GEN_MEDIA_CONNECT_STATUS:
        if (Adapter->LinkIsActive != GetConnectionStatus(Adapter))
            // if status has changed
        {
            switch ( Adapter->LinkIsActive )
            {
            case NdisMediaStateConnected:           // changing from connected
                Adapter->LinkIsActive = NdisMediaStateDisconnected;
                break;

            case NdisMediaStateDisconnected:        // changing from disconnected
                Adapter->LinkIsActive = NdisMediaStateConnected;
                break;
            }
        }
        // now we simply return our status (NdisMediaState[Dis]Connected)
        // this line also covers the case where our link status hasn't changed.
        GenericUlong = (ULONG) Adapter->LinkIsActive;

        break;

    case OID_PNP_CAPABILITIES:
        TRACEPOWER(("QUERY:Got a Query Capabilities Call\n"));
        // since we are stubbing power management, return only the
        // D0 power state indicating that the lowest power state we
        // support is D0 (full power)
        // Power_Management_Capabilities.WakeUpCapabilities.MinMagicPacketWakeUp   = NdisDeviceStateD0;
        // Power_Management_Capabilities.WakeUpCapabilities.MinPatternWakeUp       = NdisDeviceStateD0;
        // Power_Management_Capabilities.WakeUpCapabilities.MinLinkChangeWakeUp    = NdisDeviceStateD0;

        // MoveSource = (PVOID) &Power_Management_Capabilities;
        // MoveBytes = sizeof(NDIS_PNP_CAPABILITIES);
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub

        break;

    case OID_PNP_QUERY_POWER:
        TRACEPOWER(("QUERY:Got a Query Power Call of 0x%08X\n",InformationBuffer));
        // Adapter->NextPowerState = (NDIS_DEVICE_POWER_STATE) InformationBuffer;
        // return success indicating we will do nothing to jeapordize the
        // transition...
        // Status is pre-set in this routine to Success
        // but since we aren't actually supporting power manangement
        // return failure.
        Status = NDIS_STATUS_NOT_SUPPORTED; //stub

        break;

        // WMI support
        // this is the uint case
    case OID_CUSTOM_DRIVER_QUERY:
        REQUEST(Adapter,("CUSTOM_DRIVER_QUERY got a QUERY\n"));
        GenericUlong = ++Adapter->CustomDriverSet;
        break;

    case OID_CUSTOM_DRIVER_SET:
        REQUEST(Adapter,("CUSTOM_DRIVER_SET got a QUERY\n"));
        GenericUlong = Adapter->CustomDriverSet;
        break;

        // this is the array case
    case OID_CUSTOM_ARRAY:
        REQUEST(Adapter,("CUSTOM_ARRAY got a QUERY\n"));
        NdisMoveMemory(VendorId, Adapter->AiNodeAddress, 4);
        MoveSource = (PVOID) VendorId;
        MoveBytes = sizeof(VendorId);
        break;

        // this is the string case
    case OID_CUSTOM_STRING:
        REQUEST(Adapter,("CUSTOM_STRING got a QUERY\n"));
        MoveSource = (PVOID) VendorDescriptor;
        MoveBytes = sizeof(VendorDescriptor);
        break;

    default:

        // The query is for a driver statistic, so we need to first
        // update our statistics in software.

        DumpStatsCounters(Adapter);

        switch (Oid)
        {

        case OID_GEN_XMIT_OK:

            GenericUlong = (ULONG) Adapter->GoodTransmits;
            break;

        case OID_GEN_RCV_OK:

            GenericUlong = (ULONG) Adapter->GoodReceives;
            break;

        case OID_GEN_XMIT_ERROR:

            GenericUlong = (ULONG) (Adapter->TxAbortExcessCollisions +
                Adapter->TxDmaUnderrun +
                Adapter->TxLostCRS +
                Adapter->TxLateCollisions);
            break;

        case OID_GEN_RCV_ERROR:

            // receive error counters are kept on chip.
            GenericUlong = (ULONG) (Adapter->RcvCrcErrors +
                Adapter->RcvAlignmentErrors +
                Adapter->RcvResourceErrors +
                Adapter->RcvDmaOverrunErrors +
                Adapter->RcvRuntErrors);
            break;

        case OID_GEN_RCV_NO_BUFFER:

            GenericUlong = (ULONG) Adapter->RcvResourceErrors;
            break;

        case OID_GEN_RCV_CRC_ERROR:

            GenericUlong = (ULONG) Adapter->RcvCrcErrors;
            break;

        case OID_GEN_TRANSMIT_QUEUE_LENGTH:

            GenericUlong = (ULONG) Adapter->RegNumTcb;
            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:

            GenericUlong = (ULONG) Adapter->RcvAlignmentErrors;
            break;

        case OID_802_3_XMIT_ONE_COLLISION:

            GenericUlong = (ULONG) Adapter->OneRetry;
            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:

            GenericUlong = (ULONG) Adapter->MoreThanOneRetry;
            break;

        case OID_802_3_XMIT_DEFERRED:
            GenericUlong = (ULONG) Adapter->TxOKButDeferred;
            break;

        case OID_802_3_XMIT_MAX_COLLISIONS:
            GenericUlong = (ULONG) Adapter->TxAbortExcessCollisions;
            break;

        case OID_802_3_RCV_OVERRUN:
            GenericUlong = (ULONG) Adapter->RcvDmaOverrunErrors;
            break;

        case OID_802_3_XMIT_UNDERRUN:
            GenericUlong = (ULONG) Adapter->TxDmaUnderrun;
            break;

        case OID_802_3_XMIT_HEARTBEAT_FAILURE:
            GenericUlong = (ULONG) Adapter->TxLostCRS;
            break;

        case OID_802_3_XMIT_TIMES_CRS_LOST:
            GenericUlong = (ULONG) Adapter->TxLostCRS;
            break;

        case OID_802_3_XMIT_LATE_COLLISIONS:
            GenericUlong = (ULONG) Adapter->TxLateCollisions;
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
        }
    }

    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (MoveBytes > InformationBufferLength)
        {
            // Not enough room in InformationBuffer. Punt
            *BytesNeeded = MoveBytes;

            Status = NDIS_STATUS_BUFFER_TOO_SHORT;
        }
        else
        {
            // Copy result into InformationBuffer
            *BytesWritten = MoveBytes;
            if (MoveBytes > 0)
                NdisMoveMemory(InformationBuffer, MoveSource, MoveBytes);
        }
    }

    return (Status);
}   /* end of D100QueryInformation */

