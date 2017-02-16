# Microsoft Developer Studio Generated NMAKE File, Based on dxdriver.dsp
!IF "$(CFG)" == ""
CFG=dxdriver - Win32 Release
!MESSAGE No configuration specified. Defaulting to dxdriver - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "dxdriver - Win32 Release" && "$(CFG)" != "dxdriver - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dxdriver.mak" CFG="dxdriver - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dxdriver - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "dxdriver - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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

!IF  "$(CFG)" == "dxdriver - Win32 Release"
_VC_MANIFEST_INC=0
_VC_MANIFEST_BASENAME=__VC80
!ELSE
_VC_MANIFEST_INC=1
_VC_MANIFEST_BASENAME=__VC80.Debug
!ENDIF

####################################################
# Specifying name of temporary resource file used only in incremental builds:

!if "$(_VC_MANIFEST_INC)" == "1"
_VC_MANIFEST_AUTO_RES=$(_VC_MANIFEST_BASENAME).auto.res
!else
_VC_MANIFEST_AUTO_RES=
!endif

####################################################
# _VC_MANIFEST_EMBED_EXE - command to embed manifest in EXE:

!if "$(_VC_MANIFEST_INC)" == "1"

#MT_SPECIAL_RETURN=1090650113
#MT_SPECIAL_SWITCH=-notify_resource_update
MT_SPECIAL_RETURN=0
MT_SPECIAL_SWITCH=
_VC_MANIFEST_EMBED_EXE= \
if exist $@.manifest mt.exe -manifest $@.manifest -out:$(_VC_MANIFEST_BASENAME).auto.manifest $(MT_SPECIAL_SWITCH) & \
if "%ERRORLEVEL%" == "$(MT_SPECIAL_RETURN)" \
rc /r $(_VC_MANIFEST_BASENAME).auto.rc & \
link $** /out:$@ $(LFLAGS)

!else

_VC_MANIFEST_EMBED_EXE= \
if exist $@.manifest mt.exe -manifest $@.manifest -outputresource:$@;1

!endif

####################################################
# _VC_MANIFEST_EMBED_DLL - command to embed manifest in DLL:

!if "$(_VC_MANIFEST_INC)" == "1"

#MT_SPECIAL_RETURN=1090650113
#MT_SPECIAL_SWITCH=-notify_resource_update
MT_SPECIAL_RETURN=0
MT_SPECIAL_SWITCH=
_VC_MANIFEST_EMBED_EXE= \
if exist $@.manifest mt.exe -manifest $@.manifest -out:$(_VC_MANIFEST_BASENAME).auto.manifest $(MT_SPECIAL_SWITCH) & \
if "%ERRORLEVEL%" == "$(MT_SPECIAL_RETURN)" \
rc /r $(_VC_MANIFEST_BASENAME).auto.rc & \
link $** /out:$@ $(LFLAGS)

!else

_VC_MANIFEST_EMBED_EXE= \
if exist $@.manifest mt.exe -manifest $@.manifest -outputresource:$@;2

!endif
####################################################
# _VC_MANIFEST_CLEAN - command to clean resources files generated temporarily:

!if "$(_VC_MANIFEST_INC)" == "1"

_VC_MANIFEST_CLEAN=-del $(_VC_MANIFEST_BASENAME).auto.res \
    $(_VC_MANIFEST_BASENAME).auto.rc \
    $(_VC_MANIFEST_BASENAME).auto.manifest

!else

_VC_MANIFEST_CLEAN=

!endif

!IF  "$(CFG)" == "dxdriver - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\..\..\Build\Release\dxdriver.dll"

CLEAN :
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\driver.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\dxdriver.exp"
	-@erase "$(OUTDIR)\dxdriver.lib"
	-@erase "..\..\..\..\..\Build\Release\dxdriver.dll"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "../" /I "../../../../../" /I "../../../../../lib/isc/win32" /I "../../../../../lib/isc/win32/include" /I "../../../../../lib/isc/include" /I "../../../../../lib/dns/include" /D "NDEBUG" /D "WIN32" /D "__STDC__" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /Fp"$(INTDIR)\dxdriver.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dxdriver.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\dxdriver.pdb" /machine:I386 /def:".\dxdriver.def" /out:"../../../../../Build/Release/dxdriver.dll" /implib:"$(OUTDIR)\dxdriver.lib" 
DEF_FILE= \
	".\dxdriver.def"
LINK32_OBJS= \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\driver.obj"

"..\..\..\..\..\Build\Release\dxdriver.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
  $(_VC_MANIFEST_EMBED_DLL)

!ELSEIF  "$(CFG)" == "dxdriver - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\..\..\Build\Debug\dxdriver.dll" "$(OUTDIR)\dxdriver.bsc"

CLEAN :
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\DLLMain.sbr"
	-@erase "$(INTDIR)\driver.obj"
	-@erase "$(INTDIR)\driver.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\dxdriver.bsc"
	-@erase "$(OUTDIR)\dxdriver.exp"
	-@erase "$(OUTDIR)\dxdriver.lib"
	-@erase "$(OUTDIR)\dxdriver.pdb"
	-@erase "..\..\..\..\..\Build\Debug\dxdriver.dll"
	-@erase "..\..\..\..\..\Build\Debug\dxdriver.ilk"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "../" /I "../../../../../" /I "../../../../../lib/isc/win32" /I "../../../../../lib/isc/win32/include" /I "../../../../../lib/isc/include" /I "../../../../../lib/dns/include" /D "_DEBUG" /D "WIN32" /D "__STDC__" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\dxdriver.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dxdriver.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\DLLMain.sbr" \
	"$(INTDIR)\driver.sbr"

"$(OUTDIR)\dxdriver.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\dxdriver.pdb" /debug /machine:I386 /def:".\dxdriver.def" /out:"../../../../../Build/Debug/dxdriver.dll" /implib:"$(OUTDIR)\dxdriver.lib" /pdbtype:sept 
DEF_FILE= \
	".\dxdriver.def"
LINK32_OBJS= \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\driver.obj"

"..\..\..\..\..\Build\Debug\dxdriver.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
  $(_VC_MANIFEST_EMBED_DLL)

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
!IF EXISTS("dxdriver.dep")
!INCLUDE "dxdriver.dep"
!ELSE 
!MESSAGE Warning: cannot find "dxdriver.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "dxdriver - Win32 Release" || "$(CFG)" == "dxdriver - Win32 Debug"
SOURCE=.\DLLMain.c

!IF  "$(CFG)" == "dxdriver - Win32 Release"


"$(INTDIR)\DLLMain.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "dxdriver - Win32 Debug"


"$(INTDIR)\DLLMain.obj"	"$(INTDIR)\DLLMain.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=..\driver.c

!IF  "$(CFG)" == "dxdriver - Win32 Release"


"$(INTDIR)\driver.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "dxdriver - Win32 Debug"


"$(INTDIR)\driver.obj"	"$(INTDIR)\driver.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF

####################################################
# Commands to generate initial empty manifest file and the RC file
# that references it, and for generating the .res file:

$(_VC_MANIFEST_BASENAME).auto.res : $(_VC_MANIFEST_BASENAME).auto.rc

$(_VC_MANIFEST_BASENAME).auto.rc : $(_VC_MANIFEST_BASENAME).auto.manifest
    type <<$@
#include <winuser.h>
1RT_MANIFEST"$(_VC_MANIFEST_BASENAME).auto.manifest"
<< KEEP

$(_VC_MANIFEST_BASENAME).auto.manifest :
    type <<$@
<?xml version='1.0' encoding='UTF-8' standalone='yes'?>
<assembly xmlns='urn:schemas-microsoft-com:asm.v1' manifestVersion='1.0'>
</assembly>
<< KEEP
