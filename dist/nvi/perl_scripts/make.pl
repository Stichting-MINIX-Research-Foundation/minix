sub push_tags {
    my ($fh) = shift;
    my ($tagq) = $curscr->TagQ("msg");
    while(<$fh>) {
	my ($f, $l, $m);
	if ((($f, $l, $m) = split /:/) >= 2 && -f $f && $l =~ /^\d+$/) {
	    $tagq->Add($f, $l, $m);
	}
    }
    $tagq->Push();
}

sub make {
    local (*FH);
    open FH, "make 2>&1 |";
    ::push_tags(\*FH);
    close FH;
}

1;
