#/bin/sh

\rm issue93.out.*.xml
$exe -Leng -Tfull issue93.folia.xml issue93.out.1.xml
$exe -Leng -Tfull issue93b.folia.xml issue93.out.2.xml
$folialint --nooutput issue93.out.*.xml 2>&1
