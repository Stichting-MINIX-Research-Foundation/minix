########################################################################
#
# Copyright (c) 2010, Secure Endpoints Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

use HTML::TreeBuilder;


my $input_file = "index.html";
my $toc_file = "toc.hhc";

for (@ARGV) {
  ARG: {
      /-o(.*)/ && do {
          $toc_file = $1;
          last ARG;
      };

      $input_file = $_;
    }
}

print "Processing TOC in $input_file\n";
print "Writing to $toc_file\n";

open(TOC, '>', $toc_file) or die "Can't open $toc_file\n";

my $tree = HTML::TreeBuilder->new();

$tree->parse_file($input_file);

my $contents = $tree->look_down('class', 'contents');
my $clist = $contents->find_by_tag_name('ul');

print TOC '<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<meta name="GENERATOR" content="Heimdal">
</head>
<body>
';

process_ul_element($clist, 0);

print TOC '
</body>
</html>
';


sub process_ul_element
{
    my $e = shift;
    my $level = shift;

    if ($e->tag() eq "ul") {

        print TOC '  'x$level;
        print TOC "<ul>\n";

        my @items = $e->content_list();

        for (@items) {
            process_li_element($_, $level + 1);
        }

        print TOC '  'x$level;
        print TOC "</ul>\n";
    }
}

sub process_li_element
{
    my $e = shift;
    my $level = shift;

    if ($e->tag() eq "li") {
        my $a = $e->find_by_tag_name('a');

        my $href = $a->attr('href');
        my @ac = $a->content_list();
        my $title = $ac[0];

        print TOC "  "x$level;
        print TOC "<li><object type=\"text/sitemap\"><param name=\"Name\" value=\"$title\"><param name=\"Local\" value=\"$href\"></object>\n";

        my @items = $e->content_list();

        for (@items) {
            process_ul_element($_, $level + 1);
        }
    }
}

