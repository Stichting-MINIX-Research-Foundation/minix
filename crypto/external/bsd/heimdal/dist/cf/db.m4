dnl Id
dnl
dnl tests for various db libraries
dnl

AC_DEFUN([rk_DB],[
AC_ARG_WITH(db-type-preference,
                       AS_HELP_STRING([--with-db-type-preference=list],
                                      [specify HDB backend DB type preference as whitespace-separated list of db1, db3, lmdb, and/or sqlite]),
                       [db_type_preference="$withval"],
                       [db_type_preference="lmdb db3 db1 sqlite"])
AC_ARG_WITH(berkeley-db,
                       AS_HELP_STRING([--with-berkeley-db],
                                      [enable support for berkeley db @<:@default=check@:>@]),
                       [],
                       [with_berkeley_db=check])

dbheader=""
AC_ARG_WITH(berkeley-db-include,
                       AS_HELP_STRING([--with-berkeley-db-include=dir],
		                      [use berkeley-db headers in dir]),
		       [dbheader=$withval],
		       [with_berkeley_db_include=check])

AC_ARG_ENABLE(ndbm-db,
                       AS_HELP_STRING([--disable-ndbm-db],
                                      [if you don't want ndbm db]),[
])

AC_ARG_ENABLE(mdb-db,
                       AS_HELP_STRING([--disable-mdb-db],
                                      [if you don't want LMDB]),[
])

have_db1=no
have_db3=no
have_lmdb=no
db_type=unknown

AS_IF([test "x$with_berkeley_db" != xno],
  [AS_IF([test "x$with_berkeley_db_include" != xcheck],
    [AC_CHECK_HEADERS(["$dbheader/db.h"],
                   [AC_SUBST([DBHEADER], [$dbheader])
		    AC_DEFINE([HAVE_DBHEADER], [1],
		                      [Define if you have user supplied header location])
	           ],
		   [if test "x$with_berkeley_db_include" != xcheck; then
		     AC_MSG_FAILURE(
		       [--with-berkeley-db-include was given but include test failed])
		    fi
		   ])],
    [AC_CHECK_HEADERS([					\
	           db6/db.h				\
	           db5/db.h				\
	           db4/db.h				\
	           db3/db.h				\
	           db.h					\
    ])])

dnl db_create is used by db3 and db4 and db5

  AC_FIND_FUNC_NO_LIBS(db_create, [$dbheader] db-5 db5 db4 db3 db, [
  #include <stdio.h>
  #ifdef HAVE_DBHEADER
  #include <$dbheader/db.h>
  #elif HAVE_DB6_DB_H
  #include <db6/db.h>
  #elif HAVE_DB5_DB_H
  #include <db5/db.h>
  #elif HAVE_DB4_DB_H
  #include <db4/db.h>
  #elif defined(HAVE_DB3_DB_H)
  #include <db3/db.h>
  #else
  #include <db.h>
  #endif
  ],[NULL, NULL, 0])

  if test "$ac_cv_func_db_create" = "yes"; then
    have_db3=yes
    if test "$ac_cv_funclib_db_create" != "yes"; then
      DB3LIB="$ac_cv_funclib_db_create"
    else
      DB3LIB=""
    fi
    AC_DEFINE(HAVE_DB3, 1, [define if you have a berkeley db3/4/5 library])
  fi

dnl dbopen is used by db1/db2

  AC_FIND_FUNC_NO_LIBS(dbopen, db2 db, [
  #include <stdio.h>
  #if defined(HAVE_DB2_DB_H)
  #include <db2/db.h>
  #elif defined(HAVE_DB_H)
  #include <db.h>
  #else
  #error no db.h
  #endif
  ],[NULL, 0, 0, 0, NULL])

  if test "$ac_cv_func_dbopen" = "yes"; then
    have_db1=yes
    if test "$ac_cv_funclib_dbopen" != "yes"; then
      DB1LIB="$ac_cv_funclib_dbopen"
    else
      DB1LIB=""
    fi
    AC_DEFINE(HAVE_DB1, 1, [define if you have a berkeley db1/2 library])
  fi

dnl test for ndbm compatability

  if test "$ac_cv_func_dbm_firstkey" != yes; then
    AC_FIND_FUNC_NO_LIBS2(dbm_firstkey, $ac_cv_funclib_dbopen $ac_cv_funclib_db_create, [
    #include <stdio.h>
    #define DB_DBM_HSEARCH 1
    #include <db.h>
    DBM *dbm;
    ],[NULL])
  
    if test "$ac_cv_func_dbm_firstkey" = "yes"; then
      if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
        NDBMLIB="$ac_cv_funclib_dbm_firstkey"
      else
        NDBMLIB=""
      fi
      AC_DEFINE(HAVE_DB_NDBM, 1, [define if you have ndbm compat in db])
      AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files *.db)])
    else
      $as_unset ac_cv_func_dbm_firstkey
      $as_unset ac_cv_funclib_dbm_firstkey
    fi
  fi

]) # fi berkeley db


AS_IF([test "x$enable_mdb_db" != xno],
    [AC_CHECK_HEADER(lmdb.h, [
		AC_CHECK_LIB(lmdb, mdb_env_create, have_lmdb=yes; LMDBLIB="-llmdb"
		AC_DEFINE(HAVE_LMDB, 1, [define if you have the LMDB library]))])])

for db_type in unknown $db_type_preference; do
    if eval test \"x\$have_${db_type}\" = xyes -o ${db_type} = sqlite; then
        break
    fi
    db_type=unknown
done

AS_IF([test "x$have_db3" = xyes -a "$db_type" = unknown], db_type=db3, db_type="$db_type")
AS_IF([test "x$have_db1" = xyes -a "$db_type" = unknown], db_type=db1, db_type="$db_type")
AS_IF([test "x$have_lmdb" = xyes -a "$db_type" = unknown], db_type=lmdb, db_type="$db_type")

if test "$enable_ndbm_db" != "no"; then

  if test "$db_type" = "unknown" -o "$ac_cv_func_dbm_firstkey" = ""; then

    AC_CHECK_HEADERS([				\
  	dbm.h					\
  	ndbm.h					\
    ])
  
    AC_FIND_FUNC_NO_LIBS(dbm_firstkey, ndbm, [
    #include <stdio.h>
    #if defined(HAVE_NDBM_H)
    #include <ndbm.h>
    #elif defined(HAVE_DBM_H)
    #include <dbm.h>
    #endif
    DBM *dbm;
    ],[NULL])
  
    if test "$ac_cv_func_dbm_firstkey" = "yes"; then
      if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
        NDBMLIB="$ac_cv_funclib_dbm_firstkey"
      else
        NDBMLIB=""
      fi
      AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
      have_ndbm=yes
    else
  
      $as_unset ac_cv_func_dbm_firstkey
      $as_unset ac_cv_funclib_dbm_firstkey
  
      AC_CHECK_HEADERS([				\
  	  gdbm/ndbm.h				\
      ])
  
      AC_FIND_FUNC_NO_LIBS(dbm_firstkey, gdbm, [
      #include <stdio.h>
      #include <gdbm/ndbm.h>
      DBM *dbm;
      ],[NULL])
  
      if test "$ac_cv_func_dbm_firstkey" = "yes"; then
        if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
	NDBMLIB="$ac_cv_funclib_dbm_firstkey"
        else
	NDBMLIB=""
        fi
        AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
        have_ndbm=yes
        if test "$db_type" = "unknown"; then
  	db_type=ndbm
        fi
      fi
    fi
  fi #enable_ndbm_db
fi # unknown

if test "$have_ndbm" = "yes"; then
  AC_MSG_CHECKING([if ndbm is implemented with db])
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined(HAVE_NDBM_H)
#include <ndbm.h>
#elif defined(HAVE_DBM_H)
#include <dbm.h>
#endif
int main(int argc, char **argv)
{
  DBM *d;

  d = dbm_open("conftest", O_RDWR | O_CREAT, 0666);
  if (d == NULL)
    return 1;
  dbm_close(d);
  return 0;
}]])],[
    if test -f conftest.db; then
      AC_MSG_RESULT([yes])
      AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files *.db)])
    else
      AC_MSG_RESULT([no])
    fi],[AC_MSG_RESULT([no])],[AC_MSG_RESULT([no-cross])])
fi

AM_CONDITIONAL(HAVE_DB1, test "$have_db1" = yes)dnl
AM_CONDITIONAL(HAVE_DB3, test "$have_db3" = yes)dnl
AM_CONDITIONAL(HAVE_LMDB, test "$have_lmdb" = yes)dnl
AM_CONDITIONAL(HAVE_NDBM, test "$have_ndbm" = yes)dnl
AM_CONDITIONAL(HAVE_DBHEADER, test "$dbheader" != "")dnl

## it's probably not correct to include LDFLAGS here, but we might
## need it, for now just add any possible -L
z=""
for i in $LDFLAGS; do
	case "$i" in
	-L*) z="$z $i";;
	esac
done
DB3LIB="$z $DB3LIB"
DB1LIB="$z $DB1LIB"
LMDBLIB="$z $LMDBLIB"
NDMBLIB="$z $NDBMLIB"
AC_SUBST(DB3LIB)dnl
AC_SUBST(DB1LIB)dnl
AC_SUBST(LMDBLIB)dnl
AC_SUBST(NDBMLIB)dnl
AC_SUBST(NDBMLIB)dnl
AC_SUBST(db_type)dnl
AC_SUBST(db_type_preference)dnl
])
