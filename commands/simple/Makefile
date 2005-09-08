# Makefile for commands/simple.

CFLAGS	= -D_MINIX -D_POSIX_SOURCE
SYS	= ../..
CCLD	= $(CC) -i $(CFLAGS)
CC	= exec cc

# This Makefile is large, but that is because it lists all actions that must
# be taken to compile and install all the simple commands.  If there were only
# one command then it would look like this:
#
# ALL	= \
#	cat \			need the 'cat' executable
#
# all:	$(ALL)			default rule, make all binaries
#
# cat:	cat.c			'cat' is made from 'cat.c'
#	$(CCLD) -o $@ $?	compile 'cat.c' ($?) to 'cat' ($@)
#	install -S 4kw $@	stack size is 8k (8086) or 16k (others)
#
# install:	\		rule to install all binaries
#	/usr/bin/cat \		one can find 'cat' in /usr/bin
#	/bin/cat \		important binaries are also in /bin
#
# /usr/bin/cat:	cat
#	install -cs -o bin $? $@	copy 'cat' to '/usr/bin/cat' (-c),
#					strip symbol table (-s)
#
# /bin/cat:	/usr/bin/cat
#	install -lcs $? $@	install '/bin/cat' by linking (if possible)
#				or copying (otherwise)
#
# Some of the binaries are installed under more than one name.  The extra
# names are indented by one extra tab in the install rule.
# If you want to add a command then insert it at the appropriate position
# in sorted order.  Search around for the command just above or below the
# the new command and add new rules for the new command near those places.
# Observe four key things:
#	What to make, how to make, what to install, how to install.

ALL	= \
	add_route \
	at \
	backup \
	badblocks \
	banner \
	basename \
	cal \
	calendar \
	cat \
	cdiff \
	cgrep \
	chmem \
	chmod \
	chown \
	chroot \
	ci \
	cksum \
	cleantmp \
	cmp \
	co \
	comm \
	compress \
	cp \
	crc \
	cut \
	date \
	dd \
	decomp16 \
	dev2name \
	devsize \
	df \
	dhrystone \
	diff \
	dirname \
	du \
	ed \
	eject \
	env \
	expand \
	factor \
	fgrep \
	file \
	find \
	finger \
	fix \
	fold \
	fortune \
	fsck \
	fsck1 \
	getty \
	gomoku \
	grep \
	head \
	host \
	hostaddr \
	id \
	ifconfig \
	ifdef \
	in.fingerd \
	in.rshd \
	installx \
	intr \
	irdpd \
	isoread \
	join \
	kill \
	last \
	leave \
	life \
	login \
	look \
	lp \
	lpd \
	ls \
	mail \
	man \
	mesg \
	mkdir \
	mkfifo \
	mkfs \
	mknod \
	mkproto \
	mkswap \
	modem \
	mount \
	mt \
	nm \
	nonamed \
	nice \
	od \
	passwd \
	paste \
	ping \
	pr \
	pr_routes \
	progressbar \
	prep \
	printf \
	printroot \
	printenv \
	proto \
	pwd \
	pwdauth \
	rarpd \
	rcp \
	rdate \
	readall \
	rev \
	readfs \
	remsync \
	rget \
	rlogin \
	rmdir \
	rsh \
	sed \
	shar \
	size \
	sleep \
	slip \
	sort \
	split \
	stat \
	strings \
	strip \
	stty \
	su \
	sum \
	swapfs \
	sync \
	synctree \
	sysenv \
	tail \
	tar \
	tcpd \
	tcpdp \
	tee \
	term \
	termcap \
	tget \
	time \
	touch \
	tr \
	treecmp \
	tsort \
	ttt \
	tty \
	umount \
	uname \
	unexpand \
	uniq \
	update \
	uud \
	uue \
	usyslogd \
	vol \
	wc \
	which \
	who \
	whoami \
	write \
	writeisofs \
	xargs \
	yes \
	#

all:	$(ALL)

add_route:	add_route.c
	$(CCLD) -o $@ add_route.c
	@install -S 4kw $@

at:	at.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

backup:	backup.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

badblocks:	badblocks.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

banner:	banner.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

basename:	basename.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

cal:	cal.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

calendar:	calendar.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

cat:	cat.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

cdiff:	cdiff.c
	$(CCLD) -o $@ $?
	@install -S 28kw $@

cgrep:	cgrep.c
	$(CCLD) -o $@ $?
	@install -S 5kw $@

chmem:	chmem.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

chmod:	chmod.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

chown:	chown.c
	$(CCLD) -o $@ $?
	@install -S 16kw $@

chroot:	chroot.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

ci:	ci.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

cksum:	cksum.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

cleantmp:	cleantmp.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

cmp:	cmp.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

co:	co.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

comm:	comm.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

compress:	compress.c
	$(CCLD) -o $@ $?
	@install -S 450k $@

cp:    cp.c
	$(CCLD) -o $@ $?
	@install -S 16kw $@

crc:	crc.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

cut:	cut.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

date:	date.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

dd:	dd.c
	$(CCLD) -o $@ $?
	@install -S 20kw $@

decomp16:	decomp16.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

dev2name:	dev2name.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

devsize:	devsize.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

df:	df.c
	$(CCLD) -I$(SYS) -o $@ $?
	@install -S 4kw $@

dhrystone:	dhrystone.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

diff:	diff.c
	$(CCLD) -o $@ $?
	@install -S 40kw $@

dirname:	dirname.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

du:	du.c
	$(CCLD) -o $@ $?
	@install -S 16kw $@

ed:	ed.c
	$(CCLD) -o $@ $?
	@install -S 32kw $@

eject:	eject.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

env:	env.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

expand:	expand.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

factor:	factor.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

fgrep:	fgrep.c
	$(CCLD) -o $@ $?
	@install -S 10kw $@

file:	file.c
	$(CCLD) -o $@ $?
	@install -S 25kw $@

find:	find.c
	$(CCLD) -o $@ $?
	@install -S 25kw $@

finger:	finger.c
	$(CCLD) -o $@ finger.c
	@install -S 8kw $@

fix:	fix.c
	$(CCLD) -o $@ fix.c
	@install -S 32kw $@

fold:	fold.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

fortune:	fortune.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

fsck:	fsck.c
	$(CCLD) -o $@ $?
	@install -S 4096k $@

fsck1:	fsck1.c
	$(CCLD) -o $@ $?
	@install -S 32kw $@

getty:	getty.c /usr/include/minix/config.h
	$(CCLD) -o $@ getty.c
	@install -S 4kw $@

gomoku:	gomoku.c
	$(CCLD) -o $@ $? -lcurses
	@install -S 8kw $@

grep:	grep.c
	$(CCLD) -o $@ $?
	@install -S 32kw $@

head:	head.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

host:	host.c
	$(CCLD) -wo -o $@ host.c
	@install -S 4kw $@

hostaddr:	hostaddr.c
	$(CCLD) -o $@ hostaddr.c
	@install -S 8kw $@

id:	id.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

ifconfig:	ifconfig.c
	$(CCLD) -o $@ ifconfig.c
	@install -S 4kw $@

ifdef:	ifdef.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

in.fingerd:	in.fingerd.c
	$(CCLD) -o $@ in.fingerd.c
	@install -S 4kw $@

in.rshd:	in.rshd.c
	$(CCLD) -o $@ in.rshd.c
	@install -S 4kw $@

installx:	install.c	# Note: avoided confict with 'install' rule.
	$(CCLD) -o $@ $?
	@install -S 4kw $@

intr:	intr.c
	$(CCLD) -o $@ intr.c
	@install -S 4kw $@

irdpd:	irdpd.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

isoread:	isoread.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

join:	join.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

kill:	kill.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

last:	last.c
	$(CCLD) -o $@ $?
	@install -S 5kw $@

leave:	leave.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

life:	life.c
	$(CCLD) -o $@ $? -lcurses
	@install -S 15kw $@

login:	login.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

look:	look.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

lp:	lp.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

lpd:	lpd.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

ls:	ls.c
	$(CCLD) -o $@ $?
	@install -S 20kw $@

mail:	mail.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

man:	man.c
	$(CCLD) -o $@ $?
	@install -S 10kw $@

mesg:	mesg.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

mkdir:	mkdir.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

mkfifo:	mkfifo.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

mkfs:	mkfs.c
	$(CCLD) -o $@ $?
	@install -S 20kw $@

mknod:	mknod.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

mkproto:	mkproto.c
	$(CCLD) -o $@ $?
	@install -S 20kw $@

mkswap:	mkswap.c
	$(CCLD) -I$(SYS) -o $@ $?
	@install -S 4kw $@

modem:	modem.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

mount:	mount.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

mt:	mt.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

nm:	nm.c
	$(CCLD) -o $@ $?
	@install -S 32kw $@

nice:	nice.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

nonamed:	nonamed.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

od:	od.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

passwd:	passwd.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

paste:	paste.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

ping:	ping.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

pr:	pr.c
	$(CCLD) -o $@ $?
	@install -S 16kw $@

pr_routes:	pr_routes.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

progressbar:	progressbar.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

prep:	prep.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

printf:	printf.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

printenv:	printenv.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

printroot:	printroot.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

proto:	proto.c
	$(CCLD) -o $@ $?
	@install -S 15kw $@

pwd:	pwd.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

pwdauth:	pwdauth.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

rarpd:	rarpd.c
	$(CCLD) -o $@ rarpd.c
	@install -S 4kw $@

rcp:	rcp.c
	$(CCLD) -o $@ rcp.c
	@install -S 8kw $@

rdate:	rdate.c
	$(CCLD) -o $@ rdate.c
	@install -S 8kw $@

rev:	rev.c
	$(CCLD) -o $@ rev.c
	@install -S 8kw $@

readall:	readall.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

readfs:	readfs.c
	$(CCLD) -o $@ $?
	@install -S 25kw $@

remsync:	remsync.c
	$(CCLD) -o $@ $?
	@install -S 256k $@

rget:	rget.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

rlogin:	rlogin.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

rmdir:	rmdir.c
	$(CCLD) -o $@ $?
	@install -S 15kw $@

rsh:	rsh.c
	$(CCLD) -o $@ rsh.c
	@install -S 8kw $@

sed:	sed.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

shar:	shar.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

size:	size.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

sleep:	sleep.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

slip:	slip.c
	$(CCLD) -o $@ $?
	@install -S 20k $@

sort:	sort.c
	$(CCLD) -o $@ $?
	@install -S 30kw $@

split:	split.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

stat:	stat.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

strings:	strings.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

strip:	strip.c
	$(CCLD) -o $@ $?
	@install -S 8kw $@

stty:	stty.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

su:	su.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

sum:	sum.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

swapfs:	swapfs.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

sync:	sync.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

synctree:	synctree.c
	$(CCLD) -o $@ -wo $?
	install -S 64kw $@

sysenv:	sysenv.c
	$(CCLD) -o $@ -wo $?
	@install -S 4kw $@

tail:	tail.c
	$(CCLD) -o $@ $?
	@install -S 16kw $@

tar:	tar.c
	$(CCLD) -o $@ $?
	@install -S 512kw $@

tcpd:	tcpd.c
	$(CCLD) -o $@ -DPARANOID=0 tcpd.c
	@install -S 4kw $@

tcpdp:	tcpd.c
	$(CCLD) -o $@ -DPARANOID=1 tcpd.c
	@install -S 8kw $@

tee:	tee.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

term:	term.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

termcap:	termcap.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

tget:	tget.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

time:	time.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

touch:	touch.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

tr:	tr.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

tsort:	tsort.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

treecmp:	treecmp.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

ttt:	ttt.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

tty:	tty.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

umount:	umount.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

uname:	uname.c /usr/include/minix/config.h
	$(CCLD) -o $@ uname.c
	@install -S 4kw $@

unexpand:	unexpand.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

uniq:	uniq.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

update:	update.c
	$(CCLD) -o $@ $?
	@install -S 1kw $@

uud:	uud.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

usyslogd:	usyslogd.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

uue:	uue.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

vol:	vol.c
	$(CCLD) -o $@ $?
	@install -S 80k $@	# note: '-S' is upper limit to 'vol -m'

wc:	wc.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

which:	which.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

who:	who.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

whoami:	whoami.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

write:	write.c
	$(CCLD) -o $@ $?

writeisofs:	writeisofs.c
	$(CCLD) -o $@ $?

xargs:	xargs.c
	$(CCLD) -o $@ $?
	@install -S 16kw $@

yes:	yes.c
	$(CCLD) -o $@ $?
	@install -S 4kw $@

install:	\
	/usr/bin/add_route \
		/usr/bin/del_route \
	/usr/bin/at \
	/usr/bin/backup \
	/usr/bin/restore \
	/usr/bin/rev \
	/usr/bin/badblocks \
	/usr/bin/banner \
	/usr/bin/basename \
	/usr/bin/cal \
	/usr/bin/calendar \
	/usr/bin/cat \
	/usr/bin/cdiff \
	/usr/bin/cgrep \
	/usr/bin/chmem \
	/usr/bin/chmod \
	/usr/bin/chown \
	/usr/bin/chroot \
		/usr/bin/chgrp \
	/usr/bin/ci \
	/usr/bin/cksum \
	/usr/bin/cleantmp \
	/usr/bin/cmp \
	/usr/bin/co \
	/usr/bin/comm \
	/usr/bin/compress \
		/usr/bin/uncompress \
		/usr/bin/zcat \
	/bin/cp \
	/bin/rm \
	/bin/mv \
	/bin/ln \
	/usr/bin/cp \
		/usr/bin/clone \
		/usr/bin/cpdir \
		/usr/bin/ln \
		/usr/bin/mv \
		/usr/bin/rm \
		/bin/rm \
	/usr/bin/crc \
	/usr/bin/cut \
	/usr/bin/date \
	/usr/bin/dd \
	/usr/bin/decomp16 \
	/bin/dev2name \
	/usr/bin/devsize \
	/usr/bin/df \
	/usr/bin/dhrystone \
	/usr/bin/diff \
	/usr/bin/dirname \
	/usr/bin/du \
	/usr/bin/ed \
	/usr/bin/eject \
	/usr/bin/env \
	/usr/bin/expand \
	/usr/bin/factor \
	/usr/bin/fgrep \
	/usr/bin/file \
	/usr/bin/find \
	/usr/bin/finger \
	/usr/bin/fix \
	/usr/bin/fold \
	/usr/bin/fortune \
	/usr/bin/fsck \
	/usr/bin/fsck1 \
	/bin/getty \
	/usr/bin/getty \
	/usr/bin/gomoku \
	/usr/bin/grep \
		/usr/bin/egrep \
	/usr/bin/head \
	/usr/bin/host \
	/usr/bin/hostaddr \
	/usr/bin/id \
	/usr/bin/ifconfig \
	/usr/bin/ifdef \
	/usr/bin/in.fingerd \
	/usr/bin/in.rshd \
	/bin/install \
	/usr/bin/install \
	/usr/bin/intr \
	/usr/bin/irdpd \
	/usr/bin/isoread \
		/usr/bin/isodir \
		/usr/bin/isoinfo \
	/usr/bin/join \
	/usr/bin/kill \
	/usr/bin/last \
		/usr/bin/uptime \
	/usr/bin/leave \
	/usr/bin/life \
	/usr/bin/login \
	/usr/bin/look \
	/usr/bin/lp \
	/usr/bin/lpd \
	/usr/bin/ls \
	/usr/bin/mail \
	/usr/bin/man \
	/usr/bin/mesg \
	/usr/bin/mkdir \
	/usr/bin/mkfifo \
	/usr/bin/mkfs \
	/usr/bin/mknod \
	/usr/bin/mkproto \
	/usr/bin/mkswap \
	/usr/bin/modem \
	/usr/bin/mount \
	/usr/bin/mt \
	/usr/bin/nm \
	/usr/bin/nice \
	/usr/bin/nonamed \
	/usr/bin/od \
	/usr/bin/passwd \
		/usr/bin/chfn \
		/usr/bin/chsh \
	/usr/bin/paste \
	/usr/bin/ping \
	/usr/bin/pr \
	/usr/bin/pr_routes \
	/usr/bin/progressbar \
	/usr/bin/prep \
	/usr/bin/printf \
	/usr/bin/printenv \
	/usr/bin/printroot \
	/usr/bin/proto \
	/usr/bin/pwd \
	/usr/lib/pwdauth \
	/usr/bin/rarpd \
	/usr/bin/rcp \
	/usr/bin/rdate \
	/usr/bin/readall \
	/usr/bin/readfs \
	/usr/bin/remsync \
	/usr/bin/rget \
		/usr/bin/rput \
	/usr/bin/rlogin \
	/usr/bin/rmdir \
	/usr/bin/rsh \
	/usr/bin/sed \
	/bin/sed \
	/usr/bin/shar \
	/usr/bin/size \
	/usr/bin/sleep \
	/usr/bin/slip \
	/usr/bin/sort \
	/usr/bin/split \
	/usr/bin/stat \
		/usr/bin/fstat \
	/usr/bin/strings \
	/usr/bin/strip \
	/usr/bin/stty \
	/usr/bin/su \
	/usr/bin/sum \
	/usr/bin/swapfs \
	/usr/bin/sync \
	/usr/bin/synctree \
	/usr/bin/sysenv \
	/bin/sysenv \
	/usr/bin/tail \
	/usr/bin/tar \
	/usr/bin/tcpd \
	/usr/bin/tcpdp \
	/usr/bin/tee \
	/usr/bin/term \
	/usr/bin/termcap \
	/usr/bin/tget \
	/usr/bin/time \
	/usr/bin/touch \
	/usr/bin/tr \
	/usr/bin/treecmp \
	/usr/bin/tsort \
	/usr/bin/ttt \
	/usr/bin/tty \
	/usr/bin/umount \
	/usr/bin/uname \
		/usr/bin/arch \
	/usr/bin/unexpand \
	/usr/bin/uniq \
	/usr/bin/update \
	/usr/bin/uud \
		/usr/bin/uudecode \
	/usr/bin/uue \
		/usr/bin/uuencode \
	/usr/bin/usyslogd \
	/usr/bin/vol \
	/usr/bin/wc \
	/usr/bin/which \
	/usr/bin/who \
	/usr/bin/whoami \
	/usr/bin/write \
	/usr/bin/writeisofs \
	/usr/bin/xargs \
	/usr/bin/yes \
	/bin/cat \
	/bin/date \
	/bin/fsck \
	/bin/intr \
	/bin/mount \
	/bin/printroot \
	/bin/pwd \
	/bin/sync \
	/bin/umount \
	#

/usr/bin/add_route:	add_route
	install -cs -o bin $? $@

/usr/bin/del_route:	/usr/bin/add_route
	install -l $? $@

/usr/bin/at:	at
	install -cs -o root -m 4755 $? $@

/usr/bin/backup:	backup
	install -cs -o bin $? $@

/usr/bin/restore:	/usr/bin/backup
	install -l $? $@

/usr/bin/badblocks:	badblocks
	install -cs -o bin $? $@

/usr/bin/banner:	banner
	install -cs -o bin $? $@

/usr/bin/basename:	basename
	install -cs -o bin $? $@

/usr/bin/cal:	cal
	install -cs -o bin $? $@

/usr/bin/calendar:	calendar
	install -cs -o bin $? $@

/usr/bin/cat:	cat
	install -cs -o bin $? $@

/usr/bin/cdiff:	cdiff
	install -cs -o bin $? $@

/usr/bin/cgrep:	cgrep
	install -cs -o bin $? $@

/usr/bin/chmem:	chmem
	install -cs -o bin $? $@

/usr/bin/chmod:	chmod
	install -cs -o bin $? $@

/usr/bin/chown:	chown
	install -cs -o bin $? $@

/usr/bin/chroot:	chroot
	install -cs -o bin $? $@

/usr/bin/chgrp:	/usr/bin/chown
	install -l $? $@

/usr/bin/ci:	ci
	install -cs -o bin $? $@

/usr/bin/cksum:	cksum
	install -cs -o bin $? $@

/usr/bin/cleantmp:	cleantmp
	install -cs -o bin $? $@

/usr/bin/cmp:	cmp
	install -cs -o bin $? $@

/usr/bin/co:	co
	install -cs -o bin $? $@

/usr/bin/comm:	comm
	install -cs -o bin $? $@

/usr/bin/compress:	compress
	install -cs -o bin $? $@

/usr/bin/uncompress /usr/bin/zcat:	/usr/bin/compress
	install -l $? $@

/bin/cp:	cp
	install -cs -o bin $? $@

/usr/bin/cp:	cp
	install -cs -o bin $? $@

/usr/bin/clone /usr/bin/cpdir \
/usr/bin/ln /usr/bin/mv /usr/bin/rm:	/usr/bin/cp
	install -l $? $@

/bin/ln /bin/mv /bin/rm:	/bin/cp
	install -l $? $@

/usr/bin/crc:	crc
	install -cs -o bin $? $@

/usr/bin/cut:	cut
	install -cs -o bin $? $@

/usr/bin/date:	date
	install -cs -o bin $? $@

/usr/bin/dd:	dd
	install -cs -o bin $? $@

/bin/dev2name:	dev2name
	install -cs -o bin $? $@

/usr/bin/devsize:	devsize
	install -cs -o bin $? $@

/usr/bin/decomp16:	decomp16
	install -cs -o bin $? $@

/usr/bin/df:	df
	install -cs -o root -m 4755 $? $@

/usr/bin/dhrystone:	dhrystone
	install -cs -o bin $? $@

/usr/bin/diff:	diff
	install -cs -o bin $? $@

/usr/bin/dirname:	dirname
	install -cs -o bin $? $@

/usr/bin/du:	du
	install -cs -o bin $? $@

/usr/bin/ed:	ed
	install -cs -o bin $? $@

/usr/bin/eject:	eject
	install -cs -o bin $? $@

/usr/bin/env:	env
	install -cs -o bin $? $@

/usr/bin/expand:	expand
	install -cs -o bin $? $@

/usr/bin/factor:	factor
	install -cs -o bin $? $@

/usr/bin/fgrep:	fgrep
	install -cs -o bin $? $@

/usr/bin/file:	file
	install -cs -o bin $? $@

/usr/bin/find:	find
	install -cs -o bin $? $@

/usr/bin/finger:	finger
	install -cs -o bin $? $@

/usr/bin/fix:	fix
	install -cs -o bin $? $@

/usr/bin/fold:	fold
	install -cs -o bin $? $@

/usr/bin/fortune:	fortune
	install -cs -o bin $? $@

/usr/bin/fsck:	fsck
	install -cs -o bin $? $@

/usr/bin/fsck1:	fsck1
	install -cs -o bin $? $@

/bin/getty:	getty
	install -cs -o bin $? $@

/usr/bin/getty:	getty
	install -cs -o bin $? $@

/usr/bin/gomoku:	gomoku
	install -cs -o bin $? $@

/usr/bin/grep:	grep
	install -cs -o bin $? $@

/usr/bin/egrep:	/usr/bin/grep
	install -l $? $@

/usr/bin/head:	head
	install -cs -o bin $? $@

/usr/bin/host:	host
	install -cs -o bin $? $@

/usr/bin/hostaddr:	hostaddr
	install -cs -o root -m 4755 $? $@

/usr/bin/id:	id
	install -cs -o bin $? $@

/usr/bin/ifconfig:	ifconfig
	install -cs -o root -m 4755 $? $@

/usr/bin/ifdef:	ifdef
	install -cs -o bin $? $@

/usr/bin/in.fingerd:	in.fingerd
	install -cs -o bin $? $@

/usr/bin/in.rshd:	in.rshd
	install -cs -o bin $? $@

/bin/install:	installx
	install -cs -o root -m 4755 $? $@

/usr/bin/install:	installx
	install -cs -o root -m 4755 $? $@

/usr/bin/intr:	intr
	install -cs -o bin $? $@

/usr/bin/irdpd:	irdpd
	install -cs -o bin $? $@

/usr/bin/isoread:	isoread
	install -cs -o bin $? $@

/usr/bin/isodir /usr/bin/isoinfo:	/usr/bin/isoread
	install -l $? $@

/usr/bin/join:	join
	install -cs -o bin $? $@

/usr/bin/kill:	kill
	install -cs -o bin $? $@

/usr/bin/last:	last
	install -cs -o bin $? $@

/usr/bin/uptime:	/usr/bin/last
	install -l $? $@

/usr/bin/leave:	leave
	install -cs -o bin $? $@

/usr/bin/life:	life
	install -cs -o bin $? $@

/usr/bin/login:	login
	install -cs -o bin $? $@

/usr/bin/look:	look
	install -cs -o bin $? $@

/usr/bin/lp:	lp
	install -cs -o bin $? $@

/usr/bin/lpd:	lpd
	install -cs -o daemon -m 4755 $? $@

/usr/bin/ls:	ls
	install -cs -o bin $? $@

/usr/bin/mail:	mail
	install -cs -o root -m 4755 $? $@

/usr/bin/man:	man
	install -cs -o bin $? $@

/usr/bin/mesg:	mesg
	install -cs -o bin $? $@

/usr/bin/mkdir:	mkdir
	install -cs -o bin $? $@

/usr/bin/mkfifo:	mkfifo
	install -cs -o bin $? $@

/usr/bin/mkfs:	mkfs
	install -cs -o bin $? $@

/usr/bin/mknod:	mknod
	install -cs -o bin $? $@

/usr/bin/mkproto:	mkproto
	install -cs -o bin $? $@

/usr/bin/mkswap:	mkswap
	install -cs -o bin $? $@

/usr/bin/modem:	modem
	install -cs -o bin $? $@

/usr/bin/mount:	mount
	install -cs -o root -m 4755 $? $@

/usr/bin/mt:	mt
	install -cs -o bin $? $@

/usr/bin/nm:	nm
	install -cs -o bin $? $@

/usr/bin/nice:	nice
	install -cs -o bin $? $@

/usr/bin/nonamed:	nonamed
	install -cs -o bin $? $@

/usr/bin/od:	od
	install -cs -o bin $? $@

/usr/bin/passwd:	passwd
	install -cs -o root -m 4755 $? $@

/usr/bin/chfn /usr/bin/chsh:	/usr/bin/passwd
	install -l $? $@

/usr/bin/paste:	paste
	install -cs -o bin $? $@

/usr/bin/ping:	ping
	install -cs -o root -m 4755 $? $@

/usr/bin/pr:	pr
	install -cs -o bin $? $@

/usr/bin/pr_routes:	pr_routes
	install -cs -o root -m 4755 $? $@

/usr/bin/progressbar:	progressbar
	install -cs -o root -m 4755 $? $@

/usr/bin/prep:	prep
	install -cs -o bin $? $@

/usr/bin/printf:	printf
	install -cs -o bin $? $@

/usr/bin/printenv:	printenv
	install -cs -o bin $? $@

/usr/bin/printroot:	printroot
	install -cs -o bin $? $@

/usr/bin/proto:	proto
	install -cs -o bin $? $@

/usr/bin/pwd:	pwd
	install -cs -o bin $? $@

/usr/lib/pwdauth:	pwdauth
	install -cs -o root -m 4755 $? $@

/usr/bin/rarpd:	rarpd
	install -cs -o bin $? $@

/usr/bin/rcp:	rcp
	install -cs -o bin $? $@

/usr/bin/rdate:	rdate
	install -cs -o bin $? $@

/usr/bin/readall:	readall
	install -cs -o bin $? $@

/usr/bin/readfs:	readfs
	install -cs -o bin $? $@

/usr/bin/remsync:	remsync
	install -cs -o bin $? $@

/usr/bin/rev:	rev
	install -cs -o bin $? $@

/usr/bin/rget:	rget
	install -cs -o bin $? $@

/usr/bin/rput:	/usr/bin/rget
	install -l $? $@

/usr/bin/rlogin:	rlogin
	install -cs -o bin $? $@

/usr/bin/rmdir:	rmdir
	install -cs -o bin $? $@

/usr/bin/rsh:	rsh
	install -cs -o bin $? $@

/usr/bin/sed:	sed
	install -cs -o bin $? $@

/bin/sed:	sed
	install -cs -o bin $? $@

/usr/bin/shar:	shar
	install -cs -o bin $? $@

/usr/bin/size:	size
	install -cs -o bin $? $@

/usr/bin/sleep:	sleep
	install -cs -o bin $? $@

/usr/bin/slip:	slip
	install -cs -o bin $? $@

/usr/bin/sort:	sort
	install -cs -o bin $? $@

/usr/bin/split:	split
	install -cs -o bin $? $@

/usr/bin/stat:	stat
	install -cs -o bin $? $@

/usr/bin/fstat:	/usr/bin/stat
	install -l $? $@

/usr/bin/strings:	strings
	install -cs -o bin $? $@

/usr/bin/strip:	strip
	install -cs -o bin $? $@

/usr/bin/stty:	stty
	install -cs -o bin $? $@

/usr/bin/su:	su
	install -cs -o root -m 4755 $? $@

/usr/bin/sum:	sum
	install -cs -o bin $? $@

/usr/bin/swapfs:	swapfs
	install -cs -o bin $? $@

/usr/bin/sync:	sync
	install -cs -o bin $? $@

/usr/bin/synctree:	synctree
	install -cs -o bin $? $@

/bin/sysenv:	sysenv
	install -cs -o bin $? $@

/usr/bin/sysenv:	sysenv
	install -cs -o bin $? $@

/usr/bin/tail:	tail
	install -cs -o bin $? $@

/usr/bin/tar:	tar
	install -cs -o bin $? $@

/usr/bin/tcpd:	tcpd
	install -cs -o bin $? $@

/usr/bin/tcpdp:	tcpdp
	install -cs -o bin $? $@

/usr/bin/tee:	tee
	install -cs -o bin $? $@

/usr/bin/term:	term
	install -cs -o bin -g uucp -m 2755 $? $@

/usr/bin/termcap:	termcap
	install -cs -o bin $? $@

/usr/bin/tget:	tget
	install -cs -o bin $? $@

/usr/bin/time:	time
	install -cs -o bin $? $@

/usr/bin/touch:	touch
	install -cs -o bin $? $@

/usr/bin/tr:	tr
	install -cs -o bin $? $@

/usr/bin/treecmp:	treecmp
	install -cs -o bin $? $@

/usr/bin/tsort:	tsort
	install -cs -o bin $? $@

/usr/bin/ttt:	ttt
	install -cs -o bin $? $@

/usr/bin/tty:	tty
	install -cs -o bin $? $@

/usr/bin/umount:	umount
	install -cs -o root -m 4755 $? $@

/usr/bin/uname:	uname
	install -cs -o bin $? $@

/usr/bin/arch:	/usr/bin/uname
	install -l $? $@

/usr/bin/unexpand:	unexpand
	install -cs -o bin $? $@

/usr/bin/uniq:	uniq
	install -cs -o bin $? $@

/usr/bin/update:	update
	install -cs -o bin $? $@

/usr/bin/uud:	uud
	install -cs -o bin $? $@

/usr/bin/uudecode:	/usr/bin/uud
	install -l $? $@

/usr/bin/usyslogd:	usyslogd
	install -cs -o bin $? $@

/usr/bin/uue:	uue
	install -cs -o bin $? $@

/usr/bin/uuencode:	/usr/bin/uue
	install -l $? $@

/usr/bin/vol:	vol
	install -cs -o bin $? $@

/usr/bin/wc:	wc
	install -cs -o bin $? $@

/usr/bin/which:	which
	install -cs -o bin $? $@

/usr/bin/who:	who
	install -cs -o bin $? $@

/usr/bin/whoami:	whoami
	install -cs -o bin $? $@

/usr/bin/write:	write
	install -cs -o bin -g tty -m 2755 $? $@

/usr/bin/writeisofs:	writeisofs
	install -cs -S 2M -o bin $? $@

/usr/bin/xargs:	xargs
	install -cs -o bin $? $@

/usr/bin/yes:	yes
	install -cs -o bin $? $@

/bin/cat:	/usr/bin/cat
	install -lcs $? $@

/bin/date:	/usr/bin/date
	install -lcs $? $@

/bin/fsck:	/usr/bin/fsck
	install -lcs $? $@

/bin/intr:	/usr/bin/intr
	install -lcs $? $@

/bin/mount:	/usr/bin/mount
	install -lcs $? $@

/bin/printroot:	/usr/bin/printroot
	install -lcs $? $@

/bin/pwd:	/usr/bin/pwd
	install -lcs $? $@

/bin/sync:	/usr/bin/sync
	install -lcs $? $@

/bin/umount:	/usr/bin/umount
	install -lcs $? $@

clean:
	rm -rf $(ALL) a.out core
