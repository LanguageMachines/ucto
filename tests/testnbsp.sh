#/bin/sh -x

\rm -f nbsp.out.xml
$exe -Lnld nbsp.xml nbsp.out.xml
$folialint --nooutput nbsp.out.xml 2>&1
