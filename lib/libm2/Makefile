# Makefile for lib/ack/libm2.

M2	= m2
CFLAGS	= -O -I../h -wo
CC1	= $(CC) $(CFLAGS) -c
M2FLAGS	= -O -ws -n
M21	= $(M2) $(M2FLAGS) -c

LIBRARY	= ../libm2.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(Arguments.o) \
	$(LIBRARY)(ArraySort.o) \
	$(LIBRARY)(CSP.o) \
	$(LIBRARY)(Conversion.o) \
	$(LIBRARY)(EM.o) \
	$(LIBRARY)(InOut.o) \
	$(LIBRARY)(LtoUset.o) \
	$(LIBRARY)(MathLib0.o) \
	$(LIBRARY)(Mathlib.o) \
	$(LIBRARY)(PascalIO.o) \
	$(LIBRARY)(Processes.o) \
	$(LIBRARY)(RealConver.o) \
	$(LIBRARY)(RealInOut.o) \
	$(LIBRARY)(SYSTEM.o) \
	$(LIBRARY)(Semaphores.o) \
	$(LIBRARY)(Storage.o) \
	$(LIBRARY)(StrAss.o) \
	$(LIBRARY)(Streams.o) \
	$(LIBRARY)(Strings.o) \
	$(LIBRARY)(Termcap.o) \
	$(LIBRARY)(Terminal.o) \
	$(LIBRARY)(Traps.o) \
	$(LIBRARY)(XXTermcap.o) \
	$(LIBRARY)(absd.o) \
	$(LIBRARY)(absf.o) \
	$(LIBRARY)(absi.o) \
	$(LIBRARY)(absl.o) \
	$(LIBRARY)(blockmove.o) \
	$(LIBRARY)(cap.o) \
	$(LIBRARY)(catch.o) \
	$(LIBRARY)(confarray.o) \
	$(LIBRARY)(dvi.o) \
	$(LIBRARY)(halt.o) \
	$(LIBRARY)(head_m2.o) \
	$(LIBRARY)(init.o) \
	$(LIBRARY)(load.o) \
	$(LIBRARY)(par_misc.o) \
	$(LIBRARY)(random.o) \
	$(LIBRARY)(rcka.o) \
	$(LIBRARY)(rcki.o) \
	$(LIBRARY)(rckil.o) \
	$(LIBRARY)(rcku.o) \
	$(LIBRARY)(rckul.o) \
	$(LIBRARY)(sigtrp.o) \
	$(LIBRARY)(stackprio.o) \
	$(LIBRARY)(store.o) \
	$(LIBRARY)(ucheck.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(Arguments.o):	Arguments.c
	$(CC1) Arguments.c

$(LIBRARY)(ArraySort.o):	ArraySort.mod
	$(M21) ArraySort.mod

$(LIBRARY)(CSP.o):	CSP.mod
	$(M21) CSP.mod

$(LIBRARY)(Conversion.o):	Conversion.mod
	$(M21) Conversion.mod

$(LIBRARY)(EM.o):	EM.e
	$(CC1) EM.e

$(LIBRARY)(InOut.o):	InOut.mod
	$(M21) InOut.mod

$(LIBRARY)(LtoUset.o):	LtoUset.e
	$(CC1) LtoUset.e

$(LIBRARY)(MathLib0.o):	MathLib0.mod
	$(M21) MathLib0.mod

$(LIBRARY)(Mathlib.o):	Mathlib.mod
	$(M21) Mathlib.mod

$(LIBRARY)(PascalIO.o):	PascalIO.mod
	$(M21) PascalIO.mod

$(LIBRARY)(Processes.o):	Processes.mod
	$(M21) Processes.mod

$(LIBRARY)(RealConver.o):	RealConver.mod
	$(M21) RealConver.mod

$(LIBRARY)(RealInOut.o):	RealInOut.mod
	$(M21) RealInOut.mod

$(LIBRARY)(SYSTEM.o):	SYSTEM.c
	$(CC1) SYSTEM.c

$(LIBRARY)(Semaphores.o):	Semaphores.mod
	$(M21) Semaphores.mod

$(LIBRARY)(Storage.o):	Storage.mod
	$(M21) Storage.mod

$(LIBRARY)(StrAss.o):	StrAss.c
	$(CC1) StrAss.c

$(LIBRARY)(Streams.o):	Streams.mod
	$(M21) Streams.mod

$(LIBRARY)(Strings.o):	Strings.mod
	$(M21) Strings.mod

$(LIBRARY)(Termcap.o):	Termcap.mod
	$(M21) Termcap.mod

$(LIBRARY)(Terminal.o):	Terminal.mod
	$(M21) -D__USG Terminal.mod

$(LIBRARY)(Traps.o):	Traps.mod
	$(M21) Traps.mod

$(LIBRARY)(XXTermcap.o):	XXTermcap.c
	$(CC1) XXTermcap.c

$(LIBRARY)(absd.o):	absd.c
	$(CC1) absd.c

$(LIBRARY)(absf.o):	absf.e
	$(CC1) absf.e

$(LIBRARY)(absi.o):	absi.c
	$(CC1) absi.c

$(LIBRARY)(absl.o):	absl.c
	$(CC1) absl.c

$(LIBRARY)(blockmove.o):	blockmove.c
	$(CC1) blockmove.c

$(LIBRARY)(cap.o):	cap.c
	$(CC1) cap.c

$(LIBRARY)(catch.o):	catch.c
	$(CC1) catch.c

$(LIBRARY)(confarray.o):	confarray.c
	$(CC1) confarray.c

$(LIBRARY)(dvi.o):	dvi.c
	$(CC1) dvi.c

$(LIBRARY)(halt.o):	halt.c
	$(CC1) halt.c

$(LIBRARY)(head_m2.o):	head_m2.e
	$(CC1) head_m2.e

$(LIBRARY)(init.o):	init.c
	$(CC1) init.c

$(LIBRARY)(load.o):	load.c
	$(CC1) load.c

$(LIBRARY)(par_misc.o):	par_misc.e
	$(CC1) par_misc.e

$(LIBRARY)(random.o):	random.mod
	$(M21) random.mod

$(LIBRARY)(rcka.o):	rcka.c
	$(CC1) rcka.c

$(LIBRARY)(rcki.o):	rcki.c
	$(CC1) rcki.c

$(LIBRARY)(rckil.o):	rckil.c
	$(CC1) rckil.c

$(LIBRARY)(rcku.o):	rcku.c
	$(CC1) rcku.c

$(LIBRARY)(rckul.o):	rckul.c
	$(CC1) rckul.c

$(LIBRARY)(sigtrp.o):	sigtrp.c
	$(CC1) sigtrp.c

$(LIBRARY)(stackprio.o):	stackprio.c
	$(CC1) stackprio.c

$(LIBRARY)(store.o):	store.c
	$(CC1) store.c

$(LIBRARY)(ucheck.o):	ucheck.c
	$(CC1) ucheck.c
