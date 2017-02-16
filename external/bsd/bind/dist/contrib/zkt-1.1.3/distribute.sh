#################################################################
#
#	@(#) distribute.sh -- distribute and reload command for dnssec-signer
#
#	(c) Jul 2008 Holger Zuleger  hznet.de
#
#	Feb 2010	action "distkeys" added but currently not used
#
#	This shell script will be run by zkt-signer as a distribution
#	and reload command if:
#
#		a) the dnssec.conf file parameter Distribute_Cmd: points
#		   to this file
#	and
#		b) the user running the zkt-signer command is not
#		   root (uid==0)
#	and
#		c) the owner of this shell script is the same as the
#		   running user and the access rights don't allow writing
#		   for anyone except the owner
#	or
#		d) the group of this shell script is the same as the
#		   running user and the access rights don't allow writing
#		   for anyone except the group
#
#################################################################

# set path to rndc and scp
PATH="/bin:/usr/bin:/usr/local/sbin"

# remote server and directory
server=localhost	# fqdn of remote name server
dir=/var/named		# zone directory on remote name server

progname=$0
usage()
{
	echo "usage: $progname distkeys|distribute|reload <zone> <path_to_zonefile> [<viewname>]" 1>&2
	test $# -gt 0 && echo $* 1>&2
	exit 1
}

if test $# -lt 3
then
	usage
fi
action="$1"
zone="$2"
zonefile="$3"
view=""
test $# -gt 3 && view="$4"

case $action in
distkeys)
	if test -n "$view"
	then
		: echo "scp K$zone+* $server:$dir/$view/$zone/"
		scp K$zone+* $server:$dir/$view/$zone/
	else
		: echo "scp K$zone+* $server:$dir/$zone/"
		scp K$zone+* $server:$dir/$zone/
	fi
	;;
distribute)
	if test -n "$view"
	then
		: echo "scp $zonefile $server:$dir/$view/$zone/"
		scp $zonefile $server:$dir/$view/$zone/
	else
		: echo "scp $zonefile $server:$dir/$zone/"
		scp $zonefile $server:$dir/$zone/
	fi
	;;
reload)
	: echo "rndc $action $zone $view"
	rndc $action $zone $view
	;;
*)
	usage "illegal action $action"
	;;
esac

