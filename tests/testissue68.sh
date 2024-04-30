#/bin/sh

$exe issue068.xml -L deu --inputclass=OCR --textredundancy=full testissue68.out.1.xml

$exe issue068.xml -L deu --inputclass=OCR --textredundancy=full --copyclass testissue68.out.2.xml

$folialint --nooutput testissue68.out.*.xml 2>&1
