#	$NetBSD: Makefile,v 1.36 2011/01/18 23:12:38 jruoho Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

NOOBJ=	# defined

FILES=	NetBSD.el acronyms acronyms.comp airport ascii \
	birthtoken bsd-family-tree \
	country domains operator \
	eqnchar flowers indent.pro inter.phone language man.template \
	mdoc.template na.phone na.postal style zipcodes
FILESDIR=${BINDIR}/misc


# According to iana@ISI.EDU, the URL below is the authoritative list
# distributed by the ISO 3166 Maintenance Agency.
DOMAIN_URL=	http://www.iana.org/root-whois/index.html 
update-domains:
	( \
	  echo '# $$''NetBSD''$$' ;  \
	  echo '# ISO 3166 country codes, from ${DOMAIN_URL}' ; \
	  echo '# Last update:' `date` ; \
	  echo '#' ; \
	  ftp -o - ${DOMAIN_URL} \
	  | ${TOOL_SED} -n -f ${.CURDIR}/domains.sed \
	 ) > domains ; \

update-na.phone:
	< /dev/null ${TOOL_AWK} -f nanpa.awk > na.phone

update-country:
	${.CURDIR}/make.country > country

.include <bsd.prog.mk>
