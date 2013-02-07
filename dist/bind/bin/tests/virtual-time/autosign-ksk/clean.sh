# Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

# $Id: clean.sh,v 1.2 2010-06-21 02:31:45 marka Exp $

#
# Clean up after virtual time tests.
#
rm -f */K* */dsset-* */*.signed */*.jnl */tmp*
rm -f dig.out.*
rm -f random.data*
rm -f */named.memstats
rm -f */*vtwrapper.*
rm -f ns1/example.db
rm -f ns1/keyname
