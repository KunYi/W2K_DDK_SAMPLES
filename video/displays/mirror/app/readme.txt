To install:

1. Copy mirror.dll to %winnt%\system32, mirror.sys to %winnt%\system32\drivers

2. These are added during .inf file installation:
	Sets registry values (where '#' is a number):
		HKLM\System\CurrentControlSet\Services\mirror
			device#\Device Description		"Microsoft Mirror Driver" (SZ)
 			device#\Installed Display Drivers	"mirror"  (MULTI_SZ)
			device#\MirrorDriver			0x1 (DWORD)
			device#\xxx				possibly other keys
			enum\xxx				keys for enumerating devices

3. When invoking the mirrored driver on a mirrored device:

	To change the settings for your mirrored device, you must know the '\\.\DISPLAY#' name
	associated with your mirrored display.  In the case of multiple instances, '#' will be
	a different number.  This can be found by iterating through the available display devices
	using EnumDisplayDevices().  For reference/testing only, this information can be
	found under the following key:

		HKLM\Hardware\DeviceMap\Video

	To attach the mirrored device to the desktop pdev list, you must add a registry value
	'Attach.ToDesktop' = 0x1.  Subsequent ChangeDisplaySettings() will then dynamically load
	the mirrored display driver for use.

	HKLM\SYSTEM\CurrentControlSet\Hardware Profiles\Current\System\CurrentControlSet\
		Services\mirror     [where 'mirror' is short name of mirrored surface]
			device#\Attach.ToDesktop	0x1 (DWORD)

	To disable the attachment, set 'Attach.ToDesktop' to 0x0.  Otherwise your driver will
	invoked again at boot up time.  There are also other Default.Settings values saved under
	this key, use ChangeDisplaySettings() with dwFlags=CDS_UPDATEREGISTRY to properly save
	them.

	To create a DC, device managed bitmap, etc. using the mirrored surface, use the
	ordinary GDI APIs.  To create the DC specify, use the following:

        HDC hdc = CreateDC("DISPLAY",			// driver name
                           deviceName,			// example 'mirror' device name
                           NULL,
                           NULL);

	See DDK sample for a working demonstration.

Minimum requirements for barebones mirrored display driver:

DrvEnableDriver [exported]
DrvEnablePDEV
DrvCompletePDEV
DrvDisablePDEV
DrvEnableSurface
DrvDisableSurface
DrvBitBlt
DrvCopyBits
DrvCreateDeviceBitmap
DrvDeleteDeviceBitmap
DrvTextOut

Minimum requirements for barebones mirrored miniport driver:

DriverEntry [exported]
HwFindAdapter
HwInitialize
HwStartIO

Since there is no physical display device associated with a mirrored surface, these routines
can return positive results.

