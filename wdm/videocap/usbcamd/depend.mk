$(OBJDIR)\dbglog.obj $(OBJDIR)\dbglog.lst: ..\dbglog.c \
	$(WDMROOT)\ddk\inc\ks.h $(WDMROOT)\ddk\inc\ksmedia.h \
	$(WDMROOT)\ddk\inc\strmini.h $(WDMROOT)\ddk\inc\usb100.h \
	$(WDMROOT)\ddk\inc\usbcamdi.h $(WDMROOT)\ddk\inc\usbdi.h \
	$(WDMROOT)\ddk\inc\usbdlib.h $(WDMROOT)\ddk\inc\usbioctl.h \
	$(WDMROOT)\ddk\inc\wdm.h ..\..\..\..\..\dev\ntddk\inc\alpharef.h \
	..\..\..\..\..\dev\ntddk\inc\basetsd.h \
	..\..\..\..\..\dev\ntddk\inc\bugcodes.h \
	..\..\..\..\..\dev\ntddk\inc\ntdef.h \
	..\..\..\..\..\dev\ntddk\inc\ntiologc.h \
	..\..\..\..\..\dev\ntddk\inc\ntstatus.h \
	..\..\..\..\..\dev\tools\c32\inc\alphaops.h \
	..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\dev\tools\c32\inc\poppack.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack1.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\dev\tools\c32\inc\windef.h \
	..\..\..\..\..\dev\tools\c32\inc\winnt.h ..\usbcamd.h ..\warn.h
.PRECIOUS: $(OBJDIR)\dbglog.lst

$(OBJDIR)\intbulk.obj $(OBJDIR)\intbulk.lst: ..\intbulk.c \
	$(WDMROOT)\ddk\inc\ks.h $(WDMROOT)\ddk\inc\ksmedia.h \
	$(WDMROOT)\ddk\inc\strmini.h $(WDMROOT)\ddk\inc\usb100.h \
	$(WDMROOT)\ddk\inc\usbcamdi.h $(WDMROOT)\ddk\inc\usbdi.h \
	$(WDMROOT)\ddk\inc\usbdlib.h $(WDMROOT)\ddk\inc\usbioctl.h \
	$(WDMROOT)\ddk\inc\wdm.h ..\..\..\..\..\dev\ntddk\inc\alpharef.h \
	..\..\..\..\..\dev\ntddk\inc\basetsd.h \
	..\..\..\..\..\dev\ntddk\inc\bugcodes.h \
	..\..\..\..\..\dev\ntddk\inc\ntdef.h \
	..\..\..\..\..\dev\ntddk\inc\ntiologc.h \
	..\..\..\..\..\dev\ntddk\inc\ntstatus.h \
	..\..\..\..\..\dev\tools\c32\inc\alphaops.h \
	..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\dev\tools\c32\inc\poppack.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack1.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\dev\tools\c32\inc\windef.h \
	..\..\..\..\..\dev\tools\c32\inc\winnt.h ..\usbcamd.h ..\warn.h
.PRECIOUS: $(OBJDIR)\intbulk.lst

$(OBJDIR)\iso.obj $(OBJDIR)\iso.lst: ..\iso.c $(WDMROOT)\ddk\inc\ks.h \
	$(WDMROOT)\ddk\inc\ksmedia.h $(WDMROOT)\ddk\inc\strmini.h \
	$(WDMROOT)\ddk\inc\usb100.h $(WDMROOT)\ddk\inc\usbcamdi.h \
	$(WDMROOT)\ddk\inc\usbdi.h $(WDMROOT)\ddk\inc\usbdlib.h \
	$(WDMROOT)\ddk\inc\usbioctl.h $(WDMROOT)\ddk\inc\wdm.h \
	..\..\..\..\..\dev\ntddk\inc\alpharef.h \
	..\..\..\..\..\dev\ntddk\inc\basetsd.h \
	..\..\..\..\..\dev\ntddk\inc\bugcodes.h \
	..\..\..\..\..\dev\ntddk\inc\ntdef.h \
	..\..\..\..\..\dev\ntddk\inc\ntiologc.h \
	..\..\..\..\..\dev\ntddk\inc\ntstatus.h \
	..\..\..\..\..\dev\tools\c32\inc\alphaops.h \
	..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\dev\tools\c32\inc\poppack.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack1.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\dev\tools\c32\inc\windef.h \
	..\..\..\..\..\dev\tools\c32\inc\winnt.h ..\usbcamd.h ..\warn.h
.PRECIOUS: $(OBJDIR)\iso.lst

$(OBJDIR)\reset.obj $(OBJDIR)\reset.lst: ..\reset.c $(WDMROOT)\ddk\inc\ks.h \
	$(WDMROOT)\ddk\inc\ksmedia.h $(WDMROOT)\ddk\inc\strmini.h \
	$(WDMROOT)\ddk\inc\usb100.h $(WDMROOT)\ddk\inc\usbcamdi.h \
	$(WDMROOT)\ddk\inc\usbdi.h $(WDMROOT)\ddk\inc\usbdlib.h \
	$(WDMROOT)\ddk\inc\usbioctl.h $(WDMROOT)\ddk\inc\wdm.h \
	..\..\..\..\..\dev\ntddk\inc\alpharef.h \
	..\..\..\..\..\dev\ntddk\inc\basetsd.h \
	..\..\..\..\..\dev\ntddk\inc\bugcodes.h \
	..\..\..\..\..\dev\ntddk\inc\ntdef.h \
	..\..\..\..\..\dev\ntddk\inc\ntiologc.h \
	..\..\..\..\..\dev\ntddk\inc\ntstatus.h \
	..\..\..\..\..\dev\tools\c32\inc\alphaops.h \
	..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\dev\tools\c32\inc\poppack.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack1.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\dev\tools\c32\inc\windef.h \
	..\..\..\..\..\dev\tools\c32\inc\winnt.h ..\usbcamd.h ..\warn.h
.PRECIOUS: $(OBJDIR)\reset.lst

$(OBJDIR)\stream.obj $(OBJDIR)\stream.lst: ..\stream.c \
	$(WDMROOT)\ddk\inc\ks.h $(WDMROOT)\ddk\inc\ksmedia.h \
	$(WDMROOT)\ddk\inc\strmini.h $(WDMROOT)\ddk\inc\usb100.h \
	$(WDMROOT)\ddk\inc\usbcamdi.h $(WDMROOT)\ddk\inc\usbdi.h \
	$(WDMROOT)\ddk\inc\usbdlib.h $(WDMROOT)\ddk\inc\usbioctl.h \
	$(WDMROOT)\ddk\inc\wdm.h ..\..\..\..\..\dev\ntddk\inc\alpharef.h \
	..\..\..\..\..\dev\ntddk\inc\basetsd.h \
	..\..\..\..\..\dev\ntddk\inc\bugcodes.h \
	..\..\..\..\..\dev\ntddk\inc\ntdef.h \
	..\..\..\..\..\dev\ntddk\inc\ntiologc.h \
	..\..\..\..\..\dev\ntddk\inc\ntstatus.h \
	..\..\..\..\..\dev\tools\c32\inc\alphaops.h \
	..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\dev\tools\c32\inc\poppack.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack1.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\dev\tools\c32\inc\windef.h \
	..\..\..\..\..\dev\tools\c32\inc\winnt.h ..\usbcamd.h ..\warn.h
.PRECIOUS: $(OBJDIR)\stream.lst

$(OBJDIR)\usbcamd.obj $(OBJDIR)\usbcamd.lst: ..\usbcamd.c \
	$(WDMROOT)\ddk\inc\ks.h $(WDMROOT)\ddk\inc\ksmedia.h \
	$(WDMROOT)\ddk\inc\strmini.h $(WDMROOT)\ddk\inc\usb100.h \
	$(WDMROOT)\ddk\inc\usbcamdi.h $(WDMROOT)\ddk\inc\usbdi.h \
	$(WDMROOT)\ddk\inc\usbdlib.h $(WDMROOT)\ddk\inc\usbioctl.h \
	$(WDMROOT)\ddk\inc\wdm.h ..\..\..\..\..\dev\ntddk\inc\alpharef.h \
	..\..\..\..\..\dev\ntddk\inc\basetsd.h \
	..\..\..\..\..\dev\ntddk\inc\bugcodes.h \
	..\..\..\..\..\dev\ntddk\inc\ntdef.h \
	..\..\..\..\..\dev\ntddk\inc\ntiologc.h \
	..\..\..\..\..\dev\ntddk\inc\ntstatus.h \
	..\..\..\..\..\dev\tools\c32\inc\alphaops.h \
	..\..\..\..\..\dev\tools\c32\inc\ctype.h \
	..\..\..\..\..\dev\tools\c32\inc\excpt.h \
	..\..\..\..\..\dev\tools\c32\inc\poppack.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack1.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack2.h \
	..\..\..\..\..\dev\tools\c32\inc\pshpack4.h \
	..\..\..\..\..\dev\tools\c32\inc\stdio.h \
	..\..\..\..\..\dev\tools\c32\inc\string.h \
	..\..\..\..\..\dev\tools\c32\inc\windef.h \
	..\..\..\..\..\dev\tools\c32\inc\winnt.h ..\usbcamd.h ..\warn.h
.PRECIOUS: $(OBJDIR)\usbcamd.lst


