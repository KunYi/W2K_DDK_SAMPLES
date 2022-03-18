    This directory contains a sample PnP filter driver whose only 
purpose is to provide WMI data blocks. Typically driver writers will copy the 
sample code into their own driver and make any minor modifications so that the
WMI data blocks are always available when the driver is loaded. Alternatively
WmiSamp can be left as a filter driver if WMI data blocks should only be made
available when the filter driver is loaded. 

    WMI enabling a driver with the sample code consists of doing 5 things:

1. Determining what data needs to be provided and organize it into data blocks
   that contain related data items. For example packets sent and bytes sent
   could be part of a single data block while transmit errors could be part
   of a data block that contains other error counts. Determine if the device
   needs notifications of when to enable and disable collections in the case
   of data blocks that impose a performance hit to collect.

2. Write a Managed Object Format (.mof) file that describes the data blocks. 
   In doing this the driver writer will need to run the guidgen tool to 
   create globally unique guids that are assigned to the data blocks. Guidgen
   *MUST* be run so that no two data blocks will have the same guid. Reference
   the .bmf file (.bmf files are created when mof files are compiled by the 
   mofcomp tool) in the driver's .rc file so that the .bmf file data is 
   included as a resource of the driver's .sys file. One of the by products
   of generating the binary mof file is a header file that contains guid
   and structure definitions for the data blocks. It is recommended that
   this header be used since it will always be up to date with the MOF.

3. Build the WMIGUIDREGINFO structure with the guids for the data blocks 
   defined in the .mof file. If the device wants to be notified of when to 
   start and stop collection of a data block the WMIREG_FLAG_EXPENSIVE flag 
   should be set for the data block in the WMIGUIDREGINFO structure.

4. Implement the 6 WMI function callback routines and reference them in a 
   WMILIB_CONTEXT structure.

5. Fixup the sources and makefile.inc files so that the .mof file gets
   compiled into a .bmf and .h file.

6. Note that another by product of compiling the MOF is a .vbs file. This 
   is a vscript file that is run from the command line on the target machine
   that is running the new device driver. It will cause WMI to query all 
   data blocks and properties and put the results into a .log file. This
   can be very useful for testing WMI support in your driver. For more 
   sophisticated testing the vbscript can be extended by hand. Also the
   tool %windir%\system32\wbem\wbemtest.exe can also be used.

WMI Mof Check Tool - wmimofck.exe

WmiMofCk validates that the classes, properties, methods and events specified 
in a binary mof file (.bmf) are valid for use with WMI. It also generates 
useful output files needed to build and test the WMI data provider.

If the -h parameter is specified then a C language header file is created
that defines the guids, data structures and method indicies specified in the
MOF file.

If the -t parameter is specified then a VBScript applet is created that will
query all data blocks and properties specified in the MOF file. This can be
useful for testing WMI data providers.

If the -x parameter is specified then a text file is created that contains
the text representation of the binary mof data. This can be included in 
the source of the driver if the driver supports reporting the binary mof 
via a WMI query rather than a resource on the driver image file.

Usage:
    wmimofck -h<C Header output file> -x<Hexdump output file> -t<VBScript test o
utput file> <binary mof input file>

