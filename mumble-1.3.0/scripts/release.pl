#! /usr/bin/perl -w
#
# Copyright 2005-2019 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.
#
# Creates a source tar.gz and zip file with adjusted Version.h file
use strict;
use warnings;
use Carp;
use POSIX;

my $ver;

system("rm mumble-*");
chdir("scripts");
system("bash mkini.sh");
chdir("..");

if ($#ARGV < 0) {
  open(F, "git describe origin/master|") or croak "Failed to get version string";
  while (<F>) {
    chomp();
    s/^(.+)-([0-9]+)-(g.+)$/$1|$2|$3/;
    s/-/~/;
    s/\|/-/g;
    $ver = $_;
  }
  close(F);
  print "REVISION $ver\n";
} elsif ($#ARGV == 0) {
  $ver = $ARGV[0];
}

print "Adjusting Version.h\n";

open(F, "<src/Version.h") or croak "Could not open src/Version.h for reading";
my @lines = <F>;
close(F);
my $content = join('', @lines);
$content =~ s/(\#ifndef MUMBLE_VERSION)/$1\n\#define MUMBLE_VERSION $ver\n\#endif\n$1/;

open(F, ">src/Version.h") or croak "Could not open src/Version.h for writing";
print F $content;
close(F);

print "Compressing tree\n";
my $ballname = "mumble-${ver}";
my $exclusions = join(" --exclude=", ("",
     "*/.git*",
     # Exclude the archive we are currently writing to
     "${ballname}.*",
     # Exclude files with Debian FSG licensing issues (#1230)
     "${ballname}/3rdparty/speex-src/doc/draft-herlein-avt-rtp-speex-00.txt",
     "${ballname}/3rdparty/speex-src/doc/draft-herlein-speex-rtp-profile-02.txt",
     "${ballname}/3rdparty/speex-src/doc/draft-herlein-speex-rtp-profile-03.txt",
     "${ballname}/3rdparty/speex-src/doc/draft-ietf-avt-rtp-speex-00.txt",
     "${ballname}/3rdparty/speex-src/doc/draft-ietf-avt-rtp-speex-01-tmp.txt",
     "${ballname}/3rdparty/speex-src/doc/draft-ietf-avt-rtp-speex-05-tmp.txt",
     "${ballname}/3rdparty/speex-src/doc/manual.lyx",
     "${ballname}/3rdparty/celt-0.11.0-src/doc/ietf/draft-valin-celt-rtp-profile-01.txt",
     "${ballname}/3rdparty/celt-0.7.0-src/doc/ietf/draft-valin-celt-rtp-profile-01.txt"
    )
);


system("mkdir ${ballname}") == 0 or croak "Could not create target directory ${ballname}";
system("mv * ${ballname}/");
eval {
	system("tar ${exclusions} -zchvf ${ballname}.tar.gz ${ballname}") == 0 or croak "Failed to create tar.gz";
	system("zip -9 -r ${exclusions} ${ballname}.zip ${ballname}") == 0 or croak "Failed to create zip";
};
system("mv ${ballname}/* .");
system("rmdir ${ballname}");

print "Done\n";

