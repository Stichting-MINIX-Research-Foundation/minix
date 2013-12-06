#	Id: ex.awk,v 10.1 1995/06/08 18:55:37 bostic Exp  (Berkeley) Date: 1995/06/08 18:55:37 
 
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}
