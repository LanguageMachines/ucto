#/bin/sh -x

$exe -L nld folia-correct.xml folia-corrected-1.xml
$folialint --nooutput folia-corrected-1.xml 2>&1
$exe -L nld --allow-word-corrections folia-correct.xml folia-corrected-2.xml
$folialint --nooutput folia-corrected-2.xml 2>&1
$exe -L nld --allow-word-corrections folia-correct-corrected.xml folia-corrected-3.xml
$folialint --nooutput folia-corrected-3.xml 2>&1
