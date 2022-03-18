/*++

Module Name:

    appmca.h

Abstract:

	Defines related to MCA for app and driver - device names, function codes 
	and ioctls

Author:

Revision History:


--*/

#ifndef APPMCA_H
#define APPMCA_H

//
// 16 bit device type definition.
// Device types 0-32767 are reserved by Microsoft.
//
#define FILE_DEVICE_MCA                     0xb000

//
// 12 bit function codes
// Function codes 0-2047 are reserved by Microsoft.
//

#define FUNCTION_READ_BANKS									    0xb00
#define FUNCTION_READ_BANKS_ASYNC							    0xb01

#define IOCTL_READ_BANKS  (CTL_CODE(FILE_DEVICE_MCA, FUNCTION_READ_BANKS,\
  		(METHOD_BUFFERED),(FILE_READ_ACCESS|FILE_WRITE_ACCESS)))

#define IOCTL_READ_BANKS_ASYNC  (CTL_CODE(FILE_DEVICE_MCA, \
  		FUNCTION_READ_BANKS_ASYNC,(METHOD_BUFFERED), \
  		(FILE_READ_ACCESS|FILE_WRITE_ACCESS)))

//
// Name that Win32 front end will use to open the MCA device
//

#define MCA_DEVICE_NAME_WIN32      "\\\\.\\imca"

#endif // APPMCA_H

