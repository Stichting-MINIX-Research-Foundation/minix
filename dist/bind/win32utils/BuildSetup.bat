echo off
rem
rem Copyright (C) 2004,2005  Internet Systems Consortium, Inc. ("ISC")
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

rem BuildSetup.bat
rem This script sets up the files necessary ready to build BIND 9.
rem This requires perl to be installed on the system.

rem Set up the configuration file
cd ..
copy config.h.win32 config.h
cd win32utils

rem Get and update for the latest build of the openssl and libxml libraries
perl updateopenssl.pl
perl updatelibxml2.pl

rem Generate the version information
perl makeversion.pl

rem Generate header files for lib/dns

call dnsheadergen.bat

rem Make sure that the Build directories are there.

if NOT Exist ..\Build mkdir ..\Build
if NOT Exist ..\Build\Release mkdir ..\Build\Release
if NOT Exist ..\Build\Debug mkdir ..\Build\Debug

echo Copying the ARM and the Installation Notes.

copy ..\COPYRIGHT ..\Build\Release
copy ..\README ..\Build\Release
copy ..\HISTORY ..\Build\Release
copy readme1st.txt ..\Build\Release
copy index.html ..\Build\Release
copy ..\doc\arm\*.html ..\Build\Release
copy ..\doc\arm\Bv9ARM.pdf ..\Build\Release
copy ..\CHANGES ..\Build\Release
copy ..\FAQ ..\Build\Release

echo Copying the standalone manual pages.

copy ..\bin\named\named.html ..\Build\Release
copy ..\bin\rndc\*.html ..\Build\Release
copy ..\bin\confgen\*.html ..\Build\Release
copy ..\bin\dig\*.html ..\Build\Release
copy ..\bin\nsupdate\*.html ..\Build\Release
copy ..\bin\check\*.html ..\Build\Release
copy ..\bin\dnssec\dnssec-keygen.html ..\Build\Release
copy ..\bin\dnssec\dnssec-signzone.html ..\Build\Release
copy ..\bin\dnssec\dnssec-dsfromkey.html ..\Build\Release
copy ..\bin\dnssec\dnssec-keyfromlabel.html ..\Build\Release
copy ..\bin\pkcs11\pkcs11-keygen.html ..\Build\Release
copy ..\bin\pkcs11\pkcs11-list.html ..\Build\Release
copy ..\bin\pkcs11\pkcs11-destroy.html ..\Build\Release

echo Copying the migration notes.

copy ..\doc\misc\migration ..\Build\Release
copy ..\doc\misc\migration-4to9 ..\Build\Release

call SetupLibs.bat

rem
rem set vcredist here so that it is correctly expanded in the if body 
rem
set vcredist=BootStrapper\Packages\vcredist_x86\vcredist_x86.exe

if Defined FrameworkSDKDir (

rem
rem vcredist_x86.exe path relative to FrameworkSDKDir
rem 
if Exist "%FrameworkSDKDir%\%vcredist%" (

echo Copying Visual C x86 Redistributable Installer

rem
rem Use /Y so we allways have the current version of the installer.
rem

copy /Y "%FrameworkSDKDir%\%vcredist%" ..\Build\Release\
copy /Y "%FrameworkSDKDir%\%vcredist%" ..\Build\Debug\

) else (
	echo "**** %FrameworkSDKDir%\%vcredist% not found ****"
)
) else (
	echo "**** Warning FrameworkSDKDir not defined ****"
	echo "****         Run vsvars32.bat            ****"
)

echo Running Message Compiler

cd ..\lib\win32\bindevt
mc bindevt.mc
cd ..\..\..\win32utils

rem Done
