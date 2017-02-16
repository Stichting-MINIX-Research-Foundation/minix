#!/bin/sh
#
# Copyright (C) 2009, 2012  Internet Systems Consortium, Inc. ("ISC")
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

# Id: run-test.sh,v 1.3 2009/06/04 02:56:47 tbox Exp 


sign="../../dnssec/dnssec-signzone -f signed.zone -o example.com."

signit() {
	rm -f signed.zone
	grep '^;' $zone
	$sign $zone
}

expect_success() {
	if ! test -f signed.zone ; then
		echo "Error: expected success, but sign failed for $zone."
	else
		echo "Success:  Sign succeeded for $zone."
	fi
}

expect_failure() {
	if test -f signed.zone ; then
		echo "Error: expected failure, but sign succeeded for $zone."
	else
		echo "Success:  Sign failed (expected) for $zone"
	fi
}

zone="test1.zone" ; signit ; expect_success
zone="test2.zone" ; signit ; expect_failure
zone="test3.zone" ; signit ; expect_failure
zone="test4.zone" ; signit ; expect_success
zone="test5.zone" ; signit ; expect_failure
zone="test6.zone" ; signit ; expect_failure
zone="test7.zone" ; signit ; expect_failure
zone="test8.zone" ; signit ; expect_failure
