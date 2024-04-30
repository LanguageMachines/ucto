#/bin/sh

$exe -L nl encoding2.nl
$exe -L nl -e LATIN1 encoding2.nl
$exe -L nl -e JEL utf8bom.nl
$exe -L nl utf16bom.nl
$exe -L nl -e WINDOWS-1258 W1258.nl
$exe -L nl UTF16BE.nl
$exe -L nl windows_cr_lf_utf16.czech
$exe -L nld multibom.txt -X mb.xml
$folialint --nooutput mb.xml >> testoutput/testencoding2.tmp 2>& 1
