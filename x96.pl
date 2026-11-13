#
#   Input:   algorithms
#   Output:  all the algorithms remapped 96 different ways
#
my @corners = qw(UFR URB UBL ULF DRF DFL DLB DBR) ;
my @faces = qw(U F R D B L) ;
for ($i=0; $i<@faces; $i++) {
   $faces{$faces[$i]} = $i ;
}
for ($rev=0; $rev<2; $rev++) {
   for $corner (@corners) {
      for ($o=0; $o<3; $o++) {
         @f = () ;
         $rcorner = ($rev ? reverse $corner : $corner) ;
         for ($i=0; $i<3; $i++) {
            $f = substr("UFR", $i, 1) ;
            $m = substr($rcorner, ($i+$o)%3, 1) ;
            $f[$i] = $m ;
            $f[$i+3] = $faces[(3+$faces{$m}) % 6] ;
         }
         push @m, [@f] ;
      }
   }
}
my %map ;
my @tw = ("!!!", "", "2", "'") ;
sub makemove {
   my $dep = shift ;
   my $face = shift ;
   my $tw = shift ;
   if ($dep == 1) {
      return $face . $tw[$tw] ;
   } else {
      return $dep . $face . $tw[$tw] ;
   }
}
sub movesplit {
   my $mv = shift ;
   my $dep = 1 ;
   if ('1' le $mv && $mv le '9zzz') {
      $dep = substr($mv, 0, 1) ;
      $mv = substr($mv, 1) ;
   }
   my $f = substr($mv, 0, 1) ;
   my $tw = substr($mv, 1) ;
   if ($tw eq '' || $tw eq '1') {
      $tw = 1 ;
   } elsif ($tw eq '2') {
      $tw = 2 ;
   } elsif ($tw eq "3" || $tw eq "'") {
      $tw = 3 ;
   } else {
      die "Bad move $mv\n" ;
   }
   return ($dep, $f, $tw) ;
}
sub remap {
   my $mv = shift ;
   my $m = shift ;
   my ($dep, $f, $tw) = movesplit($mv) ;
   $f = $map{$f} ;
   $tw = 4 - $tw if $m >= 24 ;
   return makemove($dep, $f, $tw) ;
}
sub negate {
   my $mv = shift ;
   my ($dep, $f, $tw) = movesplit($mv) ;
   $tw = 4 - $tw ;
   return makemove($dep, $f, $tw) ;
}
while (<>) {
   chomp ;
   @f = split " ", $_ ;
   for ($rev=0; $rev<2; $rev++) {
      for ($m=0; $m<@m; $m++) {
         @g = () ;
         for ($i=0; $i<@faces; $i++) {
            $map{$faces[$i]} = $m[$m][$i] ;
         }
         for ($i=0; $i<@f; $i++) {
            push @g, remap($f[$i], $m) ;
         }
         $pr = join(' ', @g) ;
         print "$pr\n" ;
      }
      @f = reverse @f ;
      for ($i=0; $i<@f; $i++) {
         $f[$i] = negate($f[$i]) ;
      }
   }
}
