# $Id: qequals.mk,v 1.1 2014/08/21 13:44:51 apb Exp $

M= i386
V.i386= OK
V.$M ?= bug

all:
	@echo 'V.$M ?= ${V.$M}'
