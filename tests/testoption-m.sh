#/bin/sh

$exe -Lnld lines.txt

$exe -Lnld -m lines.txt

$exe -Lnld -m -n lines.txt

$exe -Lnld lines.txt -X > m1.xml
$folialint --nooutput m1.xml 2>&1

$exe -Lnld lines.txt -m -X > m2.xml
$folialint --nooutput m2.xml 2>&1
