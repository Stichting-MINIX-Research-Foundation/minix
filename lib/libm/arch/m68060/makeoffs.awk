BEGIN{FS=",";s = -16;}
/\.long/{s += 16;}
s<0 || s>1023{print $0}
s>=0 && s<1024{\
	printf "ENTRY_NOPROFILE(__fplsp060_%04x) ", s;\
	print $1 "," $2;\
	printf "ENTRY_NOPROFILE(__fplsp060_%04x) ", s+8;\
	print "	.long	" $3 "," $4;\
}
