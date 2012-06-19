.include <bsd.own.mk>
ETC=${DESTDIR}/etc/
USR=${DESTDIR}/usr/
USRETC=${DESTDIR}/usr/etc/
DEFAULTS=${ETC}/defaults

#
# FILES1 are installed in /etc *before* the tree hierarchy is created
# and can therefore not rely on /etc/groups and /etc/passwd being 
# present.

FILES1=group hostname.file inet.conf motd.install mtab profile \
	protocols rc services termcap utmp rc.cd  \
	syslog.conf rc.daemons.dist \
	rs.inet rs.single make.conf system.conf ttys resolv.conf rc.conf \
	rc.subr man.conf shells boot.cfg.default \
	devmand/scripts/block devmand/scripts/singlechar

#
# PWFILES are installed in /etc with the 600 permission
#
PWFILES=master.passwd

#
# FILES3 are installed in /usr/etc 755
#
FILES3=daily dhcptags.conf rc 
DEFAULTFILES=rc.conf minix.rc.conf 
USRFILES=Makefile

TOOL_PWD_MKDB?= pwd_mkdb

clean::

install: installpw	# installpw needed to bootstrap pw db
	@echo "Installing /etc, /usr/etc and /usr/lib.."
	${INSTALL_DIR} $(ETC)
	${INSTALL_DIR} $(ETC)/devmand/scripts
	@echo "Making hierarchy.."
	sh mtree.sh mtree/minix.tree ${TOOL_AWK} ${TOOL_STAT} ${MKUNPRIVED} ${DESTDIR};	# DESTDIR must be last; special case!
	@for f in $(FILES1); do if [ -f $(ETC)/$$f ]; then :; else cp $$f $(ETC)/$$f; chmod 755 $(ETC)/$$f; fi; done
	@for f in $(USRFILES); do cp usr/$$f $(USR)/$$f; chmod 644 $(USR)/$$f; done
	@for f in $(FILES3); do if [ -f $(USRETC)/$$f ]; then :; else cp usr/$$f $(USRETC); chmod 755 $(USRETC)/$$f; fi; done
	for f in $(DEFAULTFILES); do cp defaults/$$f $(DEFAULTS)/; chmod 644 $(DEFAULTS)/$$f; done
	@echo "Making devices.."
		if [ ${MKUNPRIVED} != yes ]	;then \
		cd ${DESTDIR}/dev && sh ${NETBSDSRCDIR}/commands/MAKEDEV/MAKEDEV.sh null ;\
		cd ${DESTDIR}/dev && sh ${NETBSDSRCDIR}/commands/MAKEDEV/MAKEDEV.sh std  2>/dev/null ;\
	fi
	${INSTALL_FILE} -o root -g operator -m 755 crontab ${DESTDIR}/usr/lib
	@echo "Making user homedirs.."
	for u in ${DESTDIR}/usr/ast ${DESTDIR}/root; do  ${INSTALL_DIR} $$u; cp ast/.[aepv]* $$u ; done
	@echo "Installing fonts.."
	${INSTALL_FILE} -m 644 -o root -g operator fonts/*.fnt ${DESTDIR}/usr/lib/fonts/

installforce:: $(ETC)/rc $(ETC)/rs.inet $(ETC)/rs.single $(ETC)/system.conf $(USRETC)/rc $(USR)/Makefile installpw

installpw::
	if [ ! -d $(ETC) ]; then ${INSTALL_DIR} $(ETC); chmod 755 $(ETC); fi
	@for f in $(PWFILES); do if [ -f $(ETC)/$$f ]; then :; else cp $$f $(ETC)/$$f; chmod 600 $(ETC)/$$f; fi; done
	touch ${ETC}/pwd.db; touch ${ETC}/spwd.db; ${TOOL_PWD_MKDB} -d ${ETC}/.. -p -V 0 ${ETC}/master.passwd

$(ETC)/rc: rc .PHONY
	${INSTALL_FILE} -m 755 -o root -g operator $> $@

$(ETC)/rs.inet: rs.inet .PHONY
	${INSTALL_FILE} -m 755 -o root -g operator $> $@

$(ETC)/rs.single: rs.single .PHONY
	${INSTALL_FILE} -m 755 -o root -g operator $> $@

$(ETC)/system.conf: system.conf .PHONY
	${INSTALL_FILE} -m 644 -o root -g operator $> $@

$(USRETC)/rc: usr/rc .PHONY
	${INSTALL_FILE} -m 755 -o root -g operator $> $@

$(USR)/Makefile: usr/Makefile .PHONY
	${INSTALL_FILE} -m 644 -o root -g operator $> $@
