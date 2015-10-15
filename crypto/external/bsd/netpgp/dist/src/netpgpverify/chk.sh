#! /bin/sh

# $NetBSD: chk.sh,v 1.3 2015/02/05 01:26:54 agc Exp $

# Copyright (c) 2013,2014,2015 Alistair Crooks <agc@NetBSD.org>
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

die() {
	echo "$*" >&2
	exit 1
}

os=EdgeBSD
osrev=6
arch=amd64
pkgsrc=pkgsrc-2013Q1
keyring=pubring.gpg
while [ $# -gt 0 ]; do
	case "$1" in
	--arch|-a)	arch=$2; shift ;;
	--keyring|-k)	keyring=$2; shift ;;
	--os|-o)	os=$2; shift ;;
	--pkgsrc)	pkgsrc=$2; shift ;;
	-v)		set -x ;;
	*)		break ;;
	esac
	shift
done

#fetch file
repo=ftp://ftp.edgebsd.org/pub/pkgsrc/packages/${os}/${os}-${osrev}/${arch}/${pkgsrc}/All/

if [ ! -f $1 ]; then
	case "${repo}" in
	*/)	remote=${repo}$1 ;;
	*)	remote=${repo}/$1 ;;
	esac
	ftp ${remote}
fi

name=$(basename $1 .tgz)
dir=$(mktemp -d /tmp/chk.XXXXXX)
here=$(pwd)
case "$1" in
/*)	archive=$1 ;;
*)	archive=${here}/$1 ;;
esac
(cd ${dir} && ar x ${archive})

# grab values from already calculated hashes
digest=$(awk '$1 ~ /algorithm:/ { print $2 }' ${dir}/+PKG_HASH)
blocksize=$(awk '/^block size:/ { print $3 }' ${dir}/+PKG_HASH)

# check the hashes in +PKG_HASH match the original archive
size=$(ls -l ${dir}/$1 | awk '{ print $5 }')
printf "pkgsrc signature\n\nversion: 1\n" > ${dir}/calc
printf "pkgname: %s\n" ${name} >> ${dir}/calc
printf "algorithm: ${digest}\n" >> ${dir}/calc
printf "block size: ${blocksize}\n" >> ${dir}/calc
printf "file size: %s\n\n" ${size} >> ${dir}/calc
off=0
n=0
while [ ${off} -lt ${size} ]; do
	rm -f ${dir}/in
	dd if=${dir}/$1 of=${dir}/in bs=${blocksize} count=1 skip=${n} 2>/dev/null
	digest ${digest} < ${dir}/in >> ${dir}/calc
	off=$(( off + ${blocksize} ))
	n=$(( n + 1 ))
done
printf "end pkgsrc signature\n" >> ${dir}/calc

# make sure what was signed is what we have
diff ${dir}/+PKG_HASH ${dir}/calc || die "Bad hashes generated"

# use netpgpverify to verify the signature
if [ -x /usr/bin/netpgpverify -o -x /usr/pkg/bin/netpgpverify ]; then
	echo "=== Using netpgpverify to verify the package signature ==="
	# check the signature in +PKG_GPG_SIGNATURE
	cp ${keyring} ${dir}/pubring.gpg
	# calculate the sig file we want to verify
	echo "-----BEGIN PGP SIGNED MESSAGE-----" > ${dir}/${name}.sig
	echo "Hash: ${digest}" >> ${dir}/${name}.sig
	echo "" >> ${dir}/${name}.sig
	cat ${dir}/+PKG_HASH ${dir}/+PKG_GPG_SIGNATURE >> ${dir}/${name}.sig
	(cd ${dir} && ${here}/netpgpverify -k pubring.gpg ${name}.sig) || die "Bad signature"
else
	echo "=== Using gpg to verify the package signature ==="
	gpg --recv --keyserver pgp.mit.edu 0x6F3AF5E2
	(cd ${dir} && gpg --verify --homedir=${dir} ./+PKG_GPG_SIGNATURE ./+PKG_HASH) || die "Bad signature"
fi
echo "Signatures match on ${name} package"

# clean up
rm -rf ${dir}

exit 0
