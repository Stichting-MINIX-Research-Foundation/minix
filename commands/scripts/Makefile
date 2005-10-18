# Makefile for cmd/scripts.

CFLAGS	= -D_MINIX -D_POSIX_SOURCE
CCLD	= $(CC) -i $(CFLAGS)
MAKE	= exec make -$(MAKEFLAGS) install

all:		# Does nothing

install:	usr root

# Commands on the /usr partition.
usr:	\
	/usr/bin/DESCRIBE \
	/usr/bin/M \
		/usr/bin/U \
	/usr/bin/MAKEDEV \
	/usr/bin/bigmake \
	/usr/bin/adduser \
	/usr/bin/cd \
		/usr/bin/[ \
		/usr/bin/command \
		/usr/bin/easypack \
		/usr/bin/echo \
		/usr/bin/expr \
		/usr/bin/false \
		/usr/bin/getopts \
		/usr/bin/read \
		/usr/bin/test \
		/usr/bin/true \
		/usr/bin/umask \
		/usr/bin/wait \
	/usr/bin/checkhier \
	/usr/bin/clear \
		/usr/bin/clr \
	/usr/bin/makewhatis \
	/usr/bin/mkdep \
	/usr/bin/mkdist \
	/usr/bin/setup \
	/usr/bin/binsizes \
	/usr/bin/rotate \
	/usr/bin/floppysetup \
	/usr/bin/spell \
	/usr/bin/srccrc \
	/usr/bin/svclog \
	/usr/bin/version \
	/usr/bin/whatis \
		/usr/bin/apropos \
	/usr/bin/whereis \

# Commands on the root partition.
root:	\
	/bin/M \
		/bin/U \
	/bin/cd \
		/bin/[ \
		/bin/command \
		/bin/echo \
		/bin/expr \
		/bin/false \
		/bin/getopts \
		/bin/read \
		/bin/test \
		/bin/true \
		/bin/umask \
		/bin/wait \

clean:

/usr/bin/DESCRIBE:	DESCRIBE.sh
	install -m 755 -c -o bin $? $@

/usr/bin/M:	M.sh
	install -m 755 -c -o bin $? $@

/usr/bin/U:	/usr/bin/M
	install -m 755 -l $? $@

/usr/bin/MAKEDEV:	MAKEDEV.sh
	install -m 755 -c -o bin $? $@

/usr/bin/bigmake:	bigmake.sh
	install -m 755 -c -o bin $? $@

/usr/bin/adduser:	adduser.sh
	install -m 755 -c -o bin $? $@

/usr/bin/cd:	cd.sh
	install -m 755 -c -o bin $? $@

/usr/bin/[ /usr/bin/command /usr/bin/echo /usr/bin/expr /usr/bin/false \
/usr/bin/getopts /usr/bin/read /usr/bin/test /usr/bin/true /usr/bin/umask \
/usr/bin/wait:	/usr/bin/cd
	install -m 755 -l $? $@

/usr/bin/easypack:	easypack.sh
	install -m 755 -c -o bin $? $@

/usr/bin/checkhier:	checkhier.sh
	install -m 755 -c -o bin $? $@

/usr/bin/clear:	clear.sh
	install -m 755 -c -o bin $? $@

/usr/bin/clr:	/usr/bin/clear
	install -m 755 -l $? $@

/usr/bin/makewhatis:	makewhatis.sh
	install -m 755 -c -o bin $? $@

/usr/bin/mkdep:	mkdep.sh
	install -m 755 -c -o bin $? $@

/usr/bin/mkdist:	mkdist.sh
	install -m 755 -c -o bin $? $@

/usr/bin/floppysetup:	floppysetup.sh
	install -m 755 -c -o bin $? $@

/usr/bin/rotate:	rotate.sh
	install -m 755 -c -o bin $? $@

/usr/bin/setup:	setup.sh
	install -m 755 -c -o bin $? $@

/usr/bin/binsizes:	binsizes.sh
	install -m 755 -c -o bin $? $@

/usr/bin/spell:	spell.sh
	install -m 755 -c -o bin $? $@

/usr/bin/srccrc:	srccrc.sh
	install -m 755 -c -o bin $? $@

/usr/bin/svclog:	svclog.sh
	install -m 755 -c -o bin $? $@

/usr/bin/version:	version.sh
	install -m 755 -c -o bin $? $@

/usr/bin/whatis:	whatis.sh
	install -m 755 -c -o bin $? $@

/usr/bin/apropos:	/usr/bin/whatis
	install -m 755 -l $? $@

/usr/bin/whereis:	whereis.sh
	install -m 755 -c -o bin $? $@

/bin/M:	/usr/bin/M
	install -m 755 -c -o bin $? $@

/bin/U:	/bin/M
	install -m 755 -l $? $@

/bin/cd:	/usr/bin/cd
	install -m 755 -lc $? $@

/bin/[ /bin/command /bin/echo /bin/expr /bin/false /bin/getopts \
/bin/read /bin/test /bin/true /bin/umask /bin/wait:	/bin/cd
	install -m 755 -l $? $@
