echo off
rem
rem Copyright (C) 2005  Internet Systems Consortium, Inc. ("ISC")
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

rem BuildPost.bat
rem This script does the final stages if BINDBuild.dsw is used.

echo Copying named-checkzone.exe to named-compilezone.exe

copy /Y ..\Build\Release\named-checkzone.exe ..\Build\Release\named-compilezone.exe
if exist ..\Build\Debug\named-checkzone.exe copy /Y ..\Build\Debug\named-checkzone.exe ..\Build\Debug\named-compilezone.exe
if exist ..\Build\Debug\named-checkzone.ilk copy /Y ..\Build\Debug\named-checkzone.ilk ..\Build\Debug\named-compilezone.ilk

echo Done.

rem exit here.
