#!/bin/sh

# Generates various defines needed for using rump on non-NetBSD systems.
# Run this occasionally (yes, it's a slightly suboptimal kludge, but
# better than nothing).

echo Generating rumpdefs.h
rm -f rumpdefs.h
exec 3>&1 > rumpdefs.h

printf '/*	$NetBSD: makerumpdefs.sh,v 1.28 2015/09/15 14:55:12 pooka Exp $	*/\n\n'
printf '/*\n *\tAUTOMATICALLY GENERATED.  DO NOT EDIT.\n */\n\n'
printf '#ifndef _RUMP_RUMPDEFS_H_\n'
printf '#define _RUMP_RUMPDEFS_H_\n\n'
printf '#include <rump/rump_namei.h>\n'

fromvers () {
	echo
	sed -n '1{s/\$//gp;q;}' $1
}

# not perfect, but works well enough for the cases so far
# (also has one struct-specific hack for MAXNAMLEN)
getstruct () {
	sed -n '/struct[ 	]*'"$2"'[ 	]*{/{
		a\
struct rump_'"$2"' {
		:loop
		n
		s/^}.*;$/};/p
		t
		/^#/!{/MAXNAMLEN/!{s/ino_t/uint64_t/;p;}}
		b loop
	}' < $1
}

# likewise not perfect, but as long as it's KNF, we're peachy (though
# I personally like nectarines more)
getenum () {
	sed -n '/enum[ 	]*'"$2"'[ 	]*{/{
		a\
enum rump_'"$2"' {
		:loop
		n
		s/^}.*;$/};/p
		t
		s/'$3'/RUMP_&/gp
		b loop
	}' < $1
}


fromvers ../../../sys/fcntl.h
sed -n '/#define	O_[A-Z]*	*0x/s/O_/RUMP_O_/gp' \
    < ../../../sys/fcntl.h

fromvers ../../../sys/vnode.h
sed -n '/enum vtype.*{/{s/vtype/rump_&/;s/ V/ RUMP_V/gp;}' <../../../sys/vnode.h
sed -n '/#define.*LK_[A-Z]/s/LK_/RUMP_LK_/gp' <../../../sys/vnode.h	\
    | sed 's,/\*.*$,,'

fromvers ../../../sys/errno.h
sed -n '/#define[ 	]*E/s/\([ 	]\)\(E[A-Z2][A-Z]*\)/\1RUMP_\2/gp' \
    < ../../../sys/errno.h

fromvers ../../../sys/reboot.h
sed -n '/#define.*RB_[A-Z]/s/RB_/RUMP_RB_/gp' <../../../sys/reboot.h	\
    | sed 's,/\*.*$,,'
sed -n '/#define.*AB_[A-Z]/s/AB_/RUMP_AB_/gp' <../../../sys/reboot.h	\
    | sed 's,/\*.*$,,'

fromvers ../../../sys/socket.h
sed -n '/#define[ 	]*SOCK_[A-Z]/s/SOCK_/RUMP_SOCK_/gp' <../../../sys/socket.h \
    | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*[AP]F_[A-Z]/s/[AP]F_/RUMP_&/gp' <../../../sys/socket.h \
    | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*SO_[A-Z]/s/SO_/RUMP_&/gp' <../../../sys/socket.h \
    | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*SOL_[A-Z]/s/SOL_/RUMP_&/gp' <../../../sys/socket.h \
    | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*MSG_[A-Z]/s/MSG_/RUMP_&/gp' <../../../sys/socket.h \
    | sed 's,/\*.*$,,'

fromvers ../../../netinet/in.h
sed -n '/#define[ 	]*IP_[A-Z]/s/IP_/RUMP_&/gp' <../../../netinet/in.h \
    | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*IPPROTO_[A-Z]/s/IPPROTO_/RUMP_&/gp' <../../../netinet/in.h \
    | sed 's,/\*.*$,,'

fromvers ../../../netinet/tcp.h
sed -n '/#define[ 	]*TCP_[A-Z]/s/TCP_/RUMP_&/gp' <../../../netinet/tcp.h \
    | sed 's,/\*.*$,,'

fromvers ../../../sys/mount.h
sed -n '/#define[ 	]*MOUNT_[A-Z]/s/MOUNT_/RUMP_MOUNT_/gp' <../../../sys/mount.h | sed 's,/\*.*$,,'

fromvers ../../../sys/fstypes.h
sed -n '/#define[ 	]*MNT_[A-Z].*[^\]$/s/MNT_/RUMP_MNT_/gp' <../../../sys/fstypes.h | sed 's,/\*.*$,,'

fromvers ../../../sys/ioccom.h
sed -n '/#define[ 	]*IOC[A-Z_].*[^\\]$/s/IOC/RUMP_&/gp' <../../../sys/ioccom.h | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*_IO.*\\$/{:t;N;/\\$/bt;s/_IOC/_RUMP_IOC/g;s/IOC[A-Z]/RUMP_&/gp}' <../../../sys/ioccom.h \
    | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*_IO.*[^\]$/{s/_IO/_RUMP_IO/g;s/IOC_/RUMP_IOC_/gp}' <../../../sys/ioccom.h \
    | sed 's,/\*.*$,,'

fromvers ../../../sys/ktrace.h
sed -n '/#define[ 	]*KTROP_[A-Z_]/s/KTROP_/RUMP_&/gp' <../../../sys/ktrace.h | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*KTR_[A-Z_]/s/KTR_/RUMP_&/gp' <../../../sys/ktrace.h | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*KTRFAC_[A-Z_]/{s/KTRFAC_/RUMP_&/g;s/KTR_/RUMP_&/g;p;}' <../../../sys/ktrace.h | sed 's,/\*.*$,,'
sed -n '/#define[ 	]*KTRFACv[0-9]/{s/KTRFACv/RUMP_&/g;s/KTRFAC_/RUMP_&/g;p;}' <../../../sys/ktrace.h | sed 's,/\*.*$,,'

fromvers ../../../sys/module.h
getstruct ../../../sys/module.h modctl_load
getenum ../../../sys/module.h modctl MODCTL

fromvers ../../../ufs/ufs/ufsmount.h
getstruct ../../../ufs/ufs/ufsmount.h ufs_args

fromvers ../../../fs/sysvbfs/sysvbfs_args.h
getstruct ../../../fs/sysvbfs/sysvbfs_args.h sysvbfs_args

fromvers ../../../sys/dirent.h
getstruct ../../../sys/dirent.h dirent

printf '\n#endif /* _RUMP_RUMPDEFS_H_ */\n'

exec 1>&3

echo Generating rumperr.h
rm -f rumperr.h
exec > rumperr.h
printf '/*	$NetBSD: makerumpdefs.sh,v 1.28 2015/09/15 14:55:12 pooka Exp $	*/\n\n'
printf '/*\n *\tAUTOMATICALLY GENERATED.  DO NOT EDIT.\n */\n'

fromvers ../../../sys/errno.h

printf "\nstatic inline const char *\nrump_strerror(int error)\n{\n\n"
printf "\tswitch (error) {\n\tcase 0:\n"
printf "\t\t return \"No error: zero, zip, zilch, none!\";\n"
awk '/^#define[ 	]*E.*[0-9]/{
	ename = $2
	evalue = $3
	error = 1
	if (ename == "ELAST") {
		printf "\tdefault:\n"
		printf "\t\treturn \"Invalid error!\";\n\t}\n}\n"
		error = 0
		exit 0
	}
	if (preverror + 1 != evalue)
		exit 1
	preverror = evalue
	printf "\tcase %d: /* (%s) */\n\t\treturn \"", evalue, ename
	sp = ""
	for (i = 5; i < NF; i++) {
		printf "%s%s", sp, $i
		sp = " "
	}
	printf "\";\n"
}
END {
	exit error
}' < ../../../sys/errno.h
if [ $? -ne 0 ]; then
	echo 'Parsing errno.h failed!' 1>&3
	rm -f rumpdefs.h rumperr.h
	exit 1
fi

echo Generating rumperrno2host.h 1>&3
rm -f rumperrno2host.h
exec > rumperrno2host.h
printf '/*	$NetBSD: makerumpdefs.sh,v 1.28 2015/09/15 14:55:12 pooka Exp $	*/\n\n'
printf '/*\n *\tAUTOMATICALLY GENERATED.  DO NOT EDIT.\n */\n'

fromvers ../../../sys/errno.h

printf "\n#ifndef ERANGE\n#error include ISO C style errno.h first\n#endif\n"
printf "\nstatic inline int \nrump_errno2host(int rumperrno)\n{\n\n"
printf "\tswitch (rumperrno) {\n\tcase 0:\n"
printf "\t\t return 0;\n"
awk '/^#define[ 	]*E.*[0-9]/{
	ename = $2
	evalue = $3
	error = 1
	if (ename == "ELAST") {
		printf "\tdefault:\n"
		printf "#ifdef EINVAL\n\t\treturn EINVAL;\n"
		printf "#else\n\t\treturn ERANGE;\n#endif\n"
		printf "\t}\n}\n"
		error = 0
		exit 0
	}
	if (preverror + 1 != evalue)
		exit 1
	preverror = evalue
	printf "#ifdef %s\n", ename
	printf "\tcase %d:\n\t\treturn %s;\n", evalue, ename
	printf "#endif\n"
}
END {
	exit error
}' < ../../../sys/errno.h
if [ $? -ne 0 ]; then
	echo 'Parsing errno.h failed!' 1>&3
	rm -f rumpdefs.h rumperr.h rumperrno2host.h
	exit 1
fi

exit 0
