/*
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND STICHTING NLNET
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * STICHTING NLNET BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * The development of Dynamically Loadable Zones (DLZ) for Bind 9 was
 * conceived and contributed by Rob Butler.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND ROB BUTLER
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * ROB BUTLER BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef DLZ_BDB

/*
 * exit codes
 * 0 everything ok
 * 1 error parsing command line
 * 2 Missing, too many or invalid combination of command line parameters
 * 3 Unable to open BDB database.
 * 4 Unable to allocate memory for, or create lexer.
 * 5 unable to perform BDB cursor operation
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/formatcheck.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/util.h>

#include <db.h>

/* shut up compiler warnings about no previous prototype */

static void
show_usage(void);

int
getzone(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);

int
gethost(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);

void
bdb_cleanup(void);

isc_result_t
bdb_opendb(DBTYPE db_type, DB **db_out, const char *db_name, int flags);

void
put_data(isc_boolean_t dns_data, char *input_key, char *input_data);

void
insert_data(void);

isc_result_t
openBDB(void);

isc_result_t
open_lexer(void);

void
close_lexer(void);

isc_result_t
bulk_write(char type, DB *database, DBC *dbcursor, DBT *bdbkey, DBT *bdbdata);

void
operation_add(void);

void
operation_bulk(void);

void
operation_listOrDelete(isc_boolean_t dlt);

                                             
/*%
 * Maximum length of a single data line that 
 * may be inserted into database by this program.
 * If you need to insert a line of data that is more
 * than 10,000 characters change this definition.
 */

#define max_data_len 10000

/*%
 * BDB database names.  If you want to use different
 * database names change them here. 
 */

#define dlz_data "dns_data"
#define dlz_zone "dns_zone"
#define dlz_host "dns_host"
#define dlz_client "dns_client"


/*%
 * Error code returned by BDB secondary index callback functions.
 * This error is returned if the callback function could not create
 * the secondary index for any reason.
 */

#define BDBparseErr 1

/* A struct to hold all the relevant info about the database */

typedef struct bdb_instance {
	DB_ENV	*dbenv;		/* BDB environment */
	DB	*data;		/* dns_data database handle */
	DBC	*cursor;	/* database cursor */
	DBC	*cursor2; /* second cursor used during list operation. */
	DBC	*cursor3; /* third cursor used during list operation */
	DBC	*cursor4; /* fourth cursor used during list operation */
	DB	*zone;		/* zone database handle */
	DB	*host;		/* host database handle */
	DB	*client;	/* client database handle */
} bdb_instance_t;

/* Possible operations */

#define list 1		/* list data */
#define dele 2		/* delete data */
#define add 3		/* add a single piece of data */
#define bulk 4		/* bulk load data */


/*%
 * quit macro is used instead of exit.  quit always trys to close the lexer
 * and the BDB database before exiting.
 */

#define quit(i) 	close_lexer(); bdb_cleanup(); exit(i);

/*%
 * checkOp is used to verify that only one operation (list, del, add,
 * bulk from file, bulk from stdin) is specified on the command line.
 * This prevents a user from specifying two operations on the command
 * line, which would make no sense anyway.
 */

#define checkOp(x) if (x != 0) {fprintf(stderr, "\nonly one operation "\
				"(l e d a f s) may be specified\n"); quit(2);}

/*%
 * checkParam is used to only allow a parameter to be specified once.
 * I.E. the parameter key can only be used on the command line once.
 * any attempt to use it twice causes an error.
 */

#define checkParam(x, y) if (x != NULL) {fprintf(stderr, "\n%s may only "\
					 "be specified once\n", y); quit(2);}

/*%
 * checkInvalidParam is used to only allow paramters which make sense for
 * the operation selected.  I.E. passing the key parameter makes no sense
 * for the add operation, and thus it isn't allowed.
 */

#define checkInvalidParam(x, y, z) if (x != NULL) {fprintf(stderr, "\n%s "\
						"may not be specified %s\n", y, z); quit(2);}

/*%
 * checkInvalidOption is used to only allow paramters which make sense for
 * the operation selected - but checks boolean options.
 * I.E. passing the "b" bare_list parameter makes no sense for the add
 * operation, and thus it isn't allowed.
 * if w == x then output error message "flag", "message"
 */

#define checkInvalidOption(w, x, y, z) if (w == x) {fprintf(stderr, "\n%s "\
						"may not be specified %s\n", y, z); quit(2);}

/* Global Variables */

int operation = 0;		/*%< operation to perform. */
/*% allow new lock files or DB to be created. */
isc_boolean_t create_allowed = isc_boolean_false;
char *key = NULL;		/*%< key to use in list & del operations */

/*% dump DB in DLZBDB bulk format */
isc_boolean_t list_everything = isc_boolean_false;	
unsigned int key_val; /*%< key as unsigned int used in list & del operations */
char *zone = NULL;		/*%< zone to use in list operations */
char *host = NULL;		/*%< host to use in list operations */
char *c_zone = NULL;	 /*%< client zone to use in list operations */
char *c_ip = NULL;	   /*%< client IP to use in list operations */
char *a_data = NULL;		/*%< data in add operation */
char *bulk_file = NULL;		/*%< bulk data file to load */
char *db_envdir = NULL;		/*%< BDB environment location  */
char *db_file = NULL;		/*%< BDB database file location. */
bdb_instance_t db;		/* BDB instance we are operating on */
isc_lex_t *lexer = NULL; /*%< lexer for use to use in parsing input */
isc_mem_t *lex_mctx = NULL;	/*%< memory context for lexer */
char lex_data_buf[max_data_len]; /*%< data array to use for lex_buffer below */
isc_buffer_t lex_buffer;   /*%< buffer for lexer during add operation */
 

/*%
 * Displays usage message
 */

static void
show_usage(void) {
	fprintf(stderr, "\n\n\
---Usage:---------------------------------------------------------------------\
\n\n\
   List data:\n\
      dlzbdb -l [-k key] [-z zone] [-h host] [-c client_zone] [-i client_ip]\n\
               BDB_environment BDB_database\n\n\
   Delete data:\n\
      dlzbdb -d [-k key] [-c client_zone] [-i client_ip]\n\
               BDB_environment BDB_database\n\n\
   Bulk load data from file:\n\
      dlzbdb -f file_to_load BDB_environment BDB_database\n\n\
   Bulk load data from stdin\n\
      dlzbdb -s BDB_environment BDB_database\n\n\
   Add data:\n\
      dlzbdb -a \"dns data to be added\" BDB_environment BDB_database\n\n\
   Export data:\n\
      dlzbdb -e BDB_environment BDB_database\n\n\
   Normally operations can only be performed on an existing database files.\n\
   Use the -n flag with any operation to allow files to be created.\n\
   Existing files will NOT be truncated by using the -n flag.\n\
   The -n flag will allow a new database to be created, or allow new\n\
   environment files to be created for an existing database.\n\n\
---Format for -f & -a options:------------------------------------------------\
\n\n\
db_type zone host dns_type ttl ip\n\
db_type zone host dns_type ttl mx_priority mail_host\n\
db_type zone host dns_type ttl nm_svr resp_psn serial refresh retry expire min\
\n\
db_type zone client_ip\n\n\
---Examples:------------------------------------------------------------------\
\n\n\
d mynm.com www A 10 127.0.0.1\n\
d mynm.com @ MX 10 5 mail\n\
d mynm.com @ SOA 10 ns1.mynm.com. root.mynm.com. 2 28800 7200 604800 86400\n\
c mynm.com 127.0.0.1\n\
c mynm.com 192.168.0.10\n\
");
quit(1);
}


/*% BDB callback to create zone secondary index */

int
getzone(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey) {
	char *tmp;
	char *left;
	char *right;
	int result=0;

	UNUSED(dbp);
	UNUSED(pkey);

	/* Allocate memory to use in parsing the string */
	tmp = right = malloc(pdata->size + 1);

	/* verify memory was allocated */
	if (right == NULL) {
		result = BDBparseErr;
		goto getzone_cleanup;
	}		

	/* copy data string into newly allocated memory */
	strncpy(right, pdata->data, pdata->size);
	right[pdata->size] = '\0';

	/* split string at the first space */
	left = isc_string_separate(&right, " ");

	/* copy string for "zone" secondary index */
	skey->data = strdup(left);
	if (skey->data == NULL) {
		result = BDBparseErr;
		goto getzone_cleanup;
	}
	/* set required values for BDB */
	skey->size = strlen(skey->data);
	skey->flags = DB_DBT_APPMALLOC;

 getzone_cleanup:

	/* cleanup memory */
	if (tmp != NULL)
		free(tmp);
	
	return result;
}

/*%
 * BDB callback to create host secondary index
 */

int
gethost(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey) {
	char *tmp;
	char *left;
	char *right;
	int result=0;

	UNUSED(dbp);
	UNUSED(pkey);

	/* allocate memory to use in parsing the string */
	tmp = right = malloc(pdata->size + 1);

	/* verify memory was allocated */
	if (tmp == NULL) {
		result = BDBparseErr;
		goto gethost_cleanup;
	}		

	/* copy data string into newly allocated memory */
	strncpy(right, pdata->data, pdata->size);
	right[pdata->size] = '\0';

	/* we don't care about left string. */
	/* memory of left string will be freed when tmp is freed. */
	isc_string_separate(&right, " ");

	/* verify right still has some characters left */
	if (right == NULL) {
		result = BDBparseErr;
		goto gethost_cleanup;
	}

	/* get "host" from data string */
	left = isc_string_separate(&right, " ");
	/* copy string for "host" secondary index */
	skey->data = strdup(left);
	if (skey->data == NULL) {
		result = BDBparseErr;
		goto gethost_cleanup;
	}
	/* set required values for BDB */
	skey->size = strlen(skey->data);
	skey->flags = DB_DBT_APPMALLOC;

 gethost_cleanup:

	/* cleanup memory */
	if (tmp != NULL)
		free(tmp);
	
	return result;
}

/*%
 * Performs BDB cleanup. Close each database that we opened.
 * Close environment.  Set each var to NULL so we know they
 * were closed and don't accidentally try to close them twice.
 */

void
bdb_cleanup(void) {

	/* close cursors */
	if (db.cursor4 != NULL) {
		db.cursor4->c_close(db.cursor4);
		db.cursor4 = NULL;
	}

	if (db.cursor3 != NULL) {
		db.cursor3->c_close(db.cursor3);
		db.cursor3 = NULL;
	}

	if (db.cursor2 != NULL) {
		db.cursor2->c_close(db.cursor2);
		db.cursor2 = NULL;
	}

	if (db.cursor != NULL) {
		db.cursor->c_close(db.cursor);
		db.cursor = NULL;
	}

	/* close databases */
	if (db.data != NULL) {
		db.data->close(db.data, 0);
		db.data = NULL;
	}
	if (db.host != NULL) {
		db.host->close(db.host, 0);
		db.host = NULL;
	}
	if (db.zone != NULL) {
		db.zone->close(db.zone, 0);
		db.zone = NULL;
	}
	if (db.client != NULL) {
		db.client->close(db.client, 0);
		db.client = NULL;
	}

	/* close environment */
	if (db.dbenv != NULL) {
		db.dbenv->close(db.dbenv, 0);
		db.dbenv = NULL;
	}
}

/*% Initializes, sets flags and then opens Berkeley databases. */

isc_result_t
bdb_opendb(DBTYPE db_type, DB **db_out, const char *db_name, int flags) {

	int result;
	int createFlag = 0;

	/* Initialize the database. */
	if ((result = db_create(db_out, db.dbenv, 0)) != 0) {
		fprintf(stderr, "BDB could not initialize %s database. BDB error: %s",
			db_name, db_strerror(result));
		return ISC_R_FAILURE;
	}

	/* set database flags. */
	if ((result = (*db_out)->set_flags(*db_out, flags)) != 0) {
		fprintf(stderr, "BDB could not set flags for %s database. BDB error: %s",
			db_name, db_strerror(result));
		return ISC_R_FAILURE;
	}

	if (create_allowed == isc_boolean_true) {
		createFlag = DB_CREATE;
	}
	/* open the database. */
	if ((result = (*db_out)->open(*db_out, NULL, db_file, db_name, db_type,
				      createFlag, 0)) != 0) {
		fprintf(stderr, "BDB could not open %s database in %s. BDB error: %s",
			db_name, db_file, db_strerror(result));
		return ISC_R_FAILURE;
	}

	return ISC_R_SUCCESS;
}

/*%
 * parses input and adds it to the BDB database
 * Lexer should be instantiated, and either a file or buffer opened for it.
 * The insert_data function is used by both the add, and bulk insert
 * operations
 */

void
put_data(isc_boolean_t dns_data, char *input_key, char *input_data) {

	int bdbres;
	DBT key, data;

	/* make sure key & data are completely empty */
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/* if client data, setup key for insertion */
	if (!dns_data && input_key != NULL) {
		key.data = input_key;
		key.size = strlen(input_key);
		key.flags = 0;
	}
	/* always setup data for insertion */
	data.data = input_data;
	data.size = strlen(input_data);
	data.flags = 0;

	/* execute insert against appropriate database. */
	if (dns_data) {
		bdbres = db.data->put(db.data, NULL, &key, &data, DB_APPEND);
	} else {
		bdbres = db.client->put(db.client, NULL, &key, &data, 0);
	}

	/* if something went wrong, log error and quit */
	if (bdbres != 0) {
		fprintf(stderr, "BDB could not insert data.  Error: %s",
			db_strerror(bdbres));
		quit(5);
	}
}

void
insert_data(void) {
	unsigned int opt =
		ISC_LEXOPT_EOL |		/* Want end-of-line token. */
		ISC_LEXOPT_EOF | 		/* Want end-of-file token. */
		ISC_LEXOPT_QSTRING |		/* Recognize qstrings. */
		ISC_LEXOPT_QSTRINGMULTILINE;	/* Allow multiline "" strings */

	isc_result_t result;
	isc_token_t token;	/* token from lexer */
	isc_boolean_t loop = isc_boolean_true;
	isc_boolean_t have_czone = isc_boolean_false;
	char data_arr[max_data_len];
	isc_buffer_t buf;
	char data_arr2[max_data_len];
	isc_buffer_t buf2;
	char data_type = 'u'; /* u =unknown, b =bad token, d/D =DNS, c/C =client IP */

	/* Initialize buffers */
	isc_buffer_init(&buf, &data_arr, max_data_len);
	isc_buffer_init(&buf2, &data_arr2, max_data_len);

	while (loop) {
		result = isc_lex_gettoken(lexer, opt, &token); 
		if (result != ISC_R_SUCCESS) 
			goto data_cleanup;

		switch(token.type) {
		case isc_tokentype_string:
			if (data_type == 'u') {
				/* store data_type */
				strncpy(&data_type, token.value.as_pointer, 1);
				/* verify data_type was specified correctly on input */
				if (strlen(token.value.as_pointer) > 1 || (
					    data_type != 'd' && data_type != 'D' &&
					    data_type != 'c' && data_type != 'C') ) {
					/* if not, set to 'b' so this line is ignored. */
					data_type = 'b';
				}
			} else if (data_type == 'c' || data_type == 'C') {
				if (have_czone == isc_boolean_true) {
					isc_buffer_putstr(&buf2, token.value.as_pointer);
					/* add string terminator to buffer */
					isc_buffer_putmem(&buf2, "\0", 1);
				} else {
					isc_buffer_putstr(&buf, token.value.as_pointer);
					/* add string terminator to buffer */
					isc_buffer_putmem(&buf, "\0", 1);
					have_czone = isc_boolean_true;
				}
			} else {
				isc_buffer_putstr(&buf, token.value.as_pointer);
				isc_buffer_putstr(&buf, " ");
			}
			break;
		case isc_tokentype_qstring:
			isc_buffer_putstr(&buf, "\"");
			isc_buffer_putstr(&buf, token.value.as_pointer);
			isc_buffer_putstr(&buf, "\" ");
			break;
		case isc_tokentype_eol:
		case isc_tokentype_eof:
			
			if ((data_type != 'u' && isc_buffer_usedlength(&buf) > 0) || data_type == 'b') {
				/* perform insert operation */
				if (data_type == 'd' || data_type == 'D') {
					/* add string terminator to buffer */
					isc_buffer_putmem(&buf, "\0", 1);
					put_data(isc_boolean_true, NULL, (char *) &data_arr);
				} else if (data_type == 'c' || data_type == 'C') {
					put_data(isc_boolean_false, (char *) &data_arr, 
						 (char *) &data_arr2);
				} else if (data_type == 'b') {
					fprintf(stderr, "Bad / unknown token encountered on line %lu."\
						"  Skipping line.",	isc_lex_getsourceline(lexer) - 1);
				} else {
					fprintf(stderr, "Bad / unknown db data type encountered on " \
						"line %lu.  Skipping line\n", isc_lex_getsourceline(lexer) - 1);
				}
			}

			if (token.type == isc_tokentype_eof) {
				loop = isc_boolean_false;
			}	

			/* reset buffer for next insert */
			isc_buffer_clear(&buf);
			isc_buffer_clear(&buf2);
			have_czone = isc_boolean_false;
			data_type ='u';
			break;
		default:
			data_type = 'b';
			break;
		}
	}

	return;

 data_cleanup:
	/* let user know we had problems */
	fprintf(stderr, "Unknown error processing tokens during \"add\" or " \
		"\"bulk\" operation.\nStoped processing on line %lu.", 
		isc_lex_getsourceline(lexer));
}


isc_result_t
openBDB(void) {

	int bdbres;
	isc_result_t result;

	/* create BDB environment  */
	/* Basically BDB allocates and assigns memory to db->dbenv */
	bdbres = db_env_create(&db.dbenv, 0);
	if (bdbres != 0) {
		fprintf(stderr, "BDB environment could not be created. BDB error: %s", 
			db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto openBDB_cleanup;
	}

	/* open BDB environment */
	if (create_allowed == isc_boolean_true) {
		/* allowed to create new files */
		bdbres = db.dbenv->open(db.dbenv, db_envdir, 
					DB_INIT_CDB | DB_INIT_MPOOL | DB_CREATE, 0);
	} else {	/* not allowed to create new files. */
		bdbres = db.dbenv->open(db.dbenv, db_envdir, 
					DB_INIT_CDB | DB_INIT_MPOOL, 0);
	}
	if (bdbres != 0) {
		fprintf(stderr, "BDB environment at '%s' could not be opened. BDB " \
			"error: %s", db_envdir, db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto openBDB_cleanup;
	}

	/* open dlz_data database. */

	result = bdb_opendb(DB_RECNO, &db.data, dlz_data, 0);
	if (result != ISC_R_SUCCESS)
		goto openBDB_cleanup;
	
	/* open dlz_host database */
	result = bdb_opendb(DB_BTREE, &db.host, dlz_host, DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto openBDB_cleanup;

	/* open dlz_zone database. */
	result = bdb_opendb(DB_BTREE, &db.zone, dlz_zone, DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto openBDB_cleanup;

	/* open dlz_client database. */
	result = bdb_opendb(DB_BTREE, &db.client, dlz_client, DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto openBDB_cleanup;

	/* associate the host secondary database with the primary database */
	bdbres = db.data->associate(db.data, NULL, db.host, gethost, 0);
	if (bdbres != 0) {
		fprintf(stderr, "BDB could not associate %s database with %s. BDB "\
			"error: %s", dlz_host, dlz_data, db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto openBDB_cleanup;
	}

	/* associate the zone secondary database with the primary database */
	bdbres = db.data->associate(db.data, NULL, db.zone, getzone, 0);
	if (bdbres != 0) {
		fprintf(stderr, "BDB could not associate %s database with %s. BDB "\
			"error: %s", dlz_zone, dlz_data, db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto openBDB_cleanup;
	}

	return result;

 openBDB_cleanup:

	bdb_cleanup();
	return result;
}

/*% Create & open lexer to parse input data */

isc_result_t
open_lexer(void) {
	isc_result_t result;

	/* check if we already opened the lexer, if we did, return success */
	if (lexer != NULL)
		return ISC_R_SUCCESS;

	/* allocate memory for lexer, and verify it was allocated */
	result = isc_mem_create(0, 0, &lex_mctx);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "unexpected error creating lexer\n");
		return result;
	}

	/* create lexer */
	result = isc_lex_create(lex_mctx, 1500, &lexer);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "unexpected error creating lexer\n");

	/* set allowed commenting style */
	isc_lex_setcomments(lexer, ISC_LEXCOMMENT_C |	/* Allow C comments */
			    ISC_LEXCOMMENT_CPLUSPLUS |	/* Allow C++ comments */
			    ISC_LEXCOMMENT_SHELL);		/* Allow shellcomments */

	isc_buffer_init(&lex_buffer, &lex_data_buf, max_data_len);

	return result;
}

/*% Close the lexer, and cleanup memory */

void
close_lexer(void) {
		
	/* If lexer is still open, close it & destroy it. */
	if (lexer != NULL) {
		isc_lex_close(lexer);
		isc_lex_destroy(&lexer);
	}

	/* if lexer memory is still allocated, destroy it. */
	if (lex_mctx != NULL)
		isc_mem_destroy(&lex_mctx);
}

/*% Perform add operation */

void
operation_add(void) {
	/* check for any parameters that are not allowed during add */
	checkInvalidParam(key, "k", "for add operation");
	checkInvalidParam(zone, "z", "for add operation");
	checkInvalidParam(host, "h", "for add operation");
	checkInvalidParam(c_zone, "c", "for add operation");
	checkInvalidParam(c_ip, "i", "for add operation");
	checkInvalidOption(list_everything, isc_boolean_true, "e",
			   "for add operation");

	/* if open lexer fails it alread prints error messages. */
	if (open_lexer() != ISC_R_SUCCESS) {
		quit(4);
	}
	
	/* copy input data to buffer */
	isc_buffer_putstr(&lex_buffer, a_data);
		
	/* tell lexer to use buffer as input  */
	if (isc_lex_openbuffer(lexer, &lex_buffer) != ISC_R_SUCCESS) {
		fprintf(stderr, "unexpected error opening lexer buffer");
		quit(4);
	}

	/*common logic for "add" & "bulk" operations are handled by insert_data */
	insert_data();
}

/*% Perform bulk insert operation */

void
operation_bulk(void) {
	/* check for any parameters that are not allowed during bulk */
	checkInvalidParam(key, "k", "for bulk load operation");
	checkInvalidParam(zone, "z", "for bulk load operation");
	checkInvalidParam(host, "h", "for bulk load operation");
	checkInvalidParam(c_zone, "c", "for bulk load operation");
	checkInvalidParam(c_ip, "i", "for bulk load operation");
	checkInvalidOption(list_everything, isc_boolean_true, "e",
			   "for bulk load operation");

	/* if open lexer fails it already prints error messages. */
	if (open_lexer() != ISC_R_SUCCESS) {
		quit(4);
	}

	if (bulk_file == NULL) {
		if (isc_lex_openstream(lexer, stdin) != ISC_R_SUCCESS) {
			fprintf(stderr, "unexpected error opening stdin by lexer.");
			quit(4);				
		}
	} else if (isc_lex_openfile(lexer, bulk_file) != ISC_R_SUCCESS) {
		fprintf(stderr, "unexpected error opening %s by lexer.", bulk_file);
		quit(4);
	}

	/* common logic for "add" & "bulk" operations are handled by insert_data */
	insert_data();	
}

isc_result_t
bulk_write(char type, DB *database, DBC *dbcursor, DBT *bdbkey, DBT *bdbdata) {

	int bdbres;
	db_recno_t recNum;
	char *retkey = NULL, *retdata;
	size_t retklen = 0, retdlen;
	void *p;

	/* use a 5MB buffer for the bulk dump */
	int buffer_size = 5 * 1024 * 1024;
         
	/* try to allocate a 5 MB buffer, if we fail write err msg, die. */
	bdbdata->data = malloc(buffer_size);
	if (bdbdata->data == NULL) {
		fprintf(stderr,
			"Unable to allocate 5 MB buffer for bulk database dump\n");
		return ISC_R_FAILURE;
	}
	bdbdata->ulen = buffer_size;
	bdbdata->flags = DB_DBT_USERMEM;

	/* get a cursor, make sure it worked. */
	bdbres = database->cursor(database, NULL, &dbcursor, 0);
	if (bdbres != 0) {
		fprintf(stderr, "Unexpected error. BDB Error: %s\n",db_strerror(bdbres));
		free(bdbdata->data);
		return ISC_R_FAILURE;
	}

	/* loop and dump all data */
	for (;;) {

		/* loop through data until DB_NOTFOUND is returned */
		bdbres = dbcursor->c_get(dbcursor, bdbkey, bdbdata, 
					 DB_MULTIPLE_KEY | DB_NEXT);
		/* if not successful did we encounter DB_NOTFOUND, or */
		/* have  a different problem. */
		if (bdbres != 0) {
			if (bdbres != DB_NOTFOUND) {
				fprintf(stderr, "Unexpected error. BDB Error: %s\n",
					db_strerror(bdbres));
				free(bdbdata->data);
				return ISC_R_FAILURE;
			}
			/* Hit DB_NOTFOUND which means end of data. */
			break;
		} /* end of if (bdbres !=0) */

		for (DB_MULTIPLE_INIT(p, bdbdata);;) {
			if (type == 'c')
				DB_MULTIPLE_KEY_NEXT(p, bdbdata, retkey, retklen, retdata, retdlen);
			else
				DB_MULTIPLE_RECNO_NEXT(p, bdbdata, recNum, retdata, retdlen);

			if (p == NULL)
				break;
			if (type == 'c')
				printf("c %.*s %.*s\n",(int)retklen, retkey,(int)retdlen, retdata);
			else
				printf("d %.*s\n", (int)retdlen, retdata); 
		} /* end of for (DB_MULTIPLE_INIT....) */

	} /* end of for (;;) */

	/* free the buffer we created earlier */
	free(bdbdata->data);

	return ISC_R_SUCCESS;
}

/*%
 * Perform listOrDelete operation
 * if dlt == true, delete data
 * else list data
 */

void
operation_listOrDelete(isc_boolean_t dlt) {

	int bdbres = 0;
	DBC *curList[3];
	DBT bdbkey, bdbdata;
	db_recno_t recno;
	int curIndex = 0;


	/* verify that only allowed parameters were passed. */
	if (dlt == isc_boolean_true) {
		checkInvalidParam(zone, "z", "for delete operation");
		checkInvalidParam(host, "h", "for delete operation");
		checkInvalidOption(list_everything, isc_boolean_true, "e",
				   "for delete operation");
		checkInvalidOption(create_allowed, isc_boolean_true, "n",
				   "for delete operation");
	} else if (key != NULL || zone != NULL || host != NULL) {
		checkInvalidParam(c_zone, "c", "for list when k, z or h are specified");
		checkInvalidParam(c_ip, "i", "for list when k, z, or h are specified");
		checkInvalidOption(list_everything, isc_boolean_true, "e",
				   "for list when k, z, or h are specified");
		checkInvalidOption(create_allowed, isc_boolean_true, "n",
				   "for list operation");
	} else if (c_ip != NULL || c_zone != NULL) {
		checkInvalidOption(list_everything, isc_boolean_true, "e",
				   "for list when c or i are specified");
		checkInvalidOption(create_allowed, isc_boolean_true, "n",
				   "for list operation");
	}

	memset(&bdbkey, 0, sizeof(bdbkey));
	memset(&bdbdata, 0, sizeof(bdbdata));

	/* Dump database in "dlzbdb" bulk format */
	if (list_everything == isc_boolean_true) {
		if (bulk_write('c', db.client, db.cursor, &bdbkey, &bdbdata)
		    != ISC_R_SUCCESS)
			return;
		memset(&bdbkey, 0, sizeof(bdbkey));
		memset(&bdbdata, 0, sizeof(bdbdata));
		bulk_write('d', db.data, db.cursor2, &bdbkey, &bdbdata);
		return;
	} /* end if (list_everything) */

	/* set NULL the 2nd and 3rd positions in curList. */
	/* that way later when add cursors to the join list */
	/* it is already null terminated. */
	curList[1] = curList[2] = NULL;

	if (key != NULL) {
		/* make sure other parameters weren't */
		checkInvalidParam(zone, "z", "when k is specified");
		checkInvalidParam(host, "h", "when k is specified");

		recno = key_val;
		bdbkey.data = &recno;
		bdbkey.size = sizeof(recno);

		if (dlt == isc_boolean_true) {
			bdbres = db.data->del(db.data, NULL, &bdbkey, 0);
		} else {
			bdbdata.flags = DB_DBT_REALLOC;
			bdbres = db.data->get(db.data, NULL, &bdbkey, &bdbdata, 0);

			if (bdbres == 0) {
				printf("KEY | DATA\n");
				printf("%lu | %.*s\n", *(u_long *) bdbkey.data,
				       (int)bdbdata.size, (char *)bdbdata.data);
			}
		} /* closes else of if (dlt == isc_boolean_true) */
		if (bdbres == DB_NOTFOUND) {
			printf("Key not found in database");
		}
	}	/* closes if (key != NULL) */

		/* if zone is passed */
	if (zone != NULL) {
		/* create a cursor and make sure it worked */
		bdbres = db.zone->cursor(db.zone, NULL, &db.cursor2, 0);
		if (bdbres != 0) {
			fprintf(stderr, "Unexpected error. BDB Error: %s\n",
				db_strerror(bdbres));
			return;
		}

		bdbkey.data = zone;
		bdbkey.size = strlen(zone);
		bdbres = db.cursor2->c_get(db.cursor2, &bdbkey, &bdbdata, DB_SET);
		if (bdbres != 0) {
			if (bdbres != DB_NOTFOUND) {
				fprintf(stderr, "Unexpected error. BDB Error: %s\n",
					db_strerror(bdbres));
			} else {
				printf("Zone not found in database");
			}
			return;
		}

		/* add cursor to cursor list for later use in join */
		curList[curIndex++] = db.cursor2;
	}

	/* if host is passed */
	if (host != NULL) {

		/* create a cursor and make sure it worked. */
		bdbres = db.host->cursor(db.host, NULL, &db.cursor3, 0);
		if (bdbres != 0) {
			fprintf(stderr, "Unexpected error. BDB Error: %s\n",
				db_strerror(bdbres));
			return;
		}
		bdbkey.data = host;
		bdbkey.size = strlen(host);
		bdbres = db.cursor3->c_get(db.cursor3, &bdbkey, &bdbdata, DB_SET);
		if (bdbres != 0) {
			if (bdbres != DB_NOTFOUND) {
				fprintf(stderr, "Unexpected error. BDB Error: %s\n",
					db_strerror(bdbres));
			} else {
				printf("Host not found in database");
			}
			return;
		}

		/* add cursor to cursor list for later use in join */
		curList[curIndex++] = db.cursor3;
	}


	if (zone != NULL || host != NULL) {

		/* join any cursors */
		bdbres = db.data->join(db.data, curList, &db.cursor4, 0);
		if (bdbres != 0) {
			fprintf(stderr, "Unexpected error. BDB Error: %s\n",
				db_strerror(bdbres));
			return;
		}

		memset(&bdbkey, 0, sizeof(bdbkey));
		bdbkey.flags = DB_DBT_REALLOC;
		memset(&bdbdata, 0, sizeof(bdbdata));
		bdbdata.flags = DB_DBT_REALLOC;

		/* print a header to explain the output */
		printf("KEY | DATA\n");
		/* loop and list all results. */
		while (bdbres == 0) {	
			/* get data */
			bdbres = db.cursor4->c_get(db.cursor4, &bdbkey, &bdbdata, 0);
			/* verify call had no errors */
			if (bdbres != 0) {
				break;
			}
			printf("%lu | %.*s\n", *(u_long *) bdbkey.data,
			       (int)bdbdata.size, (char *)bdbdata.data);
		} /* closes while loop */
	}

	if (c_ip != NULL && c_zone == NULL) {
		fprintf(stderr, "i may only be specified when c is also specified\n");			
		quit(2);
	}
	/* if client_zone was passed */
	if (c_zone != NULL) {

		/* create a cursor and make sure it worked. */
		if (dlt == isc_boolean_true) {
			/* open read-write cursor */
			bdbres = db.client->cursor(db.client, NULL, &db.cursor,
						   DB_WRITECURSOR);
		} else {
			/* open read only cursor */
			bdbres = db.client->cursor(db.client, NULL, &db.cursor, 0);
			/* print a header to explain the output */
			printf("CLIENT_ZONE | CLIENT_IP\n");
		}

		bdbkey.data = c_zone;
		bdbkey.size = strlen(c_zone);

		if (c_ip != NULL) {
			bdbdata.data = c_ip;
			bdbdata.size = strlen(c_ip);
			bdbres = db.cursor->c_get(db.cursor, &bdbkey, &bdbdata, DB_GET_BOTH);
			if (bdbres == DB_NOTFOUND) {
				printf("Client zone & IP not found in database");
			}
		} else {
			bdbdata.flags = DB_DBT_REALLOC;
			bdbres = db.cursor->c_get(db.cursor, &bdbkey, &bdbdata, DB_SET);
			if (bdbres == DB_NOTFOUND) {
				printf("Client zone not found in database");
			}
		}

		while (bdbres == 0) {
			if (dlt == isc_boolean_false) {
				printf("%.*s | %.*s\n", (int)bdbkey.size, (char *) bdbkey.data,
				       (int)bdbdata.size, (char *) bdbdata.data);
			} else {
				/* delete record. */
				bdbres = db.cursor->c_del(db.cursor, 0);
				if (bdbres != 0) {
					fprintf(stderr, "Unexpected error. BDB Error: %s\n",
						db_strerror(bdbres));
					break;
				}
			}
			if (c_ip != NULL) {
				break;
			}
			bdbres = db.cursor->c_get(db.cursor, &bdbkey, &bdbdata, DB_NEXT_DUP);			
			if (bdbres != 0) {
				break;
			}
		} /* end while loop */
	}


	if (bdbres != 0 && bdbres != DB_NOTFOUND) {
		fprintf(stderr, "Unexpected error during list operation " \
			"BDB error: %s", db_strerror(bdbres));
	}

	if (bdbkey.flags == DB_DBT_REALLOC && bdbkey.data != NULL) {
		free(bdbkey.data);
	}
	if (bdbdata.flags == DB_DBT_REALLOC && bdbdata.data != NULL) {
		free(bdbdata.data);
	}
}


int
main(int argc, char **argv) {

	int ch;
	char *endp;

	/* there has to be at least 2 args, some operations require more */
	if (argc < 2)
		show_usage();

	/* use the ISC commandline parser to get all the program arguments */
	while ((ch= isc_commandline_parse(argc, argv, "ldesna:f:k:z:h:c:i:")) != -1) {
		switch (ch) {
		case 'n':
			create_allowed = isc_boolean_true;
			break;
		case 'l':
			checkOp(operation);
			operation = list;
			break;
		case 'd':
			checkOp(operation);
			operation = dele;
			break;
		case 'a':
			checkOp(operation);
			operation = add;
			a_data = isc_commandline_argument; 
			break;
		case 'f':
			checkOp(operation);
			operation = bulk;
			bulk_file = isc_commandline_argument;
			break;
		case 's':
			checkOp(operation);
			operation = bulk;
			break;
		case 'k':
			checkParam(key, "k");
			key = isc_commandline_argument;
			key_val = strtoul(key, &endp, 10);
			if (*endp != '\0' || key_val < 1) {
				fprintf(stderr, "Error converting key to integer");
			}
			break;
		case 'z':
			checkParam(zone, "z");
			zone = isc_commandline_argument;
			break;
		case 'h':
			checkParam(host, "h");
			host = isc_commandline_argument;
			break;
		case 'c':
			checkParam(c_zone, "c");
			c_zone = isc_commandline_argument;
			break;
		case 'i':
			checkParam(c_ip, "i");
			c_ip = isc_commandline_argument;
			break;
		case 'e':
			checkOp(operation);
			operation = list;
			list_everything = isc_boolean_true;
			break;
		case '?':
			show_usage();
			break;
		default:
			/* should never reach this point */
			fprintf(stderr, "unexpected error parsing command arguments\n");
			quit(1);
			break;
		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	/* argc & argv have been modified, so now only "extra" parameters are */
	/* left in argc & argv.  "Extra" parameters are any parameters that were */
	/* not passed using a command line flag.  Exactly 2 args should be left. */
	/* The first should be the BDB environment path, the second should be the */
	/* BDB database.  The BDB database path can be either relative to the */
	/* BDB environment path, or absolute. */
	if (argc < 2) {
		fprintf(stderr, "Both a Berkeley DB environment and file "\
			"must be specified");
		quit(2);
	} else if (argc > 2) {
		fprintf(stderr, "Too many parameters. Check command line for errors.");
		quit(2);
	}

	/* get db_file to operate on */
	db_envdir = argv[0];
	db_file = argv[1];

	if (openBDB() != ISC_R_SUCCESS) {
		/* openBDB already prints error messages, don't do it here. */
		bdb_cleanup();
		quit(3);
	}

	switch(operation) {
	case list:
		operation_listOrDelete(isc_boolean_false);
		break;
	case dele:
		operation_listOrDelete(isc_boolean_true);
		break;
	case add:
		operation_add();
		break;
	case bulk:
		operation_bulk();
		break;
	default:
		fprintf(stderr, "\nNo operation was selected. "\
			"Select an operation (l d a f)");
		quit(2);
		break;
	}

	quit(0);
}
#endif

