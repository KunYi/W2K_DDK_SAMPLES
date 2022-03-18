!IF 0

Copyright (C) Microsoft Corporation, 1998 - 1999

Module Name:

    depend.mk.

!ENDIF

$(OBJDIR)\parepp.obj $(OBJDIR)\parepp.lst: ..\parepp.c \
	$(WDMROOT)\..\dev\ntddk\inc\alpharef.h \
	$(WDMROOT)\..\dev\ntddk\inc\bugcodes.h \
	$(WDMROOT)\..\dev\ntddk\inc\exlevels.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntddk.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntdef.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntiologc.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntstatus.h \
	$(WDMROOT)\..\dev\ntsdk\inc\ntddpar.h \
	$(WDMROOT)\..\dev\tools\c32\inc\alphaops.h \
	$(WDMROOT)\..\dev\tools\c32\inc\ctype.h \
	$(WDMROOT)\..\dev\tools\c32\inc\excpt.h \
	$(WDMROOT)\..\dev\tools\c32\inc\poppack.h \
	$(WDMROOT)\..\dev\tools\c32\inc\pshpack1.h \
	$(WDMROOT)\..\dev\tools\c32\inc\pshpack4.h \
	$(WDMROOT)\..\dev\tools\c32\inc\string.h ..\..\inc\parallel.h \
	..\parlog.h ..\parport.h
.PRECIOUS: $(OBJDIR)\parepp.lst

$(OBJDIR)\parinit.obj $(OBJDIR)\parinit.lst: ..\parinit.c \
	$(WDMROOT)\..\dev\ntddk\inc\alpharef.h \
	$(WDMROOT)\..\dev\ntddk\inc\bugcodes.h \
	$(WDMROOT)\..\dev\ntddk\inc\exlevels.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntddk.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntdef.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntiologc.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntstatus.h \
	$(WDMROOT)\..\dev\ntsdk\inc\ntddpar.h \
	$(WDMROOT)\..\dev\tools\c32\inc\alphaops.h \
	$(WDMROOT)\..\dev\tools\c32\inc\ctype.h \
	$(WDMROOT)\..\dev\tools\c32\inc\excpt.h \
	$(WDMROOT)\..\dev\tools\c32\inc\poppack.h \
	$(WDMROOT)\..\dev\tools\c32\inc\pshpack1.h \
	$(WDMROOT)\..\dev\tools\c32\inc\pshpack4.h \
	$(WDMROOT)\..\dev\tools\c32\inc\string.h ..\..\inc\parallel.h \
	..\parlog.h ..\parport.h
.PRECIOUS: $(OBJDIR)\parinit.lst

$(OBJDIR)\parport.obj $(OBJDIR)\parport.lst: ..\parport.c \
	$(WDMROOT)\..\dev\ntddk\inc\alpharef.h \
	$(WDMROOT)\..\dev\ntddk\inc\bugcodes.h \
	$(WDMROOT)\..\dev\ntddk\inc\exlevels.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntddk.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntdef.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntiologc.h \
	$(WDMROOT)\..\dev\ntddk\inc\ntstatus.h \
	$(WDMROOT)\..\dev\ntsdk\inc\ntddpar.h \
	$(WDMROOT)\..\dev\tools\c32\inc\alphaops.h \
	$(WDMROOT)\..\dev\tools\c32\inc\ctype.h \
	$(WDMROOT)\..\dev\tools\c32\inc\excpt.h \
	$(WDMROOT)\..\dev\tools\c32\inc\poppack.h \
	$(WDMROOT)\..\dev\tools\c32\inc\pshpack1.h \
	$(WDMROOT)\..\dev\tools\c32\inc\pshpack4.h \
	$(WDMROOT)\..\dev\tools\c32\inc\string.h ..\..\inc\parallel.h \
	..\parlog.h ..\parport.h
.PRECIOUS: $(OBJDIR)\parport.lst

$(OBJDIR)\parlog.res $(OBJDIR)\parlog.lst: ..\parlog.rc
.PRECIOUS: $(OBJDIR)\parlog.lst


