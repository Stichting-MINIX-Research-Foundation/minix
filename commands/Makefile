# Makefile for commands.

.include <bsd.own.mk>

SUBDIR=	add_route arp ash at backup basename btrace \
	cawf cd cdprobe cpp \
	chmod chown ci cleantmp cmp co \
	comm compress cp crc cron crontab cut \
	dd decomp16 DESCRIBE devmand devsize df dhcpd \
	dhrystone diff dirname diskctl dumpcore \
	eject env expand factor fbdctl \
	find fix fold format fortune fsck.mfs \
	gcore gcov-pull getty grep hexdump host \
	hostaddr id ifconfig ifdef \
	intr ipcrm ipcs irdpd isoread last \
	less loadkeys loadramdisk logger look lp \
	lpd lspci mail MAKEDEV \
	mesg mined mkfifo \
	mount mt netconf nice acknm nohup \
	nonamed od paste patch \
	ping postinstall poweroff pr prep printroot \
	profile progressbar pr_routes ps pwdauth \
	ramdisk rarpd rawspeed rcp readclock \
	reboot remsync rev rget rlogin \
	rotate rsh rshd service setup shar acksize \
	sleep slip spell split sprofalyze sprofdiff srccrc \
	stty svclog svrctl swifi synctree sysenv \
	syslogd tail tcpd tcpdp tcpstat tee telnet \
	telnetd term termcap tget time touch tr \
	truncate tty udpstat umount uname unexpand \
	unstack update uud uue version vol wc \
	whereis which write writeisofs fetch \
	xargs zdump zmodem pkgin_cd pkgin_all \
	worldstone updateboot update_bootcfg

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	atnormalize dosread fdisk loadfont \
	autopart part partition playwave  \
	recwave repartition screendump
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	eepromread
.endif

.include <bsd.subdir.mk>
