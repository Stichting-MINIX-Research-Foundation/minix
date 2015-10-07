## $NetBSD: Makefile.am,v 1.3 2010/11/06 00:03:33 agc Exp $

AM_CFLAGS		= $(WARNCFLAGS)

bin_PROGRAMS		= netpgp

netpgp_SOURCES		= netpgp.c

netpgp_CPPFLAGS		= -I$(top_srcdir)/include

netpgp_LDADD		= ../lib/libnetpgp.la ../libmj/libmj.la

dist_man_MANS		= netpgp.1
