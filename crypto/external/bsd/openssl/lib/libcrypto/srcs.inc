#	$NetBSD: srcs.inc,v 1.4 2012/07/26 19:58:40 christos Exp $

CRYPTOINCS= \
	aes.inc asn1.inc bf.inc bio.inc bn.inc buffer.inc cast.inc cmac.inc \
	cms.inc comp.inc conf.inc crypto.inc des.inc dh.inc dsa.inc dso.inc \
	ec.inc ecdh.inc ecdsa.inc engine.inc err.inc evp.inc hmac.inc \
	krb5.inc lhash.inc modes.inc camellia.inc \
	md2.inc md4.inc md5.inc objects.inc ocsp.inc pem.inc \
	pkcs12.inc pkcs7.inc pqueue.inc rand.inc rc2.inc rc4.inc ripemd.inc \
	rsa.inc sha.inc srp.inc stack.inc store.inc txt_db.inc ts.inc \
	ui.inc x509.inc x509v3.inc idea.inc mdc2.inc whrlpool.inc modes.inc \
	seed.inc

# patented algorithms - see ../libcrypto_*
CRYPTOINCS+=	rc5.inc

CRYPTOINCS+=	man.inc

.if exists(${.CURDIR}/arch/${MACHINE_ARCH})
CRYPTO_MACHINE_CPU?=    ${MACHINE_ARCH}
.else
CRYPTO_MACHINE_CPU?=    ${MACHINE_CPU}
.endif

.for cryptoinc in ${CRYPTOINCS}
.if exists(${.CURDIR}/arch/${CRYPTO_MACHINE_CPU}/${cryptoinc})
.include "${.CURDIR}/arch/${CRYPTO_MACHINE_CPU}/${cryptoinc}"
.else
.include "${cryptoinc}"
.endif
.endfor
