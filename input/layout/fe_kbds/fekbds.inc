#****************************** File Header ******************************\
# File Name: Sources.inc for FE KBDs
#
# Copyright (c) 1999, Microsoft Corporation
#
# Project specific SOURCES file.
#
#***************************************************************************/


!include ..\..\..\sources.inc

SOURCES_USED=..\..\fekbds.inc


TARGETPATH=obj
TARGETTYPE=DYNLINK

SOURCES=$(TARGETNAME).c $(TARGETNAME).rc

INCLUDES=..\inc;..\..\..\inc

C_DEFINES=$(C_DEFINES) -DFE_SB -DFE_IME


