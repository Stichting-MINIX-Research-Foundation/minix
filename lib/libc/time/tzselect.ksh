#! /bin/bash
#
#	$NetBSD: tzselect.ksh,v 1.9 2013/09/20 19:06:54 christos Exp $
#
PKGVERSION='(tzcode) '
TZVERSION=see_Makefile
REPORT_BUGS_TO=tz@iana.org

# Ask the user about the time zone, and output the resulting TZ value to stdout.
# Interact with the user via stderr and stdin.

# Contributed by Paul Eggert.

# Porting notes:
#
# This script requires a Posix-like shell with the extension of a
# 'select' statement.  The 'select' statement was introduced in the
# Korn shell and is available in Bash and other shell implementations.
# If your host lacks both Bash and the Korn shell, you can get their
# source from one of these locations:
#
#	Bash <http://www.gnu.org/software/bash/bash.html>
#	Korn Shell <http://www.kornshell.com/>
#	Public Domain Korn Shell <http://www.cs.mun.ca/~michael/pdksh/>
#
# This script also uses several features of modern awk programs.
# If your host lacks awk, or has an old awk that does not conform to Posix,
# you can use either of the following free programs instead:
#
#	Gawk (GNU awk) <http://www.gnu.org/software/gawk/>
#	mawk <http://invisible-island.net/mawk/>


# Specify default values for environment variables if they are unset.
: ${AWK=awk}
: ${TZDIR=$(pwd)}

# Check for awk Posix compliance.
($AWK -v x=y 'BEGIN { exit 123 }') </dev/null >/dev/null 2>&1
[ $? = 123 ] || {
	echo >&2 "$0: Sorry, your \`$AWK' program is not Posix compatible."
	exit 1
}

coord=
location_limit=10

usage="Usage: tzselect [--version] [--help] [-c COORD] [-n LIMIT]
Select a time zone interactively.

Options:

  -c COORD
    Instead of asking for continent and then country and then city,
    ask for selection from time zones whose largest cities
    are closest to the location with geographical coordinates COORD.
    COORD should use ISO 6709 notation, for example, '-c +4852+00220'
    for Paris (in degrees and minutes, North and East), or
    '-c -35-058' for Buenos Aires (in degrees, South and West).

  -n LIMIT
    Display at most LIMIT locations when -c is used (default $location_limit).

  --version
    Output version information.

  --help
    Output this help.

Report bugs to $REPORT_BUGS_TO."

while getopts c:n:-: opt
do
    case $opt$OPTARG in
    c*)
	coord=$OPTARG ;;
    n*)
	location_limit=$OPTARG ;;
    -help)
	exec echo "$usage" ;;
    -version)
	exec echo "tzselect $PKGVERSION$TZVERSION" ;;
    -*)
	echo >&2 "$0: -$opt$OPTARG: unknown option; try '$0 --help'"; exit 1 ;;
    *)
	echo >&2 "$0: try '$0 --help'"; exit 1 ;;
    esac
done

shift $((OPTIND-1))
case $# in
0) ;;
*) echo >&2 "$0: $1: unknown argument"; exit 1 ;;
esac

# Make sure the tables are readable.
TZ_COUNTRY_TABLE=$TZDIR/iso3166.tab
TZ_ZONE_TABLE=$TZDIR/zone.tab
for f in $TZ_COUNTRY_TABLE $TZ_ZONE_TABLE
do
	<$f || {
		echo >&2 "$0: time zone files are not set up correctly"
		exit 1
	}
done

newline='
'
IFS=$newline


# Work around a bug in bash 1.14.7 and earlier, where $PS3 is sent to stdout.
case $(echo 1 | (select x in x; do break; done) 2>/dev/null) in
?*) PS3=
esac

# Awk script to read a time zone table and output the same table,
# with each column preceded by its distance from 'here'.
output_distances='
  BEGIN {
    FS = "\t"
    while (getline <TZ_COUNTRY_TABLE)
      if ($0 ~ /^[^#]/)
        country[$1] = $2
    country["US"] = "US" # Otherwise the strings get too long.
  }
  function convert_coord(coord, deg, min, ilen, sign, sec) {
    if (coord ~ /^[-+]?[0-9]?[0-9][0-9][0-9][0-9][0-9][0-9]([^0-9]|$)/) {
      degminsec = coord
      intdeg = degminsec < 0 ? -int(-degminsec / 10000) : int(degminsec / 10000)
      minsec = degminsec - intdeg * 10000
      intmin = minsec < 0 ? -int(-minsec / 100) : int(minsec / 100)
      sec = minsec - intmin * 100
      deg = (intdeg * 3600 + intmin * 60 + sec) / 3600
    } else if (coord ~ /^[-+]?[0-9]?[0-9][0-9][0-9][0-9]([^0-9]|$)/) {
      degmin = coord
      intdeg = degmin < 0 ? -int(-degmin / 100) : int(degmin / 100)
      min = degmin - intdeg * 100
      deg = (intdeg * 60 + min) / 60
    } else
      deg = coord
    return deg * 0.017453292519943296
  }
  function convert_latitude(coord) {
    match(coord, /..*[-+]/)
    return convert_coord(substr(coord, 1, RLENGTH - 1))
  }
  function convert_longitude(coord) {
    match(coord, /..*[-+]/)
    return convert_coord(substr(coord, RLENGTH))
  }
  # Great-circle distance between points with given latitude and longitude.
  # Inputs and output are in radians.  This uses the great-circle special
  # case of the Vicenty formula for distances on ellipsoids.
  function dist(lat1, long1, lat2, long2, dlong, x, y, num, denom) {
    dlong = long2 - long1
    x = cos (lat2) * sin (dlong)
    y = cos (lat1) * sin (lat2) - sin (lat1) * cos (lat2) * cos (dlong)
    num = sqrt (x * x + y * y)
    denom = sin (lat1) * sin (lat2) + cos (lat1) * cos (lat2) * cos (dlong)
    return atan2(num, denom)
  }
  BEGIN {
    coord_lat = convert_latitude(coord)
    coord_long = convert_longitude(coord)
  }
  /^[^#]/ {
    here_lat = convert_latitude($2)
    here_long = convert_longitude($2)
    line = $1 "\t" $2 "\t" $3 "\t" country[$1]
    if (NF == 4)
      line = line " - " $4
    printf "%g\t%s\n", dist(coord_lat, coord_long, here_lat, here_long), line
  }
'

# Begin the main loop.  We come back here if the user wants to retry.
while

	echo >&2 'Please identify a location' \
		'so that time zone rules can be set correctly.'

	continent=
	country=
	region=

	case $coord in
	?*)
		continent=coord;;
	'')

	# Ask the user for continent or ocean.

	echo >&2 'Please select a continent, ocean, "coord", or "TZ".'

        quoted_continents=$(
	  $AWK -F'\t' '
	    /^[^#]/ {
              entry = substr($3, 1, index($3, "/") - 1)
              if (entry == "America")
		entry = entry "s"
              if (entry ~ /^(Arctic|Atlantic|Indian|Pacific)$/)
		entry = entry " Ocean"
              printf "'\''%s'\''\n", entry
            }
          ' $TZ_ZONE_TABLE |
	  sort -u |
	  tr '\n' ' '
	  echo ''
	)

	eval '
	    select continent in '"$quoted_continents"' \
		"coord - I want to use geographical coordinates." \
		"TZ - I want to specify the time zone using the Posix TZ format."
	    do
		case $continent in
		"")
		    echo >&2 "Please enter a number in range.";;
		?*)
		    case $continent in
		    Americas) continent=America;;
		    *" "*) continent=$(expr "$continent" : '\''\([^ ]*\)'\'')
		    esac
		    break
		esac
	    done
	'
	esac

	case $continent in
	'')
		exit 1;;
	TZ)
		# Ask the user for a Posix TZ string.  Check that it conforms.
		while
			echo >&2 'Please enter the desired value' \
				'of the TZ environment variable.'
			echo >&2 'For example, GST-10 is a zone named GST' \
				'that is 10 hours ahead (east) of UTC.'
			read TZ
			$AWK -v TZ="$TZ" 'BEGIN {
				tzname = "[^-+,0-9][^-+,0-9][^-+,0-9]+"
				time = "[0-2]?[0-9](:[0-5][0-9](:[0-5][0-9])?)?"
				offset = "[-+]?" time
				date = "(J?[0-9]+|M[0-9]+\.[0-9]+\.[0-9]+)"
				datetime = "," date "(/" time ")?"
				tzpattern = "^(:.*|" tzname offset "(" tzname \
				  "(" offset ")?(" datetime datetime ")?)?)$"
				if (TZ ~ tzpattern) exit 1
				exit 0
			}'
		do
			echo >&2 "\`$TZ' is not a conforming" \
				'Posix time zone string.'
		done
		TZ_for_date=$TZ;;
	*)
		case $continent in
		coord)
		    case $coord in
		    '')
			echo >&2 'Please enter coordinates' \
				'in ISO 6709 notation.'
			echo >&2 'For example, +4042-07403 stands for'
			echo >&2 '40 degrees 42 minutes north,' \
				'74 degrees 3 minutes west.'
			read coord;;
		    esac
		    distance_table=$($AWK \
			    -v coord="$coord" \
			    -v TZ_COUNTRY_TABLE="$TZ_COUNTRY_TABLE" \
			    "$output_distances" <$TZ_ZONE_TABLE |
		      sort -n |
		      sed "${location_limit}q"
		    )
		    regions=$(echo "$distance_table" | $AWK '
		      BEGIN { FS = "\t" }
		      { print $NF }
		    ')
		    echo >&2 'Please select one of the following' \
			    'time zone regions,'
		    echo >&2 'listed roughly in increasing order' \
			    "of distance from $coord".
		    select region in $regions
		    do
			case $region in
			'') echo >&2 'Please enter a number in range.';;
			?*) break;;
			esac
		    done
		    TZ=$(echo "$distance_table" | $AWK -v region="$region" '
		      BEGIN { FS="\t" }
		      $NF == region { print $4 }
		    ')
		    ;;
		*)
		# Get list of names of countries in the continent or ocean.
		countries=$($AWK -F'\t' \
			-v continent="$continent" \
			-v TZ_COUNTRY_TABLE="$TZ_COUNTRY_TABLE" \
		'
			/^#/ { next }
			$3 ~ ("^" continent "/") {
				if (!cc_seen[$1]++) cc_list[++ccs] = $1
			}
			END {
				while (getline <TZ_COUNTRY_TABLE) {
					if ($0 !~ /^#/) cc_name[$1] = $2
				}
				for (i = 1; i <= ccs; i++) {
					country = cc_list[i]
					if (cc_name[country]) {
					  country = cc_name[country]
					}
					print country
				}
			}
		' <$TZ_ZONE_TABLE | sort -f)


		# If there's more than one country, ask the user which one.
		case $countries in
		*"$newline"*)
			echo >&2 'Please select a country' \
				'whose clocks agree with yours.'
			select country in $countries
			do
			    case $country in
			    '') echo >&2 'Please enter a number in range.';;
			    ?*) break
			    esac
			done

			case $country in
			'') exit 1
			esac;;
		*)
			country=$countries
		esac


		# Get list of names of time zone rule regions in the country.
		regions=$($AWK -F'\t' \
			-v country="$country" \
			-v TZ_COUNTRY_TABLE="$TZ_COUNTRY_TABLE" \
		'
			BEGIN {
				cc = country
				while (getline <TZ_COUNTRY_TABLE) {
					if ($0 !~ /^#/  &&  country == $2) {
						cc = $1
						break
					}
				}
			}
			$1 == cc { print $4 }
		' <$TZ_ZONE_TABLE)


		# If there's more than one region, ask the user which one.
		case $regions in
		*"$newline"*)
			echo >&2 'Please select one of the following' \
				'time zone regions.'
			select region in $regions
			do
				case $region in
				'') echo >&2 'Please enter a number in range.';;
				?*) break
				esac
			done
			case $region in
			'') exit 1
			esac;;
		*)
			region=$regions
		esac

		# Determine TZ from country and region.
		TZ=$($AWK -F'\t' \
			-v country="$country" \
			-v region="$region" \
			-v TZ_COUNTRY_TABLE="$TZ_COUNTRY_TABLE" \
		'
			BEGIN {
				cc = country
				while (getline <TZ_COUNTRY_TABLE) {
					if ($0 !~ /^#/  &&  country == $2) {
						cc = $1
						break
					}
				}
			}
			$1 == cc && $4 == region { print $3 }
		' <$TZ_ZONE_TABLE)
		esac

		# Make sure the corresponding zoneinfo file exists.
		TZ_for_date=$TZDIR/$TZ
		<$TZ_for_date || {
			echo >&2 "$0: time zone files are not set up correctly"
			exit 1
		}
	esac


	# Use the proposed TZ to output the current date relative to UTC.
	# Loop until they agree in seconds.
	# Give up after 8 unsuccessful tries.

	extra_info=
	for i in 1 2 3 4 5 6 7 8
	do
		TZdate=$(LANG=C TZ="$TZ_for_date" date)
		UTdate=$(LANG=C TZ=UTC0 date)
		TZsec=$(expr "$TZdate" : '.*:\([0-5][0-9]\)')
		UTsec=$(expr "$UTdate" : '.*:\([0-5][0-9]\)')
		case $TZsec in
		$UTsec)
			extra_info="
Local time is now:	$TZdate.
Universal Time is now:	$UTdate."
			break
		esac
	done


	# Output TZ info and ask the user to confirm.

	echo >&2 ""
	echo >&2 "The following information has been given:"
	echo >&2 ""
	case $country%$region%$coord in
	?*%?*%)	echo >&2 "	$country$newline	$region";;
	?*%%)	echo >&2 "	$country";;
	%?*%?*) echo >&2 "	coord $coord$newline	$region";;
	%%?*)	echo >&2 "	coord $coord";;
	+)	echo >&2 "	TZ='$TZ'"
	esac
	echo >&2 ""
	echo >&2 "Therefore TZ='$TZ' will be used.$extra_info"
	echo >&2 "Is the above information OK?"

	ok=
	select ok in Yes No
	do
	    case $ok in
	    '') echo >&2 'Please enter 1 for Yes, or 2 for No.';;
	    ?*) break
	    esac
	done
	case $ok in
	'') exit 1;;
	Yes) break
	esac
do coord=
done

case $SHELL in
*csh) file=.login line="setenv TZ '$TZ'";;
*) file=.profile line="TZ='$TZ'; export TZ"
esac

echo >&2 "
You can make this change permanent for yourself by appending the line
	$line
to the file '$file' in your home directory; then log out and log in again.

Here is that TZ value again, this time on standard output so that you
can use the $0 command in shell scripts:"

echo "$TZ"
