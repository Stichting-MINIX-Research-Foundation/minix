# Makefile for commands.

.include <bsd.own.mk>

SUBDIR=	add_route arp ash at backup banner basename btrace cal \
	cawf cd cdprobe cpp \
	chmod chown ci cleantmp clear cmp co \
	comm compress cp crc cron crontab cut \
	dd decomp16 DESCRIBE dev2name devmand devsize df dhcpd \
	dhrystone diff dirname diskctl dumpcore \
	eject elvis env expand factor fbdctl \
	find finger fingerd fix fold format fortune fsck.mfs \
	ftp101 gcore gcov-pull getty grep head hexdump host \
	hostaddr id ifconfig ifdef \
	intr ipcrm ipcs irdpd isoread join kill last \
	less loadkeys loadramdisk logger look lp \
	lpd ls lspci mail MAKEDEV \
	mesg mined mkfifo mknod \
	mkproto mount mt netconf nice acknm nohup \
	nonamed od paste patch pax \
	ping postinstall poweroff pr prep printf printroot \
	profile progressbar pr_routes ps pwd pwdauth \
	ramdisk rarpd rawspeed rcp rdate readclock \
	reboot remsync rev rget rlogin \
	rotate rsh rshd service setup shar acksize \
	sleep slip sort spell split sprofalyze sprofdiff srccrc \
	stty sum svclog svrctl swifi sync synctree sysenv \
	syslogd tail tar tcpd tcpdp tcpstat tee telnet \
	telnetd term termcap tget time touch tr \
	truncate tsort tty udpstat umount uname unexpand \
	unstack update uud uue version vol wc \
	whereis which who write writeisofs fetch \
	xargs yes zdump zmodem pkgin_cd \
	worldstone updateboot update_bootcfg

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	atnormalize dosread fdisk loadfont \
	autopart part partition playwave  \
	recwave repartition screendump
.endif

.include <bsd.subdir.mk>
