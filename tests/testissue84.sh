#/bin/sh

$exe -L nld -Tfull issue84.txt issue84.folia.xml
$folialint --nooutput issue84.folia.xml 2>&1
