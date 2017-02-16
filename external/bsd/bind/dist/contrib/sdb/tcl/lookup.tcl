# Copyright (C) 2004, 2007, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: lookup.tcl,v 1.10 2007/06/19 23:47:08 tbox Exp 

#
# Sample lookup procedure for tcldb
#
# This lookup procedure defines zones with identical SOA, NS, and MX
# records at the apex and a single A record that varies from zone to
# zone at the name "www".
#
# Something like this could be used by a web hosting company to serve
# a number of domains without needing to create a separate master file
# for each domain.  Instead, all per-zone data (in this case, a single
# IP address) specified in the named.conf file like this:
#
#   zone "a.com." { type master; database "tcl 10.0.0.42"; };
#   zone "b.com." { type master; database "tcl 10.0.0.99"; };
#
# Since the tcldb driver doesn't support zone transfers, there should
# be at least two identically configured master servers.  In the
# example below, they are assumed to be called ns1.isp.nil and
# ns2.isp.nil.
#

proc lookup {zone name} {
    global dbargs
    switch -- $name {
	@ { return [list \
		{SOA 86400 "ns1.isp.nil. hostmaster.isp.nil. \
		    1 3600 1800 1814400 3600"} \
		{NS 86400 "ns1.isp.nil."} \
		{NS 86400 "ns2.isp.nil."} \
		{MX 86400 "10 mail.isp.nil."} ] }
	www { return [list [list A 3600 $dbargs($zone)] ] }
    }
    return NXDOMAIN
}
