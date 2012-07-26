# Microsoft Developer Studio Generated NMAKE File, Based on dig.dsp
!IF "$(CFG)" == ""
CFG=dig - Win32 Debug
!MESSAGE No configuration specified. Defaulting to dig - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "dig - Win32 Release" && "$(CFG)" != "dig - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dig.mak" CFG="dig - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dig - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "dig - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "dig - Win32 Release"
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

!IF  "$(CFG)" == "dig - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Release\dig.exe"

!ELSE 

ALL : "liblwres - Win32 Release" "libbind9 - Win32 Release" "libisc - Win32 Release" "libdns - Win32 Release" "..\..\..\Build\Release\dig.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libdns - Win32 ReleaseCLEAN" "libisc - Win32 ReleaseCLEAN" "libbind9 - Win32 ReleaseCLEAN" "liblwres - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dig.obj"
	-@erase "$(INTDIR)\dighost.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\..\..\Build\Release\dig.exe"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "./" /I "../include" /I "../../../" /I "../../../../libxml2-2.7.3/include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /I "../../../lib/isccfg/include" /I "../../../lib/dns/include" /I "../../../lib/bind9/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /D "WIN32" /D "__STDC__" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\dig.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dig.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/isccfg/win32/Release/libisccfg.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/bind9/win32/Release/libbind9.lib ../../../lib/lwres/win32/Release/liblwres.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\dig.pdb" /machine:I386 /out:"../../../Build/Release/dig.exe" 
LINK32_OBJS= \
	"$(INTDIR)\dig.obj" \
	"$(INTDIR)\dighost.obj" \
	"..\..\..\lib\dns\win32\Release\libdns.lib" \
	"..\..\..\lib\isc\win32\Release\libisc.lib" \
	"..\..\..\lib\isccfg\win32\Release\libisccfg.lib" \
	"..\..\..\lib\bind9\win32\Release\libbind9.lib" \
	"..\..\..\lib\lwres\win32\Release\liblwres.lib"

"..\..\..\Build\Release\dig.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
    $(_VC_MANIFEST_EMBED_EXE)

!ELSEIF  "$(CFG)" == "dig - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Debug\dig.exe" "$(OUTDIR)\dig.bsc"

!ELSE 

ALL : "liblwres - Win32 Debug" "libbind9 - Win32 Debug" "libisc - Win32 Debug" "libdns - Win32 Debug" "..\..\..\Build\Debug\dig.exe" "$(OUTDIR)\dig.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libdns - Win32 DebugCLEAN" "libisc - Win32 DebugCLEAN" "libbind9 - Win32 DebugCLEAN" "liblwres - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dig.obj"
	-@erase "$(INTDIR)\dig.sbr"
	-@erase "$(INTDIR)\dighost.obj"
	-@erase "$(INTDIR)\dighost.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\dig.bsc"
	-@erase "$(OUTDIR)\dig.pdb"
	-@erase "..\..\..\Build\Debug\dig.exe"
	-@erase "..\..\..\Build\Debug\dig.ilk"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../include" /I "../../../" /I "../../../../libxml2-2.7.3/include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /I "../../../lib/isccfg/include" /I "../../../lib/dns/include" /I "../../../lib/bind9/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dig.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\dig.sbr" \
	"$(INTDIR)\dighost.sbr"

"$(OUTDIR)\dig.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/bind9/win32/Debug/libbind9.lib ../../../lib/lwres/win32/Debug/liblwres.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\dig.pdb" /debug /machine:I386 /out:"../../../Build/Debug/dig.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\dig.obj" \
	"$(INTDIR)\dighost.obj" \
	"..\..\..\lib\dns\win32\Debug\libdns.lib" \
	"..\..\..\lib\isc\win32\Debug\libisc.lib" \
	"..\..\..\lib\isccfg\win32\Debug\libisccfg.lib" \
	"..\..\..\lib\bind9\win32\Debug\libbind9.lib" \
	"..\..\..\lib\lwres\win32\Debug\liblwres.lib"

"..\..\..\Build\Debug\dig.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
    $(_VC_MANIFEST_EMBED_EXE)

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
!IF EXISTS("dig.dep")
!INCLUDE "dig.dep"
!ELSE 
!MESSAGE Warning: cannot find "dig.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "dig - Win32 Release" || "$(CFG)" == "dig - Win32 Debug"
SOURCE=..\dig.c

!IF  "$(CFG)" == "dig - Win32 Release"


"$(INTDIR)\dig.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "dig - Win32 Debug"


"$(INTDIR)\dig.obj"	"$(INTDIR)\dig.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\dighost.c

!IF  "$(CFG)" == "dig - Win32 Release"


"$(INTDIR)\dighost.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "dig - Win32 Debug"


"$(INTDIR)\dighost.obj"	"$(INTDIR)\dighost.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

!IF  "$(CFG)" == "dig - Win32 Release"

"libdns - Win32 Release" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Release" 
   cd "..\..\..\bin\dig\win32"

"libdns - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ELSEIF  "$(CFG)" == "dig - Win32 Debug"

"libdns - Win32 Debug" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Debug" 
   cd "..\..\..\bin\dig\win32"

"libdns - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ENDIF 

!IF  "$(CFG)" == "dig - Win32 Release"

"libisc - Win32 Release" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" 
   cd "..\..\..\bin\dig\win32"

"libisc - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ELSEIF  "$(CFG)" == "dig - Win32 Debug"

"libisc - Win32 Debug" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" 
   cd "..\..\..\bin\dig\win32"

"libisc - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ENDIF 

!IF  "$(CFG)" == "dig - Win32 Release"

"libbind9 - Win32 Release" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Release" 
   cd "..\..\..\bin\dig\win32"

"libbind9 - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ELSEIF  "$(CFG)" == "dig - Win32 Debug"

"libbind9 - Win32 Debug" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Debug" 
   cd "..\..\..\bin\dig\win32"

"libbind9 - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ENDIF 

!IF  "$(CFG)" == "dig - Win32 Release"

"liblwres - Win32 Release" : 
   cd "..\..\..\lib\lwres\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\liblwres.mak" CFG="liblwres - Win32 Release" 
   cd "..\..\..\bin\dig\win32"

"liblwres - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\lwres\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\liblwres.mak" CFG="liblwres - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

!ELSEIF  "$(CFG)" == "dig - Win32 Debug"

"liblwres - Win32 Debug" : 
   cd "..\..\..\lib\lwres\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\liblwres.mak" CFG="liblwres - Win32 Debug" 
   cd "..\..\..\bin\dig\win32"

"liblwres - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\lwres\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\liblwres.mak" CFG="liblwres - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\dig\win32"

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
