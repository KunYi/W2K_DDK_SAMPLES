//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1998 - 1999
//
//  File:       power.c
//
//--------------------------------------------------------------------------

#include "pch.h"

NTSTATUS
PptPowerComplete (
                  IN PDEVICE_OBJECT       pDeviceObject,
                  IN PIRP                 pIrp,
                  IN PDEVICE_EXTENSION    Extension
                  )

/*++
      
Routine Description:
      
    This routine handles all IRP_MJ_POWER IRPs.
  
Arguments:
  
    pDeviceObject           - represents the port device
  
    pIrp                    - PNP irp
  
    Extension               - Device Extension
  
Return Value:
  
    Status
  
--*/
{
    POWER_STATE_TYPE    powerType;
    POWER_STATE         powerState;
    PIO_STACK_LOCATION  pIrpStack;
    
    UNREFERENCED_PARAMETER( pDeviceObject );

    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    
    powerType = pIrpStack->Parameters.Power.Type;
    powerState = pIrpStack->Parameters.Power.State;
    
    switch (pIrpStack->MinorFunction) {
        
    case IRP_MN_QUERY_POWER:
        
        ASSERTMSG ("Invalid power completion minor code: Query Power\n", FALSE);
        break;
        
    case IRP_MN_SET_POWER:
        
        PptDumpV( ("Power - Setting %s state to %d\n",
                   ( (powerType == SystemPowerState) ?  "System" : "Device" ), powerState.SystemState) );
        
        switch (powerType) {
        case DevicePowerState:
            if (Extension->DeviceState < powerState.DeviceState) {
                //
                // Powering down
                //
                
                ASSERTMSG ("Invalid power completion Device Down\n", FALSE);
                
            } else if (powerState.DeviceState < Extension->DeviceState) {
                //
                // Powering Up
                //
                PoSetPowerState (Extension->DeviceObject, powerType, powerState);
                
                if (PowerDeviceD0 == Extension->DeviceState) {
                    
                    //
                    // Do the power on stuff here.
                    //
                    
                }
                Extension->DeviceState = powerState.DeviceState;
            }
            break;
            
        case SystemPowerState:
            
            if (Extension->SystemState < powerState.SystemState) {
                //
                // Powering down
                //
                
                ASSERTMSG ("Invalid power completion System Down\n", FALSE);
                
            } else if (powerState.SystemState < Extension->SystemState) {
                //
                // Powering Up
                //
                if (PowerSystemWorking == powerState.SystemState) {
                    
                    //
                    // Do the system start up stuff here.
                    //
                    
                    powerState.DeviceState = PowerDeviceD0;
                    PoRequestPowerIrp (Extension->DeviceObject,
                                       IRP_MN_SET_POWER,
                                       powerState,
                                       NULL, // no completion function
                                       NULL, // and no context
                                       NULL);
                }
                
                Extension->SystemState = powerState.SystemState;
            }
            break;
        }
        
        
        break;
        
    default:
        ASSERTMSG ("Power Complete: Bad Power State", FALSE);
    }
    
    PoStartNextPowerIrp (pIrp);
    
    return STATUS_SUCCESS;
}

VOID
InitNEC_98(
    PDEVICE_EXTENSION Extension
    )
/*++

Routine Description:

    Is the hardware mode change for NEC98.
    Change to the ATmode at the NEC98 PARALLEL controller.

Arguments:

    Extension - The device extension of pointer.

Return Value:

    non

--*/
{
//
// I/O port offset by specific for NEC_98
//
#define TOKI_EXTRA   0x0E
#define TOKI_PRCTR   0x02
#define TOKI_STS     0x09

#define TOKI_AT_MODE 0x10 // Set ATmode for TOKI-Control
#define TOKI_AT      0x00 // Set ATmode for TOKI-Extended Control
#define TOKI_CTRMSK  0x04 // Set InputPrime(Doesn't init of printer)
PUCHAR  Controller = Extension->PortInfo.Controller;

    WRITE_PORT_UCHAR(Controller + TOKI_STS, TOKI_AT_MODE);
    WRITE_PORT_UCHAR(Controller + TOKI_EXTRA, TOKI_AT);
    WRITE_PORT_UCHAR(Controller + TOKI_PRCTR, TOKI_CTRMSK);

}


NTSTATUS
PptDispatchPower (
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
    )
/*++
      
Routine Description:
      
    This routine handles all IRP_MJ_POWER IRPs.
      
Arguments:
      
    pDeviceObject           - represents the port device
      
    pIrp                    - PNP irp
      
Return Value:
      
    Status
      
--*/
{
    POWER_STATE_TYPE    powerType;
    POWER_STATE         powerState;
    PIO_STACK_LOCATION  pIrpStack;
    NTSTATUS            status = STATUS_SUCCESS;
    PDEVICE_EXTENSION   Extension;
    BOOLEAN             hookit = FALSE;
    
    //
    // WORKWORK.  THIS CODE DOESN'T DO MUCH...NEED TO CHECK OUT FULL POWER FUNCTIONALITY.
    //
    
    Extension = pDeviceObject->DeviceExtension;
    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    
    {
        NTSTATUS status = PptAcquireRemoveLock(&Extension->RemoveLock, pIrp);
        if (!NT_SUCCESS(status)) {
            PoStartNextPowerIrp(pIrp);
            pIrp->IoStatus.Status = status;
            PptCompleteRequest(pIrp, IO_NO_INCREMENT);
            return status;
        }
    }
   
    powerType = pIrpStack->Parameters.Power.Type;
    powerState = pIrpStack->Parameters.Power.State;
    
    switch (pIrpStack->MinorFunction) {
        
    case IRP_MN_QUERY_POWER:
        
        status = STATUS_SUCCESS;
        break;
        
    case IRP_MN_SET_POWER:
        
        PptDumpV( ("Power - Setting %s state to %d\n",
                   ( (powerType == SystemPowerState) ?  "System" : "Device" ), powerState.SystemState) );
        
        switch (powerType) {
        case DevicePowerState:
            if (Extension->DeviceState < powerState.DeviceState) {
                //
                // Powering down
                //
                
                PoSetPowerState (Extension->DeviceObject, powerType, powerState);
                
                if (PowerDeviceD0 == Extension->DeviceState) {
                    
                    //
                    // Do the power on stuff here.
                    //
                    
                }
                Extension->DeviceState = powerState.DeviceState;
                
            } else if (powerState.DeviceState < Extension->DeviceState) {
                //
                // Powering Up
                //
                hookit = TRUE;

                //
                // Change to the AT mode for NEC_98.
                // NEC_98 has NEC_98 mode of the PARALLEL at the default, so we should change
                // to the AT mode at the PowerUP state for NEC_98.
                //
                if (IsNEC_98) {
                    InitNEC_98(Extension);
                }


            }
            
            break;
            
        case SystemPowerState:
            
            if (Extension->SystemState < powerState.SystemState) {
                //
                // Powering down
                //
                if (PowerSystemWorking == Extension->SystemState) {
                    
                    //
                    // Do the system shut down stuff here.
                    //
                    
                }
                
                powerState.DeviceState = PowerDeviceD3;
                PoRequestPowerIrp (Extension->DeviceObject,
                                   IRP_MN_SET_POWER,
                                   powerState,
                                   NULL, // no completion function
                                   NULL, // and no context
                                   NULL);
                Extension->SystemState = powerState.SystemState;
                
            } else if (powerState.SystemState < Extension->SystemState) {
                //
                // Powering Up
                //
                hookit = TRUE;
            }
            break;
        }
        
        break;
        
    default:
        
        status = STATUS_NOT_SUPPORTED;
    }
    
    IoCopyCurrentIrpStackLocationToNext (pIrp);
    
    if (!NT_SUCCESS (status)) {
        pIrp->IoStatus.Status = status;
        PoStartNextPowerIrp (pIrp);
        PptCompleteRequest (pIrp, IO_NO_INCREMENT);
        
    } else if (hookit) {
        
        IoSetCompletionRoutine (pIrp,
                                PptPowerComplete,
                                Extension,
                                TRUE,
                                TRUE,
                                TRUE);
        
        status = PoCallDriver (Extension->ParentDeviceObject, pIrp);
        
    } else {
        PoStartNextPowerIrp (pIrp);
        status = PoCallDriver (Extension->ParentDeviceObject, pIrp);
    }
    
    PptReleaseRemoveLock(&Extension->RemoveLock, pIrp);
    return status;
}

