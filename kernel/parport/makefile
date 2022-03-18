#############################################################################
#
#       Copyright (C) Microsoft Corporation 1996-1999
#       All Rights Reserved.
#
#       MAKEFILE for Parallel Port Driver
#
#############################################################################


#
# This section of the makefile is used under Win9x environments.  You
# must define an environment variable WIN95_BUILD=TRUE before typing NMAKE
#
!IFDEF WIN95_BUILD

ROOT    = ..\..\..\..\..
IS_32   = TRUE
IS_OEM  = TRUE
DEVICEDIR = PARPORT
COMMONMKFILE = $(DEVICEDIR).mk
DEPENDTARGETS =  debug

!include ..\..\..\..\..\dev\master.mk


!ELSE

#
# This section of the makefile is used under the NT environments.
# Edit .\sources. if you want to add a new source file to this component.
# This file merely indirects to the real make file that is shared by all
# the driver components of Windows NT
#

!INCLUDE $(NTMAKEENV)\makefile.def

!ENDIF

