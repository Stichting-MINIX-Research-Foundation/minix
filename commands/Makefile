# Makefile for commands.

.include <bsd.own.mk>

SUBDIR=	add_route arp ash at awk \
	backup badblocks banner basename \
	btrace cal calendar \
	cat cawf cd  cdprobe checkhier cpp \
	chmod chown chroot ci cksum cleantmp clear cmp co \
	comm compress cp crc cron crontab cut \
	dd decomp16 DESCRIBE dev2name devsize df dhcpd \
	dhrystone diff dirname diskctl du dumpcore \
	ed eject elvis env expand factor fbdctl file \
	find finger fingerd fix fold format fortune fsck.mfs \
	ftp101 gcore gcov-pull getty grep head hexdump host \
	hostaddr id ifconfig ifdef install \
	intr ipcrm ipcs irdpd isoread join kill last leave \
	less lex loadkeys loadramdisk logger login look lp \
	lpd ls lspci mail make  MAKEDEV \
	mdb  mesg mined mkfifo mkfs.mfs mknod \
	mkproto modem mount mt netconf newroot nice acknm nohup \
	nonamed od paste patch pax \
	ping postinstall poweroff pr prep printf printroot \
	profile progressbar proto pr_routes ps pwd pwdauth \
	ramdisk rarpd rawspeed rcp rdate readall readclock \
	reboot remsync rev rget rlogin rlogind \
	rotate rsh rshd sed service setup shar acksize \
	sleep slip sort spell split srccrc \
	stty su sum svclog swifi sync synctree sysenv \
	syslogd tail tar tcpd tcpdp tcpstat tee telnet \
	telnetd term termcap tget time touch tr \
	truncate tsort tty udpstat umount uname unexpand \
	unstack update uud uue version vol wc \
	whereis which who write writeisofs fetch \
	xargs yacc yes zdump zmodem pkgin_cd \
	mktemp worldstone updateboot

.if ${ARCH} == "i386"
SUBDIR+=	atnormalize dosread fdisk loadfont \
	autopart part partition playwave  \
	recwave repartition screendump
.endif

.include <bsd.subdir.mk>
