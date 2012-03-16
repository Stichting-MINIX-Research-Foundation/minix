# $NetBSD: nanpa.awk,v 1.2 2003/03/13 02:55:01 jhawk Exp $
#
# todo:
#	parse "http://docs.nanpa.com/cgi-bin/npa_reports/nanpa?
#	    function=list_npa_introduced" to produce parenthetical
#	    notes about what area codes are overlayed by others
#	    (or split from).
#
function parse(file, ispipe, isplanning,	i, planinit, t)
{
	planinit = 0;
	while((ispipe?(file | getline):(getline < file)) > 0) {
		sub(/#.*/, "");
		if (length($0)==0) continue;
		if (isplanning) {
			split($0, f);
			if (!planinit && f[2]=="NEW NPA") {
				planinit=1;
				for (i=1; i<=NF; i++)
					fnames[$i]=i-1;
			} else if (planinit && length(f[fnames["NEW NPA"]])>1) {
				t = f[fnames["LOCATION"]] FS;
				if (f[fnames["OVERLAY?"]]=="Yes")
				  t = t "Overlay of " f[fnames["OLD NPA"]];
				else if (f[fnames["OLD NPA"]])
				  t = t "Split of " f[fnames["OLD NPA"]];
				if (f[fnames["STATUS"]])
					t = t " (" f[fnames["STATUS"]] ")";
				if (length(f[fnames["IN SERVICE DATE"]]) > 1)
					t = t " effective " \
					    f[fnames["IN SERVICE DATE"]];
				data[f[fnames["NEW NPA"]] "*"] = t;
			}
		} else {
			# digits only
			match($0, /^[0-9]/);
			if (RSTART==0) continue;
			i=index($0, FS);
			data[substr($0, 1, i-1)]=substr($0,i+1);
		}
	}
	close(file);
}

BEGIN{
	FS=":"
	print "# $""NetBSD: $";
	print "# Generated from http://www.nanpa.com/area_codes/index.html";
	print "# (with local exceptions)";
	print "# ";
	print "# format:";
	print "#   Area Code : Description : Detail : State/Province Abbrev.";
	print "#   (3rd and 4th fields optional)";
	print "#   A * in the Area Code field indicates a future area code."
	print "# ";
	parse("ftp -o - " \
	    "http://docs.nanpa.com/cgi-bin/npa_reports/nanpa\\?" \
	    "function=list_npa_geo_number | sed -f nanpa.sed", 1, 0);
	parse("ftp -o - " \
	    "http://docs.nanpa.com/cgi-bin/npa_reports/nanpa\\?" \
	    "function=list_npa_non_geo | sed -f nanpa.sed", 1, 0);
	parse("ftp -o - " \
	    "http://docs.nanpa.com/cgi-bin/npa_reports/nanpa\\?" \
	    "function=list_npa_not_in_service | sed -f nanpa.sed", 1, 1);
	parse("na.phone.add", 0, 0);
	sort="sort -n";
	for (i in data)
		print i FS data[i] | sort
	close(sort);
}
