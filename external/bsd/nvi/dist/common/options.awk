#	Id: options.awk,v 10.1 1995/06/08 19:00:01 bostic Exp  (Berkeley) Date: 1995/06/08 19:00:01 
 
/^\/\* O_[0-9A-Z_]*/ {
	opt = $2
	printf("#define %s %d\n", opt, cnt++)
	ofs = FS
	FS="\""
	do getline
	while ($1 != "	{L(")
	FS=ofs
	opt_name = $2
	if (opt_name < prev_name) {
		printf "missorted %s: \"%s\" < \"%s\"\n", opt, opt_name, prev_name >"/dev/stderr"
		exit 1
	}
	prev_name = opt_name
	next
}
END {
	printf("#define O_OPTIONCOUNT %d\n", cnt);
}
