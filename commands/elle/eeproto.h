#ifndef _ANSI
#include <ansi.h>
#endif

/* eebit.c */
_PROTOTYPE( int *chballoc, (int size) );
_PROTOTYPE( int chbit, (int *array, int c) );
_PROTOTYPE( int chbis, (int *array, int c) );
_PROTOTYPE( int chbic, (int *array, int c) );

/* eebuff.c */
_PROTOTYPE( int f_selbuffer, (void) );
_PROTOTYPE( int f_selxbuffer, (void) );
_PROTOTYPE( int f_kbuffer, (void) );
_PROTOTYPE( int f_listbufs, (void) );
_PROTOTYPE( int f_bufnotmod, (void) );
_PROTOTYPE( int f_eolmode, (void) );
_PROTOTYPE( int f_gobeg, (void) );
_PROTOTYPE( int f_goend, (void) );
_PROTOTYPE( int f_whatpage, (void) );
_PROTOTYPE( int init_buf, (void) );
_PROTOTYPE( struct buffer *make_buf, (char *bname) );
_PROTOTYPE( struct buffer *find_buf, (char *name) );
_PROTOTYPE( int sel_buf, (struct buffer *b) );
_PROTOTYPE( int chg_buf, (struct buffer *newbuf) );
_PROTOTYPE( int unlk_buf, (struct buffer *bufp) );
_PROTOTYPE( struct buffer *sel_mbuf, (struct buffer *buf) );
_PROTOTYPE( struct buffer *sel_mbuf, (struct buffer *buf) );
_PROTOTYPE( struct buffer *sel_nbuf, (struct buffer *buf) );
_PROTOTYPE( int kill_buf, (struct buffer *buf) );
_PROTOTYPE( int zap_buffer, (void) );
_PROTOTYPE( int ask_kbuf, (struct buffer *buf) );
_PROTOTYPE( int f_2winds, (void) );
_PROTOTYPE( int f_1wind, (void) );
_PROTOTYPE( int f_othwind, (void) );
_PROTOTYPE( int f_growind, (void) );
_PROTOTYPE( int f_shrinkwind, (void) );
_PROTOTYPE( int f_delwind, (void) );
_PROTOTYPE( int f_sowind, (void) );
_PROTOTYPE( int f_2modewinds, (void) );
_PROTOTYPE( int chk2modws, (void) );
_PROTOTYPE( int init_win, (void) );
_PROTOTYPE( int chg_win, (struct window *newwin) );
_PROTOTYPE( struct window *make_win, (int pos, int ht, struct buffer *buf) );
_PROTOTYPE( int kill_win, (struct window *win) );
_PROTOTYPE( int mk_showin, (struct buffer *b) );
_PROTOTYPE( struct window *make_mode, (struct window *bw) );
_PROTOTYPE( int buf_mod, (void) );
_PROTOTYPE( int buf_tmat, (chroff dot) );
_PROTOTYPE( int buf_tmod, (chroff offset) );

/* eecmds.c */
_PROTOTYPE( int f_pfxmeta, (void) );
_PROTOTYPE( int f_pfxext, (void) );
_PROTOTYPE( int f_uarg, (int ch) );
_PROTOTYPE( int f_negarg, (int ch) );
_PROTOTYPE( int f_argdig, (int ch) );
_PROTOTYPE( int f_setprof, (void) );
_PROTOTYPE( int f_vtbuttons, (void) );
_PROTOTYPE( int cmd_wait, (void) );
_PROTOTYPE( int cmd_read, (void) );
_PROTOTYPE( int cmd_xct, (int ch) );
_PROTOTYPE( int cmd_idx, (int c) );
_PROTOTYPE( int set_profile, (char *filename) );
_PROTOTYPE( int init_menu, (void) );

/* eediag.c */
_PROTOTYPE( int f_debug, (int ch) );
_PROTOTYPE( char *vfy_data, (int flag) );
_PROTOTYPE( int dbg_diag, (void) );
_PROTOTYPE( int vfy_exer, (int pf, int gcfrq) );
_PROTOTYPE( int db_prwind, (struct window *w) );
_PROTOTYPE( char *db_scflgs, (int flags) );

/* eedisp.c */
_PROTOTYPE( int set_tty, (void) );
_PROTOTYPE( int clean_exit, (void) );
_PROTOTYPE( int set_scr, (void) );
_PROTOTYPE( int redisplay, (void) );
_PROTOTYPE( int fupd_wind, (struct window *w) );
_PROTOTYPE( int upd_curs, (chroff adot) );
_PROTOTYPE( int d_line, (chroff cdot) );
_PROTOTYPE( int d_ncols, (int lcnt, int ccol) );
_PROTOTYPE( int d_lupd, (struct window *w, int idx) );
_PROTOTYPE( int clear_wind, (struct window *w) );
_PROTOTYPE( int fix_wind, (struct window *win) );
_PROTOTYPE( int inwinp, (struct window *win, chroff cdot) );
_PROTOTYPE( int upd_wind, (struct window *win) );
_PROTOTYPE( int slineq, (struct scr_line *olds, struct scr_line *news) );
_PROTOTYPE( int upd_line, (int y) );
_PROTOTYPE( int fillset, (char *str, int cnt, int c) );
_PROTOTYPE( int fillsp, (char *str, int cnt) );
_PROTOTYPE( int inspc, (char *cp0, char *cpl, int cnt) );
_PROTOTYPE( int fix_line, (struct scr_line *slp, struct scr_line *olds) );
_PROTOTYPE( int sctrin, (char *to, int lim, int ccol) );
_PROTOTYPE( int inslin, (int line, int n, struct window *win) );
_PROTOTYPE( int dellin, (int line, int n, struct window *win) );
_PROTOTYPE( int t_dostandout, (int on) );
_PROTOTYPE( int t_move, (int y, int x) );
_PROTOTYPE( int t_docleol, (void) );

/* eeedit.c */
_PROTOTYPE( int e_reset, (void) );
_PROTOTYPE( int e_rgetc, (void) );
_PROTOTYPE( int e_rdelc, (void) );
_PROTOTYPE( int e_delc, (void) );
_PROTOTYPE( int e_getc, (void) );
_PROTOTYPE( int e_backc, (void) );
_PROTOTYPE( int e_putc, (int c) );
_PROTOTYPE( int e_peekc, (void) );
_PROTOTYPE( int e_ovwc, (int ch) );
_PROTOTYPE( SBSTR *e_copyn, (chroff off) );
_PROTOTYPE( int e_deln, (chroff off) );
_PROTOTYPE( int e_setcur, (void) );
_PROTOTYPE( int e_gosetcur, (chroff dot) );
_PROTOTYPE( int e_gocur, (void) );
_PROTOTYPE( int e_gobob, (void) );
_PROTOTYPE( int e_goeob, (void) );
_PROTOTYPE( int e_go, (chroff dot) );
_PROTOTYPE( int e_igoff, (int ioff) );
_PROTOTYPE( int e_goff, (chroff off) );
_PROTOTYPE( int e_gobol, (void) );
_PROTOTYPE( int e_goeol, (void) );
_PROTOTYPE( int e_gonl, (void) );
_PROTOTYPE( int e_gopl, (void) );
_PROTOTYPE( chroff e_dot, (void) );
_PROTOTYPE( chroff e_nldot, (void) );
_PROTOTYPE( chroff e_pldot, (void) );
_PROTOTYPE( chroff e_boldot, (void) );
_PROTOTYPE( chroff e_eoldot, (void) );
_PROTOTYPE( chroff e_alldot, (SBBUF *sbp, int (*rtn )()) );
_PROTOTYPE( chroff e_blen, (void) );
_PROTOTYPE( int ex_reset, (struct buffer *b) );
_PROTOTYPE( int ex_go, (SBBUF *sbp, chroff loc) );
_PROTOTYPE( chroff ex_dot, (SBBUF *sbp) );
_PROTOTYPE( chroff ex_boldot, (SBBUF *sbp, chroff dot) );
_PROTOTYPE( chroff ex_alldot, (SBBUF *sbp, int (*rtn )(), chroff dot) );
_PROTOTYPE( int ex_gonl, (SBBUF *sbp) );
_PROTOTYPE( int ex_goeol, (SBBUF *sbp) );
_PROTOTYPE( int ex_gobol, (SBBUF *sbp) );
_PROTOTYPE( int ex_gopl, (SBBUF *sbp) );
_PROTOTYPE( chroff ex_blen, (SBBUF *sbp) );
_PROTOTYPE( int e_gofwsp, (void) );
_PROTOTYPE( int e_gobwsp, (void) );
_PROTOTYPE( int e_goline, (int i) );
_PROTOTYPE( int e_lblankp, (void) );
_PROTOTYPE( int e_insn, (int ch, int cnt) );
_PROTOTYPE( int e_sputz, (char *acp) );
_PROTOTYPE( int boleq, (chroff dot1, chroff dot2) );
_PROTOTYPE( char *dottoa, (char *str, chroff val) );
_PROTOTYPE( int e_gobpa, (void) );
_PROTOTYPE( int e_goepa, (void) );
_PROTOTYPE( int exp_do, (int (*rpos )(), int (*rneg )()) );
_PROTOTYPE( int e_fwsp, (void) );
_PROTOTYPE( int e_bwsp, (void) );
_PROTOTYPE( int c_wsp, (int ch) );
_PROTOTYPE( int c_pwsp, (int ch) );
_PROTOTYPE( int delimp, (int c) );
_PROTOTYPE( int e_wding, (chroff *adot, int n) );
_PROTOTYPE( chroff e_wdot, (chroff dot, int n) );
_PROTOTYPE( int e_gowd, (int n) );
_PROTOTYPE( int e_search, (char *mstr, int mlen, int backwards) );

/* eeerr.c */
_PROTOTYPE( int f_bkpt, (void) );
_PROTOTYPE( int bpt, (void) );
_PROTOTYPE( char *strerror, (int num) );
_PROTOTYPE( int errsbm, (int type, int (*adr )(), char *str, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) );
_PROTOTYPE( int bite_bag, (int fault) );
_PROTOTYPE( int hup_exit, (void) );
_PROTOTYPE( int errint, (void) );
_PROTOTYPE( int askerr, (void) );
_PROTOTYPE( char *asklin, (char *acp) );

/* eef1.c */
_PROTOTYPE( int f_insself, (int c) );
_PROTOTYPE( int f_quotins, (void) );
_PROTOTYPE( int f_crlf, (void) );
_PROTOTYPE( int f_fchar, (void) );
_PROTOTYPE( int f_bchar, (void) );
_PROTOTYPE( int f_dchar, (void) );
_PROTOTYPE( int f_bdchar, (void) );
_PROTOTYPE( int ef_deln, (int x) );
_PROTOTYPE( int f_delspc, (void) );
_PROTOTYPE( int f_tchars, (void) );
_PROTOTYPE( int f_fword, (void) );
_PROTOTYPE( int f_bword, (void) );
_PROTOTYPE( int f_kword, (void) );
_PROTOTYPE( int f_bkword, (void) );
_PROTOTYPE( int f_twords, (void) );
_PROTOTYPE( int f_ucword, (void) );
_PROTOTYPE( int f_lcword, (void) );
_PROTOTYPE( int f_uciword, (void) );
_PROTOTYPE( int case_word, (int downp) );

/* eef2.c */
_PROTOTYPE( int f_begline, (void) );
_PROTOTYPE( int f_endline, (void) );
_PROTOTYPE( int f_nxtline, (void) );
_PROTOTYPE( int f_prvline, (void) );
_PROTOTYPE( int f_dnrline, (void) );
_PROTOTYPE( int f_uprline, (void) );
_PROTOTYPE( int f_oline, (void) );
_PROTOTYPE( int f_delblines, (void) );
_PROTOTYPE( int f_kline, (void) );
_PROTOTYPE( int f_bkline, (void) );
_PROTOTYPE( int f_goline, (void) );
_PROTOTYPE( int down_bline, (int arg) );
_PROTOTYPE( int down_line, (int x) );
_PROTOTYPE( int f_setmark, (void) );
_PROTOTYPE( int f_exchmark, (void) );
_PROTOTYPE( int f_kregion, (void) );
_PROTOTYPE( int f_copreg, (void) );
_PROTOTYPE( int f_ucreg, (void) );
_PROTOTYPE( int f_lcreg, (void) );
_PROTOTYPE( int ef_creg, (int downp) );
_PROTOTYPE( int f_fillreg, (void) );
_PROTOTYPE( int chkmark, (void) );
_PROTOTYPE( int f_fpara, (void) );
_PROTOTYPE( int f_bpara, (void) );
_PROTOTYPE( int f_mrkpara, (void) );
_PROTOTYPE( int f_fillpara, (void) );

/* eef3.c */
_PROTOTYPE( int f_appnkill, (void) );
_PROTOTYPE( int f_unkill, (void) );
_PROTOTYPE( int f_unkpop, (void) );
_PROTOTYPE( int f_indatm, (void) );
_PROTOTYPE( int f_indnl, (void) );
_PROTOTYPE( int f_backind, (void) );
_PROTOTYPE( int f_indcomm, (void) );
_PROTOTYPE( int f_indrel, (void) );
_PROTOTYPE( int insertmatch, (int c) );
_PROTOTYPE( int f_matchbrack, (void) );
_PROTOTYPE( int matchonelevel, (int mc) );

/* eefd.c */
_PROTOTYPE( int f_newwin, (void) );
_PROTOTYPE( int f_nscreen, (void) );
_PROTOTYPE( int f_pscreen, (void) );
_PROTOTYPE( int f_othnscreen, (void) );
_PROTOTYPE( int f_lwindbord, (void) );
_PROTOTYPE( int f_scupwind, (void) );
_PROTOTYPE( int f_scdnwind, (void) );
_PROTOTYPE( int f_mvwtop, (void) );
_PROTOTYPE( int f_mvwbot, (void) );
_PROTOTYPE( int d_screen, (int rep) );
_PROTOTYPE( int scroll_win, (int n) );
_PROTOTYPE( int d_curind, (void) );
_PROTOTYPE( int indtion, (chroff lin) );
_PROTOTYPE( int inindex, (chroff lin, int xpos) );
_PROTOTYPE( int d_gopl, (void) );
_PROTOTYPE( int d_gonl, (void) );
_PROTOTYPE( int d_goloff, (int cnt) );
_PROTOTYPE( int d_fgoloff, (int cnt) );
_PROTOTYPE( int d_fixcur, (void) );
_PROTOTYPE( int d_backup, (int nlin) );

/* eefed.c */
_PROTOTYPE( int ed_insert, (int c) );
_PROTOTYPE( int ed_insn, (int ch, int cnt) );
_PROTOTYPE( int ed_crins, (void) );
_PROTOTYPE( int ed_sins, (char *s) );
_PROTOTYPE( int ed_nsins, (char *s, int i) );
_PROTOTYPE( int ed_indto, (int goal) );
_PROTOTYPE( int ed_setcur, (void) );
_PROTOTYPE( int ed_go, (chroff dot) );
_PROTOTYPE( int ed_goff, (chroff off) );
_PROTOTYPE( int ed_igoff, (int ioff) );
_PROTOTYPE( int ed_reset, (void) );
_PROTOTYPE( int ed_deln, (chroff off) );
_PROTOTYPE( int ed_delete, (chroff dot1, chroff dot2) );
_PROTOTYPE( int ed_kill, (chroff dot1, chroff dot2) );
_PROTOTYPE( int kill_push, (SBSTR *sdp) );
_PROTOTYPE( int ed_case, (chroff dot1, chroff dot2, int downp) );
_PROTOTYPE( int upcase, (int ch) );

/* eefile.c */
_PROTOTYPE( int f_ffile, (void) );
_PROTOTYPE( int f_rfile, (void) );
_PROTOTYPE( int f_vfile, (void) );
_PROTOTYPE( int u_r_file, (char *prompt) );
_PROTOTYPE( int f_ifile, (void) );
_PROTOTYPE( int f_sfile, (void) );
_PROTOTYPE( int f_savefiles, (void) );
_PROTOTYPE( int f_wfile, (void) );
_PROTOTYPE( int f_wreg, (void) );
_PROTOTYPE( int f_wlastkill, (void) );
_PROTOTYPE( int hack_file, (char *prompt, int (*rtn )()) );
_PROTOTYPE( int find_file, (char *f_name) );
_PROTOTYPE( int read_file, (char *f_name) );
_PROTOTYPE( int ins_file, (char *f_name) );
_PROTOTYPE( int ferr_ropn, (void) );
_PROTOTYPE( int ferr_wopn, (void) );
_PROTOTYPE( int ferr, (char *str) );
_PROTOTYPE( char *fncons, (char *dest, char *pre, char *f_name, char *post) );
_PROTOTYPE( char *last_fname, (char *f_name) );
_PROTOTYPE( int set_fn, (char *string) );
_PROTOTYPE( int saveworld, (struct buffer *bp, int grunt) );
_PROTOTYPE( int hoard, (void) );
_PROTOTYPE( int unhoard, (void) );
_PROTOTYPE( int expand_file, (char *dfn, char *sfn) );

/* eefill.c */
_PROTOTYPE( int f_sfcol, (void) );
_PROTOTYPE( int f_sfpref, (void) );
_PROTOTYPE( int tstfillp, (int lim) );
_PROTOTYPE( int ed_fill, (chroff begloc, chroff endloc, int flag) );
_PROTOTYPE( int f_fillmode, (void) );
_PROTOTYPE( int fx_insfill, (int c) );
_PROTOTYPE( int fill_cur_line, (void) );
_PROTOTYPE( int f_textmode, (void) );
_PROTOTYPE( int fim_insself, (int c) );
_PROTOTYPE( int fim_dchar, (void) );
_PROTOTYPE( int fim_bdchar, (void) );
_PROTOTYPE( int fim_crlf, (void) );
_PROTOTYPE( int magic_wrap, (int tc) );
_PROTOTYPE( int reveal, (char *msg, int v1, int v2, int v3) );
_PROTOTYPE( int magic_backto_bol, (void) );
_PROTOTYPE( int fill_current_line, (void) );

/* eehelp.c */
_PROTOTYPE( int f_describe, (void) );

/* eekmac.c */
_PROTOTYPE( int f_skmac, (void) );
_PROTOTYPE( int f_ekmac, (void) );
_PROTOTYPE( int f_xkmac, (void) );
_PROTOTYPE( int f_vkmac, (void) );
_PROTOTYPE( int km_getc, (void) );
_PROTOTYPE( int km_inwait, (void) );
_PROTOTYPE( int km_abort, (void) );
_PROTOTYPE( int add_mode, (char *mode) );
_PROTOTYPE( int remove_mode, (char *mode) );

/* eemain.c */
_PROTOTYPE( int doargs, (int argc, char **argv) );
_PROTOTYPE( int initialize, (void) );
_PROTOTYPE( int f_throw, (void) );
_PROTOTYPE( int ring_bell, (void) );
_PROTOTYPE( int f_retsup, (void) );
_PROTOTYPE( int f_wfexit, (void) );
_PROTOTYPE( int f_pshinf, (void) );
_PROTOTYPE( char *strdup, (char *s) );
_PROTOTYPE( char *memalloc, (SBMO size) );
_PROTOTYPE( int chkfree, (SBMA ptr) );
_PROTOTYPE( int ustrcmp, (char *str1, char *str2) );
_PROTOTYPE( int writerr, (char *str) );
_PROTOTYPE( int writez, (int fd, char *acp) );

/* eemake.c */
_PROTOTYPE( int f_xucmd, (void) );
_PROTOTYPE( int f_make, (void) );
_PROTOTYPE( int f_nxterr, (void) );
_PROTOTYPE( int f_xucmd, (void) );
_PROTOTYPE( int f_make, (void) );
_PROTOTYPE( int f_nxterr, (void) );
_PROTOTYPE( int do_exec, (char *cmd, int nicely) );
_PROTOTYPE( int make_or_unix_cmd, (int domake) );
_PROTOTYPE( int sel_execbuf, (void) );
_PROTOTYPE( int popto_buf, (struct buffer *b) );

/* eequer.c */
_PROTOTYPE( int f_querep, (void) );
_PROTOTYPE( int f_repstr, (void) );
_PROTOTYPE( int f_repline, (void) );
_PROTOTYPE( int ed_dorep, (int type, struct majmode *mode) );

/* eeques.c */
#if 0
_PROTOTYPE( char *ask, (char *string, char *arg1, char *arg2, char *arg3) );
#else
char *ask();
#endif
_PROTOTYPE( int askclr, (void) );
_PROTOTYPE( int say, (char *str) );
_PROTOTYPE( int saynow, (char *str) );
_PROTOTYPE( int saytoo, (char *str) );
_PROTOTYPE( int sayntoo, (char *str) );
_PROTOTYPE( int ding, (char *str) );
_PROTOTYPE( int dingtoo, (char *str) );
_PROTOTYPE( int saylntoo, (char *str, int n) );
_PROTOTYPE( int sayclr, (void) );
#if 0
_PROTOTYPE( int sayall, (char *str, int flags, int len) );
#else
int sayall();
#endif
_PROTOTYPE( int yellat, (char *str, int line) );
_PROTOTYPE( int yelltoo, (char *str) );
_PROTOTYPE( int errbarf, (char *str) );
_PROTOTYPE( int barf, (char *str) );
_PROTOTYPE( int barf2, (char *str) );

/* eesite.c */
_PROTOTYPE( int ts_inp, (void) );
_PROTOTYPE( int ts_init, (void) );
_PROTOTYPE( int ts_enter, (void) );
_PROTOTYPE( int ts_exit, (void) );
_PROTOTYPE( int tpoke, (int cmd, int bn, int val) );
_PROTOTYPE( int ts_pause, (void) );

/* eesrch.c */
_PROTOTYPE( int f_srch, (void) );
_PROTOTYPE( int f_rsrch, (void) );
_PROTOTYPE( int lin_search, (int backwards) );
_PROTOTYPE( int srchint, (void) );
_PROTOTYPE( char *srch_ask, (char *prompt) );
_PROTOTYPE( int f_risrch, (void) );
_PROTOTYPE( int i_search, (int back) );

/* eeterm.c */
_PROTOTYPE( int t_init, (void) );
_PROTOTYPE( int t_fatal, (char *str) );
_PROTOTYPE( int t_enter, (void) );
_PROTOTYPE( int t_exit, (void) );
_PROTOTYPE( int t_clear, (void) );
_PROTOTYPE( int t_curpos, (int lin, int col) );
_PROTOTYPE( int t_backspace, (void) );
_PROTOTYPE( int t_bell, (void) );
_PROTOTYPE( int t_cleol, (void) );
_PROTOTYPE( int t_inslin, (int n, int bot) );
_PROTOTYPE( int t_dellin, (int n, int bot) );
_PROTOTYPE( int t_inschr, (int n, char *str) );
_PROTOTYPE( int t_delchr, (int n) );
_PROTOTYPE( int t_standout, (int on) );
_PROTOTYPE( int t_direct, (int lin, int col, char *str, int len) );
_PROTOTYPE( int tput, (int ch) );
_PROTOTYPE( int tputz, (char *str) );
_PROTOTYPE( int tputn, (char *str, int cnt) );
_PROTOTYPE( int tbufls, (void) );
_PROTOTYPE( int tgetc, (void) );
_PROTOTYPE( int tinwait, (void) );

/* eevini.c */
