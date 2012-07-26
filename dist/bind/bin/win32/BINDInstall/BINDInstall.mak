# Microsoft Developer Studio Generated NMAKE File, Based on BINDInstall.dsp
!IF "$(CFG)" == ""
CFG=BINDInstall - Win32 Debug
!MESSAGE No configuration specified. Defaulting to BINDInstall - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "BINDInstall - Win32 Release" && "$(CFG)" != "BINDInstall - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BINDInstall.mak" CFG="BINDInstall - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BINDInstall - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "BINDInstall - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "BINDInstall - Win32 Release"
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

!IF  "$(CFG)" == "BINDInstall - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\BINDInstall.exe"


CLEAN :
	-@erase "$(INTDIR)\AccountInfo.obj"
	-@erase "$(INTDIR)\BINDInstall.obj"
	-@erase "$(INTDIR)\BINDInstall.pch"
	-@erase "$(INTDIR)\BINDInstall.res"
	-@erase "$(INTDIR)\BINDInstallDlg.obj"
	-@erase "$(INTDIR)\DirBrowse.obj"
	-@erase "$(INTDIR)\ntgroups.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\VersionInfo.obj"
	-@erase "..\..\..\Build\Release\BINDInstall.exe"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\..\.." /I "..\include" /I "..\..\..\include" /I "..\..\named\win32\include" /I "..\..\..\lib\isc\win32\include" /I "..\..\..\lib\isc\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\BINDInstall.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\BINDInstall.res" /d "NDEBUG"
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\BINDInstall.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=version.lib netapi32.lib /nologo /subsystem:windows /pdb:none /machine:I386 /out:"..\..\..\Build\Release\BINDInstall.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AccountInfo.obj" \
	"$(INTDIR)\BINDInstall.obj" \
	"$(INTDIR)\BINDInstallDlg.obj" \
	"$(INTDIR)\DirBrowse.obj" \
	"$(INTDIR)\ntgroups.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\VersionInfo.obj" \
	"$(INTDIR)\BINDInstall.res"

"..\..\..\Build\Release\BINDInstall.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<
    $(_VC_MANIFEST_EMBED_EXE)

!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\BINDInstall.exe" "$(OUTDIR)\BINDInstall.bsc"


CLEAN :
	-@erase "$(INTDIR)\AccountInfo.obj"
	-@erase "$(INTDIR)\AccountInfo.sbr"
	-@erase "$(INTDIR)\BINDInstall.obj"
	-@erase "$(INTDIR)\BINDInstall.pch"
	-@erase "$(INTDIR)\BINDInstall.res"
	-@erase "$(INTDIR)\BINDInstall.sbr"
	-@erase "$(INTDIR)\BINDInstallDlg.obj"
	-@erase "$(INTDIR)\BINDInstallDlg.sbr"
	-@erase "$(INTDIR)\DirBrowse.obj"
	-@erase "$(INTDIR)\DirBrowse.sbr"
	-@erase "$(INTDIR)\ntgroups.obj"
	-@erase "$(INTDIR)\ntgroups.sbr"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\StdAfx.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\VersionInfo.obj"
	-@erase "$(INTDIR)\VersionInfo.sbr"
	-@erase "$(OUTDIR)\BINDInstall.bsc"
	-@erase "..\..\..\Build\Debug\BINDInstall.exe"
	-@$(_VC_MANIFEST_CLEAN)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\.." /I "..\include" /I "..\..\..\include" /I "..\..\named\win32\include" /I "..\..\..\lib\isc\win32\include" /I "..\..\..\lib\isc\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\BINDInstall.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\BINDInstall.res" /d "_DEBUG"
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\BINDInstall.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\AccountInfo.sbr" \
	"$(INTDIR)\BINDInstall.sbr" \
	"$(INTDIR)\BINDInstallDlg.sbr" \
	"$(INTDIR)\DirBrowse.sbr" \
	"$(INTDIR)\ntgroups.sbr" \
	"$(INTDIR)\StdAfx.sbr" \
	"$(INTDIR)\VersionInfo.sbr"

"$(OUTDIR)\BINDInstall.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=version.lib netapi32.lib /nologo /subsystem:windows /pdb:none /debug /machine:I386 /out:"..\..\..\Build\Debug\BINDInstall.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AccountInfo.obj" \
	"$(INTDIR)\BINDInstall.obj" \
	"$(INTDIR)\BINDInstallDlg.obj" \
	"$(INTDIR)\DirBrowse.obj" \
	"$(INTDIR)\ntgroups.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\VersionInfo.obj" \
	"$(INTDIR)\BINDInstall.res"

"..\..\..\Build\Debug\BINDInstall.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("BINDInstall.dep")
!INCLUDE "BINDInstall.dep"
!ELSE 
!MESSAGE Warning: cannot find "BINDInstall.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "BINDInstall - Win32 Release" || "$(CFG)" == "BINDInstall - Win32 Debug"
SOURCE=.\AccountInfo.cpp

!IF  "$(CFG)" == "BINDInstall - Win32 Release"


"$(INTDIR)\AccountInfo.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"


"$(INTDIR)\AccountInfo.obj"	"$(INTDIR)\AccountInfo.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ENDIF 

SOURCE=.\BINDInstall.cpp

!IF  "$(CFG)" == "BINDInstall - Win32 Release"


"$(INTDIR)\BINDInstall.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"


"$(INTDIR)\BINDInstall.obj"	"$(INTDIR)\BINDInstall.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ENDIF 

SOURCE=.\BINDInstallDlg.cpp

!IF  "$(CFG)" == "BINDInstall - Win32 Release"


"$(INTDIR)\BINDInstallDlg.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"


"$(INTDIR)\BINDInstallDlg.obj"	"$(INTDIR)\BINDInstallDlg.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ENDIF 

SOURCE=.\DirBrowse.cpp

!IF  "$(CFG)" == "BINDInstall - Win32 Release"


"$(INTDIR)\DirBrowse.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"


"$(INTDIR)\DirBrowse.obj"	"$(INTDIR)\DirBrowse.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ENDIF 

SOURCE=..\..\..\lib\isc\win32\ntgroups.c

!IF  "$(CFG)" == "BINDInstall - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\include" /I "..\..\..\include" /I "..\..\named\win32\include" /I "..\..\..\lib\isc\win32\include" /I "..\..\..\lib\isc\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\ntgroups.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\include" /I "..\..\..\include" /I "..\..\named\win32\include" /I "..\..\..\lib\isc\win32\include" /I "..\..\..\lib\isc\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /GZ /c 

"$(INTDIR)\ntgroups.obj"	"$(INTDIR)\ntgroups.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\StdAfx.cpp

!IF  "$(CFG)" == "BINDInstall - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\include" /I "..\..\..\include" /I "..\..\named\win32\include" /I "..\..\..\lib\isc\win32\include" /I "..\..\..\lib\isc\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\BINDInstall.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\BINDInstall.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\include" /I "..\..\..\include" /I "..\..\named\win32\include" /I "..\..\..\lib\isc\win32\include" /I "..\..\..\lib\isc\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\BINDInstall.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /GZ /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\StdAfx.sbr"	"$(INTDIR)\BINDInstall.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\VersionInfo.cpp

!IF  "$(CFG)" == "BINDInstall - Win32 Release"


"$(INTDIR)\VersionInfo.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ELSEIF  "$(CFG)" == "BINDInstall - Win32 Debug"


"$(INTDIR)\VersionInfo.obj"	"$(INTDIR)\VersionInfo.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\BINDInstall.pch"


!ENDIF 

SOURCE=.\BINDInstall.rc

"$(INTDIR)\BINDInstall.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



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
