echo off
rem
rem Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
rem Copyright (C) 2001-2002  Internet Software Consortium.
rem
rem Permission to use, copy, modify, and distribute this software for any
rem purpose with or without fee is hereby granted, provided that the above
rem copyright notice and this permission notice appear in all copies.
rem 
rem THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
rem REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
rem AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
rem INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
rem LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
rem OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
rem PERFORMANCE OF THIS SOFTWARE.

rem BuildAll.bat
rem This script sets up the files necessary ready to build BIND 9
rem and then builds all of the binaries that make up the installation kit.
rem This requires perl to be installed on the system.

rem IMPORTANT NOTE:
rem OpenSSL is a prerequisite for building and running this release of
rem BIND 9. You must fetch the OpenSSL sources yourself from
rem http://www.OpenSSL.org/ and compile it yourself.  The code must reside
rem at the same level as the bind 9.2.0 source tree and it's top-level
rem directory be named openssl-0.9.6k. This restriction will be lifted in
rem a future release of BIND 9 for Windows NT/2000/XP.

echo Setting up the BIND files required for the build

rem Setup the files
call BuildSetup.bat

echo Build all of the Library files

cd ..\lib

cd isc\win32
nmake /nologo -f libisc.mak CFG="libisc - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd dns\win32
nmake /nologo -f libdns.mak CFG="libdns - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd isccfg\win32
nmake /nologo -f libisccfg.mak CFG="libisccfg - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd isccc\win32
nmake /nologo -f libisccc.mak CFG="libisccc - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd bind9\win32
nmake /nologo -f libbind9.mak CFG="libbind9 - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd lwres\win32
nmake /nologo -f liblwres.mak CFG="liblwres - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

rem This is the DLL required for the event Viewer

cd win32\bindevt
nmake /nologo -f bindevt.mak CFG="bindevt - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd ..

echo Now build the apps

cd bin

cd named\win32
nmake /nologo -f named.mak CFG="named - Win32 Release"  NO_EXTERNAL_DEPS="1"

cd ..\..

cd rndc\win32
nmake /nologo -f rndc.mak CFG="rndc - Win32 Release"  NO_EXTERNAL_DEPS="1"

cd ..\..

cd confgen\win32
nmake /nologo -f rndcconfgen.mak CFG="rndcconfgen - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f ddnsconfgen.mak CFG="ddnsconfgen - Win32 Release"  NO_EXTERNAL_DEPS="1"

cd ..\..

cd dig\win32
nmake /nologo -f dig.mak CFG="dig - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo /nologo -f host.mak CFG="host - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f nslookup.mak CFG="nslookup - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd nsupdate\win32
nmake /nologo -f nsupdate.mak CFG="nsupdate - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd check\win32
nmake /nologo -f namedcheckconf.mak CFG="namedcheckconf - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f namedcheckzone.mak CFG="namedcheckzone - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd dnssec\win32
nmake /nologo -f keygen.mak CFG="keygen - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f signzone.mak CFG="signzone - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f dsfromkey.mak CFG="dsfromkey - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f keyfromlabel.mak CFG="keyfromlabel - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f revoke.mak CFG="revoke - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f settime.mak CFG="settime - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd pkcs11\win32
nmake /nologo -f pk11keygen.mak CFG="pk11keygen - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f pk11list.mak CFG="pk11list - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f pk11destroy.mak CFG="pk11destroy - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd tools\win32
nmake /nologo -f arpaname.mak CFG="arpaname - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f genrandom.mak CFG="genrandom - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f nsec3hash.mak CFG="nsec3hash - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f journalprint.mak CFG="journalprint - Win32 Release"  NO_EXTERNAL_DEPS="1"
nmake /nologo -f ischmacfixup.mak CFG="ischmacfixup - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..
rem This is the BIND 9 Installer

cd win32\BINDInstall
nmake /nologo -f BINDInstall.mak CFG="BINDInstall - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..\..

cd ..

cd win32utils

call BuildPost.bat

echo Done.

rem exit here.
