#	$NetBSD: Makefile,v 1.3 2010/04/12 18:02:46 joerg Exp $

# the makefile is not, and should not be visited during normal build process.
# try to use latest pod2man, which comes with the latest perl.

.include <bsd.own.mk>
CRYPTODIST=	${NETBSDSRCDIR}/crypto
.include "${NETBSDSRCDIR}/crypto/Makefile.openssl"

VERSION!=	head -2 ${OPENSSLSRC}/README | tail -1 | awk '{ print $$2; }'

make-manpages:
	for i in ${OPENSSLSRC}/doc/apps/*.pod; do \
		fn=`basename $$i .pod`; \
		if [ "$$fn" = "config" ]; then \
			sec=5; \
			dst=openssl.cnf.$$sec; \
		elif [ "$$fn" = "openssl" ]; then \
			sec=1; \
			dst=`basename $$i .pod`.$$sec; \
		else \
			sec=1; \
			dst=openssl_`basename $$i .pod`.$$sec; \
		fi; \
		(cd `dirname $$i`; \
		 pod2man --section=$$sec --center=OpenSSL --release=${VERSION} \
			`basename $$i`) | \
		sed -e 's/[[:space:]]*$$//' | \
		perl libcrypto.pl >$$dst; \
	done
	for i in ${OPENSSLSRC}/doc/crypto/*.pod \
	    ${OPENSSLSRC}/doc/ssl/*.pod; do \
		fn=`basename $$i .pod`; \
		if [ "$$fn" = "des_modes" ]; then \
			sec=7; \
			(cd `dirname $$i`; \
			 pod2man --section=$$sec --center=OpenSSL --release=${VERSION} \
				`basename $$i`) | \
			sed -e '/$$RCSfile/d' -e '/$$Log/d' -e 's/[[:space:]]*$$//' \
			> `basename $$i .pod`.$$sec; \
		else \
			sec=3; \
			(cd `dirname $$i`; \
			 pod2man --section=$$sec --center=OpenSSL --release=${VERSION} \
				`basename $$i`) | \
			sed -e 's/[[:space:]]*$$//' | \
			perl libcrypto.pl >`basename $$i .pod`.$$sec; \
		fi \
	done
	for i in bio blowfish bn bn_internal buffer des dh ecdsa \
	    dsa engine err \
	    evp hmac lhash md5 mdc2 pem rand rc4 ripemd rsa \
	    sha threads ui ui_compat x509; do \
		mv $$i.3 openssl_$$i.3; \
	done

clean:
	-/bin/rm -f *.[0-9]

