/*	$NetBSD: extern.h,v 1.5 2011/11/23 19:25:28 tnozaki Exp $ */

/* Do not edit: automatically built by build/distrib. */
SCR *api_fscreen __P((int, char *));
int api_aline __P((SCR *, db_recno_t, char *, size_t));
int api_extend __P((SCR *, db_recno_t));
int api_dline __P((SCR *, db_recno_t));
int api_gline __P((SCR *, db_recno_t, CHAR_T **, size_t *));
int api_iline __P((SCR *, db_recno_t, CHAR_T *, size_t));
int api_lline __P((SCR *, db_recno_t *));
int api_sline __P((SCR *, db_recno_t, CHAR_T *, size_t));
int api_getmark __P((SCR *, int, MARK *));
int api_setmark __P((SCR *, int, MARK *));
int api_nextmark __P((SCR *, int, char *));
int api_getcursor __P((SCR *, MARK *));
int api_setcursor __P((SCR *, MARK *));
void api_emessage __P((SCR *, char *));
void api_imessage __P((SCR *, char *));
int api_edit __P((SCR *, char *, SCR **, int));
int api_escreen __P((SCR *));
int api_swscreen __P((SCR *, SCR *));
int api_map __P((SCR *, char *, char *, size_t));
int api_unmap __P((SCR *, char *));
int api_opts_get __P((SCR *, const CHAR_T *, char **, int *));
int api_opts_set __P((SCR *, const CHAR_T *, const char *, u_long, int));
int api_run_str __P((SCR *, char *));
TAGQ * api_tagq_new __P((SCR*, char*));
void api_tagq_add __P((SCR*, TAGQ*, char*, char *, char *));
int api_tagq_push __P((SCR*, TAGQ**));
void api_tagq_free __P((SCR*, TAGQ*));
int cut __P((SCR *, ARG_CHAR_T *, MARK *, MARK *, int));
int cut_line __P((SCR *, db_recno_t, size_t, size_t, CB *));
void cut_close __P((WIN *));
TEXT *text_init __P((SCR *, const CHAR_T *, size_t, size_t));
void text_lfree __P((TEXTH *));
void text_free __P((TEXT *));
int db_eget __P((SCR *, db_recno_t, CHAR_T **, size_t *, int *));
int db_get __P((SCR *, db_recno_t, u_int32_t, CHAR_T **, size_t *));
int db_delete __P((SCR *, db_recno_t));
int db_append __P((SCR *, int, db_recno_t, const CHAR_T *, size_t));
int db_insert __P((SCR *, db_recno_t, CHAR_T *, size_t));
int db_set __P((SCR *, db_recno_t, CHAR_T *, size_t));
int db_exist __P((SCR *, db_recno_t));
int db_last __P((SCR *, db_recno_t *));
void db_err __P((SCR *, db_recno_t));
int scr_update __P((SCR *sp, db_recno_t lno, 
			lnop_t op, int current));
void update_cache __P((SCR *sp, lnop_t op, db_recno_t lno));
int line_insdel __P((SCR *sp, lnop_t op, db_recno_t lno));
int del __P((SCR *, MARK *, MARK *, int));
FREF *file_add __P((SCR *, const char *));
int file_init __P((SCR *, FREF *, char *, int));
int file_end __P((SCR *, EXF *, int));
int file_write __P((SCR *, MARK *, MARK *, char *, int));
int file_m1 __P((SCR *, int, int));
int file_m2 __P((SCR *, int));
int file_m3 __P((SCR *, int));
int file_aw __P((SCR *, int));
void set_alt_name __P((SCR *, const char *));
lockr_t file_lock __P((SCR *, char *, int *, int, int));
GS * gs_init __P((char*));
WIN * gs_new_win __P((GS *gp));
int win_end __P((WIN *wp));
void gs_end __P((GS *));
int v_key_init __P((SCR *));
void v_key_ilookup __P((SCR *));
size_t v_key_len __P((SCR *, ARG_CHAR_T));
u_char *v_key_name __P((SCR *, ARG_CHAR_T));
e_key_t v_key_val __P((SCR *, ARG_CHAR_T));
int v_event_push __P((SCR *, EVENT *, const CHAR_T *, size_t, u_int));
int v_event_get __P((SCR *, EVENT *, int, u_int32_t));
void v_event_err __P((SCR *, EVENT *));
int v_event_flush __P((SCR *, u_int));
int log_init __P((SCR *, EXF *));
int log_end __P((SCR *, EXF *));
int log_cursor __P((SCR *));
int log_line __P((SCR *, db_recno_t, u_int));
int log_mark __P((SCR *, LMARK *));
int log_backward __P((SCR *, MARK *));
int log_setline __P((SCR *));
int log_forward __P((SCR *, MARK *));
int log_init __P((SCR *, EXF *));
int log_end __P((SCR *, EXF *));
int log_cursor __P((SCR *));
int log_line __P((SCR *, db_recno_t, u_int));
int log_mark __P((SCR *, LMARK *));
int log_backward __P((SCR *, MARK *));
int log_setline __P((SCR *));
int log_forward __P((SCR *, MARK *));
int editor __P((WIN *, int, char *[]));
int mark_init __P((SCR *, EXF *));
int mark_end __P((SCR *, EXF *));
int mark_get __P((SCR *, ARG_CHAR_T, MARK *, mtype_t));
int mark_set __P((SCR *, ARG_CHAR_T, MARK *, int));
int mark_insdel __P((SCR *, lnop_t, db_recno_t));
void msgq __P((SCR *, mtype_t, const char *, ...))
    __attribute__((__format__(__printf__, 3, 4)));
void msgq_wstr __P((SCR *, mtype_t, const CHAR_T *, const char *));
void msgq_str __P((SCR *, mtype_t, const char *, const char *));
void mod_rpt __P((SCR *));
void msgq_status __P((SCR *, db_recno_t, u_int));
int msg_open __P((SCR *, const char *));
void msg_close __P((GS *));
const char *msg_cmsg __P((SCR *, cmsg_t, size_t *));
const char *msg_cat __P((SCR *, const char *, size_t *));
char *msg_print __P((SCR *, const char *, int *));
void thread_init __P((GS *gp));
int opts_init __P((SCR *, int *));
int opts_set __P((SCR *, ARGS *[], const char *));
int o_set __P((SCR *, int, u_int, const char *, u_long));
int opts_empty __P((SCR *, int, int));
void opts_dump __P((SCR *, enum optdisp));
int opts_save __P((SCR *, FILE *));
OPTLIST const *opts_search __P((const CHAR_T *));
void opts_nomatch __P((SCR *, const CHAR_T *));
int opts_copy __P((SCR *, SCR *));
void opts_free __P((SCR *));
int f_altwerase __P((SCR *, OPTION *, const char *, u_long *));
int f_columns __P((SCR *, OPTION *, const char *, u_long *));
int f_lines __P((SCR *, OPTION *, const char *, u_long *));
int f_lisp __P((SCR *, OPTION *, const char *, u_long *));
int f_msgcat __P((SCR *, OPTION *, const char *, u_long *));
int f_paragraph __P((SCR *, OPTION *, const char *, u_long *));
int f_print __P((SCR *, OPTION *, const char *, u_long *));
int f_readonly __P((SCR *, OPTION *, const char *, u_long *));
int f_recompile __P((SCR *, OPTION *, const char *, u_long *));
int f_reformat __P((SCR *, OPTION *, const char *, u_long *));
int f_section __P((SCR *, OPTION *, const char *, u_long *));
int f_ttywerase __P((SCR *, OPTION *, const char *, u_long *));
int f_w300 __P((SCR *, OPTION *, const char *, u_long *));
int f_w1200 __P((SCR *, OPTION *, const char *, u_long *));
int f_w9600 __P((SCR *, OPTION *, const char *, u_long *));
int f_window __P((SCR *, OPTION *, const char *, u_long *));
int f_encoding __P((SCR *, OPTION *, const char *, u_long *));
void thread_init __P((GS *gp));
int put __P((SCR *, CB *, ARG_CHAR_T *, MARK *, MARK *, int));
int rcv_tmp __P((SCR *, EXF *, char *));
int rcv_init __P((SCR *));
int rcv_sync __P((SCR *, u_int));
int rcv_list __P((SCR *));
int rcv_read __P((SCR *, FREF *));
int screen_init __P((GS *, SCR *, SCR **));
int screen_end __P((SCR *));
SCR *screen_next __P((SCR *));
int f_search __P((SCR *,
   MARK *, MARK *, CHAR_T *, size_t, CHAR_T **, u_int));
int b_search __P((SCR *,
   MARK *, MARK *, CHAR_T *, size_t, CHAR_T **, u_int));
void search_busy __P((SCR *, busy_t));
int seq_set __P((SCR *, CHAR_T *,
   size_t, CHAR_T *, size_t, CHAR_T *, size_t, seq_t, int));
int seq_delete __P((SCR *, CHAR_T *, size_t, seq_t));
int seq_mdel __P((SEQ *));
SEQ *seq_find
   __P((SCR *, SEQ **, EVENT *, CHAR_T *, size_t, seq_t, int *));
void seq_close __P((GS *));
int seq_dump __P((SCR *, seq_t, int));
int seq_save __P((SCR *, FILE *, const char *, seq_t));
int e_memcmp __P((CHAR_T *, EVENT *, size_t));
void vtrace_end __P((void));
void vtrace_init __P((char *));
void vtrace __P((const char *, ...))
    __attribute__((__format__(__printf__, 1, 2)));
void *binc __P((SCR *, void *, size_t *, size_t));
int nonblank __P((SCR *, db_recno_t, size_t *));
const char *tail __P((const char *));
char *v_strdup __P((SCR *, const char *, size_t));
CHAR_T *v_wstrdup __P((SCR *, const CHAR_T *, size_t));
enum nresult nget_uslong __P((SCR *, u_long *, const CHAR_T *, CHAR_T **, int));
enum nresult nget_slong __P((SCR *, long *, const CHAR_T *, CHAR_T **, int));
#ifdef USE_DB4_LOGGING
int __vi_marker_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t,
     u_int32_t));
int __vi_marker_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_marker_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_marker_read __P((DB_ENV *, void *, __vi_marker_args **));
int __vi_cursor_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t,
     u_int32_t, db_recno_t, size_t));
int __vi_cursor_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_cursor_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_cursor_read __P((DB_ENV *, void *, __vi_cursor_args **));
int __vi_mark_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t,
     LMARK *));
int __vi_mark_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_mark_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_mark_read __P((DB_ENV *, void *, __vi_mark_args **));
int __vi_change_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t,
     u_int32_t, db_recno_t));
int __vi_change_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_change_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops,
     void *));
int __vi_change_read __P((DB_ENV *, void *, __vi_change_args **));
int __vi_init_print __P((DB_ENV *, int (***)(DB_ENV *, DBT *,
     DB_LSN *, db_recops, void *), size_t *));
int __vi_init_getpgnos __P((DB_ENV *, int (***)(DB_ENV *, DBT *,
     DB_LSN *, db_recops, void *), size_t *));
int __vi_init_recover __P((DB_ENV *));
#endif
#ifdef USE_DB4_LOGGING
int __vi_marker_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __vi_cursor_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __vi_mark_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __vi_change_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __vi_log_truncate __P((EXF *ep));
int __vi_log_dispatch __P((DB_ENV *dbenv, DBT *data, DB_LSN *lsn, db_recops ops));
int __vi_log_traverse __P((SCR *sp, undo_t undo, MARK *));
#endif
