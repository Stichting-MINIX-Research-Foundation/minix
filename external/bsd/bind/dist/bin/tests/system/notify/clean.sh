#!/bin/sh
#
# Copyright (C) 2004, 2007, 2011, 2012, 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000, 2001  Internet Software Consortium.
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

# Id: clean.sh,v 1.14 2011/10/17 23:46:33 tbox Exp 

#
# Clean up after zone transfer tests.
#

rm -f */named.memstats
rm -f dig.out.?.ns5.test*
rm -f dig.out.ns2.test*
rm -f dig.out.ns3.test*
rm -f dig.out.ns4.test*
rm -f log.out
rm -f ns2/example.db
rm -f ns2/x21.db*
rm -f ns3/example.bk
rm -f ns4/x21.bk*
rm -f ns5/x21.bk-b
rm -f ns5/x21.bk-b.jnl
rm -f ns5/x21.bk-c
rm -f ns5/x21.bk-c.jnl
rm -f ns5/x21.db.jnl
