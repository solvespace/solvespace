#!/usr/bin/env perl

use strict;
use warnings;

use GD;

my ($srcdir) = @ARGV;
defined($srcdir) or $srcdir = '.';
-d "$srcdir/icons" || die "$srcdir/icons/: directory not found";

print "/**** This is a generated file - do not edit ****/\n\n";

for my $file (sort <$srcdir/icons/char-*.png>) {
    open(PNG, $file) or die "$file: $!\n";
    my $img = newFromPng GD::Image(\*PNG) or die;
    $img->trueColor(1);
    close PNG;

    my ($width, $height) = $img->getBounds();
    die "$file: $width, $height"  if ($width != 16) or ($height != 16);

    for(my $x = 0; $x < 16; $x++) {
        for(my $y = 0; $y < 16; $y++) {
            my $index = $img->getPixel($x, $y);
            my ($r, $g, $b) = $img->rgb($index);
            if($r + $g + $b < 11) {
                print "  0, ";
            } else {
                print "255, ";
            }
        }
        print "\n";
    }
    print "\n";
}

