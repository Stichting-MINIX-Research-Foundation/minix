#!/usr/bin/awk -
#
#	$NetBSD: MAKEDEV.awk,v 1.26.2.2 2020/06/20 16:19:43 martin Exp $
#
# Copyright (c) 2003 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jaromir Dolecek.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# Script to generate platform MAKEDEV script from MI template, MD
# MAKEDEV.conf and MD/MI major lists
#
# Uses environment variables MACHINE/MACHINE_ARCH to select
# appropriate files, and NETBSDSRCDIR to get root of source tree.

BEGIN {
	# top of source tree, used to find major number list in kernel
	# sources
	machine = ENVIRON["MACHINE"]
	maarch = ENVIRON["MACHINE_ARCH"]
	srcdir = ENVIRON["NETBSDSRCDIR"]
	if (!machine || !maarch || !srcdir) {
		print "ERROR: 'MACHINE', 'MACHINE_ARCH' and 'NETBSDSRCDIR' must be set in environment" > "/dev/stderr"
		exit 1
	}
	top = srcdir "/sys/"
	if (system("test -d '" top "'") != 0) {
		print "ERROR: can't find top of kernel source tree ('" top "' not a directory)" > "/dev/stderr"
		exit 1
	}


	# file with major definitions
	majors[0] = "conf/majors"
	if ((index(maarch, "arm") != 0 || index(maarch, "aarch64")) && system("test -f '" top "arch/" machine "/conf/majors." machine "'") != 0)
		majors[1] = "arch/arm/conf/majors.arm32";
	else if (machine == "sbmips")
		majors[1] = "arch/evbmips/conf/majors.evbmips";
	else if ((maarch == "powerpc" || maarch == "powerpc64") && system("test -f '" top "arch/" machine "/conf/majors." machine "'") != 0)
		majors[1] = "arch/powerpc/conf/majors.powerpc";
	else
		majors[1] = "arch/" machine "/conf/majors." machine;
	nm = 2;

	# process all files with majors and fill the chr[] and blk[]
	# arrays, used in template processing
	for (m = 0; m < nm; m++) {
		file = top majors[m]
		if (system("test -f '" file "'") != 0) {
			print "ERROR: can't find majors file '" file "'" > "/dev/stderr"
			exit 1
		}
		while (getline < file) {
			if ($1 == "include") {
				majors[nm++] = substr($2, 2, length($2)-2);
			} else if ($1 == "device-major") {
				if ($3 == "char") {
					chr[$2] = $4
					if ($5 == "block")
						blk[$2] = $6
				} else if ($3 == "block")
					blk[$2] = $4
			}
		}
		close(file)
	}
	CONSOLE_CMAJOR = chr["cons"]
	if (CONSOLE_CMAJOR == "") {
		print "ERROR: no entry for 'cons' in majors file" > "/dev/stderr"
		exit 1
	}

	# read MD config file for MD device targets
	cfgfile = srcdir "/etc/etc." machine "/MAKEDEV.conf"
	if (system("test -f '" cfgfile "'") != 0) {
		print "ERROR: no platform MAKEDEV.conf - '" cfgfile "' doesn't exist" > "/dev/stderr"
		exit 1
	}
	# skip first two lines
	getline CONFRCSID < cfgfile	# RCS Id
	getline < cfgfile		# blank line
	MDDEV = 0		# MD device targets
	while (getline < cfgfile) {
		#
		# Perform the same blk / chr subsitution that happens below.
		#
		md_deventry = $0
		if (match(md_deventry, /%[a-z0-9]*_(blk|chr)%/)) {
			nam = substr(md_deventry, RSTART + 1, RLENGTH - 6);
			typ = substr(md_deventry, RSTART + RLENGTH - 4, 3);
			dev = ""
			if (typ == "blk") {
				if (nam in blk) {
					dev = blk[nam];
				}
			} else {
				if (nam in chr) {
					dev = chr[nam];
				}
			}
			if (dev != "") {
				parsed = substr(md_deventry, 1, RSTART - 1) dev
				md_deventry = substr(md_deventry, RSTART + RLENGTH)
			}
			md_deventry = parsed md_deventry
		}
		if (MDDEV)
			MDDEV = MDDEV "\n" md_deventry
		else
			MDDEV = md_deventry
	}
	close(cfgfile)

	# determine number of partitions used by platform
	# there are three variants in tree:
	# 1. MAXPARTITIONS = 8
	# 2. MAXPARTITIONS = 16 with no back compat mapping
	# 3. MAXPARTITIONS = 16 with back compat with old limit of 8
	# currently all archs, which moved from 8->16 use same
	# scheme for mapping disk minors, high minor offset
	# if this changes, the below needs to be adjusted and
	# additional makedisk_p16foo needs to be added
	incdir = machine
	diskpartitions = 0
	diskbackcompat = 0
	while (1) {
		inc = top "arch/" incdir "/include/disklabel.h"
		if (system("test -f '" inc "'") != 0) {
			print "ERROR: can't find kernel include file '" inc "'" > "/dev/stderr"
			exit 1
		}
		incdir = 0
		while (getline < inc) {
			if ($1 == "#define" && $2 == "MAXPARTITIONS")
				diskpartitions = $3
			else if ($1 == "#define" && $2 == "OLDMAXPARTITIONS")
				diskbackcompat = $3
			else if ($1 == "#ifndef" && $2 == "RAW_PART" &&
			    RAWDISK_OFF) {
				# special case to ignore #ifndef RAW_PART
				# sections (e.g. in arm/include/disklabel.h,
				# when it is already set in
				# zaurus/include/disklabel.h)
				while (getline < inc) {
					# skip all lines upto the next #endif
					if ($1 == "#endif")
						break;
				}
			} else if ($1 == "#define" && $2 == "RAW_PART")
				RAWDISK_OFF = $3
			else if ($1 == "#include" && 
				 $2 ~ "<.*/disklabel.h>" &&
				 $2 !~ ".*nbinclude.*")
			{
				# wrapper, switch to the right file
				incdir = substr($2, 2)
				sub("/.*", "", incdir)
				break;
			}
		}
		close(inc)

		if (diskpartitions)
			break;

		if (!incdir) {
			print "ERROR: can't determine MAXPARTITIONS from include file '" inc "'" > "/dev/stderr"
			exit 1
		}
	}
	MKDISK = "makedisk_p" diskpartitions	# routine to create disk devs
	DISKMINOROFFSET = diskpartitions
	if (diskbackcompat) {
		MKDISK = MKDISK "high"
		DISKMINOROFFSET = diskbackcompat
	}
	RAWDISK_NAME = sprintf("%c", 97 + RAWDISK_OFF)		# a+offset

	# read etc/master.passwd for user name->UID mapping
	idfile = srcdir "/etc/master.passwd"
	if (system("test -f '" idfile "'") != 0) {
		print "ERROR: can't find password file '" idfile "'" > "/dev/stderr"
		exit 1
	}
	oldFS=FS
	FS=":"
	while (getline < idfile) {
		uid[$1] = $3
	}
	close(idfile)
	FS=oldFS

	# read etc/group for group name->GID mapping
	idfile = srcdir "/etc/group"
	if (system("test -f '" idfile "'") != 0) {
		print "ERROR: can't find group file '" idfile "'" > "/dev/stderr"
		exit 1
	}
	oldFS=FS
	FS=":"
	while (getline < idfile) {
		gid[$1] = $3
	}
	close(idfile)
	FS=oldFS

	# initially no substitutions
	devsubst = 0
	deventry = ""
}

/%MI_DEVICES_BEGIN%/ {
	devsubst = 1;
	next
}

/%MI_DEVICES_END%/ {
	devsubst = 0;
	next
}

# output 'Generated from' lines
/\$[N]etBSD/ {
	print "#"
	print "# Generated from:"

	# MAKEDEV.awk (this script) RCS Id
	ARCSID = "$NetBSD: MAKEDEV.awk,v 1.26.2.2 2020/06/20 16:19:43 martin Exp $"
	gsub(/\$/, "", ARCSID)
	print "#	" ARCSID
	
	# MAKEDEV.tmpl RCS Id
	gsub(/\$/, "")
	print $0

	# MD MAKEDEV.conf RCS Id
	# strip leading hash and insert machine subdirectory name
	gsub(/\$/, "", CONFRCSID)
	sub(/^\# /, "", CONFRCSID)
	sub(/MAKEDEV.conf/, "etc." machine "/MAKEDEV.conf", CONFRCSID)
	print "#	" CONFRCSID

	next # don't print the RCS Id line again
}

# filter the 'PLEASE RUN ...' paragraph
/^\#   PLEASE RUN/, /^\#\#\#\#\#\#/ {
	next
}
 
# filter the device list
/^\# Tapes/,/^$/ {
	next
}

# filter the two unneeded makedisk_p* routines, leave only
# the one used
/^makedisk_p8\(\) \{/, /^\}/ {
	if (MKDISK != "makedisk_p8")
		next;
}
/^makedisk_p16\(\) \{/, /^\}/ {
	if (MKDISK != "makedisk_p16")
		next;
}
/^makedisk_p16high\(\) \{/, /^\}/ {
	if (MKDISK != "makedisk_p16high")
		next;
}

# special cases aside, handle normal line
{
	sub(/^%MD_DEVICES%/, MDDEV)
	sub(/%MKDISK%/, MKDISK)
	sub(/%DISKMINOROFFSET%/, DISKMINOROFFSET)
	sub(/%RAWDISK_OFF%/, RAWDISK_OFF)
	sub(/%RAWDISK_NAME%/, RAWDISK_NAME)
	sub(/%CONSOLE_CMAJOR%/, CONSOLE_CMAJOR)
	parsed = ""
	line = $0
	while (match(line, /%[gu]id_[_a-z]*%/)) {
		typ = substr(line, RSTART + 1, 3);
		nam = substr(line, RSTART + 5, RLENGTH - 6);
		if (typ == "uid") {
			if (!(nam in uid)) {
				print "ERROR unmatched uid in `" $0 "'" > \
				    "/dev/stderr"
				exit 1
			} else
				id = uid[nam];
		} else {
			if (!(nam in gid)) {
				print "ERROR unmatched gid in `" $0 "'" > \
				    "/dev/stderr"
				exit 1
			} else
				id = gid[nam];
		}
		parsed = parsed substr(line, 1, RSTART - 1) id
		line = substr(line, RSTART + RLENGTH)
	}
	$0 = parsed line

	# if device substitutions are not active, do nothing more
	if (!devsubst) {
		print
		next
	}
}

# first line of device entry
/^[a-z].*\)$/ {
	if (length(deventry) > 0) {
		# We have a previous entry to print. Replace all known
		# character and block devices. If no unknown character
		# or block device definition remains within the entry,
		# print it to output, otherwise scrap it.
		parsed = ""
		while (match(deventry, /%[a-z]*_(blk|chr)%/)) {
			nam = substr(deventry, RSTART + 1, RLENGTH - 6);
			typ = substr(deventry, RSTART + RLENGTH - 4, 3);
			if (typ == "blk") {
				if (!(nam in blk)) {
					deventry = $0
					next
				} else
					dev = blk[nam];
			} else {
				if (!(nam in chr)) {
					deventry = $0
					next
				} else
					dev = chr[nam];
			}
			parsed = parsed substr(deventry, 1, RSTART - 1) dev
			deventry = substr(deventry, RSTART + RLENGTH)
		}

		print parsed deventry
	}
	deventry = $0
	next
}

# template line within device substitution section - just keep appending
# to the current entry
{
	deventry = deventry "\n" $0
}
