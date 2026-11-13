$good = "0123456789ABCDEFGHIJKLMN" ;
@faces = qw(U L F R B D) ;
while (<>) {
   chomp ;
   @f = split " ", $_ ;
   $r = '' ;
   $pos = $f[1] ;
   for ($o=0; $o<48; $o += 24) {
      for ($f=0; $f<6; $f++) {
         if (substr($pos, $o+4*$f, 4) eq substr($good, 4*$f, 4)) {
            $r .= "." ;
         } else {
            $r .= $faces[$f] ;
         }
      }
   }
   print "$r\n" ;
}
