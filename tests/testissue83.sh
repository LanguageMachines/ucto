#/bin/sh

rm shy.out.*.xml
$exe -L deu -Tfull shy.xml shy.out.1.xml
$folialint --nooutput shy.out.1.xml 2>&1
$exe -L deu -Tfull --outputclass=SHY shy.xml shy.out.2.xml
$folialint --nooutput shy.out.2.xml 2>&1
