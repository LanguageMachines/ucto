#/bin/sh

\rm tagged.out.*.xml

$exe -L nl -v tagged.xml tagged.out.1.xml
$exe --passthru -v tagged.xml  tagged.out.2.xml
$exe --ignore-tag-hints -L nl -v tagged.xml  tagged.out.3.xml
$folialint --nooutput tagged.out.*.xml 2>&1
