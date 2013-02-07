# Microsoft Developer Studio Project File - Name="liblwres" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=liblwres - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "liblwres.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "liblwres.mak" CFG="liblwres - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "liblwres - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "liblwres - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "liblwres - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "liblwres_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "../../../lib/lwres/win32/include/lwres" /I "include" /I "../include" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "__STDC__" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBLWRES_EXPORTS" /YX /FD /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 user32.lib advapi32.lib ws2_32.lib iphlpapi.lib /nologo /dll /machine:I386 /out:"../../../Build/Release/liblwres.dll"

!ELSEIF  "$(CFG)" == "liblwres - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "liblwres_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../lib/lwres/win32/include/lwres" /I "include" /I "../include" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/isc/include" /I "../../../lib/isc/noatomic/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "__STDC__" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBLWRES_EXPORTS" /FR /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 user32.lib advapi32.lib ws2_32.lib iphlpapi.lib /nologo /dll /debug /machine:I386 /out:"../../../Build/Debug/liblwres.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "liblwres - Win32 Release"
# Name "liblwres - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\context.c
# End Source File
# Begin Source File

SOURCE=.\DLLMain.c
# End Source File
# Begin Source File

SOURCE=..\gai_strerror.c
# End Source File
# Begin Source File

SOURCE=..\getaddrinfo.c
# End Source File
# Begin Source File

SOURCE=..\gethost.c
# End Source File
# Begin Source File

SOURCE=..\getipnode.c
# End Source File
# Begin Source File

SOURCE=..\getnameinfo.c
# End Source File
# Begin Source File

SOURCE=..\getrrset.c
# End Source File
# Begin Source File

SOURCE=..\herror.c
# End Source File
# Begin Source File

SOURCE=..\lwbuffer.c
# End Source File
# Begin Source File

SOURCE=.\lwconfig.c
# End Source File
# Begin Source File

SOURCE=..\lwinetaton.c
# End Source File
# Begin Source File

SOURCE=..\lwinetntop.c
# End Source File
# Begin Source File

SOURCE=..\lwinetpton.c
# End Source File
# Begin Source File

SOURCE=..\lwpacket.c
# End Source File
# Begin Source File

SOURCE=..\lwres_gabn.c
# End Source File
# Begin Source File

SOURCE=..\lwres_gnba.c
# End Source File
# Begin Source File

SOURCE=..\lwres_grbn.c
# End Source File
# Begin Source File

SOURCE=..\lwres_noop.c
# End Source File
# Begin Source File

SOURCE=..\lwresutil.c
# End Source File
# Begin Source File

SOURCE=.\socket.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\lwres\context.h
# End Source File
# Begin Source File

SOURCE=.\include\lwres\int.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\ipv6.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\lang.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\list.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\lwbuffer.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\lwpacket.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\lwres.h
# End Source File
# Begin Source File

SOURCE=.\include\lwres\net.h
# End Source File
# Begin Source File

SOURCE=.\include\lwres\netdb.h
# End Source File
# Begin Source File

SOURCE=.\include\lwres\platform.h
# End Source File
# Begin Source File

SOURCE=..\include\lwres\result.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\liblwres.def
# End Source File
# End Target
# End Project
