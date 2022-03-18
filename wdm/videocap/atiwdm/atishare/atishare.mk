#############################################################################
#
#	Makefile for Atishare library
#
#		$Date:   10 Sep 1998 12:01:44  $
#	$Revision:   1.2  $
#	  $Author:   Tashjian  $
#
#  $Copyright:	(c) 1997 - 1998  ATI Technologies Inc.  All Rights Reserved.  $
#
##########################################################################

BSCMAKE         = 1
#BSCTARGETS      = Atishare.BSC 
	
ROOT            = \NTDDK
MINIPORT    	= Atishare
DEVICELIB		= Atishare.lib
SRCDIR          = ..
WANT_LATEST     = TRUE
WANT_WDMDDK		= TRUE
DEPENDNAME      = ..\depend.mk
DESCRIPTION     = WDM ATISHARE common library
VERDIRLIST		= maxdebug debug retail

NOVXDVERSION    = TRUE
CFLAGS 			= -DMSC $(CFLAGS) -TP

!IF "$(VERDIR)" != "debug"
CFLAGS	 = $(CFLAGS) -Oi
!endif

OBJS =	aticonfg.obj i2script.obj registry.obj i2clog.obj
 
!include $(ROOT)\ddk\inc\master.mk

INCLUDE=$(WDMROOT)\tools\vc\include;$(INCLUDE)
INCLUDE=$(WDMROOT)\ddk\inc\win98;$(INCLUDE)

