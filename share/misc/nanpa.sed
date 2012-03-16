# $NetBSD: nanpa.sed,v 1.2 2006/12/25 18:39:48 wiz Exp $
#
# Parse HTML tables output by 
#   http://docs.nanpa.com/cgi-bin/npa_reports/nanpa
# Specifically, for each html table row (TR),
# print the <TD> elements separated by colons.
#
# This could break on HTML comments.
#
:top
#				Strip ^Ms
s///g
#				Join all lines with unterminated HTML tags
/<[^>]*$/{
	N
	b top
}
#				Replace all </TR> with EOL tag
s;</[Tt][Rr]>;$;g
# 				Join lines with only <TR>.
/<[Tt][Rr][^>]*>$/{
	N
	s/\n//g
	b top
}
#				Also, join all lines starting with <TR>.
/<[TtRr][^>]*>[^$]*$/{
	N
	s/\n//g
	b top
}
#				Remove EOL markers
s/\$$//
#				Remove lines not starting with <TR>
/<[Tt][Rr][^>]*>/!d
#				Replace all <TD> with colon
s/[ 	]*<TD[^>]*> */:/g
#				Strip all HTML tags
s/<[^>]*>//g
#				Handle HTML characters
s/&nbsp;/ /g
#				Compress spaces/tabs
s/[ 	][ 	]*/ /g
#				Strip leading colons
s/^://
#				Strip leading/trailing whitespace
s/^ //
s/ $//
