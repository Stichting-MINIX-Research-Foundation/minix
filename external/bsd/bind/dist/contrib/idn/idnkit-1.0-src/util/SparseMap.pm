# Id: SparseMap.pm,v 1.1 2003/06/04 00:27:53 marka Exp 
#
# Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set forth bellow.
# 
# 			LICENSE TERMS AND CONDITIONS 
# 
# The following License Terms and Conditions apply, unless a different
# license is obtained from Japan Network Information Center ("JPNIC"),
# a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
# Chiyoda-ku, Tokyo 101-0047, Japan.
# 
# 1. Use, Modification and Redistribution (including distribution of any
#    modified or derived work) in source and/or binary forms is permitted
#    under this License Terms and Conditions.
# 
# 2. Redistribution of source code must retain the copyright notices as they
#    appear in each source code file, this License Terms and Conditions.
# 
# 3. Redistribution in binary form must reproduce the Copyright Notice,
#    this License Terms and Conditions, in the documentation and/or other
#    materials provided with the distribution.  For the purposes of binary
#    distribution the "Copyright Notice" refers to the following language:
#    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
# 
# 4. The name of JPNIC may not be used to endorse or promote products
#    derived from this Software without specific prior written approval of
#    JPNIC.
# 
# 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
#    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
#    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
#

package SparseMap;

use strict;
use Carp;

my $debug = 0;

sub new {
    # common options are:
    #   BITS => [8, 7, 6],	# 3-level map, 2nd level bits=7, 3rd = 6.
    #   MAX  => 0x110000	# actually, max + 1.
    my $class = shift;
    my $self = {@_};

    croak "BITS unspecified" unless exists $self->{BITS};
    croak "BITS is not an array reference"
	unless ref($self->{BITS}) eq 'ARRAY';
    croak "MAX unspecified" unless exists $self->{MAX};

    $self->{MAXLV} = @{$self->{BITS}} - 1;
    $self->{FIXED} = 0;

    my $lv0size = (indices($self, $self->{MAX} - 1))[0] + 1;

    my @map = (undef) x $lv0size;
    $self->{MAP} = \@map;

    bless $self, $class;
}

sub add1 {
    my ($self, $n, $val) = @_;

    croak "Already fixed" if $self->{FIXED};
    carp("data ($n) out of range"), return if $n >= $self->{MAX};

    my @index = $self->indices($n);
    my $r = $self->{MAP};
    my $maxlv = $self->{MAXLV};
    my $idx;
    my $lv;

    for ($lv = 0; $lv < $maxlv - 1; $lv++) {
	$idx = $index[$lv];
	$r->[$idx] = $self->create_imap($lv + 1, undef)
	    unless defined $r->[$idx];
	$r = $r->[$idx];
    }
    $idx = $index[$lv];
    $r->[$idx] = $self->create_dmap() unless defined $r->[$idx];
    $self->add_to_dmap($r->[$idx], $index[$maxlv], $val);
}

sub fix {
    my $self = shift;
    my $map = $self->{MAP};
    my $maxlv = $self->{MAXLV};
    my @tmp;
    my @zero;

    carp "Already fixed" if $self->{FIXED};
    $self->collapse_tree();
    $self->fill_default();
    $self->{FIXED} = 1;
}

sub indices {
    my $self = shift;
    my $v = shift;
    my @bits = @{$self->{BITS}};
    my @idx;

    print "indices($v,", join(',', @bits), ") = " if $debug;
    for (my $i = @bits - 1; $i >= 0; $i--) {
	my $bit = $bits[$i];
	unshift @idx, $v & ((1 << $bit) - 1);
	$v = $v >> $bit;
    }
    print "(", join(',', @idx), ")\n" if $debug;
    @idx;
}

sub get {
    my $self = shift;
    my $v = shift;
    my $map = $self->{MAP};
    my @index = $self->indices($v);

    croak "Not yet fixed" unless $self->{FIXED};

    my $lastidx = pop @index;
    foreach my $idx (@index) {
	return $map->{DEFAULT} unless defined $map->[$idx];
	$map = $map->[$idx];
    }
    $map->[$lastidx];
}

sub indirectmap {
    my $self = shift;

    croak "Not yet fixed" unless $self->{FIXED};

    my @maps = $self->collect_maps();
    my $maxlv = $self->{MAXLV};
    my @bits = @{$self->{BITS}};

    my @indirect = ();
    for (my $lv = 0; $lv < $maxlv; $lv++) {
	my $offset;
	my $chunksz;
	my $mapsz = @{$maps[$lv]->[0]};
	if ($lv < $maxlv - 1) {
	    # indirect map
	    $offset = @indirect + @{$maps[$lv]} * @{$maps[$lv]->[0]};
	    $chunksz = (1 << $bits[$lv + 1]);
	} else {
	    # direct map
	    $offset = 0;
	    $chunksz = 1;
	}
	my $nextmaps = $maps[$lv + 1];
	foreach my $mapref (@{$maps[$lv]}) {
	    croak "mapsize inconsistent ", scalar(@$mapref),
	        " should be ", $mapsz, " (lv $lv)\n" if @$mapref != $mapsz;
	    foreach my $m (@$mapref) {
		my $idx;
		for ($idx = 0; $idx < @$nextmaps; $idx++) {
		    last if $nextmaps->[$idx] == $m;
		}
		croak "internal error: map corrupted" if $idx >= @$nextmaps;
		push @indirect, $offset + $chunksz * $idx;
	    }
	}
    }
    @indirect;
}

sub cprog_imap {
    my $self = shift;
    my %opt = @_;
    my $name = $opt{NAME} || 'map';
    my @indirect = $self->indirectmap();
    my $prog;
    my $i;
    my ($idtype, $idcol, $idwid);

    my $max = 0;
    $max < $_ and $max = $_ foreach @indirect;

    if ($max < 256) {
	$idtype = 'char';
	$idcol = 8;
	$idwid = 3;
    } elsif ($max < 65536) {
	$idtype = 'short';
	$idcol = 8;
	$idwid = 5;
    } else {
	$idtype = 'long';
	$idcol = 4;
	$idwid = 10;
    }
    $prog = "static const unsigned $idtype ${name}_imap[] = {\n";
    $i = 0;
    foreach my $v (@indirect) {
	if ($i % $idcol == 0) {
	    $prog .= "\n" if $i != 0;
	    $prog .= "\t";
	}
	$prog .= sprintf "%${idwid}d, ", $v;
	$i++;
    }
    $prog .= "\n};\n";
    $prog;
}

sub cprog {
    my $self = shift;
    $self->cprog_imap(@_) . "\n" . $self->cprog_dmap(@_);
}

sub stat {
    my $self = shift;
    my @maps = $self->collect_maps();
    my $elsize = $self->{ELSIZE};
    my $i;
    my $total = 0;
    my @lines;

    for ($i = 0; $i < $self->{MAXLV}; $i++) {
	my $nmaps = @{$maps[$i]};
	my $mapsz = @{$maps[$i]->[0]};
	push @lines, "level $i: $nmaps maps (size $mapsz) ";
	push @lines, "[", $nmaps * $mapsz * $elsize, "]" if $elsize;
	push @lines, "\n";
    }
    my $ndmaps = @{$maps[$i]};
    push @lines, "level $i: $ndmaps dmaps";
    my $r = $maps[$i]->[0];
    if (ref($r) eq 'ARRAY') {
	push @lines, " (size ", scalar(@$r), ")";
    }
    push @lines, "\n";
    join '', @lines;
}

sub collapse_tree {
    my $self = shift;
    my @tmp;

    $self->_collapse_tree_rec($self->{MAP}, 0, \@tmp);
}

sub _collapse_tree_rec {
    my ($self, $r, $lv, $refs) = @_;
    my $ref = $refs->[$lv];
    my $maxlv = $self->{MAXLV};
    my $found;

    return $r unless defined $r;

    $ref = $refs->[$lv] = [] unless defined $ref;

    if ($lv == $maxlv) {
	$found = $self->find_dmap($ref, $r);
    } else {
	for (my $i = 0; $i < @$r; $i++) {
	    $r->[$i] = $self->_collapse_tree_rec($r->[$i], $lv + 1, $refs);
	}
	$found = $self->find_imap($ref, $r);
    }
    unless ($found) {
	$found = $r;
	push @$ref, $found;
    }
    return $found;
}

sub fill_default {
    my $self = shift;
    my $maxlv = $self->{MAXLV};
    my $bits = $self->{BITS};
    my @zeros;

    $zeros[$maxlv] = $self->create_dmap();
    for (my $lv = $maxlv - 1; $lv >= 0; $lv--) {
	my $r = $zeros[$lv + 1];
	$zeros[$lv] = $self->create_imap($lv, $r);
    }
    _fill_default_rec($self->{MAP}, 0, $maxlv, \@zeros);
}

sub _fill_default_rec {
    my ($r, $lv, $maxlv, $zeros) = @_;

    return if $lv == $maxlv;
    for (my $i = 0; $i < @$r; $i++) {
	if (defined($r->[$i])) {
	    _fill_default_rec($r->[$i], $lv + 1, $maxlv, $zeros);
	} else {
	    $r->[$i] = $zeros->[$lv + 1];
	}
    }
}

sub create_imap {
    my ($self, $lv, $v) = @_;
    my @map;
    @map = ($v) x (1 << $self->{BITS}->[$lv]);
    \@map;
}

sub find_imap {
    my ($self, $maps, $map) = @_;
    my $i;

    foreach my $el (@$maps) {
	next unless @$el == @$map;
	for ($i = 0; $i < @$el; $i++) {
	    last unless ($el->[$i] || 0) == ($map->[$i] || 0);
	}
	return $el if $i >= @$el;
    }
    undef;
}

sub collect_maps {
    my $self = shift;
    my @maps;
    _collect_maps_rec($self->{MAP}, 0, $self->{MAXLV}, \@maps);
    @maps;
}

sub _collect_maps_rec {
    my ($r, $lv, $maxlv, $maps) = @_;
    my $mapref = $maps->[$lv];

    return unless defined $r;
    foreach my $ref (@{$mapref}) {
	return if $ref == $r;
    }
    push @{$maps->[$lv]}, $r;
    if ($lv < $maxlv) {
	_collect_maps_rec($_, $lv + 1, $maxlv, $maps) foreach @{$r};
    }
}
    
sub add {confess "Subclass responsibility";}
sub create_dmap {confess "Subclass responsibility";}
sub add_to_dmap {confess "Subclass responsibility";}
sub find_dmap {confess "Subclass responsibility";}
sub cprog_dmap {confess "Subclass responsibility";}

1;

package SparseMap::Bit;

use strict;
use vars qw(@ISA);
use Carp;
#use SparseMap;

@ISA = qw(SparseMap);

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    $self->{DEFAULT} = 0;
    bless $self, $class;
}

sub add {
    my $self = shift;

    $self->add1($_, undef) foreach @_;
}

sub create_dmap {
    my $self = shift;
    my $bmbits = $self->{BITS}->[-1];

    my $s = "\0" x (1 << ($bmbits - 3));
    \$s;
}

sub add_to_dmap {
    my ($self, $map, $idx, $val) = @_;
    vec($$map, $idx, 1) = 1;
}

sub find_dmap {
    my ($self, $ref, $r) = @_;
    foreach my $map (@$ref) {
	return $map if $$map eq $$r;
    }
    return undef;
}

sub cprog_dmap {
    my $self = shift;
    my %opt = @_;
    my $name = $opt{NAME} || 'map';
    my @maps = $self->collect_maps();
    my @bitmap = @{$maps[-1]};
    my $prog;
    my $bmsize = 1 << ($self->{BITS}->[-1] - 3);

    $prog = <<"END";
static const struct {
	unsigned char bm[$bmsize];
} ${name}_bitmap[] = {
END

    foreach my $bm (@bitmap) {
	my $i = 0;
	$prog .= "\t{{\n";
	foreach my $v (unpack 'C*', $$bm) {
	    if ($i % 16 == 0) {
		$prog .= "\n" if $i != 0;
		$prog .= "\t";
	    }
	    $prog .= sprintf "%3d,", $v;
	    $i++;
	}
	$prog .= "\n\t}},\n";
    }
    $prog .= "};\n";
    $prog;
}

1;

package SparseMap::Int;

use strict;
use vars qw(@ISA);
use Carp;
#use SparseMap;

@ISA = qw(SparseMap);

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    $self->{DEFAULT} = 0 unless exists $self->{DEFAULT};
    bless $self, $class;
}

sub add {
    my $self = shift;
    while (@_ > 0) {
	my $n = shift;
	my $val = shift;
	$self->add1($n, $val);
    }
}

sub create_dmap {
    my $self = shift;
    my $tblbits = $self->{BITS}->[-1];
    my $default = $self->{DEFAULT};

    my @tbl = ($default) x (1 << $tblbits);
    \@tbl;
}

sub add_to_dmap {
    my ($self, $map, $idx, $val) = @_;
    $map->[$idx] = $val;
}

sub find_dmap {
    my ($self, $ref, $r) = @_;
    foreach my $map (@$ref) {
	if (@$map == @$r) {
	    my $i;
	    for ($i = 0; $i < @$map; $i++) {
		last if $map->[$i] != $r->[$i];
	    }
	    return $map if $i == @$map;
	}
    }
    return undef;
}

sub cprog_dmap {
    my $self = shift;
    my %opt = @_;
    my $name = $opt{NAME} || 'map';
    my @maps = $self->collect_maps();
    my @table = @{$maps[-1]};
    my $prog;
    my $i;
    my ($idtype, $idcol, $idwid);
    my $tblsize = 1 << $self->{BITS}->[-1];

    my ($min, $max);
    foreach my $a (@table) {
	foreach my $v (@$a) {
	    $min = $v if !defined($min) or $min > $v;
	    $max = $v if !defined($max) or $max < $v;
	}
    }
    if (exists $opt{MAPTYPE}) {
	$idtype = $opt{MAPTYPE};
    } else {
	my $u = $min < 0 ? '' : 'unsigned ';
	my $absmax = abs($max);
	$absmax = abs($min) if abs($min) > $absmax;

	if ($absmax < 256) {
	    $idtype = "${u}char";
	} elsif ($absmax < 65536) {
	    $idtype = "${u}short";
	} else {
	    $idtype = "${u}long";
	}
    }

    $idwid = decimalwidth($max);
    $idwid = decimalwidth($min) if decimalwidth($min) > $idwid;

    $prog = <<"END";
static const struct {
	$idtype tbl[$tblsize];
} ${name}_table[] = {
END

    foreach my $a (@table) {
	my $i = 0;
	my $col = 0;
	$prog .= "\t{{\n\t";
	foreach my $v (@$a) {
	    my $s = sprintf "%${idwid}d, ", $v;
	    $col += length($s);
	    if ($col > 70) {
		$prog .= "\n\t";
		$col = length($s);
	    }
	    $prog .= $s;
	}
	$prog .= "\n\t}},\n";
    }
    $prog .= "};\n";
    $prog;
}

sub decimalwidth {
    my $n = shift;
    my $neg = 0;
    my $w;

    if ($n < 0) {
	$neg = 1;
	$n = -$n;
    }
    if ($n < 100) {
	$w = 2;
    } elsif ($n < 10000) {
	$w = 4;
    } elsif ($n < 1000000) {
	$w = 6;
    } elsif ($n < 100000000) {
	$w = 8;
    } else {
	$w = 10;
    }
    $w + $neg;
}

1;
