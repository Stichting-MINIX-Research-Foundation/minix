#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: media.c,v 1.6 2011/08/29 14:35:00 joerg Exp $");
#endif /* not lint */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <prop/proplib.h>

#include "env.h"
#include "extern.h"
#include "media.h"
#include "parse.h"
#include "util.h"
#include "prog_ops.h"

static void init_current_media(prop_dictionary_t, prop_dictionary_t);
static void media_constructor(void) __attribute__((constructor));
static int setmedia(prop_dictionary_t, prop_dictionary_t);
static int setmediainst(prop_dictionary_t, prop_dictionary_t);
static int setmediamode(prop_dictionary_t, prop_dictionary_t);
static int setmediaopt(prop_dictionary_t, prop_dictionary_t);
static int unsetmediaopt(prop_dictionary_t, prop_dictionary_t);

/*
 * Media stuff.  Whenever a media command is first performed, the
 * currently select media is grabbed for this interface.  If `media'
 * is given, the current media word is modifed.  `mediaopt' commands
 * only modify the set and clear words.  They then operate on the
 * current media word later.
 */
static int	media_current;
static int	mediaopt_set;
static int	mediaopt_clear;

static struct usage_func usage;

static const int ifm_status_valid_list[] = IFM_STATUS_VALID_LIST;

static const struct ifmedia_status_description ifm_status_descriptions[] =
    IFM_STATUS_DESCRIPTIONS;

static struct pstr mediamode = PSTR_INITIALIZER1(&mediamode, "mediamode",
    setmediamode, "mediamode", false, &command_root.pb_parser);

static struct pinteger mediainst = PINTEGER_INITIALIZER1(&mediainst,
    "mediainst", 0, IFM_INST_MAX, 10, setmediainst, "mediainst",
    &command_root.pb_parser);

static struct pstr unmediaopt = PSTR_INITIALIZER1(&unmediaopt, "-mediaopt",
    unsetmediaopt, "unmediaopt", false, &command_root.pb_parser);

static struct pstr mediaopt = PSTR_INITIALIZER1(&mediaopt, "mediaopt",
    setmediaopt, "mediaopt", false, &command_root.pb_parser);

static struct pstr media = PSTR_INITIALIZER1(&media, "media", setmedia, "media",
    false, &command_root.pb_parser);

static const struct kwinst mediakw[] = {
	  {.k_word = "instance", .k_key = "anymedia", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_act = "media", .k_deact = "mediainst",
	   .k_nextparser = &mediainst.pi_parser}
	, {.k_word = "inst", .k_key = "anymedia", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_act = "media", .k_deact = "mediainst",
	   .k_nextparser = &mediainst.pi_parser}
	, {.k_word = "media", .k_key = "anymedia", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_deact = "media", .k_altdeact = "anymedia",
	   .k_nextparser = &media.ps_parser}
	, {.k_word = "mediaopt", .k_key = "anymedia", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_deact = "mediaopt", .k_altdeact = "instance",
	   .k_nextparser = &mediaopt.ps_parser}
	, {.k_word = "-mediaopt", .k_key = "anymedia", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_deact = "unmediaopt", .k_altdeact = "media",
	   .k_nextparser = &unmediaopt.ps_parser}
	, {.k_word = "mode", .k_key = "anymedia", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_deact = "mode",
	   .k_nextparser = &mediamode.ps_parser}
};

struct pkw kwmedia = PKW_INITIALIZER(&kwmedia, "media keywords", NULL, NULL,
    mediakw, __arraycount(mediakw), NULL);

__dead static void
media_error(int type, const char *val, const char *opt)
{
	errx(EXIT_FAILURE, "unknown %s media %s: %s",
		get_media_type_string(type), opt, val);
}

void
init_current_media(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const char *ifname;
	struct ifmediareq ifmr;

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "getifname");

	/*
	 * If we have not yet done so, grab the currently-selected
	 * media.
	 */

	if (prop_dictionary_get(env, "initmedia") == NULL) {
		memset(&ifmr, 0, sizeof(ifmr));

		if (direct_ioctl(env, SIOCGIFMEDIA, &ifmr) == -1) {
			/*
			 * If we get E2BIG, the kernel is telling us
			 * that there are more, so we can ignore it.
			 */
			if (errno != E2BIG)
				err(EXIT_FAILURE, "SIOCGIFMEDIA");
		}

		if (!prop_dictionary_set_bool(oenv, "initmedia", true)) {
			err(EXIT_FAILURE, "%s: prop_dictionary_set_bool",
			    __func__);
		}
		media_current = ifmr.ifm_current;
	}

	/* Sanity. */
	if (IFM_TYPE(media_current) == 0)
		errx(EXIT_FAILURE, "%s: no link type?", ifname);
}

void
process_media_commands(prop_dictionary_t env)
{
	struct ifreq ifr;

	if (prop_dictionary_get(env, "media") == NULL &&
	    prop_dictionary_get(env, "mediaopt") == NULL &&
	    prop_dictionary_get(env, "unmediaopt") == NULL &&
	    prop_dictionary_get(env, "mediamode") == NULL) {
		/* Nothing to do. */
		return;
	}

	/*
	 * Media already set up, and commands sanity-checked.  Set/clear
	 * any options, and we're ready to go.
	 */
	media_current |= mediaopt_set;
	media_current &= ~mediaopt_clear;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_media = media_current;

	if (direct_ioctl(env, SIOCSIFMEDIA, &ifr) == -1)
		err(EXIT_FAILURE, "SIOCSIFMEDIA");
}

static int
setmedia(prop_dictionary_t env, prop_dictionary_t oenv)
{
	int type, subtype, inst;
	prop_data_t data;
	char *val;

	init_current_media(env, oenv);

	data = (prop_data_t)prop_dictionary_get(env, "media");
	assert(data != NULL);

	/* Only one media command may be given. */
	/* Must not come after mode commands */
	/* Must not come after mediaopt commands */

	/*
	 * No need to check if `instance' has been issued; setmediainst()
	 * craps out if `media' has not been specified.
	 */

	type = IFM_TYPE(media_current);
	inst = IFM_INST(media_current);

	val = strndup(prop_data_data_nocopy(data), prop_data_size(data));
	if (val == NULL)
		return -1;

	/* Look up the subtype. */
	subtype = get_media_subtype(type, val);
	if (subtype == -1)
		media_error(type, val, "subtype");

	/* Build the new current media word. */
	media_current = IFM_MAKEWORD(type, subtype, 0, inst);

	/* Media will be set after other processing is complete. */
	return 0;
}

static int
setmediaopt(prop_dictionary_t env, prop_dictionary_t oenv)
{
	char *invalid;
	prop_data_t data;
	char *val;

	init_current_media(env, oenv);

	data = (prop_data_t)prop_dictionary_get(env, "mediaopt");
	assert(data != NULL);

	/* Can only issue `mediaopt' once. */
	/* Can't issue `mediaopt' if `instance' has already been issued. */

	val = strndup(prop_data_data_nocopy(data), prop_data_size(data));
	if (val == NULL)
		return -1;

	mediaopt_set = get_media_options(media_current, val, &invalid);
	free(val);
	if (mediaopt_set == -1)
		media_error(media_current, invalid, "option");

	/* Media will be set after other processing is complete. */
	return 0;
}

static int
unsetmediaopt(prop_dictionary_t env, prop_dictionary_t oenv)
{
	char *invalid, *val;
	prop_data_t data;

	init_current_media(env, oenv);

	data = (prop_data_t)prop_dictionary_get(env, "unmediaopt");
	if (data == NULL) {
		errno = ENOENT;
		return -1;
	}

	val = strndup(prop_data_data_nocopy(data), prop_data_size(data));
	if (val == NULL)
		return -1;

	/*
	 * No need to check for A_MEDIAINST, since the test for A_MEDIA
	 * implicitly checks for A_MEDIAINST.
	 */

	mediaopt_clear = get_media_options(media_current, val, &invalid);
	free(val);
	if (mediaopt_clear == -1)
		media_error(media_current, invalid, "option");

	/* Media will be set after other processing is complete. */
	return 0;
}

static int
setmediainst(prop_dictionary_t env, prop_dictionary_t oenv)
{
	int type, subtype, options;
	int64_t inst;
	bool rc;

	init_current_media(env, oenv);

	rc = prop_dictionary_get_int64(env, "mediainst", &inst);
	assert(rc);

	/* Can only issue `instance' once. */
	/* Must have already specified `media' */

	type = IFM_TYPE(media_current);
	subtype = IFM_SUBTYPE(media_current);
	options = IFM_OPTIONS(media_current);

	media_current = IFM_MAKEWORD(type, subtype, options, inst);

	/* Media will be set after other processing is complete. */
	return 0;
}

static int
setmediamode(prop_dictionary_t env, prop_dictionary_t oenv)
{
	int type, subtype, options, inst, mode;
	prop_data_t data;
	char *val;

	init_current_media(env, oenv);

	data = (prop_data_t)prop_dictionary_get(env, "mediamode");
	assert(data != NULL);

	type = IFM_TYPE(media_current);
	subtype = IFM_SUBTYPE(media_current);
	options = IFM_OPTIONS(media_current);
	inst = IFM_INST(media_current);

	val = strndup(prop_data_data_nocopy(data), prop_data_size(data));
	if (val == NULL)
		return -1;

	mode = get_media_mode(type, val);
	if (mode == -1)
		media_error(type, val, "mode");

	free(val);

	media_current = IFM_MAKEWORD(type, subtype, options, inst) | mode;

	/* Media will be set after other processing is complete. */
	return 0;
}

void
print_media_word(int ifmw, const char *opt_sep)
{
	const char *str;

	printf("%s", get_media_subtype_string(ifmw));

	/* Find mode. */
	if (IFM_MODE(ifmw) != 0) {
		str = get_media_mode_string(ifmw);
		if (str != NULL)
			printf(" mode %s", str);
	}

	/* Find options. */
	for (; (str = get_media_option_string(&ifmw)) != NULL; opt_sep = ",")
		printf("%s%s", opt_sep, str);

	if (IFM_INST(ifmw) != 0)
		printf(" instance %d", IFM_INST(ifmw));
}

void
media_status(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifmediareq ifmr;
	int af, i, s;
	int *media_list;
	const char *ifname;

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "getifname");
	if ((af = getaf(env)) == -1)
		af = AF_UNSPEC;

	/* get out early if the family is unsupported by the kernel */
	if ((s = getsock(af)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);

	memset(&ifmr, 0, sizeof(ifmr));
	estrlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));

	if (prog_ioctl(s, SIOCGIFMEDIA, &ifmr) == -1) {
		/*
		 * Interface doesn't support SIOC{G,S}IFMEDIA.
		 */
		return;
	}

	if (ifmr.ifm_count == 0) {
		warnx("%s: no media types?", ifname);
		return;
	}

	media_list = (int *)malloc(ifmr.ifm_count * sizeof(int));
	if (media_list == NULL)
		err(EXIT_FAILURE, "malloc");
	ifmr.ifm_ulist = media_list;

	if (prog_ioctl(s, SIOCGIFMEDIA, &ifmr) == -1)
		err(EXIT_FAILURE, "SIOCGIFMEDIA");

	printf("\tmedia: %s ", get_media_type_string(ifmr.ifm_current));
	print_media_word(ifmr.ifm_current, " ");
	if (ifmr.ifm_active != ifmr.ifm_current) {
		printf(" (");
		print_media_word(ifmr.ifm_active, " ");
		printf(")");
	}
	printf("\n");

	if (ifmr.ifm_status & IFM_STATUS_VALID) {
		const struct ifmedia_status_description *ifms;
		int bitno, found = 0;

		printf("\tstatus: ");
		for (bitno = 0; ifm_status_valid_list[bitno] != 0; bitno++) {
			for (ifms = ifm_status_descriptions;
			     ifms->ifms_valid != 0; ifms++) {
				if (ifms->ifms_type !=
				      IFM_TYPE(ifmr.ifm_current) ||
				    ifms->ifms_valid !=
				      ifm_status_valid_list[bitno])
					continue;
				printf("%s%s", found ? ", " : "",
				    IFM_STATUS_DESC(ifms, ifmr.ifm_status));
				found = 1;

				/*
				 * For each valid indicator bit, there's
				 * only one entry for each media type, so
				 * terminate the inner loop now.
				 */
				break;
			}
		}

		if (found == 0)
			printf("unknown");
		printf("\n");
	}

	if (get_flag('m')) {
		int type, printed_type;

		for (type = IFM_NMIN; type <= IFM_NMAX; type += IFM_NMIN) {
			for (i = 0, printed_type = 0; i < ifmr.ifm_count; i++) {
				if (IFM_TYPE(media_list[i]) != type)
					continue;
				if (printed_type == 0) {
					printf("\tsupported %s media:\n",
					    get_media_type_string(type));
					printed_type = 1;
				}
				printf("\t\tmedia ");
				print_media_word(media_list[i], " mediaopt ");
				printf("\n");
			}
		}
	}

	free(media_list);
}

static void
media_usage(prop_dictionary_t env)
{
	fprintf(stderr,
	    "\t[ media type ] [ mediaopt opts ] [ -mediaopt opts ] "
	    "[ instance minst ]\n");
}

static void
media_constructor(void)
{
	if (register_flag('m') != 0)
		err(EXIT_FAILURE, __func__);
	usage_func_init(&usage, media_usage);
	register_usage(&usage);
}
