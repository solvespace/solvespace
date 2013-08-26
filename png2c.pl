#!/usr/bin/env perl

use strict;
use warnings;

use GD;

my ($out, $proto, $srcdir) = @ARGV;
defined($srcdir) or $srcdir = '.';
-d "$srcdir/icons" || die "$srcdir/icons/: directory not found";

open(OUT, ">$out") or die "$out: $!";
open(PROTO, ">$proto") or die "$proto: $!";

print OUT   "/**** This is a generated file - do not edit ****/\n\n";
print PROTO "/**** This is a generated file - do not edit ****/\n\n";

for my $file (<$srcdir/icons/*.png>) {
    next if $file =~ m#/char-[^/]+$#;

    $file =~ m#/([^/]+)\.png$#;
    my $base = "Icon_$1";
    $base =~ y/-/_/;

    open(PNG, $file) or die "$file: $!\n";
    my $img = newFromPng GD::Image(\*PNG) or die;
    $img->trueColor(1);

    close PNG;

    my ($width, $height) = $img->getBounds();
    die "$file: $width, $height"  if ($width != 24) or ($height != 24);

    print PROTO "extern unsigned char $base\[24*24*3\];\n";
    print OUT "unsigned char $base\[24*24*3] = {\n";

    for(my $y = 0; $y < 24; $y++) {
        for(my $x = 0; $x < 24; $x++) {
            my $index = $img->getPixel($x, 23-$y);
            my ($r, $g, $b) = $img->rgb($index);
            if($r + $g + $b < 11) {
                ($r, $g, $b) = (30, 30, 30);
            }
            printf OUT "    0x%02x, 0x%02x, 0x%02x,\n", $r, $g, $b;
        }
    }

    print OUT "};\n\n";
}

close PROTO;
close OUT;
