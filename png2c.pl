#!/usr/bin/perl

use GD;

my ($out, $proto) = @ARGV;
open(OUT, ">$out") or die "$out: $!";
open(PROTO, ">$proto") or die "$proto: $!";

for $file (<icons/*.png>) {
    next if $file =~ /char-/;

    $file =~ m#.*/(.*)\.png#;
    $base = "Icon_$1";
    $base =~ y/-/_/;

    open(PNG, $file) or die "$file: $!\n";
    $img = newFromPng GD::Image(\*PNG) or die;
    $img->trueColor(1);

    close PNG;

    ($width, $height) = $img->getBounds();
    die "$file: $width, $height"  if ($width != 24) or ($height != 24);

    print PROTO "extern unsigned char $base\[24*24*3\];";
    print OUT "unsigned char $base\[24*24*3] = {\n";

    for($y = 0; $y < 24; $y++) {
        for($x = 0; $x < 24; $x++) {
            $index = $img->getPixel($x, 23-$y);
            ($r, $g, $b) = $img->rgb($index);
            if($r + $g + $b < 11) {
                ($r, $g, $b) = (30, 30, 30);
            }
            printf OUT "    0x%02x, 0x%02x, 0x%02x,\n", $r, $g, $b;
        }
    }

    print OUT "};\n\n";

}
