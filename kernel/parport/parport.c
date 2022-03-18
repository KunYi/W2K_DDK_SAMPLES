/*++

Copyright (C) Microsoft Corporation, 1993 - 1999

Module Name:

    parport.c

Abstract:

    This module contains the code for the parallel port driver.

    This driver creates a device for each parallel port on the
    system.  It increments 'IoGetConfigurationInformation()->ParallelCount'
    once for each parallel port.  Each device created (\Device\ParallelPort0,
    \DeviceParallelPort1,...) supports a number of internal ioctls that
    allow parallel class drivers to get information about the parallel port and
    to share the parallel port.

    IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO returns the location
    and span of the register set for the parallel port.  This ioctl
    also returns callback functions for 'FreePort', 'TryAllocatePort',
    and 'QueryNumWaiters' (more on these below).  This ioctl
    should be called by a class driver during 'DriverEntry' and the
    information added to the class driver's device extension.

    A parallel class driver should never touch the parallel port
    registers returned by IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO
    without first allocating the port from the parallel port driver.

    The port can be allocated from IRQL <= DISPATCH_LEVEL by calling
    IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE, or 'TryAllocatePort'.
    The first call is the simplest:  the IRP will be queued in the
    parallel port driver until the port is free and then will return
    with a successful status.  The class driver may cancel this IRP
    at any time which serves as a mechanism to timeout an allocate
    request.  It is often convenient to use an incomming read or
    write IRP and pass it down to the port driver as an ALLOCATE.  That
    way the class driver may avoid having to do its own queueing.
    The 'TryAllocatePort' call returns immediately from the port
    driver with a TRUE status if the port was allocated or an
    FALSE status if the port was busy.

    Once the port is allocated, it is owned by the allocating class
    driver until a 'FreePort' call is made.  This releases the port
    and wakes up the next caller.

    The 'QueryNumWaiters' call which can be made at IRQL <= DISPATCH_LEVEL
    is useful to check and see if there are other class drivers waiting for the
    port.  In this way, a class driver that needs to hold on to the
    port for an extended period of time (e.g. tape backup) can let
    go of the port if it detects another class driver needing service.

    If a class driver wishes to use the parallel port's interrupt then
    it should connect to this interrupt by calling
    IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT during its DriverEntry
    routine.  Besides giving the port driver an interrupt service routine
    the class driver may optionally give the port driver a deferred
    port check routine.  Such a routine would be called whenever the
    port is left free.  This would allow the class driver to make sure
    that interrupts were turned on whenever the port was left idle.
    If the driver using the interrupt has an Unload routine
    then it should call IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT
    in its Unload routine.

    If a class driver's interrupt service routine is called when this
    class driver does not own the port, the class driver may attempt
    to grap the port quickly if it is free by calling the
    'TryAllocatePortAtInterruptLevel' function.  This function is returned
    when the class driver connects to the interrupt.  The class driver may
    also use the 'FreePortFromInterruptLevel' function to free the port.

    Please refer to the PARSIMP driver code for a template of a simple
    parallel class driver.  This template implements simple allocation and
    freeing of the parallel port on an IRP by IRP basis.  It also
    has optional code for timing out allocation requests and for
    using the parallel port interrupt.

Author:

    Anthony V. Ercolano 1-Aug-1992
    Norbert P. Kusters 18-Oct-1993

Environment:

    Kernel mode

Revision History :

    Timothy T. Wells (v-timtw) 23-Dec-1996

    Major revisions to this file.  Moved the older initialization code to parinit.c.

    In the WDM world we will only add a controller to our list of managed controllers
    when told to do so by the configuration manager (as part of the plug and play
    process).  WDM initialization code is now in this file.


    Don Redford 19-Feb-1998

    Added Support for 1284.3
    Added support for Harware ECP and EPP Generic and Filter driver calls

--*/

#include "pch.h"

#include <initguid.h>
#include <ntddpar.h>
//DEFINE_GUID(GUID_PARALLEL_DEVICE, 0x97F76EF0, 0xF883, 0x11D0, 0xAF, 0x1F, 0x00, 0x00, 0xF8, 0x00, 0x84, 0x5C);
//DEFINE_GUID(GUID_PARCLASS_DEVICE, 0x811FC6A5, 0xF728, 0x11D0, 0xA5, 0x37, 0x00, 0x00, 0xF8, 0x75, 0x3E, 0xD1);

//
// globals and constants
//

// debug and break point info
ULONG PptDebugLevel = PARDUMP_SILENT;
ULONG PptBreakOn    = PAR_BREAK_ON_NOTHING;

UNICODE_STRING RegistryPath = {0,0,0};

// track CREATE/CLOSE count - likely obsolete
LONG        PortInfoReferenceCount  = -1L;
PFAST_MUTEX PortInfoMutex           = NULL;

const PHYSICAL_ADDRESS PhysicalZero = {0};

// variable to know how many times to try a select or
// deselect for 1284.3 if we do not succeed.
UCHAR PptDot3Retries = 5;

