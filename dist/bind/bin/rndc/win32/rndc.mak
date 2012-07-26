# Microsoft Developer Studio Generated NMAKE File, Based on rndc.dsp
!IF "$(CFG)" == ""
CFG=rndc - Win32 Debug
!MESSAGE No configuration specified. Defaulting to rndc - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "rndc - Win32 Release" && "$(CFG)" != "rndc - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rndc.mak" CFG="rndc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rndc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "rndc - Win32 Debug" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "rndc - Win32 Release"
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

!IF  "$(CFG)" == "rndc - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Release\rndc.exe"

!ELSE 

ALL : "libbind9 - Win32 Release" "libisccfg - Win32 Release" "libisccc - Win32 Release" "libisc - Win32 Release" "..\..\..\Build\Release\rndc.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libisc - Win32 ReleaseCLEAN" "libisccc - Win32 ReleaseCLEAN" "libisccfg - Win32 ReleaseCLEAN" "libbind9 - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\rndc.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\..\..\Build\Release\rndc.exe"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "../../../../libxml2-2.7.3/include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /I "../../../lib/isccc/include" /I "../../../lib/isccfg/include" /I "../../../lib/bind9/include" /I "../../../lib/dns/include" /D "WIN32" /D "NDEBUG" /D "__STDC__" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\rndc.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\rndc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/isccfg/win32/Release/libisccfg.lib ../../../lib/isccc/win32/Release/libisccc.lib  ../../../lib/bind9/win32/Release/libbind9.lib /nologo /subsystem:console /profile /machine:I386 /out:"../../../Build/Release/rndc.exe" 
LINK32_OBJS= \
	"$(INTDIR)\rndc.obj" \
	"$(INTDIR)\util.obj" \
	"..\..\..\lib\isc\win32\Release\libisc.lib" \
	"..\..\..\lib\isccc\win32\Release\libisccc.lib" \
	"..\..\..\lib\isccfg\win32\Release\libisccfg.lib" \
	"..\..\..\lib\bind9\win32\Release\libbind9.lib"

"..\..\..\Build\Release\rndc.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
    $(_VC_MANIFEST_EMBED_EXE)

!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Debug\rndc.exe" "$(OUTDIR)\rndc.bsc"

!ELSE 

ALL : "libbind9 - Win32 Debug" "libisccfg - Win32 Debug" "libisccc - Win32 Debug" "libisc - Win32 Debug" "..\..\..\Build\Debug\rndc.exe" "$(OUTDIR)\rndc.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libisc - Win32 DebugCLEAN" "libisccc - Win32 DebugCLEAN" "libisccfg - Win32 DebugCLEAN" "libbind9 - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\rndc.obj"
	-@erase "$(INTDIR)\rndc.sbr"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\util.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\rndc.bsc"
	-@erase "$(OUTDIR)\rndc.pdb"
	-@erase "..\..\..\Build\Debug\rndc.exe"
	-@erase "..\..\..\Build\Debug\rndc.ilk"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../../../../libxml2-2.7.3/include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /I "../../../lib/isccc/include" /I "../../../lib/isccfg/include" /I "../../../lib/bind9/include" /I "../../../lib/dns/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\rndc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\rndc.sbr" \
	"$(INTDIR)\util.sbr"

"$(OUTDIR)\rndc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib ../../../lib/isccc/win32/Debug/libisccc.lib  ../../../lib/bind9/win32/Debug/libbind9.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\rndc.pdb" /debug /machine:I386 /out:"../../../Build/Debug/rndc.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\rndc.obj" \
	"$(INTDIR)\util.obj" \
	"..\..\..\lib\isc\win32\Debug\libisc.lib" \
	"..\..\..\lib\isccc\win32\Debug\libisccc.lib" \
	"..\..\..\lib\isccfg\win32\Debug\libisccfg.lib" \
	"..\..\..\lib\bind9\win32\Debug\libbind9.lib"

"..\..\..\Build\Debug\rndc.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("rndc.dep")
!INCLUDE "rndc.dep"
!ELSE 
!MESSAGE Warning: cannot find "rndc.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "rndc - Win32 Release" || "$(CFG)" == "rndc - Win32 Debug"
SOURCE=..\rndc.c

!IF  "$(CFG)" == "rndc - Win32 Release"


"$(INTDIR)\rndc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"


"$(INTDIR)\rndc.obj"	"$(INTDIR)\rndc.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\util.c

!IF  "$(CFG)" == "rndc - Win32 Release"


"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"


"$(INTDIR)\util.obj"	"$(INTDIR)\util.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

!IF  "$(CFG)" == "rndc - Win32 Release"

"libisc - Win32 Release" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" 
   cd "..\..\..\bin\rndc\win32"

"libisc - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"

"libisc - Win32 Debug" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" 
   cd "..\..\..\bin\rndc\win32"

"libisc - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ENDIF 

!IF  "$(CFG)" == "rndc - Win32 Release"

"libisccc - Win32 Release" : 
   cd "..\..\..\lib\isccc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccc.mak" CFG="libisccc - Win32 Release" 
   cd "..\..\..\bin\rndc\win32"

"libisccc - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\isccc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccc.mak" CFG="libisccc - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"

"libisccc - Win32 Debug" : 
   cd "..\..\..\lib\isccc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccc.mak" CFG="libisccc - Win32 Debug" 
   cd "..\..\..\bin\rndc\win32"

"libisccc - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\isccc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccc.mak" CFG="libisccc - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ENDIF 

!IF  "$(CFG)" == "rndc - Win32 Release"

"libisccfg - Win32 Release" : 
   cd "..\..\..\lib\isccfg\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccfg.mak" CFG="libisccfg - Win32 Release" 
   cd "..\..\..\bin\rndc\win32"

"libisccfg - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\isccfg\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccfg.mak" CFG="libisccfg - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"

"libisccfg - Win32 Debug" : 
   cd "..\..\..\lib\isccfg\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccfg.mak" CFG="libisccfg - Win32 Debug" 
   cd "..\..\..\bin\rndc\win32"

"libisccfg - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\isccfg\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisccfg.mak" CFG="libisccfg - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ENDIF 

!IF  "$(CFG)" == "rndc - Win32 Release"

"libbind9 - Win32 Release" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Release" 
   cd "..\..\..\bin\rndc\win32"

"libbind9 - Win32 ReleaseCLEAN" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"

"libbind9 - Win32 Debug" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Debug" 
   cd "..\..\..\bin\rndc\win32"

"libbind9 - Win32 DebugCLEAN" : 
   cd "..\..\..\lib\bind9\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libbind9.mak" CFG="libbind9 - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\..\bin\rndc\win32"

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
