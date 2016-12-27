#!/bin/sh
#
#	$NetBSD: makerumpmanpages.sh,v 1.13 2011/07/24 08:55:30 uch Exp $
#

IFS=' '
COPYRIGHT='.\"	WARNING: GENERATED FILE, DO NOT EDIT
.\"	INSTEAD, EDIT makerumpmanpages.sh AND REGEN
.\"	from: $NetBSD: makerumpmanpages.sh,v 1.13 2011/07/24 08:55:30 uch Exp $
.\"
.\" Copyright (c) 2008-2010 Antti Kantee.  All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"'

MANTMPL1='.Dd November 21, 2010
.Dt RUMP_XXXFSXXX 8
.Os
.Sh NAME
.Nm rump_xxxfsxxx
.Nd mount a xxxfsxxx xxxfssrcxxx with a userspace server
.Sh SYNOPSIS
.Cd "file-system PUFFS"
.Cd "pseudo-device putter"
.Pp
.Nm
.Op options
.Ar xxximagexxx
.Ar mountpoint
.Sh DESCRIPTION
.Em NOTE!
This manual page describes features specific to the
.Xr rump 3
file server.
Please see
.Xr mount_xxxfsxxx 8
for a full description of the available command line options.
.Pp
The
.Nm
utility can be used to mount xxxfsxxx file systems.
It uses
.Xr rump 3
and
.Xr p2k 3
to facilitate running the file system as a server in userspace.
As opposed to
.Xr mount_xxxfsxxx 8 ,
.Nm
does not use file system code within the kernel and therefore does
not require kernel support except
.Xr puffs 4 .
Apart from a minor speed penalty there is no downside with respect to
in-kernel code.'

MANTMPL_BLK='.Pp
.Nm
does not require using
.Xr vnconfig 8
for mounts from regular files and the file path can be passed
directly as the
.Ar xxximagexxx
parameter.
In fact, the use of
.Xr vnconfig 8
is discouraged, since it is unable to properly deal with images on
sparse files.
.Pp
In case the image contains multiple partitions, the desired partition
must be indicated by appending the token
.Dq %DISKLABEL:p%
to the
.Ar xxximagexxx
path.
The letter
.Dq p
specifies the partition as obtained via
.Xr disklabel 8 .
For example, to mount partition
.Dq e
from image
.Pa /tmp/wd0.img ,
use
.Dq /tmp/wd0.img%DISKLABEL:e% .
.Pp
It is recommended that untrusted file system images be mounted with
.Nm
instead of
.Xr mount_xxxfsxxx 8 .
Corrupt file system images commonly cause the file system
to crash the entire kernel, but with
.Nm
only the userspace server process will dump core.'

MANTMPL_NET='.Pp
Even though the
.Nm
file system client runs within a virtual rump kernel in userspace,
it uses host network services
.Pq by means of Dq rump sockin .
This means that regardless of whether using
.Nm
or
.Xr mount_xxxfsxxx 8 ,
the same network configurations will be used.
Currently,
.Dq sockin
supports IPv4.'

MANTMPL2='.Pp
To use
.Nm
via
.Xr mount 8 ,
the flags
.Fl o Ar rump
and
.Fl t Ar xxxfsxxx
should be given.
Similarly,
.Nm
is used instead of
.Xr mount_xxxfsxxx 8
if
.Dq rump
is added to the options field of
.Xr fstab 5 .
.Sh SEE ALSO
.Xr p2k 3 ,
.Xr puffs 3 ,
.Xr rump 3 ,
.Xr mount_xxxfsxxx 8
.Sh HISTORY
The
.Nm
utility first appeared in
.Nx xxxfirstxxx .'

# vary manpages slightly based on the type of server in question
disk="cd9660 efs ext2fs ffs hfs lfs msdos ntfs sysvbfs udf v7fs"
net="nfs smbfs"
fictional="fdesc kernfs tmpfs"
special="au-naturel nqmfs syspuffs"

first5="cd9660 efs ext2fs ffs hfs lfs msdos nfs ntfs syspuffs sysvbfs tmpfs udf"

member ()
{

	what=$1
	shift

	while [ $# -gt 0 ] ; do
		[ "$1" = "${what}" ] && return 0
		shift
	done
	return 1
}

sedsub='s/xxxfsxxx/$fs/g\;s/XXXFSXXX/$fsc/g\;s/xxximagexxx/$image/g\;'\
's/xxxfirstxxx/$first/g\;s/xxxfssrcxxx/$fssrc/g\;'

# auto manual pages
for x in rump_*
do
	fs=${x#rump_}

	# see if we are dealing with a new server
	if ! member $fs $disk $net $fictional $special ; then
		echo ERROR: $fs not found in any class!
		exit 1
	fi

	# special file systems have special manpages
	member $fs $special && continue

	# figure out our type
	if member $fs $disk ; then
		mytype=disk
		image=image
		fssrc=image
	fi
	if member $fs $net ; then
		mytype=net
		image=share
		fssrc=share
	fi
	if member $fs $fictional ; then
		mytype=special
		image=$fs
		fssrc='fictional fs'
	fi

	# which version did server first appear?
	if member $fs $first5 ; then
		first=5.0
	else
		first=6.0
	fi

	fsc=`echo $fs | tr '[:lower:]' '[:upper:]'`
	eval sedstr="${sedsub}"

	printf '.\\"	$NetBSD: makerumpmanpages.sh,v 1.13 2011/07/24 08:55:30 uch Exp $\n.\\"\n' > rump_${fs}/rump_${fs}.8
	echo ${COPYRIGHT} | sed -e 's/\$//g' >> rump_${fs}/rump_${fs}.8

	echo ${MANTMPL1} | sed -e "$sedstr" >> rump_${fs}/rump_${fs}.8
	[ ${mytype} = disk ] && \
	    echo ${MANTMPL_BLK} | sed -e "$sedstr" >> rump_${fs}/rump_${fs}.8
	[ ${mytype} = net ] && \
	    echo ${MANTMPL_NET} | sed -e "$sedstr" >> rump_${fs}/rump_${fs}.8
	echo ${MANTMPL2} | sed -e "$sedstr" >> rump_${fs}/rump_${fs}.8
done
