# $NetBSD: varparse-errors.mk,v 1.4 2021/03/15 12:15:03 rillig Exp $

# Tests for parsing and evaluating all kinds of variable expressions.
#
# This is the basis for redesigning the error handling in Var_Parse and
# Var_Subst, collecting typical and not so typical use cases.
#
# See also:
#	VarParseResult
#	Var_Parse
#	Var_Subst

PLAIN=		plain value

LITERAL_DOLLAR=	To get a dollar, double $$ it.

INDIRECT=	An ${:Uindirect} value.

REF_UNDEF=	A reference to an ${UNDEF}undefined variable.

ERR_UNCLOSED=	An ${UNCLOSED variable expression.

ERR_BAD_MOD=	An ${:Uindirect:Z} expression with an unknown modifier.

ERR_EVAL=	An evaluation error ${:Uvalue:C,.,\3,}.

# In a conditional, a variable expression that is not enclosed in quotes is
# expanded using the mode VARE_UNDEFERR.
# The variable itself must be defined.
# It may refer to undefined variables though.
.if ${REF_UNDEF} != "A reference to an undefined variable."
.  error
.endif

# As of 2020-12-01, errors in the variable name are silently ignored.
# Since var.c 1.754 from 2020-12-20, unknown modifiers at parse time result
# in an error message and a non-zero exit status.
VAR.${:U:Z}=	unknown modifier in the variable name
.if ${VAR.} != "unknown modifier in the variable name"
.  error
.endif

# As of 2020-12-01, errors in the variable name are silently ignored.
# Since var.c 1.754 from 2020-12-20, unknown modifiers at parse time result
# in an error message and a non-zero exit status.
VAR.${:U:Z}post=	unknown modifier with text in the variable name
.if ${VAR.post} != "unknown modifier with text in the variable name"
.  error
.endif

all:
