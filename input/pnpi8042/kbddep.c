/*++

Copyright (c) 1990-1998 Microsoft Corporation, All Rights Reserved

Module Name:

    kbddep.c

Abstract:

    The initialization and hardware-dependent portions of
    the Intel i8042 port driver which are specific to the
    keyboard.

Environment:

    Kernel mode only.

Notes:

    NOTES:  (Future/outstanding issues)

    - Powerfail not implemented.

    - Consolidate duplicate code, where possible and appropriate.

Revision History:

--*/

#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include <ntddk.h>
#include <windef.h>
#include <imm.h>
#include "i8042prt.h"
#include "i8042log.h"
//
// Use the alloc_text pragma to specify the driver initialization routines
// (they can be paged out).
//
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, I8xKeyboardConfiguration)
#pragma alloc_text(PAGE, I8xInitializeKeyboard)
#pragma alloc_text(PAGE, I8xKeyboardServiceParameters)

#if defined(NEC_98)
#pragma alloc_text(PAGE,NEC98_KeyboardDetection)
#pragma alloc_text(PAGE,NEC98_GetKeyboardSpecificData)
#endif // defined(NEC_98)

#endif

#define BUFFER_FULL   (OUTPUT_BUFFER_FULL|MOUSE_OUTPUT_BUFFER_FULL)

BOOLEAN
I8042KeyboardInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine performs the actual work.  It either processes a keystroke or
    the results from a write to the device.

Arguments:

    CallIsrContext - Contains the interrupt object and device object.  
    
Return Value:

    TRUE if the interrupt was truly ours
    
--*/
{
    UCHAR scanCode, statusByte;
    PPORT_KEYBOARD_EXTENSION deviceExtension;
    KEYBOARD_SCAN_STATE *scanState;
    PKEYBOARD_INPUT_DATA input;
    ULONG i;
#ifdef FE_SB
    PKEYBOARD_ID KeyboardId;
#endif

    IsrPrint(DBG_KBISR_TRACE, ("enter\n"));

    //
    // Get the device extension.
    //
    deviceExtension = (PPORT_KEYBOARD_EXTENSION) DeviceObject->DeviceExtension;

    //
    // The interrupt will fire when we try to toggle the interrupts on the 
    // controller itself.  Don't touch any of the ports in this state and the
    // toggle will succeed.
    //
    if (deviceExtension->PowerState != PowerDeviceD0) {
        return FALSE;
    }

#ifdef FE_SB
    //
    // Get a pointer to keyboard id.
    //
    KeyboardId = &deviceExtension->KeyboardAttributes.KeyboardIdentifier;
#endif

    //
    // Verify that this device really interrupted.  Check the status
    // register.  The Output Buffer Full bit should be set, and the
    // Auxiliary Device Output Buffer Full bit should be clear.
    //

#if defined(NEC_98)
    for (i = 0; i < Globals.ControllerData->Configuration.ResendIterations; i++) {
        statusByte = I8X_GET_STATUS_BYTE(Globals.ControllerData->DeviceRegisters[CommandPort]);
        if (statusByte & PC98_8251_DATA_ERROR) {
            I8X_PUT_COMMAND_BYTE(
                Globals.ControllerData->DeviceRegisters[CommandPort],
                (UCHAR) PC98_KB_RETRY_COMMAND
                );
            //
            // Get rid of stale data.
            //
            I8xDrainOutputBuffer(
                Globals.ControllerData->DeviceRegisters[DataPort],
                Globals.ControllerData->DeviceRegisters[CommandPort]
                );
            IsrPrint(DBG_KBISR_ERROR,
                  ("Error, retries = %d\n",
                  Globals.ControllerData->Configuration.ResendIterations + 1
                  ));
        }
        else {
            break;
        }
    }

    if (i >= (ULONG)Globals.ControllerData->Configuration.ResendIterations) {
        IsrPrint(DBG_KBISR_INFO,
              ("time for retrying is out\n"
              ));
        //
        // Reset 8251 Controler
        //
        NEC98_KeyboardReset();
        NEC98_EnableExtKeys();

        return FALSE;
    }

    I8X_PUT_COMMAND_BYTE(
        Globals.ControllerData->DeviceRegisters[CommandPort],
        (UCHAR) PC98_KB_NON_RETRY_COMMAND
        );
#else // defined(NEC_98)
    statusByte =
      I8X_GET_STATUS_BYTE(Globals.ControllerData->DeviceRegisters[CommandPort]);
    if ((statusByte & BUFFER_FULL) != OUTPUT_BUFFER_FULL) {

        //
        // Stall and then try again.  The Olivetti MIPS machine
        // sometimes gets an interrupt before the status
        // register is set.  They do this for DOS compatibility (some
        // DOS apps do things in polled mode, until they see a character
        // in the keyboard buffer at which point they expect to get
        // an interrupt???).
        //

        for (i = 0; i < (ULONG)Globals.ControllerData->Configuration.PollStatusIterations; i++) {
            KeStallExecutionProcessor(1);
            statusByte = I8X_GET_STATUS_BYTE(Globals.ControllerData->DeviceRegisters[CommandPort]);
            if ((statusByte & BUFFER_FULL) == (OUTPUT_BUFFER_FULL)) {
                break;
            }
        }

        statusByte = I8X_GET_STATUS_BYTE(Globals.ControllerData->DeviceRegisters[CommandPort]);
        if ((statusByte & BUFFER_FULL) != (OUTPUT_BUFFER_FULL)) {

            //
            // Not our interrupt.
            //
            // NOTE:  If the keyboard has not yet been "enabled", go ahead
            //        and read a byte from the data port anyway.
            //        This fixes weirdness on some Gateway machines, where
            //        we get an interrupt sometime during driver initialization
            //        after the interrupt is connected, but the output buffer
            //        full bit never gets set.
            //

            IsrPrint(DBG_KBISR_ERROR|DBG_KBISR_INFO, ("not our interrupt!\n"));

            if (deviceExtension->EnableCount == 0) {
                scanCode =
                    I8X_GET_DATA_BYTE(Globals.ControllerData->DeviceRegisters[DataPort]);
            }

            return FALSE;
        }
    }
#endif // defined(NEC_98)

    //
    // The interrupt is valid.  Read the byte from the i8042 data port.
    //

    I8xGetByteAsynchronous(
        (CCHAR) KeyboardDeviceType,
        &scanCode
        );

    IsrPrint(DBG_KBISR_SCODE, ("scanCode 0x%x\n", scanCode));

    if (deviceExtension->IsrHookCallback) {
        BOOLEAN cont = FALSE, ret;

        ret = (*deviceExtension->IsrHookCallback)(
                  deviceExtension->HookContext,
                  &deviceExtension->CurrentInput,
                  &deviceExtension->CurrentOutput,
                  statusByte,
                  &scanCode,
                  &cont,
                  &deviceExtension->CurrentScanState
                  );

        if (!cont) {
            return ret;
        }
    }       

    //
    // Take the appropriate action, depending on whether the byte read
    // is a keyboard command response or a real scan code.
    //

    switch(scanCode) {

        //
        // The keyboard controller requests a resend.  If the resend count
        // has not been exceeded, re-initiate the I/O operation.
        //

        case RESEND:

            IsrPrint(DBG_KBISR_INFO,
                  (" RESEND, retries = %d\n",
                  deviceExtension->ResendCount + 1
                  ));

            //
            // If the timer count is zero, don't process the interrupt
            // further.  The timeout routine will complete this request.
            //

            if (Globals.ControllerData->TimerCount == 0) {
                break;
            }

            //
            // Reset the timeout value to indicate no timeout.
            //

            Globals.ControllerData->TimerCount = I8042_ASYNC_NO_TIMEOUT;

            //
            // If the maximum number of retries has not been exceeded,
            //

            if ((deviceExtension->CurrentOutput.State == Idle)
                || (DeviceObject->CurrentIrp == NULL)) {

                //
                // We weren't sending a command or parameter to the hardware.
                // This must be a scan code.  I hear the Brazilian keyboard
                // actually uses this.
                //

                goto ScanCodeCase;

            } else if (deviceExtension->ResendCount
                       < Globals.ControllerData->Configuration.ResendIterations) {

                //
                // retard the byte count to resend the last byte
                //
                deviceExtension->CurrentOutput.CurrentByte -= 1;
                deviceExtension->ResendCount += 1;
                I8xInitiateIo(DeviceObject);

            } else {

                deviceExtension->CurrentOutput.State = Idle;

                KeInsertQueueDpc(
                    &deviceExtension->RetriesExceededDpc,
                    DeviceObject->CurrentIrp,
                    NULL
                    );
            }

            break;

        //
        // The keyboard controller has acknowledged a previous send.
        // If there are more bytes to send for the current packet, initiate
        // the next send operation.  Otherwise, queue the completion DPC.
        //

        case ACKNOWLEDGE:

            IsrPrint(DBG_KBISR_STATE, (": ACK, "));

            //
            // If the timer count is zero, don't process the interrupt
            // further.  The timeout routine will complete this request.
            //

            if (Globals.ControllerData->TimerCount == 0) {
                break;
            }

            //
            // If the E0 or E1 is set, that means that this keyboard's
            // manufacturer made a poor choice for a scan code, 0x7A, whose 
            // break code is 0xFA.  Thankfully, they used the E0 or E1 prefix
            // so we can tell the difference.
            //
            if (deviceExtension->CurrentInput.Flags & (KEY_E0 | KEY_E1)) {

                //
                // Make sure we are trully not writing out data to the device
                //
                ASSERT(Globals.ControllerData->TimerCount == I8042_ASYNC_NO_TIMEOUT);
                ASSERT(deviceExtension->CurrentOutput.State == Idle);

                goto ScanCodeCase;
            }

            //
            // Reset the timeout value to indicate no timeout.
            //

            Globals.ControllerData->TimerCount = I8042_ASYNC_NO_TIMEOUT;

            //
            // Reset resend count.
            //

            deviceExtension->ResendCount = 0;
            if (deviceExtension->CurrentOutput.CurrentByte <
                deviceExtension->CurrentOutput.ByteCount) {
                //
                // We've successfully sent the first byte of a 2-byte
                // command sequence.  Initiate a send of the second byte.
                //

                IsrPrint(DBG_KBISR_STATE,
                      ("now initiate send of byte #%d\n",
                       deviceExtension->CurrentOutput.CurrentByte
                      ));

                I8xInitiateIo(DeviceObject);
            }
            else {
                //
                // We've successfully sent all bytes in the command sequence.
                // Reset the current state and queue the completion DPC.
                //

                IsrPrint(DBG_KBISR_STATE,
                      ("all bytes have been sent\n"
                      ));

                deviceExtension->CurrentOutput.State = Idle;

                ASSERT(DeviceObject->CurrentIrp != NULL);

                IoRequestDpc(
                    DeviceObject,
                    DeviceObject->CurrentIrp,
                    (PVOID) IsrDpcCauseKeyboardWriteComplete
                    );
            }
            break;

        //
        // Assume we've got a real, live scan code (or perhaps a keyboard
        // overrun code, which we treat like a scan code).  I.e., a key
        // has been pressed or released.  Queue the ISR DPC to process
        // a complete scan code sequence.
        //

        ScanCodeCase:
        default:

            IsrPrint(DBG_KBISR_SCODE, ("real scan code\n"));

            //
            // Differentiate between an extended key sequence (first
            // byte is E0, followed by a normal make or break byte), or
            // a normal make code (one byte, the high bit is NOT set),
            // or a normal break code (one byte, same as the make code
            // but the high bit is set), or the key #126 byte sequence
            // (requires special handling -- sequence is E11D459DC5).
            //
            // If there is a key detection error/overrun, the keyboard
            // sends an overrun indicator (0xFF in scan code set 1).
            // Map it to the overrun indicator expected by the Windows
            // USER Raw Input Thread.
            //

            input = &deviceExtension->CurrentInput;
            scanState = &deviceExtension->CurrentScanState;

            if (scanCode == (UCHAR) 0xFF) {
#if defined(NEC_98)
                return FALSE;
#else // defined(NEC_98)
                IsrPrint(DBG_KBISR_ERROR, ("OVERRUN\n"));
                input->MakeCode = KEYBOARD_OVERRUN_MAKE_CODE;
                input->Flags = 0;
                *scanState = Normal;
#endif // defined(NEC_98)
            } else {

#if defined(NEC_98)
                //
                // Translate NEC98 H/W scancode -> 106 keyboard scancode
                //
                if ((deviceExtension->ScancodeProc)(&scanCode, &input->Flags) != STATUS_SUCCESS) {
                    break;
                }
                if (input->Flags & KEY_E0) {
                    *scanState = GotE0;
                }
#endif // defined(NEC_98)
                switch (*scanState) {
                  case Normal:
                    if (scanCode == (UCHAR) 0xE0) {
                        input->Flags |= KEY_E0;
                        *scanState = GotE0;
                        IsrPrint(DBG_KBISR_STATE, ("change state to GotE0\n"));
                        break;
                    } else if (scanCode == (UCHAR) 0xE1) {
                        input->Flags |= KEY_E1;
                        *scanState = GotE1;
                        IsrPrint(DBG_KBISR_STATE, ("change state to GotE1\n"));
                        break;
                    }

                    //
                    // Fall through to the GotE0/GotE1 case for the rest of the
                    // Normal case.
                    //

                  case GotE0:
                  case GotE1:

// 
// Enabled for all builds
// 
// #if defined(FE_SB) && defined(_X86_)
                    if(deviceExtension->Dump1Keys != 0) {
                        LONG Dump1Keys;
                        UCHAR DumpKey,DumpKey2;
                        BOOLEAN onflag;
                        static UCHAR KeyToScanTbl[134] = {
                            0x00,0x29,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
                            0x0A,0x0B,0x0C,0x0D,0x7D,0x0E,0x0F,0x10,0x11,0x12,
                            0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x00,
                            0x3A,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
                            0x27,0x28,0x2B,0x1C,0x2A,0x00,0x2C,0x2D,0x2E,0x2F,
                            0x30,0x31,0x32,0x33,0x34,0x35,0x73,0x36,0x1D,0x00,
                            0x38,0x39,0xB8,0x00,0x9D,0x00,0x00,0x00,0x00,0x00,
                            0x00,0x00,0x00,0x00,0x00,0xD2,0xD3,0x00,0x00,0xCB,
                            0xC7,0xCF,0x00,0xC8,0xD0,0xC9,0xD1,0x00,0x00,0xCD,
                            0x45,0x47,0x4B,0x4F,0x00,0xB5,0x48,0x4C,0x50,0x52,
                            0x37,0x49,0x4D,0x51,0x53,0x4A,0x4E,0x00,0x9C,0x00,
                            0x01,0x00,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,
                            0x43,0x44,0x57,0x58,0x00,0x46,0x00,0x00,0x00,0x00,
                            0x00,0x7B,0x79,0x70 };

                        Dump1Keys = deviceExtension->Dump1Keys;
                        switch(deviceExtension->Dump2Key) {
                            case 124:           // 'Print Screen'
                                DumpKey = 0xB7;
                                DumpKey2 = 0x54;
                                break;
                            default:
                                if(deviceExtension->Dump2Key <= 133)
                                    DumpKey = KeyToScanTbl[deviceExtension->Dump2Key];
                                else
                                    DumpKey = 0;
                                DumpKey2 = 0;
                                break;
                        }
                        if(scanCode <= (UCHAR) 0x7F) {
                            //
                            // make code
                            //
                            switch(scanCode) {
                                case 0x1D:          // 'CTRL'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags |= 0x20;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags |= 0x02;
                                    break;
                                case 0x38:          // 'ALT'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags |= 0x40;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags |= 0x04;
                                    break;
                                case 0x36:          // Right 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags |= 0x01;
                                    break;
                                case 0x2A:          // Left 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags |= 0x10;
                                    break;
                                default:
                                    if((DumpKey & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey == scanCode)
                                            break;
                                    } else {
                                        if(*scanState == GotE0
                                         && (DumpKey & 0x7F) == scanCode)
                                            break;
                                    }
                                    if((DumpKey2 & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey2 == scanCode)
                                            break;
                                    } else {
                                        if(*scanState == GotE0
                                         && (DumpKey2 & 0x7F) == scanCode)
                                            break;
                                    }
                                    deviceExtension->DumpFlags = 0;
                                    break;
                            }
                        } else {
                            //
                            // break code
                            //
                            switch(scanCode & 0x7F) {
                                case 0x1D:          // 'CTRL'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags &= ~0x320;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags &= ~0x302;
                                    break;
                                case 0x38:          // 'ALT'
                                    if(*scanState == Normal)     // Left
                                        deviceExtension->DumpFlags &= ~0x340;
                                    else if(*scanState == GotE0) // Right
                                        deviceExtension->DumpFlags &= ~0x304;
                                    break;
                                case 0x36:          // Right 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags &= ~0x301;
                                    break;
                                case 0x2A:          // Left 'Shift'
                                    if(*scanState == Normal)
                                        deviceExtension->DumpFlags &= ~0x310;
                                    break;
                                default:
                                    onflag = 0;
                                    if((DumpKey & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey == (scanCode & 0x7F))
                                            onflag = 1;
                                    } else {
                                        if(*scanState == GotE0
                                         && DumpKey == scanCode)
                                            onflag = 1;
                                    }
                                    if((DumpKey2 & 0x80) == 0) {
                                        if(*scanState == Normal
                                         && DumpKey2 == (scanCode & 0x7F))
                                            onflag = 1;
                                    } else {
                                        if(*scanState == GotE0
                                         && DumpKey2 == scanCode)
                                            onflag = 1;
                                    }
                                    if(onflag) {
                                        if((deviceExtension->DumpFlags & Dump1Keys) != Dump1Keys)
                                            break;
                                        if(deviceExtension->DumpFlags & 0x100)
                                           deviceExtension->DumpFlags |= 0x200;
                                        else
                                           deviceExtension->DumpFlags |= 0x100;
                                        break;
                                    }
                                    deviceExtension->DumpFlags = 0;
                                    break;
                                }
                            }

                            Dump1Keys |= 0x300;
                            if(deviceExtension->DumpFlags == Dump1Keys) {
                                deviceExtension->DumpFlags = 0;
                                KeBugCheckEx(MANUALLY_INITIATED_CRASH,0,0,0,0);
                                                  // make occured blue screen
                            }
                    }
// #endif // defiend(FE_SB) && defined(_X86_)

                    if (scanCode > 0x7F) {
                        PIRP irp;
                        SYS_BUTTON_ACTION action;

                        //
                        // Got a break code.  Strip the high bit off
                        // to get the associated make code and set flags
                        // to indicate a break code.
                        //

                        IsrPrint(DBG_KBISR_SCODE, ("BREAK code\n"));

                        input->MakeCode = scanCode & 0x7F;
                        input->Flags |= KEY_BREAK;

                        if (input->Flags & KEY_E0) {
                            switch (input->MakeCode) {
                            case KEYBOARD_POWER_CODE:
                                if (deviceExtension->PowerCaps &
                                        I8042_POWER_SYS_BUTTON) {
                                    IsrPrint(DBG_KBISR_POWER, ("Send Power Button\n"));
                                    action = SendAction;
                                }
                                else {
                                    IsrPrint(DBG_KBISR_POWER, ("Update Power Button\n"));
                                    action = UpdateAction;
                                }
                                break;
    
                            case KEYBOARD_SLEEP_CODE:
                                if (deviceExtension->PowerCaps &
                                        I8042_SLEEP_SYS_BUTTON) {
                                    IsrPrint(DBG_KBISR_POWER, ("Send Sleep Button\n"));
                                    action = SendAction;
                                }
                                else {
                                    IsrPrint(DBG_KBISR_POWER, ("Update Sleep Button\n"));
                                    action = UpdateAction;
                                }
                                break;
    
                            case KEYBOARD_WAKE_CODE:
                                if (deviceExtension->PowerCaps &
                                        I8042_WAKE_SYS_BUTTON) {
                                    IsrPrint(DBG_KBISR_POWER, ("Send Wake Button\n"));
                                    action = SendAction;
                                }
                                else {
                                    IsrPrint(DBG_KBISR_POWER, ("Update Wake Button\n"));
                                    action = UpdateAction;
                                }
                                break;
    
                            default:
                                action = NoAction;
                                break;
                            }
    
                            if (action != NoAction) {
                                //
                                // Queue a DPC so that we can do the appropriate
                                // action
                                //
                                KeInsertQueueDpc(
                                    &deviceExtension->SysButtonEventDpc,
                                    (PVOID) action,
                                    (PVOID) input->MakeCode
                                    );
                            }
                        }

                    } else {

                        //
                        // Got a make code.
                        //

                        IsrPrint(DBG_KBISR_SCODE, ("MAKE code\n"));

                        input->MakeCode = scanCode;

                        //
                        // If the input scan code is debug stop, then drop
                        // into the kernel debugger if it is active.
                        //

                        if ((**((PBOOLEAN *)&KdDebuggerNotPresent))
                                == FALSE && !(input->Flags & KEY_BREAK)) {
                            if (ENHANCED_KEYBOARD(
                                     deviceExtension->KeyboardAttributes.KeyboardIdentifier
                                     )) {
                                //
                                // Enhanced 101 keyboard, SysReq key is 0xE0 0x37.
                                //

                                if ((input->MakeCode == KEYBOARD_DEBUG_HOTKEY_ENH) &&
                                     (input->Flags & KEY_E0)) {
                                    try {
                                        if ((**((PUCHAR *)&KdDebuggerEnabled) != FALSE) &&
                                            Globals.BreakOnSysRq) {
                                            DbgBreakPointWithStatus(DBG_STATUS_SYSRQ);
                                        }

                                    } except(EXCEPTION_EXECUTE_HANDLER) {
                                    }
                                }
                                //
                                // 84-key AT keyboard, SysReq key is 0xE0 0x54.
                                //

                            } else if ((input->MakeCode == KEYBOARD_DEBUG_HOTKEY_AT)) {
                                try {
                                    if ((**((PUCHAR *)&KdDebuggerEnabled) != FALSE)
                                        && Globals.BreakOnSysRq) {
                                            DbgBreakPointWithStatus(DBG_STATUS_SYSRQ);
                                    }

                                } except(EXCEPTION_EXECUTE_HANDLER) {
                                }
                            }
                        }
                    }


                    //
                    // Reset the state to Normal.
                    //

                    *scanState = Normal;
                    break;

                  default:

                    //
                    // Queue a DPC to log an internal driver error.
                    //

                    KeInsertQueueDpc(
                        &deviceExtension->ErrorLogDpc,
                        (PIRP) NULL,
                        (PVOID) (ULONG) I8042_INVALID_ISR_STATE);

                    ASSERT(FALSE);
                    break;
                }
            }

            //
            // In the Normal state, if the keyboard device is enabled,
            // add the data to the InputData queue and queue the ISR DPC.
            //

            if (*scanState == Normal) {

#if defined(NEC_98)
                //
                // Watch the Ctrl Key status.
                //
                if (input->MakeCode == CTRL_KEY) {
                    if (input->Flags & KEY_BREAK) {
                        deviceExtension->CtrlKeyStatus &= (input->Flags & KEY_E0) ? ~0x20 : ~0x02;
                    } else {
                        deviceExtension->CtrlKeyStatus |= (input->Flags & KEY_E0) ? 0x20 : 0x02;
                    }
                }

                if (deviceExtension->KeyboardTypeNEC != PC98_N106KEY) {
                    switch (input->MakeCode) {
                        case CAPS_KEY:    // LED keys generate Make and Break.
                        case KANA_KEY:
                            input->Flags &= ~KEY_BREAK;
                            NEC98_QueueEmulationKey(
                                DeviceObject,
                                input
                                );
                            input->Flags |= KEY_BREAK;
                            break;
                        case STOP_KEY: // Ctrl+Pause Key emuration  by PC9800 STOP key
                            if (input->Flags & KEY_E0) {
                                IsrPrint(DBG_KBISR_BREAK,
                                      ((input->Flags & KEY_BREAK)?
                                      "STOP(Ctrl+C) BREAK\n":
                                      "STOP(Ctrl+C) MAKE\n"
                                      ));
                                input->MakeCode = CTRL_KEY;
                                input->Flags &= ~KEY_E0;
                                NEC98_QueueEmulationKey(
                                    DeviceObject,
                                    input
                                    );
                                input->MakeCode = STOP_KEY;
                                input->Flags |= KEY_E0;
                            }
                            break;
                        case COPY_KEY: // PrintScreen emulation(PC/AT) by COPY
                            if (input->Flags & KEY_E0) {
                                IsrPrint(DBG_KBISR_BREAK,
                                      ((input->Flags & KEY_BREAK)?
                                      "PrintScreen BREAK\n":
                                      "PrintScreen MAKE\n"
                                      ));
                                input->Flags |= KEY_E0;
                                input->MakeCode = SHIFT_KEY;
                                NEC98_QueueEmulationKey(
                                    DeviceObject,
                                    input
                                    );
                                input->MakeCode = PRINTSCREEN_KEY;
                            }
                            break;
                        case VF3_KEY: // NumLock emulation by vf3
                            if (!(input->Flags & KEY_E0)&&(deviceExtension->VfKeyEmulation)) {
                                IsrPrint(DBG_KBISR_BREAK,
                                      ((input->Flags & KEY_BREAK)?
                                      "NumLock BREAK\n":
                                      "NumLock MAKE\n"
                                      ));
                                input->MakeCode = NUMLOCK_KEY;
                            }
                            break;
                        case VF4_KEY: // ScrollLock emulation by vf4
                            if (!(input->Flags & KEY_E0)&&(deviceExtension->VfKeyEmulation)) {
                                IsrPrint(DBG_KBISR_BREAK,
                                      ((input->Flags & KEY_BREAK)?
                                      "ScrollLock BREAK\n":
                                      "ScrollLock MAKE\n"
                                      ));
                                input->MakeCode = SCROLLLOCK_KEY;
                            }
                            break;
                        case VF5_KEY: // ScrollLock emulation by vf5
                            if (!(input->Flags & KEY_E0)&&(deviceExtension->VfKeyEmulation)) {
                                IsrPrint(DBG_KBISR_BREAK,
                                      ((input->Flags & KEY_BREAK)?
                                      "Hankaku/Zenkaku BREAK\n":
                                      "Hankaku/Zenkaku MAKE\n"
                                      ));
                                input->MakeCode = HANKAKU_ZENKAKU_KEY;
                            }
                            break;
                     }
                } else { // only 106 keyboard
                    switch (input->MakeCode) {
                        case PRINTSCREEN_KEY: // PrintScreen(PC/AT) emulation by PrintScreen(PC9800)
                            if (input->Flags & KEY_E0) {
                                IsrPrint(DBG_KBISR_BREAK,
                                      ((input->Flags & KEY_BREAK)?
                                      "PrintScreen BREAK\n":
                                      "PrintScreen MAKE\n"
                                      ));
                                input->Flags |= KEY_E0;
                                input->MakeCode = SHIFT_KEY;
                                NEC98_QueueEmulationKey(
                                    DeviceObject,
                                    input
                                    );
                                input->MakeCode = PRINTSCREEN_KEY;
                            }
                            break;
                        case PAUSE_KEY: // Pause(PC/AT) emuration by Pause(PC9800)
                            if (input->Flags & KEY_E0) {
                                if (!(input->Flags & KEY_BREAK)) {
                                    NEC98_PauseKeyEmulation(
                                        DeviceObject,
                                        input
                                       );
                                } else {
                                    input->Flags = 0;
                                    return TRUE; 
                                }
                            }
                            break;
                    }
                }
#endif //defined(NEC_98)
                I8xQueueCurrentKeyboardInput(DeviceObject);
            }

            break;

    }

    IsrPrint(DBG_KBISR_TRACE, ("exit\n"));

    return TRUE;
}


//
//  The following table is used to convert typematic rate (keys per
//  second) into the value expected by the keyboard.  The index into the
//  array is the number of keys per second.  The resulting value is
//  the bit equate to send to the keyboard.
//

UCHAR
I8xConvertTypematicParameters(
    IN USHORT Rate,
    IN USHORT Delay
    )

/*++

Routine Description:

    This routine converts the typematic rate and delay to the form the
    keyboard expects.

    The byte passed to the keyboard looks like this:

        - bit 7 is zero
        - bits 5 and 6 indicate the delay
        - bits 0-4 indicate the rate

    The delay is equal to 1 plus the binary value of bits 6 and 5,
    multiplied by 250 milliseconds.

    The period (interval from one typematic output to the next) is
    determined by the following equation:

        Period = (8 + A) x (2^B) x 0.00417 seconds
        where
            A = binary value of bits 0-2
            B = binary value of bits 3 and 4


Arguments:

    Rate - Number of keys per second.

    Delay - Number of milliseconds to delay before the key repeat starts.

Return Value:

    The byte to pass to the keyboard.

--*/

{
    UCHAR value;
    UCHAR   TypematicPeriod[] = {
        31,    // 0 keys per second
        31,    // 1 keys per second
        28,    // 2 keys per second, This is really 2.5, needed for NEXUS.
        26,    // 3 keys per second
        23,    // 4 keys per second
        20,    // 5 keys per second
        18,    // 6 keys per second
        17,    // 7 keys per second
        15,    // 8 keys per second
        13,    // 9 keys per second
        12,    // 10 keys per second
        11,    // 11 keys per second
        10,    // 12 keys per second
         9,    // 13 keys per second
         9,    // 14 keys per second
         8,    // 15 keys per second
         7,    // 16 keys per second
         6,    // 17 keys per second
         5,    // 18 keys per second
         4,    // 19 keys per second
         4,    // 20 keys per second
         3,    // 21 keys per second
         3,    // 22 keys per second
         2,    // 23 keys per second
         2,    // 24 keys per second
         1,    // 25 keys per second
         1,    // 26 keys per second
         1     // 27 keys per second
               // > 27 keys per second, use 0
    };

    Print(DBG_CALL_TRACE, ("I8xConvertTypematicParameters: enter\n"));

    //
    // Calculate the delay bits.
    //

    value = (UCHAR) ((Delay / 250) - 1);

    //
    // Put delay bits in the right place.
    //

    value <<= 5;

    //
    // Get the typematic period from the table.  If keys per second
    // is > 27, the typematic period value is zero.
    //

    if (Rate <= 27) {
        value |= TypematicPeriod[Rate];
    }

    Print(DBG_CALL_TRACE, ("I8xConvertTypematicParameters: exit\n"));

    return(value);
}

#define KB_INIT_FAILED_RESET                0x00000001
#define KB_INIT_FAILED_XLATE_OFF            0x00000010
#define KB_INIT_FAILED_XLATE_ON             0x00000020
#define KB_INIT_FAILED_SET_TYPEMATIC        0x00000100
#define KB_INIT_FAILED_SET_TYPEMATIC_PARAM  0x00000200
#define KB_INIT_FAILED_SET_LEDS             0x00001000
#define KB_INIT_FAILED_SET_LEDS_PARAM       0x00002000
#define KB_INIT_FAILED_SELECT_SS            0x00010000
#define KB_INIT_FAILED_SELECT_SS_PARAM      0x00020000

#if KEYBOARD_RECORD_INIT

ULONG KeyboardInitStatus;
#define SET_KB_INIT_FAILURE(flag) KeyboardInitStatus |= flag
#define KB_INIT_START() KeyboardInitStatus = 0x0;

#else 

#define SET_KB_INIT_FAILURE(flag)
#define KB_INIT_START() 

#endif // KEYBOARD_RECORD_INIT

NTSTATUS
I8xInitializeKeyboard(
    VOID
    )
/*++

Routine Description:

    This routine initializes the i8042 keyboard hardware.  It is called
    only at initialization, and does not synchronize access to the hardware.

Arguments:

    DeviceObject - Pointer to the device object.

Return Value:

    Returns status.

--*/

{
    NTSTATUS                            status;
    PKEYBOARD_ID                        id;
    PPORT_KEYBOARD_EXTENSION            deviceExtension;
    PDEVICE_OBJECT                      deviceObject;
    UCHAR                               byte;
#ifndef NEC_98
    I8042_TRANSMIT_CCB_CONTEXT          transmitCCBContext;
#endif // NEC_98
    ULONG                               i;
    ULONG                               limit;
    PIO_ERROR_LOG_PACKET                errorLogEntry;
    ULONG                               uniqueErrorValue;
    NTSTATUS                            errorCode = STATUS_SUCCESS;
    ULONG                               dumpCount;
    PI8042_CONFIGURATION_INFORMATION    configuration;
    PKEYBOARD_ID                        keyboardId;
    LARGE_INTEGER                       startOfSpin,
                                        nextQuery,
                                        difference,
                                        tenSeconds,
                                        li;
    BOOLEAN                             waitForAckOnReset = WAIT_FOR_ACKNOWLEDGE,
                                        translationOn = TRUE,
                                        failedReset = FALSE,
                                        failedTypematic = FALSE,
                                        failedLeds = FALSE;
    
#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    PAGED_CODE();

    KB_INIT_START();

    Print(DBG_SS_TRACE, ("I8xInitializeKeyboard, enter\n"));

    for (i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    //
    // Get the device extension.
    //
    deviceExtension = Globals.KeyboardExtension; 
    deviceObject = deviceExtension->Self;

    //
    // Reset the keyboard.
    //

#if defined(NEC_98)
    NEC98_KeyboardReset();

    deviceExtension->CtrlKeyStatus = 0;
    if (deviceExtension->KeyboardTypeNEC == -1) {
        deviceExtension->KeyboardTypeNEC = NEC98_KeyboardDetection();
    }

    NEC98_EnableExtKeys();
#else // defined(NEC_98)
StartOfReset:
    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 waitForAckOnReset,
                 (CCHAR) KeyboardDeviceType,
                 (UCHAR) KEYBOARD_RESET
                 );
    if (!NT_SUCCESS(status)) {

        SET_KB_INIT_FAILURE(KB_INIT_FAILED_RESET);
        failedReset = TRUE;

        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard: failed keyboard reset, status 0x%x\n",
              status
              ));

        //
        // Set up error log info.
        //

        errorCode = I8042_KBD_RESET_COMMAND_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 510;
        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
        dumpData[1] = DataPort;
        dumpData[2] = KEYBOARD_RESET;
        dumpCount = 3;

        //
        // NOTE:  The following line was commented out to work around a
        //        problem with the Gateway 4DX2/66V when an old Compaq 286
        //        keyboard is attached.  In this case, the keyboard reset
        //        is not acknowledged (at least, the system never
        //        receives the ack).  Instead, the KEYBOARD_COMPLETE_SUCCESS
        //        byte is sitting in the i8042 output buffer.  The workaround
        //        is to ignore the keyboard reset failure and continue.
        //
        // goto I8xInitializeKeyboardExit;
    }

    //
    // Get the keyboard reset self-test response.  A response byte of
    // KEYBOARD_COMPLETE_SUCCESS indicates success; KEYBOARD_COMPLETE_FAILURE
    // indicates failure.
    //
    // Note that it is usually necessary to stall a long time to get the
    // keyboard reset/self-test to work.  The stall value was determined by
    // experimentation.
    //

    limit = I8X_POLL_ITERATIONS_MAX / 2;
    li.QuadPart = -100;

    tenSeconds.QuadPart = 10*10*1000*1000;
    KeQueryTickCount(&startOfSpin);

    for (i = 0; i < limit; i++) {

        status = I8xGetBytePolled(
                     (CCHAR) KeyboardDeviceType,
                     &byte
                     );

        if (NT_SUCCESS(status)) {
            if (byte == (UCHAR) KEYBOARD_COMPLETE_SUCCESS) {

                //
                // The reset completed successfully.
                //

                break;

            } else {

                //
                // There was some sort of failure during the reset
                // self-test.  Continue anyway.
                //

                //
                // Log a warning.
                //


                dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
                dumpData[1] = KeyboardDeviceType;
                dumpData[2] = KEYBOARD_COMPLETE_SUCCESS;
                dumpData[3] = byte;

                I8xLogError(
                    deviceObject,
                    I8042_KBD_RESET_RESPONSE_FAILED,
                    I8042_ERROR_VALUE_BASE + 515,
                    status,
                    dumpData,
                    4
                    );

                break;
            }


        } else {

            if (status == STATUS_IO_TIMEOUT) {

                //
                // Stall, and then try again to get a response from
                // the reset.
                //

                if (TRUE) {     // Globals.PoweringUp) {
                    KeDelayExecutionThread(KernelMode,
                                           FALSE,
                                           &li);
                }
                else {
                    KeStallExecutionProcessor(50);  
                }

                KeQueryTickCount(&nextQuery);

                difference.QuadPart = nextQuery.QuadPart - startOfSpin.QuadPart;

                ASSERT(KeQueryTimeIncrement() <= MAXLONG);
                if (difference.QuadPart*KeQueryTimeIncrement() >=
                    tenSeconds.QuadPart) {

                    break;
                }

            } else {

                break;

            }

        }
    }

    if (!NT_SUCCESS(status)) {

        if (waitForAckOnReset == WAIT_FOR_ACKNOWLEDGE) {
            waitForAckOnReset = NO_WAIT_FOR_ACKNOWLEDGE;
            goto StartOfReset;
        }

        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard, failed reset response, status 0x%x, byte 0x%x\n",
              status,
              byte
              ));

        //
        // Set up error log info.
        //

        errorCode = I8042_KBD_RESET_RESPONSE_FAILED;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 520;
        dumpData[0] = KBDMOU_INCORRECT_RESPONSE;
        dumpData[1] = KeyboardDeviceType;
        dumpData[2] = KEYBOARD_COMPLETE_SUCCESS;
        dumpData[3] = byte;
        dumpCount = 4;

        goto I8xInitializeKeyboardExit;
    }

    //
    // Turn off Keyboard Translate Mode.  Call I8xTransmitControllerCommand
    // to read the Controller Command Byte, modify the appropriate bits, and
    // rewrite the Controller Command Byte.
    //

    transmitCCBContext.HardwareDisableEnableMask = 0;
    transmitCCBContext.AndOperation = AND_OPERATION;
    transmitCCBContext.ByteMask = (UCHAR) ~((UCHAR)CCB_KEYBOARD_TRANSLATE_MODE);

    I8xTransmitControllerCommand(
        (PVOID) &transmitCCBContext
        );

    if (!NT_SUCCESS(transmitCCBContext.Status)) {
        //
        // If failure then retry once.  This is for Toshiba T3400CT.
        //
        I8xTransmitControllerCommand(
            (PVOID) &transmitCCBContext
            );
    }

    if (!NT_SUCCESS(transmitCCBContext.Status)) {
        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard: could not turn off translate\n"
              ));
        status = transmitCCBContext.Status;
        SET_KB_INIT_FAILURE(KB_INIT_FAILED_XLATE_OFF);
        goto I8xInitializeKeyboardExit;
    }
#endif // defined(NEC_98)

    //
    // Get a pointer to the keyboard identifier field.
    //

    id = &deviceExtension->KeyboardAttributes.KeyboardIdentifier;

    //
    // Set the typematic rate and delay.  Send the Set Typematic Rate command
    // to the keyboard, followed by the typematic rate/delay parameter byte.
    // Note that it is often necessary to stall a long time to get this
    // to work.  The stall value was determined by experimentation.  Some
    // broken hardware does not accept this command, so ignore errors in the
    // hope that the keyboard will work okay anyway.
    //
    //

    if ((status = I8xPutBytePolled(
                      (CCHAR) DataPort,
                      WAIT_FOR_ACKNOWLEDGE,
                      (CCHAR) KeyboardDeviceType,
                      (UCHAR) SET_KEYBOARD_TYPEMATIC
                      )) != STATUS_SUCCESS) {

        SET_KB_INIT_FAILURE(KB_INIT_FAILED_SET_TYPEMATIC);
        failedTypematic = TRUE;

        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard: could not send SET TYPEMATIC cmd\n"
              ));

        //
        // Log an error.
        //
        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_TYPEMATIC;

        I8xLogError(
            deviceObject,
            I8042_SET_TYPEMATIC_FAILED,
            I8042_ERROR_VALUE_BASE + 535,
            status,
            dumpData,
            3
            );

    } else if ((status = I8xPutBytePolled(
                          (CCHAR) DataPort,
                          WAIT_FOR_ACKNOWLEDGE,
                          (CCHAR) KeyboardDeviceType,
#if defined(NEC_98)
                          (UCHAR) PC98_STOP_AUTO_REPEAT
                          )) != STATUS_SUCCESS) {
#else // defined(NEC_98)
                          I8xConvertTypematicParameters(
                          deviceExtension->KeyRepeatCurrent.Rate,
                          deviceExtension->KeyRepeatCurrent.Delay
                          ))) != STATUS_SUCCESS) {
#endif // defined(NEC_98)

        SET_KB_INIT_FAILURE(KB_INIT_FAILED_SET_TYPEMATIC_PARAM);
        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard: could not send typematic param\n"
              ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_TYPEMATIC;
        dumpData[3] =
            I8xConvertTypematicParameters(
                deviceExtension->KeyRepeatCurrent.Rate,
                deviceExtension->KeyRepeatCurrent.Delay
                );

        I8xLogError(
            deviceObject,
            I8042_SET_TYPEMATIC_FAILED,
            I8042_ERROR_VALUE_BASE + 540,
            status,
            dumpData,
            4
            );

    }

    status = STATUS_SUCCESS;

#if defined(NEC_98)
    KeStallExecutionProcessor(3000);
#endif // defined(NEC_98)

    //
    // Set the keyboard indicator lights.  Ignore errors.
    //

    if ((status = I8xPutBytePolled(
                      (CCHAR) DataPort,
                      WAIT_FOR_ACKNOWLEDGE,
                      (CCHAR) KeyboardDeviceType,
                      (UCHAR) SET_KEYBOARD_INDICATORS
                      )) != STATUS_SUCCESS) {

        SET_KB_INIT_FAILURE(KB_INIT_FAILED_SET_LEDS);
        failedLeds = TRUE;

        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard: could not send SET LEDS cmd\n"
              ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_INDICATORS;

        I8xLogError(
            deviceObject,
            I8042_SET_LED_FAILED,
            I8042_ERROR_VALUE_BASE + 545,
            status,
            dumpData,
            3
            );

    } else if ((status = I8xPutBytePolled(
                             (CCHAR) DataPort,
                             WAIT_FOR_ACKNOWLEDGE,
                             (CCHAR) KeyboardDeviceType,
#if defined(NEC_98)
                             (UCHAR) (deviceExtension->KeyboardIndicators.LedFlags | 0x0070)
#else // defined(NEC_98)
                             (UCHAR) deviceExtension->KeyboardIndicators.LedFlags
#endif // defined(NEC_98)
                             )) != STATUS_SUCCESS) {

        SET_KB_INIT_FAILURE(KB_INIT_FAILED_SET_LEDS_PARAM);

        Print(DBG_SS_ERROR,
              ("I8xInitializeKeyboard: could not send SET LEDS param\n"
              ));

        //
        // Log an error.
        //

        dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
        dumpData[1] = DataPort;
        dumpData[2] = SET_KEYBOARD_INDICATORS;
        dumpData[3] =
            deviceExtension->KeyboardIndicators.LedFlags;

        I8xLogError(
            deviceObject,
            I8042_SET_LED_FAILED,
            I8042_ERROR_VALUE_BASE + 550,
            status,
            dumpData,
            4
            );

    }

    status = STATUS_SUCCESS;

#if !(defined(_X86_) || defined(_IA64_) || defined(_PPC_))  // IBMCPK: MIPS specific initialization

    //
    // NOTE:    This code is necessary until the MIPS firmware stops
    //          selecting scan code set 3.  Select scan code set 2 here.
    //          Since the translate bit is set, the net effect is that
    //          we will receive scan code set 1 bytes.
    //

    if (ENHANCED_KEYBOARD(*id))  {
        status = I8xPutBytePolled(
                     (CCHAR) DataPort,
                     WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) KeyboardDeviceType,
                     (UCHAR) SELECT_SCAN_CODE_SET
                     );

        if (NT_SUCCESS(status)) {

            //
            // Send the associated parameter byte.
            //

            status = I8xPutBytePolled(
                         (CCHAR) DataPort,
                         WAIT_FOR_ACKNOWLEDGE,
                         (CCHAR) KeyboardDeviceType,
                         (UCHAR) 2
                         );
        }

        if (!NT_SUCCESS(status)) {
            Print(DBG_SS_ERROR,
                  ("I8xInitializeKeyboard: could not send Select Scan command\n"
                  ));

            //
            // This failed so probably what we have here isn't an enhanced
            // keyboard at all.  Make this an old style keyboard.
            //

            configuration = &Globals.ControllerData->Configuration;
            keyboardId = &deviceExtension->KeyboardAttributes.KeyboardIdentifier;

            keyboardId->Type = 3;

            deviceExtension->KeyboardAttributes.NumberOfFunctionKeys =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfFunctionKeys;
            deviceExtension->KeyboardAttributes.NumberOfIndicators =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfIndicators;
            deviceExtension->KeyboardAttributes.NumberOfKeysTotal =
                KeyboardTypeInformation[keyboardId->Type - 1].NumberOfKeysTotal;

            status = STATUS_SUCCESS;
        }
    }
#endif

#if defined(FE_SB) && !defined(NEC_98) // I8xInitializeKeyboard()

    if (IBM02_KEYBOARD(*id)) {

        //
        // IBM-J 5576-002 Keyboard should set local scan code set for
        // supplied NLS key.
        //

        status = I8xPutBytePolled(
                     (CCHAR) DataPort,
                     WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) KeyboardDeviceType,
                     (UCHAR) SELECT_SCAN_CODE_SET
                     );
        if (status != STATUS_SUCCESS) {
            Print(DBG_SS_ERROR,
                  ("I8xInitializeKeyboard: could not send Select Scan command\n"
                  ));
            Print(DBG_SS_ERROR,
                  ("I8xInitializeKeyboard: WARNING - using scan set 82h\n"
                  ));
            deviceExtension->KeyboardAttributes.KeyboardMode = 3;
        } else {

            //
            // Send the associated parameter byte.
            //

            status = I8xPutBytePolled(
                         (CCHAR) DataPort,
                         WAIT_FOR_ACKNOWLEDGE,
                         (CCHAR) KeyboardDeviceType,
                         (UCHAR) 0x82
                         );
            if (status != STATUS_SUCCESS) {
                Print(DBG_SS_ERROR,
                      ("I8xInitializeKeyboard: could not send Select Scan param\n"
                      ));
                Print(DBG_SS_ERROR,
                      ("I8xInitializeKeyboard: WARNING - using scan set 82h\n"
                      ));
                deviceExtension->KeyboardAttributes.KeyboardMode = 3;
            }
        }
    }
#endif // FE_SB

    if (deviceExtension->InitializationHookCallback) {
        (*deviceExtension->InitializationHookCallback) (
            deviceExtension->HookContext,
            (PVOID) deviceObject,
            (PI8042_SYNCH_READ_PORT) I8xKeyboardSynchReadPort,
            (PI8042_SYNCH_WRITE_PORT) I8xKeyboardSynchWritePort,
            &translationOn
            );
    }

#ifndef NEC_98
    if (deviceExtension->KeyboardAttributes.KeyboardMode == 1 &&
        translationOn) {

        //
        // Turn translate back on.  The keyboard should, by default, send
        // scan code set 2.  When the translate bit in the 8042 command byte
        // is on, the 8042 translates the scan code set 2 bytes to scan code
        // set 1 before sending them to the CPU.  Scan code set 1 is
        // the industry standard scan code set.
        //
        // N.B.  It does not appear to be possible to change the translate
        //       bit on some models of PS/2.
        //

        transmitCCBContext.HardwareDisableEnableMask = 0;
        transmitCCBContext.AndOperation = OR_OPERATION;
        transmitCCBContext.ByteMask = (UCHAR) CCB_KEYBOARD_TRANSLATE_MODE;

        I8xTransmitControllerCommand(
            (PVOID) &transmitCCBContext
            );

        if (!NT_SUCCESS(transmitCCBContext.Status)) {
            SET_KB_INIT_FAILURE(KB_INIT_FAILED_XLATE_ON);
            Print(DBG_SS_ERROR,
                  ("I8xInitializeKeyboard: couldn't turn on translate\n"
                  ));

            if (transmitCCBContext.Status == STATUS_DEVICE_DATA_ERROR) {

                //
                // Could not turn translate back on.  This happens on some
                // PS/2 machines.  In this case, select scan code set 1
                // for the keyboard, since the 8042 will not do the
                // translation from the scan code set 2, which is what the
                // KEYBOARD_RESET caused the keyboard to default to.
                //

                if (ENHANCED_KEYBOARD(*id))  {
                    status = I8xPutBytePolled(
                                 (CCHAR) DataPort,
                                 WAIT_FOR_ACKNOWLEDGE,
                                 (CCHAR) KeyboardDeviceType,
                                 (UCHAR) SELECT_SCAN_CODE_SET
                                 );
                    if (!NT_SUCCESS(status)) {
                        SET_KB_INIT_FAILURE(KB_INIT_FAILED_SELECT_SS);
                        Print(DBG_SS_ERROR,
                              ("I8xInitializeKeyboard: could not send Select Scan command\n"
                              ));
                        Print(DBG_SS_ERROR,
                              ("I8xInitializeKeyboard: WARNING - using scan set 2\n"
                              ));
                        deviceExtension->KeyboardAttributes.KeyboardMode = 2;
                        //
                        // Log an error.
                        //

                        dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
                        dumpData[1] = DataPort;
                        dumpData[2] = SELECT_SCAN_CODE_SET;

                        I8xLogError(
                            deviceObject,
                            I8042_SELECT_SCANSET_FAILED,
                            I8042_ERROR_VALUE_BASE + 555,
                            status,
                            dumpData,
                            3
                            );

                    } else {

                        //
                        // Send the associated parameter byte.
                        //

                        status = I8xPutBytePolled(
                                     (CCHAR) DataPort,
                                     WAIT_FOR_ACKNOWLEDGE,
                                     (CCHAR) KeyboardDeviceType,
#ifdef FE_SB // I8xInitializeKeyboard()
                                     (UCHAR) (IBM02_KEYBOARD(*id) ? 0x81 : 1 )
#else
                                     (UCHAR) 1
#endif // FE_SB
                                     );
                        if (!NT_SUCCESS(status)) {
                            SET_KB_INIT_FAILURE(KB_INIT_FAILED_SELECT_SS_PARAM);
                            Print(DBG_SS_ERROR,
                                  ("I8xInitializeKeyboard: could not send Select Scan param\n"
                                  ));
                            Print(DBG_SS_ERROR,
                                  ("I8xInitializeKeyboard: WARNING - using scan set 2\n"
                                  ));
                            deviceExtension->KeyboardAttributes.KeyboardMode = 2;
                            //
                            // Log an error.
                            //

                            dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
                            dumpData[1] = DataPort;
                            dumpData[2] = SELECT_SCAN_CODE_SET;
                            dumpData[3] = 1;

                            I8xLogError(
                                deviceObject,
                                I8042_SELECT_SCANSET_FAILED,
                                I8042_ERROR_VALUE_BASE + 560,
                                status,
                                dumpData,
                                4
                                );

                        }
                    }
                }

            } else {
                status = transmitCCBContext.Status;
                goto I8xInitializeKeyboardExit;
            }
        }
    }

I8xInitializeKeyboardExit:

    if (failedReset && failedTypematic && failedLeds) {
        // set to io timeout so nec98 works correctly
        status = STATUS_IO_TIMEOUT;
        errorCode = I8042_NO_KBD_DEVICE;
    }

    //
    // If the keyboard initialization failed, log an error.
    //

    if (errorCode != STATUS_SUCCESS) {

        errorLogEntry = (PIO_ERROR_LOG_PACKET)
            IoAllocateErrorLogEntry(
                deviceObject,
                (UCHAR) (sizeof(IO_ERROR_LOG_PACKET)
                         + (dumpCount * sizeof(ULONG)))
                );

        if (errorLogEntry != NULL) {

            errorLogEntry->ErrorCode = errorCode;
            errorLogEntry->DumpDataSize = (USHORT) dumpCount * sizeof(ULONG);
            errorLogEntry->SequenceNumber = 0;
            errorLogEntry->MajorFunctionCode = 0;
            errorLogEntry->IoControlCode = 0;
            errorLogEntry->RetryCount = 0;
            errorLogEntry->UniqueErrorValue = uniqueErrorValue;
            errorLogEntry->FinalStatus = status;
            for (i = 0; i < dumpCount; i++)
                errorLogEntry->DumpData[i] = dumpData[i];

            IoWriteErrorLogEntry(errorLogEntry);
        }
    }
#endif // NEC_98

    if (NT_SUCCESS(status)) {
        Globals.ControllerData->HardwarePresent |= KEYBOARD_HARDWARE_INITIALIZED;
    }
    else {
        CLEAR_KEYBOARD_PRESENT();
    }

    //
    // Initialize current keyboard set packet state.
    //
    deviceExtension->CurrentOutput.State = Idle;
    deviceExtension->CurrentOutput.Bytes = NULL;
    deviceExtension->CurrentOutput.ByteCount = 0;

    Print(DBG_SS_TRACE, ("I8xInitializeKeyboard (0x%x)\n", status));

    return status;
}

NTSTATUS
I8xKeyboardConfiguration(
    IN PPORT_KEYBOARD_EXTENSION KeyboardExtension,
    IN PCM_RESOURCE_LIST ResourceList
    )
/*++

Routine Description:

    This routine retrieves the configuration information for the keyboard.

Arguments:

    KeyboardExtension - Keyboard extension
    
    ResourceList - Translated resource list give to us via the start IRP
    
Return Value:

    STATUS_SUCCESS if all the resources required are presented
    
--*/
{
    NTSTATUS                            status = STATUS_SUCCESS;

    PCM_PARTIAL_RESOURCE_LIST           partialResList = NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     firstResDesc = NULL,
                                        currentResDesc = NULL;
    PCM_FULL_RESOURCE_DESCRIPTOR        fullResDesc = NULL;
    PI8042_CONFIGURATION_INFORMATION    configuration;

    PKEYBOARD_ID                        keyboardId;

    ULONG                               count,
                                        i;
 
    KINTERRUPT_MODE                     defaultInterruptMode;
    BOOLEAN                             defaultInterruptShare;

    PAGED_CODE();

    if (!ResourceList) {
        Print(DBG_SS_INFO | DBG_SS_ERROR, ("keyboard with null resources\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    fullResDesc = ResourceList->List;
    if (!fullResDesc) {
        //
        // this should never happen
        //
        ASSERT(fullResDesc != NULL);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    configuration = &Globals.ControllerData->Configuration;

    partialResList = &fullResDesc->PartialResourceList;
    currentResDesc = firstResDesc = partialResList->PartialDescriptors;
    count = partialResList->Count;
  
    configuration->FloatingSave   = I8042_FLOATING_SAVE;
    configuration->BusNumber      = fullResDesc->BusNumber;
    configuration->InterfaceType  = fullResDesc->InterfaceType;

    if (configuration->InterfaceType == MicroChannel) {
        defaultInterruptShare = TRUE;
        defaultInterruptMode = LevelSensitive;
    }
    else {
        defaultInterruptShare = I8042_INTERRUPT_SHARE;
        defaultInterruptMode = I8042_INTERRUPT_MODE;
    }
    
    for (i = 0;     i < count;     i++, currentResDesc++) {
        switch (currentResDesc->Type) {
        case CmResourceTypeMemory:
            Globals.RegistersMapped = TRUE;

        case CmResourceTypePort:
            //
            // Copy the port information.  We will sort the port list
            // into ascending order based on the starting port address
            // later (note that we *know* there are a max of two port
            // ranges for the i8042).
            //
            if (currentResDesc->Flags == CM_RESOURCE_PORT_MEMORY) {
                Globals.RegistersMapped = TRUE;
            }

            Print(DBG_SS_NOISE,
                  ("port is %s.\n",
                  Globals.RegistersMapped ? "memory" : "io"));

            if (configuration->PortListCount < MaximumPortCount) {
                configuration->PortList[configuration->PortListCount] =
                    *currentResDesc;
                configuration->PortList[configuration->PortListCount].ShareDisposition =
                    I8042_REGISTER_SHARE ? CmResourceShareShared:
                                           CmResourceShareDriverExclusive;
                configuration->PortListCount += 1;
            }
            else {
                Print(DBG_SS_INFO | DBG_SS_ERROR,
                      ("KB::PortListCount already at max (%d)\n",
                       configuration->PortListCount
                      )
                     );
            }
            break;

        case CmResourceTypeInterrupt:
                
            //
            // Copy the interrupt information.
            //
            KeyboardExtension->InterruptDescriptor = *currentResDesc;
            KeyboardExtension->InterruptDescriptor.ShareDisposition =
                defaultInterruptShare ? CmResourceShareShared :
                                        CmResourceShareDeviceExclusive;

            break;

        default:
            Print(DBG_ALWAYS,
                  ("resource type 0x%x unhandled...\n",
                  (LONG) currentResDesc->Type
                  ));
            break;
        } 
    } 

    if (KeyboardExtension->InterruptDescriptor.Type & CmResourceTypeInterrupt) {
        Print(DBG_SS_INFO,
              ("Keyboard interrupt config --\n"
              "    %s, %s, Irq = 0x%x\n",
              KeyboardExtension->InterruptDescriptor.ShareDisposition ==
                  CmResourceShareShared ? "Sharable" : "NonSharable",
              KeyboardExtension->InterruptDescriptor.Flags ==
                  CM_RESOURCE_INTERRUPT_LATCHED ? "Latched" : "Level Sensitive",
              KeyboardExtension->InterruptDescriptor.u.Interrupt.Vector
              ));
    }
    //
    // If no keyboard-specific information (i.e., keyboard type, subtype,
    // and initial LED settings) was found, use the keyboard driver
    // defaults.
    //
    if (KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Type == 0) {
    
        Print(DBG_SS_INFO, ("Using default keyboard type\n"));
    
        KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Type =
            KEYBOARD_TYPE_DEFAULT;
        KeyboardExtension->KeyboardIndicators.LedFlags =
            KEYBOARD_INDICATORS_DEFAULT;
    
    }
    
#if defined(NEC_98)

    //
    // if KeyboardSubtype is not overridden, get it from
    // \\Registry\Machine\Hardware\Description.
    //

    if (KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype == 0) {

        ULONG                    i;
        INTERFACE_TYPE           interfaceType;
        CONFIGURATION_TYPE       controllerType = KeyboardController;
        CONFIGURATION_TYPE       peripheralType = KeyboardPeripheral;

        for (i = 0; i < MaximumInterfaceType; i++) {
            //
            // Get the registry information for this device.
            //
            interfaceType = i;
            IoQueryDeviceDescription(&interfaceType,
                NULL,
                &controllerType,
                NULL,
                &peripheralType,
                NULL,
                NEC98_GetKeyboardSpecificData,
                KeyboardExtension
                );

            if (KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype != 0) {

                Print(DBG_SS_INFO,
                    ("I8xKeyboardConfiguration: Keyboard Subtype is %d\n",
                    KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype
                    ));

                break;

            } else {

                Print(DBG_SS_INFO | DBG_SS_ERROR,
                    ("IoQueryDeviceDescription for bus type %d failed\n",
                    interfaceType
                    ));

            }
        }

        //
        // if failed to get KeyboardSubtype, use default(PC-9800 Standard keyboard)
        //

        if (KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype == 0) {
            Print(DBG_SS_INFO, ("Using default keyboard subtype\n"));
            KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype = 1;
        }

    }
#endif // defined(NEC_98)

    Print(DBG_SS_INFO,
          ("Keyboard device specific data --\n"
          "    Type = %d, Subtype = %d, Initial LEDs = 0x%x\n",
          KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Type,
          KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype,
          KeyboardExtension->KeyboardIndicators.LedFlags
          ));

    keyboardId = &KeyboardExtension->KeyboardAttributes.KeyboardIdentifier;
    if (!ENHANCED_KEYBOARD(*keyboardId)) {
        Print(DBG_SS_INFO, ("Old AT-style keyboard\n"));
        configuration->PollingIterations =
            configuration->PollingIterationsMaximum;
    }

    //
    // Initialize keyboard-specific configuration parameters.
    //

    if (FAREAST_KEYBOARD(*keyboardId)) {
        ULONG                      iIndex = 0;
        PKEYBOARD_TYPE_INFORMATION pKeyboardTypeInformation = NULL;

        while (KeyboardFarEastOemInformation[iIndex].KeyboardId.Type) {
            if ((KeyboardFarEastOemInformation[iIndex].KeyboardId.Type
                         == keyboardId->Type) &&
                (KeyboardFarEastOemInformation[iIndex].KeyboardId.Subtype
                         == keyboardId->Subtype)) {

                pKeyboardTypeInformation = (PKEYBOARD_TYPE_INFORMATION)
                    &(KeyboardFarEastOemInformation[iIndex].KeyboardTypeInformation);
                break;
            }

            iIndex++;
        }

        if (pKeyboardTypeInformation == NULL) {

            //
            // Set default...
            //

            pKeyboardTypeInformation = (PKEYBOARD_TYPE_INFORMATION)
                &(KeyboardTypeInformation[KEYBOARD_TYPE_DEFAULT-1]);
        }

        KeyboardExtension->KeyboardAttributes.NumberOfFunctionKeys =
            pKeyboardTypeInformation->NumberOfFunctionKeys;
        KeyboardExtension->KeyboardAttributes.NumberOfIndicators =
            pKeyboardTypeInformation->NumberOfIndicators;
        KeyboardExtension->KeyboardAttributes.NumberOfKeysTotal =
            pKeyboardTypeInformation->NumberOfKeysTotal;
    }
    else {
        KeyboardExtension->KeyboardAttributes.NumberOfFunctionKeys =
            KeyboardTypeInformation[keyboardId->Type - 1].NumberOfFunctionKeys;
        KeyboardExtension->KeyboardAttributes.NumberOfIndicators =
            KeyboardTypeInformation[keyboardId->Type - 1].NumberOfIndicators;
        KeyboardExtension->KeyboardAttributes.NumberOfKeysTotal =
            KeyboardTypeInformation[keyboardId->Type - 1].NumberOfKeysTotal;
    }

    KeyboardExtension->KeyboardAttributes.KeyboardMode =
        KEYBOARD_SCAN_CODE_SET;

    KeyboardExtension->KeyboardAttributes.KeyRepeatMinimum.Rate =
        KEYBOARD_TYPEMATIC_RATE_MINIMUM;
    KeyboardExtension->KeyboardAttributes.KeyRepeatMinimum.Delay =
        KEYBOARD_TYPEMATIC_DELAY_MINIMUM;
    KeyboardExtension->KeyboardAttributes.KeyRepeatMaximum.Rate =
        KEYBOARD_TYPEMATIC_RATE_MAXIMUM;
    KeyboardExtension->KeyboardAttributes.KeyRepeatMaximum.Delay =
        KEYBOARD_TYPEMATIC_DELAY_MAXIMUM;
    KeyboardExtension->KeyRepeatCurrent.Rate =
        KEYBOARD_TYPEMATIC_RATE_DEFAULT;
    KeyboardExtension->KeyRepeatCurrent.Delay =
        KEYBOARD_TYPEMATIC_DELAY_DEFAULT;

    return status;
}

#if defined(_X86_) && !defined(NEC_98)
ULONG
I8042ConversionStatusForOasys(
    IN ULONG fOpen,
    IN ULONG ConvStatus)

/*++

Routine Description:

    This routine convert ime open/close status and ime converion mode to
    FMV oyayubi-shift keyboard device internal input mode.

Arguments:


Return Value:

    FMV oyayubi-shift keyboard's internal input mode.

--*/
{
    ULONG ImeMode = 0;

    if (fOpen) {
        if (ConvStatus & IME_CMODE_ROMAN) {
            if (ConvStatus & IME_CMODE_ALPHANUMERIC) {
                //
                // Alphanumeric, roman mode.
                //
                ImeMode = THUMB_ROMAN_ALPHA_CAPSON;
            } else if (ConvStatus & IME_CMODE_KATAKANA) {
                //
                // Katakana, roman mode.
                //
                ImeMode = THUMB_ROMAN_KATAKANA;
            } else if (ConvStatus & IME_CMODE_NATIVE) {
                //
                // Hiragana, roman mode.
                //
                ImeMode = THUMB_ROMAN_HIRAGANA;
            } else {
                ImeMode = THUMB_ROMAN_ALPHA_CAPSON;
            }
        } else {
            if (ConvStatus & IME_CMODE_ALPHANUMERIC) {
                //
                // Alphanumeric, no-roman mode.
                //
                ImeMode = THUMB_NOROMAN_ALPHA_CAPSON;
            } else if (ConvStatus & IME_CMODE_KATAKANA) {
                //
                // Katakana, no-roman mode.
                //
                ImeMode = THUMB_NOROMAN_KATAKANA;
            } else if (ConvStatus & IME_CMODE_NATIVE) {
                //
                // Hiragana, no-roman mode.
                //
                ImeMode = THUMB_NOROMAN_HIRAGANA;
            } else {
                ImeMode = THUMB_NOROMAN_ALPHA_CAPSON;
            }
        }
    } else {
        //
        // Ime close. In this case, internal mode is always this value.
        // (the both LED off roman and kana)
        //
        ImeMode = THUMB_NOROMAN_ALPHA_CAPSON;
    }

    return ImeMode;
}

ULONG
I8042QueryIMEStatusForOasys(
    IN PKEYBOARD_IME_STATUS KeyboardIMEStatus
    )
{
    ULONG InternalMode;

    //
    // Map to IME mode to hardware mode.
    //
    InternalMode = I8042ConversionStatusForOasys(
                KeyboardIMEStatus->ImeOpen,
                KeyboardIMEStatus->ImeConvMode
                );

    return InternalMode;
}

NTSTATUS
I8042SetIMEStatusForOasys(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PINITIATE_OUTPUT_CONTEXT InitiateContext
    )
{
    PKEYBOARD_IME_STATUS KeyboardIMEStatus;
    PPORT_KEYBOARD_EXTENSION  kbExtension;
    ULONG InternalMode;
    LARGE_INTEGER deltaTime;

    kbExtension = DeviceObject->DeviceExtension;

    //
    // Get pointer to KEYBOARD_IME_STATUS buffer.
    //
    KeyboardIMEStatus = (PKEYBOARD_IME_STATUS)(Irp->AssociatedIrp.SystemBuffer);

    //
    // Map IME mode to keyboard hardware mode.
    //
    InternalMode = I8042QueryIMEStatusForOasys(KeyboardIMEStatus);

    //
    // Set up the context structure for the InitiateIo wrapper.
    //
    InitiateContext->Bytes = Globals.ControllerData->DefaultBuffer;
    InitiateContext->DeviceObject = DeviceObject;
    InitiateContext->ByteCount = 3;
    InitiateContext->Bytes[0] = 0xF0;
    InitiateContext->Bytes[1] = 0x8C;
    InitiateContext->Bytes[2]  = (UCHAR)InternalMode;

    return (STATUS_SUCCESS);
}
#endif // defined(_X86_) && !defined(NEC_98)

#if defined(NEC_98)
NTSTATUS
NEC98_KeyboardCommandByte(
    IN UCHAR KeyboardCommand
    )

/*++

Routine Description:

    This routine is command out routine for PC9800 Keyboard controller

Arguments:

    KeyboardCommand - A command sent to keyboard.

Return Value:

    Returns status.

--*/

{
    ULONG i,j;
    NTSTATUS Status;
    PUCHAR dataAddress, commandAddress;

    Print(DBG_BUFIO_TRACE, ("NEC98_KeyboardCommandByte: enter\n"));

    dataAddress = Globals.ControllerData->DeviceRegisters[DataPort];
    commandAddress = Globals.ControllerData->DeviceRegisters[CommandPort];

    //
    // Disable keyboard interrupt.
    //

    I8X_PUT_COMMAND_BYTE(
        commandAddress,
        (UCHAR) 0x37
        );
    KeStallExecutionProcessor(10);

    I8X_PUT_COMMAND_BYTE(
        commandAddress,
        (UCHAR) 0x33
        );
    KeStallExecutionProcessor(10);

    //
    // Write a command.
    //

    I8X_PUT_DATA_BYTE(
        dataAddress,
        (UCHAR) KeyboardCommand
        );
    KeStallExecutionProcessor(45);

    //
    // Make sure the Input Buffer Full controller status bit is clear.
    // Time out if necessary.
    //
    Status = STATUS_SUCCESS;
    for (j = 0; j < Globals.ControllerData->Configuration.ResendIterations; j++) {
        for (i = 0; i < Globals.ControllerData->Configuration.PollingIterations; i++) {
            if ((I8X_GET_STATUS_BYTE(commandAddress) & PC98_8251_BUFFER_EMPTY) == PC98_8251_BUFFER_EMPTY) {
                break;
            } else {
                Print(DBG_BUFIO_NOISE, ("NEC98_KeyboardCommandByte: stalling\n"));
                KeStallExecutionProcessor(Globals.ControllerData->Configuration.StallMicroseconds);
            }
        }
        if (i >= Globals.ControllerData->Configuration.PollingIterations) {
            Print(DBG_BUFIO_ERROR |DBG_BUFIO_INFO, ("NEC98_KeyboardCommandByte: timing out\n"));
            Status = STATUS_IO_TIMEOUT;
            break;
        }
    }

    //
    // Enable keyboard interrupt.
    //

    I8X_PUT_COMMAND_BYTE(
        commandAddress,
        (UCHAR) 0x37
        );
    KeStallExecutionProcessor(10);

    I8X_PUT_COMMAND_BYTE(
        commandAddress,
        (UCHAR) 0x16
        );

    KeStallExecutionProcessor(100);

    Print(DBG_BUFIO_TRACE, ("NEC98_KeyboardCommandByte: exit\n"));

    return(Status);
}

NTSTATUS
NEC98_QueueEmulationKey(
    IN PDEVICE_OBJECT deviceObject,
    IN PKEYBOARD_INPUT_DATA input
    )

/*++

Routine Description:

    This routine sends the emulation key data to the queue.

Arguments:

    deviceExtention - A pointer to device extension.

    input - Pointer to the scancode and flags.

Return Value:

    Returns status.

--*/

{
    if (!I8xWriteDataToKeyboardQueue(
            Globals.KeyboardExtension,
            input
            )) {
        IsrPrint(DBG_KBISR_ERROR, ("NEC98_QueueEmulationKey queue overflow\n"));
        KeInsertQueueDpc(
            &Globals.KeyboardExtension->ErrorLogDpc,
            (PIRP) NULL,
            (PVOID) (ULONG) I8042_KBD_BUFFER_OVERFLOW
            );
            return(!STATUS_SUCCESS);
    } else {
        KeInsertQueueDpc(
            &Globals.KeyboardExtension->KeyboardIsrDpc,
            (PIRP) NULL, // deviceObject->CurrentIrp,
            NULL
            );
            return(STATUS_SUCCESS);
    }
}

VOID
NEC98_KeyboardReset(
    VOID
    )

/*++

Routine Description:

    This routine resets the keyboard hardware.

Arguments:

    deviceExtention - A pointer to device extension.

Return Value:

    none.

--*/

{
    ULONG   i;

    IsrPrint(DBG_KBISR_TRACE, ("NEC98_KeyboardReset: enter\n"));

    //
    // Send dummy command three times.
    //

    for (i = 0; i < 3; i++) {
        I8X_PUT_COMMAND_BYTE(
            Globals.ControllerData->DeviceRegisters[CommandPort],
            (UCHAR) PC98_8251_KEYBOARD_RESET_DUMMY
            );
    }

    //
    // Keyboard I/F Internal Reset
    //

    I8X_PUT_COMMAND_BYTE(
        Globals.ControllerData->DeviceRegisters[CommandPort],
        (UCHAR) PC98_8251_KEYBOARD_RESET_INTERNAL1
        );

    I8X_PUT_COMMAND_BYTE(
        Globals.ControllerData->DeviceRegisters[CommandPort],
        (UCHAR) PC98_8251_KEYBOARD_RESET_INTERNAL2
        );

    //
    // Software Reset
    //

    I8X_PUT_COMMAND_BYTE(
                Globals.ControllerData->DeviceRegisters[CommandPort],
                (UCHAR) PC98_8251_KEYBOARD_RESET1
                 );

    KeStallExecutionProcessor(26);

    I8X_PUT_COMMAND_BYTE(
                Globals.ControllerData->DeviceRegisters[CommandPort],
                (UCHAR) PC98_8251_KEYBOARD_RESET2
                 );

    KeStallExecutionProcessor(36);

    I8X_PUT_COMMAND_BYTE(
                Globals.ControllerData->DeviceRegisters[CommandPort],
                (UCHAR) PC98_8251_KEYBOARD_RESET3
                 );

    KeStallExecutionProcessor(50000);

    IsrPrint(DBG_KBISR_TRACE, ("NEC98_KeyboardReset: exit\n"));
}

NTSTATUS
NEC98_TranslateScancodeNEC98(
    IN UCHAR *ScanCode,
    IN USHORT *InputFlags
    )

/*++

Routine Description:

    This routine executes following translation.

    NEC PC9800 Keyboard ScanCode --> PC/AT 106 Keyboard ScanCode Set

Arguments:

    Scancode - A pointer to the PC9800 keyboard scancode.
               It is replaced by PC/AT 106 Keyboard ScanCode.

    InputFlags - A pointer to the PC9800 keyboard InputFlags.
                 It is repraced by PC/AT 106 Keyboard InputFlags.

Return Value:

    Returns status.

--*/

{
    UCHAR ScanCode98 = *ScanCode;

    *ScanCode = ScanCodeSet1_98[ScanCode98 & 0x7f];
    if (ScanCode98 & 0x80) {
        *ScanCode |= 0x80;
    }

    if (Ext0FlgNEC98[ScanCode98 & 0x7f]) {
        *InputFlags |= KEY_E0;
    } else {
        *InputFlags &= ~KEY_E0;
    }

   if (*ScanCode == 0xFF) {
       return (!STATUS_SUCCESS);
   } else {
       return (STATUS_SUCCESS);
   }
}

NTSTATUS
NEC98_TranslateScancodeN106(
    IN UCHAR *ScanCode,
    IN USHORT *InputFlags
    )

/*++

Routine Description:

    This routine executes following translation.

    NEC PC9800 106 Keyboard ScanCode --> PC/AT 106 Keyboard ScanCode Set

Arguments:

    Scancode - A pointer to the PC9800 106 keyboard scancode.
               It is replaced by PC/AT 106 keyboard scancode.

    InputFlags - A pointer to the PC9800 106 keyboard InputFlags.
                 It is repraced by PC/AT 106 Keyboard InputFlags.

Return Value:

    Returns status.

--*/

{
    UCHAR ScanCode98 = *ScanCode;

    *ScanCode = ScanCodeSet1_N106[ScanCode98 & 0x7f];
    if (ScanCode98 & 0x80) {
        *ScanCode |= 0x80;
    }

    if (Ext0FlgN106[ScanCode98 & 0x7f]) {
        *InputFlags |= KEY_E0;
    } else {
        *InputFlags &= ~KEY_E0;
    }

   if (*ScanCode == 0xFF) {
       return (!STATUS_SUCCESS);
   } else {
       return (STATUS_SUCCESS);
   }
}

VOID
NEC98_PauseKeyEmulation(
    IN PDEVICE_OBJECT deviceObject,
    IN PKEYBOARD_INPUT_DATA input
    )

/*++

Routine Description:

    Pause Key(PC/AT) emuration by Pause Key(PC9800).

Arguments:

    DeviceObject - Pointer to the device object.

    input - Pointer to the scancode and flags.

Return Value:

    none.

--*/

{
    PPORT_KEYBOARD_EXTENSION deviceExtension;

    deviceExtension = (PPORT_KEYBOARD_EXTENSION) deviceObject->DeviceExtension;

    if (deviceExtension->CtrlKeyStatus & 0x22) {
        IsrPrint(DBG_KBISR_EMUL, ("Ctrl+Pause Emulation\n"));
        //
        // Generate MAKE code.
        //
        input->MakeCode = STOP_KEY;
        input->Flags |= KEY_E0;
        NEC98_QueueEmulationKey(
            deviceObject,
            input
            );
        //
        // Generate BREAK code.
        //
        input->Flags |= KEY_BREAK;
    } else {
        IsrPrint(DBG_KBISR_EMUL, ("Pause Emulation\n"));
        //
        // Generate MAKE code.
        //
        input->Flags &= ~KEY_E0;
        input->Flags |= KEY_E1;
        input->MakeCode = CTRL_KEY;
        NEC98_QueueEmulationKey(
            deviceObject,
            input
            );
        input->Flags = 0;
        input->MakeCode = PAUSE_KEY;
        NEC98_QueueEmulationKey(
            deviceObject,
            input
            );
        //
        // Generate BREAK code.
        //
        input->Flags = KEY_E1|KEY_BREAK;
        input->MakeCode = CTRL_KEY;
        NEC98_QueueEmulationKey(
            deviceObject,
            input
            );
        input->Flags = KEY_BREAK;
        input->MakeCode = PAUSE_KEY;
    }
}

ULONG
NEC98_KeyboardDetection(
    VOID 
    )

/*++

Routine Description:

    This routine detects keyboard hardware for PC9800.

Arguments:

    deviceExtention - A pointer to device extension.

Return Value:

    keyboard type(see i8042prt.h)

--*/

{
    PPORT_KEYBOARD_EXTENSION    deviceExtension;
    BOOLEAN                     IsWinKey = FALSE;
    BOOLEAN                     IsLapTopKey = FALSE;
    UCHAR                       IDStatus = 0;
    ULONG                       KeyboardType;
    NTSTATUS                    status;

    Print(DBG_SS_TRACE, ("NEC98_KeyboardDetection: enter \n"));

    //
    // send Keyboard ID request command
    //

    status = I8xPutBytePolled(
                 (CCHAR) DataPort,
                 WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) KeyboardDeviceType,
                 (UCHAR) READ_KEYBOARD_ID);

    KeStallExecutionProcessor(3000);

    if (status != STATUS_SUCCESS) {

        Print(DBG_SS_ERROR | DBG_SS_INFO,
              ("NEC98_KeyboardDetection: could not send ID Request command\n"
              ));

    } else {

        //
        // waiting for Keyboard ID
        //

        while (I8xGetBytePolled(
                   (CCHAR) KeyboardDeviceType,
                   &IDStatus) != STATUS_SUCCESS);

        switch(IDStatus){
            case  PC98_KEYBOARD_ID_FIRST_BYTE:

                //
                // Maybe standard keyboard. I sent 2nd ID Request.
                //

                while (I8xGetBytePolled(
                           (CCHAR) KeyboardDeviceType,
                           &IDStatus) != STATUS_SUCCESS);

                switch(IDStatus){
                    case PC98_KEYBOARD_ID_2ND_BYTE_STD:

                        KeStallExecutionProcessor(3000);

                        status = I8xPutBytePolled(
                                     (CCHAR) DataPort,
                                     WAIT_FOR_ACKNOWLEDGE,
                                     (CCHAR) KeyboardDeviceType,
                                     (UCHAR) READ_KEYBOARD_2ND_ID);

                        if (status != STATUS_SUCCESS) {
                             Print(DBG_SS_ERROR | DBG_SS_INFO,
                                   ("NEC98_KeyboardDetection: could not send 2nd ID command\n"
                                   ));
                        } else {
                            while (I8xGetBytePolled(
                                       (CCHAR) KeyboardDeviceType,
                                       &IDStatus) != STATUS_SUCCESS);

                            switch(IDStatus){
                                case  PC98_KEYBOARD_ID_FIRST_BYTE:
                                    while (I8xGetBytePolled(
                                               (CCHAR) KeyboardDeviceType,
                                               &IDStatus) != STATUS_SUCCESS);

                                    KeStallExecutionProcessor(3000);

                                    switch(IDStatus){
                                        case PC98_KEYBOARD_ID_2ND_BYTE_N106:
                                            KeyboardType = PC98_N106KEY;  // NEC_98 106 type keyboard
                                            break;
                                        case PC98_KEYBOARD_ID_2ND_BYTE_Win95:
                                            KeyboardType = PC98_Win95KEY;  // NEC_98 Win95 type keyboard
                                            break;
                                        case RESEND:
                                        default:
                                            KeyboardType = PC98_106KEY;  // NEC_98 Standard keyboard
                                            break;
                                    }
                                    break;
                                case RESEND:
                                default:
                                    KeyboardType = PC98_106KEY;  // NEC_98 Standard keyboard
                                    break;
                            }
                        }
                        break;

                    case PC98_KEYBOARD_ID_2ND_BYTE_PTOS:
                        KeyboardType = PC98_PTOSKEY;  // PC-PTOS keyboard
                        break;

                    default:
                        KeyboardType = PC98_NmodeKEY;  // keyboard can't detect.
                        break;
                }
                break;

            case RESEND:
            default:
                Print(DBG_SS_ERROR | DBG_SS_INFO,
                      ("NEC98_KeyboardDetection: cannot get keyboard ID\n"
                      ));
                KeyboardType = PC98_106KEY;  // default keyboard type.
                break;
        }
    }

    if ((KeyboardType < 0)||(KeyboardType > 2)) {
        KeyboardType = PC98_106KEY;  // default keyboard type.
    }

    deviceExtension = Globals.KeyboardExtension;
    switch(KeyboardType) {
        case PC98_N106KEY:
            deviceExtension->ScancodeProc = NEC98_TranslateScancodeN106;
      //case PC98_106KEY:
      //case PC98_Win95KEY:
        default:
            deviceExtension->ScancodeProc = NEC98_TranslateScancodeNEC98;
            break;
    }

    Print(DBG_SS_TRACE,
          ("NEC98_KeyboardDetection: exit(%d) \n",
          KeyboardType
          ));

    return KeyboardType;
}

VOID
NEC98_EnableExtKeys(
    VOID
    )
/*++

Routine Description:

    This routine enables extended keys.
      Windows95 keyboard : Windows(left and right), Apps
      106 keyboard : Right Shift

Arguments:

    none.

Return Value:

    none.

--*/

{
    NTSTATUS                            status;
    PPORT_KEYBOARD_EXTENSION            deviceExtension;
    PDEVICE_OBJECT                      deviceObject;
    ULONG                               dumpCount;
    ULONG                               i;

    ULONG dumpData[DUMP_COUNT];

    Print(DBG_CALL_TRACE, ("NEC98_EnableExtKeys:enter\n"));

    for (i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    //
    // Get the device extension.
    //
    deviceExtension = Globals.KeyboardExtension; 
    deviceObject = deviceExtension->Self;

    switch(deviceExtension->KeyboardTypeNEC) {
        case PC98_Win95KEY:
            if ((status = I8xPutBytePolled(
                              (CCHAR) DataPort,
                              WAIT_FOR_ACKNOWLEDGE,
                              (CCHAR) KeyboardDeviceType, 
                              (UCHAR) SET_WIN95_KEYBOARD_MODE
                              )) != STATUS_SUCCESS) {
                Print(DBG_SS_ERROR,
                      ("I8xInitializeKeyboard: could not send Win95Keyboard mode cmd\n"
                      ));

                //
                // Log an error.
                //

                dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
                dumpData[1] = DataPort;
                dumpData[2] = SET_WIN95_KEYBOARD_MODE;
                dumpCount = 3;

                I8xLogError(
                    deviceObject,
                    I8042_SET_TYPEMATIC_FAILED,
                    I8042_ERROR_VALUE_BASE + 535,
                    status,
                    dumpData,
                    3
                    );

            } else {
                if ((status = I8xPutBytePolled(
                                  (CCHAR) DataPort,
                                  WAIT_FOR_ACKNOWLEDGE,
                                  (CCHAR) KeyboardDeviceType, 
                                  (UCHAR) SET_WIN95_KEYBOARD_MODE_WIN95
                                  )) != STATUS_SUCCESS) {

                    Print(DBG_SS_ERROR,
                          ("I8xInitializeKeyboard: could not send Win95Keyboard mode param\n"
                          ));

                    //
                    // Log an error.
                    //
                    dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
                    dumpData[1] = DataPort;
                    dumpData[2] = SET_WIN95_KEYBOARD_MODE_WIN95;
                    dumpCount = 3;

                    I8xLogError(
                        deviceObject,
                        I8042_SET_TYPEMATIC_FAILED,
                        I8042_ERROR_VALUE_BASE + 540,
                        status,
                        dumpData,
                        3
                        );
                }
            }

            KeStallExecutionProcessor(3000);

            status = STATUS_SUCCESS;
            break;

        case PC98_N106KEY:
            deviceExtension->ScancodeProc = NEC98_TranslateScancodeN106;
            if ((status = I8xPutBytePolled(
                              (CCHAR) DataPort,
                              WAIT_FOR_ACKNOWLEDGE,
                              (CCHAR) KeyboardDeviceType, 
                              (UCHAR) SET_PAO_KEYBOARD_MODE
                              )) != STATUS_SUCCESS) {
                Print(DBG_SS_ERROR,
                      ("I8xInitializeKeyboard: could not send N106Keyboard mode cmd\n"
                      ));

                //
                // Log an error.
                //

                dumpData[0] = KBDMOU_COULD_NOT_SEND_COMMAND;
                dumpData[1] = DataPort;
                dumpData[2] = SET_PAO_KEYBOARD_MODE;

                I8xLogError(
                    deviceObject,
                    I8042_SET_TYPEMATIC_FAILED,
                    I8042_ERROR_VALUE_BASE + 535,
                    status,
                    dumpData,
                    3
                    );

            } else if ((status = I8xPutBytePolled(
                                  (CCHAR) DataPort,
                                  WAIT_FOR_ACKNOWLEDGE,
                                  (CCHAR) KeyboardDeviceType, 
                                  (UCHAR) SET_PAO_KEYBOARD_MODE_VMODE
                                  )) != STATUS_SUCCESS) {

                Print(DBG_SS_ERROR,
                      ("I8xInitializeKeyboard: could not send N106Keyboard mode param\n"
                      ));

                //
                // Log an error.
                //

                dumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
                dumpData[1] = DataPort;
                dumpData[2] = SET_PAO_KEYBOARD_MODE;

                I8xLogError(
                    deviceObject,
                    I8042_SET_TYPEMATIC_FAILED,
                    I8042_ERROR_VALUE_BASE + 540,
                    status,
                    dumpData,
                    3
                    );

            }

            KeStallExecutionProcessor(3000);

            status = STATUS_SUCCESS;
            break;

        case PC98_106KEY:
        default:
            break;
    }

    Print(DBG_CALL_TRACE, ("NEC98_EnableExtKeys: exit\n"));
}

NTSTATUS
NEC98_GetKeyboardSpecificData(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This is the callout routine sent as a parameter to
    IoQueryDeviceDescription.  It grabs the keyboard controller and
    peripheral configuration information.

Arguments:

    Context - Context parameter that was passed in by the routine
        that called IoQueryDeviceDescription.

    PathName - The full pathname for the registry key.

    BusType - Bus interface type (Isa, Eisa, Mca, etc.).

    BusNumber - The bus sub-key (0, 1, etc.).

    BusInformation - Pointer to the array of pointers to the full value
        information for the bus.

    ControllerType - The controller type (should be KeyboardController).

    ControllerNumber - The controller sub-key (0, 1, etc.).

    ControllerInformation - Pointer to the array of pointers to the full
        value information for the controller key.

    PeripheralType - The peripheral type (should be KeyboardPeripheral).

    PeripheralNumber - The peripheral sub-key.

    PeripheralInformation - Pointer to the array of pointers to the full
        value information for the peripheral key.


Return Value:

    Returns status.
    If successful, will have the following side-effects:
        - Sets configuration fields in
          Globals.KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype

--*/
{
    PPORT_KEYBOARD_EXTENSION            deviceExtension = Context;
    PUCHAR                              peripheralData;
    NTSTATUS                            status = STATUS_SUCCESS;
    ULONG                               i;
    ULONG                               listCount;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     resourceDescriptor;
    PCM_KEYBOARD_DEVICE_DATA            keyboardSpecificData;
    PKEY_VALUE_FULL_INFORMATION         configData = NULL;

    PAGED_CODE();

    Print(DBG_SS_TRACE,
        ("I8xKeyboardPeripheralCallout: Path @ 0x%x, Bus Type 0x%x, Bus Number 0x%x\n",
        PathName, BusType, BusNumber
        ));
    Print(DBG_SS_TRACE,
        ("    Controller Type 0x%x, Controller Number 0x%x, Controller info @ 0x%x\n",
        ControllerType, ControllerNumber, ControllerInformation
        ));
    Print(DBG_SS_TRACE,
        ("    Peripheral Type 0x%x, Peripheral Number 0x%x, Peripheral info @ 0x%x\n",
        PeripheralType, PeripheralNumber, PeripheralInformation
        ));

    //
    // Look through the peripheral's resource list for device-specific
    // information.  The keyboard-specific information is defined
    // in sdk\inc\ntconfig.h.
    //

    configData = PeripheralInformation[IoQueryDeviceConfigurationData];

    if (configData->DataLength != 0) {
        peripheralData = ((PUCHAR) configData) + configData->DataOffset; 

        peripheralData += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR,
                                       PartialResourceList);

        listCount = ((PCM_PARTIAL_RESOURCE_LIST) peripheralData)->Count;

        resourceDescriptor =
            ((PCM_PARTIAL_RESOURCE_LIST) peripheralData)->PartialDescriptors;

        for (i = 0; i < listCount; i++, resourceDescriptor++) {
            switch(resourceDescriptor->Type) {

                case CmResourceTypeDeviceSpecific:

                    //
                    // Get the keyboard subtype.
                    //

                    keyboardSpecificData =
                        (PCM_KEYBOARD_DEVICE_DATA)(((PUCHAR)resourceDescriptor)
                            + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
                    deviceExtension->KeyboardAttributes.KeyboardIdentifier.Subtype =
                        keyboardSpecificData->Subtype;

                    break;

                default:
                    break;
            }
        }
    }

    return(status);
}
#endif // defined(NEC_98)

VOID
I8xQueueCurrentKeyboardInput(
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine queues the current input data to be processed by a
    DPC outside the ISR

Arguments:

    DeviceObject - Pointer to the device object

Return Value:

    None

--*/
{
    PPORT_KEYBOARD_EXTENSION deviceExtension;

    deviceExtension = DeviceObject->DeviceExtension;

    if (deviceExtension->EnableCount) {

        if (!I8xWriteDataToKeyboardQueue(
                 deviceExtension,
                 &deviceExtension->CurrentInput
                 )) {

            //
            // The InputData queue overflowed.  There is
            // not much that can be done about it, so just
            // continue (but don't queue the ISR DPC, since
            // no new packets were added to the queue).
            //
            // Queue a DPC to log an overrun error.
            //

            IsrPrint(DBG_KBISR_ERROR, ("queue overflow\n"));

            if (deviceExtension->OkayToLogOverflow) {
                KeInsertQueueDpc(
                    &deviceExtension->ErrorLogDpc,
                    (PIRP) NULL,
                    (PVOID) (ULONG) I8042_KBD_BUFFER_OVERFLOW
                    );
                deviceExtension->OkayToLogOverflow = FALSE;
            }

        } else if (deviceExtension->DpcInterlockKeyboard >= 0) {

           //
           // The ISR DPC is already executing.  Tell the ISR DPC
           // it has more work to do by incrementing
           // DpcInterlockKeyboard.
           //

           deviceExtension->DpcInterlockKeyboard += 1;

        } else {

            //
            // Queue the ISR DPC.
            //

            KeInsertQueueDpc(
                &deviceExtension->KeyboardIsrDpc,
                DeviceObject->CurrentIrp,
                NULL
                );
        }
    }

    //
    // Reset the input state.
    //
    deviceExtension->CurrentInput.Flags = 0;
}

VOID
I8xKeyboardServiceParameters(
    IN PUNICODE_STRING          RegistryPath,
    IN PPORT_KEYBOARD_EXTENSION KeyboardExtension
    )
/*++

Routine Description:

    This routine retrieves this driver's service parameters information
    from the registry.  Overrides these values if they are present in the
    devnode.

Arguments:

    RegistryPath - Pointer to the null-terminated Unicode name of the
        registry path for this driver.

    KeyboardExtension - Keyboard extension
    
Return Value:

    None.  

--*/
{
    NTSTATUS                            status = STATUS_SUCCESS;
    PI8042_CONFIGURATION_INFORMATION    configuration;
    PRTL_QUERY_REGISTRY_TABLE           parameters = NULL;
    PWSTR                               path = NULL;
    ULONG                               defaultDataQueueSize = DATA_QUEUE_SIZE;
    ULONG                               invalidKeyboardSubtype = (ULONG) -1;
    ULONG                               invalidKeyboardType = 0;
    ULONG                               overrideKeyboardSubtype = (ULONG) -1;
    ULONG                               overrideKeyboardType = 0;
    ULONG                               pollStatusIterations = 0;
    ULONG                               defaultPowerCaps = 0x0, powerCaps = 0x0;
    ULONG                               i = 0;
    UNICODE_STRING                      parametersPath;
    HANDLE                              keyHandle;
    ULONG                               defaultPollStatusIterations = I8042_POLLING_DEFAULT;

    ULONG                               crashOnCtrlScroll = 0,
                                        defaultCrashOnCtrlScroll = 0;

#if defined(NEC_98)
    ULONG                               vfKeyEmulation = FALSE;
    ULONG                               defaultVfKeyEmulation = FALSE;
    USHORT                              queries = 8; 
#else
    USHORT                              queries = 7;
#endif

    PAGED_CODE();

#if I8042_VERBOSE
    queries += 2;
#endif 
    
    configuration = &(Globals.ControllerData->Configuration);
    parametersPath.Buffer = NULL;

    //
    // Registry path is already null-terminated, so just use it.
    //
    path = RegistryPath->Buffer;

    if (NT_SUCCESS(status)) {

        //
        // Allocate the Rtl query table.
        //
        parameters = ExAllocatePool(
            PagedPool,
            sizeof(RTL_QUERY_REGISTRY_TABLE) * (queries + 1)
            );

        if (!parameters) {

            Print(DBG_SS_ERROR,
                 ("%s: couldn't allocate table for Rtl query to %ws for %ws\n",
                 pFncServiceParameters,
                 pwParameters,
                 path
                 ));
            status = STATUS_UNSUCCESSFUL;

        } else {

            RtlZeroMemory(
                parameters,
                sizeof(RTL_QUERY_REGISTRY_TABLE) * (queries + 1)
                );

            //
            // Form a path to this driver's Parameters subkey.
            //
            RtlInitUnicodeString( &parametersPath, NULL );
            parametersPath.MaximumLength = RegistryPath->Length +
                (wcslen(pwParameters) * sizeof(WCHAR) ) + sizeof(UNICODE_NULL);

            parametersPath.Buffer = ExAllocatePool(
                PagedPool,
                parametersPath.MaximumLength
                );

            if (!parametersPath.Buffer) {

                Print(DBG_SS_ERROR,
                     ("%s: Couldn't allocate string for path to %ws for %ws\n",
                     pFncServiceParameters,
                     pwParameters,
                     path
                     ));
                status = STATUS_UNSUCCESSFUL;

            }
        }
    }

    if (NT_SUCCESS(status)) {

        //
        // Form the parameters path.
        //

        RtlZeroMemory(
            parametersPath.Buffer,
            parametersPath.MaximumLength
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            path
            );
        RtlAppendUnicodeToString(                             
            &parametersPath,
            pwParameters
            );

        //
        // Gather all of the "user specified" information from
        // the registry.
        //
        parameters[i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwKeyboardDataQueueSize;
        parameters[i].EntryContext =
            &KeyboardExtension->KeyboardAttributes.InputDataQueueLength;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &defaultDataQueueSize;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwOverrideKeyboardType;
        parameters[i].EntryContext = &overrideKeyboardType;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &invalidKeyboardType;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwOverrideKeyboardSubtype;
        parameters[i].EntryContext = &overrideKeyboardSubtype;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &invalidKeyboardSubtype;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwPollStatusIterations;
        parameters[i].EntryContext = &pollStatusIterations;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &defaultPollStatusIterations;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwPowerCaps; 
        parameters[i].EntryContext = &powerCaps;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &defaultPowerCaps;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = L"CrashOnCtrlScroll";
        parameters[i].EntryContext = &crashOnCtrlScroll;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &defaultCrashOnCtrlScroll;
        parameters[i].DefaultLength = sizeof(ULONG);

#if defined(NEC_98)
        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwVfKeyEmulation;
        parameters[i].EntryContext = &vfKeyEmulation;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &defaultVfKeyEmulation;
        parameters[i].DefaultLength = sizeof(ULONG);
#endif // defined(NEC_98)

        status = RtlQueryRegistryValues(
            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
            parametersPath.Buffer,
            parameters,
            NULL,
            NULL
            );

        if (!NT_SUCCESS(status)) {
            Print(DBG_SS_INFO,
                 ("kb RtlQueryRegistryValues failed (0x%x)\n",
                 status
                 ));
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // Go ahead and assign driver defaults.
        //
        configuration->PollStatusIterations = (USHORT)
            defaultPollStatusIterations;
        KeyboardExtension->KeyboardAttributes.InputDataQueueLength =
            defaultDataQueueSize;

#ifdef NEC_98
        KeyboardExtension->VfKeyEmulation = defaultVfKeyEmulation;
#endif // NEC_98

    }
    else {
        configuration->PollStatusIterations = (USHORT) pollStatusIterations;

#ifdef NEC_98
        KeyboardExtension->VfKeyEmulation = vfKeyEmulation;
#endif // NEC_98

    }

    status = IoOpenDeviceRegistryKey(KeyboardExtension->PDO,
                                     PLUGPLAY_REGKEY_DEVICE, 
                                     STANDARD_RIGHTS_READ,
                                     &keyHandle
                                     );

    if (NT_SUCCESS(status)) {
        //
        // If the value is not present in devnode, then the default is the value
        // read in from the Services\i8042prt\Parameters key
        //
        ULONG prevInputDataQueueLength,
              prevPowerCaps,
              prevOverrideKeyboardType,
              prevOverrideKeyboardSubtype,
              prevPollStatusIterations;
#ifdef NEC_98
        ULONG prevVfKeyEmulation = vfKeyEmulation;
#endif // NEC_98

        prevInputDataQueueLength =
            KeyboardExtension->KeyboardAttributes.InputDataQueueLength;
        prevPowerCaps = powerCaps;
        prevOverrideKeyboardType = overrideKeyboardType;
        prevOverrideKeyboardSubtype = overrideKeyboardSubtype;
        prevPollStatusIterations = pollStatusIterations;

        RtlZeroMemory(
            parameters,
            sizeof(RTL_QUERY_REGISTRY_TABLE) * (queries + 1)
            );

        i = 0;

        //
        // Gather all of the "user specified" information from
        // the registry (this time from the devnode)
        //
        parameters[i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwKeyboardDataQueueSize;
        parameters[i].EntryContext =
            &KeyboardExtension->KeyboardAttributes.InputDataQueueLength;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &prevInputDataQueueLength;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwOverrideKeyboardType;
        parameters[i].EntryContext = &overrideKeyboardType;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &prevOverrideKeyboardType;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwOverrideKeyboardSubtype;
        parameters[i].EntryContext = &overrideKeyboardSubtype;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &prevOverrideKeyboardSubtype;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwPollStatusIterations;
        parameters[i].EntryContext = &pollStatusIterations;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &prevPollStatusIterations;
        parameters[i].DefaultLength = sizeof(ULONG);

        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwPowerCaps, 
        parameters[i].EntryContext = &powerCaps;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &prevPowerCaps;
        parameters[i].DefaultLength = sizeof(ULONG);

#ifdef NEC_98 
        parameters[++i].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[i].Name = pwVfKeyEmulation;
        parameters[i].EntryContext = &vfKeyEmulation;
        parameters[i].DefaultType = REG_DWORD;
        parameters[i].DefaultData = &prevVfKeyEmulation;
        parameters[i].DefaultLength = sizeof(ULONG);
#endif // NEC_98 

        status = RtlQueryRegistryValues(
                    RTL_REGISTRY_HANDLE,
                    (PWSTR) keyHandle, 
                    parameters,
                    NULL,
                    NULL
                    );

        if (!NT_SUCCESS(status)) {
            Print(DBG_SS_INFO,
                  ("kb RtlQueryRegistryValues (via handle) failed (0x%x)\n",
                  status
                  ));
        }

        ZwClose(keyHandle);
    }
    else {
        Print(DBG_SS_INFO | DBG_SS_ERROR,
             ("kb, opening devnode handle failed (0x%x)\n",
             status
             ));
    }

    Print(DBG_SS_NOISE, ("I8xKeyboardServiceParameters results..\n"));

    Print(DBG_SS_NOISE,
          (pDumpDecimal,
          pwPollStatusIterations,
          configuration->PollStatusIterations
          ));

    if (KeyboardExtension->KeyboardAttributes.InputDataQueueLength == 0) {

        Print(DBG_SS_INFO | DBG_SS_ERROR,
             ("\toverriding %ws = 0x%x\n",
             pwKeyboardDataQueueSize,
             KeyboardExtension->KeyboardAttributes.InputDataQueueLength
             ));

        KeyboardExtension->KeyboardAttributes.InputDataQueueLength =
            defaultDataQueueSize;

    }
    KeyboardExtension->KeyboardAttributes.InputDataQueueLength *=
        sizeof(KEYBOARD_INPUT_DATA);

    KeyboardExtension->PowerCaps = (UCHAR) (powerCaps & I8042_SYS_BUTTONS);
    Print(DBG_SS_NOISE, (pDumpHex, pwPowerCaps, KeyboardExtension->PowerCaps));

    if (overrideKeyboardType != invalidKeyboardType) {

        if (overrideKeyboardType <= NUM_KNOWN_KEYBOARD_TYPES) {

            Print(DBG_SS_NOISE,
                 (pDumpDecimal,
                 pwOverrideKeyboardType,
                 overrideKeyboardType
                 ));

            KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Type =
                (UCHAR) overrideKeyboardType;

        } else {

            Print(DBG_SS_NOISE,
                 (pDumpDecimal,
                 pwOverrideKeyboardType,
                 overrideKeyboardType
                 ));

        }

    }

    if (overrideKeyboardSubtype != invalidKeyboardSubtype) {

        Print(DBG_SS_NOISE,
             (pDumpDecimal,
             pwOverrideKeyboardSubtype,
             overrideKeyboardSubtype
             ));

        KeyboardExtension->KeyboardAttributes.KeyboardIdentifier.Subtype =
            (UCHAR) overrideKeyboardSubtype;

    }

    if (crashOnCtrlScroll) {
        Print(DBG_SS_INFO, ("Crashing on Ctrl + Scroll Lock\n"));

        //
        // Index into the KeyToScanTbl array in the keyboard ISR
        //
        KeyboardExtension->Dump2Key = 125;

        //
        // The right control key
        //
        KeyboardExtension->Dump1Keys = 0x02;
    }


    //
    // Free the allocated memory before returning.
    //
    if (parametersPath.Buffer)
        ExFreePool(parametersPath.Buffer);
    if (parameters)
        ExFreePool(parameters);
}

