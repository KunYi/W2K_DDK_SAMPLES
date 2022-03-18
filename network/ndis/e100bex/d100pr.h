/****************************************************************************
** COPYRIGHT (C) 1994-1997 INTEL CORPORATION                               **
** DEVELOPED FOR MICROSOFT BY INTEL CORP., HILLSBORO, OREGON               **
** HTTP://WWW.INTEL.COM/                                                   **
** THIS FILE IS PART OF THE INTEL ETHEREXPRESS PRO/100B(TM) AND            **
** ETHEREXPRESS PRO/100+(TM) NDIS 5.0 MINIPORT SAMPLE DRIVER               **
****************************************************************************/

/****************************************************************************
Module Name:
    d100pr.h

This driver runs on the following hardware:
    - 82557/82558 based PCI 10/100Mb ethernet adapters
    (aka Intel EtherExpress(TM) PRO Adapters)

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

Revision History
    - JCB 8/14/97 Example Driver Created
*****************************************************************************/

#ifndef _D100PR_H
#define _D100PR_H

//
// We define the external interfaces to the D100 driver.
// These routines are only external to permit separate
// compilation.  Given a truely fast compiler they could
// all reside in a single file and be static.
//


// =============================================
// Routines in d100.c
// =============================================

VOID
SoftwareReset(
              IN PD100_ADAPTER Adapter
              );

NTSTATUS
DriverEntry(
            IN PDRIVER_OBJECT DriverObject,
            IN PUNICODE_STRING RegistryPath
            );

BOOLEAN
D100CheckForHang(
                 IN NDIS_HANDLE MiniportAdapterContext
                 );

VOID
D100Halt(
         IN  NDIS_HANDLE MiniportAdapterContext
         );

VOID
D100ShutdownHandler(
                    IN  NDIS_HANDLE MiniportAdapterContext
                    );

NDIS_STATUS
D100Initialize(
               OUT PNDIS_STATUS OpenErrorStatus,
               OUT PUINT SelectedMediumIndex,
               IN PNDIS_MEDIUM MediumArray,
               IN UINT MediumArraySize,
               IN NDIS_HANDLE MiniportAdapterHandle,
               IN NDIS_HANDLE WrapperConfigurationContext
               );

NDIS_STATUS
D100Reset(
          OUT PBOOLEAN AddressingReset,
          IN  NDIS_HANDLE MiniportAdapterContext
          );

VOID
D100GetReturnedPackets(
                       IN NDIS_HANDLE  MiniportAdapterContext,
                       IN PNDIS_PACKET Packet
                       );

VOID
InitAndChainPacket(
                   IN OUT PD100_ADAPTER Adapter,
                   IN OUT PD100SwRfd SwRfdPtr
                   );

VOID
D100AllocateComplete(NDIS_HANDLE MiniportAdapterContext,
                     IN PVOID VirtualAddress,
                     IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
                     IN ULONG Length,
                     IN PVOID Context);


VOID
D100ResetComplete(PVOID sysspiff1,
                  NDIS_HANDLE MiniportAdapterContext,
                  PVOID sysspiff2, PVOID sysspiff3);

NDIS_STATUS
AllocateRMD(ReceiveMemoryDescriptor *new,
            UINT count,
            ULONG_PTR Virt,
            NDIS_PHYSICAL_ADDRESS Phys,
            UINT Len);

PD100SwRfd
BuildSwRfd(PD100_ADAPTER Adapter,
           ReceiveMemoryDescriptor *newmem,
           UINT startpoint);

VOID
FreeSwRfd(D100_ADAPTER *adapter,
          D100SwRfd *rfd);

VOID
FreeRMD(D100_ADAPTER *adapter,
        ReceiveMemoryDescriptor *rmd);




// =============================================
// Routines in interrup.c
// =============================================

VOID
D100Isr(
        OUT PBOOLEAN InterruptRecognized,
        OUT PBOOLEAN QueueMiniportHandleInterrupt,
        IN NDIS_HANDLE MiniportAdapterContext
        );

VOID
D100HandleInterrupt(
                    IN NDIS_HANDLE MiniportAdapterContext
                    );

BOOLEAN
StartReceiveUnit(
                 IN PD100_ADAPTER Adapter
                 );

BOOLEAN
ProcessRXInterrupt(
                   IN PD100_ADAPTER Adapter
                   );




// =============================================
// Routines in request.c
// =============================================

NDIS_STATUS
D100ChangeMCAddresses(
                      IN PD100_ADAPTER Adapter,
                      IN UINT AddressCount
                      );


NDIS_STATUS
D100SetInformation(
                   IN NDIS_HANDLE MiniportAdapterContext,
                   IN NDIS_OID Oid,
                   IN PVOID InformationBuffer,
                   IN ULONG InformationBufferLength,
                   OUT PULONG BytesRead,
                   OUT PULONG BytesNeeded
                   );

NDIS_STATUS
D100QueryInformation(
                     IN NDIS_HANDLE MiniportAdapterContext,
                     IN NDIS_OID Oid,
                     IN PVOID InformationBuffer,
                     IN ULONG InformationBufferLength,
                     OUT PULONG BytesWritten,
                     OUT PULONG BytesNeeded
                     );

// =============================================
// Routines in routines.c
// =============================================

VOID
MdiWrite(
         IN PD100_ADAPTER Adapter,
         IN ULONG RegAddress,
         IN ULONG PhyAddress,
         IN USHORT DataValue
         );

VOID
MdiRead(
        IN PD100_ADAPTER Adapter,
        IN ULONG RegAddress,
        IN ULONG PhyAddress,
        IN OUT PUSHORT DataValue
        );

VOID
DumpStatsCounters(
                  IN PD100_ADAPTER Adapter
                  );

VOID
DoBogusMulticast(
                 IN PD100_ADAPTER Adapter
                 );

VOID
D100IssueSelectiveReset(
                        IN PD100_ADAPTER Adapter
                        );

BOOLEAN
D100SubmitCommandBlockAndWait(
                              IN PD100_ADAPTER Adapter
                              );

NDIS_MEDIA_STATE
GetConnectionStatus(
                    IN PD100_ADAPTER Adapter
                    );

// =============================================
// routines in send.c
// =============================================
VOID
D100MultipleSend(
                 IN  NDIS_HANDLE             MiniportAdapterContext,
                 IN  PPNDIS_PACKET           PacketArray,
                 IN  UINT                    NumberOfPackets
                 );

VOID
D100CopyFromPacketToBuffer(
                           IN PD100_ADAPTER Adapter,
                           IN PNDIS_PACKET Packet,
                           IN UINT BytesToCopy,
                           IN PCHAR DestBuffer,
                           IN PNDIS_BUFFER FirstBuffer,
                           OUT PUINT BytesCopied
                           );

BOOLEAN
ProcessTXInterrupt(
                   IN OUT PD100_ADAPTER Adapter
                   );

NDIS_STATUS
SetupNextSend(PD100_ADAPTER Adapter,
              PNDIS_PACKET Packet);

BOOLEAN
PrepareForTransmit(PD100_ADAPTER Adapter,
                   PNDIS_PACKET Packet,
                   PD100SwTcb SwTcb);

BOOLEAN
AcquireCoalesceBuffer(PD100_ADAPTER Adapter,
                      PD100SwTcb SwTcb);


BOOLEAN
TransmitCleanup(PD100_ADAPTER Adapter);

// =============================================
// routines in pci.c
// =============================================

USHORT
FindPciDevice50Scan(
                    IN PD100_ADAPTER Adapter,
                    IN USHORT     VendorID,
                    IN USHORT     DeviceID,
                    OUT PPCI_CARDS_FOUND_STRUC PciCardsFound
                    );

// =============================================
// routines in parse.c
// =============================================

NDIS_STATUS
ParseRegistryParameters(
                        IN PD100_ADAPTER Adapter,
                        IN NDIS_HANDLE ConfigHandle
                        );


// =============================================
// routines in physet.c
// =============================================

BOOLEAN
PhyDetect(
          IN PD100_ADAPTER Adapter
          );

VOID
ResetPhy(
         IN PD100_ADAPTER Adapter
         );

VOID
SelectPhy(
          IN PD100_ADAPTER Adapter,
          IN UINT SelectPhyAddress,
          IN BOOLEAN WaitAutoNeg
          );

BOOLEAN
SetupPhy(
         IN PD100_ADAPTER Adapter
         );

VOID
FindPhySpeedAndDpx(
                   IN PD100_ADAPTER Adapter,
                   IN UINT PhyId
                   );

VOID
ResetPhy(
         IN PD100_ADAPTER Adapter
         );




// =============================================
// routines in eeprom.c
// =============================================

VOID
D100UpdateChecksum(
                   IN PD100_ADAPTER Adapter,
                   IN ULONG CSRBaseIoAddress
                   );

USHORT
ReadEEprom(
           IN PD100_ADAPTER Adapter,
           IN USHORT Reg,
           IN ULONG CSRBaseIoAddress
           );

USHORT
MemReadEEprom(
              IN PD100_ADAPTER Adapter,
              IN USHORT Reg,
              IN PCSR_STRUC CSRVirtAddress
              );

VOID
ShiftOutBits(
             IN PD100_ADAPTER Adapter,
             IN USHORT data,
             IN USHORT count,
             IN ULONG CSRBaseIoAddress
             );

VOID
RaiseClock(
           IN PD100_ADAPTER Adapter,
           IN OUT USHORT *x,
           IN ULONG CSRBaseIoAddress
           );

VOID
LowerClock(
           IN PD100_ADAPTER Adapter,
           IN OUT USHORT *x,
           IN ULONG CSRBaseIoAddress
           );

USHORT
ShiftInBits(
            IN PD100_ADAPTER Adapter,
            IN ULONG CSRBaseIoAddress
            );

VOID
EEpromCleanup(
              IN PD100_ADAPTER Adapter,
              IN ULONG CSRBaseIoAddress
              );

USHORT
MemReadEEprom(
              IN PD100_ADAPTER Adapter,
              IN USHORT Reg,
              IN PCSR_STRUC CSRVirtAddress
              );

VOID
MemShiftOutBits(
                IN PD100_ADAPTER Adapter,
                IN USHORT data,
                IN USHORT count,
                IN PCSR_STRUC CSRVirtAddress
                );

VOID
MemRaiseClock(
              IN PD100_ADAPTER Adapter,
              IN OUT USHORT *x,
              IN PCSR_STRUC CSRVirtAddress
              );

VOID
MemLowerClock(
              IN PD100_ADAPTER Adapter,
              IN OUT USHORT *x,
              IN PCSR_STRUC CSRVirtAddress
              );

USHORT
MemShiftInBits(
               IN PD100_ADAPTER Adapter,
               IN PCSR_STRUC CSRVirtAddress
               );

VOID
MemEEpromCleanup(
                 IN PD100_ADAPTER Adapter,
                 IN PCSR_STRUC CSRVirtAddress
                 );



// =============================================
// routines in init.c
// =============================================

NDIS_STATUS
ClaimAdapter(
             IN OUT PD100_ADAPTER Adapter
             );

NDIS_STATUS
SetupAdapterInfo(
                 IN OUT PD100_ADAPTER Adapter
                 );

NDIS_STATUS
SetupSharedAdapterMemory(
                         IN PD100_ADAPTER Adapter
                         );

NDIS_STATUS
SelfTestHardware(
                 IN PD100_ADAPTER Adapter
                 );

VOID
SetupTransmitQueues(
                    IN OUT PD100_ADAPTER Adapter,
                    IN BOOLEAN DebugPrint
                    );

VOID
SetupReceiveQueues (
                    IN PD100_ADAPTER Adapter
                    );

BOOLEAN
InitializeAdapter(
                  IN PD100_ADAPTER Adapter
                  );

BOOLEAN
InitializeD100(
               IN PD100_ADAPTER Adapter
               );

BOOLEAN
Configure(
          IN PD100_ADAPTER Adapter
          );

BOOLEAN
SetupIAAddress(
               IN PD100_ADAPTER Adapter
               );

VOID
ClearAllCounters(
                 IN PD100_ADAPTER Adapter
                 );



#endif  //_D100PR_H

