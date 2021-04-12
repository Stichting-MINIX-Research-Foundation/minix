# $NetBSD: comment.mk,v 1.3 2020/11/15 14:07:53 rillig Exp $
#
# Demonstrate how comments are written in makefiles.

# This is a comment.

#\
This is a multiline comment.

# Another multiline comment \
that \
goes \
on and on.

 # Comments can be indented with spaces, but that is rather unusual.

	# Comments can be indented with a tab.
	# These are not shell commands, they are just makefile comments.

.if 1			# There can be comments after conditions.
.endif			# And after the closing directive.

VAR=			# This comment makes the variable value empty.
			# ParseGetLine removes any whitespace before the
			# comment.
.if ${VAR} != ""
.  error
.endif

# The comment does not need to start at the beginning of a word (as in the
# shell), it can start anywhere.
VAR=# defined but empty

# The space before the comment is always trimmed.
VAR=	value
.if ${VAR} != "value"
.  error
.endif

# This comment ends with 2 backslashes.  An even number of backslashes does
# not count as a line continuation, therefore the variable assignment that
# follows is actively interpreted. \\
VAR=	not part of the comment
.if ${VAR} != "not part of the comment"
.  error
.endif

# To escape a comment sign, precede it with a backslash.
VAR=	\#		# Both in the assignment.
.if ${VAR} != "\#"	# And in the comparison.
.  error
.endif

# Since 2012-03-24 the variable modifier :[#] does not need to be escaped.
# To keep the parsing code simple, any "[#" does not start a comment, even
# outside of a variable expression.
WORDS=	${VAR:[#]} [#
.if ${WORDS} != "1 [#"
.  error
.endif

# An odd number of backslashes makes a line continuation, \\\
no matter if it is 3 or 5 \\\\\
or 9 backslashes. \\\\\\\\\
This is the last line of the comment.
VAR=	no comment anymore
.if ${VAR} != "no comment anymore"
.  error
.endif

all:
# In the commands associated with a target, the '#' does not start a makefile
# comment.  The '#' is just passed to the shell, like any ordinary character.
	echo This is a shell comment: # comment
# If the '#' were to start a makefile comment, the following shell command
# would have unbalanced quotes.
	echo This is not a shell comment: '# comment'
	@echo A shell comment can#not start in the middle of a word.
