#/bin/sh

$exe -L nl -v datetime.nl.txt
$exe -L nl -v --normalize=DATE,TIME,NUMBER-YEAR  datetime.nl.txt
$exe -L nl -v --normalize=DATE,TIME,NUMBER-YEAR,PUNCTUATION  datetime.nl.txt
