/*	$NetBSD: ipropd_slave.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "iprop.h"

__RCSID("$NetBSD: ipropd_slave.c,v 1.2 2017/01/28 21:31:49 christos Exp $");

static const char *config_name = "ipropd-slave";

static int verbose;

static krb5_log_facility *log_facility;
static char five_min[] = "5 min";
static char *server_time_lost = five_min;
static int time_before_lost;
const char *slave_str = NULL;

static int
connect_to_master (krb5_context context, const char *master,
		   const char *port_str)
{
    char port[NI_MAXSERV];
    struct addrinfo *ai, *a;
    struct addrinfo hints;
    int error;
    int one = 1;
    int s = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    if (port_str == NULL) {
	snprintf(port, sizeof(port), "%u", IPROP_PORT);
	port_str = port;
    }

    error = getaddrinfo(master, port_str, &hints, &ai);
    if (error) {
	krb5_warnx(context, "Failed to get address of to %s: %s",
		   master, gai_strerror(error));
	return -1;
    }

    for (a = ai; a != NULL; a = a->ai_next) {
	char node[NI_MAXHOST];
	error = getnameinfo(a->ai_addr, a->ai_addrlen,
			    node, sizeof(node), NULL, 0, NI_NUMERICHOST);
	if (error)
	    strlcpy(node, "[unknown-addr]", sizeof(node));

	s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	if (connect(s, a->ai_addr, a->ai_addrlen) < 0) {
	    krb5_warn(context, errno, "connection failed to %s[%s]",
		      master, node);
	    close(s);
	    continue;
	}
	krb5_warnx(context, "connection successful "
		   "to master: %s[%s]", master, node);
	break;
    }
    freeaddrinfo(ai);

    if (a == NULL)
	return -1;

    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) < 0)
        krb5_warn(context, errno, "setsockopt(SO_KEEPALIVE) failed");

    return s;
}

static void
get_creds(krb5_context context, const char *keytab_str,
	  krb5_ccache *cache, const char *serverhost)
{
    krb5_keytab keytab;
    krb5_principal client;
    krb5_error_code ret;
    krb5_get_init_creds_opt *init_opts;
    krb5_creds creds;
    char *server;
    char keytab_buf[256];
    int aret;

    if (keytab_str == NULL) {
	ret = krb5_kt_default_name (context, keytab_buf, sizeof(keytab_buf));
	if (ret)
	    krb5_err (context, 1, ret, "krb5_kt_default_name");
	keytab_str = keytab_buf;
    }

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "%s", keytab_str);


    ret = krb5_sname_to_principal (context, slave_str, IPROP_NAME,
				   KRB5_NT_SRV_HST, &client);
    if (ret) krb5_err(context, 1, ret, "krb5_sname_to_principal");

    ret = krb5_get_init_creds_opt_alloc(context, &init_opts);
    if (ret) krb5_err(context, 1, ret, "krb5_get_init_creds_opt_alloc");

    aret = asprintf (&server, "%s/%s", IPROP_NAME, serverhost);
    if (aret == -1 || server == NULL)
	krb5_errx (context, 1, "malloc: no memory");

    ret = krb5_get_init_creds_keytab(context, &creds, client, keytab,
				     0, server, init_opts);
    free (server);
    krb5_get_init_creds_opt_free(context, init_opts);
    if(ret) krb5_err(context, 1, ret, "krb5_get_init_creds");

    ret = krb5_kt_close(context, keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_close");

    ret = krb5_cc_new_unique(context, krb5_cc_type_memory, NULL, cache);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_new_unique");

    ret = krb5_cc_initialize(context, *cache, creds.client);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_initialize");

    ret = krb5_cc_store_cred(context, *cache, &creds);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_store_cred");

    krb5_free_cred_contents(context, &creds);
    krb5_free_principal(context, client);
}

static krb5_error_code
ihave(krb5_context context, krb5_auth_context auth_context,
      int fd, uint32_t version)
{
    int ret;
    u_char buf[8];
    krb5_storage *sp;
    krb5_data data;

    sp = krb5_storage_from_mem(buf, 8);
    ret = krb5_store_uint32(sp, I_HAVE);
    if (ret == 0)
        ret = krb5_store_uint32(sp, version);
    krb5_storage_free(sp);
    data.length = 8;
    data.data   = buf;

    if (ret == 0) {
        if (verbose)
            krb5_warnx(context, "telling master we are at %u", version);

        ret = krb5_write_priv_message(context, auth_context, &fd, &data);
        if (ret)
            krb5_warn(context, ret, "krb5_write_message");
    }
    return ret;
}

#ifndef EDQUOT
/* There's no EDQUOT on WIN32, for example */
#define EDQUOT ENOSPC
#endif

static int
append_to_log_file(krb5_context context,
                   kadm5_server_context *server_context,
                   krb5_storage *sp, off_t start, ssize_t slen)
{
    size_t len;
    ssize_t sret;
    off_t log_off;
    int ret, ret2;
    void *buf;

    if (verbose)
        krb5_warnx(context, "appending diffs to log");

    if (slen == 0)
        return 0;
    if (slen < 0)
        return EINVAL;
    len = slen;
    if (len != slen)
        return EOVERFLOW;

    buf = malloc(len);
    if (buf == NULL && len != 0) {
        krb5_warn(context, errno, "malloc: no memory");
        return ENOMEM;
    }

    if (krb5_storage_seek(sp, start, SEEK_SET) != start) {
        krb5_errx(context, IPROPD_RESTART,
                  "krb5_storage_seek() failed"); /* can't happen */
    }
    sret = krb5_storage_read(sp, buf, len);
    if (sret < 0)
        return errno;
    if (len != (size_t)sret) {
        /* Can't happen */
        krb5_errx(context, IPROPD_RESTART,
                  "short krb5_storage_read() from memory buffer");
    }
    log_off = lseek(server_context->log_context.log_fd, 0, SEEK_CUR);
    if (log_off == -1)
        return errno;

    /*
     * Use net_write() so we get an errno if less that len bytes were
     * written.
     */
    sret = net_write(server_context->log_context.log_fd, buf, len);
    free(buf);
    if (sret != slen)
        ret = errno;
    else
        ret = fsync(server_context->log_context.log_fd);
    if (ret == 0)
        return 0;

    /*
     * Attempt to recover from this.  First, truncate the log file
     * and reset the fd offset.  Failure to do this -> unlink the
     * log file and re-create it.  Since we're the slave, we ought to be
     * able to recover from the log being unlinked...
     */
    if (ftruncate(server_context->log_context.log_fd, log_off) == -1 ||
        lseek(server_context->log_context.log_fd, log_off, SEEK_SET) == -1) {
        (void) kadm5_log_end(server_context);
        if (unlink(server_context->log_context.log_file) == -1) {
            krb5_err(context, IPROPD_FATAL, errno,
                     "Failed to recover from failure to write log "
                     "entries from master to disk");
        }
        ret2 = kadm5_log_init(server_context);
        if (ret2) {
            krb5_err(context, IPROPD_RESTART_SLOW, ret2,
                     "Failed to initialize log to recover from "
                     "failure to write log entries from master to disk");
        }
    }
    if (ret == ENOSPC || ret == EDQUOT || ret == EFBIG) {
        /* Unlink the file in these cases. */
        krb5_warn(context, IPROPD_RESTART_SLOW,
                  "Failed to write log entries from master to disk");
        (void) kadm5_log_end(server_context);
        if (unlink(server_context->log_context.log_file) == -1) {
            krb5_err(context, IPROPD_FATAL, errno,
                     "Failed to recover from failure to write log "
                     "entries from master to disk");
        }
        ret2 = kadm5_log_init(server_context);
        if (ret2) {
            krb5_err(context, IPROPD_RESTART_SLOW, ret2,
                     "Failed to initialize log to recover from "
                     "failure to write log entries from master to disk");
        }
        return ret;
    }
    /*
     * All other errors we treat as fatal here.  This includes, for
     * example, EIO and EPIPE (sorry, can't log to pipes nor sockets).
     */
    krb5_err(context, IPROPD_FATAL, ret,
             "Failed to write log entries from master to disk");
}

static int
receive_loop (krb5_context context,
	      krb5_storage *sp,
	      kadm5_server_context *server_context)
{
    int ret;
    off_t left, right, off;
    uint32_t len, vers;

    if (verbose)
        krb5_warnx(context, "receiving diffs");

    /*
     * Seek to the first entry in the message from the master that is
     * past the current version of the local database.
     */
    do {
	uint32_t timestamp;
        uint32_t op;

        if ((ret = krb5_ret_uint32(sp, &vers)) == HEIM_ERR_EOF) {
            krb5_warnx(context, "master sent no new iprop entries");
            return 0;
        }

        /*
         * TODO We could do more to validate the entries from the master
         * here.  And we could use/reuse more kadm5_log_*() code here.
         *
         * Alternatively we should trust that the master sent us exactly
         * what we needed and just write this to the log file and let
         * kadm5_log_recover() do the rest.
         */
	if (ret || krb5_ret_uint32(sp, &timestamp) != 0 ||
            krb5_ret_uint32(sp, &op) != 0 ||
            krb5_ret_uint32(sp, &len) != 0) {

            /*
             * This shouldn't happen.  Reconnecting probably won't help
             * if it does happen, but by reconnecting we get a chance to
             * connect to a new master if a new one is configured.
             */
            krb5_warnx(context, "iprop entries from master were truncated");
            return EINVAL;
        }
	if (vers > server_context->log_context.version) {
            break;
        }
        off = krb5_storage_seek(sp, 0, SEEK_CUR);
        if (krb5_storage_seek(sp, len + 8, SEEK_CUR) != off + len + 8) {
            krb5_warnx(context, "iprop entries from master were truncated");
            return EINVAL;
        }
        if (verbose) {
            krb5_warnx(context, "diff contains old log record version "
                       "%u %lld %u length %u",
                       vers, (long long)timestamp, op, len);
        }
    } while(vers <= server_context->log_context.version);

    /*
     * Read the remaining entries into memory...
     */
    /* SEEK_CUR is a header into the first entry we care about */
    left  = krb5_storage_seek(sp, -16, SEEK_CUR);
    right = krb5_storage_seek(sp, 0, SEEK_END);
    if (right - left < 24 + len) {
        krb5_warnx(context, "iprop entries from master were truncated");
        return EINVAL;
    }

    /*
     * ...and then write them out to the on-disk log.
     */

    ret = append_to_log_file(context, server_context, sp, left, right - left);
    if (ret)
        return ret;

    /*
     * Replay the new entries.
     */
    if (verbose)
        krb5_warnx(context, "replaying entries from master");
    ret = kadm5_log_recover(server_context, kadm_recover_replay);
    if (ret) {
        krb5_warn(context, ret, "replay failed");
        return ret;
    }

    ret = kadm5_log_get_version(server_context, &vers);
    if (ret) {
        krb5_warn(context, ret,
                  "could not get log version after applying diffs!");
        return ret;
    }
    if (verbose)
        krb5_warnx(context, "slave at version %u", vers);

    if (vers != server_context->log_context.version) {
        krb5_warnx(context, "slave's log_context version (%u) is "
                   "inconsistent with log's version (%u)",
                   server_context->log_context.version, vers);
    }

    return 0;
}

static int
receive(krb5_context context,
        krb5_storage *sp,
        kadm5_server_context *server_context)
{
    krb5_error_code ret, ret2;

    ret = server_context->db->hdb_open(context,
				       server_context->db,
				       O_RDWR | O_CREAT, 0600);
    if (ret)
        krb5_err(context, IPROPD_RESTART_SLOW, ret, "db->open");

    ret2 = receive_loop(context, sp, server_context);
    if (ret2)
	krb5_warn(context, ret2, "receive from ipropd-master had errors");

    ret = server_context->db->hdb_close(context, server_context->db);
    if (ret)
        krb5_err(context, IPROPD_RESTART_SLOW, ret, "db->close");

    return ret2;
}

static void
send_im_here(krb5_context context, int fd,
	     krb5_auth_context auth_context)
{
    krb5_storage *sp;
    krb5_data data;
    krb5_error_code ret;

    ret = krb5_data_alloc(&data, 4);
    if (ret)
        krb5_err(context, IPROPD_RESTART, ret, "send_im_here");

    sp = krb5_storage_from_data (&data);
    if (sp == NULL)
        krb5_errx(context, IPROPD_RESTART, "krb5_storage_from_data");
    ret = krb5_store_uint32(sp, I_AM_HERE);
    krb5_storage_free(sp);

    if (ret == 0) {
        ret = krb5_write_priv_message(context, auth_context, &fd, &data);
        krb5_data_free(&data);

        if (ret)
            krb5_err(context, IPROPD_RESTART, ret, "krb5_write_priv_message");

        if (verbose)
            krb5_warnx(context, "pinged master");
    }

    return;
}

static void
reinit_log(krb5_context context,
	   kadm5_server_context *server_context,
	   uint32_t vno)
{
    krb5_error_code ret;

    if (verbose)
        krb5_warnx(context, "truncating log on slave");

    ret = kadm5_log_reinit(server_context, vno);
    if (ret)
        krb5_err(context, IPROPD_RESTART_SLOW, ret, "kadm5_log_reinit");
}


static krb5_error_code
receive_everything(krb5_context context, int fd,
		   kadm5_server_context *server_context,
		   krb5_auth_context auth_context)
{
    int ret;
    krb5_data data;
    uint32_t vno = 0;
    uint32_t opcode;
    krb5_storage *sp;

    char *dbname;
    HDB *mydb;

    krb5_warnx(context, "receive complete database");

    ret = asprintf(&dbname, "%s-NEW", server_context->db->hdb_name);
    if (ret == -1)
        krb5_err(context, IPROPD_RESTART, ENOMEM, "asprintf");
    ret = hdb_create(context, &mydb, dbname);
    if(ret)
        krb5_err(context, IPROPD_RESTART, ret, "hdb_create");
    free(dbname);

    ret = hdb_set_master_keyfile(context,
				 mydb, server_context->config.stash_file);
    if(ret)
        krb5_err(context, IPROPD_RESTART, ret, "hdb_set_master_keyfile");

    /* I really want to use O_EXCL here, but given that I can't easily clean
       up on error, I won't */
    ret = mydb->hdb_open(context, mydb, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (ret)
        krb5_err(context, IPROPD_RESTART, ret, "db->open");

    sp = NULL;
    krb5_data_zero(&data);
    do {
	ret = krb5_read_priv_message(context, auth_context, &fd, &data);

	if (ret) {
	    krb5_warn(context, ret, "krb5_read_priv_message");
	    goto cleanup;
	}

	sp = krb5_storage_from_data(&data);
	if (sp == NULL)
	    krb5_errx(context, IPROPD_RESTART, "krb5_storage_from_data");
	krb5_ret_uint32(sp, &opcode);
	if (opcode == ONE_PRINC) {
	    krb5_data fake_data;
	    hdb_entry_ex entry;

	    krb5_storage_free(sp);

	    fake_data.data   = (char *)data.data + 4;
	    fake_data.length = data.length - 4;

	    memset(&entry, 0, sizeof(entry));

	    ret = hdb_value2entry(context, &fake_data, &entry.entry);
	    if (ret)
		krb5_err(context, IPROPD_RESTART, ret, "hdb_value2entry");
	    ret = mydb->hdb_store(server_context->context,
				  mydb,
				  0, &entry);
	    if (ret)
		krb5_err(context, IPROPD_RESTART_SLOW, ret, "hdb_store");

	    hdb_free_entry(context, &entry);
	    krb5_data_free(&data);
	} else if (opcode == NOW_YOU_HAVE)
	    ;
	else
	    krb5_errx(context, 1, "strange opcode %d", opcode);
    } while (opcode == ONE_PRINC);

    if (opcode != NOW_YOU_HAVE)
        krb5_errx(context, IPROPD_RESTART_SLOW,
                  "receive_everything: strange %d", opcode);

    krb5_ret_uint32(sp, &vno);
    krb5_storage_free(sp);

    reinit_log(context, server_context, vno);

    ret = mydb->hdb_close(context, mydb);
    if (ret)
        krb5_err(context, IPROPD_RESTART_SLOW, ret, "db->close");

    ret = mydb->hdb_rename(context, mydb, server_context->db->hdb_name);
    if (ret)
        krb5_err(context, IPROPD_RESTART_SLOW, ret, "db->rename");


    return 0;

 cleanup:
    krb5_data_free(&data);

    if (ret)
        krb5_err(context, IPROPD_RESTART_SLOW, ret, "db->close");

    ret = mydb->hdb_destroy(context, mydb);
    if (ret)
        krb5_err(context, IPROPD_RESTART, ret, "db->destroy");

    krb5_warnx(context, "receive complete database, version %ld", (long)vno);
    return ret;
}

static void
slave_status(krb5_context context,
	     const char *file,
	     const char *status, ...)
     __attribute__ ((__format__ (__printf__, 3, 4)));


static void
slave_status(krb5_context context,
	     const char *file,
	     const char *fmt, ...)
{
    char *status;
    char *fmt2;
    va_list args;
    int len;
    
    if (asprintf(&fmt2, "%s\n", fmt) == -1 || fmt2 == NULL) {
        (void) unlink(file);
        return;
    }
    va_start(args, fmt);
    len = vasprintf(&status, fmt2, args);
    free(fmt2);
    va_end(args);
    if (len < 0 || status == NULL) {
	(void) unlink(file);
	return;
    }
    krb5_warnx(context, "slave status change: %s", status);
    
    rk_dumpdata(file, status, len);
    free(status);
}

static void
is_up_to_date(krb5_context context, const char *file,
	      kadm5_server_context *server_context)
{
    krb5_error_code ret;
    char buf[80];
    ret = krb5_format_time(context, time(NULL), buf, sizeof(buf), 1);
    if (ret) {
	unlink(file);
	return;
    }
    slave_status(context, file, "up-to-date with version: %lu at %s",
		 (unsigned long)server_context->log_context.version, buf);
}

static char *status_file;
static char *config_file;
static char *realm;
static int version_flag;
static int help_flag;
static char *keytab_str;
static char *port_str;
static int detach_from_console;
static int daemon_child = -1;

static struct getargs args[] = {
    { "config-file", 'c', arg_string, &config_file, NULL, NULL },
    { "realm", 'r', arg_string, &realm, NULL, NULL },
    { "keytab", 'k', arg_string, &keytab_str,
      "keytab to get authentication from", "kspec" },
    { "time-lost", 0, arg_string, &server_time_lost,
      "time before server is considered lost", "time" },
    { "status-file", 0, arg_string, &status_file,
      "file to write out status into", "file" },
    { "port", 0, arg_string, &port_str,
      "port ipropd-slave will connect to", "port"},
    { "detach", 0, arg_flag, &detach_from_console,
      "detach from console", NULL },
    { "daemon-child",       0 ,      arg_integer, &daemon_child,
      "private argument, do not use", NULL },
    { "hostname", 0, arg_string, rk_UNCONST(&slave_str),
      "hostname of slave (if not same as hostname)", "hostname" },
    { "verbose", 0, arg_flag, &verbose, NULL, NULL },
    { "version", 0, arg_flag, &version_flag, NULL, NULL },
    { "help", 0, arg_flag, &help_flag, NULL, NULL }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int status)
{
    arg_printusage(args, num_args, NULL, "master");
    exit(status);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret, ret2;
    krb5_context context;
    krb5_auth_context auth_context;
    void *kadm_handle;
    kadm5_server_context *server_context;
    kadm5_config_params conf;
    int master_fd;
    krb5_ccache ccache;
    krb5_principal server;
    char **files;
    int optidx = 0;
    time_t reconnect_min;
    time_t backoff;
    time_t reconnect_max;
    time_t reconnect;
    time_t before = 0;
    int restarter_fd = -1;

    const char *master;

    setprogname(argv[0]);

    if (getarg(args, num_args, argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage(0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    if (detach_from_console && daemon_child == -1)
        roken_detach_prep(argc, argv, "--daemon-child");
    rk_pidfile(NULL);

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    setup_signal();

    if (config_file == NULL) {
	if (asprintf(&config_file, "%s/kdc.conf", hdb_db_dir(context)) == -1
	    || config_file == NULL)
	    errx(1, "out of memory");
    }

    ret = krb5_prepend_config_files_default(config_file, &files);
    if (ret)
	krb5_err(context, 1, ret, "getting configuration files");

    ret = krb5_set_config_files(context, files);
    krb5_free_config_files(files);
    if (ret)
	krb5_err(context, 1, ret, "reading configuration files");

    argc -= optidx;
    argv += optidx;

    if (argc != 1)
	usage(1);

    master = argv[0];

    if (status_file == NULL) {
	if (asprintf(&status_file,  "%s/ipropd-slave-status", hdb_db_dir(context)) < 0 || status_file == NULL)
	    krb5_errx(context, 1, "can't allocate status file buffer"); 
    }

    krb5_openlog(context, "ipropd-slave", &log_facility);
    krb5_set_warn_dest(context, log_facility);

    slave_status(context, status_file, "bootstrapping");

    ret = krb5_kt_register(context, &hdb_get_kt_ops);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_register");

    time_before_lost = parse_time (server_time_lost,  "s");
    if (time_before_lost < 0)
	krb5_errx (context, 1, "couldn't parse time: %s", server_time_lost);

    slave_status(context, status_file, "getting credentials from keytab/database");

    memset(&conf, 0, sizeof(conf));
    if(realm) {
	conf.mask |= KADM5_CONFIG_REALM;
	conf.realm = realm;
    }
    ret = kadm5_init_with_password_ctx (context,
					KADM5_ADMIN_SERVICE,
					NULL,
					KADM5_ADMIN_SERVICE,
					&conf, 0, 0,
					&kadm_handle);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_init_with_password_ctx");

    server_context = (kadm5_server_context *)kadm_handle;

    slave_status(context, status_file, "creating log file");

    ret = kadm5_log_init (server_context);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_log_init");

    get_creds(context, keytab_str, &ccache, master);

    ret = krb5_sname_to_principal (context, master, IPROP_NAME,
				   KRB5_NT_SRV_HST, &server);
    if (ret)
	krb5_err (context, 1, ret, "krb5_sname_to_principal");

    auth_context = NULL;
    master_fd = -1;

    krb5_appdefault_time(context, config_name, NULL, "reconnect-min",
			 10, &reconnect_min);
    krb5_appdefault_time(context, config_name, NULL, "reconnect-max",
			 300, &reconnect_max);
    krb5_appdefault_time(context, config_name, NULL, "reconnect-backoff",
			 10, &backoff);
    reconnect = reconnect_min;

    slave_status(context, status_file, "ipropd-slave started");

    roken_detach_finish(NULL, daemon_child);
    restarter_fd = restarter(context, NULL);

    while (!exit_flag) {
        struct timeval to;
	time_t now, elapsed;
        fd_set readset;
	int connected = FALSE;

#ifndef NO_LIMIT_FD_SETSIZE
        if (restarter_fd >= FD_SETSIZE)
            krb5_errx(context, IPROPD_RESTART, "fd too large");
#endif

        FD_ZERO(&readset);
        if (restarter_fd > -1)
            FD_SET(restarter_fd, &readset);

	now = time(NULL);
	elapsed = now - before;

	if (elapsed < reconnect) {
	    time_t left = reconnect - elapsed;
	    krb5_warnx(context, "sleeping %d seconds before "
		       "retrying to connect", (int)left);
            to.tv_sec = left;
            to.tv_usec = 0;
            if (select(restarter_fd + 1, &readset, NULL, NULL, &to) == 1) {
                exit_flag = SIGTERM;
                continue;
            }
	}
	before = now;

	slave_status(context, status_file, "connecting to master: %s\n", master);

	master_fd = connect_to_master (context, master, port_str);
	if (master_fd < 0)
	    goto retry;

	reconnect = reconnect_min;

	if (auth_context) {
	    krb5_auth_con_free(context, auth_context);
	    auth_context = NULL;
	    krb5_cc_destroy(context, ccache);
	    get_creds(context, keytab_str, &ccache, master);
	}
        if (verbose)
            krb5_warnx(context, "authenticating to master");
	ret = krb5_sendauth (context, &auth_context, &master_fd,
			     IPROP_VERSION, NULL, server,
			     AP_OPTS_MUTUAL_REQUIRED, NULL, NULL,
			     ccache, NULL, NULL, NULL);
	if (ret) {
	    krb5_warn (context, ret, "krb5_sendauth");
	    goto retry;
	}

	krb5_warnx(context, "ipropd-slave started at version: %ld",
		   (long)server_context->log_context.version);

	ret = ihave(context, auth_context, master_fd,
		    server_context->log_context.version);
	if (ret)
	    goto retry;

	connected = TRUE;

        if (verbose)
            krb5_warnx(context, "connected to master");

	slave_status(context, status_file, "connected to master, waiting instructions");

	while (connected && !exit_flag) {
	    krb5_data out;
	    krb5_storage *sp;
	    uint32_t tmp;
            int max_fd;

#ifndef NO_LIMIT_FD_SETSIZE
	    if (master_fd >= FD_SETSIZE)
                krb5_errx(context, IPROPD_RESTART, "fd too large");
            if (restarter_fd >= FD_SETSIZE)
                krb5_errx(context, IPROPD_RESTART, "fd too large");
            max_fd = max(restarter_fd, master_fd);
#endif

	    FD_ZERO(&readset);
	    FD_SET(master_fd, &readset);
            if (restarter_fd != -1)
                FD_SET(restarter_fd, &readset);

	    to.tv_sec = time_before_lost;
	    to.tv_usec = 0;

	    ret = select (max_fd + 1,
			  &readset, NULL, NULL, &to);
	    if (ret < 0) {
		if (errno == EINTR)
		    continue;
		else
		    krb5_err (context, 1, errno, "select");
	    }
	    if (ret == 0) {
		krb5_warnx(context, "server didn't send a message "
                           "in %d seconds", time_before_lost);
		connected = FALSE;
		continue;
	    }

            if (restarter_fd > -1 && FD_ISSET(restarter_fd, &readset)) {
                if (verbose)
                    krb5_warnx(context, "slave restarter exited");
                exit_flag = SIGTERM;
            }

            if (!FD_ISSET(master_fd, &readset))
                continue;

            if (verbose)
                krb5_warnx(context, "message from master");

	    ret = krb5_read_priv_message(context, auth_context, &master_fd, &out);
	    if (ret) {
		krb5_warn(context, ret, "krb5_read_priv_message");
		connected = FALSE;
		continue;
	    }

	    sp = krb5_storage_from_mem (out.data, out.length);
            if (sp == NULL)
                krb5_err(context, IPROPD_RESTART, errno, "krb5_storage_from_mem");
	    ret = krb5_ret_uint32(sp, &tmp);
            if (ret == HEIM_ERR_EOF) {
                krb5_warn(context, ret, "master sent zero-length message");
                connected = FALSE;
                continue;
            }
            if (ret != 0) {
                krb5_warn(context, ret, "couldn't read master's message");
                connected = FALSE;
                continue;
            }

            ret = kadm5_log_init(server_context);
            if (ret) {
                krb5_err(context, IPROPD_RESTART, ret, "kadm5_log_init while "
                         "handling a message from the master");
            }
	    switch (tmp) {
	    case FOR_YOU :
                if (verbose)
                    krb5_warnx(context, "master sent us diffs");
		ret2 = receive(context, sp, server_context);
                if (ret2)
                    krb5_warn(context, ret2,
                              "receive from ipropd-master had errors");
		ret = ihave(context, auth_context, master_fd,
			    server_context->log_context.version);
		if (ret || ret2)
		    connected = FALSE;

                /*
                 * If it returns an error, receive() may nonetheless
                 * have committed some entries successfully, so we must
                 * update the slave_status even if there were errors.
                 */
                is_up_to_date(context, status_file, server_context);
		break;
	    case TELL_YOU_EVERYTHING :
                if (verbose)
                    krb5_warnx(context, "master sent us a full dump");
		ret = receive_everything(context, master_fd, server_context,
					 auth_context);
                if (ret == 0) {
                    ret = ihave(context, auth_context, master_fd,
                                server_context->log_context.version);
                }
                if (ret)
		    connected = FALSE;
                else
                    is_up_to_date(context, status_file, server_context);
		break;
	    case ARE_YOU_THERE :
                if (verbose)
                    krb5_warnx(context, "master sent us a ping");
		is_up_to_date(context, status_file, server_context);
                ret = ihave(context, auth_context, master_fd,
                            server_context->log_context.version);
                if (ret)
                    connected = FALSE;

		send_im_here(context, master_fd, auth_context);
		break;
	    case YOU_HAVE_LAST_VERSION:
                if (verbose)
                    krb5_warnx(context, "master tells us we are up to date");
		is_up_to_date(context, status_file, server_context);
		break;
	    case NOW_YOU_HAVE :
	    case I_HAVE :
	    case ONE_PRINC :
	    case I_AM_HERE :
	    default :
		krb5_warnx (context, "Ignoring command %d", tmp);
		break;
	    }
	    krb5_storage_free (sp);
	    krb5_data_free (&out);

	}

	slave_status(context, status_file, "disconnected from master");
    retry:
	if (connected == FALSE)
	    krb5_warnx (context, "disconnected for server");

	if (exit_flag)
	    krb5_warnx (context, "got an exit signal");

	if (master_fd >= 0)
	    close(master_fd);

	reconnect += backoff;
	if (reconnect > reconnect_max) {
	    slave_status(context, status_file, "disconnected from master for a long time");
	    reconnect = reconnect_max;
	}
    }

    if (status_file) {
        /* XXX It'd be better to leave it saying we're not here */
	unlink(status_file);
    }

    if (0);
#ifndef NO_SIGXCPU
    else if(exit_flag == SIGXCPU)
	krb5_warnx(context, "%s CPU time limit exceeded", getprogname());
#endif
    else if(exit_flag == SIGINT || exit_flag == SIGTERM)
	krb5_warnx(context, "%s terminated", getprogname());
    else
	krb5_warnx(context, "%s unexpected exit reason: %ld",
		       getprogname(), (long)exit_flag);

    return 0;
}
