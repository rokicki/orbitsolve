open F, "bad.index" or die "Can't read bad.index" ;
while (<F>) {
   chomp ;
   $bad{$_}++ ;
}
close F ;
open F, "xz -d < 6centers.log.xz |" or die "Can't pipe" ;
$seq = 1 ;
while (<F>) {
   print if !$bad{$seq} ;
   $seq++ ;
}
close F ;
