/*++
Copyright (c) 1997  Microsoft Corporation

Module Name:

    ENUM.C

Abstract:

    This module contains the enumeration code needed to figure out
    whether or not a device is attached to the serial port.  If there
    is one, it will obtain the PNP COM ID (if the device is PNP) and
    parse out the relevant fields.


Environment:

    kernel mode only

Notes:



--*/

#include "pch.h"

#define MAX_DEVNODE_NAME        256 // Total size of Device ID

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESENM, Serenum_ReenumerateDevices)
#pragma alloc_text(PAGESENM, SerenumScanOtherIdForMouse)
#pragma alloc_text(PAGESENM, Serenum_IoSyncReq)
#pragma alloc_text(PAGESENM, Serenum_IoSyncReqWithIrp)
#pragma alloc_text(PAGESENM, Serenum_IoSyncIoctlEx)
#pragma alloc_text(PAGESENM, Serenum_ReadSerialPort)
//#pragma alloc_text (PAGE, Serenum_PDO_EnumMarkMissing)
#pragma alloc_text(PAGESENM, Serenum_Wait)

#pragma alloc_text(PAGESENM, Serenum_PollingRoutine)

//#pragma alloc_text (PAGE, Serenum_GetRegistryKeyValue)
#endif

#if !defined(__isascii)
#define __isascii(_c)   ( (unsigned)(_c) < 0x80 )
#endif // !defined(__isascii)

void
SerenumScanOtherIdForMouse(IN PCHAR PBuffer, IN ULONG BufLen,
                           OUT PCHAR *PpMouseId)
/*++

Routine Description:

    This routines a PnP packet for a mouse ID up to the first PnP delimiter
    (i.e, '(').

Arguments:

   PBuffer - Pointer to the buffer to scan

   BufLen - Length of the buffer in bytes

   PpMouseId - Pointer to the pointer to the mouse ID (this will be set
               to point to the location in the buffer where the mouse ID
               was found)

Return value:

    void

--*/
{
   PAGED_CODE();

   *PpMouseId = PBuffer;

   while (BufLen--) {
      if (**PpMouseId == 'M' || **PpMouseId == 'B') {
         return;
      } else if (**PpMouseId == '(' || **PpMouseId == ('(' - 0x20)) {
         *PpMouseId = NULL;
         return;
      }
      (*PpMouseId)++;
   }

   *PpMouseId = NULL;
}

NTSTATUS
Serenum_ReenumerateDevices(IN PIRP Irp, IN PFDO_DEVICE_DATA FdoData)
/*++

Routine Description:

    This enumerates the serenum bus which is represented by Fdo (a pointer
    to the device object representing the serial bus). It creates new PDOs
    for any new devices which have been discovered since the last enumeration

Arguments:

    FdoData - Pointer to the fdo's device extension
                for the serial bus which needs to be enumerated
    Irp - Pointer to the Irp which was sent to reenumerate.

Return value:

    NTSTATUS

--*/
{
   PIRP NewIrp;
   NTSTATUS status;
   KEVENT event;
   KTIMER timer;

   IO_STATUS_BLOCK IoStatusBlock;
   UNICODE_STRING pdoUniName;
   PDEVICE_OBJECT pdo = FdoData->AttachedPDO;
   PPDO_DEVICE_DATA pdoData;
   PIO_STACK_LOCATION stack;

   UNICODE_STRING HardwareIDs;
   UNICODE_STRING CompIDs;
   UNICODE_STRING DeviceIDs;
   UNICODE_STRING DevDesc;

   LARGE_INTEGER defaultTime = RtlConvertLongToLargeInteger (-2000000);
   ULONG defaultSerialTime = 240;
   LARGE_INTEGER startingOffset = RtlConvertLongToLargeInteger (0);

   ULONG bitMask;
   SERIAL_BAUD_RATE baudRate;
   SERIAL_LINE_CONTROL lineControl;
   SERIAL_HANDFLOW handflow;

   BOOLEAN legacyPotential = FALSE;
   BOOLEAN legacyFound = FALSE;

   USHORT nActual = 0;
   ULONG i;

   PCHAR ReadBuffer = NULL;
   WCHAR pdoName[] = SERENUM_PDO_NAME_BASE;

   ULONG FdoFlags = FdoData->Self->Flags;

   SERIAL_BASIC_SETTINGS basicSettings;
   BOOLEAN basicSettingsDone = FALSE;
   SERIAL_TIMEOUTS timeouts, newTimeouts;

   PAGED_CODE();

   //
   // Initialization
   //

   RtlInitUnicodeString(&pdoUniName, pdoName);
   pdoName [((sizeof(pdoName)/sizeof(WCHAR)) - 2)] =
      L'0' + FdoData->PdoIndex++;
   KeInitializeEvent(&event, NotificationEvent, FALSE);
   KeInitializeTimer(&timer);

   RtlInitUnicodeString(&HardwareIDs, NULL);
   RtlInitUnicodeString(&CompIDs, NULL);
   RtlInitUnicodeString(&DeviceIDs, NULL);
   RtlInitUnicodeString(&DevDesc, NULL);

   //
   // If the current PDO should be marked missing, do so.
   //
   if (FdoData->PDOForcedRemove) {

       Serenum_PDO_EnumMarkMissing(FdoData, pdo->DeviceExtension);
       pdo = NULL;
   }

   //
   // Open the Serial port before sending Irps down
   // Use the Irp passed to us, and grab it on the way up.
   // Must save away the system buffer.  Use the stack space.
   //

   stack = IoGetCurrentIrpStackLocation( Irp );
   stack->Parameters.Others.Argument1 = Irp->AssociatedIrp.SystemBuffer;

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Opening the serial port...\n"));

   status = Serenum_IoSyncReqWithIrp(Irp, IRP_MJ_CREATE, &event,
                                     FdoData->TopOfStack);

   //
   // If we cannot open the stack, odd's are we have a live and started PDO on
   // it. Since enumeration might interfere with running devices, we do not
   // adjust our list of children if we cannot open the stack.
   //
   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                      ("Failed to open the serial port...\n"));
      Irp->AssociatedIrp.SystemBuffer = stack->Parameters.Others.Argument1;
      return status;
   }

   //
   // Set up the COM port
   //

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Setting up port\n"));

   status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_INTERNAL_BASIC_SETTINGS, TRUE,
                                  FdoData->TopOfStack, &event, &IoStatusBlock,
                                  NULL, 0, &basicSettings,
                                  sizeof(basicSettings));

   if (NT_SUCCESS(status)) {
      basicSettingsDone = TRUE;
   } else {
      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_GET_TIMEOUTS, FALSE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            NULL, 0, &timeouts, sizeof(timeouts));

      RtlZeroMemory(&newTimeouts, sizeof(newTimeouts));

      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_TIMEOUTS, FALSE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            &newTimeouts, sizeof(newTimeouts), NULL, 0);
   }

   //
   // Set DTR
   //
   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Setting DTR...\n"));
   status = Serenum_IoSyncIoctl(IOCTL_SERIAL_SET_DTR, FALSE,
                                FdoData->TopOfStack, &event, &IoStatusBlock);

   if (!NT_SUCCESS(status)) {
      goto EnumerationDone;
   }

   //
   // Clear RTS
   //
   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Clearing RTS...\n"));
   status = Serenum_IoSyncIoctl(IOCTL_SERIAL_CLR_RTS, FALSE,
                                FdoData->TopOfStack, &event, &IoStatusBlock);

   if (!NT_SUCCESS(status)) {
      goto EnumerationDone;
   }

   //
   // Set a timer for 200 ms
   //

   status = Serenum_Wait(&timer, defaultTime);

   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                      ("Timer failed with status %x\n", status ));

      goto EnumerationDone;
   }

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Checking DSR...\n"));

   status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_GET_MODEMSTATUS, FALSE,
                                  FdoData->TopOfStack, &event, &IoStatusBlock,
                                  NULL, 0, &bitMask, sizeof(ULONG));

   if (!NT_SUCCESS(status)) {
      goto EnumerationDone;
   }

   if ((SERIAL_DSR_STATE & bitMask) == 0) {
      Serenum_KdPrint (FdoData, SER_DBG_SS_TRACE,
                       ("No PNP device available - DSR not set.\n"));
      legacyPotential = TRUE;
   }

   //
   // Setup the serial port for 1200 bits/s, 7 data bits,
   // no parity, one stop bit
   //
   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Setting baud rate to 1200..."
                                               "\n"));
   baudRate.BaudRate = 1200;
   status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_BAUD_RATE, FALSE,
                                  FdoData->TopOfStack, &event, &IoStatusBlock,
                                  &baudRate, sizeof(SERIAL_BAUD_RATE), NULL, 0);
   if (!NT_SUCCESS(status)) {
      goto EnumerationDone;
   }

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                   ("Setting the line control...\n"));

   lineControl.StopBits = STOP_BIT_1;
   lineControl.Parity = NO_PARITY;
   lineControl.WordLength = 7;

   status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_LINE_CONTROL, FALSE,
                                  FdoData->TopOfStack, &event, &IoStatusBlock,
                                  &lineControl, sizeof(SERIAL_LINE_CONTROL),
                                  NULL, 0);

   if (!NT_SUCCESS(status)) {
      goto EnumerationDone;
   }



   ReadBuffer = ExAllocatePool(NonPagedPool, MAX_DEVNODE_NAME);

   if (ReadBuffer == NULL) {
      Irp->AssociatedIrp.SystemBuffer = stack->Parameters.Others.Argument1;
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   //
   // loop twice
   // The first iteration is for reading the PNP ID string from modems
   // and mice.
   // The second iteration is for other devices.
   //
   for (i = 0; i < 2; i++) {
      //
      // Purge the buffers before reading
      //

      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Purging all buffers...\n"));

      bitMask = SERIAL_PURGE_RXCLEAR;

      status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_PURGE, FALSE,
                                     FdoData->TopOfStack, &event,
                                     &IoStatusBlock, &bitMask, sizeof(ULONG),
                                     NULL, 0);

      if (!NT_SUCCESS(status)) {
         goto EnumerationDone;
      }

      //
      // Clear DTR
      //
      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Clearing DTR...\n"));

      status = Serenum_IoSyncIoctl(IOCTL_SERIAL_CLR_DTR, FALSE,
                                   FdoData->TopOfStack, &event, &IoStatusBlock);

      if (!NT_SUCCESS(status)) {
         goto EnumerationDone;
      }

      //
      // Clear RTS
      //
      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Clearing RTS...\n"));
      status = Serenum_IoSyncIoctl(IOCTL_SERIAL_CLR_RTS, FALSE,
                                   FdoData->TopOfStack, &event, &IoStatusBlock);

      if (!NT_SUCCESS(status)) {
         goto EnumerationDone;
      }

      //
      // Set a timer for 200 ms
      //

      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Waiting...\n"));

      status = Serenum_Wait(&timer, defaultTime);

      if (!NT_SUCCESS(status)) {
         Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                         ("Timer failed with status %x\n", status ));
         goto EnumerationDone;
      }

      //
      // set DTR
      //

      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Setting DTR...\n"));

      status = Serenum_IoSyncIoctl(IOCTL_SERIAL_SET_DTR, FALSE,
                                   FdoData->TopOfStack, &event, &IoStatusBlock);

      if (!NT_SUCCESS(status)) {
         goto EnumerationDone;
      }

      //
      // First iteration is for modems
      // Therefore wait for 200 ms as per protocol for getting PNP string out
      //

      if (!i) {
         status = Serenum_Wait(&timer, defaultTime);
         if (!NT_SUCCESS(status)) {
            Serenum_KdPrint (FdoData, SER_DBG_SS_ERROR,
                             ("Timer failed with status %x\n", status ));
            goto EnumerationDone;
         }
      }

      //
      // set RTS
      //

      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Setting RTS...\n"));

      status = Serenum_IoSyncIoctl(IOCTL_SERIAL_SET_RTS, FALSE,
                                   FdoData->TopOfStack, &event, &IoStatusBlock);

      if (!NT_SUCCESS(status)) {
         goto EnumerationDone;
      }

      //
      // Read from the serial port
      //
      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                      ("Reading the serial port...\n"));

      Serenum_KdPrint(FdoData, SER_DBG_SS_INFO, ("Address: %x\n", ReadBuffer));

      nActual = 0;

#if DBG
      RtlFillMemory(ReadBuffer, MAX_DEVNODE_NAME, 0xff);
#endif

      //
      // Flush the input buffer
      //

      status = Serenum_ReadSerialPort(ReadBuffer, MAX_DEVNODE_NAME,
                                      defaultSerialTime, &nActual,
                                      &IoStatusBlock, FdoData);

      switch (status) {
      case STATUS_TIMEOUT:
         if (nActual == 0) {
            Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                      ("Timeout with no bytes read; continuing\n"));
            continue;
         }
         break;

      case STATUS_SUCCESS:
         Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                         ("Read succeeded\n"));
         goto EnumerationDone;
         break;

      default:
         Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                         ("Read failed with 0x%x\n", status));
         goto EnumerationDone;
         break;
      }

      //
      // If anything was read from the serial port, we're done!
      //

      if (nActual) {
         break;
      }
   }

   //
   // If DSR wasn't set, then a jump will be made here, and any existing
   // pdos will be eliminated
   //

EnumerationDone:;

   if (basicSettingsDone) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                      ("Restoring basic settings\n"));

      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_INTERNAL_RESTORE_SETTINGS, TRUE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            &basicSettings, sizeof(basicSettings), NULL, 0);
   } else {
      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_TIMEOUTS, FALSE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            &timeouts, sizeof(timeouts), NULL, 0);
   }

   //
   // Cleanup and then Close
   //
   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                   ("Cleanup on the serial port...\n"));

   status = Serenum_IoSyncReqWithIrp(Irp, IRP_MJ_CLEANUP, &event,
                                     FdoData->TopOfStack);

#if DBG
   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                      ("Failed to cleanup the serial port...\n"));
      // don't return because we want to attempt to close!
   }
#endif

   //
   // Close the Serial port after everything is done
   //

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                   ("Closing the serial port...\n"));

   status = Serenum_IoSyncReqWithIrp(Irp, IRP_MJ_CLOSE, &event,
                                     FdoData->TopOfStack);

   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                      ("Failed to close the serial port...\n"));
      Irp->AssociatedIrp.SystemBuffer = stack->Parameters.Others.Argument1;
      return status;
   }

   //
   // Check if anything was read, and if not, we're done
   //

   if (nActual == 0) {
      if (ReadBuffer != NULL) {
         ExFreePool(ReadBuffer);
      }

      if (pdo != NULL) {
         //
         // Something was there.  The device must have been unplugged.
         // Remove the PDO.
         //

         Serenum_PDO_EnumMarkMissing(FdoData, pdo->DeviceExtension);
         pdo = NULL;
      }

      goto ExitReenumerate;
   }

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                   ("Something was read from the serial port...\n"));



#if DBG
   if (FdoData->DebugLevel & SER_DBG_PNP_DUMP_PACKET) {
      ULONG dmpi;
      ULONG col;

      DbgPrint("SERENUM: Raw Data Packet on probe\n");


      for (dmpi = 0; dmpi < ((ULONG)nActual / 20); dmpi++) {
         DbgPrint("-------: ");
         for (col = 0; col < 20; col++) {
            DbgPrint("%02x ", (unsigned char)ReadBuffer[dmpi * 20 + col]);
         }

         DbgPrint(" ");
         for (col = 0; col < 20; col++) {
            if (__isascii(ReadBuffer[dmpi * 20 + col])
                && (ReadBuffer[dmpi * 20 + col] > ' ')) {
               DbgPrint("%c", ReadBuffer[dmpi * 20 + col]);
            } else {
               DbgPrint(".");
            }
         }

         DbgPrint("\n");
      }

      if (nActual % 20) {

         DbgPrint("-------: ");

         for (col = 0; col < ((ULONG)nActual % 20); col++) {
            DbgPrint("%02x ", (unsigned char)ReadBuffer[dmpi * 20 + col]);
         }

         for (col = 0; col < (20 - ((ULONG)nActual % 20)); col++) {
            DbgPrint("   ");
         }

         for (col = 0; col < ((ULONG)nActual % 20); col++) {
            if (__isascii(ReadBuffer[dmpi * 20 + col])
                && (ReadBuffer[dmpi * 20 + col] > ' ')) {
               DbgPrint("%c", ReadBuffer[dmpi * 20 + col]);
            } else {
               DbgPrint(".");
            }

         }

         DbgPrint("\n");
      }


   }
#endif

   //
   // Determine from the result whether the current pdo (if we have one),
   // should be deleted.  If it's the same device, then keep it.  If it's a
   // different device or if the device is a legacy device, then create a
   // new pdo.
   //
   if (legacyPotential) {

      PCHAR mouseId = ReadBuffer;
      ULONG charCnt;

      SerenumScanOtherIdForMouse(ReadBuffer, nActual, &mouseId);

      if (mouseId != NULL) {
         //
         // A legacy device is attached to the serial port, since DSR was
         // not set when RTS was set.
         // If we find a mouse from the readbuffer, copy the appropriate
         // strings into the hardwareIDs and CompIDs manually.
         //
         if (*mouseId == 'M') {
            if ((mouseId - ReadBuffer) > 1 && mouseId[1] == '3') {
               Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("*PNP0F08 mouse\n"));
               Serenum_InitMultiString(FdoData, &HardwareIDs, "*PNP0F08",
                                       NULL);
               Serenum_InitMultiString(FdoData, &CompIDs, "SERIAL_MOUSE",
                                       NULL);
               //
               // ADRIAO CIMEXCIMEX 04/28/1999 -
               //     Device ID's should be unique, at least as unique as the
               // hardware ID's. This ID should really be Serenum\\PNP0F08
               //
               Serenum_InitMultiString(FdoData, &DeviceIDs, "Serenum\\Mouse",
                                       NULL);
               legacyFound = TRUE;

               Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                               ("Buffers at 0x%x 0x%x 0x%x\n",
                                HardwareIDs.Buffer, CompIDs.Buffer,
                                DeviceIDs.Buffer));

            } else {
               Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("*PNP0F01 mouse\n"));
               Serenum_InitMultiString(FdoData, &HardwareIDs, "*PNP0F01", NULL);
               Serenum_InitMultiString(FdoData, &CompIDs, "SERIAL_MOUSE", NULL);
               //
               // ADRIAO CIMEXCIMEX 04/28/1999 -
               //     Device ID's should be unique, at least as unique as the
               // hardware ID's. This ID should really be Serenum\\PNP0F01
               //
               Serenum_InitMultiString(FdoData, &DeviceIDs, "Serenum\\Mouse",
                                       NULL);
               legacyFound = TRUE;

               Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                               ("Buffers at 0x%x 0x%x 0x%x\n",
                                HardwareIDs.Buffer, CompIDs.Buffer,
                                DeviceIDs.Buffer));
            }
         } else if (*mouseId == 'B') {
            Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("*PNP0F09 mouse\n"));
            Serenum_InitMultiString(FdoData, &HardwareIDs, "*PNP0F09", NULL);
            Serenum_InitMultiString(FdoData, &CompIDs, "*PNP0F0F",
                                    "SERIAL_MOUSE", NULL);
            //
            // ADRIAO CIMEXCIMEX 04/28/1999 -
            //     Device ID's should be unique, at least as unique as the
            // hardware ID's. This ID should really be Serenum\\PNP0F09
            //
            Serenum_InitMultiString(FdoData, &DeviceIDs, "Serenum\\BallPoint",
                                    NULL);
            legacyFound = TRUE;

            Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                            ("Buffers at 0x%x 0x%x 0x%x\n",
                             HardwareIDs.Buffer, CompIDs.Buffer,
                             DeviceIDs.Buffer));
         }
      }
   }

   if (!legacyFound) {
      //
      // A legacy device wasn't found.  Even if DSR wasn't set, it could
      // just be a broken PNP device, so check here by parsing the
      // PNP COM ID
      //
      if (!NT_SUCCESS(Serenum_ParseData(FdoData, ReadBuffer, nActual,
                                        &HardwareIDs, &CompIDs, &DeviceIDs,
                                        &DevDesc))) {
         Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                         ("Failed to parse the data for the new device\n"));

         //
         // Legacy device.  Remove the current pdo and make a new one
         // so that always pops up message asking for new device driver.
         //

         if (pdo) {
            Serenum_PDO_EnumMarkMissing(FdoData, pdo->DeviceExtension);
            pdo = NULL;
         }

         ExFreePool(ReadBuffer);

         goto ExitReenumerate;
      }
   }

   //
   // We're now finally able to free this read buffer.
   //

   if (ReadBuffer != NULL) {
      ExFreePool(ReadBuffer);
   }

   //
   // Check if the current device is the same as the one that we're
   // enumerating.  If so, we'll just keep the current pdo.
   //
   if (pdo) {
      pdoData = pdo->DeviceExtension;

      //
      // ADRIAO CIMEXCIMEX 04/28/1999 -
      //     We should be comparing device ID's here, but the above mentioned
      // bug must be fixed first. Note that even this code is broken as it
      // doesn't take into account that hardware/compID's are multiSz.
      //

      if (!(RtlEqualUnicodeString(&pdoData->HardwareIDs, &HardwareIDs, FALSE)
            && RtlEqualUnicodeString(&pdoData->CompIDs, &CompIDs, FALSE))) {
         //
         // The ids are not the same, so get rid of this pdo and create a
         // new one so that the PNP system will query the ids and find a
         // new driver
         //
         Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Different device."
                                                     " Removing PDO %x\n",
                                                     pdo));
         Serenum_PDO_EnumMarkMissing(FdoData, pdoData);
         pdo = NULL;
      } else {
         Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                         ("Same device. Keeping current Pdo %x\n", pdo));
      }
   }

   //
   // If there isn't a pdo, then create one!
   //
   if (!pdo) {
      //
      // Allocate a pdo
      //
      status = IoCreateDevice(FdoData->Self->DriverObject,
                              sizeof(PDO_DEVICE_DATA), &pdoUniName,
                              FILE_DEVICE_UNKNOWN,
                              FILE_AUTOGENERATED_DEVICE_NAME, FALSE, &pdo);

      if (!NT_SUCCESS(status)) {
         Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR, ("Create device failed\n"));
         Irp->AssociatedIrp.SystemBuffer = stack->Parameters.Others.Argument1;
         return status;
      }

      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE,
                      ("Created PDO on top of filter: %x\n",pdo));

      pdoData = pdo->DeviceExtension;

      //
      // Initialize the rest of the device object
      //
      // Set this to potential, not found, since this is used for dynamic
      // detection using the DSR bit.  There could be a broken pnp device
      // attached that isn't able to set DSR.
      //

      FdoData->PDOLegacyEnumerated = legacyPotential;

      //
      // Copy our temp buffers over to the DevExt
      //

      pdoData->HardwareIDs = HardwareIDs;
      pdoData->CompIDs = CompIDs;
      pdoData->DeviceIDs = DeviceIDs;
      pdoData->DevDesc = DevDesc;

      Serenum_InitPDO(pdo, FdoData);
   }

   // TODO: Poll

ExitReenumerate:;

   stack = IoGetCurrentIrpStackLocation(Irp);
   Irp->AssociatedIrp.SystemBuffer = stack->Parameters.Others.Argument1;

   return STATUS_SUCCESS;
}

void
Serenum_PDO_EnumMarkMissing(PFDO_DEVICE_DATA FdoData, PPDO_DEVICE_DATA PdoData)
/*++

Routine Description:
    Removes the attached pdo from the fdo's list of children.

    NOTE: THIS FUNCTION CAN ONLY BE CALLED DURING AN ENUMERATION. If called
          outside of enumeration, Serenum might delete it's PDO before PnP has
          been told the PDO is gone.

Arguments:
    FdoData - Pointer to the fdo's device extension
    PdoData - Pointer to the pdo's device extension

Return value:
    none

--*/
{
    Serenum_KdPrint (FdoData, SER_DBG_SS_TRACE, ("Removing Pdo %x\n",
                                                 PdoData->Self));

    ASSERT(PdoData->Attached);
    PdoData->Attached = FALSE;
    FdoData->AttachedPDO = NULL;
    FdoData->PdoData = NULL;
    FdoData->NumPDOs = 0;
    FdoData->PDOForcedRemove = FALSE;
}

NTSTATUS
Serenum_IoSyncReqWithIrp(PIRP PIrp, UCHAR MajorFunction, PKEVENT PEvent,
                         PDEVICE_OBJECT PDevObj )
/*++

Routine Description:
    Performs a synchronous IO request by waiting on the event object
    passed to it.  The IRP isn't deallocated after this call.

Arguments:
    PIrp - The IRP to be used for this request

    MajorFunction - The major function

    PEvent - An event used to wait for the IRP

    PDevObj - The object that we're performing the IO request upon

Return value:
    NTSTATUS

--*/
{
    PIO_STACK_LOCATION stack;
    NTSTATUS status;

    stack = IoGetNextIrpStackLocation(PIrp);

    stack->MajorFunction = MajorFunction;

    KeClearEvent(PEvent);

    IoSetCompletionRoutine(PIrp, Serenum_EnumComplete, PEvent, TRUE,
                           TRUE, TRUE);

    status = Serenum_IoSyncReq(PDevObj, PIrp, PEvent);

    if (status == STATUS_SUCCESS) {
       status = PIrp->IoStatus.Status;
    }

    return status;
}

NTSTATUS
Serenum_IoSyncIoctlEx(ULONG Ioctl, BOOLEAN Internal, PDEVICE_OBJECT PDevObj,
                      PKEVENT PEvent, PIO_STATUS_BLOCK PIoStatusBlock,
                      PVOID PInBuffer, ULONG InBufferLen, PVOID POutBuffer,                    // output buffer - optional
                      ULONG OutBufferLen)
/*++

Routine Description:
    Performs a synchronous IO control request by waiting on the event object
    passed to it.  The IRP is deallocated by the IO system when finished.

Return value:
    NTSTATUS

--*/
{
    PIRP pIrp;
    NTSTATUS status;

    KeClearEvent(PEvent);

    // Allocate an IRP - No need to release
    // When the next-lower driver completes this IRP, the IO Mgr releases it.

    pIrp = IoBuildDeviceIoControlRequest(Ioctl, PDevObj, PInBuffer, InBufferLen,
                                         POutBuffer, OutBufferLen, Internal,
                                         PEvent, PIoStatusBlock);

    if (pIrp == NULL) {
        Serenum_KdPrint_Def (SER_DBG_SS_ERROR, ("Failed to allocate IRP\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = Serenum_IoSyncReq(PDevObj, pIrp, PEvent);


    if (status == STATUS_SUCCESS) {
       status = PIoStatusBlock->Status;
    }

    return status;
}


NTSTATUS
Serenum_IoSyncReq(PDEVICE_OBJECT PDevObj, IN PIRP PIrp, PKEVENT PEvent)
/*++

Routine Description:
    Performs a synchronous IO request by waiting on the event object
    passed to it.  The IRP is deallocated by the IO system when finished.

Return value:
    NTSTATUS

--*/
{
   NTSTATUS status;

   status = IoCallDriver(PDevObj, PIrp);

   if (status == STATUS_PENDING) {
      // wait for it...
      status = KeWaitForSingleObject(PEvent, Executive, KernelMode, FALSE,
                                     NULL);
   }

    return status;
}

NTSTATUS
Serenum_Wait(IN PKTIMER Timer, IN LARGE_INTEGER DueTime)
/*++

Routine Description:
    Performs a wait for the specified time.
    NB: Negative time is relative to the current time.  Positive time
    represents an absolute time to wait until.

Return value:
    NTSTATUS

--*/
{
   if (KeSetTimer(Timer, DueTime, NULL)) {
      Serenum_KdPrint_Def(SER_DBG_SS_INFO, ("Timer already set: %x\n", Timer));
   }

   return KeWaitForSingleObject(Timer, Executive, KernelMode, FALSE, NULL);
}

NTSTATUS
Serenum_EnumComplete (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
/*++
Routine Description:
    A completion routine for use when calling the lower device objects to
    which our bus (FDO) is attached.  It sets the event for the synchronous
    calls done.

--*/
{
    UNREFERENCED_PARAMETER (DeviceObject);

    if (Irp->PendingReturned) {
        IoMarkIrpPending( Irp );
    }

    KeSetEvent ((PKEVENT) Context, 1, FALSE);
    // No special priority
    // No Wait

    return STATUS_MORE_PROCESSING_REQUIRED; // Keep this IRP
}


NTSTATUS
Serenum_ReadSerialPort(OUT PCHAR PReadBuffer, IN USHORT Buflen,
                       IN ULONG Timeout, OUT PUSHORT nActual,
                       OUT PIO_STATUS_BLOCK PIoStatusBlock,
                       IN const PFDO_DEVICE_DATA FdoData)
{
    NTSTATUS status;
    PIRP pIrp;
    LARGE_INTEGER startingOffset = RtlConvertLongToLargeInteger(0);
    KEVENT event;
    SERIAL_TIMEOUTS timeouts;
    IO_STATUS_BLOCK statusBlock;
    ULONG i;

    //
    // Set the proper timeouts for the read
    //

    timeouts.ReadIntervalTimeout = MAXULONG;
    timeouts.ReadTotalTimeoutMultiplier = MAXULONG;
    timeouts.ReadTotalTimeoutConstant = Timeout;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_TIMEOUTS, FALSE,
                                   FdoData->TopOfStack, &event, &statusBlock,
                                   &timeouts, sizeof(timeouts), NULL, 0);

    if (!NT_SUCCESS(status)) {
       return status;
    }

    Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Read pending...\n"));

    *nActual = 0;

    while (*nActual < Buflen) {
        KeClearEvent(&event);

        pIrp = IoBuildSynchronousFsdRequest(IRP_MJ_READ, FdoData->TopOfStack,
                                            PReadBuffer, 1, &startingOffset,
                                            &event, PIoStatusBlock);

        if (pIrp == NULL) {
            Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR, ("Failed to allocate IRP"
                                                        "\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        status = IoCallDriver(FdoData->TopOfStack, pIrp);

        if (status == STATUS_PENDING) {

            //
            // Wait for the IRP
            //

            status = KeWaitForSingleObject(&event, Executive, KernelMode,
                                           FALSE, NULL);

            if (status == STATUS_SUCCESS) {
               status = PIoStatusBlock->Status;
            }
        }

        if (!NT_SUCCESS(status) || status == STATUS_TIMEOUT) {
           Serenum_KdPrint (FdoData, SER_DBG_SS_ERROR,
                            ("IO Call failed with status %x\n", status));
           return status;
        }

        *nActual += (USHORT)PIoStatusBlock->Information;
        PReadBuffer += (USHORT)PIoStatusBlock->Information;
    }

    return status;
}

void
Serenum_PollingTimerRoutine (IN PKDPC Dpc, IN PFDO_DEVICE_DATA FdoData,
                             IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    UNREFERENCED_PARAMETER (Dpc);
    UNREFERENCED_PARAMETER (SystemArgument1);
    UNREFERENCED_PARAMETER (SystemArgument2);

    //
    // Make sure we are allowed to poll
    //

    if (KeResetEvent(&FdoData->PollingEvent) == 0) {
       return;
    }

    IoQueueWorkItem(FdoData->PollingWorker,
                    (PIO_WORKITEM_ROUTINE)Serenum_PollingRoutine,
                    DelayedWorkQueue, FdoData);
}

void
Serenum_PollingRoutine(PDEVICE_OBJECT PDevObj, PFDO_DEVICE_DATA FdoData)
{
   ULONG bitMask;
   KEVENT event;
   KTIMER timer;
   IO_STATUS_BLOCK IoStatusBlock;
   NTSTATUS status;
   PIRP Irp;
   CHAR ReadBuffer = '\0';
   USHORT nActual = 0;
   SERIAL_BASIC_SETTINGS basicSettings;
   BOOLEAN restoreSettings = FALSE;
   SERIAL_TIMEOUTS timeouts, newTimeouts;

   UNREFERENCED_PARAMETER(PDevObj);


   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Serenum_PollingRoutine for "
                                               "0x%x\n", FdoData));

   KeInitializeEvent(&event, NotificationEvent, FALSE);

   //
   // Open the serial port before doing anything
   //

   if (NULL == (Irp = IoAllocateIrp(FdoData->Self->StackSize, FALSE))) {
      Serenum_KdPrint (FdoData, SER_DBG_SS_ERROR, ("Failed to allocate IRP\n"));
      KeSetEvent(&FdoData->PollingEvent, 1, FALSE);
      return;
   }

   Irp->UserIosb = &IoStatusBlock;
   status = Serenum_IoSyncReqWithIrp(Irp, IRP_MJ_CREATE, &event,
                                     FdoData->TopOfStack);

   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint (FdoData, SER_DBG_SS_INFO,
                       ("Failed to open the serial port while polling DSR\n"));
      IoFreeIrp(Irp);
      KeSetEvent(&FdoData->PollingEvent, 1, FALSE);
      return;
   }

   //
   // Save off the old settings
   //

   status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_INTERNAL_BASIC_SETTINGS, TRUE,
                                  FdoData->TopOfStack, &event, &IoStatusBlock,
                                  NULL, 0, &basicSettings,
                                  sizeof(basicSettings));

   if (status == STATUS_SUCCESS) {
      restoreSettings = TRUE;
   } else {
      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_GET_TIMEOUTS, FALSE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            NULL, 0, &timeouts, sizeof(timeouts));

      RtlZeroMemory(&newTimeouts, sizeof(newTimeouts));

      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_TIMEOUTS, FALSE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            &newTimeouts, sizeof(newTimeouts), NULL, 0);
   }

   //
   // Set DTR - for mice that need to echo DTR back for DSR
   //

   Serenum_IoSyncIoctl(IOCTL_SERIAL_SET_DTR, FALSE, FdoData->TopOfStack, &event,
                       &IoStatusBlock);

   //
   // Set a timer for 200 ms to give time for DTR to propagate through
   //

   KeInitializeTimer(&timer);
   Serenum_Wait(&timer, RtlConvertLongToLargeInteger(-2000000));

   //
   // Poll DSR
   //

   status = Serenum_IoSyncIoctlEx(IOCTL_SERIAL_GET_MODEMSTATUS, FALSE,
                                  FdoData->TopOfStack, &event, &IoStatusBlock,
                                  NULL, 0, &bitMask, sizeof(ULONG));

   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                      ("Polling the serial port for DSR failed.  FDO: %x\n",
                       FdoData->Self));
   } else {
      if (SERIAL_DSR_STATE & bitMask) {
         //
         // DSR is set - there is a powered PNP device on the serial port
         //

         if (!FdoData->AttachedPDO) {
            Serenum_KdPrint(FdoData, SER_DBG_SS_INFO,
                            ("Device status has changed.\n"));

            IoInvalidateDeviceRelations(FdoData->UnderlyingPDO, BusRelations);
         }
      } else {
         //
         // DSR isn't set - Check for a legacy mouse
         // Set RTS
         //

         Serenum_IoSyncIoctl(IOCTL_SERIAL_SET_RTS, FALSE, FdoData->TopOfStack,
                             &event, &IoStatusBlock);

         //
         // Read one byte from the serial port
         //

         nActual = 0;
         status = Serenum_ReadSerialPort(&ReadBuffer, 1, 200, &nActual,
                                         &IoStatusBlock, FdoData);

         if (NT_SUCCESS(status) && nActual) {
            //
            // Check for 'M' or 'B'! - The only legacy devices we
            // dynamically detect are serial mice!
            //

            if (ReadBuffer == 'M' || ReadBuffer == 'B') {
               if (!FdoData->AttachedPDO ||
                   (FdoData->AttachedPDO && !FdoData->PDOLegacyEnumerated)) {
                  Serenum_KdPrint(FdoData, SER_DBG_SS_INFO,
                                  ("Device status has changed.\n"));

                  IoInvalidateDeviceRelations(FdoData->UnderlyingPDO,
                                              BusRelations);
               }
            }
         } else if (FdoData->AttachedPDO) {
            Serenum_KdPrint(FdoData, SER_DBG_SS_INFO,
                            ("Device status has changed.\n"));

            IoInvalidateDeviceRelations(FdoData->UnderlyingPDO, BusRelations);
         }
      }
   }

   //
   // CLEANUP and then Close
   //

   if (restoreSettings) {
      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_INTERNAL_RESTORE_SETTINGS, TRUE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            &basicSettings, sizeof(basicSettings), NULL, 0);
   } else {
      Serenum_IoSyncIoctlEx(IOCTL_SERIAL_SET_TIMEOUTS, FALSE,
                            FdoData->TopOfStack, &event, &IoStatusBlock,
                            &timeouts, sizeof(timeouts), NULL, 0);
   }

   status = Serenum_IoSyncReqWithIrp(Irp, IRP_MJ_CLEANUP, &event,
                                     FdoData->TopOfStack);

   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_INFO,
                      ("Failed to cleanup the serial port while polling DSR"
                       "\n"));

      //
      // Don't return.  Try to close.
      //
   }

   //
   // Close the Serial port after everything is done
   //

   status = Serenum_IoSyncReqWithIrp(Irp, IRP_MJ_CLOSE, &event,
                                     FdoData->TopOfStack);

   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_INFO,
                      ("Failed to close the serial port while polling DSR\n"));
      //
      // Don't return.  Still have to set the polling event.
      //
   }

   IoFreeIrp(Irp);

   KeSetEvent(&FdoData->PollingEvent, 1, FALSE);
}

void
Serenum_StopPolling(PFDO_DEVICE_DATA FdoData, BOOLEAN PollingLock)
{
    UNREFERENCED_PARAMETER(PollingLock);

    Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Serenum_StopPolling for "
                                                "0x%x\n", FdoData));

    //
    // Wait for permission to enter
    //

    KeWaitForSingleObject(&FdoData->PollStateEvent, Executive, KernelMode,
                          FALSE, NULL);

    if (FdoData->Polling == 0) {
       Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Serenum_StopPolling "
                                                   "leaving --  not polling on"
                                                   "0x%x\n", FdoData));
       KeSetEvent(&FdoData->PollStateEvent, 1, FALSE);
       return;
    }

    KeWaitForSingleObject(&FdoData->PollingEvent, Executive, KernelMode,
                          FALSE, NULL);

    if(!KeCancelTimer( &FdoData->PollingTimer )) {
       Serenum_KdPrint (FdoData, SER_DBG_SS_TRACE, ("The timer wasn't in the "
                                                    "system queue: %x\n",
                                                    FdoData->PollingTimer));
    }

    FdoData->Polling = 0;

    KeSetEvent(&FdoData->PollStateEvent, 1, FALSE);

    Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Leaving Serenum_StopPolling "
                                                "for 0x%x\n", FdoData));
}

void
Serenum_StartPolling(PFDO_DEVICE_DATA FdoData, BOOLEAN PollingLock)
{
   HANDLE keyHandle;
   NTSTATUS status;
   UNICODE_STRING keyName;

   UNREFERENCED_PARAMETER(PollingLock);

   Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Serenum_StartPolling for "
                                               "0x%x\n", FdoData));
#if 0
   //
   // Wait for permission to enter
   //

   KeWaitForSingleObject(&FdoData->PollStateEvent, Executive, KernelMode,
                         FALSE, NULL);

   if (FdoData->Polling != 0) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_TRACE, ("Already polling for "
                                               "0x%x\n", FdoData));
      KeSetEvent(&FdoData->PollStateEvent, 1, FALSE);
      return;
   }


   KeInitializeTimerEx(&FdoData->PollingTimer, NotificationTimer);


   //
   // Open the device registry key and use the symbolic name the PnP
   // manager is recommending for us.
   //

   status = IoOpenDeviceRegistryKey(FdoData->UnderlyingPDO,
                                    PLUGPLAY_REGKEY_DEVICE,
                                    STANDARD_RIGHTS_READ, &keyHandle);
   if (!NT_SUCCESS(status)) {
      Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                      ("IoOpenDeviceRegistryKey failed - %x \n", status));

      FdoData->PollingPeriod = 0;
      FdoData->Polling = 0;
   } else {
      status = Serenum_GetRegistryKeyValue(keyHandle, L"PollingPeriod",
                                           sizeof(L"PollingPeriod"),
                                           &FdoData->PollingPeriod,
                                           sizeof(ULONG), NULL);
      ZwClose (keyHandle);

      if (!NT_SUCCESS(status)) {
         Serenum_KdPrint (FdoData, SER_DBG_SS_INFO,
                          ("Failed to get polling period from registry - %x \n",
                           status));
         FdoData->PollingPeriod = 0;
         FdoData->Polling = 0;
      }
   }

   if (FdoData->AttachedPDO) {
      status = IoOpenDeviceRegistryKey(FdoData->AttachedPDO,
                                       PLUGPLAY_REGKEY_DEVICE,
                                       STANDARD_RIGHTS_WRITE, &keyHandle);

      if (!NT_SUCCESS(status)) {
         Serenum_KdPrint(FdoData, SER_DBG_SS_ERROR,
                         ("IoOpenDeviceRegistryKey failed - %x \n", status));
      } else {
         RtlInitUnicodeString(&keyName, L"DeviceDetectionTimeout");

         //
         // Doesn't matter whether this works or not.
         //

         ZwSetValueKey(keyHandle, &keyName, 0, REG_DWORD,
                       &FdoData->PollingPeriod, sizeof(ULONG));

         ZwClose(keyHandle);
      }
   }


   if (FdoData->PollingPeriod && FdoData->PollingPeriod != (ULONG)-1) {
      //
      // Start the polling timer
      //
      if (KeSetTimerEx(&FdoData->PollingTimer,
                       RtlConvertLongToLargeInteger(-(LONG)
                                                    (FdoData->PollingPeriod
                                                     * 1000000)),
                       FdoData->PollingPeriod * 100, &FdoData->DPCPolling)) {
         Serenum_KdPrint(FdoData, SER_DBG_SS_INFO,
                         ("The timer was already in the system queue: %x\n",
                          FdoData->PollingTimer));
      }
      FdoData->Polling = 1;
      KeSetEvent(&FdoData->PollingEvent, 1, FALSE);
   }

   KeSetEvent(&FdoData->PollStateEvent, 1, FALSE);
#else
   FdoData->Polling = 0;
   FdoData->PollingPeriod = 0;
#endif
}

NTSTATUS
Serenum_GetRegistryKeyValue(IN HANDLE Handle, IN PWCHAR KeyNameString,
                            IN ULONG KeyNameStringLength, IN PVOID Data,
                            IN ULONG DataLength, OUT PULONG ActualLength)
/*++

Routine Description:

    Reads a registry key value from an already opened registry key.

Arguments:

    Handle              Handle to the opened registry key

    KeyNameString       ANSI string to the desired key

    KeyNameStringLength Length of the KeyNameString

    Data                Buffer to place the key value in

    DataLength          Length of the data buffer

Return Value:

    STATUS_SUCCESS if all works, otherwise status of system call that
    went wrong.

--*/
{
    UNICODE_STRING              keyName;
    ULONG                       length;
    PKEY_VALUE_FULL_INFORMATION fullInfo;

    NTSTATUS                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

    RtlInitUnicodeString (&keyName, KeyNameString);

    length = sizeof(KEY_VALUE_FULL_INFORMATION) + KeyNameStringLength
      + DataLength;
    fullInfo = ExAllocatePool(PagedPool, length);

    if (ActualLength != NULL) {
       *ActualLength = 0;
    }

    if (fullInfo) {
        ntStatus = ZwQueryValueKey (Handle,
                                  &keyName,
                                  KeyValueFullInformation,
                                  fullInfo,
                                  length,
                                  &length);

        if (NT_SUCCESS(ntStatus)) {
            //
            // If there is enough room in the data buffer, copy the output
            //

            if (DataLength >= fullInfo->DataLength) {
                RtlCopyMemory(Data, ((PUCHAR)fullInfo) + fullInfo->DataOffset,
                              fullInfo->DataLength);
                if (ActualLength != NULL) {
                   *ActualLength = fullInfo->DataLength;
                }
            }
        }

        ExFreePool(fullInfo);
    }

    if (!NT_SUCCESS(ntStatus) && !NT_ERROR(ntStatus)) {
       if (ntStatus == STATUS_BUFFER_OVERFLOW) {
          ntStatus = STATUS_BUFFER_TOO_SMALL;
       } else {
          ntStatus = STATUS_UNSUCCESSFUL;
       }
    }
    return ntStatus;
}


