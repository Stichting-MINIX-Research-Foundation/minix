# Make prototypes from .c files
# Id

use Getopt::Std;
use File::Compare;

use JSON;

my $comment = 0;
my $doxygen = 0;
my $funcdoc = 0;
my $if_0 = 0;
my $brace = 0;
my $line = "";
my $debug = 0;
my $oproto = 1;
my $private_func_re = "^_";
my %depfunction;
my %exported;
my %deprecated;
my $apple = 0;
my %documentation;

getopts('x:m:o:p:dqE:R:P:') || die "foo";
if($opt_a) {
    $apple = 1;
}

if($opt_a) {
    $apple = 1;
}

if($opt_d) {
    $debug = 1;
}

if($opt_q) {
    $oproto = 0;
}

if($opt_R) {
    $private_func_re = $opt_R;
}
my %flags = (
	  'multiline-proto' => 1,
	  'header' => 1,
	  'function-blocking' => 0,
	  'gnuc-attribute' => 1,
	  'cxx' => 1
	  );
if($opt_m) {
    foreach $i (split(/,/, $opt_m)) {
	if($i eq "roken") {
	    $flags{"multiline-proto"} = 0;
	    $flags{"header"} = 0;
	    $flags{"function-blocking"} = 0;
	    $flags{"gnuc-attribute"} = 0;
	    $flags{"cxx"} = 0;
	} else {
	    if(substr($i, 0, 3) eq "no-") {
		$flags{substr($i, 3)} = 0;
	    } else {
		$flags{$i} = 1;
	    }
	}
    }
}

if($opt_x) {
    my $EXP;
    local $/;
    open(EXP, '<', $opt_x) || die "open ${opt_x}";
    my $obj = JSON->new->utf8->decode(<EXP>);
    close $EXP;

    foreach my $x (keys %$obj) {
	if (defined $obj->{$x}->{"export"}) {
	    $exported{$x} = $obj->{$x};
	}
	if (defined $obj->{$x}->{"deprecated"}) {
	    $deprecated{$x} = $obj->{$x}->{"deprecated"};
	}
    }
}

while(<>) {
    print $brace, " ", $_ if($debug);
    
    # Handle C comments
    s@/\*.*\*/@@;
    s@//.*/@@;
    if ( s@/\*\*(.*)@@) { $comment = 1; $doxygen = 1; $funcdoc = $1;
    } elsif ( s@/\*.*@@) { $comment = 1;
    } elsif ($comment && s@.*\*/@@) { $comment = 0; $doxygen = 0;
    } elsif ($doxygen) { $funcdoc .= $_; next;
    } elsif ($comment) { next; }

    if(/^\#if 0/) {
	$if_0 = 1;
    }
    if($if_0 && /^\#endif/) {
	$if_0 = 0;
    }
    if($if_0) { next }
    if(/^\s*\#/) {
	next;
    }
    if(/^\s*$/) {
	$line = "";
	next;
    }
    if(/\{/){
	if (!/\}/) {
	    $brace++;
	}
	$_ = $line;
	while(s/\*\//\ca/){
	    s/\/\*(.|\n)*\ca//;
	}
	s/^\s*//;
	s/\s*$//;
	s/\s+/ /g;
	if($_ =~ /\)$/){
	    if(!/^static/ && !/^PRIVATE/){
		$attr = "";
		if(m/(.*)(__attribute__\s?\(.*\))/) {
		    $attr .= " $2";
		    $_ = $1;
		}
		if(m/(.*)\s(\w+DEPRECATED_FUNCTION)\s?(\(.*\))(.*)/) {
		    $depfunction{$2} = 1;
		    $attr .= " $2$3";
		    $_ = "$1 $4";
		}
		if(m/(.*)\s(\w+DEPRECATED)(.*)/) {
		    $attr .= " $2";
		    $_ = "$1 $3";
		}
		if(m/(.*)\s(HEIMDAL_\w+_ATTRIBUTE)\s?(\(.*\))?(.*)/) {
		    $attr .= " $2$3";
		    $_ = "$1 $4";
		}
		# remove outer ()
		s/\s*\(/</;
		s/\)\s?$/>/;
		# remove , within ()
		while(s/\(([^()]*),(.*)\)/($1\$$2)/g){}
		s/\<\s*void\s*\>/<>/;
		# remove parameter names 
		if($opt_P eq "remove") {
		    s/(\s*)([a-zA-Z0-9_]+)([,>])/$3/g;
		    s/\s+\*/*/g;
		    s/\(\*(\s*)([a-zA-Z0-9_]+)\)/(*)/g;
		} elsif($opt_P eq "comment") {
		    s/([a-zA-Z0-9_]+)([,>])/\/\*$1\*\/$2/g;
		    s/\(\*([a-zA-Z0-9_]+)\)/(*\/\*$1\*\/)/g;
		}
		s/\<\>/<void>/;
		# add newlines before parameters
		if($flags{"multiline-proto"}) {
		    s/,\s*/,\n\t/g;
		} else {
		    s/,\s*/, /g;
		}
		# fix removed ,
		s/\$/,/g;
		# match function name
		/([a-zA-Z0-9_]+)\s*\</;
		$f = $1;
		if($oproto) {
		    $LP = "__P((";
		    $RP = "))";
		} else {
		    $LP = "(";
		    $RP = ")";
		}
		# only add newline if more than one parameter
                if($flags{"multiline-proto"} && /,/){ 
		    s/\</ $LP\n\t/;
		}else{
		    s/\</ $LP/;
		}
		s/\>/$RP/;
		# insert newline before function name
		if($flags{"multiline-proto"}) {
		    s/(.*)\s([a-zA-Z0-9_]+ \Q$LP\E)/$1\n$2/;
		}
		if($attr ne "") {
		    $_ .= "\n    $attr";
		}
		if ($funcdoc) {
		    $documentation{$f} = $funcdoc;
		}
		$funcdoc = undef;
		if ($apple && exists $exported{$f}) {
		    $ios = $exported{$f}{ios};
		    $ios = "NA" if (!defined $ios);
		    $mac = $exported{$f}{macos};
		    $mac = "NA" if (!defined $mac);
		    die "$f neither" if ($mac eq "NA" and $ios eq "NA");
		    $_ = $_ . "  __OSX_AVAILABLE_STARTING(__MAC_${mac}, __IPHONE_${ios})";
		}
		if (exists $deprecated{$f}) {
		    $_ = $_ . "  GSSAPI_DEPRECATED_FUNCTION(\"$deprecated{$f}\")";
		    $depfunction{GSSAPI_DEPRECATED_FUNCTION} = 1;
		}
		$_ = $_ . ";";
		$funcs{$f} = $_;
	    }
	}
	$line = "";
    }
    if(/\}/){
	$brace--;
    }
    if(/^\}/){
	$brace = 0;
    }
    if($brace == 0) {
	$line = $line . " " . $_;
    }
}

die "reached end of code and still in doxygen comment" if ($doxygen);
die "reached end of code and still in comment" if ($comment);

sub foo {
    local ($arg) = @_;
    $_ = $arg;
    s/.*\/([^\/]*)/$1/;
    s/.*\\([^\\]*)/$1/;
    s/[^a-zA-Z0-9]/_/g;
    "__" . $_ . "__";
}

if($opt_o) {
    open(OUT, ">${opt_o}.new");
    $block = &foo($opt_o);
} else {
    $block = "__public_h__";
}

if($opt_p) {
    open(PRIV, ">${opt_p}.new");
    $private = &foo($opt_p);
} else {
    $private = "__private_h__";
}

$public_h = "";
$private_h = "";

$public_h_header .= "/* This is a generated file */
#ifndef $block
#define $block
#ifndef DOXY

";
if ($oproto) {
    $public_h_header .= "#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

";
} else {
    $public_h_header .= "#include <stdarg.h>

";
}
$public_h_trailer = "";

$private_h_header = "/* This is a generated file */
#ifndef $private
#define $private

";
if($oproto) {
    $private_h_header .= "#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

";
} else {
    $private_h_header .= "#include <stdarg.h>

";
}
$private_h_trailer = "";


foreach(sort keys %funcs){
    if(/^(DllMain|main)$/) { next }
    if ($funcs{$_} =~ /\^/) {
	$beginblock = "#ifdef __BLOCKS__\n";
	$endblock = "#endif /* __BLOCKS__ */\n";
    } else {
	$beginblock = $endblock = "";
    }
    # if we have an export table and doesn't have content, or matches private RE
    if((scalar(keys(%exported)) ne 0 && !exists $exported{$_} ) || /$private_func_re/) {
	$private_h .= $beginblock;
#	if ($apple and not /$private_func_re/) {
#	    $private_h .= "#define $_ __ApplePrivate_${_}\n";
#	}
	$private_h .= $funcs{$_} . "\n" ;
	$private_h .= $endblock . "\n";
	if($funcs{$_} =~ /__attribute__/) {
	    $private_attribute_seen = 1;
	}
    } else {
	if($documentation{$_}) {
	    $public_h .= "/**\n";
	    $public_h .= "$documentation{$_}";
	    $public_h .= " */\n\n";
	}
	if($flags{"function-blocking"}) {
	    $fupper = uc $_;
	    if($exported{$_} =~ /proto/) {
		$public_h .= "#if !defined(HAVE_$fupper) || defined(NEED_${fupper}_PROTO)\n";
	    } else {
		$public_h .= "#ifndef HAVE_$fupper\n";
	    }
	}
	$public_h .= $beginblock . $funcs{$_} . "\n" . $endblock;
	if($funcs{$_} =~ /__attribute__/) {
	    $public_attribute_seen = 1;
	}
	if($flags{"function-blocking"}) {
	    $public_h .= "#endif\n";
	}
	$public_h .= "\n";
    }
}

if($flags{"gnuc-attribute"}) {
    if ($public_attribute_seen) {
	$public_h_header .= "#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

";
    }

    if ($private_attribute_seen) {
	$private_h_header .= "#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

";
    }
}

my $depstr = "";
my $undepstr = "";
foreach (keys %depfunction) {
    $depstr .= "#ifndef $_
#ifndef __has_extension
#define __has_extension(x) 0
#define ${_}has_extension 1
#endif
#if __has_extension(attribute_deprecated_with_message)
#define $_(x) __attribute__((__deprecated__(x)))
#elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define $_(X) __attribute__((__deprecated__))
#else
#define $_(X)
#endif
#ifdef ${_}has_extension
#undef __has_extension
#undef ${_}has_extension
#endif
#endif /* $_ */


";
    $public_h_trailer .= "#undef $_

";
    $private_h_trailer .= "#undef $_
#define $_(X)

";
}

$public_h_header .= $depstr;
$private_h_header .= $depstr;


if($flags{"cxx"}) {
    $public_h_header .= "#ifdef __cplusplus
extern \"C\" {
#endif

";
    $public_h_trailer = "#ifdef __cplusplus
}
#endif

" . $public_h_trailer;

}
if ($opt_E) {
    $public_h_header .= "#ifndef $opt_E
#ifndef ${opt_E}_FUNCTION
#if defined(_WIN32)
#define ${opt_E}_FUNCTION __declspec(dllimport)
#define ${opt_E}_CALL __stdcall
#define ${opt_E}_VARIABLE __declspec(dllimport)
#else
#define ${opt_E}_FUNCTION
#define ${opt_E}_CALL
#define ${opt_E}_VARIABLE
#endif
#endif
#endif
";
    
    $private_h_header .= "#ifndef $opt_E
#ifndef ${opt_E}_FUNCTION
#if defined(_WIN32)
#define ${opt_E}_FUNCTION __declspec(dllimport)
#define ${opt_E}_CALL __stdcall
#define ${opt_E}_VARIABLE __declspec(dllimport)
#else
#define ${opt_E}_FUNCTION
#define ${opt_E}_CALL
#define ${opt_E}_VARIABLE
#endif
#endif
#endif

";
}
    
$public_h_trailer .= $undepstr;
$private_h_trailer .= $undepstr;

if ($public_h ne "" && $flags{"header"}) {
    $public_h = $public_h_header . $public_h . 
	$public_h_trailer . "#endif /* DOXY */\n#endif /* $block */\n";
}
if ($private_h ne "" && $flags{"header"}) {
    $private_h = $private_h_header . $private_h .
	$private_h_trailer . "#endif /* $private */\n";
}

if($opt_o) {
    print OUT $public_h;
} 
if($opt_p) {
    print PRIV $private_h;
} 

close OUT;
close PRIV;

if ($opt_o) {

    if (compare("${opt_o}.new", ${opt_o}) != 0) {
	printf("updating ${opt_o}\n");
	rename("${opt_o}.new", ${opt_o});
    } else {
	unlink("${opt_o}.new");
    }
}
	
if ($opt_p) {
    if (compare("${opt_p}.new", ${opt_p}) != 0) {
	printf("updating ${opt_p}\n");
	rename("${opt_p}.new", ${opt_p});
    } else {
	unlink("${opt_p}.new");
    }
}
