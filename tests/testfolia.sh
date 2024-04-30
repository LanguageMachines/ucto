#/bin/sh

$exe -Q -X --id=foliatest -L nld folia.txt testfolia1.xml
$folialint --nooutput testfolia1.xml >> testoutput/testfolia.tmp 2>&1
$exe -Q -T none -X --id=foliatest -L nld folia.txt testfolia2.xml
$folialint --nooutput testfolia1.xml >> testoutput/testfolia.tmp 2>&1
