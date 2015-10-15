# $Id: varmisc.mk,v 1.2 2014/08/30 22:21:08 sjg Exp $
#
# Miscellaneous variable tests.

all: unmatched_var_paren

unmatched_var_paren:
	@echo ${foo::=foo-text}
