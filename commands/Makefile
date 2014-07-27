# Makefile for commands.

.include <bsd.own.mk>

SUBDIR=	add_route arp ash at backup btrace \
	cawf cd cdprobe \
	ci cleantmp cmp co \
	compress cp crc cron crontab \
	dd decomp16 DESCRIBE devmand devsize dhcpd \
	dhrystone diff diskctl \
	eject fbdctl \
	find fix format fsck.mfs \
	gcov-pull grep host \
	hostaddr ifconfig ifdef \
	intr ipcrm ipcs irdpd isoread \
	loadkeys loadramdisk logger look lp \
	lpd lspci mail MAKEDEV \
	mined \
	mount mt netconf \
	nonamed \
	ping postinstall prep printroot \
	profile progressbar pr_routes ps pwdauth \
	ramdisk rarpd rawspeed rcp readclock \
	remsync rget rlogin \
	rotate rsh rshd service setup \
	slip spell sprofalyze sprofdiff srccrc \
	svclog svrctl swifi synctree sysenv \
	syslogd tcpd tcpdp tcpstat telnet \
	telnetd term termcap tget time \
	truncate udpstat umount \
	unstack update uud uue version vol \
	writeisofs fetch \
	zdump zmodem pkgin_cd pkgin_all \
	worldstone updateboot update_bootcfg \
	atnormalize dosread fdisk loadfont \
	autopart part partition playwave  \
	recwave repartition screendump \
	command expr getopts read test \
       	umask wait

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	eepromread
.endif

.include <bsd.subdir.mk>
