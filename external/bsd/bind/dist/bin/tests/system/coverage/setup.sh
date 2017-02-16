#!/bin/sh
#
# Copyright (C) 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

KEYGEN="$KEYGEN -qr $RANDFILE"

$SHELL clean.sh

ln -s $CHECKZONE named-compilezone

# Test 1: KSK goes inactive before successor is active
dir=01-ksk-inactive
rm -f $dir/K*.key
rm -f $dir/K*.private
ksk1=`$KEYGEN -K $dir -3fk example.com`
$SETTIME -K $dir -I +9mo -D +1y $ksk1 > /dev/null 2>&1
ksk2=`$KEYGEN -K $dir -S $ksk1`
$SETTIME -K $dir -I +7mo $ksk1 > /dev/null 2>&1
zsk1=`$KEYGEN -K $dir -3 example.com`

# Test 2: ZSK goes inactive before successor is active
dir=02-zsk-inactive
rm -f $dir/K*.key
rm -f $dir/K*.private
zsk1=`$KEYGEN -K $dir -3 example.com`
$SETTIME -K $dir -I +9mo -D +1y $zsk1 > /dev/null 2>&1
zsk2=`$KEYGEN -K $dir -S $zsk1`
$SETTIME -K $dir -I +7mo $zsk1 > /dev/null 2>&1
ksk1=`$KEYGEN -K $dir -3fk example.com`

# Test 3: KSK is unpublished before its successor is published
dir=03-ksk-unpublished
rm -f $dir/K*.key
rm -f $dir/K*.private
ksk1=`$KEYGEN -K $dir -3fk example.com`
$SETTIME -K $dir -I +9mo -D +1y $ksk1 > /dev/null 2>&1
ksk2=`$KEYGEN -K $dir -S $ksk1`
$SETTIME -K $dir -D +6mo $ksk1 > /dev/null 2>&1
zsk1=`$KEYGEN -K $dir -3 example.com`

# Test 4: ZSK is unpublished before its successor is published
dir=04-zsk-unpublished
rm -f $dir/K*.key
rm -f $dir/K*.private
zsk1=`$KEYGEN -K $dir -3 example.com`
$SETTIME -K $dir -I +9mo -D +1y $zsk1 > /dev/null 2>&1
zsk2=`$KEYGEN -K $dir -S $zsk1`
$SETTIME -K $dir -D +6mo $zsk1 > /dev/null 2>&1
ksk1=`$KEYGEN -K $dir -3fk example.com`

# Test 5: KSK deleted and successor published before KSK is deactivated
# and successor activated.
dir=05-ksk-unpub-active
rm -f $dir/K*.key
rm -f $dir/K*.private
ksk1=`$KEYGEN -K $dir -3fk example.com`
$SETTIME -K $dir -I +9mo -D +8mo $ksk1 > /dev/null 2>&1
ksk2=`$KEYGEN -K $dir -S $ksk1`
zsk1=`$KEYGEN -K $dir -3 example.com`

# Test 6: ZSK deleted and successor published before ZSK is deactivated
# and successor activated.
dir=06-zsk-unpub-active
rm -f $dir/K*.key
rm -f $dir/K*.private
zsk1=`$KEYGEN -K $dir -3 example.com`
$SETTIME -K $dir -I +9mo -D +8mo $zsk1 > /dev/null 2>&1
zsk2=`$KEYGEN -K $dir -S $zsk1`
ksk1=`$KEYGEN -K $dir -3fk example.com`

# Test 7: KSK rolled with insufficient delay after prepublication.
dir=07-ksk-ttl
rm -f $dir/K*.key
rm -f $dir/K*.private
ksk1=`$KEYGEN -K $dir -3fk example.com`
$SETTIME -K $dir -I +9mo -D +1y $ksk1 > /dev/null 2>&1
ksk2=`$KEYGEN -K $dir -S $ksk1`
# allow only 1 day between publication and activation
$SETTIME -K $dir -P +269d $ksk2 > /dev/null 2>&1
zsk1=`$KEYGEN -K $dir -3 example.com`

# Test 8: ZSK rolled with insufficient delay after prepublication.
dir=08-zsk-ttl
rm -f $dir/K*.key
rm -f $dir/K*.private
zsk1=`$KEYGEN -K $dir -3 example.com`
$SETTIME -K $dir -I +9mo -D +1y $zsk1 > /dev/null 2>&1
zsk2=`$KEYGEN -K $dir -S $zsk1`
# allow only 1 day between publication and activation
$SETTIME -K $dir -P +269d $zsk2 > /dev/null 2>&1
ksk1=`$KEYGEN -K $dir -3fk example.com`

# Test 9: KSK goes inactive before successor is active, but checking ZSKs
dir=09-check-zsk
rm -f $dir/K*.key
rm -f $dir/K*.private
ksk1=`$KEYGEN -K $dir -3fk example.com`
$SETTIME -K $dir -I +9mo -D +1y $ksk1 > /dev/null 2>&1
ksk2=`$KEYGEN -K $dir -S $ksk1`
$SETTIME -K $dir -I +7mo $ksk1 > /dev/null 2>&1
zsk1=`$KEYGEN -K $dir -3 example.com`

# Test 10: ZSK goes inactive before successor is active, but checking KSKs
dir=10-check-ksk
rm -f $dir/K*.key
rm -f $dir/K*.private
zsk1=`$KEYGEN -K $dir -3 example.com`
$SETTIME -K $dir -I +9mo -D +1y $zsk1 > /dev/null 2>&1
zsk2=`$KEYGEN -K $dir -S $zsk1`
$SETTIME -K $dir -I +7mo $zsk1 > /dev/null 2>&1
ksk1=`$KEYGEN -K $dir -3fk example.com`

# Test 11: ZSK goes inactive before successor is active, but after cutoff
dir=11-cutoff
rm -f $dir/K*.key
rm -f $dir/K*.private
zsk1=`$KEYGEN -K $dir -3 example.com`
$SETTIME -K $dir -I +18mo -D +2y $zsk1 > /dev/null 2>&1
zsk2=`$KEYGEN -K $dir -S $zsk1`
$SETTIME -K $dir -I +16mo $zsk1 > /dev/null 2>&1
ksk1=`$KEYGEN -K $dir -3fk example.com`
