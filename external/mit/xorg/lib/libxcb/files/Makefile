#	$NetBSD: Makefile,v 1.2 2014/03/17 07:25:34 mrg Exp $

# This makefile is used to generate the libxcb xml/python generated files.
# For now it is broken, and these files were copied from a normal libxcb
# build.

.include <bsd.own.mk>
.include "Makefile.xmlsrcs"

PYTHON?=	/usr/pkg/bin/python2.7
PYTHONLIB?=	/usr/pkg/lib/python2.7
C_CLIENT?=	${X11SRCDIR.xcb}/src/c_client.py

.SUFFIXES: .c .xml

# this doesn't work; needs to run in the xcb tree for some reason?
.xml.c:
	${PYTHON} ${C_CLIENT} -p ${PYTHONLIB}/site-packages ${.IMPSRC}

generate_from_xml: ${XMLSRCS}

all: generate_from_xml

.include <bsd.subdir.mk>

.PATH: ${X11SRCDIR.xcb-proto}/src
