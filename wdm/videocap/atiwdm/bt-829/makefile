!ifdef NTMAKEENV
all:
clean:
!else
#############################################################################
#
#       Makefile for bt-829 directory
#
#       $Date:   29 Jul 1999 12:26:40  $
#       $Revision:   1.1  $
#       $Author:   tloveall  $
#
#  $Copyright:	(c) 1997 - 1999  ATI Technologies Inc.  All Rights Reserved.$
#
#############################################################################

ROOT = ..\..\..
DIR = mini

PROPLIST =

#
# forrestf - bt848 uses MmGetPhysicalAddress(), a non-WDM function, when
#            it should be using IoMapTransfer() instead.  I want to check in
#            a new WDM.H before other folks start to use Bad Functions as well,
#            which unfortunately renders bt848 unbuildable until this is fixed.
#
#DIRLIST = testcap usbcamd bt848
DIRLIST = atishare bt-829 tuners crossbar tvaudio

ALLLIST = $(DIRLIST)

!include $(ROOT)\dev\master.mk
!endif

