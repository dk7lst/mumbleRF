#!/usr/bin/perl

use File::Copy;

my @LANG;
$dir = "bin\\Release\\";

opendir($D, "Translations");
while ($x = readdir($D)) {
	next unless $x =~ /\.wxl$/;
	next if $x eq "English.wxl";

	open(F, "<Translations\\$x");
	while (<F>) {
		if ($_ =~ /Culture="([a-z]{2}-[a-z]{2})"/) {
			$culture = $1;
		} elsif ($_ =~ /Id="LANG">(\d+)/) {
			$langcode = $1;
		}
	}
	close(F);
	system("msitran -g $dir\\en-us\\Mumble.msi $dir\\$culture\\Mumble.msi $langcode");
	print "\n";
	push @LANG, $langcode;
}

copy("$dir\\en-us\\Mumble.msi", "$dir\\Mumble.msi");
foreach (@LANG) {
	print "$_\n";
	system("msidb -d $dir\\Mumble.msi -r $_");
	unlink("$_");
}

system("msiinfo $dir\\Mumble.msi /p Intel;1033," . join(",", @LANG));
