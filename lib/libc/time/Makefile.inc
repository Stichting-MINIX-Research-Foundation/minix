#	$NetBSD: Makefile.inc,v 1.21 2014/10/08 05:44:03 msaitoh Exp $

.PATH: ${.CURDIR}/time

SRCS+=	asctime.c difftime.c localtime.c getdate.c strftime.c strptime.c
MAN+=	ctime.3 getdate.3 offtime.3 strftime.3 strptime.3		\
	time2posix.3 tzfile.5 tzset.3
CPPFLAGS+=-DALL_STATE -DUSG_COMPAT

MLINKS+=ctime.3 ctime_r.3	\
	ctime.3 asctime.3	\
	ctime.3 asctime_r.3	\
	ctime.3 ctime_rz.3	\
	ctime.3 difftime.3	\
	ctime.3 gmtime.3	\
	ctime.3 gmtime_r.3	\
	ctime.3 localtime.3	\
	ctime.3 localtime_r.3	\
	ctime.3 localtime_rz.3	\
	ctime.3 mktime.3	\
	ctime.3 mktime_z.3	\
	getdate.3 getdate_err.3	\
	offtime.3 offtime_r.3	\
	offtime.3 timegm.3	\
	offtime.3 timelocal.3	\
	offtime.3 timeoff.3	\
	strftime.3 strftime_z.3 \
	time2posix.3 posix2time.3 \
	time2posix.3 posix2time_z.3 \
	time2posix.3 time2posix_z.3 \
	tzset.3 tzgetname.3	\
	tzset.3 daylight.3	\
	tzset.3 tzalloc.3	\
	tzset.3 tzfree.3	\
	tzset.3 tzsetwall.3	

COPTS.strftime.c = -Wno-format-nonliteral
