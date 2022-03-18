/*++

Copyright (C) Microsoft Corporation, 1993 - 1999

Module Name :

    parport.h

Abstract:

    Type definitions and data for the parallel port driver.

Revision History:

--*/

#if 0
#ifdef  IoCompleteRequest
#undef  IoCompleteRequest
#define IoCompleteRequest PptCompleteRequest
#endif
#endif

#define ASSERT_EVENT(E) {                             \
    ASSERT((E)->Header.Type == NotificationEvent ||   \
           (E)->Header.Type == SynchronizationEvent); \
}

VOID
PptCompleteRequest(
    IN PIRP Irp,
    IN CCHAR PriorityBoost
    );

#ifndef _PARPORT_
#define _PARPORT_

#define PptSetFlags( FlagsVariable, FlagsToSet ) { (FlagsVariable) |= (FlagsToSet); }
#define PptClearFlags( FlagsVariable, FlagsToClear ) { (FlagsVariable) &= ~(FlagsToClear); }

#define MAX_PNP_IRP_MN_HANDLED IRP_MN_QUERY_LEGACY_BUS_INFORMATION

extern ULONG PptDebugLevel;
extern ULONG PptBreakOn;
extern UNICODE_STRING RegistryPath;       // copy of the registry path passed to DriverEntry()

extern UCHAR PptDot3Retries;    // variable to know how many times to try a select or
                                // deselect for 1284.3 if we do not succeed.

extern const PHYSICAL_ADDRESS PhysicalZero;

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'PraP')
#endif

#define PARPORT_POOL_TAG (ULONG) 'PraP'


//
// used for FilterResourceMethod in processing PnP IRP_MN_FILTER_RESOURCE_REQUIREMENTS
//
#define PPT_TRY_USE_NO_IRQ    0 // if alternatives exist that don't require an IRQ then
                                //   delete those alternatives that do, otherwise do nothing
#define PPT_FORCE_USE_NO_IRQ  1 // try previous method - if it fails (i.e., all alternatives 
                                //   require resources), then nuke IRQ resource descriptors 
                                //   in all alternatives
#define PPT_ACCEPT_IRQ        2 // don't do any resource filtering - accept resources that 
                                //   we are given


//
// Keep track of GET and RELEASE port info.
//
extern LONG PortInfoReferenceCount;
extern PFAST_MUTEX PortInfoMutex;

//
// extension->DeviceStateFlags - define the current PnP state of the device
//
#define PPT_DEVICE_STARTED          ((ULONG)0x00000001) // Device has succeeded START

#define PPT_DEVICE_STOP_PENDING     ((ULONG)0x00000010) // Device has succeeded QUERY_STOP, waiting for STOP or CANCEL
#define PPT_DEVICE_STOPPED          ((ULONG)0x00000020) // Device has received STOP

#define PPT_DEVICE_REMOVE_PENDING   ((ULONG)0x00000100) // Device has succeeded QUERY_REMOVE, waiting for REMOVE or CANCEL
#define PPT_DEVICE_REMOVED          ((ULONG)0x00000200) // Device has received REMOVE

#define PPT_DEVICE_SURPRISE_REMOVED ((ULONG)0x00001000) // Device has received SURPRISE_REMOVAL

#define PPT_DEVICE_PAUSED           ((ULONG)0x00010000) // stop-pending, stopped, or remove-pending states - hold requests


typedef struct _DEVICE_EXTENSION {

    // Used to increase our confidence that this is a ParPort extension
    ULONG   ExtensionSignatureBegin; 

    // Device State - See Device State Flags: PPT_DEVICE_... above
    ULONG   DeviceStateFlags;   

    //
    // Points to the device object that contains
    // this device extension.
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // Points to the driver object that contains
    // this instance of parport.
    //
    PDRIVER_OBJECT DriverObject;

    //
    // Points to the PDO
    //
    PDEVICE_OBJECT PhysicalDeviceObject;

    //
    // Points to our parent
    //
    PDEVICE_OBJECT ParentDeviceObject;

    //
    // Keep track of creates versus closes so that we know 
    //   at all times whether anyone has a handle to us
    //
    // Used for QUERY_REMOVE decision, Succeed QUERY_REMOVE 
    //   if this count is zero, otherwise fail QUERY_REMOVE 
    //
    LONG OpenCloseRefCount;

    //
    // Queue of DeviceObjects to report to PnP for
    //   RemovalRelations Query
    //
    LIST_ENTRY RemovalRelationsList;

    //
    // Queue of irps waiting to be processed.  Access with
    // cancel spin lock.
    //
    LIST_ENTRY WorkQueue;

    //
    // The number of irps in the queue where -1 represents
    // a free port, 0 represents an allocated port with
    // zero waiters, 1 represents an allocated port with
    // 1 waiter, etc...
    //
    // This variable must be accessed with the cancel spin
    // lock or at interrupt level whenever interrupts are
    // being used.
    //
    LONG WorkQueueCount;

    //
    // This structure holds the port address and range for the
    // parallel port.
    //
    PARALLEL_PORT_INFORMATION PortInfo;
    PARALLEL_PNP_INFORMATION PnpInfo;

    //
    // Information about the interrupt so that we
    // can connect to it when we have a client that
    // uses the interrupt.
    //
    ULONG AddressSpace;
    ULONG EcpAddressSpace;

    INTERFACE_TYPE InterfaceType;
    ULONG BusNumber;

    BOOLEAN FoundInterrupt;
    KIRQL InterruptLevel;
    ULONG InterruptVector;
    KAFFINITY InterruptAffinity;
    KINTERRUPT_MODE InterruptMode;

    //
    // Information about the DMA channel used by this parallel
    // port device.
    //
    ULONG DmaChannel;
    ULONG DmaPort;
    USHORT DmaWidth;

    //
    // This list contains all of the interrupt service
    // routines registered by class drivers.  All access
    // to this list should be done at interrupt level.
    //
    // This list also contains all of the deferred port check
    // routines.  These routines are called whenever
    // the port is freed if there are no IRPs queued for
    // the port.  Access this list only at interrupt level.
    //
    LIST_ENTRY IsrList;

    //
    // The parallel port interrupt object.
    //
    PKINTERRUPT InterruptObject;

    //
    // Keep a reference count for the interrupt object.
    // This count should be referenced with the cancel
    // spin lock.
    //
    ULONG InterruptRefCount;

    //
    // DPC for freeing the port from the interrupt routine.
    //
    KDPC FreePortDpc;

    //
    // Set at initialization to indicate that on the current
    // architecture we need to unmap the base register address
    // when we unload the driver.
    //
    BOOLEAN UnMapRegisters;

    //
    // Flags for ECP and EPP detection and changing of the modes
    //
    BOOLEAN NationalChecked;
    BOOLEAN NationalChipFound;
    BOOLEAN FilterMode;
    UCHAR EcrPortData;
    
    //
    // Structure that hold information from the Chip Filter Driver
    //
    PARALLEL_PARCHIP_INFO   ChipInfo;    

    UNICODE_STRING DeviceName;
    UNICODE_STRING SymbolicLinkName;

    //
    // Current Device Power State
    //
    DEVICE_POWER_STATE DeviceState;
    SYSTEM_POWER_STATE SystemState;

    IO_REMOVE_LOCK RemoveLock;

    FAST_MUTEX     ExtensionFastMutex;
    FAST_MUTEX     OpenCloseMutex;

    WMILIB_CONTEXT                WmiLibContext;
    PARPORT_WMI_ALLOC_FREE_COUNTS WmiPortAllocFreeCounts;

    BOOLEAN CheckedForGenericEpp; // did we check for Generic (via the ECR) EPP capability?
    BOOLEAN spare[3];

    // Used to increase our confidence that this is a ParPort extension
    ULONG   ExtensionSignatureEnd; 

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _SYNCHRONIZED_COUNT_CONTEXT {
    PLONG   Count;
    LONG    NewCount;
} SYNCHRONIZED_COUNT_CONTEXT, *PSYNCHRONIZED_COUNT_CONTEXT;

typedef struct _SYNCHRONIZED_LIST_CONTEXT {
    PLIST_ENTRY List;
    PLIST_ENTRY NewEntry;
} SYNCHRONIZED_LIST_CONTEXT, *PSYNCHRONIZED_LIST_CONTEXT;

typedef struct _SYNCHRONIZED_DISCONNECT_CONTEXT {
    PDEVICE_EXTENSION                   Extension;
    PPARALLEL_INTERRUPT_SERVICE_ROUTINE IsrInfo;
} SYNCHRONIZED_DISCONNECT_CONTEXT, *PSYNCHRONIZED_DISCONNECT_CONTEXT;

typedef struct _ISR_LIST_ENTRY {
    LIST_ENTRY                  ListEntry;
    PKSERVICE_ROUTINE           ServiceRoutine;
    PVOID                       ServiceContext;
    PPARALLEL_DEFERRED_ROUTINE  DeferredPortCheckRoutine;
    PVOID                       CheckContext;
} ISR_LIST_ENTRY, *PISR_LIST_ENTRY;

typedef struct _REMOVAL_RELATIONS_LIST_ENTRY {
    LIST_ENTRY                  ListEntry;
    PDEVICE_OBJECT              DeviceObject;
    ULONG                       Flags;
    UNICODE_STRING              DeviceName;
} REMOVAL_RELATIONS_LIST_ENTRY, *PREMOVAL_RELATIONS_LIST_ENTRY;

#endif // _PARPORT_

