#!/usr/bin/perl

$POLY = 0xedb88320;

sub ProcessBit {
    my ($bit) = @_;

    my $topWasSet = ($shiftReg & (1 << 31));

    $shiftReg <<= 1;
    if($bit) {
        $shiftReg |= 1;
    }
    
    if($topWasSet) {
        $shiftReg ^= $POLY;
    }
}

sub ProcessByte {
    my ($v) = @_;
    for(0..7) {
        ProcessBit($v & (1 << $_));
    }
}

sub ProcessString {
    my ($str) = @_;
    for (split //, $str) {
        if($_ ne "\n" and $_ ne "\r") {
            ProcessByte(ord($_));
        }
    }
}

sub LicenseKey {
    my @MAGIC = (  203, 244, 134, 225,  45, 250,  70,  65, 
                224, 189,  35,   3, 228,  51,  77, 169, );
    my $magic = join('', map { chr($_) } @MAGIC);

    my ($line1, $line2, $users) = @_;

    $shiftReg = 0;
    ProcessString($line1);
    ProcessString($line2);
    ProcessString($users);
    ProcessString($magic);

    my $key = '±²³SolveSpaceLicense' . "\n";
    $key .= $line1 . "\n";
    $key .= $line2 . "\n";
    $key .= $users . "\n";
    $key .= sprintf("%08x\n", $shiftReg);

    return $key;
}


$line1 = "Jonathan Westhues";
$line2 = "";
$users = "single-user license";

print LicenseKey($line1, $line2, $users);

