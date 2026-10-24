#!/usr/bin/perl
# Sierra BMD HDMI CTA-861 EDID witness; READ ONLY.
# No device driver, monitor mode, capture API or screenshot access.
use strict;
use warnings;

my $registry = '';
{
    local $/;
    open my $ioreg, '-|', '/usr/sbin/ioreg', '-r', '-c', 'AppleDisplay',
        '-l', '-w0' or die "cannot open ioreg: $!\n";
    $registry = <$ioreg> // '';
    close $ioreg or die "ioreg failed\n";
}
if ($registry !~ /"DisplayVendorID"\s*=\s*8961\b/) {
    print "stage=hdmi_edid result=no_bmd_sink\n";
    exit 21;
}
my ($hex) = $registry =~ /"IODisplayEDID"\s*=\s*<([0-9a-fA-F]+)>/;
if (!defined $hex || length($hex) < 256 || length($hex) % 2 != 0) {
    print "stage=hdmi_edid result=missing_or_incomplete\n";
    exit 22;
}
my @bytes = unpack('C*', pack('H*', $hex));
my $blocks = int(scalar(@bytes) / 128);
my $valid = scalar(@bytes) % 128 == 0;
my @checksums;
for my $block (0 .. $blocks - 1) {
    my $sum = 0;
    for my $i ($block * 128 .. ($block + 1) * 128 - 1) {
        $sum += $bytes[$i];
    }
    push @checksums, $sum % 256;
    $valid &&= $sum % 256 == 0;
}
if (!$valid) {
    print "stage=hdmi_edid result=checksum_failed\n";
    exit 23;
}

my $cta = 0;
my ($revision, $flags, $can422, $can444, $audio) = (0, 0, 0, 0, 0);
for my $block (1 .. $blocks - 1) {
    my $base = 128 * $block;
    next unless $bytes[$base] == 0x02;
    $cta++;
    $revision = $bytes[$base + 1];
    $flags = $bytes[$base + 3];
    $can422 = ($flags & 0x10) ? 1 : 0;
    $can444 = ($flags & 0x20) ? 1 : 0;
    $audio = ($flags & 0x40) ? 1 : 0;
    last;
}
printf "stage=hdmi_edid result=valid bytes=%u blocks=%u cta_blocks=%u "
     . "cta_revision=%u cta_flags=0x%02x base_checksum=%u "
     . "advertised_ycbcr_422=%u advertised_ycbcr_444=%u basic_audio=%u "
     . "actual_hdmi_wire_encoding=not_measured\n",
     scalar(@bytes), $blocks, $cta, $revision, $flags,
     $checksums[0], $can422, $can444, $audio;
printf "stage=hdmi_receiver result=not_tested waveform_verified=0 signal_lock=unknown\n";
exit 0;
