# Id: Makefile,v 1.1 2003/06/04 00:27:52 marka Exp 

PERL = /usr/local/bin/perl -w

V301_NORM_DIR = data/unicode-3.0.0
V310_NORM_DIR = data/unicode-3.1.0
V320_NORM_DIR = data/unicode-3.2.0
NORM_GENERATOR = ./generate_normalize_data.pl

NAMEPREP_DIR  = data/nameprep
NAMEPREP_FILES = \
	$(NAMEPREP_DIR)/nameprep.rfc3491.map \
	$(NAMEPREP_DIR)/nameprep.rfc3491.prohibited \
	$(NAMEPREP_DIR)/nameprep.rfc3491.unassigned \
	$(NAMEPREP_DIR)/nameprep.rfc3491.bidi
NAMEPREP_GENERATOR = ./generate_nameprep_data.pl
NAMEPREP_VERSIONS = rfc3491

all: normalize nameprep

normalize: ../lib/unicodedata_320.c

nameprep: ../lib/nameprepdata.c

../lib/unicodedata_301.c: $(NORM_GENERATOR)
	-rm -f $@.tmp
	$(PERL) $(NORM_GENERATOR) -prefix v301_ -dir $(V301_NORM_DIR) \
		> $@.tmp && mv $@.tmp $@

../lib/unicodedata_310.c: $(NORM_GENERATOR)
	-rm -f $@.tmp
	$(PERL) $(NORM_GENERATOR) -prefix v310_ -dir $(V310_NORM_DIR) \
		> $@.tmp && mv $@.tmp $@

../lib/unicodedata_320.c: $(NORM_GENERATOR)
	-rm -f $@.tmp
	$(PERL) $(NORM_GENERATOR) -prefix v320_ -dir $(V320_NORM_DIR) \
		> $@.tmp && mv $@.tmp $@

../lib/nameprepdata.c: $(NAMEPREP_GENERATOR) $(NAMEPREP_FILES)
	-rm -f $@.tmp
	$(PERL) $(NAMEPREP_GENERATOR) -dir $(NAMEPREP_DIR) \
		$(NAMEPREP_VERSIONS) > $@.tmp && mv $@.tmp $@
