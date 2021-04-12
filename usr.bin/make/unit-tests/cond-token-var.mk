# $NetBSD: cond-token-var.mk,v 1.5 2020/11/15 14:58:14 rillig Exp $
#
# Tests for variable expressions in .if conditions.
#
# Note the fine distinction between a variable and a variable expression.
# A variable has a name and a value.  To access the value, one writes a
# variable expression of the form ${VAR}.  This is a simple variable
# expression.  Variable expressions can get more complicated by adding
# variable modifiers such as in ${VAR:Mpattern}.
#
# XXX: Strictly speaking, variable modifiers should be called expression
# modifiers instead since they only modify the expression, not the variable.
# Well, except for the assignment modifiers, these do indeed change the value
# of the variable.

DEF=	defined

# A defined variable may appear on either side of the comparison.
.if ${DEF} == ${DEF}
.  info ok
.else
.  error
.endif

# A variable that appears on the left-hand side must be defined.
# The following line thus generates a parse error.
.if ${UNDEF} == ${DEF}
.  error
.endif

# A variable that appears on the right-hand side must be defined.
# The following line thus generates a parse error.
.if ${DEF} == ${UNDEF}
.  error
.endif

# A defined variable may appear as an expression of its own.
.if ${DEF}
.endif

# An undefined variable on its own generates a parse error.
.if ${UNDEF}
.endif

# The :U modifier turns an undefined expression into a defined expression.
# Since the expression is defined now, it doesn't generate any parse error.
.if ${UNDEF:U}
.endif
