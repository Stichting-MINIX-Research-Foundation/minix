# $NetBSD: varmod-loop.mk,v 1.13 2021/03/15 17:54:49 rillig Exp $
#
# Tests for the :@var@...${var}...@ variable modifier.

.MAKE.SAVE_DOLLARS=	yes

all: varname-overwriting-target
all: mod-loop-resolve
all: mod-loop-varname-dollar
all: mod-loop-dollar

# In the :@ modifier, the name of the loop variable can even be generated
# dynamically.  There's no practical use-case for this, and hopefully nobody
# will ever depend on this, but technically it's possible.
# Therefore, in -dL mode, this is forbidden, see lint.mk.
.if ${:Uone two three:@${:Ubar:S,b,v,}@+${var}+@} != "+one+ +two+ +three+"
.  error
.endif

# ":::" is a very creative variable name, unlikely in practice.
# The expression ${\:\:\:} would not work since backslashes can only
# be escaped in the modifiers, but not in the variable name.
.if ${:U1 2 3:@:::@x${${:U\:\:\:}}y@} != "x1y x2y x3y"
.  error
.endif

# "@@" is another creative variable name.
.if ${:U1 2 3:@\@\@@x${@@}y@} != "x1y x2y x3y"
.  error
.endif

varname-overwriting-target:
	# Even "@" works as a variable name since the variable is installed
	# in the "current" scope, which in this case is the one from the
	# target.  Because of this, after the loop has finished, '$@' is
	# undefined.  This is something that make doesn't expect, this may
	# even trigger an assertion failure somewhere.
	@echo :$@: :${:U1 2 3:@\@@x${@}y@}: :$@:

# In extreme cases, even the backslash can be used as variable name.
# It needs to be doubled though.
.if ${:U1 2 3:@\\@x${${:Ux:S,x,\\,}}y@} != "x1y x2y x3y"
.  error
.endif

# The variable name can technically be empty, and in this situation
# the variable value cannot be accessed since the empty "variable"
# is protected to always return an empty string.
.if ${:U1 2 3:@@x${}y@} != "xy xy xy"
.  error
.endif


# The :@ modifier resolves the variables from the replacement text once more
# than expected.  In particular, it resolves _all_ variables from the scope,
# and not only the loop variable (in this case v).
SRCS=		source
CFLAGS.source=	before
ALL_CFLAGS:=	${SRCS:@src@${CFLAGS.${src}}@}	# note the ':='
CFLAGS.source+=	after
.if ${ALL_CFLAGS} != "before"
.  error
.endif


# In the following example, the modifier ':@' expands the '$$' to '$'.  This
# means that when the resulting expression is evaluated, these resulting '$'
# will be interpreted as starting a subexpression.
#
# The d means direct reference, the i means indirect reference.
RESOLVE=	${RES1} $${RES1}
RES1=		1d${RES2} 1i$${RES2}
RES2=		2d${RES3} 2i$${RES3}
RES3=		3

# TODO: convert to '.if'.
mod-loop-resolve:
	@echo $@:${RESOLVE:@v@w${v}w@:Q}:


# Until 2020-07-20, the variable name of the :@ modifier could end with one
# or two dollar signs, which were silently ignored.
# There's no point in allowing a dollar sign in that position.
mod-loop-varname-dollar:
	@echo $@:${1 2 3:L:@v$@($v)@:Q}.
	@echo $@:${1 2 3:L:@v$$@($v)@:Q}.
	@echo $@:${1 2 3:L:@v$$$@($v)@:Q}.

# Demonstrate that it is possible to generate dollar signs using the
# :@ modifier.
#
# These are edge cases that could have resulted in a parse error as well
# since the $@ at the end could have been interpreted as a variable, which
# would mean a missing closing @ delimiter.
mod-loop-dollar:
	@echo $@:${:U1:@word@${word}$@:Q}:
	@echo $@:${:U2:@word@$${word}$$@:Q}:
	@echo $@:${:U3:@word@$$${word}$$$@:Q}:
	@echo $@:${:U4:@word@$$$${word}$$$$@:Q}:
	@echo $@:${:U5:@word@$$$$${word}$$$$$@:Q}:
	@echo $@:${:U6:@word@$$$$$${word}$$$$$$@:Q}:

# It may happen that there are nested :@ modifiers that use the same name for
# for the loop variable.  These modifiers influence each other.
#
# As of 2020-10-18, the :@ modifier is implemented by actually setting a
# variable in the scope of the expression and deleting it again after the
# loop.  This is different from the .for loops, which substitute the variable
# expression with ${:Uvalue}, leading to different unwanted side effects.
#
# To make the behavior more predictable, the :@ modifier should restore the
# loop variable to the value it had before the loop.  This would result in
# the string "1a b c1 2a b c2 3a b c3", making the two loops independent.
.if ${:U1 2 3:@i@$i${:Ua b c:@i@$i@}${i:Uu}@} != "1a b cu 2a b cu 3a b cu"
.  error
.endif

# During the loop, the variable is actually defined and nonempty.
# If the loop were implemented in the same way as the .for loop, the variable
# would be neither defined nor nonempty since all expressions of the form
# ${var} would have been replaced with ${:Uword} before evaluating them.
.if defined(var)
.  error
.endif
.if ${:Uword:@var@${defined(var):?def:undef} ${empty(var):?empty:nonempty}@} \
    != "def nonempty"
.  error
.endif
.if defined(var)
.  error
.endif

# Assignment using the ':=' operator, combined with the :@var@ modifier
#
8_DOLLARS=	$$$$$$$$
# This string literal is written with 8 dollars, and this is saved as the
# variable value.  But as soon as this value is evaluated, it goes through
# Var_Subst, which replaces each '$$' with a single '$'.  This could be
# prevented by VarEvalFlags.keepDollar, but that flag is usually removed
# before expanding subexpressions.  See ApplyModifier_Loop and
# ParseModifierPart for examples.
#
.MAKEFLAGS: -dcp
USE_8_DOLLARS=	${:U1:@var@${8_DOLLARS}@} ${8_DOLLARS} $$$$$$$$
.if ${USE_8_DOLLARS} != "\$\$\$\$ \$\$\$\$ \$\$\$\$"
.  error
.endif
#
SUBST_CONTAINING_LOOP:= ${USE_8_DOLLARS}
# The ':=' assignment operator evaluates the variable value using the mode
# VARE_KEEP_DOLLAR_UNDEF, which means that some dollar signs are preserved,
# but not all.  The dollar signs in the top-level expression and in the
# indirect ${8_DOLLARS} are preserved.
#
# The variable modifier :@var@ does not preserve the dollar signs though, no
# matter in which context it is evaluated.  What happens in detail is:
# First, the modifier part "${8_DOLLARS}" is parsed without expanding it.
# Next, each word of the value is expanded on its own, and at this moment
# in ApplyModifier_Loop, the flag keepDollar is not passed down to
# ModifyWords, resulting in "$$$$" for the first word of USE_8_DOLLARS.
#
# The remaining words of USE_8_DOLLARS are not affected by any variable
# modifier and are thus expanded with the flag keepDollar in action.
# The variable SUBST_CONTAINING_LOOP therefore gets assigned the raw value
# "$$$$ $$$$$$$$ $$$$$$$$".
#
# The variable expression in the condition then expands this raw stored value
# once, resulting in "$$ $$$$ $$$$".  The effects from VARE_KEEP_DOLLAR no
# longer take place since they had only been active during the evaluation of
# the variable assignment.
.if ${SUBST_CONTAINING_LOOP} != "\$\$ \$\$\$\$ \$\$\$\$"
.  error
.endif
.MAKEFLAGS: -d0

# After looping over the words of the expression, the loop variable gets
# undefined.  The modifier ':@' uses an ordinary global variable for this,
# which is different from the '.for' loop, which replaces ${var} with
# ${:Uvalue} in the body of the loop.  This choice of implementation detail
# can be used for a nasty side effect.  The expression ${:U:@VAR@@} evaluates
# to an empty string, plus it undefines the variable 'VAR'.  This is the only
# possibility to undefine a global variable during evaluation.
GLOBAL=		before-global
RESULT:=	${:U${GLOBAL} ${:U:@GLOBAL@@} ${GLOBAL:Uundefined}}
.if ${RESULT} != "before-global  undefined"
.  error
.endif

# The above side effect of undefining a variable from a certain scope can be
# further combined with the otherwise undocumented implementation detail that
# the argument of an '.if' directive is evaluated in cmdline scope.  Putting
# these together makes it possible to undefine variables from the cmdline
# scope, something that is not possible in a straight-forward way.
.MAKEFLAGS: CMDLINE=cmdline
.if ${:U${CMDLINE}${:U:@CMDLINE@@}} != "cmdline"
.  error
.endif
# Now the cmdline variable got undefined.
.if ${CMDLINE} != "cmdline"
.  error
.endif
# At this point, it still looks as if the cmdline variable were defined,
# since the value of CMDLINE is still "cmdline".  That impression is only
# superficial though, the cmdline variable is actually deleted.  To
# demonstrate this, it is now possible to override its value using a global
# variable, something that was not possible before:
CMDLINE=	global
.if ${CMDLINE} != "global"
.  error
.endif
# Now undefine that global variable again, to get back to the original value.
.undef CMDLINE
.if ${CMDLINE} != "cmdline"
.  error
.endif
# What actually happened is that when CMDLINE was set by the '.MAKEFLAGS'
# target in the cmdline scope, that same variable was exported to the
# environment, see Var_SetWithFlags.
.unexport CMDLINE
.if ${CMDLINE} != "cmdline"
.  error
.endif
# The above '.unexport' has no effect since UnexportVar requires a global
# variable of the same name to be defined, otherwise nothing is unexported.
CMDLINE=	global
.unexport CMDLINE
.undef CMDLINE
.if ${CMDLINE} != "cmdline"
.  error
.endif
# This still didn't work since there must not only be a global variable, the
# variable must be marked as exported as well, which it wasn't before.
CMDLINE=	global
.export CMDLINE
.unexport CMDLINE
.undef CMDLINE
.if ${CMDLINE:Uundefined} != "undefined"
.  error
.endif
# Finally the variable 'CMDLINE' from the cmdline scope is gone, and all its
# traces from the environment are gone as well.  To do that, a global variable
# had to be defined and exported, something that is far from obvious.  To
# recap, here is the essence of the above story:
.MAKEFLAGS: CMDLINE=cmdline	# have a cmdline + environment variable
.if ${:U:@CMDLINE@@}}		# undefine cmdline, keep environment
.endif
CMDLINE=	global		# needed for deleting the environment
.export CMDLINE			# needed for deleting the environment
.unexport CMDLINE		# delete the environment
.undef CMDLINE			# delete the global helper variable
.if ${CMDLINE:Uundefined} != "undefined"
.  error			# 'CMDLINE' is gone now from all scopes
.endif


# TODO: Actually trigger the undefined behavior (use after free) that was
#  already suspected in Var_Parse, in the comment 'the value of the variable
#  must not change'.
