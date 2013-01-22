#	@(#)vipc.pl	10.1 (Berkeley) 6/8/95
 
if (/^\/\* (VI_[0-9A-Z_]*)/) {
	$cmd = $1;
	$name = lc $1;
	$_ = <>;
	next unless /"([^"]*)"/;
	@fpars = "IPVIWIN *ipvi";
	@pars = $cmd;
	for (split "", $1) {
	    if (/\d/) {
		push @fpars, "u_int32_t val$_";
		push @pars, "val$_";
	    }
	    if (/[a-z]/) {
		push @fpars, "const char *str$_, u_int32_t len$_";
		push @pars, "str$_, len$_";
	    }
	}
	$fpars = join ', ', @fpars;
	$pars = join ', ', @pars;
	print <<EOI
static int
$name($fpars)
{
	return vi_send_$1(ipvi, $pars);
}

EOI
}
