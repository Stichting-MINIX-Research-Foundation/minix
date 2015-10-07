## $NetBSD: Makefile.am,v 1.4 2010/11/06 00:03:33 agc Exp $

AM_CFLAGS		= $(WARNCFLAGS)

bin_PROGRAMS		= netpgpkeys

netpgpkeys_SOURCES	= netpgpkeys.c

netpgpkeys_CPPFLAGS	= -I$(top_srcdir)/include -I../libmj

netpgpkeys_LDADD	= ../lib/libnetpgp.la ../libmj/libmj.la

dist_man_MANS		= netpgpkeys.1
