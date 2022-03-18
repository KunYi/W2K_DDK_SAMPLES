#############################################################################
#
#       Sample makefile for the WDM audio mini driver
#
#       $Date:   29 Jul 1999 12:26:40  $
#       $Revision:   1.1  $
#       $Author:   tloveall  $
#
#  $Copyright:	(c) 1997 - 1999  ATI Technologies Inc.  All Rights Reserved.$
#
#############################################################################

#BSCMAKE         = 1
#BSCTARGETS      = ATITVSND.BSC 

ROOT            = $(BASEDIR)
MINIPORT        = atitvsnd
SRCDIR          = ..
ALTSRCDIR       = $(ROOT)\src\wdm\capture\mini\ATIShare
WANT_LATEST     = TRUE
WANT_WDMDDK     = TRUE
DEPENDNAME      = ..\depend.mk
DESCRIPTION     = TVAudio WDM MiniDriver
VERDIRLIST      = maxdebug debug retail
LIBS			= $(BASEDIR)\lib\*\$(DDKBUILDENV)\stream.lib\
                  $(BASEDIR)\lib\*\$(DDKBUILDENV)\ksguid.lib

!if "$(VERDIR)" == "debug"
CFLAGS 			= -DMSC $(CFLAGS) -TP
!else
CFLAGS 			= -DMSC $(CFLAGS) -TP -Oi
!endif

RESNAME         = atitvsnd

NOVXDVERSION    = TRUE

OBJS =	atitvsnd.obj wdmtvsnd.obj tsndprop.obj tsndhdw.obj \
		aticonfg.obj i2script.obj pinmedia.obj registry.obj \
		i2clog.obj atitvsnd.res
 
!include $(ROOT)\ddk\inc\master.mk

INCLUDE=$(WDMROOT)\tools\vc\include;$(INCLUDE)
INCLUDE=$(WDMROOT)\ddk\inc\win98;$(INCLUDE)
INCLUDE=$(WDMROOT)\capture\mini\atishare;$(INCLUDE)


