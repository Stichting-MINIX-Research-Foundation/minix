#! /bin/sh
#
# $NetBSD: bpm.sh.in,v 1.4 2007/08/02 23:30:20 wiz Exp $
#
# Copyright (c) 2003 Alistair G. Crooks.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Alistair G. Crooks.
# 4. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

die()
{
	echo >&2 "$@"
	exit 1
}

check_prog()
{
	_var="$1"; _name="$2"

	eval _tmp=\"\$$_var\"
	if [ "x$_tmp" != "x" ]; then
		# Variable is already set (by the user, for example)
		return 0
	fi

	for _d in `echo $PATH | tr ':' ' '`; do
		if [ -x "$_d/$_name" ]; then
			# Program found
			eval $_var=\""$_d/$_name"\"
			return 1
		fi
	done

	die "$_name not found in path."
}

check_prog awkprog awk
check_prog echoprog echo
check_prog ftpprog ftp
check_prog idprog id
check_prog moreprog more
check_prog pkg_addprog pkg_add
check_prog rmprog rm
check_prog sedprog sed
check_prog suprog su
check_prog unameprog uname

# print version and exit
version() {
	$pkg_addprog -V
	exit 0
}

# temporary files
tmpcategories=/tmp/categories.$$
tmppackages=/tmp/packages.$$

# some base parameters
site=ftp.minix3.org
base=pub/minix/packages
release=`${unameprog} -r | ${sedprog} -e 's/_STABLE//'`
machine=`${unameprog} -p`

read_ftp_dir()
{
	ftp_base=$1
	ftp_dir=$2

${ftpprog} <<EOF
open $ftp_base
user
anonymous
cd $ftp_dir
ls
EOF
}

get_dir_entries()
{
	start='150 Here comes the directory listing.'
	end='226 Directory send OK.'
	sed -n "/$start/,/$end/{ /$start/! {/$end/!p;}; }"		
}

doit=""
sleepsecs=0

while [ $# -gt 0 ]; do
	case $1 in
	-V)	version ;;
	-b)	base=$2; shift ;;
	-h)	${echoprog} "$0 [-b BaseURL] [-h] [-m machine] [-n] [-r release] [-v] [-w secs]"; exit 0;;
	-m)	machine=$2; shift ;;
	-n)	doit=":" ;;
	-r)	release=$2; shift ;;
	-v)	set -x ;;
	-w)	sleepsecs=$2; shift ;;
	*)	break ;;
	esac
	shift
done

category=""

while true; do
	# if we don't have a packages file, then we need to choose a category
	case "$category" in
	"")	# get possible categories
		if [ ! -f $tmpcategories ]; then
			${echoprog} "Downloading package categories from ftp://${site}/${base}..."
			${echoprog} "** QUIT" > $tmpcategories
			read_ftp_dir $site $base/${release}/${machine} | get_dir_entries >> $tmpcategories
		fi

		# check for bad release numbering
		# - it usually shows with 0 categories being displayed
		${awkprog} 'END { if (NR == 1) { print "\n\n\n*** No categories found - is the OS release set properly? ***\n\n\n" } }' < $tmpcategories
	
		# display possible categories
		${awkprog} '{ print NR ". " $0 }' < $tmpcategories | ${moreprog}

		# read a category number from the user
		${echoprog} -n "Please type the category number: "
		read choice

		# validate user's choice
		case "$choice" in
		0|1)		${rmprog} -f $tmpcategories $tmppackages; exit 0 ;;
		[2-9]|[0-9]*)	category=`${awkprog} 'NR == '$choice' { print }' < $tmpcategories` ;;
		*)		category="" ;;
		esac
		case "$category" in
		"")	${echoprog} "No such category \"$choice\""
			sleep $sleepsecs
			continue
			;;
		esac

		# get possible packages
		${echoprog} ""
		${echoprog} "Downloading package names from ftp://${site}/${base}/${category}..."
		${echoprog} "** QUIT" > $tmppackages
		${echoprog} "** Change category" >> $tmppackages
		read_ftp_dir $site $base/${release}/${machine}/${category} | get_dir_entries >> $tmppackages
		;;
	esac

	# display possible packages
	${awkprog} '{ print NR ". " $0 }' < $tmppackages | ${moreprog}

	# read a package number from the user
	${echoprog} -n "Please type the package number: "
	read choice

	# validate user's choice
	case "$choice" in
	1)	${rmprog} -f $tmppackages $tmpcategories; exit 0 ;;
	2)	category=""; continue ;; # no package to install - choose new category
	[3-9]|[0-9]*)	package=`${awkprog} 'NR == '$choice' { print }' < $tmppackages` ;;
	*)	package="" ;;
	esac
	case "$package" in
	"")	${echoprog} "No such package \"$choice\""
		sleep $sleepsecs
		continue
		;;
	esac

	# check it's not already installed
	pkgbase=`${echoprog} ${package} | ${sedprog} -e 's|-[0-9].*||'`
	installed=`pkg_info -e $pkgbase`
	case "$installed" in
	"")	;;
	*)	${echoprog} "$package selected, but $installed already installed"
		sleep $sleepsecs
		continue
		;;
	esac

	# Tell people what we're doing
	${echoprog} ""
	${echoprog} "Adding package ftp://${site}/${base}/${release}/${machine}/${category}/${package}"

	cmd="${pkg_addprog} ftp://${site}/${base}/${release}/${machine}/All/${package}"

	# check if we need to become root for this
	if [ `${idprog} -u` != 0 ]; then
		${echoprog} "Becoming root@`/bin/hostname` to add a binary package"
		${echoprog} -n "`${echoprog} ${suprog} | $awkprog '{ print $1 }'` "
		$doit ${suprog} root -c "$cmd"
		success=$?
	else
		$doit $cmd
		success=$?
	fi

	# give feedback after adding the package
	case $success in
	0)	${echoprog} "$package successfully installed" ;;
	*)	${echoprog} "Problems when installing $package - please try again" ;;
	esac

	${echoprog} ""
	${echoprog} -n "[Q]uit, [C]hange category, [I]nstall another package: "
	read choice

	case "$choice" in
	[Qq])	break ;;
	[Cc])	category="" ;;
	[Ii])	;;
	esac
done

${rmprog} -f $tmpcategories $tmppackages

exit 0
