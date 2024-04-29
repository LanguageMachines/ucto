#/bin/sh -x

rm ml*.xml

$exe -L nl -F folia-lang.xml ml01.xml
$folialint --nooutput ml01.xml 2>&1
$exe -L en -F folia-lang.xml ml02.xml
$folialint --nooutput ml02.xml 2>&1
#$exe --uselanguages=eng -F folia-lang-2.xml ml03.xml
#$folialint --nooutput ml03.xml 2>&1
$exe --detectlanguages=nld,eng -F folia-lang.xml ml04.xml
$folialint --nooutput ml04.xml 2>&1
$exe --detectlanguages=nld,eng,deu,spa,fra,swe -X --id=ml multilang.txt ml.xml
$folialint --nooutput ml.xml 2>&1
$exe --detectlanguages=nld,eng -X --id=ml multilang2.txt ml2.xml
$folialint --nooutput ml2.xml 2>&1
$exe --detectlanguages=und,nld -n --id=ml multilang2.txt ml3.xml
$folialint --nooutput ml3.xml 2>&1
$exe --detectlanguages=und,deu,nld, --id=ml multilang.txt ml4.xml
$folialint --nooutput ml4.xml 2>&1
$exe --detectlanguages=und,eng, --id=ml multilang3.txt ml5.xml
$folialint --nooutput ml5.xml 2>&1
