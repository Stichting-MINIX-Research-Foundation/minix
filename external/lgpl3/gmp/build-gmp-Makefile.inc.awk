#! /usr/bin/awk -f

/^config.status: linking/ {
	# $3 = src
	# $5 = dst

	sub(/mpn\//, "", $5)
	sub(/.*external\/lgpl3\/gmp\/dist\//, "", $3)

	srcname = $3
	sub(/.*\//, "", srcname)

	if (match($3, /\.c$/)) {
		if ($5 == srcname) {
			c_list[$5] = $3
		} else {
			c_src_list[$5] = $3
		}
	} else if (match($3, /\.(asm|s|S)$/)) {
		asm_list[$5] = $3
	}
}

END {
	printf("SRCS+= \\\n");
	for (c in c_list) {
		printf("\t%s \\\n", c)
	}
	printf("\nC_SRCS_LIST= \\\n");
	for (c in c_src_list) {
		printf("\t%s\t\t%s \\\n", c, c_src_list[c])
	}
	printf("\nASM_SRCS_LIST= \\\n");
	for (asm in asm_list) {
		printf("\t%s\t\t%s \\\n", asm, asm_list[asm])
	}
}
