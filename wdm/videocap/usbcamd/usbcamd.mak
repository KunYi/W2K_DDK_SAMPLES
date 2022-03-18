# Microsoft Developer Studio Generated NMAKE File, Based on usbcamd.dsp
!IF "$(CFG)" == ""
CFG=usbcamd - Win32 Debug
!MESSAGE No configuration specified. Defaulting to usbcamd - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "usbcamd - Win32 Release" && "$(CFG)" != "usbcamd - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "usbcamd.mak" CFG="usbcamd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "usbcamd - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "usbcamd - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "usbcamd - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\usbcamd.dll"


CLEAN :
	-@erase "$(INTDIR)\dbglog.obj"
	-@erase "$(INTDIR)\intbulk.obj"
	-@erase "$(INTDIR)\iso.obj"
	-@erase "$(INTDIR)\reset.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\usbcamd.obj"
	-@erase "$(INTDIR)\usbcamd.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\usbcamd.dll"
	-@erase "$(OUTDIR)\usbcamd.exp"
	-@erase "$(OUTDIR)\usbcamd.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\usbcamd.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\usbcamd.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\usbcamd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\usbcamd.pdb" /machine:I386 /def:".\usbcamd.def" /out:"$(OUTDIR)\usbcamd.dll" /implib:"$(OUTDIR)\usbcamd.lib" 
DEF_FILE= \
	".\usbcamd.def"
LINK32_OBJS= \
	"$(INTDIR)\intbulk.obj" \
	"$(INTDIR)\iso.obj" \
	"$(INTDIR)\reset.obj" \
	"$(INTDIR)\stream.obj" \
	"$(INTDIR)\usbcamd.obj" \
	"$(INTDIR)\usbcamd.res" \
	"$(INTDIR)\dbglog.obj"

"$(OUTDIR)\usbcamd.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\usbintel.sys" "$(OUTDIR)\usbcamd.bsc"


CLEAN :
	-@erase "$(INTDIR)\dbglog.obj"
	-@erase "$(INTDIR)\dbglog.sbr"
	-@erase "$(INTDIR)\intbulk.obj"
	-@erase "$(INTDIR)\intbulk.sbr"
	-@erase "$(INTDIR)\iso.obj"
	-@erase "$(INTDIR)\iso.sbr"
	-@erase "$(INTDIR)\reset.obj"
	-@erase "$(INTDIR)\reset.sbr"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\stream.sbr"
	-@erase "$(INTDIR)\usbcamd.obj"
	-@erase "$(INTDIR)\usbcamd.res"
	-@erase "$(INTDIR)\usbcamd.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\usbcamd.bsc"
	-@erase "$(OUTDIR)\usbintel.exp"
	-@erase "$(OUTDIR)\usbintel.ilk"
	-@erase "$(OUTDIR)\usbintel.lib"
	-@erase "$(OUTDIR)\usbintel.map"
	-@erase "$(OUTDIR)\usbintel.pdb"
	-@erase "$(OUTDIR)\usbintel.sys"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /Gz /MT /W3 /GX /Z7 /Od /D "WIN32" /D "NDEBUG" /D "_PNP_POWER_" /D "NTKERN" /D "_X86_" /D "_PNP_POWER_STUB_ENABLED_" /D "PNP" /D "WIN32_LEAN_AND_MEAN" /D "DBG" /D "WINNT" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /D /c" " /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\usbcamd.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\usbcamd.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\intbulk.sbr" \
	"$(INTDIR)\iso.sbr" \
	"$(INTDIR)\reset.sbr" \
	"$(INTDIR)\stream.sbr" \
	"$(INTDIR)\usbcamd.sbr" \
	"$(INTDIR)\dbglog.sbr"

"$(OUTDIR)\usbcamd.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=wdm.lib ks.lib stream.lib ksguid.lib usbd.lib /nologo /entry:"DriverEntry@8" /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)\usbintel.pdb" /map:"$(INTDIR)\usbintel.map" /debug /debugtype:both /machine:I386 /nodefaultlib /def:".\usbcamd.def" /out:"$(OUTDIR)\usbintel.sys" /implib:"$(OUTDIR)\usbintel.lib" 
DEF_FILE= \
	".\usbcamd.def"
LINK32_OBJS= \
	"$(INTDIR)\intbulk.obj" \
	"$(INTDIR)\iso.obj" \
	"$(INTDIR)\reset.obj" \
	"$(INTDIR)\stream.obj" \
	"$(INTDIR)\usbcamd.obj" \
	"$(INTDIR)\usbcamd.res" \
	"$(INTDIR)\dbglog.obj"

"$(OUTDIR)\usbintel.sys" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("usbcamd.dep")
!INCLUDE "usbcamd.dep"
!ELSE 
!MESSAGE Warning: cannot find "usbcamd.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "usbcamd - Win32 Release" || "$(CFG)" == "usbcamd - Win32 Debug"
SOURCE=.\dbglog.c

!IF  "$(CFG)" == "usbcamd - Win32 Release"


"$(INTDIR)\dbglog.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"


"$(INTDIR)\dbglog.obj"	"$(INTDIR)\dbglog.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\intbulk.c

!IF  "$(CFG)" == "usbcamd - Win32 Release"


"$(INTDIR)\intbulk.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"


"$(INTDIR)\intbulk.obj"	"$(INTDIR)\intbulk.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\iso.c

!IF  "$(CFG)" == "usbcamd - Win32 Release"


"$(INTDIR)\iso.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"


"$(INTDIR)\iso.obj"	"$(INTDIR)\iso.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\reset.c

!IF  "$(CFG)" == "usbcamd - Win32 Release"


"$(INTDIR)\reset.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"


"$(INTDIR)\reset.obj"	"$(INTDIR)\reset.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\stream.c

!IF  "$(CFG)" == "usbcamd - Win32 Release"


"$(INTDIR)\stream.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"


"$(INTDIR)\stream.obj"	"$(INTDIR)\stream.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\usbcamd.c

!IF  "$(CFG)" == "usbcamd - Win32 Release"


"$(INTDIR)\usbcamd.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "usbcamd - Win32 Debug"


"$(INTDIR)\usbcamd.obj"	"$(INTDIR)\usbcamd.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\usbcamd.rc

"$(INTDIR)\usbcamd.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 


