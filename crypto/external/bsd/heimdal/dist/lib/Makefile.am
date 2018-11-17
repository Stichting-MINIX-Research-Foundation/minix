# Id

include $(top_srcdir)/Makefile.am.common

if LIBEDIT
dir_editline = libedit
endif
if OTP
dir_otp = otp
endif
if DCE
dir_dce = kdfs
endif
if COM_ERR
dir_com_err = com_err
endif
if !SQLITE3
dir_sqlite = sqlite
endif
if MAINTAINER_MODE
dir_sqlite = sqlite
endif
if !NO_AFS
dir_afs = kafs
endif

SUBDIRS = \
	roken \
	base \
	vers \
	$(dir_editline) \
	$(dir_com_err) \
	sl \
	wind \
	asn1 \
	$(dir_sqlite) \
	hcrypto \
	ipc \
	hx509 \
	krb5 \
	ntlm \
	$(dir_afs) \
	gssapi \
	hdb \
	kadm5 \
	$(dir_otp) \
	$(dir_dce)

EXTRA_DIST = NTMakefile heimdal
