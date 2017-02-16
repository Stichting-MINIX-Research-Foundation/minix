# Copyright (C) 2011, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

rm -f ns1/K*
rm -f ns1/*.signed
rm -f ns1/*.db
rm -f ns1/bind.keys
rm -f ns1/*.mkeys.jnl
rm -f ns1/*.mkeys
rm -f */named.run
rm -f */named.memstats
rm -f ns1/dsset-*.
rm -f ns2/*.mkeys
rm -f ns2/*.mkeys.jnl
rm -f dig.out.ns?.test*
rm -f ns2/named.secroots
