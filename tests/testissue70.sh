#/bin/sh

$exe issue70.xml -L nld-historical testissue70.out.1.xml

$exe issue70_b.xml -L nld-historical  testissue70.out.2.xml

$exe issue70_c.xml -L nld  testissue70.out.3.xml

$folialint --nooutput testissue70.out.*.xml 2>&1
