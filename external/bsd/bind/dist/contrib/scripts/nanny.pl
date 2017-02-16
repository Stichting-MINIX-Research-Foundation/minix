#!/usr/bin/perl
#
# Copyright (C) 2004, 2007, 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

# Id: nanny.pl,v 1.11 2007/06/19 23:47:07 tbox Exp 

# A simple nanny to make sure named stays running.

$pid_file_location = '/var/run/named.pid';
$nameserver_location = 'localhost';
$dig_program = 'dig';
$named_program =  'named';

fork() && exit();

for (;;) {
	$pid = 0;
	open(FILE, $pid_file_location) || goto restart;
	$pid = <FILE>;
	close(FILE);
	chomp($pid);

	$res = kill 0, $pid;

	goto restart if ($res == 0);

	$dig_command =
	       "$dig_program +short . \@$nameserver_location > /dev/null";
	$return = system($dig_command);
	goto restart if ($return == 9);

	sleep 30;
	next;

 restart:
	if ($pid != 0) {
		kill 15, $pid;
		sleep 30;
	}
	system ($named_program);
	sleep 120;
}
