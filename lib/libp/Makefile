# Makefile for lib/libp.

CFLAGS	= -O -I../h -wo
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libp.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(abi.o) \
	$(LIBRARY)(abl.o) \
	$(LIBRARY)(abr.o) \
	$(LIBRARY)(arg.o) \
	$(LIBRARY)(ass.o) \
	$(LIBRARY)(asz.o) \
	$(LIBRARY)(atn.o) \
	$(LIBRARY)(bcp.o) \
	$(LIBRARY)(bts.o) \
	$(LIBRARY)(buff.o) \
	$(LIBRARY)(catch.o) \
	$(LIBRARY)(clock.o) \
	$(LIBRARY)(cls.o) \
	$(LIBRARY)(cvt.o) \
	$(LIBRARY)(diag.o) \
	$(LIBRARY)(dis.o) \
	$(LIBRARY)(efl.o) \
	$(LIBRARY)(eln.o) \
	$(LIBRARY)(encaps.o) \
	$(LIBRARY)(exp.o) \
	$(LIBRARY)(fef.o) \
	$(LIBRARY)(fif.o) \
	$(LIBRARY)(get.o) \
	$(LIBRARY)(gto.o) \
	$(LIBRARY)(head_pc.o) \
	$(LIBRARY)(hlt.o) \
	$(LIBRARY)(hol0.o) \
	$(LIBRARY)(incpt.o) \
	$(LIBRARY)(ini.o) \
	$(LIBRARY)(log.o) \
	$(LIBRARY)(mdi.o) \
	$(LIBRARY)(mdl.o) \
	$(LIBRARY)(new.o) \
	$(LIBRARY)(nfa.o) \
	$(LIBRARY)(nobuff.o) \
	$(LIBRARY)(notext.o) \
	$(LIBRARY)(opn.o) \
	$(LIBRARY)(outcpt.o) \
	$(LIBRARY)(pac.o) \
	$(LIBRARY)(pclose.o) \
	$(LIBRARY)(pcreat.o) \
	$(LIBRARY)(pentry.o) \
	$(LIBRARY)(perrno.o) \
	$(LIBRARY)(pexit.o) \
	$(LIBRARY)(popen.o) \
	$(LIBRARY)(put.o) \
	$(LIBRARY)(rcka.o) \
	$(LIBRARY)(rdc.o) \
	$(LIBRARY)(rdi.o) \
	$(LIBRARY)(rdl.o) \
	$(LIBRARY)(rdr.o) \
	$(LIBRARY)(rf.o) \
	$(LIBRARY)(rln.o) \
	$(LIBRARY)(rnd.o) \
	$(LIBRARY)(sav.o) \
	$(LIBRARY)(sig.o) \
	$(LIBRARY)(sin.o) \
	$(LIBRARY)(sqt.o) \
	$(LIBRARY)(string.o) \
	$(LIBRARY)(trap.o) \
	$(LIBRARY)(trp.o) \
	$(LIBRARY)(unp.o) \
	$(LIBRARY)(uread.o) \
	$(LIBRARY)(uwrite.o) \
	$(LIBRARY)(wdw.o) \
	$(LIBRARY)(wf.o) \
	$(LIBRARY)(wrc.o) \
	$(LIBRARY)(wrf.o) \
	$(LIBRARY)(wri.o) \
	$(LIBRARY)(wrl.o) \
	$(LIBRARY)(wrr.o) \
	$(LIBRARY)(wrs.o) \
	$(LIBRARY)(wrz.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(abi.o):	abi.c
	$(CC1) abi.c

$(LIBRARY)(abl.o):	abl.c
	$(CC1) abl.c

$(LIBRARY)(abr.o):	abr.c
	$(CC1) abr.c

$(LIBRARY)(arg.o):	arg.c
	$(CC1) arg.c

$(LIBRARY)(ass.o):	ass.c
	$(CC1) ass.c

$(LIBRARY)(asz.o):	asz.c
	$(CC1) asz.c

$(LIBRARY)(atn.o):	atn.c
	$(CC1) atn.c

$(LIBRARY)(bcp.o):	bcp.c
	$(CC1) bcp.c

$(LIBRARY)(bts.o):	bts.e
	$(CC1) bts.e

$(LIBRARY)(buff.o):	buff.c
	$(CC1) buff.c

$(LIBRARY)(catch.o):	catch.c
	$(CC1) catch.c

$(LIBRARY)(clock.o):	clock.c
	$(CC1) clock.c

$(LIBRARY)(cls.o):	cls.c
	$(CC1) cls.c

$(LIBRARY)(cvt.o):	cvt.c
	$(CC1) cvt.c

$(LIBRARY)(diag.o):	diag.c
	$(CC1) diag.c

$(LIBRARY)(dis.o):	dis.c
	$(CC1) dis.c

$(LIBRARY)(efl.o):	efl.c
	$(CC1) efl.c

$(LIBRARY)(eln.o):	eln.c
	$(CC1) eln.c

$(LIBRARY)(encaps.o):	encaps.e
	$(CC1) encaps.e

$(LIBRARY)(exp.o):	exp.c
	$(CC1) exp.c

$(LIBRARY)(fef.o):	fef.e
	$(CC1) fef.e

$(LIBRARY)(fif.o):	fif.e
	$(CC1) fif.e

$(LIBRARY)(get.o):	get.c
	$(CC1) get.c

$(LIBRARY)(gto.o):	gto.e
	$(CC1) gto.e

$(LIBRARY)(head_pc.o):	head_pc.e
	$(CC1) head_pc.e

$(LIBRARY)(hlt.o):	hlt.c
	$(CC1) hlt.c

$(LIBRARY)(hol0.o):	hol0.e
	$(CC1) hol0.e

$(LIBRARY)(incpt.o):	incpt.c
	$(CC1) incpt.c

$(LIBRARY)(ini.o):	ini.c
	$(CC1) ini.c

$(LIBRARY)(log.o):	log.c
	$(CC1) log.c

$(LIBRARY)(mdi.o):	mdi.c
	$(CC1) mdi.c

$(LIBRARY)(mdl.o):	mdl.c
	$(CC1) mdl.c

$(LIBRARY)(new.o):	new.c
	$(CC1) new.c

$(LIBRARY)(nfa.o):	nfa.c
	$(CC1) nfa.c

$(LIBRARY)(nobuff.o):	nobuff.c
	$(CC1) nobuff.c

$(LIBRARY)(notext.o):	notext.c
	$(CC1) notext.c

$(LIBRARY)(opn.o):	opn.c
	$(CC1) opn.c

$(LIBRARY)(outcpt.o):	outcpt.c
	$(CC1) outcpt.c

$(LIBRARY)(pac.o):	pac.c
	$(CC1) pac.c

$(LIBRARY)(pclose.o):	pclose.c
	$(CC1) pclose.c

$(LIBRARY)(pcreat.o):	pcreat.c
	$(CC1) pcreat.c

$(LIBRARY)(pentry.o):	pentry.c
	$(CC1) pentry.c

$(LIBRARY)(perrno.o):	perrno.c
	$(CC1) perrno.c

$(LIBRARY)(pexit.o):	pexit.c
	$(CC1) pexit.c

$(LIBRARY)(popen.o):	popen.c
	$(CC1) popen.c

$(LIBRARY)(put.o):	put.c
	$(CC1) put.c

$(LIBRARY)(rcka.o):	rcka.c
	$(CC1) rcka.c

$(LIBRARY)(rdc.o):	rdc.c
	$(CC1) rdc.c

$(LIBRARY)(rdi.o):	rdi.c
	$(CC1) rdi.c

$(LIBRARY)(rdl.o):	rdl.c
	$(CC1) rdl.c

$(LIBRARY)(rdr.o):	rdr.c
	$(CC1) rdr.c

$(LIBRARY)(rf.o):	rf.c
	$(CC1) rf.c

$(LIBRARY)(rln.o):	rln.c
	$(CC1) rln.c

$(LIBRARY)(rnd.o):	rnd.c
	$(CC1) rnd.c

$(LIBRARY)(sav.o):	sav.e
	$(CC1) sav.e

$(LIBRARY)(sig.o):	sig.e
	$(CC1) sig.e

$(LIBRARY)(sin.o):	sin.c
	$(CC1) sin.c

$(LIBRARY)(sqt.o):	sqt.c
	$(CC1) sqt.c

$(LIBRARY)(string.o):	string.c
	$(CC1) string.c

$(LIBRARY)(trap.o):	trap.e
	$(CC1) trap.e

$(LIBRARY)(trp.o):	trp.e
	$(CC1) trp.e

$(LIBRARY)(unp.o):	unp.c
	$(CC1) unp.c

$(LIBRARY)(uread.o):	uread.c
	$(CC1) uread.c

$(LIBRARY)(uwrite.o):	uwrite.c
	$(CC1) uwrite.c

$(LIBRARY)(wdw.o):	wdw.c
	$(CC1) wdw.c

$(LIBRARY)(wf.o):	wf.c
	$(CC1) wf.c

$(LIBRARY)(wrc.o):	wrc.c
	$(CC1) wrc.c

$(LIBRARY)(wrf.o):	wrf.c
	$(CC1) wrf.c

$(LIBRARY)(wri.o):	wri.c
	$(CC1) wri.c

$(LIBRARY)(wrl.o):	wrl.c
	$(CC1) wrl.c

$(LIBRARY)(wrr.o):	wrr.c
	$(CC1) wrr.c

$(LIBRARY)(wrs.o):	wrs.c
	$(CC1) wrs.c

$(LIBRARY)(wrz.o):	wrz.c
	$(CC1) wrz.c
