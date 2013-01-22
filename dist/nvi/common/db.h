/*	$NetBSD: db.h,v 1.3 2008/08/05 15:49:18 aymeric Exp $ */

#include <db.h>

#ifndef DB_BUFFER_SMALL
#define DB_BUFFER_SMALL		ENOMEM
#endif

#if USE_DB1 || (DB_VERSION_MAJOR >= 3 && DB_VERSION_MINOR >= 1)
#define db_env_open(env,path,flags,mode)				\
    (env)->open(env, path, flags, mode)
#define db_env_remove(env,path,flags)					\
    (env)->remove(env, path, flags)
#else
#define db_env_open(env,path,flags,mode)				\
    (env)->open(env, path, NULL, flags, mode)
#define db_env_remove(env,path,flags)					\
    (env)->remove(env, path, NULL, flags)
#endif

#if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
#define db_open(db,file,type,flags,mode)				\
    (db)->open(db, NULL, file, NULL, type, flags, mode)
#else
#define db_open(db,file,type,flags,mode)				\
    (db)->open(db, file, NULL, type, flags, mode)
#endif

#ifdef USE_DYNAMIC_LOADING
#define db_create   	nvi_db_create
#define db_env_create   nvi_db_env_create
#define db_strerror 	nvi_db_strerror

extern int   (*nvi_db_create) __P((DB **, DB_ENV *, u_int32_t));
extern int   (*nvi_db_env_create) __P((DB_ENV **, u_int32_t));
extern char *(*nvi_db_strerror) __P((int));
#endif

#ifdef USE_DB1

#define DB_AFTER	1
#define DB_APPEND	2
#define DB_BEFORE	3
#define DB_FIRST	7
#define DB_LAST		15
#define DB_SET		25

#define DB_NOTFOUND (-30989)

/* DBT emulation */
typedef DBT DBT_v1;
#undef DBT
#define DBT DBT_new

typedef struct {
	void *data;
	size_t size;

	u_int32_t ulen;

#define DB_DBT_USERMEM 0x040
	u_int32_t flags;
} DBT;

/* DB_ENV emulation */
struct __db_env_new;
typedef struct __db_env_new DB_ENV;

struct __db_env_new {
	int (*close)(DB_ENV *, u_int32_t);
	int (*open)(DB_ENV *, char *, u_int32_t, int);
#define DB_INIT_MPOOL	0x004000
#define DB_PRIVATE	0x200000
	int (*remove)(DB_ENV *, char *, u_int32_t);

	char *base_path;
	int mode;
};

/* DBC emulation */

struct __dbc_new;
typedef struct __dbc_new DBC;

typedef recno_t db_recno_t;
#define DB_MAX_RECORDS MAX_REC_NUMBER

#define DB_UNKNOWN (-1)

/* DB emulation */
typedef DB DB_old;
#undef DB
#define DB DB_new
typedef struct __db_new DB;

#undef DB_TXN
typedef void DB_TXN;

#undef DB_LSN
typedef struct {
	int dummy;
} DB_LSN;

struct __db_new {
	DB_old *actual_db;

	int type;

	int (*close)(DB *, u_int32_t);
#define	DB_NOSYNC	26		/* close() */

	int (*open)(DB *, const char *, const char *, DBTYPE, u_int32_t, int);
#define	DB_CREATE	0x000001	/* Create file as necessary. */
#define	DB_TRUNCATE	0x004000	/* Discard existing DB (O_TRUNC) */

	int (*sync)(DB *, u_int32_t);
	int (*get)(DB *, DB_TXN *, DBT *, DBT *, u_int32_t);
	int (*put)(DB *, DB_TXN *, DBT *, DBT *, u_int32_t);
	int (*del)(DB *, DB_TXN *, DBT *, u_int32_t);

	int (*cursor)(DB *, DB_TXN *, DBC **, u_int32_t);

	int (*set_flags)(DB *, u_int32_t);
#define	DB_RENUMBER	0x0008		/* Recno: renumber on insert/delete. */
#define	DB_SNAPSHOT	0x0020		/* Recno: snapshot the input. */

	int (*set_pagesize)(DB *, u_int32_t);

	int (*set_re_delim)(DB *, int);
	int (*set_re_source)(DB *, const char *);

	RECNOINFO _recno_info;
	u_int32_t _pagesize;
	u_int32_t _flags;
};

struct __dbc_new {
	DB *db;
	db_recno_t pos;
	DBT_v1 pos_key;
	int (*c_close)(DBC *);
	int (*c_get)(DBC *, DBT *, DBT *, u_int32_t);
	int (*c_put)(DBC *, DBT *, DBT *, u_int32_t);
};

#endif /* USE_DB1 */
