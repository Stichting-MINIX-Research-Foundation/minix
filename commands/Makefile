# Makefile for commands.

.include <minix.own.mk>

SUBDIR=	aal add_route adduser advent arp ash at autil awk \
	backup badblocks banner basename bigmake binpackage \
	binpackages binsizes bzip2 bzip2recover cal calendar \
	cat cawf cd cdiff cdprobe cgrep checkhier chmem \
	chmod chown chroot ci cksum cleantmp clear cmp co \
	comm compress cp crc cron crontab cut datasizes date \
	dd de decomp16 DESCRIBE dev2name devsize df dhcpd \
	dhrystone diff dirname dis88 du dumpcore easypack \
	ed eject elle elvis env expand factor fgrep file \
	find finger fingerd fix fold format fortune fsck \
	fsck1 ftp101 ftpd200 getty gomoku grep head host \
	hostaddr id ifconfig ifdef indent install \
	intr ipcrm ipcs irdpd isoread join kill last leave \
	lex life loadkeys loadramdisk logger login look lp \
	lpd ls lspci M m4 mail make MAKEDEV makewhatis man \
	mdb mesg mined mkdep mkdir mkdist mkfifo mkfs mknod \
	mkproto modem mount mt netconf newroot nice nm nohup \
	nonamed od packit packman passwd paste patch pax \
	ping postinstall poweroff pr prep printf printroot \
	profile progressbar proto pr_routes ps pwd pwdauth \
	ramdisk rarpd rawspeed rcp rdate readall readclock \
	readfs reboot remsync rev rget rlogin rlogind rmdir \
	rotate rsh rshd sed service setup shar size \
	sleep slip sort spell split srccrc stat strings strip \
	stty su sum svclog swapfs swifi sync synctree sysenv \
	syslogd tail talk talkd tcpd tcpdp tcpstat tee telnet \
	telnetd term termcap tget time tinyhalt top touch tr \
	truncate tsort ttt tty udpstat umount uname unexpand \
	uniq unstack update urlget uud uue version vol wc \
	whatis whereis which who whoami write writeisofs \
	xargs yacc yap yes zdump zic zmodem

.if ${ARCH} == "i386"
SUBDIR+=	atnormalize dosread fdisk loadfont \
	mixer autopart part partition playwave postmort \
	recwave repartition screendump
SUBDIR+=	acd asmconv gas2ack
.endif

.include <minix.subdir.mk>
