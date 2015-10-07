print '.\\"	$NetBSD: libcrypto.pl,v 1.2 2011/06/13 18:53:39 spz Exp $' . "\n";
print '.\\"' . "\n";
while (<>) {
	next if (/\$RCSfile/);
	next if (/\$Log/);

	if (/\.SH "SYNOPSIS"/i) {
		print '.SH "LIBRARY"' . "\n";
		print 'libcrypto, -lcrypto' . "\n";
	}

	s/ICA.pl\\fR\\\|\(1\)/Iopenssl_CA.pl\\fR\\|(1)/g;
	s/Iasn1parse\\fR\\\|\(1\)/Iopenssl_asn1parse\\fR\\|(1)/g;
	s/Ica\\fR\\\|\(1\)/Iopenssl_ca\\fR\\|(1)/g;
	s/Iciphers\\fR\\\|\(1\)/Iopenssl_ciphers\\fR\\|(1)/g;
	s/Icrl\\fR\\\|\(1\)/Iopenssl_crl\\fR\\|(1)/g;
	s/Icrl2pkcs7\\fR\\\|\(1\)/Iopenssl_crl2pkcs7\\fR\\|(1)/g;
	s/Idgst\\fR\\\|\(1\)/Iopenssl_dgst\\fR\\|(1)/g;
	s/Idhparam\\fR\\\|\(1\)/Iopenssl_dhparam\\fR\\|(1)/g;
	s/Idsa\\fR\\\|\(1\)/Iopenssl_dsa\\fR\\|(1)/g;
	s/Idsaparam\\fR\\\|\(1\)/Iopenssl_dsaparam\\fR\\|(1)/g;
	s/Ienc\\fR\\\|\(1\)/Iopenssl_enc\\fR\\|(1)/g;
	s/Igendsa\\fR\\\|\(1\)/Iopenssl_gendsa\\fR\\|(1)/g;
	s/Igenrsa\\fR\\\|\(1\)/Iopenssl_genrsa\\fR\\|(1)/g;
	s/Inseq\\fR\\\|\(1\)/Iopenssl_nseq\\fR\\|(1)/g;
	s/Ipasswd\\fR\\\|\(1\)/Iopenssl_passwd\\fR\\|(1)/g;
	s/Ipkcs12\\fR\\\|\(1\)/Iopenssl_pkcs12\\fR\\|(1)/g;
	s/Ipkcs7\\fR\\\|\(1\)/Iopenssl_pkcs7\\fR\\|(1)/g;
	s/Ipkcs8\\fR\\\|\(1\)/Iopenssl_pkcs8\\fR\\|(1)/g;
	s/Irand\\fR\\\|\(1\)/Iopenssl_rand\\fR\\|(1)/g;
	s/Ireq\\fR\\\|\(1\)/Iopenssl_req\\fR\\|(1)/g;
	s/Irsa\\fR\\\|\(1\)/Iopenssl_rsa\\fR\\|(1)/g;
	s/Irsautl\\fR\\\|\(1\)/Iopenssl_rsautl\\fR\\|(1)/g;
	s/Is_client\\fR\\\|\(1\)/Iopenssl_s_client\\fR\\|(1)/g;
	s/Is_server\\fR\\\|\(1\)/Iopenssl_s_server\\fR\\|(1)/g;
	s/Isess_id\\fR\\\|\(1\)/Iopenssl_sess_id\\fR\\|(1)/g;
	s/Ismime\\fR\\\|\(1\)/Iopenssl_smime\\fR\\|(1)/g;
	s/Ispeed\\fR\\\|\(1\)/Iopenssl_speed\\fR\\|(1)/g;
	s/Ispkac\\fR\\\|\(1\)/Iopenssl_spkac\\fR\\|(1)/g;
	s/Iverify\\fR\\\|\(1\)/Iopenssl_verify\\fR\\|(1)/g;
	s/Iversion\\fR\\\|\(1\)/Iopenssl_version\\fR\\|(1)/g;
	s/Ix509\\fR\\\|\(1\)/Iopenssl_x509\\fR\\|(1)/g;
	s/Ibio\\fR\\\|\(3\)/Iopenssl_bio\\fR\\|(3)/g;
	s/Iblowfish\\fR\\\|\(3\)/Iopenssl_blowfish\\fR\\|(3)/g;
	s/Ibn\\fR\\\|\(3\)/Iopenssl_bn\\fR\\|(3)/g;
	s/Ibn_internal\\fR\\\|\(3\)/Iopenssl_bn_internal\\fR\\|(3)/g;
	s/Ibuffer\\fR\\\|\(3\)/Iopenssl_buffer\\fR\\|(3)/g;
	s/Ides\\fR\\\|\(3\)/Iopenssl_des\\fR\\|(3)/g;
	s/Idh\\fR\\\|\(3\)/Iopenssl_dh\\fR\\|(3)/g;
	s/Idsa\\fR\\\|\(3\)/Iopenssl_dsa\\fR\\|(3)/g;
	s/Ierr\\fR\\\|\(3\)/Iopenssl_err\\fR\\|(3)/g;
	s/Ievp\\fR\\\|\(3\)/Iopenssl_evp\\fR\\|(3)/g;
	s/Ihmac\\fR\\\|\(3\)/Iopenssl_hmac\\fR\\|(3)/g;
	s/Ilhash\\fR\\\|\(3\)/Iopenssl_lhash\\fR\\|(3)/g;
	s/Imd5\\fR\\\|\(3\)/Iopenssl_md5\\fR\\|(3)/g;
	s/Imdc2\\fR\\\|\(3\)/Iopenssl_mdc2\\fR\\|(3)/g;
	s/Irand\\fR\\\|\(3\)/Iopenssl_rand\\fR\\|(3)/g;
	s/Irc4\\fR\\\|\(3\)/Iopenssl_rc4\\fR\\|(3)/g;
	s/Iripemd\\fR\\\|\(3\)/Iopenssl_ripemd\\fR\\|(3)/g;
	s/Irsa\\fR\\\|\(3\)/Iopenssl_rsa\\fR\\|(3)/g;
	s/Isha\\fR\\\|\(3\)/Iopenssl_sha\\fR\\|(3)/g;
	s/Ithreads\\fR\\\|\(3\)/Iopenssl_threads\\fR\\|(3)/g;
	s/Iconfig\\fR\\\|\(5\)/Iopenssl.cnf\\fR\\|(5)/g;

	s,/usr/local/ssl/lib/openssl.cnf,/etc/openssl/openssl.cnf      ,g;

	print;
}
