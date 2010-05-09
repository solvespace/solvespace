#!/usr/bin/perl

use GD;

for $file (sort <icons/char-*.png>) {
    open(PNG, $file) or die "$file: $!\n";
    $img = newFromPng GD::Image(\*PNG) or die;
    $img->trueColor(1);
    close PNG;

    ($width, $height) = $img->getBounds();
    die "$file: $width, $height"  if ($width != 16) or ($height != 16);

    for($x = 0; $x < 16; $x++) {
        for($y = 0; $y < 16; $y++) {
            $index = $img->getPixel($x, $y);
            ($r, $g, $b) = $img->rgb($index);
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

