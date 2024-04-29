#/bin/sh -x

\rm -f folia2a.xml folia2b.xml
$exe -Q -X -T none -L nl folia2.txt folia2a.xml
$folialint --nooutput folia2a.xml >> testoutput/testfolia2.tmp 2>&1
$exe -Q -L nl folia2.txt folia2b.xml
$folialint --nooutput folia2b.xml >> testoutput/testfolia2.tmp 2>&1
