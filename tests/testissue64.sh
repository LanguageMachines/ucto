#/bin/sh

$exe issue064a.xml -L nld outa.xml
$folialint --nooutput outa.xml 2>&1
$exe issue064b.xml -L nld outb.xml
$folialint --nooutput outb.xml 2>&1
$exe issue064c.xml -L nld outc.xml
$folialint --nooutput outc.xml 2>&1
