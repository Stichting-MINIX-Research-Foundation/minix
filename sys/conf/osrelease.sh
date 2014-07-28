#!/bin/sh
#
#	$NetBSD: osrelease.sh,v 1.122 2012/02/16 23:56:57 christos Exp $
#
# Copyright (c) 1997 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Luke Mewburn.
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

# We use the number specified in <sys/param.h>

path="$0"
[ "${path#/*}" = "$path" ] && path="./$path"

release=`grep "define OS_RELEASE" ${path%/*}/../../minix/include/minix/config.h | awk '{ print $3} ' | tr -d '"'  | awk -F. ' { print $1 }'`
major=`grep "define OS_RELEASE" ${path%/*}/../../minix/include/minix/config.h | awk '{ print $3 }' | tr -d '"'  | awk -F. ' { print $2 }'`
minor=`grep "define OS_RELEASE" ${path%/*}/../../minix/include/minix/config.h | awk '{ print $3 }' | tr -d '"'  | awk -F. ' { print $3 }'`


case "$option" in
-m)
	echo $release.$major
	;;
-n)
	echo $release.$major.$minor
	;;
-s)
	echo $release$major$minor
	;;
*)
	echo $release.$major.$minor
	;;
esac
