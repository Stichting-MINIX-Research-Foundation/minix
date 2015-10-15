BEGIN { split(COMPATDIRS, dirs); n = 1; last_prefix = "" }
/^#/ { print; }
{
	if (NF > 0) {
		pos = index($0, S);
		if (pos == 0) {
			print;
			next;
		}
		prefix = substr($0, 1, pos)
		if (prefix != last_prefix) {
			for (d in dirs) {
				for (f = 1; f < n; f++) {
					x=files[f]; sub(S, S "/" dirs[d], x);
					print x;
				}
			}
			delete files;
			n = 1;
			last_prefix = prefix;
		}
		files[n++] = $0;
	}
	next
}
END {
	for (d in dirs) {
		for (f = 1; f < n; f++) {
		    x=files[f]; sub(S, S "/" dirs[d], x); print x;
		}
	}
}
