#	$NetBSD: man.inc,v 1.6 2016/10/14 16:09:43 spz Exp $

.PATH:	${.CURDIR}/man

MAN+=	openssl.1            openssl_errstr.1     openssl_rsautl.1	\
	openssl_CA.pl.1      openssl_gendsa.1     openssl_s_client.1	\
	openssl_asn1parse.1  openssl_genpkey.1    openssl_s_server.1	\
	openssl_c_rehash.1   openssl_genrsa.1     openssl_s_time.1	\
	openssl_ca.1         openssl_nseq.1       openssl_sess_id.1	\
	openssl_ciphers.1    openssl_ocsp.1       openssl_smime.1	\
	openssl_cms.1        openssl_passwd.1     openssl_speed.1	\
	openssl_crl.1        openssl_pkcs12.1     openssl_spkac.1	\
	openssl_crl2pkcs7.1  openssl_pkcs7.1      openssl_ts.1		\
	openssl_dgst.1       openssl_pkcs8.1      openssl_tsget.1	\
	openssl_dhparam.1    openssl_pkey.1       openssl_verify.1	\
	openssl_dsa.1        openssl_pkeyparam.1  openssl_version.1	\
	openssl_dsaparam.1   openssl_pkeyutl.1    openssl_x509.1	\
	openssl_ec.1         openssl_rand.1       openssl_x509v3_config.1 \
	openssl_ecparam.1    openssl_req.1        openssl_enc.1		\
	openssl_rsa.1

MAN+=	ASN1_OBJECT_new.3                RSA_sign_ASN1_OCTET_STRING.3	\
	ASN1_STRING_length.3             RSA_size.3			\
	ASN1_STRING_new.3                SMIME_read_CMS.3		\
	ASN1_STRING_print_ex.3           SMIME_read_PKCS7.3		\
	ASN1_generate_nconf.3            SMIME_write_CMS.3		\
	BIO_ctrl.3                       SMIME_write_PKCS7.3		\
	BIO_f_base64.3                   SSL_CIPHER_get_name.3		\
	BIO_f_buffer.3                   SSL_COMP_add_compression_method.3 \
	BIO_f_cipher.3                   SSL_CTX_add_extra_chain_cert.3	\
	BIO_f_md.3                       SSL_CTX_add_session.3		\
	BIO_f_null.3                     SSL_CTX_ctrl.3			\
	BIO_f_ssl.3                      SSL_CTX_flush_sessions.3	\
	BIO_find_type.3                  SSL_CTX_free.3			\
	BIO_new.3                        SSL_CTX_get_ex_new_index.3	\
	BIO_new_CMS.3                    SSL_CTX_get_verify_mode.3	\
	BIO_push.3                       SSL_CTX_load_verify_locations.3 \
	BIO_read.3                       SSL_CTX_new.3			\
	BIO_s_accept.3                   SSL_CTX_sess_number.3		\
	BIO_s_bio.3                      SSL_CTX_sess_set_cache_size.3	\
	BIO_s_connect.3                  SSL_CTX_sess_set_get_cb.3	\
	BIO_s_fd.3                       SSL_CTX_sessions.3		\
	BIO_s_file.3                     SSL_CTX_set_cert_store.3	\
	BIO_s_mem.3                      SSL_CTX_set_cert_verify_callback.3 \
	BIO_s_null.3                     SSL_CTX_set_cipher_list.3	\
	BIO_s_socket.3                   SSL_CTX_set_client_CA_list.3	\
	BIO_set_callback.3               SSL_CTX_set_client_cert_cb.3	\
	BIO_should_retry.3               SSL_CTX_set_default_passwd_cb.3 \
	BN_BLINDING_new.3                SSL_CTX_set_generate_session_id.3 \
	BN_CTX_new.3                     SSL_CTX_set_info_callback.3	\
	BN_CTX_start.3                   SSL_CTX_set_max_cert_list.3	\
	BN_add.3                         SSL_CTX_set_mode.3		\
	BN_add_word.3                    SSL_CTX_set_msg_callback.3	\
	BN_bn2bin.3                      SSL_CTX_set_options.3		\
	BN_cmp.3                         SSL_CTX_set_psk_client_callback.3 \
	BN_copy.3                        SSL_CTX_set_quiet_shutdown.3	\
	BN_generate_prime.3              SSL_CTX_set_read_ahead.3	\
	BN_mod_inverse.3                 SSL_CTX_set_session_cache_mode.3 \
	BN_mod_mul_montgomery.3          SSL_CTX_set_session_id_context.3 \
	BN_mod_mul_reciprocal.3          SSL_CTX_set_ssl_version.3	\
	BN_new.3                         SSL_CTX_set_timeout.3		\
	BN_num_bytes.3                   SSL_CTX_set_tlsext_status_cb.3	\
	BN_rand.3                        SSL_CTX_set_tlsext_ticket_key_cb.3 \
	BN_set_bit.3                     SSL_CTX_set_tmp_dh_callback.3	\
	BN_swap.3                        SSL_CTX_set_tmp_rsa_callback.3	\
	BN_zero.3                        SSL_CTX_set_verify.3		\
	CMS_add0_cert.3                  SSL_CTX_use_certificate.3	\
	CMS_add1_recipient_cert.3        SSL_CTX_use_psk_identity_hint.3 \
	CMS_add1_signer.3                SSL_SESSION_free.3		\
	CMS_compress.3                   SSL_SESSION_get_ex_new_index.3	\
	CMS_decrypt.3                    SSL_SESSION_get_time.3		\
	CMS_encrypt.3                    SSL_accept.3			\
	CMS_final.3                      SSL_alert_type_string.3	\
	CMS_get0_RecipientInfos.3        SSL_clear.3			\
	CMS_get0_SignerInfos.3           SSL_connect.3			\
	CMS_get0_type.3                  SSL_do_handshake.3		\
	CMS_get1_ReceiptRequest.3        SSL_free.3			\
	CMS_sign.3                       SSL_get_SSL_CTX.3		\
	CMS_sign_receipt.3               SSL_get_ciphers.3		\
	CMS_uncompress.3                 SSL_get_client_CA_list.3	\
	CMS_verify.3                     SSL_get_current_cipher.3	\
	CMS_verify_receipt.3             SSL_get_default_timeout.3	\
	CONF_modules_free.3              SSL_get_error.3		\
	CONF_modules_load_file.3         SSL_get_ex_data_X509_STORE_CTX_idx.3 \
	CRYPTO_set_ex_data.3             SSL_get_ex_new_index.3		\
	DH_generate_key.3                SSL_get_fd.3			\
	DH_generate_parameters.3         SSL_get_peer_cert_chain.3	\
	DH_get_ex_new_index.3            SSL_get_peer_certificate.3	\
	DH_new.3                         SSL_get_psk_identity.3		\
	DH_set_method.3                  SSL_get_rbio.3			\
	DH_size.3                        SSL_get_session.3		\
	DSA_SIG_new.3                    SSL_get_verify_result.3	\
	DSA_do_sign.3                    SSL_get_version.3		\
	DSA_dup_DH.3                     SSL_library_init.3		\
	DSA_generate_key.3               SSL_load_client_CA_file.3	\
	DSA_generate_parameters.3        SSL_new.3			\
	DSA_get_ex_new_index.3           SSL_pending.3			\
	DSA_new.3                        SSL_read.3			\
	DSA_set_method.3                 SSL_rstate_string.3		\
	DSA_sign.3                       SSL_session_reused.3		\
	DSA_size.3                       SSL_set_bio.3			\
	ERR_GET_LIB.3                    SSL_set_connect_state.3	\
	ERR_clear_error.3                SSL_set_fd.3			\
	ERR_error_string.3               SSL_set_session.3		\
	ERR_get_error.3                  SSL_set_shutdown.3		\
	ERR_load_crypto_strings.3        SSL_set_verify_result.3	\
	ERR_load_strings.3               SSL_shutdown.3			\
	ERR_print_errors.3               SSL_state_string.3		\
	ERR_put_error.3                  SSL_want.3			\
	ERR_remove_state.3               SSL_write.3			\
	ERR_set_mark.3                   X509_NAME_ENTRY_get_object.3	\
	EVP_BytesToKey.3                 X509_NAME_add_entry_by_txt.3	\
	EVP_DigestInit.3                 X509_NAME_get_index_by_NID.3	\
	EVP_DigestSignInit.3             X509_NAME_print_ex.3		\
	EVP_DigestVerifyInit.3           X509_STORE_CTX_get_error.3	\
	EVP_EncodeInit.3                 X509_STORE_CTX_get_ex_new_index.3 \
	EVP_EncryptInit.3                X509_STORE_CTX_new.3		\
	EVP_OpenInit.3                   X509_STORE_CTX_set_verify_cb.3	\
	EVP_PKEY_CTX_ctrl.3              X509_STORE_set_verify_cb_func.3 \
	EVP_PKEY_CTX_new.3               X509_VERIFY_PARAM_set_flags.3	\
	EVP_PKEY_cmp.3                   X509_new.3			\
	EVP_PKEY_decrypt.3               X509_verify_cert.3		\
	EVP_PKEY_derive.3                crypto.3			\
	EVP_PKEY_encrypt.3               d2i_ASN1_OBJECT.3		\
	EVP_PKEY_get_default_digest.3    d2i_CMS_ContentInfo.3		\
	EVP_PKEY_keygen.3                d2i_DHparams.3			\
	EVP_PKEY_new.3                   d2i_DSAPublicKey.3		\
	EVP_PKEY_print_private.3         d2i_ECPrivateKey.3		\
	EVP_PKEY_set1_RSA.3              d2i_PKCS8PrivateKey.3		\
	EVP_PKEY_sign.3                  d2i_RSAPublicKey.3		\
	EVP_PKEY_verify.3                d2i_SSL_SESSION.3		\
	EVP_PKEY_verify_recover.3        d2i_X509.3			\
	EVP_SealInit.3                   d2i_X509_ALGOR.3		\
	EVP_SignInit.3                   d2i_X509_CRL.3			\
	EVP_VerifyInit.3                 d2i_X509_NAME.3		\
	OBJ_nid2obj.3                    d2i_X509_REQ.3			\
	OPENSSL_Applink.3                d2i_X509_SIG.3			\
	OPENSSL_VERSION_NUMBER.3         i2d_CMS_bio_stream.3		\
	OPENSSL_config.3                 i2d_PKCS7_bio_stream.3		\
	OPENSSL_ia32cap.3                lh_stats.3			\
	OPENSSL_load_builtin_modules.3   openssl_bio.3			\
	OpenSSL_add_all_algorithms.3     openssl_blowfish.3		\
	PEM_write_bio_CMS_stream.3       openssl_bn.3			\
	PEM_write_bio_PKCS7_stream.3     openssl_bn_internal.3		\
	PKCS12_create.3                  openssl_buffer.3		\
	PKCS12_parse.3                   openssl_des.3			\
	PKCS7_decrypt.3                  openssl_dh.3			\
	PKCS7_encrypt.3                  openssl_dsa.3			\
	PKCS7_sign.3                     openssl_ecdsa.3		\
	PKCS7_sign_add_signer.3          openssl_engine.3		\
	PKCS7_verify.3                   openssl_err.3			\
	RAND_add.3                       openssl_evp.3			\
	RAND_bytes.3                     openssl_hmac.3			\
	RAND_cleanup.3                   openssl_lhash.3		\
	RAND_egd.3                       openssl_md5.3			\
	RAND_load_file.3                 openssl_mdc2.3			\
	RAND_set_rand_method.3           openssl_pem.3			\
	RSA_blinding_on.3                openssl_rand.3			\
	RSA_check_key.3                  openssl_rc4.3			\
	RSA_generate_key.3               openssl_ripemd.3		\
	RSA_get_ex_new_index.3           openssl_rsa.3			\
	RSA_new.3                        openssl_sha.3			\
	RSA_padding_add_PKCS1_type_1.3   openssl_threads.3		\
	RSA_print.3                      openssl_ui.3			\
	RSA_private_encrypt.3            openssl_ui_compat.3		\
	RSA_public_encrypt.3             openssl_x509.3			\
	RSA_set_method.3                 ssl.3				\
	RSA_sign.3


MAN+=	openssl.cnf.5

#MAN+=	des_modes.7
