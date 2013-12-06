visrcdir= @srcdir@/..

ACLOCAL_AMFLAGS = -I m4

if BUNDLED_DB
    DB_C = $(visrcdir)/common/vi_db1.c
    DB_SRCS = \
	$(visrcdir)/db.1.85/recno/rec_delete.c \
	$(visrcdir)/db.1.85/recno/rec_get.c \
	$(visrcdir)/db.1.85/recno/rec_put.c \
	$(visrcdir)/db.1.85/recno/rec_utils.c \
	$(visrcdir)/db.1.85/recno/rec_open.c \
	$(visrcdir)/db.1.85/recno/rec_seq.c \
	$(visrcdir)/db.1.85/recno/rec_close.c \
	$(visrcdir)/db.1.85/recno/rec_search.c \
	$(visrcdir)/db.1.85/db/db.c \
	$(visrcdir)/db.1.85/btree/bt_debug.c \
	$(visrcdir)/db.1.85/btree/bt_get.c \
	$(visrcdir)/db.1.85/btree/bt_open.c \
	$(visrcdir)/db.1.85/btree/bt_delete.c \
	$(visrcdir)/db.1.85/btree/bt_put.c \
	$(visrcdir)/db.1.85/btree/bt_overflow.c \
	$(visrcdir)/db.1.85/btree/bt_close.c \
	$(visrcdir)/db.1.85/btree/bt_page.c \
	$(visrcdir)/db.1.85/btree/bt_split.c \
	$(visrcdir)/db.1.85/btree/bt_search.c \
	$(visrcdir)/db.1.85/btree/bt_utils.c \
	$(visrcdir)/db.1.85/btree/bt_seq.c \
	$(visrcdir)/db.1.85/btree/bt_conv.c \
	$(visrcdir)/db.1.85/mpool/mpool.c \
	$(visrcdir)/db.1.85/hash/hash_bigkey.c \
	$(visrcdir)/db.1.85/hash/hash_log2.c \
	$(visrcdir)/db.1.85/hash/hash.c \
	$(visrcdir)/db.1.85/hash/hash_func.c \
	$(visrcdir)/db.1.85/hash/hash_page.c \
	$(visrcdir)/db.1.85/hash/hash_buf.c \
	$(visrcdir)/db.1.85/hash/hsearch.c
else
    DB_C = $(visrcdir)/common/vi_db.c
endif

lib_LTLIBRARIES = libvi.la
libvi_la_SOURCES = \
	$(DB_SRCS) \
	$(visrcdir)/common/api.c \
	$(visrcdir)/common/conv.c \
	$(visrcdir)/common/conv.h \
	$(visrcdir)/common/cut.c \
	$(visrcdir)/common/delete.c \
	$(visrcdir)/common/args.h \
	$(visrcdir)/common/common.h \
	$(visrcdir)/common/cut.h \
	$(visrcdir)/common/vi_db.h \
	$(visrcdir)/common/exf.h \
	$(visrcdir)/common/gs.h \
	$(visrcdir)/common/key.h \
	$(visrcdir)/common/log.h \
	$(visrcdir)/common/mark.h \
	$(visrcdir)/common/mem.h \
	$(visrcdir)/common/multibyte.h \
	$(visrcdir)/common/msg.h \
	$(visrcdir)/common/options.h \
	$(visrcdir)/common/options_def.h \
	$(visrcdir)/common/screen.h \
	$(visrcdir)/common/seq.h \
	$(visrcdir)/common/util.h \
	$(visrcdir)/ex/ex.c \
	$(visrcdir)/ex/ex_abbrev.c \
	$(visrcdir)/ex/ex_append.c \
	$(visrcdir)/ex/ex_args.c \
	$(visrcdir)/ex/ex_argv.c \
	$(visrcdir)/ex/ex_at.c \
	$(visrcdir)/ex/ex_bang.c \
	$(visrcdir)/ex/ex_cd.c \
	$(visrcdir)/ex/ex_cmd.c \
	$(visrcdir)/ex/ex_cscope.c \
	$(visrcdir)/ex/ex_delete.c \
	$(visrcdir)/ex/ex_display.c \
	$(visrcdir)/ex/ex_edit.c \
	$(visrcdir)/ex/ex_equal.c \
	$(visrcdir)/ex/ex_file.c \
	$(visrcdir)/ex/ex_filter.c \
	$(visrcdir)/ex/ex_global.c \
	$(visrcdir)/ex/ex_init.c \
	$(visrcdir)/ex/ex_join.c \
	$(visrcdir)/ex/ex_map.c \
	$(visrcdir)/ex/ex_mark.c \
	$(visrcdir)/ex/ex_mkexrc.c \
	$(visrcdir)/ex/ex_move.c \
	$(visrcdir)/ex/ex_open.c \
	$(visrcdir)/ex/ex_perl.c \
	$(visrcdir)/ex/ex_preserve.c \
	$(visrcdir)/ex/ex_print.c \
	$(visrcdir)/ex/ex_put.c \
	$(visrcdir)/ex/ex_quit.c \
	$(visrcdir)/ex/ex_read.c \
	$(visrcdir)/ex/ex_screen.c \
	$(visrcdir)/ex/ex_script.c \
	$(visrcdir)/ex/ex_set.c \
	$(visrcdir)/ex/ex_shell.c \
	$(visrcdir)/ex/ex_shift.c \
	$(visrcdir)/ex/ex_source.c \
	$(visrcdir)/ex/ex_stop.c \
	$(visrcdir)/ex/ex_subst.c \
	$(visrcdir)/ex/ex_tag.c \
	$(visrcdir)/ex/ex_tcl.c \
	$(visrcdir)/ex/ex_txt.c \
	$(visrcdir)/ex/ex_undo.c \
	$(visrcdir)/ex/ex_usage.c \
	$(visrcdir)/ex/ex_util.c \
	$(visrcdir)/ex/ex_version.c \
	$(visrcdir)/ex/ex_visual.c \
	$(visrcdir)/ex/ex_write.c \
	$(visrcdir)/ex/ex_yank.c \
	$(visrcdir)/ex/ex_z.c \
	$(visrcdir)/ex/ex.h \
	$(visrcdir)/ex/ex_def.h \
	$(visrcdir)/ex/script.h \
	$(visrcdir)/ex/tag.h \
	$(visrcdir)/ex/version.h \
	$(visrcdir)/common/exf.c \
	$(visrcdir)/vi/getc.c \
	$(visrcdir)/vi/vi.h \
	$(visrcdir)/common/gs.c \
	$(visrcdir)/common/key.c \
	$(DB_C) \
	$(visrcdir)/common/main.c \
	$(visrcdir)/common/mark.c \
	$(visrcdir)/common/msg.c \
	$(visrcdir)/common/options.c \
	$(visrcdir)/common/options_f.c \
	$(visrcdir)/common/put.c \
	$(visrcdir)/common/recover.c \
	$(visrcdir)/common/screen.c \
	$(visrcdir)/common/search.c \
	$(visrcdir)/common/seq.c \
	$(visrcdir)/common/trace.c \
	$(visrcdir)/common/util.c \
	$(visrcdir)/common/util2.c \
	$(visrcdir)/vi/v_at.c \
	$(visrcdir)/vi/v_ch.c \
	$(visrcdir)/vi/v_cmd.c \
	$(visrcdir)/vi/v_delete.c \
	$(visrcdir)/vi/v_event.c \
	$(visrcdir)/vi/v_ex.c \
	$(visrcdir)/vi/v_increment.c \
	$(visrcdir)/vi/v_init.c \
	$(visrcdir)/vi/v_itxt.c \
	$(visrcdir)/vi/v_left.c \
	$(visrcdir)/vi/v_mark.c \
	$(visrcdir)/vi/v_match.c \
	$(visrcdir)/vi/v_paragraph.c \
	$(visrcdir)/vi/v_put.c \
	$(visrcdir)/vi/v_redraw.c \
	$(visrcdir)/vi/v_replace.c \
	$(visrcdir)/vi/v_right.c \
	$(visrcdir)/vi/v_screen.c \
	$(visrcdir)/vi/v_scroll.c \
	$(visrcdir)/vi/v_search.c \
	$(visrcdir)/vi/v_section.c \
	$(visrcdir)/vi/v_sentence.c \
	$(visrcdir)/vi/v_status.c \
	$(visrcdir)/vi/v_txt.c \
	$(visrcdir)/vi/v_ulcase.c \
	$(visrcdir)/vi/v_undo.c \
	$(visrcdir)/vi/v_util.c \
	$(visrcdir)/vi/v_word.c \
	$(visrcdir)/vi/v_xchar.c \
	$(visrcdir)/vi/v_yank.c \
	$(visrcdir)/vi/v_z.c \
	$(visrcdir)/vi/v_zexit.c \
	$(visrcdir)/vi/vi.c \
	$(visrcdir)/vi/vs_line.c \
	$(visrcdir)/vi/vs_msg.c \
	$(visrcdir)/vi/vs_refresh.c \
	$(visrcdir)/vi/vs_relative.c \
	$(visrcdir)/vi/vs_smap.c \
	$(visrcdir)/vi/vs_split.c
EXTRA_libvi_la_SOURCES = \
	$(visrcdir)/regex/regcomp.c \
	$(visrcdir)/regex/regerror.c \
	$(visrcdir)/regex/regexec.c \
	$(visrcdir)/regex/regfree.c \
	$(visrcdir)/regex/cclass.h \
	$(visrcdir)/regex/cname.h \
	$(visrcdir)/regex/regex.h \
	$(visrcdir)/regex/regex2.h \
	$(visrcdir)/regex/utils.h \
	$(visrcdir)/common/vi_db.c \
	$(visrcdir)/common/vi_db1.c \
	$(visrcdir)/common/dldb.c \
	$(visrcdir)/common/log.c \
	$(visrcdir)/common/log1.c \
	$(visrcdir)/common/log4.c \
	$(visrcdir)/clib/bsearch.c \
	$(visrcdir)/clib/env.c \
	$(visrcdir)/clib/gethostname.c \
	$(visrcdir)/clib/iswblank.c \
	$(visrcdir)/clib/memchr.c \
	$(visrcdir)/clib/memset.c \
	$(visrcdir)/clib/mkstemp.c \
	$(visrcdir)/clib/mmap.c \
	$(visrcdir)/clib/snprintf.c \
	$(visrcdir)/clib/strdup.c \
	$(visrcdir)/clib/strpbrk.c \
	$(visrcdir)/clib/strsep.c \
	$(visrcdir)/clib/strtol.c \
	$(visrcdir)/clib/strtoul.c \
	$(visrcdir)/clib/vsnprintf.c \
	$(visrcdir)/common/pthread.c \
	$(visrcdir)/common/vi_auto.c \
	$(visrcdir)/common/vi_rec.c \
	$(visrcdir)/perl_api/perl.xs \
	$(visrcdir)/perl_api/perlsfio.c \
	$(visrcdir)/tcl_api/tcl.c
# Is this the way to do it ?
libvi_la_DEPENDENCIES = @LTLIBOBJS@
libvi_la_LIBADD = @LTLIBOBJS@

bin_PROGRAMS = @vi_programs@ @vi_ipc@
EXTRA_PROGRAMS = vi vi-ipc vi-motif vi-gtk

vi_SOURCES = \
	$(visrcdir)/cl/cl.h \
	$(visrcdir)/cl/cl_bsd.c \
	$(visrcdir)/cl/cl_funcs.c \
	$(visrcdir)/cl/cl_main.c \
	$(visrcdir)/cl/cl_read.c \
	$(visrcdir)/cl/cl_screen.c \
	$(visrcdir)/cl/cl_term.c \
	$(visrcdir)/common/nothread.c
vi_LDADD = libvi.la @perllibs@ @CURSLIBS@
vi_CPPFLAGS = $(AM_CPPFLAGS) @CURSCPPFLAGS@ @perlldflags@
vi_LDFLAGS = @CURSLDFLAGS@ @perlldflags@

vi_ipc_SOURCES = \
	$(visrcdir)/ip/ip_funcs.c \
	$(visrcdir)/ip/ip_main.c \
	$(visrcdir)/ip/ip_read.c \
	$(visrcdir)/ip/ip_screen.c \
	$(visrcdir)/ipc/ip_send.c \
	$(visrcdir)/ipc/ip.h \
	$(visrcdir)/ip/ip_term.c 
vi_ipc_LDADD = libvi.la @perllibs@ @IPCOBJS@
vi_ipc_LDFLAGS = @perlldflags@
vi_ipc_DEPENDENCIES = @IPCOBJS@

vi_gtk_SOURCES = \
	$(visrcdir)/common/util2.c \
	$(visrcdir)/ipc/ip_run.c \
	$(visrcdir)/ipc/ip_send.c \
	$(visrcdir)/ipc/ip_trans.c \
	$(visrcdir)/ipc/ipc_cmd.c \
	$(visrcdir)/ipc/ipc_method.c \
	$(visrcdir)/gtk/gtk_main.c \
	$(visrcdir)/gtk/gtkvi.c \
	$(visrcdir)/gtk/gtkviscreen.c \
	$(visrcdir)/gtk/gtkviwindow.c
vi_gtk_LDADD = @GTKLIBS@ @LIBOBJS@
vi_gtk_CFLAGS = @GTKINCS@ @XINCS@

vi_motif_SOURCES = \
	$(visrcdir)/ipc/ip_run.c \
	$(visrcdir)/ipc/ip_send.c \
	$(visrcdir)/ipc/ip_trans.c \
	$(visrcdir)/ipc/ipc_cmd.c \
	$(visrcdir)/ipc/ipc_method.c \
	$(visrcdir)/motif/m_main.c \
	$(visrcdir)/motif/m_cde.c \
	$(visrcdir)/common/trace.c \
	$(visrcdir)/motif_l/m_copypaste.c \
	$(visrcdir)/motif_l/m_func.c \
	$(visrcdir)/motif_l/m_menu.c \
	$(visrcdir)/motif_l/m_options.c \
	$(visrcdir)/motif_l/m_prompt.c \
	$(visrcdir)/motif_l/m_ruler.c \
	$(visrcdir)/motif_l/m_search.c \
	$(visrcdir)/motif_l/m_tags.c \
	$(visrcdir)/motif_l/m_util.c \
	$(visrcdir)/motif_l/m_vi.c \
	$(visrcdir)/motif_l/xtabbed.c
vi_motif_LDADD = @MOTIFLIBS@ @LIBOBJS@
vi_motif_CFLAGS = @XINCS@

VI=-DVI=\"$(bindir)/`echo vi-ipc | sed '$(transform)'`\"
AM_CFLAGS=	$(OPTFLAG)
if BUNDLED_DB
AM_CPPFLAGS=	 -D__DBINTERFACE_PRIVATE -I$(visrcdir)/db.1.85/include \
		 -D__REGEX_PRIVATE -I. -I$(visrcdir)/include $(VI)
else
AM_CPPFLAGS=	 -D__REGEX_PRIVATE -I. -I$(visrcdir)/include $(VI)
endif
PERL=	@vi_cv_path_perl@
PERLLIB=@vi_cv_perllib@
SHRPENV=@shrpenv@

perl.c: $(visrcdir)/perl_api/perl.xs $(visrcdir)/perl_api/typemap
	echo "#define _PATH_PERLSCRIPTS \"$(datadir)/vi/perl\"" > $@
	$(PERL) -e 'unless ($$] >= 5.005) {' \
	       	-e   'for (qw(incgv sv_undef errgv na)) {' \
		-e     'print "#define PL_$$_ $$_\n" }} ' >> $@
	$(PERL) $(PERLLIB)/ExtUtils/xsubpp -typemap \
	    $(PERLLIB)/ExtUtils/typemap $(visrcdir)/perl_api/perl.xs >> $@
	($(PERL) -ne 'print "sub $$1 {\$$curscr->$$1(\@_)}\n"' \
	          -e   'if /newXS\("VI::([^\":]*)"/;' $@ ; echo "1;") > VI.pm
perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -std VI

chmod=	@vi_cv_path_chmod@
cp=	@vi_cv_path_cp@
ln=	@vi_cv_path_ln@
mkdir=	@vi_cv_path_mkdir@
rm=	@vi_cv_path_rm@
strip=	@vi_cv_path_strip@

dmode=	755
emode=	555
fmode=	444

install-exec-local:
	cd $(DESTDIR)$(bindir) && $(rm) -f `echo ex | sed '$(transform)'`
	cd $(DESTDIR)$(bindir) && $(rm) -f `echo view | sed '$(transform)'`
	cd $(DESTDIR)$(bindir) && $(ln) \
		`echo vi | sed '$(transform)'` `echo ex | sed '$(transform)'`
	cd $(DESTDIR)$(bindir) && $(ln) \
		`echo vi | sed '$(transform)'` `echo view | sed '$(transform)'`

install-data-local: install_common
	@[ -d $(DESTDIR)$(mandir) ] || \
	    ($(mkdir) $(DESTDIR)$(mandir) && \
	     $(chmod) $(dmode) $(DESTDIR)$(mandir))
	@[ -d $(DESTDIR)$(mandir)/cat1 ] || \
	    ($(mkdir) $(DESTDIR)$(mandir)/cat1 && \
	     $(chmod) $(dmode) $(DESTDIR)$(mandir)/cat1)
	@echo "Installing man pages: $(DESTDIR)$(mandir) ..."
	@cd $(DESTDIR)$(mandir)/cat1 && \
	    $(rm) -f `echo vi.0 | sed '$(transform)'`
	@$(cp) $(visrcdir)/docs/vi.man/vi.0 \
	    $(DESTDIR)$(mandir)/cat1/`echo vi.0 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/cat1 && \
	    $(chmod) $(fmode) `echo vi.0 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/cat1 && \
	    $(rm) -f `echo ex.0 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/cat1 && \
	    $(rm) -f `echo view.0 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/cat1 && $(ln) \
	    `echo vi.0 | sed '$(transform)'` `echo ex.0 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/cat1 && $(ln) \
	    `echo vi.0 | sed '$(transform)'` `echo view.0 | sed '$(transform)'`
	@[ -d $(DESTDIR)$(mandir)/man1 ] || \
	    ($(mkdir) $(DESTDIR)$(mandir)/man1 && \
	     $(chmod) $(dmode) $(DESTDIR)$(mandir)/man1)
	@cd $(DESTDIR)$(mandir)/man1 && \
	    $(rm) -f `echo vi.1 | sed '$(transform)'`
	@$(cp) $(visrcdir)/docs/vi.man/vi.1 \
	    $(DESTDIR)$(mandir)/man1/`echo vi.1 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/man1 && \
	    $(chmod) $(fmode) `echo vi.1 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/man1 && \
	    $(rm) -f `echo ex.1 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/man1 && \
	    $(rm) -f `echo view.1 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/man1 && $(ln) \
	    `echo vi.1 | sed '$(transform)'` `echo ex.1 | sed '$(transform)'`
	@cd $(DESTDIR)$(mandir)/man1 && $(ln) \
	    `echo vi.1 | sed '$(transform)'` `echo view.1 | sed '$(transform)'`

cat=	dutch english french german ru_SU.KOI8-R spanish swedish
install_common:
	@[ -f $(chmod) ] || (echo 'chmod not found.'; exit 1)
	@[ -f $(cp) ] || (echo 'cp not found.'; exit 1)
	@[ -f $(ln) ] || (echo 'ln not found.'; exit 1)
	@[ -f $(mkdir) ] || (echo 'mkdir not found.'; exit 1)
	@[ -f $(rm) ] || (echo 'rm not found.'; exit 1)
	@[ -d $(DESTDIR)$(prefix) ] || \
	    ($(mkdir) $(DESTDIR)$(prefix) && \
		$(chmod) $(dmode) $(DESTDIR)$(prefix))
	@[ -d $(DESTDIR)$(exec_prefix) ] || \
	    ($(mkdir) $(DESTDIR)$(exec_prefix) && \
		$(chmod) $(dmode) $(DESTDIR)$(exec_prefix))
	@[ -d $(DESTDIR)$(datadir) ] || \
	    ($(mkdir) $(DESTDIR)$(datadir) && \
		$(chmod) $(dmode) $(DESTDIR)$(datadir))
	@$(rm) -rf $(DESTDIR)$(datadir)/vi
	@$(mkdir) $(DESTDIR)$(datadir)/vi && \
	    $(chmod) $(dmode) $(DESTDIR)$(datadir)/vi
	@echo "Installing message catalogs: $(datadir)/vi/catalog ..."
	@$(mkdir) $(DESTDIR)$(datadir)/vi/catalog && \
	    $(chmod) $(dmode) $(DESTDIR)$(datadir)/vi/catalog
	@(cd $(visrcdir)/catalog && \
	    $(cp) $(cat) $(DESTDIR)$(datadir)/vi/catalog && \
	    cd $(DESTDIR)$(datadir)/vi/catalog && $(chmod) $(fmode) *)
	@echo "Installing Perl scripts: $(datadir)/vi/perl ..."
	@$(mkdir) $(DESTDIR)$(datadir)/vi/perl && \
	    $(chmod) $(dmode) $(DESTDIR)$(datadir)/vi/perl
	@[ ! -f VI.pm ] || \
	    ($(cp) VI.pm $(DESTDIR)$(datadir)/vi/perl && \
	    (cd $(DESTDIR)$(datadir)/vi/perl && $(chmod) $(fmode) VI.pm))
	@(cd $(visrcdir)/perl_scripts && \
	    $(cp) *.pl $(DESTDIR)$(datadir)/vi/perl && \
	    cd $(DESTDIR)$(datadir)/vi/perl && $(chmod) $(fmode) *.pl)
	@echo "Installing Tcl scripts: $(datadir)/vi/tcl ..."
	@$(mkdir) $(DESTDIR)$(datadir)/vi/tcl && \
	    $(chmod) $(dmode) $(DESTDIR)$(datadir)/vi/tcl
	@(cd $(visrcdir)/tcl_scripts && \
	    $(cp) *.tcl $(DESTDIR)$(datadir)/vi/tcl && \
	    cd $(DESTDIR)$(datadir)/vi/tcl && $(chmod) $(fmode) *.tcl)
	@echo "Installing recover script: $(datadir)/vi/recover ..."
	@($(cp) recover $(DESTDIR)$(datadir)/vi/recover && \
	    $(chmod) $(emode) $(DESTDIR)$(datadir)/vi/recover)

uninstall-local:
	$(rm) -rf $(DESTDIR)$(datadir)/vi
	cd $(DESTDIR)$(bindir) && $(rm) -f `echo ex | sed '$(transform)'`
	cd $(DESTDIR)$(bindir) && $(rm) -f `echo view | sed '$(transform)'`
	cd $(DESTDIR)$(mandir)/cat1 && \
	    $(rm) -f `echo ex.0 | sed '$(transform)'`
	cd $(DESTDIR)$(mandir)/cat1 && \
	    $(rm) -f `echo vi.0 | sed '$(transform)'`
	cd $(DESTDIR)$(mandir)/cat1 && \
	    $(rm) -f `echo view.0 | sed '$(transform)'`
	cd $(DESTDIR)$(mandir)/man1 && \
	    $(rm) -f `echo ex.1 | sed '$(transform)'`
	cd $(DESTDIR)$(mandir)/man1 && \
	    $(rm) -f `echo vi.1 | sed '$(transform)'`
	cd $(DESTDIR)$(mandir)/man1 && \
	    $(rm) -f `echo view.1 | sed '$(transform)'`
