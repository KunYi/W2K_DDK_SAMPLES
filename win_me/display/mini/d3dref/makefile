#
# MAKEFILE for Reference Rasterizer (D3DREF.DLL)
#
# Specify "nmake debug" or "nmake retail" as appropriate.  Builds debug by
# default.  (Object files placed in the DEBUG32 or RETAIL32 respectively)
#
# Assumes the following enviroment variables are set:
#       
#       C32_ROOT   - Path to Visual C++ Directory
#       DDKROOT    - Path to Windows 98 DDK
#       DXDDKROOT  - Path to this DirectX DDK 
#       DXSDKROOT  - Path to DirectX SDK
#       SDKROOT    - Path to Windows 98 SDK
#


CL   = $(C32_ROOT)\bin\cl.exe
LINK = $(C32_ROOT)\bin\link.exe
RC   = $(MSDEVDIR)\bin\RC.exe

# Bit or recursion here, to let us specify "retail" or debug" as the goal,
# yet contain ourselves to one makefile.
debug:
   $(MAKE) DEST=debug32 Goals

retail:
    $(MAKE) DEST=retail32 Goals

CINCLUDES = /Iinc /Idrv /I $(DXDDKROOT)\inc /I$(SDKROOT)\include /I$(DDKROOT)\inc\win_me \
            /I$(C32_ROOT)\include
RCINCLUDES = /Iinc /I$(SDKROOT)\include /I$(C32_ROOT)\include

!if "$(DEST)"=="debug32"

C_DBG_FLAGS = -Zi -DDEBUG -Fd$*.pdb
RC_DBG_FLAGS = -DDEBUG
L_DBG_FLAGS = -DEBUG

!endif

CFLAGS = $(C_DBG_FLAGS) -c -Fo$*.obj $(CINCLUDES) -DWIN95
LFLAGS = $(L_DBG_FLAGS)
RCFLAGS = $(RC_DBG_FLAGS) $(RCINCLUDES) -Fo$*.res -DWIN95

OBJ_LIST = \
# DRV Source files
           $(DEST)\dprim2.obj   \
           $(DEST)\drawprim.obj \
           $(DEST)\init.obj     \
           $(DEST)\primfns.obj \
           $(DEST)\refif.obj   \
           $(DEST)\refprov.obj  \
# RAST source files
           $(DEST)\attrfunc.obj \
           $(DEST)\cvgmask.obj  \
           $(DEST)\edgefunc.obj \
           $(DEST)\fragproc.obj \
           $(DEST)\fragrslv.obj \
           $(DEST)\maplegcy.obj \
           $(DEST)\pixproc.obj  \
           $(DEST)\pixref.obj   \
           $(DEST)\refrast.obj  \
           $(DEST)\refrasti.obj \
           $(DEST)\refs3tc.obj  \
           $(DEST)\rrutil.obj   \
           $(DEST)\rtarget.obj  \
           $(DEST)\scancnv.obj  \
           $(DEST)\setup.obj    \
           $(DEST)\shadow.obj   \
           $(DEST)\texmap.obj   \
           $(DEST)\texstage.obj \
           $(DEST)\texture.obj  \
# TNL source Files
           $(DEST)\clipping.obj \
           $(DEST)\lighting.obj \
           $(DEST)\procprim.obj \
           $(DEST)\reftnl.obj   \
           $(DEST)\xform.obj 


LIB_LIST = user32.lib uuid.lib ddraw.lib advapi32.lib



Goals: makedir $(DEST)\d3dref.dll

makedir:
        @if not exist $(DEST)\nul mkdir $(DEST)

$(DEST)\d3dref.dll: $(OBJ_LIST) $(DEST)\d3dref.res
    $(LINK) $(LFLAGS) /DLL /MACHINE:x86 /DEF:link\d3dref.def /OUT:$(DEST)\d3dref.dll /LIBPATH:$(DDKROOT)\lib\win_me /LIBPATH:$(DXSDKROOT)\lib /LIBPATH:$(SDKROOT)\lib /LIBPATH:$(C32_ROOT)\lib $(OBJ_LIST) $(LIB_LIST) $(DEST)\d3dref.res


{link}.rc{$(DEST)}.res:
   $(RC) $(RCFLAGS) $<

{drv}.cpp{$(DEST)}.obj:
   $(CL) $(CFLAGS) $<
               
{drv}.c{$(DEST)}.obj:
   $(CL) $(CFLAGS) $<


{rast}.cpp{$(DEST)}.obj:
   $(CL) $(CFLAGS) $<
               
{rast}.c{$(DEST)}.obj:
   $(CL) $(CFLAGS) $<


{tnl}.cpp{$(DEST)}.obj:
   $(CL) $(CFLAGS) $<
               
{tnl}.c{$(DEST)}.obj:
   $(CL) $(CFLAGS) $<


clean:
!if EXIST(debug32)
    echo Y | del debug32\*.* > nul
    rmdir debug32
!endif
!if EXIST(retail32)
    echo Y | del retail32\*.* > nul
    rmdir retail32
!endif

