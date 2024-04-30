#/bin/sh

\rm -irf foliatestout/*
mkdir foliatestout

echo "start"
$exe -L nl -F folia1.xml foliatestout/folia00.xml
echo "step 1"
$exe -L nl --outputclass=ligafilter folia1.xml foliatestout/folia01.xml
echo "step 2"
$exe -L nl --filterpunct --outputclass=FILTER -F folia2.xml foliatestout/folia02.xml
echo "step 3"
$exe -L nl -u --outputclass=UPP -F folia3.xml foliatestout/folia03.xml
echo "step 4"
$exe -L nl -F folia4.xml foliatestout/folia04.xml
echo "step 5"
$exe -L nl -F empty.xml foliatestout/folia05.xml
echo "step 6"
$exe -L nl --inputclass OCR -F folia5.xml foliatestout/folia06.xml
echo "step 7"
$exe -L nl --inputclass OCR --outputclass=AHA -F folia5.xml foliatestout/folia07.xml
echo "step 8"
$exe -L nl --inputclass OCR -F folia6.xml foliatestout/folia08.xml
echo "step 9"
$exe -L nl -F folia7.xml foliatestout/folia09.xml
echo "step 10"
$exe -L nl --textredundancy none folia7.xml foliatestout/folia10.xml
echo "step 11"
$exe -L nl --textredundancy minimal folia7.xml foliatestout/folia11.xml
echo "step 12"
$exe -L nl --textredundancy full folia7.xml foliatestout/folia12.xml
echo "step 13"
$exe -L nl -F folia8.xml foliatestout/folia13.xml
echo "step 14"
$exe -L nl -F folia9a.xml  foliatestout/folia14.xml
echo "step 15"
$exe -L nl -F folia9b.xml foliatestout/folia15.xml
echo "step 16"
$exe -L nl -Tfull textproblem.xml foliatestout/folia16.xml
echo "step 17"
$exe -L nl cell.xml foliatestout/folia17.xml
$folialint --nooutput foliatestout/*.xml 2>&1
echo "done"
