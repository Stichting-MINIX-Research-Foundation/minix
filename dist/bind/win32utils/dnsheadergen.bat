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

cd ..\lib\dns
cd win32
nmake /nologo /f gen.mak CFG="gen - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..
gen -s . -t > include/dns/enumtype.h
gen -s . -c > include/dns/enumclass.h
gen -s . -i -P ./rdata/rdatastructpre.h -S ./rdata/rdatastructsuf.h > include/dns/rdatastruct.h
gen -s . > code.h
cd ..\..\win32utils
