/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	vi_AUTO_H
#define	vi_AUTO_H
#define	DB_vi_marker	200
typedef struct _vi_marker_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
} __vi_marker_args;

#define	DB_vi_cursor	201
typedef struct _vi_cursor_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	db_recno_t	lno;
	size_t	cno;
} __vi_cursor_args;

#define	DB_vi_mark	202
typedef struct _vi_mark_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	LMARK	lmp;
} __vi_mark_args;

#define	DB_vi_change	203
typedef struct _vi_change_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	db_recno_t	lno;
} __vi_change_args;

#endif
