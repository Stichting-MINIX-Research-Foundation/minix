# Microsoft Developer Studio Generated NMAKE File, Based on namedcheckzone.dsp
!IF "$(CFG)" == ""
CFG=namedcheckzone - Win32 Debug
!MESSAGE No configuration specified. Defaulting to namedcheckzone - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "namedcheckzone - Win32 Release" && "$(CFG)" != "namedcheckzone - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "namedcheckzone.mak" CFG="namedcheckzone - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "namedcheckzone - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "namedcheckzone - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"
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

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Release\named-checkzone.exe"

!ELSE 

ALL : "libisc - Win32 Release" "libdns - Win32 Release" "..\..\..\Build\Release\named-checkzone.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libdns - Win32 ReleaseCLEAN" "libisc - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\check-tool.obj"
	-@erase "$(INTDIR)\named-checkzone.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\..\..\Build\Release\named-checkzone.exe"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "../../../../libxml2-2.7.3/include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /I "../../../lib/dns/include" /I "../../../lib/isccfg/include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "__STDC__" /Fp"$(INTDIR)\namedcheckzone.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\namedcheckzone.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/isccfg/win32/Release/libisccfg.lib ../../../lib/dns/win32/Release/libdns.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\named-checkzone.pdb" /machine:I386 /out:"../../../Build/Release/named-checkzone.exe" 
LINK32_OBJS= \
	"$(INTDIR)\check-tool.obj" \
	"$(INTDIR)\named-checkzone.obj" \
	"..\..\..\lib\dns\win32\Release\libdns.lib" \
	"..\..\..\lib\isccfg\win32\Release\libisccfg.lib" \
	"..\..\..\lib\isc\win32\Release\libisc.lib"

"..\..\..\Build\Release\named-checkzone.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
    $(_VC_MANIFEST_EMBED_EXE)

!ELSEIF  "$(CFG)" == "namedcheckzone - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Debug\named-checkzone.exe" "$(OUTDIR)\namedcheckzone.bsc"

!ELSE 

ALL : "libisc - Win32 Debug" "libdns - Win32 Debug" "..\..\..\Build\Debug\named-checkzone.exe" "$(OUTDIR)\namedcheckzone.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libdns - Win32 DebugCLEAN" "libisc - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\check-tool.obj"
	-@erase "$(INTDIR)\check-tool.sbr"
	-@erase "$(INTDIR)\named-checkzone.obj"
	-@erase "$(INTDIR)\named-checkzone.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\named-checkzone.pdb"
	-@erase "$(OUTDIR)\namedcheckzone.bsc"
	-@erase "..\..\..\Build\Debug\named-checkzone.exe"
	-@erase "..\..\..\Build\Debug\named-checkzone.ilk"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../../../../libxml2-2.7.3/include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /I "../../../lib/dns/include" /I "../../../lib/isccfg/include" /D "_DEBUG" /D "__STDC__" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\namedcheckzone.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\check-tool.sbr" \
	"$(INTDIR)\named-checkzone.sbr"

"$(OUTDIR)\namedcheckzone.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib ../../../lib/dns/win32/Debug/libdns.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\named-checkzone.pdb" /debug /machine:I386 /out:"../../../Build/Debug/named-checkzone.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\check-tool.obj" \
	"$(INTDIR)\named-checkzone.obj" \
	"..\..\..\lib\dns\win32\Debug\libdns.lib" \
	"..\..\..\lib\isccfg\win32\Debug\libisccfg.lib" \
	"..\..\..\lib\isc\win32\Debug\libisc.lib"

"..\..\..\Build\Debug\named-checkzone.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
    $(_VC_MANIFEST_EMBED_EXE)

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("namedcheckzone.dep")
!INCLUDE "namedcheckzone.dep"
!ELSE 
!MESSAGE Warning: cannot find "namedcheckzone.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "namedcheckzone - Win32 Release" || "$(CFG)" == "namedcheckzone - Win32 Debug"
SOURCE="..\check-tool.c"

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"


"$(INTDIR)\check-tool.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "namedcheckzone - Win32 Debug"


"$(INTDIR)\check-tool.obj"	"$(INTDIR)\check-tool.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\named-checkzone.c"

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"


"$(INTDIR)\named-checkzone.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "namedcheckzone - Win32 Debug"


"$(INTDIR)\named-checkzone.obj"	"$(INTDIR)\named-checkzone.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"

"libdns - Win32 Release" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Release" 
   cd "..\..\..\bin\check\win32"

"libdns - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\check\win32"

!ELSEIF  "$(CFG)" == "namedcheckzone - Win32 Debug"

"libdns - Win32 Debug" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Debug" 
   cd "..\..\..\bin\check\win32"

"libdns - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\check\win32"

!ENDIF 

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"

"libisc - Win32 Release" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" 
   cd "..\..\..\bin\check\win32"

"libisc - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\check\win32"

!ELSEIF  "$(CFG)" == "namedcheckzone - Win32 Debug"

"libisc - Win32 Debug" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" 
   cd "..\..\..\bin\check\win32"

"libisc - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\check\win32"

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
