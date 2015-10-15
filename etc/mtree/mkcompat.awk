BEGIN { n = 1 }
/^#/ { print }
{
	if (NF > 0 && substr($0, 1, 1) != "#") {
		files[n++] = $0;
	}
	next;
}
END {
	split(COMPATDIRS, dirs);
	for (d in dirs) {
		for (f = 1; f < n; f++) {
			x=files[f]; sub("@ARCH_SUBDIR@", dirs[d], x);
			print x;
		}
	}
}
