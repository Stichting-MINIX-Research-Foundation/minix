# $Id: cond2.mk,v 1.1 2015/05/05 21:51:09 sjg Exp $

TEST_UNAME_S= NetBSD

# this should be ok
X:= ${${TEST_UNAME_S} == "NetBSD":?Ok:fail}
.if $X == "Ok"
Y= good
.endif
# expect: Bad conditional expression ` == "empty"' in  == "empty"?oops:ok
X:= ${${TEST_NOT_SET} == "empty":?oops:ok}
# expect: Malformed conditional ({TEST_TYPO} == "Ok")
.if {TEST_TYPO} == "Ok"
Y= oops
.endif
.if empty(TEST_NOT_SET)
Y!= echo TEST_NOT_SET is empty or not defined >&2; echo
.endif
# expect: Malformed conditional (${TEST_NOT_SET} == "empty")
.if ${TEST_NOT_SET} == "empty"
Y= oops
.endif

all:
	@echo $@
