# $NetBSD: d_qequals.mk,v 1.1 2012/03/17 16:33:14 jruoho Exp $

M= i386
V.i386= OK
V.$M ?= bug

all:
	@echo 'V.$M ?= ${V.$M}'
