echo off
rem
rem Copyright (C) 2007,2009  Internet Systems Consortium, Inc. ("ISC")
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

rem SetupLibs.bat
rem This script copys the openssl and libxml2 dlls into place.
rem This script may be modified by updateopenssl.pl and/or updatelibxml2.pl.

echo Copying the OpenSSL DLL and LICENSE.

copy ..\..\openssl-0.9.8l\out32dll\libeay32.dll ..\Build\Release\
copy ..\..\openssl-0.9.8l\out32dll\libeay32.dll ..\Build\Debug\
copy ..\..\openssl-0.9.8l\LICENSE ..\Build\Release\OpenSSL-LICENSE

echo Copying the libxml DLL.

copy ..\..\libxml2-2.7.3\win32\bin.msvc\libxml2.dll ..\Build\Release\
copy ..\..\libxml2-2.7.3\win32\bin.msvc\libxml2.dll ..\Build\Debug\

rem Done
