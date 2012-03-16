# $NetBSD: domains.sed,v 1.3 2007/09/29 15:40:31 hubertf Exp $
s/<[^>]*>//g
/&nbsp;&nbsp/ {
	s/&nbsp;/ /g
	s/&#[0-9]*;/ /g
	s/  */ /g
	s/^ *\.//
	s/$//
	p
}
