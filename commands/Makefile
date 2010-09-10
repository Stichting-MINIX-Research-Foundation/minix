# Makefile for commands.

.include <bsd.own.mk>

SUBDIR=	aal add_route adduser advent arp ash at autil awk \
	backup badblocks banner basename bigmake binpackage \
	binpackages binsizes bzip2 bzip2recover cal calendar \
	cat cawf cd  cdprobe checkhier chmem \
	chmod chown chroot ci cksum cleantmp clear cmp co \
	comm compress cp crc cron crontab cut date \
	dd de decomp16 DESCRIBE dev2name devsize df dhcpd \
	dhrystone diff dirname dis88 diskctl du dumpcore \
	ed eject elle elvis env expand factor file \
	find finger fingerd fix fold format fortune fsck.mfs \
	fsck1 ftp101 ftpd200 gcov-pull getty grep gomoku head hexdump host \
	hostaddr id ifconfig ifdef indent install \
	intr ipcrm ipcs irdpd isoread join kill last leave \
	less lex life loadkeys loadramdisk logger login look lp \
	lpd ls lspci M m4 mail make  MAKEDEV man \
	mdb mdocml mesg mined mkdep mkdir mkdist mkfifo mkfs.mfs mknod \
	mkproto modem mount mt netconf newroot nice acknm nohup \
	nonamed od packman passwd paste patch pax \
	ping postinstall poweroff pr prep printf printroot \
	profile progressbar proto pr_routes ps pwd pwdauth \
	ramdisk rarpd rawspeed rcp rdate readall readclock \
	readfs reboot remsync rev rget rlogin rlogind rmdir \
	rotate rsh rshd sed service setup shar size \
	sleep slip sort spell split srccrc stat strings ackstrip \
	stty su sum svclog swapfs swifi sync synctree sysenv \
	syslogd tail talk talkd tar tcpd tcpdp tcpstat tee telnet \
	telnetd term termcap tget time tinyhalt top touch tr \
	truncate tsort ttt tty udpstat umount uname unexpand \
	uniq unstack update urlget uud uue version vol wc \
	whereis which who write writeisofs fetch \
	xargs yacc yes zdump zic zmodem pkg_install pkgin_cd

.if ${ARCH} == "i386"
SUBDIR+=	atnormalize dosread fdisk loadfont \
	mixer autopart part partition playwave postmort \
	recwave repartition screendump padtext
SUBDIR+=	acd asmconv gas2ack
.endif

.include <bsd.subdir.mk>
