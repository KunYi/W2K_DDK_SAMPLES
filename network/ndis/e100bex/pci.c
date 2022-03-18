/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    pci.c

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
#pragma warning (disable: 4514)

//-----------------------------------------------------------------------------
// Procedure:   FindPciDevice50Scan
//
// Description: This routine finds an adapter for the driver to load on
//              The critical piece to understanding this routine is that
//              the System will not let us read any information from PCI
//              space from any slot but the one that the System thinks
//              we should be using. The configuration manager rules this
//              land... The Slot number used by this routine is just a
//              placeholder, it could be zero even.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      VendorID - Vendor ID of the adapter.
//      DeviceID - Device ID of the adapter.
//      PciCardsFound - A structure that contains an array of the IO addresses,
//                   IRQ, and node addresses of each PCI card that we find.
//
//    NOTE: due to NT 5's Plug and Play configuration manager
//          this routine will never return more than one device.
//
// Returns:
//      USHORT - Number of D100 based PCI adapters found in the scanned bus
//-----------------------------------------------------------------------------

USHORT
FindPciDevice50Scan(
                    IN PD100_ADAPTER Adapter,
                    IN USHORT     VendorID,
                    IN USHORT     DeviceID,
                    OUT PPCI_CARDS_FOUND_STRUC pPciCardsFound
                    )

{
    UINT                i;
    USHORT              found = 0;
    USHORT              PciCommandWord;
    USHORT              EepromWordValue;
    ULONG               Device_Vendor_Id;
    UCHAR               BridgeRevId = 0;
    NDIS_STATUS         Status;
    USHORT              Slot;
    PNDIS_RESOURCE_LIST  AssignedResources;

    // Zero out our Device and Vendor ID storage area.
    Device_Vendor_Id = 0;

    Slot = Adapter->AiSlot;

    // Read in the Device and Vendor ID of a given slot.
    NdisReadPciSlotInformation(
        Adapter->D100AdapterHandle,
        Slot,
        PCI_VENDOR_ID_REGISTER,
        (PVOID) &Device_Vendor_Id,
        0x4);

    // Check to see if we found a 82557/82558 based adapter.
    if ((((USHORT) Device_Vendor_Id) == VendorID) &&
        (((USHORT) (Device_Vendor_Id >> 16)) == DeviceID))
    {

        DEBUGSTR(("\n\n\n                          Found adapter %x at slot=%x\n",
            Device_Vendor_Id, Slot));

        //  We found our adapter, so now we need to get the resources that were
        //  assigned to the adapter that we just found.

        Status = NdisMPciAssignResources(
            Adapter->D100AdapterHandle,
            Slot,
            &AssignedResources);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            D100LogError(Adapter, EVENT_18, NDIS_ERROR_CODE_ADAPTER_DISABLED, 0);

            DEBUGSTR(("NdisMPciAssignResources on slot %x Failed - %x\n", Slot, Status));

            found = 0;
            pPciCardsFound->NumFound = found;
            return found;
        }

        pPciCardsFound->PciSlotInfo[found].MemPhysAddress = 0;

        for (i=0;i < AssignedResources->Count;i++ )
        {
            switch (AssignedResources->PartialDescriptors[i].Type)
            {
            case CmResourceTypePort:
                if (AssignedResources->PartialDescriptors[i].Flags & CM_RESOURCE_PORT_IO)
                {
                    pPciCardsFound->PciSlotInfo[found].BaseIo =
                        (ULONG) ((AssignedResources)->PartialDescriptors[i].u.Port.Start.u.LowPart);
                }
                break;
            case CmResourceTypeInterrupt:
                pPciCardsFound->PciSlotInfo[found].Irq =
                    (UCHAR) ((AssignedResources)->PartialDescriptors[i].u.Interrupt.Level);
                break;
            case CmResourceTypeMemory:
                // this if gets only the last memory bar with a length of 0x1000, as others
                // might be our flash address, a boot ROM address, or otherwise.
                if (((USHORT) (AssignedResources)->PartialDescriptors[i].u.Memory.Length) == 0x1000 )
                {
                    pPciCardsFound->PciSlotInfo[found].MemPhysAddress =
                        (ULONG) ((AssignedResources)->PartialDescriptors[i].u.Memory.Start.u.LowPart);
                }
                break;
            }
        }

        // read the revision id of the 82557/82558 (or controller)
        NdisReadPciSlotInformation(
            Adapter->D100AdapterHandle,
            Slot,
            PCI_REV_ID_REGISTER,
            &pPciCardsFound->PciSlotInfo[found].ChipRevision,
            0x1);

        DEBUGSTR(("NIC Controller revision id = %x\n", pPciCardsFound->PciSlotInfo[found].ChipRevision));


        // Read in the SubDevice and SubVendor ID
        NdisReadPciSlotInformation(
            Adapter->D100AdapterHandle,
            Slot,
            PCI_SUBVENDOR_ID_REGISTER,
            &pPciCardsFound->PciSlotInfo[found].SubVendor_DeviceID,
            0x4);


        pPciCardsFound->PciSlotInfo[found].SlotNumber = (USHORT) Slot;

        DEBUGSTR(("Found: bus=%x, slot=%x, mem_phys=%x, io=%x, irq=%x\n",
            Adapter->BusNumber,
            Adapter->AiSlot,
            pPciCardsFound->PciSlotInfo[found].MemPhysAddress,
            pPciCardsFound->PciSlotInfo[found].BaseIo,
            pPciCardsFound->PciSlotInfo[found].Irq));

        // We found a card and read it's assigned resources.
        // Now make sure that the resources are valid.
        if ((pPciCardsFound->PciSlotInfo[found].MemPhysAddress == 0) ||
            (pPciCardsFound->PciSlotInfo[found].MemPhysAddress == 0xfffffff0) ||
            (pPciCardsFound->PciSlotInfo[found].BaseIo == 0) ||
            (pPciCardsFound->PciSlotInfo[found].BaseIo == 0xfffffffc) ||
            (pPciCardsFound->PciSlotInfo[found].Irq == 0xff))
        {
            DEBUGSTR(("PCI resources were invalid\n"));

            // One of the resources wasn't valid, so log a message
            // into the event viewer.
            D100LogError(Adapter,
                EVENT_1,
                NDIS_ERROR_CODE_ADAPTER_DISABLED,
                0);
        }

        else
        {
            // We found a valid adapter with resources allocated,
            // so make sure its master bit is set (Some systems,
            // i.e. Dell and Compaq, do not set this bit via
            // the BIOS, so we have to do it).
            NdisReadPciSlotInformation(
                Adapter->D100AdapterHandle,
                Slot,
                PCI_COMMAND_REGISTER,
                &PciCommandWord,
                0x2);

            DEBUGSTR(("PCI command word = %x\n",PciCommandWord));

            if ((PciCommandWord & CMD_MEM_WRT_INVALIDATE) && (Adapter->MWIEnable))
            {
                DEBUGSTR(("MWI command will be enabled\n"));
                Adapter->MWIEnable = TRUE;
            }
            else
            {
                Adapter->MWIEnable = FALSE;
            }

            // enable BUS MASTERING if it isn't enabled by the BIOS
            if (!(PciCommandWord & CMD_BUS_MASTER))
            {
                DEBUGSTR(("Enable master -- Command word = %x\n",PciCommandWord | CMD_BUS_MASTER));
                PciCommandWord |= CMD_BUS_MASTER;

                NdisWritePciSlotInformation(
                    Adapter->D100AdapterHandle,
                    Slot,
                    PCI_COMMAND_REGISTER,
                    &PciCommandWord,
                    0x2);

                NdisReadPciSlotInformation(
                    Adapter->D100AdapterHandle,
                    Slot,
                    PCI_COMMAND_REGISTER,
                    &PciCommandWord,
                    0x2);

                DEBUGSTR(("PCI command word now = %x\n",PciCommandWord));

            }
            else
            {
                DEBUGSTR(("Bus master enabled, no change to config -- Command word = %x\n",PciCommandWord));
            }

            // Read our node address from the EEPROM.  We Must use
            // I/O do this, because have not yet memory mapped the
            // CSR with the NdisMMapIoSpace call.
            for (i=0; i<6; i += 2)
            {
                EepromWordValue = ReadEEprom(Adapter,
                    (USHORT) (EEPROM_NODE_ADDRESS_BYTE_0 + (i/2)),
                    pPciCardsFound->PciSlotInfo[found].BaseIo);

                DEBUGSTR(("EEPROM word %x reads %x\n",
                    EEPROM_NODE_ADDRESS_BYTE_0 + (i/2), EepromWordValue));

                pPciCardsFound->PciSlotInfo[found].NodeAddress[i] =
                    (UCHAR) EepromWordValue;
                pPciCardsFound->PciSlotInfo[found].NodeAddress[i+1] =
                    (UCHAR) (EepromWordValue >> 8);
            }
            ++found;
        } // End if valid resources
    } // End if D100 adapter
    pPciCardsFound->NumFound = found;
    return found;
}


