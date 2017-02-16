#!/bin/sh
#
# Copyright (C) 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: sign.sh,v 1.3 2011/05/26 23:47:28 tbox Exp 

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

zone=example.
infile=example.db.in
outfile=example.db.bad

for i in Xexample.+005+51829.key Xexample.+005+51829.private \
	Xexample.+005+05896.key Xexample.+005+05896.private
do
	cp $i `echo $i | sed s/X/K/`
done

$SIGNER -r $RANDFILE -g -s 20000101000000 -e 20361231235959 -o $zone \
	$infile Kexample.+005+51829 Kexample.+005+51829 \
	> /dev/null 2> signer.err
