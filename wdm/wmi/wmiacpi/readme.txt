This directory contains files that allow an ACPI bios developer to add 
instrumentation from within ASL code. ASL code can expose data blocks, methods
and events via WMI by leveraging the WMIACPI.SYS driver. More information
about the mechanics of writing ASL to expose instrumentation information is
available in wmi-acpi.doc or on 
http://www.microsoft.com/HWDev/MANAGEABILITY/wmi-acpi.htm.


Device.ASL - ASL code that can be included in the acpi bios that exposes 
             a set of packages, strings, data, methods and events.

acpimof.mof - Managed Object Format file that contains a description  of the 
              data blocks, methods and events exposed. This description is
              required so that WMI is able to access the data blocks, methods
              and events.

acpimof.rc, acpimof.def - FIles required to build acpimof.dll, a resource only
              dll.

makefile, sources, makefile.inc - build glue

wmi-acpi.doc - White paper describing how to add instrumentation to ASL code.

acpimof.vbs - Built at the same time as acpimof.dll, this file contains a
              VBScript applet that will query all classes specified in the
              acpimof.mof file.


To add the sample code to your acpi bios and access via WMI:

1. Include the contents of device.asl to your asl source and rebuild the
   DSDT. Update the OS with the new dsdt via the registry or reflashing.

2. Build acpimof.dll in this directory. acpimof.dll is a resource only dll
   that contains the compiled mof in a form that WMI can import into its
   schema.

3. Copy acpimof.dll to %windir%\system32 and add a value named "MofImagePath"
   under the HKEY_LOCAL_MACHINE\CurrentControlSet\Services\WmiAcpi key. The
   contents of the value should be a path to the acpimof.dll file.

4. Reboot. When PNP recognizes the new device with a pnpid of pnp0c14
   it will install wmiacpi.sys automatically and make the mof resource
   in acpimof.dll available to the WMI schema.

5. Execute the acpimof.vbs test by entering acpimof.vbs at the command line
   and reviewing the acpitest.log to insure that all data returned is correct.


