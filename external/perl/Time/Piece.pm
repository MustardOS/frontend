# Build time stand in for Time::Piece, used only when the real module is missing
#
# OpenSSL's makefile template needs exactly three things from it: an overridden
# localtime, a strptime class method for the "%d %b %Y" release date, and strftime.
# Everything here leans on core Perl only, so no extra package has to be installed.
#
# This is never compiled into anything and never ships on a device. If the real
# Time::Piece is present it always wins, because external/openssl.sh only puts this
# directory on PERL5LIB as a fallback.

package Time::Piece;

use strict;
use warnings;

use POSIX ();
use Exporter 'import';

our $VERSION = '0.00_shim';
our @EXPORT = qw(localtime gmtime);

my @MONTH = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);

# Holds the nine fields of a struct tm in the order POSIX::strftime wants them
sub _wrap {
    my @tm = @_;
    return bless [@tm[0 .. 8]], 'Time::Piece';
}

sub _epoch_arg {
    shift @_ if @_ && defined $_[0] && $_[0] eq 'Time::Piece';
    return @_ && defined $_[0] ? $_[0] : time;
}

sub localtime {
    return _wrap(CORE::localtime(_epoch_arg(@_)));
}

sub gmtime {
    return _wrap(CORE::gmtime(_epoch_arg(@_)));
}

sub strptime {
    my ($class, $string, $format) = @_;

    unless (defined $format && $format eq '%d %b %Y') {
        die "Time::Piece shim only understands the '%d %b %Y' format, got '"
            . (defined $format ? $format : 'undef') . "'\n";
    }

    my ($mday, $name, $year) = $string =~ /\A\s*(\d{1,2})\s+([A-Za-z]{3})\s+(\d{4})\s*\z/
        or die "Error parsing time '$string'\n";

    my $mon;
    for my $i (0 .. $#MONTH) {
        $mon = $i if lc $MONTH[$i] eq lc $name;
    }

    defined $mon or die "Error parsing month '$name'\n";

    return _wrap(0, 0, 0, $mday, $mon, $year - 1900, 0, 0, -1);
}

sub strftime {
    my ($self, $format) = @_;

    $format = '%a, %d %b %Y %H:%M:%S %Z' unless defined $format;

    return POSIX::strftime($format, @$self);
}

1;
